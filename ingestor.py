# Copyright (c) 2019 Intel Corporation.

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import zmq
import cv2
import os
import threading
import json
import queue
import time
import uuid
import os
import logging

MAX_CAM_FAIL_COUNT = 10
MAX_CAM_CONN_RETRY = 5

class Ingestor:

    def __init__(self, ingestor_config, ingestor_queue):
        """Constructor
        Parameters
        ----------
        ingestor_config : dict
            Configuration object for the video ingestor
        ingestor_queue : queue
            Ingestor Queue
        """
        self.log = logging.getLogger(__name__)
        self.ingestor_queue = ingestor_queue
        self.video_src = ingestor_config['video_src']
        self.topic = os.environ['PubTopics'].split(",")[0]
        self.poll_interval = ingestor_config.get('poll_interval', None)
        self.stop_ev = threading.Event()
        self.loop_video = ingestor_config.get("loop_video", None)
        self.encoding = ingestor_config.get("encoding", None)
        self.resolution = ingestor_config.get("resolution", None)

    def start(self):
        """
        Starts the ingestor thread
        """
        self.log.info("=====Starting ingsestor thread======")
        self.thread = threading.Thread(target=self.run)
        self.thread.setDaemon(True)
        self.thread.start()

    def connect(self):
        """
        To connect to a camera or a video source
        """
        if self.video_src is not None:
            self.log.info("initializing cv2 videocapture")
            cap = cv2.VideoCapture(self.video_src)
            if cap.isOpened() is False:
                cap.release()
            return cap
        else:
            self.log.error("Invalid video source: {}".format(self.video_src))

    def run(self):
        """
        To read frames from a camera or video source
        """
        camFailCount = 0
        cap =  self.connect()
        while not self.stop_ev.is_set():
            try:
                ret, frame = cap.read()
                if not ret:
                    if self.loop_video is not None:
                        self.loop_video = self.loop_video.lower()
                    if self.loop_video == "true":
                        cap.release()
                        cap = cv2.VideoCapture(self.video_src)
                        continue
                    elif self.loop_video == "false":
                        break
                    camFailCount = camFailCount + 1
                    self.log.error("Failed to retrive frame from camera")
                    if camFailCount >= MAX_CAM_FAIL_COUNT:
                        raise Exception("Too many fails. Retry Connection")
                else:
                    try:
                        metadata = {
                            'encoding': self.encoding,
                            'resolution': self.resolution
                        }
                        data = [self.topic, metadata, frame]
                        self.log.debug("Data added to ingestor queue...")
                        self.ingestor_queue.put(data)
                    except Exception as ex:
                        self.log.exception('Exception: {}'.format(ex))
                    camFailCount = 0
            except Exception as ex:
                self.log.exception('Exception: {}'.format(ex))
                try:
                    camConnRetry = MAX_CAM_CONN_RETRY
                    while(camConnRetry):
                        self.log.info("Attempting to reconnect to camera with\
                                      iteration:%s", camConnRetry)
                        cap.release()
                        cap = self.connect()
                        if cap is not None:
                            self.log.info("Re-Connected the camera again...")
                            camConnRetry = MAX_CAM_CONN_RETRY
                            break
                        camConnRetry = camConnRetry - 1
                    else:
                        raise Exception(
                            "Maximum connection Retry completed...")
                except Exception as ex:
                    self.log.error(ex)
                    break
            if self.poll_interval is not None:
                time.sleep(self.poll_interval)
        cap.release()
        self.log.info("=====Stopped ingestor thread======")


    def stop(self):
        """
        Stops the ingestor thread
        """
        self.stop_ev.set()

    def join(self):
        """
        Blocks until the ingestor thread stops running
        """
        self.thread.join()

