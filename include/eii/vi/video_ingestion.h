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
 * @brief VideoIngestion interface
 */

#ifndef _EIS_VI_VIDEOINGESTION_H
#define _EIS_VI_VIDEOINGESTION_H

#include <thread>
#include <atomic>
#include <condition_variable>
#include <eis/udf/frame.h>
#include <string.h>
#include <eis/utils/config.h>
#include <eis/utils/thread_safe_queue.h>
#include <eis/utils/json_config.h>
#include <eis/config_manager/env_config.h>
#include <eis/config_manager/config_manager.h>
#include <eis/msgbus/msgbus.h>
#include <eis/msgbus/msg_envelope.h>
#include <eis/udf/udf_manager.h>
#include "eis/vi/ingestor.h"

using namespace eis::utils;
using namespace eis::udf;

namespace eis {
    namespace vi {
        enum AckToClient {
            REQ_HONORED,
            REQ_NOT_HONORED,
            REQ_ALREADY_RUNNING,
            REQ_ALREADY_STOPPED
        };

        /**
         * VideoIngestion class
         */
        class VideoIngestion {
            private:

                // App name
                std::string m_app_name;

                // Ingestor type - opencv or gstreamer
                std::string m_ingestor_type;

                // Ingestor object
                Ingestor* m_ingestor;

                // EIS MsgBus Publisher
                msgbus::Publisher* m_publisher;

                // EIS UDFManager
                UdfManager* m_udf_manager;

                // UDF input queue
                FrameQueue* m_udf_input_queue;

                // UDF output queue
                FrameQueue* m_udf_output_queue;

                // Error condition variable
                std::condition_variable& m_err_cv;

                // ingestor config
                config_t* m_ingestor_cfg;

                // Encoding details
                EncodeType m_enc_type;
                int m_enc_lvl;

                // Software trigger enabled flag
                bool m_sw_trgr_en;

                // bool value of the init_state - true - if init_state = running ; false - if init state = stopped
                bool m_init_state_start;

                // variable to store the current Ingestion_state i.e. if Ingestion is going on or stopped
                bool m_ingestion_running;

                //  Thread to monitor the start/stop ingestion requests
                // which run the "control_ingestion()" function
                std::thread *m_th_ingest_control;

                // control_ingestion thread method responsible for controlling the ingestion
                // based on the requests (Start/stop ingestion from clients)
                void control_ingestion();

                // Msgbus server related member variables
                void* m_msgbus_ctx_server;
                recv_ctx_t* m_service_ctx;
                msg_envelope_t* m_msg;
                //config_t* m_service_config;

                // exit condition for sw trigger
                std::atomic<bool> m_exit_sw_trigger_monitor;

                // Receive sw trigger from Client to either Start/stop ingestion
                std::string receive_sw_trigger();

                // Process SW trigger - ret value bool-
                // true - STOP MONITORING not called.
                // false - STOP MONITORING called.
                AckToClient process_sw_trigger(std::string tr_msg);

                // Acknowledge  back to client that SW trigger request has been honored/Not honored
                bool ack_sw_trigger(AckToClient ack);

                /**
                * Initialize the msgbus server for sw trigger feature
                @param env_config         - env config client
                @param 
                */
                void service_init(const env_config_t* env_config, const config_mgr_t* g_config_mgr);

            public:

                /**
                * Constructor
                *
                * \note The environmental configuration memory is not managed
                *      by this object. It is managed by the caller.
                *
                * @param app_name   - App_name env variable for App_Name
                * @param err_cv     - Error condition variable
                * @param env_config - Environmental configuration
                * @param vi_config  - VideoIngestion/config
                */
                VideoIngestion(std::string app_name, std::condition_variable& err_cv, const env_config_t* env_config, char* vi_config, const config_mgr_t* g_config_mgr);

                // Destructor
                ~VideoIngestion();

                /**
                 * Start the VI pipeline in order of MsgBusPublisher, UDFManager and Ingestor
                 */
                void start();

                /**
                 * Stop the VI pipeline in reverse order
                 */
                void stop();

            };
    }
}
#endif
