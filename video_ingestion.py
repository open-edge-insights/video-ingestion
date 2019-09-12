
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


import threading
import queue
import time
import datetime
import json
import os
import signal
import logging
import argparse
from distutils.util import strtobool
import sys
from ingestor import Ingestor
from libs.base_filter import load_filter
from util.log import configure_logging, LOG_LEVELS
from libs.ConfigManager import ConfigManager
from util.msgbusutil import MsgBusUtil
from publisher import Publisher

# Default queue size for filter input queue for `no_filter` case
QUEUE_SIZE = 10


class VideoIngestion:

    def __init__(self, dev_mode, config_client):
        """Get the frames from camera or video, filters and add the results
        to the messagebus

        :param dev_mode: indicates whether it's dev or prod mode
        :type dev_mode: bool
        :param config_client: distributed store config client
        :type config_client: config client object
        """
        self.log = logging.getLogger(__name__)
        self.profiling = bool(strtobool(os.environ['PROFILING_MODE']))
        self.dev_mode = dev_mode
        self.app_name = os.environ["AppName"]
        self.config_client = config_client
        self._read_ingestor_filter_config()
        self.config_client.RegisterDirWatch("/{0}/".format(self.app_name),
                                            self._on_change_config_callback)

    def _print_config(self):
        self.log.info('ingestor_config: {}'.format(self.ingestor_config))
        if self.filter_config:
            self.log.info('filter name: {} filter config: {}'.format(
                self.filter_config["name"], self.filter_config))

    def _read_ingestor_filter_config(self):
        CONFIG_KEY_PATH = "/config"
        self.config = self.config_client.GetConfig("/{0}{1}".format(
            self.app_name, CONFIG_KEY_PATH))

        self.config = json.loads(self.config)
        self.ingestor_config = self.config["ingestor"]
        self.filter_config = self.config.get("filter", None)

        self.filter_input_queue = queue.Queue(
            maxsize=QUEUE_SIZE)

    def start(self):
        """Start Video Ingestion.
        """
        log_msg = "======={} {}======="
        self.log.info(log_msg.format("Starting", self.app_name))

        self._print_config()

        pub_topic = MsgBusUtil.get_topics_from_env("pub")

        if len(pub_topic) > 1:
            self.log.error("More than one publish topic in {} " +
                           "is not allowed".format(self.app_name))
            sys.exit(1)

        if self.filter_config:
            queue_size = self.filter_config["queue_size"]
            self.filter_input_queue = queue.Queue(
                maxsize=queue_size)
            self.filter_output_queue = queue.Queue(
                maxsize=queue_size)
        else:
            # for `no filter` cfg
            self.filter_output_queue = self.filter_input_queue

        pub_topic = pub_topic[0]
        self.publisher = Publisher(self.filter_output_queue, pub_topic,
                                   self.config_client, self.dev_mode)
        self.publisher.start()

        if self.filter_config:
            self.filter = load_filter(self.filter_config["name"],
                                      self.filter_config,
                                      self.filter_input_queue,
                                      self.filter_output_queue)
            self.filter.start()

        self.ingestor = Ingestor(self.ingestor_config, self.filter_input_queue)
        self.ingestor.start()
        self.log.info(log_msg.format("Started", self.app_name))

    def stop(self):
        """ Stops Video Ingestion pipeline
        """
        log_msg = "======={} {}======="
        self.log.info(log_msg.format("Stopping", self.app_name))
        os._exit(1)

    def _on_change_config_callback(self, key, value):
        """Callback method to be called by etcd

        :param key: Etcd key
        :type key: str
        :param value: Etcd value
        :type value: str
        """
        self.log.info("key: {}, value: {}".format(key, value))
        value = json.loads(value)
        ingestor_cfg = value["ingestor"]
        filter_cfg = value.get("filter", None)
        if filter_cfg != self.filter_config:
            self.log.info("Filter cfg: {}".format(filter_cfg))
            os._exit(0)
        if ingestor_cfg != self.ingestor_config:
            self.log.info("Ingestor cfg: {}".format(ingestor_cfg))
            self.ingestor.stop()
            self.ingestor_config = ingestor_cfg
            self.ingestor = Ingestor(self.ingestor_config,
                                     self.filter_input_queue)
            self.ingestor.start()
        else:
            self.log.info("Received same ingestor config...")


def main():
    """Main method
    """
    dev_mode = bool(strtobool(os.environ["DEV_MODE"]))
    conf = {
        "certFile": "",
        "keyFile": "",
        "trustFile": ""
    }
    if not dev_mode:
        conf = {
            "certFile": "/run/secrets/etcd_VideoIngestion_cert",
            "keyFile": "/run/secrets/etcd_VideoIngestion_key",
            "trustFile": "/run/secrets/ca_etcd"
        }
    cfg_mgr = ConfigManager()
    config_client = cfg_mgr.get_config_client("etcd", conf)

    log = configure_logging(os.environ['PY_LOG_LEVEL'].upper(),
                            __name__)

    vi = VideoIngestion(dev_mode, config_client)

    def handle_signal(signum, frame):
        log.info('Video Ingestion program killed...')
        vi.stop()

    signal.signal(signal.SIGTERM, handle_signal)

    try:
        vi.start()
    except Exception as ex:
        log.exception('Exception: {}'.format(ex))
        vi.stop()


if __name__ == '__main__':
    main()
