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
import sys
import os
import numpy as np
import time
import threading
import argparse
import json
import cv2
import uuid
import os
import logging
import subprocess

class Publisher:

    def __init__ (self, filter_output_queue):
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
        self.sockets = []

    def start(self):
        """
        Starts the publisher thread
        """
        self.log.info("=====Starting publisher thread======")
        self.thread = threading.Thread(target=self.publish)
        self.thread.setDaemon(True)
        self.thread.start()

    def publish(self):
        """
        Publishes the data i.e. [topic, metadata, frame] to zmq
        """
        context = zmq.Context()
        self.socket = context.socket(zmq.PUB)
        topics = os.environ['PubTopics'].split(",")

        # Keeping the logic of being able to publish to multiple topics
        # with each publish happening on different/same bind socket
        # address as per the ENV configuration
        try:
            for topic in topics:
                address = os.environ["{}_cfg".format(topic)].split(",")
                mode = address[0].lower()
                if "tcp" in mode:
                    self.socket.bind("tcp://{}".format(address[1]))
                elif "ipc" in mode:
                    self.socket.bind("ipc://{}".format("{0}{1}".format(
                        "/var/run/eis/", address[1])))
                self.sockets.append(self.socket)
        except Exception as ex:
            self.log.exception(ex)

        while not self.stop_ev.is_set():
            try:
                data = self.filter_output_queue.get()
                topic = data[0]
                metadata = data[1]
                frame = data[2]

                self.encoding = metadata["encoding"]
                self.resolution = metadata["resolution"]

                if self.resolution is not None:
                    width, height = self.resolution.split("x")
                    frame = self.resize(frame)

                height, width, channel = frame.shape

                if self.encoding is not None:
                    frame = self.encode(frame)

                metadata['height'] = height
                metadata['width'] = width
                metadata['channel'] = channel
                # imgHandle field will be used by the ImageStore
                # container to add the `frame blob` as value with
                # `imgHandle` as key into ImageStore DB
                metadata['imgHandle'] = str(uuid.uuid1())[:8]
                metaData = json.dumps(metadata).encode()
                data = [topic.encode(), metaData, frame]
                if self.socket._closed is False:
                    self.socket.send_multipart(data, copy=False)
            except Exception as ex:
                self.log.exception('Error while publishing data: {}'.format(ex))
            self.log.debug("Published data: {}".format(data))
        self.log.info("=====Stopped publisher thread======")

    def encode(self,frame):
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


    def resize(self,frame):
        width, height = self.resolution.split("x")
        frame = cv2.resize(frame, (int(width), int(height)),
                           interpolation=cv2.INTER_AREA)
        return frame

    def stop(self):
        """
        Stops the publisher thread
        """
        for socket in self.sockets:
            socket.close()
            if socket._closed == "False":
                self.log.error("Unable to close socket connection")
        self.stop_ev.set()

    def join(self):
        """
        Blocks until the publisher thread stops running
        """
        self.thread.join()
