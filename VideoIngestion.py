"""
Copyright (c) 2018 Intel Corporation.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""

import os
import sys
import datetime
import signal
import argparse
import logging
import traceback as tb
import threading
import queue
import time
import cv2
from concurrent.futures import ThreadPoolExecutor
from algos.dpm.triggers import load_trigger
from algos.dpm.ingestion.data_ingestion_manager\
    import DataIngestionManager
from DataIngestionLib.DataIngestionLib import DataIngestionLib
from Util.log import configure_logging, LOG_LEVELS
from algos.dpm.config import Configuration

MEASUREMENT_NAME = "stream1"
MAX_BUFFERS = 10


class VideoIngestionError(Exception):
    """ Exception raised by VideoIngestion.
    """
    pass


def get_cameras_sn(config_obj):
    """Get the list of camera serial numbers based on config specified"""
    cameras_sn = []
    ingestors = config_obj.data_ingestion_manager['ingestors']
    if ingestors.get('video') and\
            ingestors['video'].get('streams') and\
            ingestors['video']['streams'].get('capture_streams'):
        # if config is other than factory.json
        capture_streams = ingestors.get('video').get('streams').get(
            'capture_streams')
        cameras_sn.extend(capture_streams.keys())
    else:
        # if config is factory.json
        video_file = ingestors['video_file']['video_file']
        cameras_sn.append(video_file[video_file.rfind('/')+1:])

    return cameras_sn


class VideoIngestion:
    """ This module ingests camera output to ImageStore using DataIngestion lib
    """
    def __init__(self, config, log):
        """ Constructor
        Parameters:
            config : Contains JSON file parameters.
        """
        self.log = log
        self.log.info('Initialize Data Ingestion Manager')
        self.DataInMgr = DataIngestionManager(config.data_ingestion_manager)
        self.log.info('Loading Triggers')
        self.trigger_ex = \
            ThreadPoolExecutor(max_workers=config.trigger_threads)
        self.pool_ex = \
            ThreadPoolExecutor(max_workers=config.trigger_threads)

        self.trigger_thread = threading.Thread(target=self.trigger_process)
        self.trigger_queue = queue.Queue(maxsize=MAX_BUFFERS)
        self.trigger_ev = threading.Event()

        self.dil_thread = threading.Thread(target=self.dil_process)
        self.dil_queue = queue.Queue(maxsize=MAX_BUFFERS)
        self.dil_ev = threading.Event()

        self.triggers = []
        self.cam_triggers = {}
        self.config = config
        try:
            self.DataInLib = DataIngestionLib(log)
        except Exception as e:
            self.log.error(e)
            os._exit(1)
        for (n, c) in config.classification['classifiers'].items():
            self.log.info('Setting up pipeline for %s classifier', n)

            # Only initialize the trigger for a classifier if the user has
            # specified its existence
            if 'trigger' in c:
                triggers = c['trigger']
                if isinstance(triggers, list):

                    # Get all the camera serial numbers and create trigger
                    # for each camera
                    cameras_sn = get_cameras_sn(self.config)
                    self.log.info("Creating triggers for cameras: {0}".format(
                        cameras_sn))

                    for camera_sn in cameras_sn:
                        # Load the first trigger, and register it to its
                        # supported ingestors
                        prev_trigger = self._init_trigger(triggers[0],
                                                          register=True)

                        self.cam_triggers.update({camera_sn: prev_trigger})
                        prev_name = triggers[0]

                        # Load the rest of the trigger pipeline
                        for t in triggers[1:]:
                            # Initialize the trigger
                            trigger = self._init_trigger(t)

                            # Register it to receive data from the previous
                            # trigger in the pipeline
                            prev_trigger.register_trigger_callback(
                                lambda data: self._on_trigger_data(
                                    trigger, prev_name, data, filtering=True))

                            # Set previous trigger
                            prev_trigger = trigger
                            prev_name = t

                        # Register the callback with last trigger.
                        prev_trigger.register_trigger_callback(
                            lambda data: self._on_trigger(prev_name, data))
                else:
                    # Only the single trigger.
                    for camera_sn in cameras_sn:
                        trigger = self._init_trigger(triggers, register=True)
                        trigger.register_trigger_callback(
                            lambda data: self._on_trigger(triggers, data))
                        self.cam_triggers.update({camera_sn: trigger})
            else:
                for i in ['video', 'video_file']:
                    if self.DataInMgr.has_ingestor(i):
                        self.DataInMgr.register_interest(
                                i,
                                lambda i, d: self._on_trigger(
                                    None, [(1, 1, d)]))

    def _init_trigger(self, name, register=False):
        """Initialize trigger
        """
        self.log.info('Loading trigger %s', name)

        if name not in self.config.triggers:
            raise VideoIngestionError(
                    ('Trigger \'{}\' is not specified in the '
                        'configuration').format(name))

        config = self.config.triggers[name]
        trigger = load_trigger(name, config)

        if register:
            registered = False
            ingestors = trigger.get_supported_ingestors()
            for i in ingestors:
                if self.DataInMgr.has_ingestor(i):
                    self.log.info(
                            'Registering %s trigger to %s ingestor', name, i)
                    self.DataInMgr.register_interest(
                            i, lambda i, d: self._on_trigger_data(
                                    trigger, i, d))
                    registered = True

            if not registered:
                raise VideoIngestionError(
                    ('None of the supported ingestors are loaded for '
                     'trigger: {}').format(i))

        self.triggers.append(trigger)

        return trigger

    def run(self):
        """Run Video Ingestion.
        """
        self.log.info('Starting video ingestion')
        self.DataInMgr.start()
        self.trigger_thread.start()
        self.dil_thread.start()
        self.DataInMgr.join()

    def stop(self):
        """ Stop the Video Ingestion.
        """
        self.log.info('Exiting from Video Ingestion')
        sys.exit(1)

    def _on_trigger_data(self, trigger, ingestor, data, filtering=False):
        """Private method to submit a worker to execute a trigger on ingestion
        data.
        """
        self.trigger_queue.put((trigger, ingestor, data, filtering))

    def _on_trigger_done(self, fut):
        """Private method to log errors if they occur in the trigger method.
        """
        exc = fut.exception()
        if exc is not None:
            self.log.error('Error in trigger: \n%s', tb.format_exc(exc))

    def _on_filter_trigger_data(self, ingestor, trigger, data):
        """Private method for passing data onto a trigger setup to filter data.
        """
        # TODO: Could probably optimize to not use entire executor thread for
        # this ingestor

        for i in data:
            if i is None:
                break
            # Unpacking the data
            video_data = i
            # Send the data through the trigger
            trigger.process_data(ingestor, video_data)

    def _on_trigger(self, trigger, data):
        """Private method for handling the trigger start event, starting a
        thread to process all of the incoming data from the trigger.
        """
        self.log.info('Received start signal from trigger "%s"', trigger)
        self.dil_queue.put(data)

    def _data_ingest_done(self, fut):
        """Private method to log errors if they occur whilel processing frames.
        """
        exc = fut.exception()
        if exc is not None:
            self.log.error(
                    'Error while classifying frames:\n%s', tb.format_exc(exc))

    def trigger_process(self):
        while not self.trigger_ev.is_set():
            pending = self.trigger_ex._work_queue.qsize()
            if pending < self.config.trigger_threads:
                trigger, ingestor, data, filtering = self.trigger_queue.get()
                cam_sn = data[0]
                self.log.debug('Trigger {0} for cam_sn: {1}'.format(
                    self.cam_triggers.get(cam_sn), cam_sn))
                if filtering:
                    fut = self.trigger_ex.submit(
                            self._on_filter_trigger_data, ingestor,
                            self.cam_triggers.get(cam_sn), data)
                else:
                    fut = self.trigger_ex.submit(
                        self.cam_triggers.get(cam_sn).process_data,
                        ingestor, data)
                fut.add_done_callback(self._on_trigger_done)
            else:
                time.sleep(0.01)

    def dil_process(self):
        while not self.dil_ev.is_set():
            pending = self.pool_ex._work_queue.qsize()
            if pending < self.config.trigger_threads:
                data = self.dil_queue.get()

                fut = self.pool_ex.submit(self._data_ingest, data)
                fut.add_done_callback(self._data_ingest_done)
            else:
                time.sleep(0.01)

    def frame_encode(self, frame, config):
        if config["encoding"]["type"] == "jpg":
            if config["encoding"]["level"] in range(0, 101):
                result, frame = cv2.imencode('.jpg', frame,
                                             [int(cv2.IMWRITE_JPEG_QUALITY),
                                              config["encoding"]["level"]])
            else:
                self.log.error("JPG Encoding value must be between 0-100")
        elif config["encoding"]["type"] == "png":
            if config["encoding"]["level"] in range(0, 10):
                result, frame = cv2.imencode('.png', frame,
                                             [int(cv2.IMWRITE_PNG_COMPRESSION),
                                              config["encoding"]["level"]])
            else:
                self.log.error("PNG Encoding value must be between 0-9")
        else:
            self.log.error(config["encoding"]["type"] + "is not supported")
        return frame

    def frame_resize(self, frame, config):
        width, height = config["resize_resolution"].split("x")
        frame = cv2.resize(frame, (int(width), int(height)),
                           interpolation=cv2.INTER_AREA)
        return frame

    def _data_ingest(self, data):

        for res in data:
            if res is None:
                break
            sample_num, user_data, (cam_sn, frame, config) = res
            resolution = config.get('resize_resolution', None)
            encoding = config.get('encoding', None)
            storage = config.get('img_store_type', 'inmemory')

            # Get the video frame info.
            height, width, channels = frame.shape
            # Add the video buffer handle, info to the datapoint.
            dp = self.DataInLib.init_data_point()
            # TODO: This should go away once we have identified proper input
            #       streams (this is for video file or video with just 1
            #       single stream)
            if "serial" not in cam_sn:
                dp.set_measurement_name(MEASUREMENT_NAME)
            else:
                dp.set_measurement_name(cam_sn)
            try:
                if resolution is not None:
                    width, height = config["resize_resolution"].split("x")
                    frame = self.frame_resize(frame, config)

                if encoding is not None:
                    frame = self.frame_encode(frame, config)

                ret = dp.add_fields("vid-fr-" + storage, frame.tobytes(),
                                    storage)
                assert (ret is not False), 'Captured buffer could be added to\
                    DataPoint'

            except Exception as e:
                self.log.error(e)

            ret = dp.add_fields("Width", float(width))
            assert ret is not False, "Adding ofwidth to DataPoint Failed"
            ret = dp.add_fields("Height", float(height))
            assert ret is not False, "Adding of height to DataPoint Failed"
            ret = dp.add_fields("Channels", float(channels))
            assert ret is not False, "Adding of channels to DataPoint Failed"
            ret = dp.add_fields("Cam_Sn", cam_sn)
            assert ret is not False, "Adding of Camera SN to DataPoint Failed"
            ret = dp.add_fields("Sample_num", sample_num)
            assert ret is not False, "Adding of Sample Num to DataPoint Failed"
            ret = dp.add_fields("user_data", user_data)
            assert ret is not False, "Adding of user_data to DataPoint Failed"
            if encoding is not None:
                ret = dp.add_fields("encoding", config["encoding"]["type"])
                assert ret is not False, "Adding of encoding to DataPoint\
                     Failed"
            try:
                ret = self.DataInLib.save_data_point(dp)
                assert ret is not False, "Saving of DataPoint Failed"
            except Exception as e:
                self.log.error(e)
                os._exit(1)


def parse_args():
    """Parse command line arguments
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', default='factory_video_file.json',
                        help='JSON configuration file')
    parser.add_argument('--log', choices=LOG_LEVELS.keys(), default='INFO',
                        help='Logging level (df: INFO)')
    parser.add_argument('--log-dir', dest='log_dir', default='logs',
                        help='Directory to for log files')
    parser.set_defaults(func=run_videopipeline)

    return parser.parse_args()


