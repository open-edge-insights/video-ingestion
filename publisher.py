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

import zmq
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
from libs.common.py.util import get_topics_from_env,\
                                get_messagebus_config_from_env
import eis.msgbus as mb


class Publisher:

    def __init__(self, filter_output_queue):
        """Constructor

        Parameters
        ----------
        filter_output_queue : Queue
            Input queue for publisher (has [topic, metadata, keyframe] data
            entries)
        """
        self.log = logging.getLogger(__name__)
        self.filter_output_queue = filter_output_queue
        self.stop_ev = threading.Event()
        self.resolution = None
        self.encoding = None

    def start(self):
        """
        Starts the publisher thread(s)
        """
        self.context = zmq.Context()
        socket = self.context.socket(zmq.PUB)
        topics = get_topics_from_env("pub")
        self.publisher_threadpool = ThreadPoolExecutor(max_workers=len(topics))
        self.sockets = []
        for topic in topics:
            topic_cfg = get_messagebus_config_from_env(topic, "pub")
            self.publisher_threadpool.submit(self.publish, socket, topic,
                                             topic_cfg)

    def publish(self, socket, topic, config):
        """
        Send the data to the publish topic
        Parameters:
        ----------
        socket: ZMQ socket
            socket instance
        topic: str
            topic name
        config: str
            topic config
        """

        self.msgbus = mb.MsgbusContext(config)
        self.publisher = self.msgbus.new_publisher(topic)

        thread_id = threading.get_ident()
        log_msg = "Thread ID: {} {} with topic:{} and topic_cfg:{}"
        self.log.info(log_msg.format(thread_id, "started", topic, config))
        self.log.info("Publishing to topic: {}...".format(topic))

        while not self.stop_ev.is_set():
            try:
                metadata, frame = self.filter_output_queue.get()
                if "resolution" in metadata:
                    self.resolution = metadata["resolution"]
                if "encoding_type" and "encoding_level" in metadata:
                    self.encoding = {"type": metadata["encoding_type"],
                                     "level": metadata["encoding_level"]}
                if self.resolution is not None:
                    width, height = self.resolution.split("x")
                    frame = self.resize(frame)
                height, width, channel = frame.shape
                if "encoding_type" and "encoding_level" in metadata:
                    frame = self.encode(frame)
                metadata['height'] = height
                metadata['width'] = width
                metadata['channel'] = channel
                # img_handle field will be used by the ImageStore
                # container to add the `frame blob` as value with
                # `img_handle` as key into ImageStore DB
                metadata['img_handle'] = str(uuid.uuid1())[:8]
                self.publisher.publish((metadata, frame.tobytes()))
                self.log.debug("Published data: {}".format(metadata))
            except Exception as ex:
                self.log.exception('Error while publishing data:{}'.format(ex))
                log_msg = "Thread ID: {} {} with topic:{} and topic_cfg:{}"
                self.log.info(log_msg.format(thread_id, "stopped",
                                             topic, config))

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

    def resize(self, frame):
        width, height = self.resolution.split("x")
        frame = cv2.resize(frame, (int(width), int(height)),
                           interpolation=cv2.INTER_AREA)
        return frame

    def stop(self):
        """
        Stops the publisher thread
        """
        try:
            self.stop_ev.set()
            self.publisher_threadpool.shutdown(wait=False)
            for socket in self.sockets:
                socket.close()
                if socket._closed == "False":
                    self.log.error("Unable to close socket connection")
            self.context.term()
        except Exception as ex:
            self.log.exception(ex)
