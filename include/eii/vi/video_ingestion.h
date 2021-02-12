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

#ifndef _EII_VI_VIDEOINGESTION_H
#define _EII_VI_VIDEOINGESTION_H

#include <thread>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <eii/udf/frame.h>
#include <string.h>
#include <eii/utils/config.h>
#include <eii/utils/thread_safe_queue.h>
#include <eii/utils/json_config.h>
#include <eii/msgbus/msgbus.h>
#include <eii/msgbus/msg_envelope.h>
#include <eii/udf/udf_manager.h>
#include "eii/vi/ingestor.h"
#include "eii/config_manager/config_mgr.hpp"
#include "eii/ch/command_handler.h"

using namespace eii::utils;
using namespace eii::udf;
using namespace eii::ch;
using namespace eii::config_manager;

namespace eii {
    namespace vi {

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

                // CommandHandler object
                CommandHandler* m_commandhandler;

                // EII MsgBus Publisher
                msgbus::Publisher* m_publisher;

                // EII UDFManager
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

                // Snapshot condition variable
                std::condition_variable m_snapshot_cv;

                /**
	         * Process the start ingestion software trigger and control the ingestor
                 * @param arg_payload -- Argument Payload object received (in the main payload) from client
                 * @return reply_payload - return values payload JSON buffer to be returned back to the client
                 */
                msg_envelope_elem_body_t* process_start_ingestion(msg_envelope_elem_body_t *arg_payload);

                /**
		 * Process the stop ingestion software trigger and control the ingestor
                 * @param arg_payload -- Main Payload object received from client
                 * @return reply_payload - return values payload JSON buffer to be returned back to the client
                 */
                msg_envelope_elem_body_t* process_stop_ingestion(msg_envelope_elem_body_t *arg_payload);

                /**
                 * Process the start snapshot software trigger and control the ingestor
                 * @param arg_payload -- Argument Payload object received (in the main payload) from client
                 * @return reply_payload - return values payload JSON buffer to be returned back to the client
                 */
                msg_envelope_elem_body_t* process_snapshot(msg_envelope_elem_body_t *arg_payload);

            public:

                /**
                 * Constructor
                 *
                 * \note The environmental configuration memory is not managed
                 *      by this object. It is managed by the caller.
                 *
                 * @param app_name          - App_name env variable for App_Name
                 * @param err_cv            - Error condition variable
                 * @param vi_config         - VideoIngestion/config
                 * @param ctx               - ConfigManager object
                 * @param commandhandler    - Command Handler context
                 */
                VideoIngestion(std::string app_name, std::condition_variable& err_cv, char* vi_config, ConfigMgr* ctx, CommandHandler* commandhandler);

                /*
                 * Destructor
                 */
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
