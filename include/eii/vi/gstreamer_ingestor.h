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
 * @brief Gstreamer ingestor interface
 */

#ifndef _EIS_VI_GSTREAMER_H
#define _EIS_VI_GSTREAMER_H

#include <gst/gst.h>
#include <glib.h>
#include <eis/utils/thread_safe_queue.h>
#include <eis/utils/json_config.h>
#include <eis/udf/frame.h>
#include "eis/vi/ingestor.h"

namespace eis {
    namespace vi {

        /**
         * GStreamer Ingestor
         */
        class GstreamerIngestor : public Ingestor {

            private:
                // Gstreamer state/elements
                GstElement* m_gst_pipeline;
                GstElement* m_sink;
                guint m_bus_watch_id;

                // Glib main loop
                GMainLoop* m_loop;
                int64_t m_frame_count;

                /**
                 * Gstreamer initialization function
                 */
                void gstreamer_init();

                static GstFlowReturn new_sample(GstElement* sink, GstreamerIngestor* ctx);

            protected:
                /**
                 * Overridden run thread method.
                 */
                void run() override;

                /**
                 * Overridden frame method.
                 */
                void read(udf::Frame*& frame);

            public:
                /**
                 * Constructor
                 */
                GstreamerIngestor(config_t* config, FrameQueue* frame_queue, EncodeType enc_type, int enc_lvl);

                /**
                 * Destructor
                 */
                ~GstreamerIngestor();

                /**
                 * Overridden stop method.
                 */
                void stop() override;

        };

    } // vi
} // eis

#endif // _EIS_VI_GSTREAMER_H
