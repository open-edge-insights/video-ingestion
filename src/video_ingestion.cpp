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
 * @brief VideoIngestion Implementation
 */

#include <iostream>
#include <safe_lib.h>
#include "eis/vi/video_ingestion.h"
#include "eis/vi/ingestor.h"
#include "eis/vi/gstreamer_ingestor.h"

#define INTEL_VENDOR "GenuineIntel"
#define INTEL_VENDOR_LENGTH 12
#define DEFAULT_QUEUE_SIZE 10
#define PUB "pub"
#define SW_TRIGGER "sw_trigger"

using namespace eis::vi;
using namespace eis::utils;
using namespace eis::msgbus;
using namespace eis::udf;

VideoIngestion::VideoIngestion(
        std::string app_name, std::condition_variable& err_cv, const env_config_t* env_config, char* vi_config, const config_mgr_t* g_config_mgr) :
    m_err_cv(err_cv), m_enc_type(EncodeType::NONE), m_enc_lvl(0)
{
    m_app_name = app_name;

    // Parse the configuration
    config_t* config = json_config_new_from_buffer(vi_config);
    if(config == NULL) {
        const char* err = "Failed to initialize configuration object";
        LOG_ERROR("%s", err);
        throw(err);
    }
    config_value_t* encoding_value = config->get_config_value(config->cfg,
                                                              "encoding");
    if (encoding_value == NULL) {
        const char* err = "\"encoding\" key is missing";
        LOG_WARN("%s", err);
    } else {
        config_value_t* encoding_type_cvt = config_value_object_get(encoding_value,
                                                                    "type");
        if( encoding_type_cvt == NULL ) {
            const char* err = "encoding \"type\" key missing";
            LOG_ERROR("%s", err);
            config_destroy(config);
            throw(err);
        }
        if(encoding_type_cvt->type != CVT_STRING) {
            const char* err = "encoding \"type\" value has to be of string type";
            LOG_ERROR("%s", err);
            config_destroy(config);
            config_value_destroy(encoding_type_cvt);
            throw(err);
        }
        char* enc_type = encoding_type_cvt->body.string;
        if(strcmp(enc_type, "jpeg") == 0) {
            m_enc_type = EncodeType::JPEG;
            LOG_DEBUG_0("Encoding type is jpeg");
        } else if(strcmp(enc_type, "png") == 0) {
            m_enc_type = EncodeType::PNG;
            LOG_DEBUG_0("Encoding type is png");
        }

        config_value_t* encoding_level_cvt = config_value_object_get(encoding_value,
                                                                    "level");
        if(encoding_level_cvt == NULL) {
            const char* err = "encoding \"level\" key missing";
            LOG_ERROR("%s", err);
            config_destroy(config);
            throw(err);
        }
        if(encoding_level_cvt->type != CVT_INTEGER) {
            const char* err = "encoding \"level\" value has to be of string type";
            LOG_ERROR("%s", err);
            config_destroy(config);
            config_value_destroy(encoding_level_cvt);
            throw(err);
        }
        m_enc_lvl = encoding_level_cvt->body.integer;
        LOG_DEBUG("Encoding value is %d", m_enc_lvl);
    }

    config_value_t* ingestor_value = config->get_config_value(config->cfg,
                                                              "ingestor");
    if(ingestor_value == NULL) {
        const char* err = "\"ingestor\" key is missing";
        LOG_ERROR("%s", err);
        throw(err);
    }
    config_value_t* ingestor_type_cvt = config_value_object_get(ingestor_value,
                                                                "type");
    if(ingestor_type_cvt == NULL) {
        const char* err = "\"type\" key missing";
        LOG_ERROR("%s", err);
        config_destroy(config);
        throw(err);
    }
    if(ingestor_type_cvt->type != CVT_STRING) {
        const char* err = "\"type\" value has to be of string type";
        LOG_ERROR("%s", err);
        config_destroy(config);
        config_value_destroy(ingestor_type_cvt);
        throw(err);
    }

    m_ingestor_type = std::string(ingestor_type_cvt->body.string);

    config_value_t* ingestor_queue_cvt = config_value_object_get(ingestor_value,
                                                                    "queue_size");
    size_t queue_size = DEFAULT_QUEUE_SIZE;
    if(ingestor_queue_cvt == NULL) {
        LOG_INFO("\"queue_size\" key missing, so using default queue size: \
                    %ld", queue_size);
    } else {
        if(ingestor_queue_cvt->type != CVT_INTEGER) {
            const char* err = "\"queue_size\" value has to be of integer type";
            LOG_ERROR("%s", err);
            config_destroy(config);
            config_value_destroy(ingestor_queue_cvt);
            throw(err);
        }
        queue_size = ingestor_queue_cvt->body.integer;
    }

    m_udf_input_queue = new FrameQueue(queue_size);

    config_value_object_t* ingestor_cvt = ingestor_value->body.object;
    m_ingestor_cfg = config_new(ingestor_cvt->object, free, get_config_value);
    if (m_ingestor_cfg == NULL) {
        const char* err = "Unable to get ingestor config";
        LOG_ERROR("%s", err);
        config_destroy(config);
        throw(err);
    }

    // get config SW_Trigger logic start
    config_value_t* sw_trigger = config->get_config_value(config->cfg,
                                                            SW_TRIGGER);
    if (sw_trigger == NULL) {
        LOG_INFO("Software Trigger feature is disabled");
        m_sw_trgr_en = false;
    } else {
        LOG_INFO("Software Trigger feature is enabled");
        m_sw_trgr_en = true;

        // Read config from config_mgr
        config_value_t* sw_trigger_init_state_cvt = config_value_object_get(sw_trigger,
                                                                "init_state");
        if (sw_trigger_init_state_cvt == NULL) {
            const char* err = "\"init_state\" key missing";
            LOG_ERROR("%s", err);
            config_destroy(config);
            throw(err);
        }

        // SW trigger init state : Possible values are "running"/"stopped"
        std::string initial_state = sw_trigger_init_state_cvt->body.string;

        // bool value of the init_state read from config
        m_init_state_start = (!initial_state.compare("running")) ? true : false;

        // Ingestion is not going ON by default when VideoIngestion constructor is called. Even if init_state=start, the ingestion
        // will start when start() is called
        m_ingestion_running = false;

        // Initialize the msgbus server for sw trigger feature
        service_init(env_config, g_config_mgr);

        // initialize the exit conditiopn for ingestion monitor to false
        m_exit_sw_trigger_monitor.store(false);
    }


    // sw-trigger get config end

    config_value_t* udf_value = config->get_config_value(config->cfg,
                                                            "udfs");
    if(udf_value == NULL) {
        LOG_INFO("\"udfs\" key doesn't exist, so udf output queue is same as \
                udf input queue!!")
        m_udf_output_queue = m_udf_input_queue;
    } else {
        m_udf_output_queue = new FrameQueue(queue_size);
        m_udf_manager = new UdfManager(config, m_udf_input_queue, m_udf_output_queue,
                                        m_enc_type, m_enc_lvl);
    }

    // Get ingestor
    m_ingestor = get_ingestor(m_ingestor_cfg, m_udf_input_queue, m_ingestor_type.c_str(), m_enc_type, m_enc_lvl);

    char** pub_topics = env_config->get_topics_from_env(PUB);
    size_t num_of_pub_topics = env_config->get_topics_count(pub_topics);

    if(num_of_pub_topics != 1){
        const char* err = "Only one topic is supported. Neither more, nor less";
        LOG_ERROR("%s", err);
        config_destroy(config);
        throw(err);
    }

    LOG_DEBUG_0("Successfully read PubTopics env value...");

    config_t* pub_config = env_config->get_messagebus_config(g_config_mgr, pub_topics, num_of_pub_topics, PUB);
    if(pub_config == NULL) {
        const char* err = "Failed to get publisher message bus config";
        LOG_ERROR("%s", err);
        config_destroy(config);
        throw(err);
    }
    LOG_DEBUG_0("Publisher Config received...");


    m_publisher = new Publisher(
            pub_config, m_err_cv, pub_topics[0], (MessageQueue*) m_udf_output_queue);
    free(pub_topics);

    config_destroy(config);
    config_value_destroy(ingestor_type_cvt);
    config_value_destroy(ingestor_queue_cvt);
    config_value_destroy(encoding_value);
    config_value_destroy(udf_value);
}

