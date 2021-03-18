// Copyright (c) 2021 Intel Corporation.
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
 * @brief RealSense Ingestor implementation
 */

#include <string>
#include <vector>
#include <cerrno>
#include <unistd.h>
#include <eii/msgbus/msgbus.h>
#include <eii/utils/logger.h>
#include <eii/utils/json_config.h>
#include "eii/vi/realsense_ingestor.h"

using namespace eii::vi;
using namespace eii::utils;
using namespace eii::udf;

#define SERIAL "serial"
#define IMU "imu_on"
#define UUID_LENGTH 5

RealSenseIngestor::RealSenseIngestor(config_t* config, FrameQueue* frame_queue, std::string service_name, std::condition_variable& snapshot_cv, EncodeType enc_type, int enc_lvl):
    Ingestor(config, frame_queue, service_name, snapshot_cv, enc_type, enc_lvl) {
    m_encoding = false;
    m_initialized.store(true);
    m_imu_on = false;
    m_imu_support = false;

    const auto dev_list = m_ctx.query_devices();
    if(dev_list.size() == 0) {
        const char* err = "No RealSense devices found";
        LOG_ERROR("%s", err);
        throw(err);
    }

    std::string first_serial = std::string(dev_list[0].get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));

    config_value_t* cvt_serial = config->get_config_value(config->cfg, SERIAL);
    if(cvt_serial == NULL) {
        LOG_DEBUG_0("\"serial\" key is not added, first connected device in the list will be enabled");
        // Get the serial number of the first connected device in the list
        m_serial = first_serial;
    } else {
        LOG_DEBUG_0("cvt_serial initialized");
        if(cvt_serial->type != CVT_STRING) {
            config_value_destroy(cvt_serial);
            const char* err = "JSON value must be a string";
            LOG_ERROR("%s for \'%s\'", err, SERIAL);
            throw(err);
        }

        m_serial = std::string(cvt_serial->body.string);
        LOG_DEBUG("Device Serial: %s", m_serial.c_str());
        config_value_destroy(cvt_serial);

        if(dev_list.size() == 1 && m_serial != first_serial) {
            const char* err = "Input serial not matching with RealSence Device Serial";
            LOG_ERROR("%s", err);
            throw(err);
        }

        // TODO: Verify with multi cam scenario
        // When multiple devices are connected
        /*if (dev_list.size() > 1) {
            std::vector<std::string> serials;
            for (auto&& dev : m_ctx.query_devices())
                serials.push_back(dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));
            for (auto&& s : serials)
            {
                if(m_serial == s) {
                    LOG_DEBUG_0("Input serial matches RealSense Device Serial");
                } else {
                    const char* err = "Input serial not matching with RealSence Device Serial";
                    LOG_ERROR("%s", err);
                    throw(err);
                }
            }
        }*/
    }

    // Enable streaming configuration
    m_cfg.enable_device(m_serial);
    m_cfg.enable_stream(RS2_STREAM_COLOR, RS2_FORMAT_RGB8);
    m_cfg.enable_stream(RS2_STREAM_DEPTH, RS2_FORMAT_Z16);

    //TODO: Verify pose stream from tracking camera
    //m_cfg.enable_stream(RS2_STREAM_POSE, RS2_FORMAT_6DOF);


    config_value_t* cvt_loop_video = config->get_config_value(
            config->cfg, IMU);
    if(cvt_loop_video != NULL) {
        if(cvt_loop_video->type != CVT_BOOLEAN) {
            LOG_ERROR_0("IMU must be a boolean");
            config_value_destroy(cvt_loop_video);
        }
        if(cvt_loop_video->body.boolean) {
            m_imu_on = true;
        }
        config_value_destroy(cvt_loop_video);
    }

    if(m_imu_on) {
        // Enable streams to get IMU data
        if (!check_imu_is_supported()) {
            LOG_DEBUG_0("Device supporting IMU not found");
        } else {
            LOG_DEBUG_0("Device supports IMU");
            m_cfg.enable_stream(RS2_STREAM_ACCEL, RS2_FORMAT_MOTION_XYZ32F);
            m_cfg.enable_stream(RS2_STREAM_GYRO, RS2_FORMAT_MOTION_XYZ32F);
            m_imu_support = true;
        }
    }

    // Start streaming with enabled configuration
    m_pipe.start(m_cfg);

    }

