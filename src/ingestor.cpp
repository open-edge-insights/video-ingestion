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

Ingestor::Ingestor(config_t* config, FrameQueue* frame_queue, EncodeType enc_type=EncodeType::NONE, int enc_lvl=0) :
    m_th(NULL), m_initialized(false), m_stop(false),
    m_udf_input_queue(frame_queue),  m_enc_type(enc_type), m_enc_lvl(enc_lvl) {
        // Setting default poll interval
        // This will be over written with the sub class poll interval
        m_poll_interval = 0;
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

void Ingestor::run() {
    // indicate that the run() function corresponding to the m_th thread has started
    m_running.store(true);
    LOG_INFO_0("Ingestor thread running publishing on stream");

    Frame* frame = NULL;

    int64_t frame_count = 0;
    while (!m_stop.load()) {
        this->read(frame);

        // Adding image handle to frame
        std::string randuuid = generate_image_handle(UUID_LENGTH);
        msg_envelope_t* meta_data = frame->get_meta_data();
        // Profiling start
        DO_PROFILING(this->m_profile, meta_data, "ts_Ingestor_entry")

        // Profiling end


        msg_envelope_elem_body_t* elem = NULL;
        msgbus_ret_t ret;
        if(frame_count == INT64_MAX) {
            LOG_WARN_0("frame count has reached INT64_MAX, so resetting \
                        it back to zero");
            frame_count = 0;
        }
        frame_count++;

        elem = msgbus_msg_envelope_new_integer(frame_count);
        if (elem == NULL) {
            LOG_ERROR_0("Failed to create frame_number element");
            delete frame;
        }
        ret = msgbus_msg_envelope_put(meta_data, "frame_number", elem);
        if(ret != MSG_SUCCESS) {
            LOG_ERROR_0("Failed to put frame_number meta-data");
            delete frame;
            continue;
        }
        LOG_DEBUG("Frame number: %d", frame_count);

        elem = msgbus_msg_envelope_new_string(randuuid.c_str());
        if (elem == NULL) {
            LOG_ERROR_0("Failed to create image handle element");
            delete frame;
        }
        ret = msgbus_msg_envelope_put(meta_data, "img_handle", elem);
        if(ret != MSG_SUCCESS) {
            LOG_ERROR_0("Failed to put image handle meta-data");
            delete frame;
            continue;
        }

        // Profiling start
        DO_PROFILING(this->m_profile, meta_data, "ts_filterQ_entry")
        // Profiling end

        // Set encding type and level
        frame->set_encoding(m_enc_type, m_enc_lvl);

        m_udf_input_queue->push_wait(frame);

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
            // wait for the ingestor thread function run() to finish its execution.
            if(m_th != NULL) {
                m_th->join();
            }
        }
    }
    // After its made sure that the Ingestor run() function has been stopped (as in m_th-> join() above), m_stop flag is reset
    // so that the ingestor is ready for the next ingestion.
    m_running.store(false);
    m_stop.store(false);
}

IngestRetCode Ingestor::start() {
    if (m_stop.load())
        return IngestRetCode::STOPPED;
    else if (m_running.load())
        return IngestRetCode::ALREAD_RUNNING;

    m_th = new std::thread(&Ingestor::run, this);

    return IngestRetCode::SUCCESS;
}

Ingestor* eis::vi::get_ingestor(config_t* config, FrameQueue* frame_queue, const char* type, EncodeType enc_type, int enc_lvl) {
    Ingestor* ingestor = NULL;
    // Create the ingestor object based on the type specified in the config
    if(!strcmp(type, "opencv")) {
        ingestor = new OpenCvIngestor(config, frame_queue, enc_type, enc_lvl);
    } else if(!strcmp(type, "gstreamer")) {
        ingestor = new GstreamerIngestor(config, frame_queue, enc_type, enc_lvl);
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
