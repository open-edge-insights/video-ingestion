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

#ifndef _EII_VI_INGESTOR_H
#define _EII_VI_INGESTOR_H

#include <string>
#include <thread>
#include <atomic>
#include <eii/utils/thread_safe_queue.h>
#include <eii/udf/frame.h>
#include <eii/utils/config.h>
#include <eii/utils/profiling.h>
#include <chrono>

#define TYPE1 "type"
#define PIPELINE "pipeline"
#define POLL_INTERVAL "poll_interval"


using namespace eii::utils;
using namespace eii::udf;

namespace eii {
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
        typedef ThreadSafeQueue<udf::Frame*> FrameQueue;

        /**
         * Base ingestor interface.
         */
        class Ingestor {
            private:
                // Caller's AppName
                std::string m_service_name;

            protected:
                // Underlying ingestion thread
                std::thread* m_th;

                // Flag indicating the ingestor thread (running run()) has started & is running;
                std::atomic<bool> m_running;

                // Flag for if the ingestor has been initialized
                std::atomic<bool> m_initialized;

                // Flag to stop the ingestor from running
                std::atomic<bool> m_stop;

                // UDF input queue
                FrameQueue* m_udf_input_queue;

                // Queue blocked variable
                std::string m_ingestor_block_key;

                // Snapshot condition variable
                std::condition_variable& m_snapshot_cv;

                // Encoding details
                EncodeType m_enc_type;
                int m_enc_lvl;

                // pipeline
                std::string m_pipeline;

                // poll interval
                double m_poll_interval;

                // profiling
                Profiling* m_profile = NULL;

                // Flag for snapshot mode
                bool m_snapshot;

                /**
                 * Ingestion thread run method
                 */
                virtual void run(bool snapshot_mode=false) = 0;

                /**
                 * Read method implemented by subclasses to retrieve the next frame from
                 * the ingestion stream.
                 */
                virtual void read(udf::Frame*& frame) = 0;

                /**
                 * Private @c Ingestor assignment operator.
                 */
                Ingestor& operator=(const Ingestor& src);

            public:
                /**
                 * Constructor
                 * @param config        - Ingestion config
                 * @param frame_queue   - Frame Queue context
                 * @param service_name  - Service Name env variable
                 * @param snapshot_cv   - Snapshot contion variable
                 * @param enc_type      - Frame encoding type(Optional)
                 * @param enc_lvl       - Frame encoding level(Optional)
                 */
                Ingestor(config_t* config, FrameQueue* frame_queue, std::string service_name, std::condition_variable& snapshot_cv, EncodeType enc_type, int enc_lvl);

                /**
                 * Destructor
                 */
                virtual ~Ingestor();

                /**
                 * Start the ingestor.
                 */
                virtual IngestRetCode start(bool snapshot_mode=false);

                /**
                 * Stop the ingestor.
                 */
                virtual void stop() = 0;
        };
        /**
         * Method to get the ingestor object based on the ingestor type
         * @param config            - Ingestion config
         * @param udf_input_queue   - UDF input queue context
         * @param type              - Ingestor type
         * @param service_name      - Ingestor service name
         * @param snapshot_cv       - Snapshot condition variable
         * @param enc_type          - Frame encoding type(Optional)
         * @param enc_lvl           - Frame encoding level(Optional)
         */
        Ingestor* get_ingestor(config_t* ingestor_cfg, FrameQueue* udf_input_queue, const char* type, std::string service_name, std::condition_variable& snapshot_cv, EncodeType enc_type, int enc_lvl);

    } // vi
} // eii
#endif // _EII_VI_INGESTOR_H
