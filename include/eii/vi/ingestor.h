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
 * @brief Ingestor interface
 */

#ifndef _EIS_VI_INGESTOR_H
#define _EIS_VI_INGESTOR_H

#include <string>
#include <thread>
#include <atomic>
#include <eis/utils/thread_safe_queue.h>
#include <eis/utils/frame.h>
#include <eis/utils/config.h>

#define TYPE1 "type"
#define PIPELINE "pipeline"
#define POLL_INTERVAL "poll_interval"


using namespace eis::utils;

namespace eis {
    namespace vi {

        /**
         * Ingestion return codes.
         */
        enum IngestRetCode {
            SUCCESS,
            NOT_INITIALIZED,
            ALREADY_INITIALIZED,
            STOPPED,
            ALREAD_RUNNING,
            INVALID_CONFIG,
            MSGBUS_ERR,
            INIT_ERROR,
            UNKNOWN_INGESTOR,
        };

        // typedef struct {
        //     IngestRetCode code;
        //     const char *name;
        // } StatusCodeName;

        // static const size_t StatusCodeDescriptionsSize = 9;
        // static const StatusCodeName StatusCodeDescriptions[StatusCodeDescriptionsSize] = {
        //     {SUCCESS, "SUCCESS"},
        //     {NOT_INITIALIZED, "NOT_INITIALIZED"},
        //     {ALREADY_INITIALIZED, "ALREADY_INITIALIZED"},
        //     {STOPPED, "STOPPED"},
        //     {ALREADY_RUNNING, "ALREADY_RUNNING"},
        //     {INVALID_CONFIG, "INVALID_CONFIG"},
        //     {MSGBUS_ERR, "MSGBUS_ERR"},
        //     {INIT_ERROR, "INIT_ERROR"},
        //     {UNKNOWN_INGESTOR, "UNKNOWN_INGESTOR"}
        // };

        // const char* get_statuscode_name(IngestRetCode code) {
        //     for (size_t i = 0; i < StatusCodeDescriptionsSize; ++i) {
        //         if (StatusCodeDescriptions[i].code == code)
        //             return StatusCodeDescriptions[i].name;
        //     }
        //     return StatusCodeDescriptions[StatusCodeDescriptionsSize-1].name;
        // }

        /**
         * Ingestor type
         */
        enum IngestorType {
            OPENCV,
            GSTREAMER
        };

        /**
         * Thread safe frame queue.
         */
        typedef ThreadSafeQueue<Frame*> FrameQueue;

        /**
         * Base ingestor interface.
         */
        class Ingestor {
            private:
                // Underlying ingestion thread
                std::thread* m_th;

            protected:

                // Flag for if the ingestor has been initialized
                std::atomic<bool> m_initialized;

                // Flag to stop the ingestor from running
                std::atomic<bool> m_stop;

                // UDF input queue
                FrameQueue* m_udf_input_queue;

                // pipeline
                std::string m_pipeline;

                // poll interval
                double m_poll_interval;

                /**
                 * Ingestion thread run method
                 */
                virtual void run();

                /**
                 * Read method implemented by subclasses to retrieve the next frame from
                 * the ingestion stream.
                 */
                virtual void read(Frame*& frame) = 0;

            public:

                /**
                 * Constructor
                 */
                Ingestor(config_t* config, FrameQueue* frame_queue);

                /**
                 * Destructor
                 */
                virtual ~Ingestor();

                /**
                 * Start the ingestor.
                 */
                virtual IngestRetCode start();

                /**
                 * Stop the ingestor.
                 */
                virtual void stop();
        };

        Ingestor* get_ingestor(config_t* ingestor_cfg, FrameQueue* udf_input_queue, const char* type);

    } // vi
} // eis
#endif // _EIS_VI_INGESTOR_H
