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
 * @brief CommandHandler Implementation
 */

#include "eii/ch/command_handler.h"
#include <unistd.h>

using namespace eii::ch;
using namespace eii::config_manager;

#define COMMAND "command"
#define REPLY_PAYLOAD "reply_payload"
#define STATUS_CODE "status_code"
#define ERR "err"

#define FREE_MSG_ENVELOPE(msg_env) { \
    if (msg_env != NULL) { \
        msgbus_msg_envelope_destroy(msg_env); \
        msg_env = NULL; \
    } \
}

CommandHandler::CommandHandler(ConfigMgr* ctx) {
    // initialize the exit condition for command handler monitor to false
    m_exit_command_handler_monitor.store(false);
    int server_ret;

    // server gets initialized
    server_ret = service_init(ctx);

    // server_ret = 0 -> Server creation is success else failure
    if (server_ret != 0) {
        throw("Command Handler initialization failed. Check the server configuration.");
    } else {
        // Thread to monitor the command requests which run the "run()" function
        m_ch_thread = new std::thread(&CommandHandler::run, this);
        if (m_ch_thread == NULL) {
            throw("Command Handler thread creation failure");
        }
    }
}

CommandHandler::CommandHandler(const CommandHandler& src) {
    throw "This object should not be copied";
}

CommandHandler& CommandHandler::operator=(const CommandHandler& src) {
    return *this;
}

int CommandHandler::service_init(ConfigMgr* ctx) {
    // Server related env_config msgbus initializations
    ServerCfg* server_ctx = ctx->getServerByIndex(0);
    if (server_ctx == NULL) {
        const char* err = "Failed to initialize server_ctx";
        LOG_ERROR("%s", err);
        return -1;
    }
    config_t* service_config = server_ctx->getMsgBusConfig();
    if (service_config == NULL) {
        const char* err = "Failed to fetch msgbus config for Server";
        LOG_ERROR("%s", err);
        return -1;
    }

    char* name = NULL;
    config_value_t* interface_value = server_ctx->getInterfaceValue("Name");
    if (interface_value == NULL || interface_value->type != CVT_STRING){
        LOG_ERROR_0("Failed to get interface value");
        return -1;
    }
    name = interface_value->body.string;

    m_ch_ctx_server = msgbus_initialize(service_config);
    if (m_ch_ctx_server == NULL) {
        const char* err = "Failed to initialize message bus for server configuration";
        LOG_ERROR("%s", err);
        config_destroy(service_config);
        return -1;
    }

    msgbus_ret_t ret;
    ret = msgbus_service_new(m_ch_ctx_server, name, NULL, &m_ch_service_ctx);
    if (ret != MSG_SUCCESS) {
        const char* err = "Failed to initialize service for server config";
        LOG_ERROR("%s", err);
        if (m_ch_service_ctx != NULL) {
            msgbus_recv_ctx_destroy(m_ch_ctx_server, m_ch_service_ctx);
        }
        config_destroy(service_config);
        return -1;
    }

    // clean up service config
    config_destroy(service_config);

    return 0;
}


void CommandHandler::register_callback (int key, std::function<msg_envelope_elem_body_t* (msg_envelope_elem_body_t *)> cb_func) {
    // Add the callback functions to the map with the key value
    auto result = m_cmd_handler_map.insert({key, cb_func});
    if (!result.second) {
        LOG_ERROR_0("Command registration failed");
    }
}

msg_envelope_elem_body_t* CommandHandler::form_reply_payload(int status_code, std::string err, msg_envelope_elem_body_t *return_values) {
    msg_envelope_elem_body_t* reply_payload_obj = msgbus_msg_envelope_new_object();
    if (reply_payload_obj == NULL) {
        const char* er = "Error creating the message envelope object";
        LOG_ERROR("%s", er);
        throw(er);
    }
    // wrap status code in envelope format
    msg_envelope_elem_body_t* env_status_code = msgbus_msg_envelope_new_integer(status_code);
    if (env_status_code == NULL) {
        const char* er = "Error creating message envelope integer";
        LOG_ERROR("%s", er);
        throw(er);
    }

    msgbus_ret_t ret = msgbus_msg_envelope_elem_object_put(reply_payload_obj, STATUS_CODE, env_status_code);
    if (ret != MSG_SUCCESS) {
        const char* er = "Error in puting status code into json buffer";
        LOG_ERROR("%s", er);
        throw(er);
    }

    // wrap error value in envelope format
    if (err.empty()) {
        msg_envelope_elem_body_t* env_err = msgbus_msg_envelope_new_string(err.c_str());
        ret = msgbus_msg_envelope_elem_object_put(reply_payload_obj, ERR, env_err);
        if (ret != MSG_SUCCESS) {
            const char* er = "Error in puting reply_payload_object into json buffer";
            LOG_ERROR("%s", er);
            throw(er);
        }
    }

    // wrap the bool in envelope format - if any more return values need to be sent bck to client specific to the command
    if (return_values != NULL) {
        if (ret != MSG_SUCCESS) {
            const char* er = "Error in puting reply_payload_object into json buffer";
            LOG_ERROR("%s", er);
            throw(er);
        }

        // NOTE: The set of return values will be specific to the command & will be sent by the command specific handler
        ret = msgbus_msg_envelope_elem_object_put(reply_payload_obj, "return_values", return_values);
        if (ret != MSG_SUCCESS) {
            const char* er = "Error in puting return_values into json buffer";
            LOG_ERROR("%s", er);
            throw(er);
        }
    }
    return reply_payload_obj;
}

