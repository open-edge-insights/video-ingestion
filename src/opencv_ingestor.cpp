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
 * @brief OpenCV Ingestor implementation
 */

#include <string>
#include <vector>
#include <cerrno>

#include <eis/msgbus/msgbus.h>
#include <eis/utils/logger.h>
#include <eis/utils/json_config.h>
#include "eis/vi/opencv_ingestor.h"


using namespace eis::vi;
using namespace eis::utils;
using namespace eis::udf;

#define PIPELINE "pipeline"
#define RESIZE   "resize"
#define LOOP_VIDEO "loop_video"

OpenCvIngestor::OpenCvIngestor(config_t* config, FrameQueue* frame_queue, std::string service_name, std::condition_variable& snapshot_cv, EncodeType enc_type, int enc_lvl):
    Ingestor(config, frame_queue, service_name, snapshot_cv, enc_type, enc_lvl) {
    m_resize = false;
    m_width = 0;
    m_height = 0;
    m_cap = NULL;
    m_encoding = false;
    m_loop_video = false;
    m_initialized.store(true);

    config_value_t* cvt_pipeline = config->get_config_value(config->cfg, PIPELINE);
    LOG_INFO("cvt_pipeline initialized");
    if(cvt_pipeline == NULL) {
        const char* err = "JSON missing key";
        LOG_ERROR("%s \'%s\'", err, PIPELINE);
        throw(err);
    } else if(cvt_pipeline->type != CVT_STRING) {
        config_value_destroy(cvt_pipeline);
        const char* err = "JSON value must be a string";
        LOG_ERROR("%s for \'%s\'", err, PIPELINE);
        throw(err);
    }
    m_pipeline = std::string(cvt_pipeline->body.string);
    LOG_INFO("Pipeline: %s", m_pipeline.c_str());
    config_value_destroy(cvt_pipeline);

    config_value_t* cvt_loop_video = config->get_config_value(
            config->cfg, LOOP_VIDEO);
    if(cvt_loop_video != NULL) {
        if(cvt_loop_video->type != CVT_BOOLEAN) {
            LOG_ERROR_0("Loop video must be a boolean");
            config_value_destroy(cvt_loop_video);
        }
        if(cvt_loop_video->body.boolean) {
            m_loop_video = true;
        }
        config_value_destroy(cvt_loop_video);
    }

    m_cap = new cv::VideoCapture(m_pipeline);
    if(!m_cap->isOpened()) {
        LOG_ERROR("Failed to open gstreamer pipeline: %s", m_pipeline.c_str());
    }
}

OpenCvIngestor::~OpenCvIngestor() {
    LOG_DEBUG_0("OpenCV ingestor destructor");
    if(m_cap != NULL) {
        m_cap->release();
        LOG_DEBUG_0("Cap deleted");
    }
}

void free_cv_frame(void* obj) {
    cv::Mat* frame = (cv::Mat*) obj;
    frame->release();
    delete frame;
}

void OpenCvIngestor::read(Frame*& frame) {

    cv::Mat* cv_frame = new cv::Mat();

    if(!m_cap->read(*cv_frame)) {
        if(cv_frame->empty()) {
            // cv_frame->empty signifies video has ended
            if(m_loop_video == true) {
                // Re-opening the video capture
                LOG_WARN_0("Video ended. Looping...");
                m_cap->release();
                delete m_cap;
                m_cap = new cv::VideoCapture(m_pipeline);
            } else {
                const char* err = "Video ended...";
                LOG_WARN("%s", err);
                // Sleeping indefinitely to avoid restart
                while(true) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            }
            m_cap->read(*cv_frame);
        } else {
            // Error due to malformed frame
            const char* err = "Failed to read frame from OpenCV video capture";
            LOG_ERROR("%s", err);
        }
    }

    LOG_DEBUG_0("Frame read successfully");

    frame = new Frame(
            (void*) cv_frame, cv_frame->cols, cv_frame->rows,
            cv_frame->channels(), cv_frame->data, free_cv_frame);
}

void OpenCvIngestor::stop() {
    if(m_initialized.load()) {
        if(!m_stop.load()) {
            m_stop.store(true);
            // wait for the ingestor thread function run() to finish its execution.
            if(m_th != NULL) {
                m_th->join();
            }
        }
    // After its made sure that the Ingestor run() function has been stopped (as in m_th-> join() above), m_stop flag is reset
    // so that the ingestor is ready for the next ingestion.
    m_running.store(false);
    m_stop.store(false);
    LOG_INFO_0("Releasing video capture object");
    if(m_cap != NULL) {
        m_cap->release();
        LOG_DEBUG_0("Capture object deleted");
    }
    }
}
