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
#include <unistd.h>

#include <eii/msgbus/msgbus.h>
#include <eii/utils/logger.h>
#include <eii/utils/json_config.h>
#include "eii/vi/opencv_ingestor.h"

#include <glob.h>
#include <sys/stat.h>

using namespace std;

using namespace eii::vi;
using namespace eii::utils;
using namespace eii::udf;

#define PIPELINE "pipeline"
#define LOOP_VIDEO "loop_video"
#define UUID_LENGTH 5


OpenCvIngestor::OpenCvIngestor(config_t* config, FrameQueue* frame_queue, std::string service_name, std::condition_variable& snapshot_cv, EncodeType enc_type, int enc_lvl):
    Ingestor(config, frame_queue, service_name, snapshot_cv, enc_type, enc_lvl) {
    m_width = 0;
    m_height = 0;
    m_cap = NULL;
    m_encoding = false;
    m_loop_video = false;
    m_double_frames = false;
    m_initialized.store(true);
    m_img_flag = false;


    config_value_t* cvt_double = config_get(config, "double_frames");
    if (cvt_double != NULL) {
        LOG_DEBUG_0("DOUBLING FRAMES");
        if (cvt_double->type != CVT_BOOLEAN) {
            config_value_destroy(cvt_double);
            LOG_ERROR_0("double_frames must be a boolean");
            throw "ERROR";
        }
        m_double_frames = cvt_double->body.boolean;
        config_value_destroy(cvt_double);
    }

    if (config_value_t* cvt_image = config_get(config, "image_ingestion")) {
        if (cvt_image->type == CVT_BOOLEAN) {
            m_img_flag = cvt_image->body.boolean;
            config_value_destroy(cvt_image);
        } else {
            config_value_destroy(cvt_image);
            const char* err = "JSON value must be a boolean";
            LOG_ERROR("%s for \'%s\'", err, "IMAGE_INGESTION");
            throw(err);
        } 
    }    
        
    config_value_t* cvt_pipeline = config->get_config_value(config->cfg, PIPELINE);
    LOG_INFO("cvt_pipeline initialized");
    if (cvt_pipeline == NULL) {
        const char* err = "JSON missing key";
        LOG_ERROR("%s \'%s\'", err, PIPELINE);
        throw(err);
    } else if (cvt_pipeline->type != CVT_STRING) {
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
    if (cvt_loop_video != NULL) {
        if (cvt_loop_video->type != CVT_BOOLEAN) {
            LOG_ERROR_0("Loop video must be a boolean");
            config_value_destroy(cvt_loop_video);
        }
        if (cvt_loop_video->body.boolean) {
            m_loop_video = true;
        }
        config_value_destroy(cvt_loop_video);
    }

    if (m_img_flag) {
	// Verify if image directory is volume mounted    
	struct stat buffer;
        if (stat(m_pipeline.c_str(), &buffer) != 0) {
            LOG_ERROR("%s directory is empty. Failed to open opencv pipeline", m_pipeline.c_str());
        }
    } else {
        m_cap = new cv::VideoCapture(m_pipeline);
        if (!m_cap->isOpened()) {
        LOG_ERROR("Failed to open opencv pipeline: %s", m_pipeline.c_str());
        }
    }     
}

OpenCvIngestor::~OpenCvIngestor() {
    LOG_DEBUG_0("OpenCV ingestor destructor");
    if (m_cap != NULL) {
        m_cap->release();
        LOG_DEBUG_0("Cap deleted");
    }
}

void free_cv_frame(void* obj) {
    cv::Mat* frame = (cv::Mat*) obj;
    frame->release();
    delete frame;
}

void OpenCvIngestor::run(bool snapshot_mode) {
    // indicate that the run() function corresponding to the m_th thread has started
    m_running.store(true);
    LOG_INFO_0("Ingestor thread running publishing on stream");

    Frame* frame = NULL;

    int64_t frame_count = 0;

    msg_envelope_elem_body_t* elem = NULL;

    try {
        while (!m_stop.load()) {

            if (m_img_flag) {
                this->imread(frame);
            } else {
                this->read(frame);
            }            
            msg_envelope_t* meta_data = frame->get_meta_data();
            // Profiling start
            DO_PROFILING(this->m_profile, meta_data, "ts_Ingestor_entry")

            // Profiling end

            msgbus_ret_t ret;
            if (frame_count == INT64_MAX) {
                LOG_WARN_0("frame count has reached INT64_MAX, so resetting \
                            it back to zero");
                frame_count = 0;
            }
            frame_count++;

            elem = msgbus_msg_envelope_new_integer(frame_count);
            if (elem == NULL) {
                delete frame;
                const char* err = "Failed to create frame_number element";
                LOG_ERROR("%s", err);
                throw err;
            }
            ret = msgbus_msg_envelope_put(meta_data, "frame_number", elem);
            if (ret != MSG_SUCCESS) {
                delete frame;
                const char* err = "Failed to put frame_number in meta-data";
                LOG_ERROR("%s", err);
                throw err;
            }
            elem = NULL;
            LOG_DEBUG("Frame number: %ld", frame_count);

            // Profiling start
            DO_PROFILING(this->m_profile, meta_data, "ts_filterQ_entry")
            // Profiling end

            // Set encding type and level
            try {
                frame->set_encoding(m_enc_type, m_enc_lvl);
            } catch(const char *err) {
                LOG_ERROR("Exception: %s", err);
            } catch(...) {
                LOG_ERROR("Exception occurred in set_encoding()");
            }

            QueueRetCode ret_queue = m_udf_input_queue->push(frame);
            if (ret_queue == QueueRetCode::QUEUE_FULL) {
                if (m_udf_input_queue->push_wait(frame) != QueueRetCode::SUCCESS) {
                    LOG_ERROR_0("Failed to enqueue message, "
                                "message dropped");
                }
                // Add timestamp which acts as a marker if queue if blocked
                DO_PROFILING(this->m_profile, meta_data, m_ingestor_block_key.c_str());
            }

            frame = NULL;

            if (snapshot_mode) {
                m_stop.store(true);
                m_snapshot_cv.notify_all();
            }
        }
    } catch(const char* err) {
        LOG_ERROR("Exception: %s", err);
        if (elem != NULL)
            msgbus_msg_envelope_elem_destroy(elem);
        if (frame != NULL)
            delete frame;
        throw err;
    } catch(...) {
        LOG_ERROR("Exception occured in opencv ingestor run()");
        if (elem != NULL)
            msgbus_msg_envelope_elem_destroy(elem);
        if (frame != NULL)
            delete frame;
        throw;
    }
    if (elem != NULL)
        msgbus_msg_envelope_elem_destroy(elem);
    if (frame != NULL)
        delete frame;
    LOG_INFO_0("Ingestor thread stopped");
    if (snapshot_mode)
        m_running.store(false);
}

void OpenCvIngestor::read(Frame*& frame) {

    cv::Mat* cv_frame = new cv::Mat();
    cv::Mat* frame_copy = NULL;
 
    if (m_cap == NULL) {
        m_cap = new cv::VideoCapture(m_pipeline);
        if (!m_cap->isOpened()) {
            LOG_ERROR("Failed to open opencv pipeline: %s", m_pipeline.c_str());
        }
    }

    if (!m_cap->read(*cv_frame)) {
        if (cv_frame->empty()) {
            // cv_frame->empty signifies video has ended
            if (m_loop_video == true) {
                // Re-opening the video capture
                LOG_WARN_0("Video ended. Looping...");
                m_cap->release();
                delete m_cap;
                m_cap = new cv::VideoCapture(m_pipeline);
            } else {
                const char* err = "Video ended...";
                LOG_WARN("%s", err);
                // Sleeping indefinitely to avoid restart
                while (true) {
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
            (void*) cv_frame, free_cv_frame, (void*) cv_frame->data,
            cv_frame->cols, cv_frame->rows, cv_frame->channels());

    if (m_double_frames) {
        frame_copy = new cv::Mat();
        *frame_copy = cv_frame->clone();
        frame->add_frame(
            (void*) frame_copy, free_cv_frame, (void*) frame_copy->data,
            frame_copy->cols, frame_copy->rows, frame_copy->channels(),
            EncodeType::NONE, 0);
    }

    if (m_poll_interval > 0) {
        usleep(m_poll_interval * 1000 * 1000);
    }
}

void OpenCvIngestor::imread(Frame*& frame) {
    cv::Mat* cv_frame = new cv::Mat();
    // save the path to the images present in directory /app/img_dir/  
    static vector<cv::String> filenames;
    // current image format
    static int image_format_index;
    // for indexing filename
    static size_t k;
    const string image_formats[] = {"jpg" , "JPG" , "jpeg" , "JPEG" ,  "jpe", "JPE" ,  "bmp" , "BMP" , "png" , "PNG"};       
    // length of the array image_format
    const int len_image_format = sizeof(image_formats) / sizeof(image_formats[0]);
    // pattern for reading images
    string img_path;
    // counter to count the total number of image formats read for verifying empty image directory
    int count = 0;
    // store the resolution of the image
    cv::Size size;
    // maximum height and width of the image
    const int MAX_IMAGE_WIDTH = 1920;
    const int MAX_IMAGE_HEIGHT = 1200;

    if (image_format_index == 0 && k == 0) {
        img_path = m_pipeline + "*." + image_formats[image_format_index];
	cv::glob(img_path , filenames , true);
        count++;
    } 
    // filename.size() gives the total number of images within the directory having image format specified in the image_formats[type]
    if (k < filenames.size()) {
	cv::Mat image  = cv::imread(filenames[k]);
	if (image.empty()) {
	    LOG_ERROR("Could not read image : %s", filenames[k].c_str());
	} else {
	    // find the resolution of the image
	    size = image.size();
	    if (size.width > MAX_IMAGE_WIDTH || size.height > MAX_IMAGE_HEIGHT) {
		// resize the image to width = 1920 and height = 1200
		cv::resize(image, *cv_frame, cv::Size(MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT));
	    } else {
                *cv_frame = image;
	    }
	}
        k++;
    } else {
        k = 0;
        do {
            count++;
            if (count == len_image_format+1) {
		// Exit loop if no images are present within the directory    
                LOG_ERROR("No images present within directory. Failed to open opencv pipeline %s", m_pipeline.c_str());
		image_format_index = 0;
	        break;
            }         
	    // If type has reached last image format value, then all image formats are processed
	    if (image_format_index == len_image_format - 1) {
                if (m_loop_video == true) {
                    // Start reading images from begining
                    LOG_WARN_0("Images ended. Looping...");
                    image_format_index = 0;
                } else {
                    const char* err = "Images ended...";
                    LOG_WARN("%s", err);
                    // Sleeping indefinitely to avoid restart
                    while (true) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    }
	        }
            } else {
		// proceed to the next image format    
		image_format_index++;
	    } 
	    img_path = m_pipeline + "*." + image_formats[image_format_index];
            cv::glob(img_path , filenames , true);
	} while (filenames.size() == 0);
        
	if (count != len_image_format+1) {
	    cv::Mat image  = cv::imread(filenames[k]);
            if (image.empty()) {
                LOG_ERROR("Could not read image : %s", filenames[k].c_str());
            } else {
		// find the resolution of the image 
                size = image.size();
                if (size.width > MAX_IMAGE_WIDTH || size.height > MAX_IMAGE_HEIGHT) {
		    // resize the image to resolution width = 1920 and height = 1200
		    cv::resize(image, *cv_frame, cv::Size(MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT));
                } else {
                    *cv_frame = image;
                }
	    }
	}
        k++;
    }
    
    LOG_DEBUG_0("Image read successfully");

    frame = new Frame(
            (void*) cv_frame, free_cv_frame, (void*) cv_frame->data,
            cv_frame->cols, cv_frame->rows, cv_frame->channels());

    if (m_poll_interval > 0) {
        usleep(m_poll_interval * 1000 * 1000);
    }
} 

void OpenCvIngestor::stop() {
    if (m_initialized.load()) {
        if (!m_stop.load()) {
            m_stop.store(true);
            // wait for the ingestor thread function run() to finish its execution.
            if (m_th != NULL) {
                m_th->join();
            }
        }
    // After its made sure that the Ingestor run() function has been stopped (as in m_th-> join() above), m_stop flag is reset
    // so that the ingestor is ready for the next ingestion.
    m_running.store(false);
    m_stop.store(false);
    LOG_INFO_0("Releasing video capture object");
    if (m_cap != NULL) {
        m_cap->release();
        delete m_cap;
        m_cap = NULL;
        LOG_DEBUG_0("Capture object deleted");
    }
    }
}
