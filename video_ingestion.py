
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
from ingestor import Ingestor
from libs.base_filter import load_filter
from libs.log import configure_logging, LOG_LEVELS
from libs.ConfigManager.etcd.py.etcd_client import EtcdCli
from publisher import Publisher

# Etcd paths
INGESTOR_KEY_PATH = "/ingestor"
INGESTOR_NAME_PATH = INGESTOR_KEY_PATH + "/ingestor_name"
FILTER_KEY_PATH = "/filter"
FILTER_NAME_PATH = FILTER_KEY_PATH + "/filter_name"


class VideoIngestion:

    def __init__(self):
        """Constructor

        Returns
        -------
            VideoIngestion object
        """
        self.log = logging.getLogger(__name__)
        self.profiling = bool(strtobool(os.environ['PROFILING']))
        self.dev_mode = bool(strtobool(os.environ["DEV_MODE"]))
        self.app_name = os.environ["AppName"]
        conf = {
            "certFile": "",
            "keyFile": "",
            "trustFile": ""
        }

        self.etcd_cli = EtcdCli(conf)
        self.ingestor_name = self.etcd_cli.GetConfig("/{0}{1}".format(
            self.app_name, INGESTOR_NAME_PATH))
        self.ingestor_config = self.etcd_cli.GetConfig("/{0}{1}/{2}".format(
            self.app_name, INGESTOR_KEY_PATH, self.ingestor_name))

        self.filter_name = self.etcd_cli.GetConfig("/{0}{1}".format(
            self.app_name, FILTER_NAME_PATH))
        self.filter_config = self.etcd_cli.GetConfig("/{0}{1}/{2}".format(
            self.app_name, FILTER_KEY_PATH, self.filter_name))

        self.log.info('ingestor_name: {}, ingestor_config: {}'.format(
            self.ingestor_name, self.ingestor_config))
        self.log.info('filter name: {}, filter config: {}'.format(
            self.filter_name, self.filter_config))

        self.ingestor_config = json.loads(self.ingestor_config)
        self.filter_config = json.loads(self.filter_config)

        self.etcd_cli.RegisterDirWatch("/{0}/".format(self.app_name)
            , self.onChangeConfigCB)

    def start(self):
        """Start Video Ingestion.
        """
        self.log.info('=======Starting {}======='.format(self.app_name))
        queue_size = self.filter_config["queue_size"]
        self.filter_input_queue = queue.Queue(
            maxsize=queue_size)
        self.filter_output_queue = queue.Queue(
            maxsize=queue_size)

        self.publisher = Publisher(self.filter_output_queue)
        self.publisher.start()

        self.filter = load_filter(
            self.filter_name, self.filter_config, self.filter_input_queue, self.filter_output_queue)
        self.filter.start()

        self.ingestor = Ingestor(self.ingestor_config, self.filter_input_queue)
        self.ingestor.start()
        self.log.info('=======Started {}======='.format(self.app_name))

    def stop(self):
        """ Stop the Video Ingestion.
        """
        self.log.info('=======Stopping {}======='.format(self.app_name))
        self.ingestor.stop()
        self.filter.stop()
        self.publisher.stop()
        self.log.info('=======Stopped {}======='.format(self.app_name))

    def onChangeConfigCB(self, key, value):
        """
        Callback method to be called by etcd

        Parameters:
        ----------
        key: str
            etcd key
        value: str
            etcd value
        """
        # TODO: To be added:
        # 1. Add logic to control restart of filter/ingestor or publisher
        #    alone based on the config change instead of restarting all threads.
        self.log.info("{}:{}".format(key, value))
        try:
            if "_filter" in value:
                filter_config = self.etcd_cli.GetConfig("/{0}{1}/{2}".format(
                    self.app_name, FILTER_KEY_PATH, value))
                self.filter_name = value
                self.filter_config = json.loads(filter_config)
            elif "_filter" in key:
                self.filter_name = key.split("/")[3]
                self.filter_config = json.loads(value) # filter json config

            if "_ingestor" in value:
                ingestor_config = self.etcd_cli.GetConfig("/{0}{1}/{2}".format(
                    self.app_name, INGESTOR_KEY_PATH, value))
                self.ingestor_name = value
                self.ingestor_config = json.loads(ingestor_config)
            elif "_ingestor" in key:
                self.ingestor_name = key.split("/")[3]
                self.ingestor_config = json.loads(value) # ingestor json config
        except:
            self.log.exception(ex)
        
        self.stop()
        self.start()

def parse_args():
    """Parse command line arguments
    """
    parser = argparse.ArgumentParser()

    parser.add_argument('--log', choices=LOG_LEVELS.keys(), default='INFO',
                        help='Logging level (df: INFO)')
    parser.add_argument('--log-dir', dest='log_dir', default='logs',
                        help='Directory to for log files')

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

    vi = VideoIngestion()

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