def main():
    """Main method
    """
    # Parse command line arguments
    args = parse_args()

    currentDateTime = str(datetime.datetime.now())
    listDateTime = currentDateTime.split(" ")
    currentDateTime = "_".join(listDateTime)
    logFileName = 'videoingestion_' + currentDateTime + '.log'

    # Creating log directory if it does not exist
    if not os.path.exists(args.log_dir):
        os.mkdir(args.log_dir)

    log = configure_logging(args.log.upper(), logFileName, args.log_dir,
                            __name__)
    try:
        # Read in the configuration file and initialize needed objects
        config = Configuration(args.config)
    except KeyError as e:
        log.error('!!! ERROR: Configuration missing key: {}'.format(str(e)))
        return -1

    args.func(config, log)


def run_videopipeline(config, log):
    """ Method to run the VideoPipeline.
    """
    log.info('Initializing the factory agent')

    agent = VideoIngestion(config, log)

    def handle_signal(signum, frame):
        log.info('IEI killed...')
        agent.stop()

    signal.signal(signal.SIGTERM, handle_signal)

    try:
        agent.run()
    except KeyboardInterrupt:
        log.error('Quitting...')
    except Exception:
        log.error('Error during execution:\n%s', tb.format_exc())
    finally:
        agent.stop()


if __name__ == '__main__':
    main()