void VideoIngestion::service_init(const env_config_t* env_config, const config_mgr_t* g_config_mgr) {
    // Server related env_config msgbus initializations
    char* c_app_name = const_cast<char*>(m_app_name.c_str());
    char* app_name_arr[] = {c_app_name};
    config_t* service_config = env_config->get_messagebus_config(g_config_mgr, app_name_arr, 1, "server");
    if (service_config == NULL) {
        const char* err = "Failed to get server message bus config";
        LOG_ERROR("%s", err);
        throw(err);
    }

    m_msgbus_ctx_server = msgbus_initialize(service_config);
    if (m_msgbus_ctx_server == NULL) {
        const char* err = "Failed to initialize message bus for server configuration";
        LOG_ERROR("%s", err);
        config_destroy(service_config);
        throw(err);
    }

    msgbus_ret_t ret;
    ret = msgbus_service_new(m_msgbus_ctx_server, m_app_name.c_str(), NULL, &m_service_ctx);
    if (ret != MSG_SUCCESS) {
        const char* err = "Failed to initialize service for server config";
        LOG_ERROR("%s", err);
        if (m_service_ctx != NULL) {
            msgbus_recv_ctx_destroy(m_msgbus_ctx_server, m_service_ctx);
        }
        config_destroy(service_config);
        throw(err);
    }
    // clean up service config
    config_destroy(service_config);
}

