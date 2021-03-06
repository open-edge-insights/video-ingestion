// Copyright (c) 2020 Intel Corporation.
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
 * @brief CommandHandler interface
 */

#ifndef _EII_CH_COMMANDHANDLER_H
#define _EII_CH_COMMANDHANDLER_H

#include <map>
#include <condition_variable>
#include <functional>
#include <thread>
#include <atomic>
#include <eii/msgbus/msg_envelope.h>
#include <eii/msgbus/msgbus.h>
#include "eii/config_manager/config_mgr.hpp"
#include "commands.h"


namespace eii {
    namespace ch {

        class CommandHandler {
            private:

                // Map of Fuction pointers for the function specific to the Commanads with its key value
                std::map<int, std::function<msg_envelope_elem_body_t* (msg_envelope_elem_body_t*)> > m_cmd_handler_map;

                // exit condition for command handler
                std::atomic<bool> m_exit_command_handler_monitor;

                // Msgbus server related member variables
                void* m_ch_ctx_server;
                recv_ctx_t* m_ch_service_ctx;

                // Thread to monitor the command requests which run the "run()" function
                std::thread* m_ch_thread;

                /**
                 * Start the command handler process of receiving, processing and acknowledging the command
                 */
                void run();

                /**
                 * Receive command payload from the client
                 * @param arg_payload -- Argument Payload object received (in the main payload) from client
                 * return value - return values payload JSON buffer to be returned back to the client
                 */
                msg_envelope_elem_body_t* receive_command_payload(msg_envelope_t* msg);

                /**
                 * Process the sw trigger command sent from the client
                 * @param arg_payload -- Main Payload object received from client
                 * return value - return values payload JSON buffer to be returned back to the client
                 */
                msg_envelope_elem_body_t* process_command(msg_envelope_elem_body_t *payload_body);

                /**
                 * Acknowledge about the response of teh command back to client
                 * @param response_payload -- Payload that will be sent back to client
                 */
                void ack_to_command(msg_envelope_elem_body_t *response_payload);

                /**
                 * Initialize the msgbus server for sw trigger feature
                 * @param ctx - ConfigManager object
                 * return value - returns the error codes.
                 */
                int service_init(eii::config_manager::ConfigMgr* ctx);

                /**
                 * Private @c CommandHandler copy constructor.
                 */
                CommandHandler(const CommandHandler& src);

                /**
                 * Private @c CommandHandler assignment operator.
                 */
                CommandHandler& operator=(const CommandHandler& src);

            public:
            
                /**
                 * Constructor
                 * @param ctx - ConfigManager object
                 */
                CommandHandler(eii::config_manager::ConfigMgr* ctx);
                
                /**
                 * Add the callback functions to the map with the key value
                 * @param int : Key on which callback functions are registered
                 * @param std::function<msg_envelope_elem_body_t* (msg_envelope_elem_body_t*)> : Function pointer for the callback function
                 */
                void register_callback(int key, std::function<msg_envelope_elem_body_t* (msg_envelope_elem_body_t*)> cb_func);

                /**
                 * Form the JSON reply buffer to be sent back to the client
                 * @param msg_envelope_elem_body_t : Object of the values to be returned back to client
                 * @param status_code : (integer) 0 - for success, non-zero for failure
                 * @param err_string   : (string) - error message
                 */
                msg_envelope_elem_body_t* form_reply_payload(int status_code, std::string err, msg_envelope_elem_body_t *return_values);

                /**
                 *  Destructor
                 */ 
                ~CommandHandler();

        }; // CommandHandler class ending
    } // ch namespace ending
} // eii namespace ending
#endif