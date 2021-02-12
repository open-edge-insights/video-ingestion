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

/**
 * @file
 * @brief OpenCV Ingestor interface
 */

#ifndef _EII_VI_OPENCV_H
#define _EII_VI_OPENCV_H

#include <opencv2/opencv.hpp>
#include <eii/utils/thread_safe_queue.h>
#include "eii/vi/ingestor.h"


namespace eii {
    namespace vi {

        /**
         * OpenCV ingestor
         */
        class OpenCvIngestor : public Ingestor {
        private:
            // OpenCV video capture object
            cv::VideoCapture* m_cap;

            // Resize parameters
            int m_width;
            int m_height;

            // Flag for if encoding/compression is needed
            bool m_encoding;

            // video source
            std::string m_pipeline;

            // video loop option
            bool m_loop_video;

        protected:
            /**
             * Overridden run method.
             */
            void run(bool snapshot_mode=false) override;

            /**
             * Overridden read method.
             */
            void read(udf::Frame*& frame) override;

        public:
            /**
             * Constructor
             * @param config        - Ingestion config
             * @param frame_queue   - Frame Queue context
             * @param service_name  - Service Name env variable
             * @param snapshot_cv   - Snapshot condition variable
             * @param enc_type      - Frame encoding type(Optional)
             * @param enc_lvl       - Frame encoding level(Optional)
             */
            OpenCvIngestor(config_t* config, FrameQueue* frame_queue, std::string service_name, std::condition_variable& snapshot_cv, EncodeType enc_type, int enc_lvl);

            /**
             * Destructor
             */
            ~OpenCvIngestor();

           /**
            * Overridden stop method.
            */
           void stop() override;

        };

    } // vi
} // eii

#endif // _EII_VI_OPENCV_H