std::string VideoIngestion::receive_sw_trigger() {
    msgbus_ret_t ret;
    msg_envelope_t* msg = NULL;
    ret = msgbus_recv_wait(m_msgbus_ctx_server, m_service_ctx, &msg);
    if (ret != MSG_SUCCESS) {
        const char* err = "";
        // Interrupt is an acceptable error
        if (ret == MSG_ERR_EINTR) {
            err = "MSG_ERR_EINTR received, hence failed to receive software trigger ";
        }
        err = "failed to receive software trigger ";
        throw err;
    }

    msg_envelope_elem_body_t* sw_trig_env;
    ret = msgbus_msg_envelope_get(msg, SW_TRIGGER, &sw_trig_env);
    if (ret != MSG_SUCCESS) {
        const char* err = "Failed to receive sw_trigger";
        LOG_ERROR("%s", err);
        throw(err);
    }
    if (MSG_ENV_DT_STRING != sw_trig_env->type) {
        const char* err = "sw_trigger received has a wrong data type";
        LOG_ERROR("%s", err);
        throw(err);
    }
    std::string received_msg = std::string(sw_trig_env->body.string);

    LOG_INFO("Received software trigger message on server side is %s", received_msg.c_str());
    // clean up:
    if(msg != NULL) {
        msgbus_msg_envelope_destroy(msg);
        msg = NULL;
    }
    return received_msg;
}


AckToClient VideoIngestion::process_sw_trigger(std::string tr_msg) {
    try {
        // Map the received msg into the right enum
        if (!tr_msg.compare("START_INGESTION")) {
            LOG_INFO_0("START INGESTION request received from client");

            // If Ingestion thread is already running (i.e. if ingestion is going on) then don't propogate
            // another duplicate sw_trigger to do START_INGESTION if the client app by mistake sends another software trigger
            if (m_ingestion_running) {
                // Request is not honored because, ingestion is running currently & still a duplicate sw trigger for START_INGESTION is sent by client again
                return REQ_ALREADY_RUNNING;
            }

            IngestRetCode ret = m_ingestor->start();
            if (ret != IngestRetCode::SUCCESS) {
                LOG_ERROR("Failed to start ingestor thread: %d",ret);
                return REQ_NOT_HONORED;
            } else {
                LOG_INFO("Ingestor thread started...");
                m_ingestion_running = true;
                // acknowledging back to client that ingestion has actually started
                return REQ_HONORED;
            }

        } else if (!tr_msg.compare("STOP_INGESTION")) {
            LOG_INFO_0("STOP INGESTION request received from client");

            // If Ingestion thread is already stopped (i.e. if ingestion is NOT going on) then don't propogate
            // another duplicate sw_trigger to do STOP_INGESTION if the client app by mistake sends another software trigger
            if (!m_ingestion_running) {
            // Request is not honored because, ingestion not running currently & still a duplicate sw trigger for STOP_INGESTION is sent by client again
                return REQ_ALREADY_STOPPED;
            }
            // stop the ingestor
            if (m_ingestor) {
                m_ingestor->stop();
            }

            m_ingestion_running = false;
            return REQ_HONORED;
        } else {
            LOG_ERROR("INVALID MESSAGE RECEIVED: %s",tr_msg);
            return REQ_NOT_HONORED;
        }
    } catch(std::exception& ex) {
        LOG_INFO("exception %s occurred during processIng SW trigger, so requested won't be honored", ex.what());
        return REQ_NOT_HONORED;
    }
}

