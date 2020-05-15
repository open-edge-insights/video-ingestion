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
#include <map>
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
            REQ_HONORED=0,
            REQ_NOT_HONORED=1,
            REQ_ALREADY_RUNNING=2,
            REQ_ALREADY_STOPPED=3
        };

        enum CommandsList {
            START_INGESTION,
            STOP_INGESTION
            // MORE COMMANDS TO BE ADDED BASED ON THE NEED
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
                std::atomic<bool> m_ingestion_running;

                //  Thread to monitor the command requests
                // which run the "command_handler()" function
                std::thread *m_th_ingest_control;

                // command_handler thread method responsible for controlling the commands
                // based on the requests from clients
                void command_handler();

                // Msgbus server related member variables
                void* m_msgbus_ctx_server;
                recv_ctx_t* m_service_ctx;
                msg_envelope_t* m_msg;

                // exit condition for sw trigger
                std::atomic<bool> m_exit_sw_trigger_monitor;

                // Map of Fuction pointers for the function specific to the Commanads with its key value
                std::map<int, msg_envelope_elem_body_t *(VideoIngestion::*)(msg_envelope_elem_body_t *)> cmd_handler_map;

                /**
		* Receive command payload from the client
                * @param arg_payload -- Argument Payload object received (in the main payload) from client
                * return value - return values payload JSON buffer to be returned back to the client
                */
                msg_envelope_elem_body_t* receive_command_payload(msg_envelope_t* msg);

                /**
		* Process the start ingestion software trigger and control the ingestor
                * @param arg_payload -- Argument Payload object received (in the main payload) from client
                * return value - return values payload JSON buffer to be returned back to the client
                */
                msg_envelope_elem_body_t* process_start_ingestion(msg_envelope_elem_body_t *arg_payload);

                /**
		* Process the stop ingestion software trigger and control the ingestor
                * @param arg_payload -- Main Payload object received from client
                * return value - return values payload JSON buffer to be returned back to the client
                */
                msg_envelope_elem_body_t* process_stop_ingestion(msg_envelope_elem_body_t *arg_payload);

                /**
                * Process the sw trigger command sent from the client
                * @param arg_payload -- Main Payload object received from client
                * return value - return values payload JSON buffer to be returned back to the client
                */
		msg_envelope_elem_body_t* process_command(msg_envelope_elem_body_t *payload);

                // Acknowledge about the response of teh command back to client
                void ack_to_command(msg_envelope_elem_body_t *response_payload);

                /**
                * Initialize the msgbus server for sw trigger feature
                * @param env_config         - env config client
                * @param h_config_mgr         - config manager object reference
                */
                void service_init(const env_config_t* env_config, const config_mgr_t* config_mgr);

                /**
                * Form the JSON reply buffer to be sent back to the client
                * @param msg_envelope_elem_body_t : Object of the values to be returned back to client
                * @param status_code : (integer) 0 - for success, non-zero for failure
                * @param err_string   : (string) - error message
                */
                msg_envelope_elem_body_t* form_reply_payload(int status_code, std::string err, msg_envelope_elem_body_t* return_values);

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
                VideoIngestion(std::string app_name, std::condition_variable& err_cv, const env_config_t* env_config, char* vi_config, const config_mgr_t* config_mgr);

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
