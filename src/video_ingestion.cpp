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
#include <eis/utils/logger.h>
#include <eis/msgbus/msgbus.h>
#include <eis/config_manager/config_manager.h>
#include <unistd.h>
#include "eis/vi/video_ingestion.h"
#include "eis/vi/ingestor.h"

#define INTEL_VENDOR "GenuineIntel"
#define INTEL_VENDOR_LENGTH 12
#define MAX_CONFIG_KEY_LENGTH 40
#define DEFAULT_QUEUE_SIZE 10

using namespace eis::vi;
using namespace eis::utils;
using namespace eis::msgbus;

VideoIngestion::VideoIngestion() {

    m_app_name = getenv("AppName");
    m_env_config = new MsgBusUtil();
    m_config_mgr_client = m_env_config->get_config_mgr_client();

    char config_key[MAX_CONFIG_KEY_LENGTH];
    sprintf(config_key, "/%s/config", m_app_name.c_str());
    const char* vi_config = m_config_mgr_client->get_config(config_key);

    LOG_INFO("App config: %s", vi_config);
    config_t* config = json_config_new_from_buffer(vi_config);
    if(config == NULL) {
        throw("Failed to initialize configuration object");
    }

    config_value_t* ingestor_value = config->get_config_value(config->cfg,
                                                              "ingestor");
    if(ingestor_value == NULL) {
        const char* err = "ingestor key is missing";
        LOG_ERROR("%s", err);
        throw(err);
    } else {
        config_value_t* ingestor_type_cvt = config_value_object_get(ingestor_value,
                                                                    "type");
        if(ingestor_type_cvt == NULL) {
            const char* err = "\"type\" key missing";
            LOG_ERROR("%s", err);
            throw(err);
        }
        if(ingestor_type_cvt->type != CVT_STRING) {
            const char* err = "\"type\" value has to be of string type";
            LOG_ERROR("%s", err);
            throw(err);
        }
        m_ingestor_type = ingestor_type_cvt->body.string;

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
                throw(err);
            }
            queue_size = ingestor_queue_cvt->body.integer;
        }
        m_udf_input_queue = new FrameQueue(queue_size);

        config_value_object_t* ingestor_cvt = ingestor_value->body.object;
        m_ingestor_cfg = config_new(ingestor_cvt->object, free, get_config_value);

        config_value_t* udf_value = config->get_config_value(config->cfg,
                                                             "udf");
        if(udf_value == NULL) {
            LOG_INFO("\"udf\" key doesn't exist, so udf output queue is same as \
                    udf input queue!!")
            m_udf_output_queue = m_udf_input_queue;
        } else {
            m_udf_output_queue = new FrameQueue(queue_size);
            config_value_object_t* udf_cvt = udf_value->body.object;
            m_udf_cfg = config_new(udf_cvt, free, get_config_value);
        }
    }
}

void VideoIngestion::start() {
    try {
        std::vector<std::string> topics = m_env_config->get_topics_from_env("pub");
        if(topics.size() > 1) {
            const char* err = "Only one topic is supported";
            LOG_ERROR("%s", err);
            throw(err);
        }
        std::string topic_type = "pub";
      	config_t* msgbus_config = m_env_config->get_messagebus_config(topics[0],
                                                                     topic_type);

        m_publisher = new Publisher(
                msgbus_config, topics[0], (InputMessageQueue*) m_udf_output_queue);
        m_publisher->start();
        LOG_INFO("Publisher thread started...");

        #if 0
        m_udf_manager = new UdfManager(
            m_udf_cfg, m_udf_input_queue, m_udf_output_queue);
        m_udf_manager->start();
        #endif

        m_ingestor = get_ingestor(m_ingestor_cfg, m_udf_input_queue, m_ingestor_type);

        m_ingestor->start();
        LOG_INFO("Ingestor thread started...");

        while(1) {
            usleep(2);
        }
    } catch(const std::exception ex) {
        LOG_ERROR("Exception '%s' occurred", ex.what());
        cleanup();
    }
}

void VideoIngestion::stop() {
    if(m_ingestor) {
        m_ingestor->stop();
        delete m_ingestor;
    }
    #if 0
    if(m_udf_manager) {
        m_udf_manager->stop();
        delete m_udf_manager;
    }
    #endif
    if(m_publisher) {
        m_publisher->stop();
        delete m_publisher;
    }
}

void VideoIngestion::cleanup() {
    stop();
    if(m_ingestor_cfg) {
        delete m_ingestor_cfg;
    }
    if(m_udf_cfg) {
        delete m_udf_cfg;
    }
    if(m_ingestor_cfg) {
        delete m_ingestor_cfg;
    }
    if(m_udf_cfg) {
        delete m_udf_cfg;
    }
    if(m_config_mgr_client) {
        delete m_config_mgr_client;
    }
    if(m_config_mgr_config) {
        delete m_config_mgr_config;
    }
    if(m_env_config) {
        delete m_env_config;
    }
}

VideoIngestion::~VideoIngestion() {
    cleanup();
}