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
#include <eis/utils/logger.h>
#include "eis/vi/ingestor.h"
#include "eis/vi/opencv_ingestor.h"
#include "eis/vi/gstreamer_ingestor.h"

using namespace eis::vi;
using namespace eis::utils;

Ingestor::Ingestor(config_t* config, FrameQueue* frame_queue) :
    m_th(NULL), m_initialized(false), m_stop(false),
    m_udf_input_queue(frame_queue) {

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
        if(m_udf_input_queue->push(frame) != QueueRetCode::SUCCESS) {
            LOG_ERROR_0("Frame queue full, frame dropped");
        }
        frame = NULL;
        if(m_poll_interval > 0) {
            usleep(m_poll_interval);
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
    if(!m_initialized.load())
        return IngestRetCode::NOT_INITIALIZED;
    else if(m_stop.load())
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