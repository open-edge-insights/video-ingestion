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

using namespace eis::vi;
using namespace eis::utils;
using namespace eis::msgbus;
using namespace eis::udf;

VideoIngestion::VideoIngestion(
        std::condition_variable& err_cv, const env_config_t* env_config, char* vi_config, const config_mgr_t* g_config_mgr) :
    m_err_cv(err_cv), m_enc_type(EncodeType::NONE), m_enc_lvl(0)
{
    // Parse the configuration
    config_t* config = json_config_new_from_buffer(vi_config);
    if(config == NULL) {
        const char* err = "Failed to initialize configuration object";
        LOG_ERROR("%s", err);
        throw(err);
    }
    config_value_t* encoding_value = config->get_config_value(config->cfg,
                                                              "encoding");
    if(encoding_value == NULL) {
        const char* err = "\"encoding\" key is missing";
        LOG_WARN("%s", err);
    } else {
        config_value_t* encoding_type_cvt = config_value_object_get(encoding_value,
                                                                    "type");
        if(encoding_type_cvt == NULL) {
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

    // Get ingestor
    m_ingestor = get_ingestor(ingestor_cfg, m_udf_input_queue, m_ingestor_type);


    char** pub_topics = env_config->get_topics_from_env(PUB);

    int pub_topic_length = 0;
    while (pub_topics[pub_topic_length] != NULL) {
        pub_topic_length++;
        if(pub_topic_length != 1){
            const char* err = "Only one topic is supported. Neither more, nor less";
            LOG_ERROR("%s", err);
            config_destroy(config);
            throw(err);
        }
    }

    LOG_DEBUG_0("Successfully read PubTopics env value...");

    config_t* pub_config = env_config->get_messagebus_config(g_config_mgr,pub_topics[0], PUB);      
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
    
    m_udf_manager = new UdfManager(config, m_udf_input_queue, m_udf_output_queue,
                                   m_enc_type, m_enc_lvl, m_udfs_key_exists);

    config_value_destroy(ingestor_type_cvt);
    config_value_destroy(ingestor_queue_cvt);
    config_value_destroy(encoding_value);
    config_value_destroy(udf_value);

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
