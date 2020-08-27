/*
 * GStreamer Generic Camera Plugin
 * Copyright (c) 2020, Intel Corporation
 * All rights reserved.
 *
 * Authors:
 *   Gowtham Hosamane <gowtham.hosamane@intel.com>
 *   Smitesh Sutaria <smitesh.sutaria@intel.com>
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

#ifndef _GEN_CAM_BASE_H_
#define _GEN_CAM_BASE_H_

#include <stdbool.h>

#include <gst/gst.h>
#include <gst/video/video-format.h>

// TODO : Can be removed later
// This basic RANDOM_RGB_TEST can be kept while in developing state.
// Enable the below macro for dummy data test. undefine it for actual camera
// data.
//#define RANDOM_RGB_TEST


#define EXTERNC extern "C"

#ifdef __cplusplus
extern "C"
{
#endif

  /* Parameters structure for configuration by the user */
  typedef struct _GencamParams
  {
    char *deviceSerialNumber;   /* Identify the device to stream from */
    char *pixelFormat;          /* Format of the pixels from camera */
    char *binningSelector;      /* Binning engine controlled by
                                   binning horizontal and binning vertical */
    char *binningHorizontalMode;        /* Mode to combine horizontal
                                           photo-sensitive cells */
    char *binningVerticalMode;  /* Mode to combine vertical
                                   photo-sensitive cells */
    char *exposureAuto;		/* Set exposure type */
    char *exposureTimeSelector; /* Exposure related operations */
    char *exposureMode;         /* Operation mode of exposure */
    int binningHorizontal;      /* Number of horizontal photo-sensitive
                                   cells to combine */
    int binningVertical;        /* Number of vertical photo-sensitive
                                   cells to combine */
    int width;
    int height;
    int widthMax;               // Not Configured TODO: Remove this
    int heightMax;              // Not Configured TODO: Remove this
    int offsetX;
    int offsetY;
    float exposureTime;
    float acquisitionFrameRate; // Controls the acquisition rate
    bool deviceReset;           // Resets the device to factory state
  } GencamParams;

  /* Initialize generic camera base class */
  bool gencamsrc_init (GencamParams *);

  /* Open the camera device and connect */
  bool gencamsrc_start (void);

  /* Close the device */
  bool gencamsrc_stop (void);

  /* Receive the frame to create output buffer */
  void gencamsrc_create (GstBuffer ** buf, GstMapInfo * mapInfo);
#ifdef __cplusplus
}
#endif

#endif
