# Copyright (c) 2019 Intel Corporation.

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import sys
import os
import numpy as np
import time
import threading
import json
import cv2
import uuid
import os
import logging
import subprocess
from concurrent.futures import ThreadPoolExecutor
from util.msgbusutil import MsgBusUtil
import eis.msgbus as mb
from distutils.util import strtobool


class Publisher:
    def __init__(self, filter_output_queue, topic, config_client, dev_mode):
        """Publisher will get the filtered data from the filter queue and
           send it to EIS Message Bus

        :param filter_output_queue: Input queue for publisher (has [topic,
                                    metadata, keyframe] data entries)
        :type filter_output_queue: queue
        :param topic: Topic on which data is published
        :type topic: String
        :param config_client: Used to get keys value from ETCD.
        :type config_client: Class Object
        :param dev_mode: To check whether it is running in production mode or
                         development
        :type dev_mode: Boolean
        """
        self.log = logging.getLogger(__name__)
        self.filter_output_queue = filter_output_queue
        self.stop_ev = threading.Event()
        self.encoding = None
        self.topic = topic
        self.config_client = config_client
        self.dev_mode = dev_mode
        self.profiling = bool(strtobool(os.environ['PROFILING_MODE']))

    def start(self):
        """Starts the publisher thread(s)
        """
        self.publisher_threadpool = \
            ThreadPoolExecutor(max_workers=len(self.topic))
        subscribers = os.environ['Clients'].split(",")
        msgbus_cfg = \
            MsgBusUtil.get_messagebus_config(self.topic, "pub", subscribers,
                                             self.config_client,
                                             self.dev_mode)

        self.publisher_threadpool.submit(self.publish, msgbus_cfg)

    def publish(self, msgbus_cfg):
        """Send the data to the publish topic

        :param topic: Publishers's topic name
        :type topic: str
        :param msgbus_cfg: Topic msgbus_cfg
        :type msgbus_cfg: str
        """
        publisher = None
        try:
            msgbus = mb.MsgbusContext(msgbus_cfg)
            publisher = msgbus.new_publisher(self.topic)

            thread_id = threading.get_ident()
            log_msg = "Thread ID: {} {} with topic:{} and msgbus_cfg:{}"
            self.log.info(log_msg.format(thread_id, "started",
                                         self.topic,
                                         msgbus_cfg))
            self.log.info("Publishing to topic: {}...".format(self.topic))
            while not self.stop_ev.is_set():
                metadata, frame = self.filter_output_queue.get()

                if self.profiling is True:
                    ts_vi_entry = int(metadata['ts_vi_entry'])
                    ts_vi_queue_exit = round(time.time()*1000)
                    metadata['ts_vi_queue_wait'] = \
                        ts_vi_queue_exit - ts_vi_entry

                if "encoding_type" and "encoding_level" in metadata:
                    self.encoding = {"type": metadata["encoding_type"],
                                     "level": metadata["encoding_level"]}

                if len(frame.shape) == 3:
                    height, width, channel = frame.shape
                elif len(frame.shape) == 2:
                    height, width = frame.shape
                    channel = 3

                if self.profiling is True:
                    metadata['ts_vi_encode_start'] = \
                        str(round(time.time()*1000))

                if "encoding_type" and "encoding_level" in metadata:
                    frame = self.encode(frame)

                if self.profiling is True:
                    metadata['ts_vi_encode_end'] = str(round(time.time()*1000))

                metadata['height'] = height
                metadata['width'] = width
                metadata['channel'] = channel
                # img_handle field will be used by the ImageStore
                # container to add the `frame blob` as value with
                # `img_handle` as key into ImageStore DB
                metadata['img_handle'] = str(uuid.uuid1())[:8]

                if self.profiling is True:
                    metadata['ts_vi_exit'] = str(round(time.time()*1000))

                publisher.publish((metadata, frame.tobytes()))
                self.log.debug("Published data: {} on topic: {} with \
                               config: {}...".format(metadata, self.topic,
                                                     msgbus_cfg))
        except Exception as ex:
            self.log.exception('Error while publishing data:{}'.format(ex))
        finally:
            if publisher is not None:
                publisher.close()
        self.log.info(log_msg.format(thread_id, "stopped", self.topic,
                                     msgbus_cfg))

    def encode(self, frame):
        if self.encoding["type"] == "jpg":
            if self.encoding["level"] in range(0, 101):
                result, frame = cv2.imencode('.jpg', frame,
                                             [int(cv2.IMWRITE_JPEG_QUALITY),
                                              self.encoding["level"]])
            else:
                self.log.info("JPG Encoding value must be between 0-100")
        elif self.encoding["type"] == "png":
            if self.encoding["level"] in range(0, 10):
                result, frame = cv2.imencode('.png', frame,
                                             [int(cv2.IMWRITE_PNG_COMPRESSION),
                                              self.encoding["level"]])
            else:
                self.log.info("PNG Encoding value must be between 0-9")
        else:
            self.log.info(self.encoding["type"] + "is not supported")
        return frame

    def stop(self):
        """Stops the publisher thread
        """
        try:
            self.stop_ev.set()
            self.publisher_threadpool.shutdown(wait=False)
        except Exception as ex:
            self.log.exception(ex)