msg_envelope_elem_body_t* CommandHandler::process_command(msg_envelope_elem_body_t *payload_body) {
    try {
        // store the command name in a string
        std::string command_name_str = std::string(payload_body->body.string);
        LOG_INFO("Received command request on the VI server side is %s", command_name_str.c_str());

        msg_envelope_elem_body_t* args_obj = NULL;
        args_obj = msgbus_msg_envelope_elem_object_get(payload_body, "arguments");
        if (args_obj == NULL) {
            const char* err = "JSON payload doesn't contain arguments";
            LOG_INFO("%s", err);
        }

        CommandsList cmnd;
        cmnd = COMMAND_INVALID;
        if (!command_name_str.compare("START_INGESTION")) {
            cmnd = START_INGESTION;
        } else if (!command_name_str.compare("STOP_INGESTION")) {
            cmnd = STOP_INGESTION;
        } else if (!command_name_str.compare("SNAPSHOT")) {
            cmnd = SNAPSHOT;
        }

        msg_envelope_elem_body_t *final_reply_payload;

        // Find if the Command is registerd or not using map iterator.
        auto command_iterator = m_cmd_handler_map.find((int)cmnd);
        if (command_iterator == m_cmd_handler_map.end()) {
            const char*  err = "Command is not registered";
            LOG_ERROR("%s", err);
            return form_reply_payload((int)REQ_COMMAND_NOT_REGISTERED, err, NULL);
        }

        final_reply_payload = command_iterator->second(args_obj);
        return final_reply_payload;
    } catch(std::exception& ex) {
        const char*  err = "exception occurred request not honored";
        LOG_ERROR("%s %s", ex.what(), err);
        return form_reply_payload((int)REQ_NOT_HONORED, err, NULL);
    }
}

msg_envelope_elem_body_t* CommandHandler::receive_command_payload(msg_envelope_t* msg) {
    msgbus_ret_t ret;
    ret = msgbus_recv_wait(m_ch_ctx_server, m_ch_service_ctx, &msg);
    if (ret != MSG_SUCCESS) {
        const char* err = "";
        // Interrupt is an acceptable error
        if (ret == MSG_ERR_EINTR) {
            err = "MSG_ERR_EINTR received, hence failed to receive command payload";
        }
        err = "Failed to receive command payload";
        throw err;
    }

    msg_envelope_elem_body_t* payload_body;
    ret = msgbus_msg_envelope_get(msg, COMMAND, &payload_body);
    if (ret != MSG_SUCCESS) {
        const char* err = "Failed to receive payload";
        LOG_ERROR("%s", err);
        throw(err);
    }
    return payload_body;
}

void CommandHandler::ack_to_command(msg_envelope_elem_body_t *response_payload) {
    msg_envelope_t* msg = msgbus_msg_envelope_new(CT_JSON);
    msgbus_ret_t ret = msgbus_msg_envelope_put(msg, REPLY_PAYLOAD, response_payload);
    if (ret != MSG_SUCCESS) {
        const char* err = "Failed to put the message into message envelope";
        LOG_ERROR("%s", err);
        FREE_MSG_ENVELOPE(msg);
        throw err;
    }

    ret = msgbus_response(m_ch_ctx_server, m_ch_service_ctx, msg);
    if (ret != MSG_SUCCESS) {
        const char* err = "Failed to send ACK from server back to client";
        LOG_ERROR("%s", err);
        FREE_MSG_ENVELOPE(msg);
        throw err;
    }
    // clean up:
    FREE_MSG_ENVELOPE(msg);
}

void CommandHandler::run() {
    try {
        do {
            msg_envelope_t* outer_msg_env = NULL;

            // Step1: Receive the Command payload from client
            msg_envelope_elem_body_t *arg_payload = receive_command_payload(outer_msg_env);  // specific to msgbus api calls

            // Step2: Process the command  & do command specific action
            msg_envelope_elem_body_t *reply_payload = process_command(arg_payload);

            // Step3: Send the ACK back to client with return status & values
            ack_to_command(reply_payload);

            // Free the msgEnvelope
            FREE_MSG_ENVELOPE(outer_msg_env);
        } while (!(m_exit_command_handler_monitor.load()));
    } catch (std::exception &ex) {
        LOG_ERROR("Exception :: %s in the command_handler thread.", ex.what());
        msg_envelope_elem_body_t *rep = form_reply_payload(REQ_NOT_HONORED, NULL, NULL);
        ack_to_command(rep);
    }
}

CommandHandler::~CommandHandler() {
    // server related clean for client server model
    if (m_ch_service_ctx != NULL)
        msgbus_recv_ctx_destroy(m_ch_ctx_server, m_ch_service_ctx);
    if (m_ch_ctx_server != NULL)
        msgbus_destroy(m_ch_ctx_server);
    if (m_ch_thread != NULL)
        delete m_ch_thread;

    m_exit_command_handler_monitor.store(true);
}

