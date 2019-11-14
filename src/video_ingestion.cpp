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

using namespace eis::vi;
using namespace eis::utils;
using namespace eis::config_manager;
using namespace eis::msgbus;
using namespace eis::udf;

VideoIngestion::VideoIngestion(
        std::condition_variable& err_cv, EnvConfig* env_config, char* vi_config) :
    m_err_cv(err_cv)
{
    // Parse the configuration
    config_t* config = json_config_new_from_buffer(vi_config);
    if(config == NULL) {
        const char* err = "Failed to initialize configuration object";
        LOG_ERROR("%s", err);
        throw(err);
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
                config_destroy(config);
                config_value_destroy(ingestor_queue_cvt);
                throw(err);
            }
            queue_size = ingestor_queue_cvt->body.integer;
        }
        m_udf_input_queue = new FrameQueue(queue_size);

        config_value_object_t* ingestor_cvt = ingestor_value->body.object;
        config_t* ingestor_cfg = config_new(ingestor_cvt->object, free, get_config_value);
        if(ingestor_cfg == NULL) {
            const char* err = "Unable to get ingestor config";
            LOG_ERROR("%s", err);
            config_destroy(config);
            throw(err);
        }

        config_value_t* udf_value = config->get_config_value(config->cfg,
                                                             "udfs");
        if(udf_value == NULL) {
            m_udfs_key_exists = false;
            LOG_INFO("\"udfs\" key doesn't exist, so udf output queue is same as \
                    udf input queue!!")
            m_udf_output_queue = m_udf_input_queue;
        } else {
            m_udfs_key_exists = true;
            m_udf_output_queue = new FrameQueue(queue_size);
        }
        m_ingestor = get_ingestor(ingestor_cfg, m_udf_input_queue, m_ingestor_type);
    }

    std::vector<std::string> pub_topics = env_config->get_topics_from_env("pub");
    if(pub_topics.size() != 1) {
        const char* err = "Only one topic is supported. Neither more nor less";
        LOG_ERROR("%s", err);
        config_destroy(config);
        throw(err);
    }
    std::string topic_type = "pub";
    config_t* pub_config = env_config->get_messagebus_config(pub_topics[0],
                                                                    topic_type);
    if(pub_config == NULL) {
        const char* err = "Failed to get message bus config";
        LOG_ERROR("%s", err);
        config_destroy(config);
        throw(err);
    }
    m_publisher = new Publisher(
            pub_config, m_err_cv, pub_topics[0], (MessageQueue*) m_udf_output_queue);
   
    if(m_udfs_key_exists) {
        m_udf_manager = new UdfManager(config, m_udf_input_queue, m_udf_output_queue);
    }
}

void VideoIngestion::start() {
    m_publisher->start();
    LOG_INFO("Publisher thread started...");
    m_udf_manager->start();
    LOG_INFO_0("Started udf manager");
    IngestRetCode ret = m_ingestor->start();
    if(ret != IngestRetCode::SUCCESS) {
        LOG_ERROR_0("Failed to start ingestor thread");
    }
    else{
        LOG_INFO("Ingestor thread started...");
    }
}

void VideoIngestion::stop() {
    if(m_ingestor) {
        m_ingestor->stop();
    }
    if(m_udf_manager) {
        m_udf_manager->stop();
    }
    if(m_publisher) {
        m_publisher->stop();
    }
}

VideoIngestion::~VideoIngestion() {
    delete m_ingestor;
    delete m_udf_manager;
    delete m_publisher;
}
