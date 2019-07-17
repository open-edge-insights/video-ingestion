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


import logging
import cv2
import numpy as np
from libs.base_filter import BaseFilter
import threading


class Filter(BaseFilter):
    """Bypass filter to send all the frames without any filter logic to
    select key frames.
    """

    def __init__(self, filter_config, input_queue, output_queue):
        """Constructor

        Parameters
        ----------
        filter_config : dict
            Configuration object for the filter
        input_queue : Queue
            input queue for filter
        output_queue : Queue
            output queue of filter
       
        Returns
        -------
        Filter object
        """
        super().__init__(filter_config, input_queue, output_queue)
        self.log = logging.getLogger('BYPASS_FILTER')
        self.training_mode = filter_config.get("training_mode", False)
        self.count = 0

    def on_data(self):
        """Runs video frames from filter input queue and adds only the key 
        frames to filter output queue based on the filter logic used
        """
        thread_id = threading.get_ident()
        self.log.info("Filter thread ID: {} started...".format(thread_id))
        while True:
            data = self.input_queue.get()
            if self.training_mode is True:
                self.count = self.count + 1
                cv2.imwrite("./frames/"+str(self.count)+".png", data[1])
            else:
                # Sending Frames to Store
                self.log.debug("Sending frame. Thread id: {}".format(threading.get_ident()))
                self.send_data(data)
        self.log.info("Filter thread ID: {} exited...".format(thread_id))
        