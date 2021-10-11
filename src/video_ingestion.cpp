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
#include "eii/vi/video_ingestion.h"
#include "eii/vi/ingestor.h"
#include "eii/vi/gstreamer_ingestor.h"
#include <mutex>

#define INTEL_VENDOR "GenuineIntel"
#define INTEL_VENDOR_LENGTH 12
#define DEFAULT_QUEUE_SIZE 10
#define PUB "pub"
#define SW_TRIGGER "sw_trigger"
#define ARGUMENTS "arguments"

using namespace eii::vi;
using namespace eii::utils;
using namespace eii::msgbus;
using namespace eii::udf;

VideoIngestion::VideoIngestion(
        std::string app_name, std::condition_variable& err_cv, char* vi_config, ConfigMgr* ctx, CommandHandler* commandhandler) :
    m_app_name(app_name), m_commandhandler(commandhandler), m_err_cv(err_cv), m_enc_type(EncodeType::NONE), m_enc_lvl(0) {

    // Parse the configuration
    config_t* config = json_config_new_from_buffer(vi_config);
    if (config == NULL) {
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
        if ( encoding_type_cvt == NULL ) {
            const char* err = "encoding \"type\" key missing";
            LOG_ERROR("%s", err);
            config_destroy(config);
            throw(err);
        }
        if (encoding_type_cvt->type != CVT_STRING) {
            const char* err = "encoding \"type\" value has to be of string type";
            LOG_ERROR("%s", err);
            config_destroy(config);
            config_value_destroy(encoding_type_cvt);
            throw(err);
        }
        char* enc_type = encoding_type_cvt->body.string;
        if (strcmp(enc_type, "jpeg") == 0) {
            m_enc_type = EncodeType::JPEG;
            LOG_DEBUG_0("Encoding type is jpeg");
        } else if (strcmp(enc_type, "png") == 0) {
            m_enc_type = EncodeType::PNG;
            LOG_DEBUG_0("Encoding type is png");
        } else {
            throw "Encoding type is not supported";
        }

        config_value_t* encoding_level_cvt = config_value_object_get(encoding_value,
                                                                    "level");
        if (encoding_level_cvt == NULL) {
            const char* err = "encoding \"level\" key missing";
            LOG_ERROR("%s", err);
            config_destroy(config);
            throw(err);
        }
        if (encoding_level_cvt->type != CVT_INTEGER) {
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
    if (ingestor_value == NULL) {
        const char* err = "\"ingestor\" key is missing";
        LOG_ERROR("%s", err);
        throw(err);
    }
    config_value_t* ingestor_type_cvt = config_value_object_get(ingestor_value,
                                                                "type");
    if (ingestor_type_cvt == NULL) {
        const char* err = "\"type\" key missing";
        LOG_ERROR("%s", err);
        config_destroy(config);
        throw(err);
    }
    if (ingestor_type_cvt->type != CVT_STRING) {
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
    if (ingestor_queue_cvt == NULL) {
        LOG_INFO("\"queue_size\" key missing, so using default queue size: \
                    %ld", queue_size);
    } else {
        if (ingestor_queue_cvt->type != CVT_INTEGER) {
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
    m_ingestor_cfg = config_new(ingestor_cvt->object, free, get_config_value, NULL);
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
        m_init_state_start = true;
        m_sw_trgr_en = false;
    } else {
        LOG_INFO("Software Trigger feature is enabled");
        m_sw_trgr_en = true;

        if (m_commandhandler != NULL) {
            m_commandhandler->register_callback((int)START_INGESTION, std::bind(&VideoIngestion::process_start_ingestion, this, std::placeholders::_1));
            m_commandhandler->register_callback((int)STOP_INGESTION, std::bind(&VideoIngestion::process_stop_ingestion, this, std::placeholders::_1));
            m_commandhandler->register_callback((int)SNAPSHOT, std::bind(&VideoIngestion::process_snapshot, this, std::placeholders::_1));
        }

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
        if (initial_state == "running") {
            m_init_state_start = true;
        } else if (initial_state == "stopped") {
            m_init_state_start = false;
        } else {
            const char* err = "init_state value not supported";
            LOG_ERROR("\"%s\" %s", initial_state.c_str(), err);
            throw(err);
        }

        // Ingestion is not going ON by default when VideoIngestion constructor is called. Even if init_state=start, the ingestion
        // will start when start() is called
        m_ingestion_running.store(false);
    }

    config_value_t* udf_value = config->get_config_value(config->cfg,
                                                            "udfs");
    if (udf_value == NULL) {
        LOG_INFO("\"udfs\" key doesn't exist, so udf output queue is same as \
                udf input queue!!")
        m_udf_output_queue = m_udf_input_queue;
        m_udf_manager = NULL;
    } else {
        m_udf_output_queue = new FrameQueue(queue_size);
        m_udf_manager = new UdfManager(config, m_udf_input_queue, m_udf_output_queue, m_app_name,
                                        m_enc_type, m_enc_lvl);
    }

    // Get ingestor
    m_ingestor = get_ingestor( m_ingestor_cfg, m_udf_input_queue, m_ingestor_type.c_str(), m_app_name, m_snapshot_cv, m_enc_type, m_enc_lvl);

    PublisherCfg* pub_ctx = ctx->getPublisherByIndex(0);
    if (pub_ctx == NULL) {
        const char* err = "pub_ctx initialization failed";
        LOG_ERROR("%s", err);
        throw(err);
    }
    config_t* pub_config = pub_ctx->getMsgBusConfig();
    if (pub_config == NULL) {
        const char* err = "Failed to fetch msgbus config for Publisher";
        LOG_ERROR("%s", err);
        throw(err);
    }
    std::vector<std::string> topics = pub_ctx->getTopics();
    if (topics.empty()) {
        const char* err = "Topics list cannot be empty";
        LOG_ERROR("%s", err);
        throw(err);
    }
    LOG_DEBUG_0("Publisher Config received...");

    m_publisher = new PublisherThread(
            pub_config, m_err_cv, topics[0], (MessageQueue*) m_udf_output_queue, m_app_name);

    config_destroy(config);
    config_value_destroy(ingestor_type_cvt);
    config_value_destroy(ingestor_queue_cvt);
    config_value_destroy(encoding_value);
    config_value_destroy(udf_value);
}

msg_envelope_elem_body_t* VideoIngestion::process_start_ingestion(msg_envelope_elem_body_t *arg_payload) {
    try {
            LOG_INFO_0("START INGESTION request received from client");
            if (m_ingestion_running.load()) {
                std::string err = "Ingestion already running";
                return m_commandhandler->form_reply_payload((int)REQ_ALREADY_RUNNING, err, NULL);
            }
            IngestRetCode ret = m_ingestor->start();
            if (ret != IngestRetCode::SUCCESS) {
                LOG_ERROR("Failed to start ingestor thread: %d",ret);
                std::string err = "Failed to start ingestor thread";
                return m_commandhandler->form_reply_payload((int)REQ_NOT_HONORED, err, NULL);
            } else {
                LOG_INFO("Ingestor thread started...");
                m_ingestion_running.store(true);
                // acknowledging back to client that ingestion has actually started
                return m_commandhandler->form_reply_payload((int)REQ_HONORED, "SUCCESS", NULL);
            }
    } catch(std::exception& ex) {
        std::string err = "exception occurred request not honored";
        LOG_ERROR("%s %s", ex.what(), err.c_str());
        return m_commandhandler->form_reply_payload((int)REQ_NOT_HONORED, err, NULL);
    }
}

msg_envelope_elem_body_t* VideoIngestion::process_stop_ingestion(msg_envelope_elem_body_t *arg_payload) {
    try {
            LOG_INFO_0("STOP INGESTION request received from client");

            if (!m_ingestion_running.load()) {
                // form a JSON reply buffer
                std::string err = "Ingestion already stopped";
                return m_commandhandler->form_reply_payload((int)REQ_ALREADY_STOPPED, err, NULL);
            }
            // stop the ingestor
            if (m_ingestor) {
                m_ingestor->stop();
            }

            m_ingestion_running.store(false);

            // form a JSON reply buffer
            return m_commandhandler->form_reply_payload((int)REQ_HONORED, "SUCCESS", NULL);
    } catch(std::exception& ex) {
        std::string err = "exception occurred request not honored";
        LOG_ERROR("%s %s", ex.what(), err.c_str());
        return m_commandhandler->form_reply_payload((int)REQ_NOT_HONORED, err, NULL);
    }
}

msg_envelope_elem_body_t* VideoIngestion::process_snapshot(msg_envelope_elem_body_t *arg_payload) {
    try {
            LOG_INFO_0("SNAPSHOT request received from client");

            if (m_ingestion_running.load()) {
                std::string err = "Ingestion already running";
                return m_commandhandler->form_reply_payload((int)REQ_ALREADY_RUNNING, err, NULL);
            }

            IngestRetCode ret = IngestRetCode::NOT_INITIALIZED;
            if (m_ingestor) {
                ret = m_ingestor->start(true);
            }
            if (ret != IngestRetCode::SUCCESS) {
                LOG_ERROR("Failed to start ingestor thread: %d",ret);
                std::string err = "Failed to start ingestor thread";
                return m_commandhandler->form_reply_payload((int)REQ_NOT_HONORED, err, NULL);
            }
            LOG_DEBUG_0("Ingestor thread snapshot started");
            std::mutex mtx;
            std::unique_lock<std::mutex> lck(mtx);
            m_snapshot_cv.wait(lck);
            lck.unlock();
            LOG_DEBUG_0("Stopping ingestor thread");
            if (m_ingestor) {
                m_ingestor->stop();
            }
            m_ingestion_running.store(false);
            return m_commandhandler->form_reply_payload((int)REQ_HONORED, "SUCCESS", NULL);
    } catch(std::exception& ex) {
        std::string err = "exception occurred request not honored";
        LOG_ERROR("%s %s", ex.what(), err.c_str());
        return m_commandhandler->form_reply_payload((int)REQ_NOT_HONORED, err, NULL);
    }
}

VideoIngestion& VideoIngestion::operator=(const VideoIngestion& src) {
    return *this;
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

    // if SW trigger is disabled OR (if sw trigger is enabled && init_state = running) then start ingestion
    if (!m_sw_trgr_en || (m_sw_trgr_en && m_init_state_start)) {
        IngestRetCode ret = m_ingestor->start();
        if (ret != IngestRetCode::SUCCESS) {
            LOG_ERROR_0("Failed to start ingestor thread");
        } else {
            LOG_INFO("Ingestor thread started...");
            m_ingestion_running.store((m_sw_trgr_en) ? true : false);
        }
    }
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
    if (m_udf_input_queue) {
        delete m_udf_input_queue;
    }
    if (m_udf_output_queue) {
        delete m_udf_output_queue;
    }
}
