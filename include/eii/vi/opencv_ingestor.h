// Copyright (c) 2019 Intel Corporation.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM,OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef _EIS_VI_OPENCV_H
#define _EIS_VI_OPENCV_H

#include <opencv2/opencv.hpp>
#include <eis/utils/thread_safe_queue.h>
#include "eis/vi/ingestor.h"


namespace eis {
    namespace vi {

        /**
         * OpenCV ingestor
         */
        class OpenCvIngestor : public Ingestor {
        private:
            // OpenCV video capture object
            cv::VideoCapture* m_cap;

            // Flag for if the frame should be resized
            bool m_resize;

            // Resize parameters
            int m_width;
            int m_height;

            // Flag for if encoding/compression is needed
            bool m_encoding;

            // video source
            std::string m_pipeline;

        protected:
            /**
             * Overridden read method.
             */
            void read(Frame*& frame) override;

        public:
            /**
             * Constructor
             */
            OpenCvIngestor(config_t* config, FrameQueue* frame_queue);

            /**
             * Destructor
             */
            ~OpenCvIngestor();
        };

    } // vi
} // eis

#endif // _EIS_VI_OPENCV_H