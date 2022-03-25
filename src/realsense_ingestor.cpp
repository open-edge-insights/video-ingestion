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

#include <unistd.h>
#include <eii/msgbus/msgbus.h>
#include <eii/utils/logger.h>
#include <eii/utils/json_config.h>
#include <string>
#include <vector>
#include <cerrno>
#include "eii/msgbus/msg_envelope.hpp"
#include "eii/vi/realsense_ingestor.h"

using namespace eii::vi;
using namespace eii::utils;
using namespace eii::udf;
using namespace eii::msgbus;

#define SERIAL "serial"
#define IMU "imu_on"
#define FRAMERATE "framerate"
#define UUID_LENGTH 5

RealSenseIngestor::RealSenseIngestor(config_t* config, FrameQueue* frame_queue,
                                     std::string service_name, std::condition_variable& snapshot_cv,
                                     EncodeType enc_type, int enc_lvl):
    Ingestor(config, frame_queue, service_name, snapshot_cv, enc_type, enc_lvl) {
    m_encoding = false;
    m_initialized.store(true);
    m_imu_on = false;
    m_imu_support = false;
    m_framerate = 0;

    const auto dev_list = m_ctx.query_devices();
    if (dev_list.size() == 0) {
        const char* err = "No RealSense devices found";
        LOG_ERROR("%s", err);
        throw(err);
    }

    std::string first_serial = std::string(dev_list[0].get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));

    config_value_t* cvt_serial = config->get_config_value(config->cfg, SERIAL);
    if (cvt_serial == NULL) {
        LOG_DEBUG_0("\"serial\" key is not added, first connected device in the list will be enabled");
        // Get the serial number of the first connected device in the list
        m_serial = first_serial;
    } else {
        LOG_DEBUG_0("cvt_serial initialized");
        if (cvt_serial->type != CVT_STRING) {
            config_value_destroy(cvt_serial);
            const char* err = "JSON value must be a string";
            LOG_ERROR("%s for \'%s\'", err, SERIAL);
            throw(err);
        }

        m_serial = std::string(cvt_serial->body.string);
        config_value_destroy(cvt_serial);

        // Verify serial number when single device is connected
        if (dev_list.size() == 1 && m_serial != first_serial) {
            const char* err = "Input serial not matching with RealSence Device Serial";
            LOG_ERROR("%s", err);
            throw(err);
        }

        // Verify serial number when multiple devices are connected
        if (dev_list.size() > 1) {
            bool serial_found = false;
            for (uint16_t i = 0; i <= dev_list.size(); i++) {
                if (m_serial == std::string(dev_list[i].get_info(RS2_CAMERA_INFO_SERIAL_NUMBER))) {
                    serial_found = true;
                    break;
                }
            }

            if (!serial_found) {
                const char* err = "Input serial not matching with RealSence Device Serial";
                LOG_ERROR("%s", err);
                throw(err);
            }
        }
    }
    LOG_INFO("Device Serial: %s", m_serial.c_str());

    // Get framerate config value
    config_value_t* cvt_framerate = config->get_config_value(config->cfg, FRAMERATE);
    if (cvt_framerate != NULL) {
        if (cvt_framerate->type != CVT_INTEGER) {
            const char* err = "framerate must be an integer";
            LOG_ERROR("%s", err);
            config_value_destroy(cvt_framerate);
            throw(err);
        }
        m_framerate = cvt_framerate->body.integer;
        config_value_destroy(cvt_framerate);
    } else if (cvt_framerate == NULL) {
        m_framerate = 30;
    }
    LOG_INFO("Framerate: %d", m_framerate);

    // Enable streaming configuration
    m_cfg.enable_device(m_serial);
    m_cfg.enable_stream(RS2_STREAM_COLOR, RS2_FORMAT_BGR8, m_framerate);
    m_cfg.enable_stream(RS2_STREAM_DEPTH, RS2_FORMAT_Z16, m_framerate);

    // TODO: Verify pose stream from tracking camera

    config_value_t* cvt_imu = config->get_config_value(
            config->cfg, IMU);
    if (cvt_imu != NULL) {
        if (cvt_imu->type != CVT_BOOLEAN) {
            LOG_ERROR_0("IMU must be a boolean");
            config_value_destroy(cvt_imu);
        }
        if (cvt_imu->body.boolean) {
            m_imu_on = true;
        }
        config_value_destroy(cvt_imu);
    }

    if (m_imu_on) {
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
    } catch(const char* e) {
        LOG_ERROR("Exception: %s", e);
        if (elem != NULL)
            msgbus_msg_envelope_elem_destroy(elem);
        if (frame != NULL)
            delete frame;
        throw e;
    } catch (const rs2::error & e) {
        LOG_ERROR("RealSense error calling %s( %s ): %s",
                  e.get_failed_function().c_str(),
                  e.get_failed_args().c_str(), e.what());
        if (elem != NULL)
            msgbus_msg_envelope_elem_destroy(elem);
        if (frame != NULL)
            delete frame;
        throw e;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception: %s", e.what());
        if (elem != NULL)
            msgbus_msg_envelope_elem_destroy(elem);
        if (frame != NULL)
            delete frame;
        throw e;
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

void RealSenseIngestor::read(Frame*& frame) {

    LOG_DEBUG_0("Reading set of frames from RealSense camera");

    // Wait for next set of frames from the camera
    rs2::frameset data = m_pipe.wait_for_frames();

    // Retrieve the first color frame
    rs2::video_frame color = data.get_color_frame();

    // Retrieve the first depth frame
    rs2::depth_frame depth = data.get_depth_frame();

    // Query frame width and height
    const int color_width = color.get_width();
    const int color_height = color.get_height();
    const int depth_width = depth.get_width();
    const int depth_height = depth.get_height();

    frame = new Frame(
            (void*) color.get(), free_rs2_frame, (void*) color.get_data(),
            color_width , color_height, 3);
    frame->add_frame((void*) depth.get(), free_rs2_frame, (void*) depth.get_data(),
            depth_width, depth_height, 3, EncodeType::NONE, 0);

    auto depth_profile = depth.get_profile().as<rs2::video_stream_profile>();

    auto depth_intrinsics = depth_profile.get_intrinsics();

    msg_envelope_t* rs2_meta = frame->get_meta_data();

    MsgEnvelope* msgEnv = NULL;
    MsgEnvelope* msgEnvImu = NULL;
    MsgEnvelopeList* rs2_meta_arr = NULL;
    MsgEnvelopeObject* accel_obj = NULL;
    MsgEnvelopeObject* gyro_obj = NULL;

    try {
        msgEnv = new MsgEnvelope(rs2_meta);

        msgEnv->put_integer("rs2_depth_intrinsics_width", depth_intrinsics.width);
        msgEnv->put_integer("rs2_depth_intrinsics_height", depth_intrinsics.height);
        msgEnv->put_integer("rs2_depth_intrinsics_model", (int)depth_intrinsics.model);

        msgEnv->put_float("rs2_depth_intrinsics_ppx", depth_intrinsics.ppx);
        msgEnv->put_float("rs2_depth_intrinsics_ppy", depth_intrinsics.ppy);
        msgEnv->put_float("rs2_depth_intrinsics_fx", depth_intrinsics.fx);
        msgEnv->put_float("rs2_depth_intrinsics_fy", depth_intrinsics.fy);

        auto color_profile = color.get_profile().as<rs2::video_stream_profile>();

        auto color_intrinsics = color_profile.get_intrinsics();

        msgEnv->put_integer("rs2_color_intrinsics_width", color_intrinsics.width);
        msgEnv->put_integer("rs2_color_intrinsics_height", color_intrinsics.height);
        msgEnv->put_integer("rs2_color_intrinsics_model", (int)color_intrinsics.model);
        msgEnv->put_float("rs2_color_intrinsics_ppx", color_intrinsics.ppx);
        msgEnv->put_float("rs2_color_intrinsics_ppy", color_intrinsics.ppy);
        msgEnv->put_float("rs2_color_intrinsics_fx", color_intrinsics.fx);
        msgEnv->put_float("rs2_color_intrinsics_fy", color_intrinsics.fy);

        auto depth_to_color_extrinsics = depth_profile.get_extrinsics_to(color_profile);

        // Add rs2 extrinsics rotation
        std::vector<double> rotation_array;
        for (int i = 0; i < 9; i++) {
            rotation_array.push_back(depth_to_color_extrinsics.rotation[i]);
        }
        msgEnv->put_vector("rotation_arr", rotation_array);

        std::vector<double> translation_array;
        for (int i = 0; i < 3; i++) {
            translation_array.push_back(depth_to_color_extrinsics.rotation[i]);
        }
        msgEnv->put_vector("translation_arr", translation_array);

        if (m_imu_support) {

            msgEnvImu = new MsgEnvelope(rs2_meta);

            // Add IMU data to frame metadata
            rs2_meta_arr = new MsgEnvelopeList();

            // Find and retrieve IMU and/or tracking data
            if (rs2::motion_frame accel_frame = data.first_or_default(RS2_STREAM_ACCEL)) {
                rs2_vector accel_sample = accel_frame.get_motion_data();
                LOG_DEBUG("Accel_Sample: x:%f, y:%f, z:%f", accel_sample.x, accel_sample.y, accel_sample.z);

                accel_obj = new MsgEnvelopeObject();
                accel_obj->put_float("accel_sample_x", accel_sample.x);
                accel_obj->put_float("accel_sample_y", accel_sample.y);
                accel_obj->put_float("accel_sample_z", accel_sample.z);
                rs2_meta_arr->put_object(accel_obj);
            }

            if (rs2::motion_frame gyro_frame = data.first_or_default(RS2_STREAM_GYRO)) {
                rs2_vector gyro_sample = gyro_frame.get_motion_data();
                LOG_DEBUG("Gyro Sample: x:%f, y:%f, z:%f", gyro_sample.x, gyro_sample.y, gyro_sample.z);

                gyro_obj = new MsgEnvelopeObject();
                gyro_obj->put_float("gyro_sample_x", gyro_sample.x);
                gyro_obj->put_float("gyro_sample_y", gyro_sample.y);
                gyro_obj->put_float("gyro_sample_z", gyro_sample.z);
                rs2_meta_arr->put_object(gyro_obj);
            }

            // TODO: Verify pose sample from tracking camera
            msgEnvImu->put_array("rs2_imu_meta_data", rs2_meta_arr);
        }
    } catch (const std::exception& MsgbusException) {
        LOG_ERROR("Error in RealSense ingestor %s", MsgbusException.what());
        delete msgEnv;
        delete msgEnvImu;
        delete rs2_meta_arr;
        delete accel_obj;
        delete gyro_obj;
    }

    if (m_poll_interval > 0) {
        LOG_WARN("poll_interval not supported in realsense ingestor please use framerate config");
    }
}

void RealSenseIngestor::stop() {
    if (m_initialized.load()) {
        if (!m_stop.load()) {
            m_stop.store(true);
            // wait for the ingestor thread run() to finish its execution.
            if (m_th != NULL) {
                m_th->join();
            }
        }
    // After run() has been stopped m_stop flag is reset,
    // so that the ingestor is ready for the next ingestion.
    m_running.store(false);
    m_stop.store(false);
    }
}

bool RealSenseIngestor::check_imu_is_supported() {
    bool found_gyro = false;
    bool found_accel = false;
    for (auto dev : m_ctx.query_devices()) {
        // The same device should support gyro and accel
        found_gyro = false;
        found_accel = false;
        for (auto sensor : dev.query_sensors()) {
            for (auto profile : sensor.get_stream_profiles()) {
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