bool VideoIngestion::ack_sw_trigger(AckToClient ack) {
    std::string rep_msg = "";
    if (ack == REQ_HONORED) {
        rep_msg = "REQUEST_HONORED";
    } else if (ack == REQ_ALREADY_RUNNING) {
        rep_msg = "REQUEST_ALREADY_RUNNING";
    } else if (ack == REQ_ALREADY_STOPPED) {
        rep_msg = "REQUEST_ALREADY_STOPPED";
    } else {
        rep_msg = "REQUEST_NOT_HONORED";
    }
    int ret = 0;
    msg_envelope_elem_body_t* reply = msgbus_msg_envelope_new_string(rep_msg.c_str());
    msg_envelope_t* msg = msgbus_msg_envelope_new(CT_JSON);
    ret = msgbus_msg_envelope_put(msg, "ack_for_sw_trigger", reply);
    if (ret != MSG_SUCCESS) {
        const char* err = "Failed to put the message into message envelope";
        LOG_ERROR("%s", err)
        throw err;
    }
    ret = msgbus_response(m_msgbus_ctx_server, m_service_ctx, msg);
    if (ret != MSG_SUCCESS) {
        const char* err = "Failed to send ACK for Software trigger  from server back to client";
        LOG_ERROR("%s", err)
        throw err;
    }
    // clean up:
    if (msg != NULL) {
        msgbus_msg_envelope_destroy(msg);
        msg = NULL;
    }
    return true;
}

void VideoIngestion::control_ingestion() {
    try {
        do {
            // STep1: Receive the software trigger from client
            std::string recv_msg = receive_sw_trigger();  // specific to msgbus api calls

            // Step2: Process the SW trigger i.e start/stop ingestion & store result if client ask was honored or not
            AckToClient trg_result = process_sw_trigger(recv_msg);

            // Step3: Send the ACK back to client telling req-honored or not
            ack_sw_trigger(trg_result);
        } while (!(m_exit_sw_trigger_monitor.load()));
    } catch (std::exception &ex) {
        LOG_INFO("Exception :: %s in the control_ingestion thread.", ex.what());
        ack_sw_trigger(REQ_NOT_HONORED);
    }
}

void VideoIngestion::start() {
    if (m_publisher) {
        m_publisher->start();
        LOG_INFO("Publisher thread started...");
    }
    if (m_udf_manager) {
        m_udf_manager->start();
        LOG_INFO("Started udf manager");
    }

    if (m_sw_trgr_en) {
        // if SW trigger is enabled then start this "control_ingestion thread which will keep monitoring for client sw trigger requests"
        m_th_ingest_control = new std::thread(&VideoIngestion::control_ingestion, this);
    }

    // if SW trigger is disabled OR (if sw trigger is enabled && init_state = start) then start ingestion
    if (!m_sw_trgr_en || (m_sw_trgr_en && m_init_state_start)) {
        IngestRetCode ret = m_ingestor->start();
        if (ret != IngestRetCode::SUCCESS) {
            LOG_ERROR_0("Failed to start ingestor thread");
        } else {
            LOG_INFO("Ingestor thread started...");
            m_ingestion_running = (m_sw_trgr_en) ? true : false;
        }
    }
    m_th_ingest_control->join();
}

void VideoIngestion::stop() {
    if (m_ingestor) {
        m_ingestor->stop();
    }
    if (m_udf_manager) {
        m_udf_manager->stop();
    }
    if (m_publisher) {
        m_publisher->stop();
    }
}

VideoIngestion::~VideoIngestion() {
    if (m_sw_trgr_en) {
        m_exit_sw_trigger_monitor.store(true);
    }
    // Stop the thread (if it is running)
    if (m_ingestor) {
        m_ingestor->stop();
    }
    if (m_ingestor) {
        delete m_ingestor;
    }
    if (m_udf_manager) {
        delete m_udf_manager;
    }
    if (m_publisher) {
        delete m_publisher;
    }
    // server related clean for client server model
    if (m_service_ctx != NULL)
        msgbus_recv_ctx_destroy(m_msgbus_ctx_server, m_service_ctx);
    if (m_msgbus_ctx_server != NULL)
        msgbus_destroy(m_msgbus_ctx_server);
}
