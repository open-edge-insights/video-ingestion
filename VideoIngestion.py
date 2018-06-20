import os
import signal
import argparse
import logging

import traceback as tb
from concurrent.futures import ThreadPoolExecutor
from agent.dpm.triggers import load_trigger
from agent.dpm.ingestion.data_ingestion_manager\
    import DataIngestionManager
from DataIngestionLib.DataIngestionLib import DataIngestionLib
from agent.etr_utils.log import configure_logging, LOG_LEVELS
from agent.dpm.config import Configuration


class VideoIngestionError(Exception):
    """ Exception raised by VideoIngestion.
    """
    pass


class VideoIngestion:
    """ This module ingests camera output to ImageStore using DataIngestion lib.
    """
    def __init__(self, config):
        """ Constructor
        Parameters:
            config : Contains JSON file parameters.
        """
        self.log = logging.getLogger(__name__)
        self.log.info('Initialize Data Ingestion Manager')
        self.DataInMgr = DataIngestionManager(config.data_ingestion_manager)

        self.log.info('Loading Triggers')
        self.trigger_ex = ThreadPoolExecutor(max_workers=config.trigger_threads)
        self.triggers = {}
        for (trigger_name, conf) in config.triggers.items():
            self.triggers[trigger_name] = load_trigger(trigger_name, conf)
            ingestors = self.triggers[trigger_name].get_supported_ingestors()
            registered = False
            for i in ingestors:
                if self.DataInMgr.has_ingestor(i):
                    self.DataInMgr.register_interest(
                        i, lambda i, d: self._on_trigger_data(
                            self.triggers[trigger_name], i, d))
                    registered = True

            if not registered:
                raise VideoIngestionError(
                    ('None of the supported ingestors are loaded for '
                        'trigger: {}').format(trigger_name))
            trigger = self.triggers.get(trigger_name, None)
            if trigger is None:
                raise VideoIngestionError(
                    ('Trigger Not found: {}').format(trigger_name))
            self.DataInLib = DataIngestionLib()
            trigger.register_trigger_callback(
                lambda data: self._on_trigger(trigger_name, data))

    def run(self):
        """Run Video Ingestion.
        """
        self.log.info('Starting video ingestion')
        self.DataInMgr.start()
        self.DataInMgr.join()

    def stop(self):
        """ Stop the Video Ingestion.
        """
        self.log.info('Stopping Video Ingestion')
        self.DataInMgr.stop()
        for t in self.triggers.values():
            t.stop()
        self.trigger_ex.shutdown()
        self.log.info('Video Ingestion stopped')

    def _on_trigger_data(self, trigger, ingestor, data):
        """Private method to submit a worker to execute a trigger on ingestion
        data.
        """
        fut = self.trigger_ex.submit(trigger.process_data, ingestor, data)
        fut.add_done_callback(self._on_trigger_done)

    def _on_trigger_done(self, fut):
        """Private method to log errors if they occur in the trigger method.
        """
        exc = fut.exception()
        if exc is not None:
            self.log.error(
                    'Error in trigger:\n%s',
                    ''.join(tb.format_tb(exc.__traceback__)))

    def _on_trigger(self, trigger, data):
        """Private method for handling the trigger start event, starting a
        thread to process all of the incoming data from the trigger.
        """
        self.log.info('Received start signal from trigger "%s"', trigger)
        fut = self.trigger_ex.submit(self._data_ingest, data)
        fut.add_done_callback(self._data_ingest_done)

    def _data_ingest_done(self, fut):
        """Private method to log errors if they occur whilel processing frames.
        """
        exc = fut.exception()
        if exc is not None:
            self.log.error(
                    'Error while sending the video frames:\n%s',
                    ''.join(tb.format_tb(exc.__traceback__)))

    def _data_ingest(self, data):
        # Set measurement name.
        DataInLib = DataIngestionLib()
        DataInLib.set_measurement_name('stream1')
        for res in data:
            if res is None:
                break
            sample_num, user_data, (cam_sn, frame) = res
            # Get the video frame info.
            height, width, channels = frame.shape
            print(type(height), "****", type(width), "****", type(channels))
            # Add the video buffer handle, info to the datapoint.
            ret = DataInLib.add_fields("vid-fr", frame.tobytes())
            assert (ret is not False), 'Captured buffer could be added to\
                DataPoint'
            ret = DataInLib.add_fields("Width", width)
            assert ret is not False, "Adding ofwidth to DataPoint Failed"
            ret = DataInLib.add_fields("Height", height)
            assert ret is not False, "Adding of height to DataPoint Failed"
            ret = DataInLib.add_fields("Channels", channels)
            assert ret is not False, "Adding of channels to DataPoint Failed"
            ret = DataInLib.save_data_point()
            assert ret is not False, "Saving of DataPoint Failed"


def parse_args():
    """Parse command line arguments
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', default='factory.json',
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

    # Creating log directory if it does not exist
    if not os.path.exists(args.log_dir):
        os.mkdir(args.log_dir)

    try:
        # Read in the configuration file and initialize needed objects
        config = Configuration(args.config)
    except KeyError as e:
        print('!!! ERROR: Configuration missing key: {}'.format(str(e)))
        return -1

    # Configuring logging
    if config.log_file_size is not None:
        configure_logging(args.log.upper(), 'etr.log', args.log_dir,
                          max_bytes=config.log_file_size)
    else:
        configure_logging(args.log.upper(), 'etr.log', args.log_dir)
    log = logging.getLogger('MAIN')

    args.func(log, config)


def run_videopipeline(log, config):
    """ Method to run the VideoPipeline.
    """
    log.info('Initializing the factory agent')
    agent = VideoIngestion(config)

    def handle_signal(signum, frame):
        log.info('ETR killed...')
        agent.stop()

    signal.signal(signal.SIGTERM, handle_signal)

    try:
        agent.run()
    except KeyboardInterrupt:
        log.info('Quitting...')
    except Exception:
        log.error('Error during execution:\n%s', tb.format_exc())
    finally:
        agent.stop()


if __name__ == '__main__':
    main()
