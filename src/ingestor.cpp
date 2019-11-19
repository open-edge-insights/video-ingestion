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

#include <unistd.h>
#include <sstream>
#include <random>
#include <string>
#include <string.h>
#include <algorithm>
#include <eis/utils/logger.h>
#include "eis/vi/ingestor.h"
#include "eis/vi/opencv_ingestor.h"
#include "eis/vi/gstreamer_ingestor.h"

#define UUID_LENGTH 5

using namespace eis::vi;
using namespace eis::utils;
using namespace eis::udf;

Ingestor::Ingestor(config_t* config, FrameQueue* frame_queue) :
    m_th(NULL), m_initialized(false), m_stop(false),
    m_udf_input_queue(frame_queue) {
        this->m_profile = new Profiling();
}

Ingestor::~Ingestor() {
    LOG_DEBUG_0("Ingestor destructor");
    if(m_initialized.load()) {
        // Stop the thread (if it is running)
        this->stop();

        // Delete the thread
        delete m_th;
    }
}

void Ingestor::run() {
    LOG_INFO_0("Ingestor thread running publishing on stream");

    Frame* frame = NULL;

    while(!m_stop.load()) {
        this->read(frame);

        // Adding image handle to frame
        std::string randuuid = generate_image_handle(UUID_LENGTH);
        msg_envelope_t* meta_data = frame->get_meta_data();
        // Profiling start
        DO_PROFILING(this->m_profile, meta_data, "ts_Ingestor_entry")
        // Profiling end
        msg_envelope_elem_body_t* elem = msgbus_msg_envelope_new_string(randuuid.c_str());
        if (elem == NULL) {
            throw "Failed to create image handle element";
        }
        msgbus_ret_t ret = msgbus_msg_envelope_put(meta_data, "img_handle", elem);
        if(ret != MSG_SUCCESS) {
            LOG_ERROR_0("Failed to put image handle meta-data");
            continue;
        }
        // Profiling start
        DO_PROFILING(this->m_profile, meta_data, "ts_filterQ_entry")
        // Profiling end

        if(m_udf_input_queue->push_wait(frame) != QueueRetCode::SUCCESS) {
            LOG_ERROR_0("Frame queue full, frame dropped");
            delete frame;
        }

        // Profiling start
        DO_PROFILING(this->m_profile, meta_data, "ts_filterQ_exit")
        // Profiling end

        frame = NULL;
        if(m_poll_interval > 0) {
            usleep(m_poll_interval * 1000 * 1000);
        }

    }
    if(frame != NULL)
        delete frame;
    LOG_INFO_0("Ingestor thread stopped");
}

void Ingestor::stop() {
    if(m_initialized.load()) {
        if(!m_stop.load()) {
            m_stop.store(true);
            m_th->join();
        }
    }
}

IngestRetCode Ingestor::start() {
    if(m_stop.load())
        return IngestRetCode::STOPPED;
    else if(m_th != NULL)
        return IngestRetCode::ALREAD_RUNNING;

    m_th = new std::thread(&Ingestor::run, this);

    return IngestRetCode::SUCCESS;
}

Ingestor* eis::vi::get_ingestor(config_t* config, FrameQueue* frame_queue, const char* type) {
    Ingestor* ingestor = NULL;
    // Create the ingestor object based on the type specified in the config
    if(!strcmp(type, "opencv")) {
        ingestor = new OpenCvIngestor(config, frame_queue);
    } else if(!strcmp(type, "gstreamer")) {
        ingestor = new GstreamerIngestor(config, frame_queue);
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
