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
 * @brief Ingestor Base Implementation
 */

#include <sstream>
#include <random>
#include <string>
#include <string.h>
#include <algorithm>
#include <eis/utils/logger.h>
#include <eis/utils/thread_safe_queue.h>
#include "eis/vi/ingestor.h"
#include "eis/vi/opencv_ingestor.h"
#include "eis/vi/gstreamer_ingestor.h"

using namespace eis::vi;
using namespace eis::utils;
using namespace eis::udf;

Ingestor::Ingestor(config_t* config, FrameQueue* frame_queue, std::string service_name, std::condition_variable& snapshot_cv, EncodeType enc_type=EncodeType::NONE, int enc_lvl=0) :
      m_service_name(service_name), m_th(NULL), m_initialized(false), m_stop(false), m_udf_input_queue(frame_queue), m_snapshot_cv(snapshot_cv), m_enc_type(enc_type), m_enc_lvl(enc_lvl) {

        m_ingestor_block_key = m_service_name + "_ingestor_blocked_ts";
        config_value_t* cvt_poll_interval = config->get_config_value(config->cfg, POLL_INTERVAL);
        if(cvt_poll_interval != NULL) {
            if(cvt_poll_interval->type != CVT_FLOATING && cvt_poll_interval->type != CVT_INTEGER) {
                const char* err = "Poll interval must be a number";
                LOG_ERROR("%s for \'%s\'", err, PIPELINE);
                config_value_destroy(cvt_poll_interval);
                throw(err);
            } else {
                if(cvt_poll_interval->type == CVT_FLOATING) {
                    m_poll_interval = cvt_poll_interval->body.floating;
                } else if(cvt_poll_interval->type == CVT_INTEGER) {
                    m_poll_interval = (double)cvt_poll_interval->body.integer;
                }
            }
            config_value_destroy(cvt_poll_interval);
        } else {
            m_poll_interval = 0.0 ;
        }
        LOG_INFO("Poll interval: %lf", m_poll_interval);

        m_running.store(false);
        this->m_profile = new Profiling();
}

Ingestor::~Ingestor() {
    LOG_DEBUG_0("Ingestor destructor");
    if(m_initialized.load()) {
        // Delete the thread
        delete m_th;
        // Delete profiling variable
        delete m_profile;
    }
}

IngestRetCode Ingestor::start(bool snapshot_mode) {
    if (snapshot_mode) {
        m_stop.store(false);
    }
    if (m_stop.load())
        return IngestRetCode::STOPPED;
    else if (m_running.load())
        return IngestRetCode::ALREAD_RUNNING;

    m_th = new std::thread(&Ingestor::run, this, snapshot_mode);

    return IngestRetCode::SUCCESS;
}

Ingestor* eis::vi::get_ingestor(config_t* config, FrameQueue* frame_queue, const char* type, std::string service_name, std::condition_variable& snapshot_cv, EncodeType enc_type, int enc_lvl) {
    Ingestor* ingestor = NULL;
    // Create the ingestor object based on the type specified in the config
    if(!strcmp(type, "opencv")) {
        ingestor = new OpenCvIngestor(config, frame_queue, service_name, snapshot_cv, enc_type, enc_lvl);
    } else if(!strcmp(type, "gstreamer")) {
        ingestor = new GstreamerIngestor(config, frame_queue, service_name, snapshot_cv, enc_type, enc_lvl);
    } else {
        throw("Unknown ingestor");
    }
    return ingestor;
}

std::string Ingestor::generate_image_handle(const int len) {
    std::stringstream ss;
    for (auto i = 0; i < len; i++) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        const auto rc = dis(gen);
        std::stringstream hexstream;
        hexstream << std::hex << rc;
        auto hex = hexstream.str();
        ss << (hex.length() < 2 ? '0' + hex : hex);
    }
    return ss.str();
}
