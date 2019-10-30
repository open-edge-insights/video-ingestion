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

#define PIPELINE "pipeline"
#define RESIZE   "resize"

OpenCvIngestor::OpenCvIngestor(config_t* config, FrameQueue* frame_queue):
    Ingestor(config, frame_queue) {
    m_resize = false;
    m_width = 0;
    m_height = 0;
    m_cap = NULL;
    m_initialized.store(true);

    config_value_t* cvt_pipeline = config->get_config_value(config->cfg, PIPELINE);
    LOG_INFO("cvt_pipeline initialized");
    if(cvt_pipeline == NULL) {
        LOG_ERROR("JSON missing key \'%s\'", PIPELINE);
    } else if(cvt_pipeline->type != CVT_STRING) {
        LOG_ERROR("JSON value for \'%s\' must be a string", PIPELINE);
        config_value_destroy(cvt_pipeline);
    }
    m_pipeline = std::string(cvt_pipeline->body.string);
    config_value_destroy(cvt_pipeline);

    config_value_t* cvt_poll_interval = config->get_config_value(
            config->cfg, POLL_INTERVAL);
    if(cvt_poll_interval != NULL) {
        if(cvt_poll_interval->type != CVT_FLOATING) {
            LOG_INFO_0("Poll interval must be a number");
            config_value_destroy(cvt_poll_interval);
        }
        m_poll_interval = cvt_poll_interval->body.floating;
        config_value_destroy(cvt_poll_interval);
    }

    LOG_INFO("Pipeline: %s", m_pipeline.c_str());
    LOG_INFO("Poll interval: %lf", m_poll_interval);

    m_cap = new cv::VideoCapture(m_pipeline);
    if(!m_cap->isOpened()) {
        LOG_ERROR("Failed to open gstreamer pipeline: %s", m_pipeline.c_str());
    }
}

OpenCvIngestor::~OpenCvIngestor() {
    LOG_DEBUG_0("OpenCV ingestor destructor");
    if(m_cap != NULL) {
        m_cap->release();
        delete m_cap;
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
        LOG_ERROR_0("Failed to read frame from OpenCV video capture");
        // Re-opening the video capture
        m_cap->release();
        delete m_cap;
        m_cap = new cv::VideoCapture(m_pipeline);

        m_cap->read(*cv_frame);
    }

    LOG_DEBUG_0("Frame read successfully");

    frame = new Frame(
            (void*) cv_frame, cv_frame->cols, cv_frame->rows,
            cv_frame->channels(), cv_frame->data, free_cv_frame);
}