RealSenseIngestor::~RealSenseIngestor() {
    LOG_DEBUG_0("RealSense ingestor destructor");
    m_pipe.stop();
}

void free_rs2_frame(void* obj) {
    // This may not be needed as the frame memory
    // is internally handled.
    //LOG_DEBUG_0("Freeing RealSense frame");
    //rs2_frame* frame = (rs2_frame*) obj;
    //delete frame;

}

void RealSenseIngestor::run(bool snapshot_mode) {
    // indicate that the run() function corresponding to the m_th thread has started
    m_running.store(true);
    LOG_INFO_0("Ingestor thread running publishing on stream");

    Frame* frame = NULL;

    int64_t frame_count = 0;

    msg_envelope_elem_body_t* elem = NULL;

    try {
        while (!m_stop.load()) {
            this->read(frame);

            // Adding image handle to frame
            std::string randuuid = generate_image_handle(UUID_LENGTH);
            msg_envelope_t* meta_data = frame->get_meta_data();
            // Profiling start
            DO_PROFILING(this->m_profile, meta_data, "ts_Ingestor_entry")

            // Profiling end

            msgbus_ret_t ret;
            if(frame_count == INT64_MAX) {
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
            if(ret != MSG_SUCCESS) {
                delete frame;
                const char* err = "Failed to put frame_number in meta-data";
                LOG_ERROR("%s", err);
                throw err;
            }
            elem = NULL;
            LOG_DEBUG("Frame number: %ld", frame_count);

            elem = msgbus_msg_envelope_new_string(randuuid.c_str());
            if (elem == NULL) {
                delete frame;
                const char* err = "Failed to create image handle element";
                LOG_ERROR("%s", err);
                throw err;
            }
            ret = msgbus_msg_envelope_put(meta_data, "img_handle", elem);
            if(ret != MSG_SUCCESS) {
                delete frame;
                const char* err = "Failed to put image handle in meta-data";
                LOG_ERROR("%s", err);
                throw err;
            }
            elem = NULL;

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
            if(ret_queue == QueueRetCode::QUEUE_FULL) {
                if(m_udf_input_queue->push_wait(frame) != QueueRetCode::SUCCESS) {
                    LOG_ERROR_0("Failed to enqueue message, "
                                "message dropped");
                }
                // Add timestamp which acts as a marker if queue if blocked
                DO_PROFILING(this->m_profile, meta_data, m_ingestor_block_key.c_str());
            }

            frame = NULL;

            if(snapshot_mode) {
                m_stop.store(true);
                m_snapshot_cv.notify_all();
            }
        }
    } catch(const char* e) {
        LOG_ERROR("Exception: %s", e);
        if (elem != NULL)
            msgbus_msg_envelope_elem_destroy(elem);
        if(frame != NULL)
            delete frame;
        throw e;
    } catch (const rs2::error & e) {
        LOG_ERROR("RealSense error calling %s( %s ):'\n%s",
                  e.get_failed_function(), e.get_failed_args(), e.what());
        if (elem != NULL)
            msgbus_msg_envelope_elem_destroy(elem);
        if(frame != NULL)
            delete frame;
        throw e;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception: %s", e.what());
        if (elem != NULL)
            msgbus_msg_envelope_elem_destroy(elem);
        if(frame != NULL)
            delete frame;
        throw e;
    } catch(...) {
        LOG_ERROR("Exception occured in opencv ingestor run()");
        if (elem != NULL)
            msgbus_msg_envelope_elem_destroy(elem);
        if(frame != NULL)
            delete frame;
        throw;
    }
    if (elem != NULL)
        msgbus_msg_envelope_elem_destroy(elem);
    if(frame != NULL)
        delete frame;
    LOG_INFO_0("Ingestor thread stopped");
    if(snapshot_mode)
        m_running.store(false);
}

void RealSenseIngestor::read(Frame*& frame) {

    LOG_DEBUG_0("Reading set of frames from RealSense camera");

    // Wait for next set of frames from the camera
    rs2::frameset data = m_pipe.wait_for_frames();

    // Retrieve the first color frame
    rs2::video_frame color = data.get_color_frame();

    // Retrieve the first depth frame
    //rs2::depth_frame depth = data.get_depth_frame();

    // Query frame width and height
    const int color_width = color.get_width();
    const int color_height = color.get_height();
    //const int depth_width = depth.get_width();
    //const int depth_height = depth.get_height();

    frame = new Frame(
            (void*) color.get(), color_width , color_height,
            3, (void*)color.get_data(), free_rs2_frame);

    if (m_imu_support) {
        // Add IMU data to frame metadata
        msgbus_ret_t ret = MSG_SUCCESS;

        msg_envelope_t* rs2_imu_meta_data = frame->get_meta_data();

        msg_envelope_elem_body_t* rs2_meta_arr = msgbus_msg_envelope_new_array();
        if(rs2_meta_arr == NULL) {
            LOG_ERROR_0("Failed to initialize rs2 metadata");
            delete frame;
        }

        // Find and retrieve IMU and/or tracking data
        if (rs2::motion_frame accel_frame = data.first_or_default(RS2_STREAM_ACCEL))
        {
            rs2_vector accel_sample = accel_frame.get_motion_data();
            LOG_DEBUG("Accel_Sample: x:%f, y:%f, z:%f", accel_sample.x, accel_sample.y, accel_sample.z);

            msg_envelope_elem_body_t* accel_obj= msgbus_msg_envelope_new_object();
            if(accel_obj == NULL) {
                LOG_ERROR_0("Failed to initialize accel metadata");
                delete frame;
            }

            msg_envelope_elem_body_t* accel_x = msgbus_msg_envelope_new_floating(accel_sample.x);
            if(accel_x == NULL) {
                LOG_ERROR_0("Failed to initialize accel sample x-coordinate metadata");
                delete frame;
            }
            ret = msgbus_msg_envelope_elem_object_put(accel_obj, "accel_sample_x", accel_x);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to put accel sample x-coordinate metadata");
                delete frame;
            }

            msg_envelope_elem_body_t* accel_y = msgbus_msg_envelope_new_floating(accel_sample.y);
            if(accel_y == NULL) {
                LOG_ERROR_0("Failed to initialize accel sample y-coordinate metadata");
                delete frame;
            }
            ret = msgbus_msg_envelope_elem_object_put(accel_obj, "accel_sample_y", accel_y);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to put accel sample y-coordinate metadata");
                delete frame;
            }

            msg_envelope_elem_body_t* accel_z = msgbus_msg_envelope_new_floating(accel_sample.z);
            if(accel_z == NULL) {
                LOG_ERROR_0("Failed to initialize accel sample z-coordinate metadata");
                delete frame;
            }
            ret = msgbus_msg_envelope_elem_object_put(accel_obj, "accel_sample_z", accel_z);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to put accel sample z-coordinate metadata");
                delete frame;
            }

            ret = msgbus_msg_envelope_elem_array_add(rs2_meta_arr, accel_obj);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to add accel object to rs2 metadata");
                delete frame;
            }

          }

        if (rs2::motion_frame gyro_frame = data.first_or_default(RS2_STREAM_GYRO))
        {
            rs2_vector gyro_sample = gyro_frame.get_motion_data();
            LOG_DEBUG("Gyro Sample: x:%f, y:%f, z:%f", gyro_sample.x, gyro_sample.y, gyro_sample.z);

            msg_envelope_elem_body_t* gyro_obj= msgbus_msg_envelope_new_object();
            if(gyro_obj == NULL) {
                LOG_ERROR_0("Failed to initialize gyro metadata");
                delete frame;
            }

            msg_envelope_elem_body_t* gyro_x = msgbus_msg_envelope_new_floating(gyro_sample.x);
            if(gyro_x == NULL) {
                LOG_ERROR_0("Failed to initialize gyro sample x-coordinate metadata");
                delete frame;
            }
            ret = msgbus_msg_envelope_elem_object_put(gyro_obj, "gyro_sample_x", gyro_x);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to put gyro sample x-coordinate metadata");
                delete frame;
            }

            msg_envelope_elem_body_t* gyro_y = msgbus_msg_envelope_new_floating(gyro_sample.y);
            if(gyro_y == NULL) {
                LOG_ERROR_0("Failed to initialize gyro sample y-coordinate metadata");
                delete frame;
            }
            ret = msgbus_msg_envelope_elem_object_put(gyro_obj, "gyro_sample_y", gyro_y);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to put gyro sample y-coordinate metadata");
                delete frame;
            }

            msg_envelope_elem_body_t* gyro_z = msgbus_msg_envelope_new_floating(gyro_sample.z);
            if(gyro_z == NULL) {
                LOG_ERROR_0("Failed to initialize gyro sample z-coordinate metadata");
                delete frame;
            }
            ret = msgbus_msg_envelope_elem_object_put(gyro_obj, "gyro_sample_z", gyro_z);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to put gyro sample z-coordinate metadata");
                delete frame;
            }

            ret = msgbus_msg_envelope_elem_array_add(rs2_meta_arr, gyro_obj);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to add gyro object to rs2 metadata");
                delete frame;
            }
        }

        //TODO: Verify pose sample from tracking camera
        /*/if (rs2::pose_frame pose_frame = data.first_or_default(RS2_STREAM_POSE))
        {
            rs2_pose pose_sample = pose_frame.get_pose_data();
            LOG_DEBUG("Pose Sample x:%f,y:%f,z:%f", pose_sample.translation.x, pose_sample.translation.y, pose_sample.translation.z);


            msg_envelope_elem_body_t* pose_obj= msgbus_msg_envelope_new_object();
            if(pose_obj == NULL) {
                LOG_ERROR_0("Failed to initialize pose metadata");
                delete frame;
            }

            msg_envelope_elem_body_t* pose_x = msgbus_msg_envelope_new_floating(pose_sample.translation.x);
            if(accel_x == NULL) {
                LOG_ERROR_0("Failed to initialize pose sample x-coordinate metadata");
                delete frame;
            }
            ret = msgbus_msg_envelope_elem_object_put(pose_obj, "pose_sample_x", pose_x);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to put pose sample x-coordinate metadata");
                delete frame;
            }


            msg_envelope_elem_body_t* pose_y = msgbus_msg_envelope_new_floating(pose_sample.translation.y);
            if(pose_y == NULL) {
                LOG_ERROR_0("Failed to initialize pose sample y-coordinate metadata");
                delete frame;
            }
            ret = msgbus_msg_envelope_elem_object_put(pose_obj, "pose_sample_y", pose_y);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to put pose sample y-coordinate metadata");
                delete frame;
            }

            msg_envelope_elem_body_t* pose_z = msgbus_msg_envelope_new_floating(pose_sample.translation.z);
            if(pose_z == NULL) {
                LOG_ERROR_0("Failed to initialize accel sample z-coordinate metadata");
                delete frame;
            }
            ret = msgbus_msg_envelope_elem_object_put(pose_obj, "pose_sample_z", pose_z);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to put pose sample z-coordinate metadata");
                delete frame;
            }

            ret = msgbus_msg_envelope_elem_array_add(rs2_meta_arr, pose_obj);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to add pose object to rs2 metadata");
                delete frame;
            }
        }*/

        ret = msgbus_msg_envelope_put(rs2_imu_meta_data, "rs2_imu_meta_data", rs2_meta_arr);
        if(ret != MSG_SUCCESS) {
            LOG_ERROR_0("Failed to put rs2 metadata");
            delete frame;
        }
    }

    if(m_poll_interval > 0) {
        usleep(m_poll_interval * 1000 * 1000);
    }
}

void RealSenseIngestor::stop() {
    if(m_initialized.load()) {
        if(!m_stop.load()) {
            m_stop.store(true);
            // wait for the ingestor thread run() to finish its execution.
            if(m_th != NULL) {
                m_th->join();
            }
        }
    // After run() has been stopped m_stop flag is reset,
    // so that the ingestor is ready for the next ingestion.
    m_running.store(false);
    m_stop.store(false);
    }
}

bool RealSenseIngestor::check_imu_is_supported()
{
    bool found_gyro = false;
    bool found_accel = false;
    for (auto dev : m_ctx.query_devices())
    {
        // The same device should support gyro and accel
        found_gyro = false;
        found_accel = false;
        for (auto sensor : dev.query_sensors())
        {
            for (auto profile : sensor.get_stream_profiles())
            {
                if (profile.stream_type() == RS2_STREAM_GYRO)
                    found_gyro = true;

                if (profile.stream_type() == RS2_STREAM_ACCEL)
                    found_accel = true;
            }
        }
        if (found_gyro && found_accel)
            break;
    }
    return found_gyro && found_accel;
}
