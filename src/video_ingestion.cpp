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

#include <iostream>
#include <safe_lib.h>
#include <eis/utils/logger.h>
#include <eis/utils/msgbus_util.h>
#include <eis/msgbus/msgbus.h>
#include <eis/config_manager/config_manager.h>
#include <unistd.h>
#include "eis/vi/video_ingestion.h"
#include "eis/vi/ingestor.h"

#define INTEL_VENDOR "GenuineIntel"
#define INTEL_VENDOR_LENGTH 12
#define MAX_CONFIG_KEY_LENGTH 40

using namespace eis::vi;
using namespace eis::utils;
using namespace eis::msgbus;

VideoIngestion::VideoIngestion() {

    std::string app_name = getenv("AppName");
    std::string dev_mode_str = getenv("DEV_MODE");
    bool dev_mode;
    if (dev_mode_str == "false") {
        dev_mode = false;
    } else if (dev_mode_str == "true") {
        dev_mode = true;
    }
    std::string pub_cert_file = "";
    std::string pri_key_file = "";
    std::string trust_file = "";
    if(!dev_mode) {
        pub_cert_file = "/run/secrets/etcd_" + app_name + "_cert";
        pri_key_file = "/run/secrets/etcd_" + app_name + "_key";
        trust_file = "/run/secrets/ca_etcd";
    }
    char* err_status = init("etcd", (char*)pub_cert_file.c_str(),
                                (char*) pri_key_file.c_str(), (char*) trust_file.c_str());
    if(!strcmp(err_status, "-1")) {
        throw "Config manager initializtaion failed";
    }

    char config_key[MAX_CONFIG_KEY_LENGTH];
    sprintf(config_key, "/%s/config", app_name.c_str());
    const char* vi_config = get_config(config_key);

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
        m_ingestor_type = ingestor_type_cvt->body.string;

        config_value_object_t* ingestor_cvt = ingestor_value->body.object;
        m_ingestor_cfg = config_new(ingestor_cvt->object, free, get_config_value);
    }

    config_value_t* filter_value = config->get_config_value(config->cfg,
                                                            "filter");
    m_udf_input_queue = new FrameQueue(-1);
    if(filter_value == NULL) {
        LOG_INFO("No filter specified!!")
        m_udf_output_queue = m_udf_input_queue;
    } else {
        config_value_object_t* filter_cvt = filter_value->body.object;
        m_filter_cfg = config_new(filter_cvt, free, get_config_value);
        m_udf_output_queue = new FrameQueue(-1);
    }
}

void VideoIngestion::start() {
    try {
        MsgBusUtil* msgbus_util = new MsgBusUtil();
        std::vector<std::string> topics = msgbus_util->get_topics_from_env("pub");
        if(topics.size() > 1) {
            LOG_INFO("Only one topic is supported");
            return;
        }
        std::string topic_type = "pub";
      	config_t* msgbus_config = msgbus_util->get_messagebus_config(topics[0],
                                                                     topic_type);

        m_publisher = new Publisher(
                msgbus_config, topics[0], (InputMessageQueue*) m_udf_output_queue);
        m_publisher->start();
        LOG_INFO("Publisher thread started...");

        #if 0
        m_udf_manager = new UdfManager(
            m_filter_cfg, m_udf_input_queue, m_udf_output_queue);
        m_udf_manager->start();
        #endif

        m_ingestor = IngestorWrapper::get_ingestor(m_ingestor_cfg, m_udf_input_queue, m_ingestor_type);
        m_ingestor->start();
        LOG_INFO("Ingestor thread started...");

        while(1) {
            usleep(2);
        }
    } catch(const std::exception ex) {
        LOG_ERROR("Exception '%s' occurred", ex.what());
        cleanup();
    } catch(...) { // To catch all other non-exception types
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
    if(m_filter_cfg) {
        delete m_filter_cfg;
    }
    if(m_ingestor_cfg) {
        delete m_ingestor_cfg;
    }
    if(m_filter_cfg) {
        delete m_filter_cfg;
    }
}

VideoIngestion::~VideoIngestion() {
    cleanup();
}