/*
 * GStreamer Generic Camera Plugin
 * Copyright (c) 2020, Intel Corporation
 * All rights reserved.
 *
 * Authors:
 *   Gowtham Hosamane <gowtham.hosamane@intel.com>
 *   Smitesh Sutaria <smitesh.sutaria@intel.com>
 *   Deval Vekaria <deval.vekaria@intel.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _GEN_I_CAM_H_
#define _GEN_I_CAM_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video-format.h>

#include <gencambase.h>

//---------------------------- includes for streaming -----------------------------
#include "genicam-core/rc_genicam_api/buffer.h"
#include "genicam-core/rc_genicam_api/config.h"
#include "genicam-core/rc_genicam_api/device.h"
#include "genicam-core/rc_genicam_api/image.h"
#include "genicam-core/rc_genicam_api/image_store.h"
#include "genicam-core/rc_genicam_api/interface.h"
#include "genicam-core/rc_genicam_api/stream.h"
#include "genicam-core/rc_genicam_api/system.h"
#include "genicam-core/rc_genicam_api/pixel_formats.h"

#include <Base/GCException.h>

#include <signal.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
// ------------------------------------------------------------------------------

// TODO : Can be removed later
// This basic RANDOM_RGB_TEST can be kept while in developing state.
// Enable the below macro for dummy data test. undefine it for actual camera
// data.
//#define RANDOM_RGB_TEST

#define EXTERNC extern "C"

#define ROUNDED_DOWN(val, align)        ((val) & ~((align)))
#define ROUNDED_UP(  val, align)        ROUNDED_DOWN((val) + (align) - 1, (align))

class Genicam
{
public:

  /*
   * Initializes the pointer data member to parameter structure

   @param params       Pointer to gencamParams structure to initialize
   @return             True if data member is assigned from the pointer
   */
  bool Init (GencamParams * params);

  /*
   * Opens the camera device, configures the camera with the defined parameters
   and starts the streaming

   @return             True if width, height and pixel format are successfully
   configured. False otherwise. True is returned
   irrespective of other configurations success or fail.
   */
  bool Start (void);

  /*
   * Stops the streaming and closes the device

   @return             True after stopping the streaming and closing the device
   */
  bool Stop (void);

  /*
   * Creates the buffer corresponding to a frame to be pushed in the pipeline

   @param buf          Double pointer GstBuffer structure to allocate the
   buffer and copy the frame data
   @param mapInfo      Pointer to the structure that contains buffer
   information
   @return             True after stopping the streaming and closing the device
   */
  void Create (GstBuffer ** buf, GstMapInfo * mapInfo);

private:
  /* Pointer to gencamParams structure */
    GencamParams * gencamParams;

  /* Shared pointer to device object */
    std::shared_ptr < rcg::Device > dev;

  /* Shared pointer to stream object */
    std::vector < std::shared_ptr < rcg::Stream >> stream;

  /* Shared pointer to nodemap */
    std::shared_ptr < GenApi::CNodeMapRef > nodemap;

  /* Resets the device to factory power up state */
  bool resetDevice (void);

  /* Sets binning selector feature */
  bool setBinningSelector (void);

  /* Sets binning horizontal mode feature */
  bool setBinningHorizontalMode (void);

  /* Sets binning horizontal feature */
  bool setBinningHorizontal (void);

  /* Sets binning vertical mode feature */
  bool setBinningVerticalMode (void);

  /* Sets binning vertical feature */
  bool setBinningVertical (void);

  /* Sets width and height */
  bool setWidthHeight (void);

  /* Sets offset-x and offset-y features */
  bool setOffsetXY (void);

  /* Sets acquisition frame rate */
  bool setAcquisitionFrameRate (void);

  /* Sets pixel format */
  bool setPixelFormat (void);

  /* Sets exposure mode */
  bool setExposureMode (void);

  /* Sets exposure time if exposure mode is timed */
  bool setExposureTime (void);

  /* Sets exposure auto */
  bool setExposureAuto (void);

  /* Sets exposure time selector */
  bool setExposureTimeSelector (void);
};

#endif
