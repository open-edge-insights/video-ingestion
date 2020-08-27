/*
 * GStreamer Generic Camera Plugin
 * Copyright (c) 2020, Intel Corporation
 * All rights reserved.
 *
 * Author: Gowtham Hosamane <gowtham.hosamane@intel.com>
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

/**
 * SECTION:element-gstgencamsrc
 *
 * The gencamsrc element streams video from GenICam compliant
 * industrial machine vision camera.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 gencamsrc serial=<deviceSerialNumber> pixel-format=mono8 !
 * videoconvert ! ximagesink
 * gst-launch-1.0 gencamsrc serial=<deviceSerialNumber> ! bayer2rgb !
 * ximagesink
 * ]|
 * This is an example pipeline to stream from GenICam camera pushing to
 * to ximagesink with a color space converter in between
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video-format.h>

#include "gencambase.h"
#include "gstgencamsrc.h"

#define WIDTH (1280)
#define HEIGHT (960)

GST_DEBUG_CATEGORY_STATIC (gst_gencamsrc_debug_category);
#define GST_CAT_DEFAULT gst_gencamsrc_debug_category

/* prototypes */
static void
gst_gencamsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_gencamsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);
static void gst_gencamsrc_dispose (GObject * object);
static void gst_gencamsrc_finalize (GObject * object);

static GstCaps *gst_gencamsrc_get_caps (GstBaseSrc * src, GstCaps * filter);
// static gboolean gst_gencamsrc_negotiate (GstBaseSrc * src);
// static GstCaps *gst_gencamsrc_fixate (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_gencamsrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_gencamsrc_start (GstBaseSrc * src);
static gboolean gst_gencamsrc_stop (GstBaseSrc * src);
static void
gst_gencamsrc_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
// static gboolean gst_gencamsrc_unlock (GstBaseSrc * src);
// static gboolean gst_gencamsrc_unlock_stop (GstBaseSrc * src);
// static gboolean gst_gencamsrc_query (GstBaseSrc * src, GstQuery * query);
// static gboolean gst_gencamsrc_event (GstBaseSrc * src, GstEvent * event);
static GstFlowReturn gst_gencamsrc_create (GstPushSrc * src, GstBuffer ** buf);

enum
{
  PROP_0,
  PROP_SERIAL,
  PROP_PIXELFORMAT,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_OFFSETX,
  PROP_OFFSETY,
  PROP_BINNINGSELECTOR,
  PROP_BINNINGHORIZONTALMODE,
  PROP_BINNINGVERTICALMODE,
  PROP_BINNINGHORIZONTAL,
  PROP_BINNINGVERTICAL,
  PROP_EXPOSUREMODE,
  PROP_EXPOSURETIME,
  PROP_EXPOSUREAUTO,
  PROP_EXPOSURETIMESELECTOR,
  PROP_FRAMERATE,
  PROP_RESET
};

/* pad templates */

#define GCS_FORMATS_SUPPORTED "{ BGR, RGB, I420, YUY2, GRAY8 }"

#define GCS_CAPS                                                               \
  GST_VIDEO_CAPS_MAKE(GCS_FORMATS_SUPPORTED)                                   \
  ","                                                                          \
  "multiview-mode = { mono, left, right }"                                     \
  ";"                                                                          \
  "video/x-bayer, format=(string) { bggr, rggb, grbg, gbrg }, "                \
  "width = " GST_VIDEO_SIZE_RANGE ", "                                         \
  "height = " GST_VIDEO_SIZE_RANGE ", "                                        \
  "framerate = " GST_VIDEO_FPS_RANGE ", "                                      \
  "multiview-mode = { mono, left, right }"

static GstStaticPadTemplate
    gst_gencamsrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GCS_CAPS));

/* class initialization */

#define gst_gencamsrc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGencamsrc, gst_gencamsrc, GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (gst_gencamsrc_debug_category, "gencamsrc", 0,
        "debug category for gencamsrc element"));

static void
gst_gencamsrc_class_init (GstGencamsrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_gencamsrc_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Intel Generic Camera Src Plugin", "Source/Video/Camera",
      "Intel Generic Camera Source Plugin",
      "Gowtham Hosamane <gowtham.hosamane@intel.com>");

  gobject_class->set_property = gst_gencamsrc_set_property;
  gobject_class->get_property = gst_gencamsrc_get_property;
  gobject_class->dispose = gst_gencamsrc_dispose;
  gobject_class->finalize = gst_gencamsrc_finalize;

  // TODO cleanup as some of the following are not required
  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_gencamsrc_get_caps);
  // base_src_class->negotiate = GST_DEBUG_FUNCPTR (gst_gencamsrc_negotiate);
  // base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_gencamsrc_fixate);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_gencamsrc_set_caps);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_gencamsrc_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_gencamsrc_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_gencamsrc_get_times);
  // base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_gencamsrc_unlock);
  // base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_gencamsrc_unlock_stop);
  // base_src_class->query = GST_DEBUG_FUNCPTR (gst_gencamsrc_query);
  // base_src_class->event = GST_DEBUG_FUNCPTR (gst_gencamsrc_event);

  // Following are virtual overridden by push src
  push_src_class->create = GST_DEBUG_FUNCPTR (gst_gencamsrc_create);

  // The property list
  g_object_class_install_property (gobject_class, PROP_SERIAL,
      g_param_spec_string ("serial", "DeviceSerialNumber",
          "Device's serial number. This string is a unique identifier of the device.",
          "", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_PIXELFORMAT,
      g_param_spec_string ("pixel-format", "PixelFormat",
          "Format of the pixels provided by the device. It represents all the information provided by PixelSize, PixelColorFilter combined in a single feature. Possible values (mono8/ycbcr411_8/ycbcr422_8/rgb8/bgr8/bayerbggr/bayerrggb/bayergrbg/bayergbrg)",
          "mono8", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_int ("width", "Width",
          "Width of the image provided by the device (in pixels).", 0 /*Min */ ,
          1920 /*Max */ , 1280 /*Default */ ,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_int ("height", "Height",
          "Height of the image provided by the device (in pixels).",
          0 /*Min */ , 1080 /*Max */ , 960 /*Default */ ,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_OFFSETX,
      g_param_spec_int ("offset-x", "OffsetX",
          "Horizontal offset from the origin to the region of interest (in pixels).",
          0 /*Min */ , 1920 /*Max */ , 0 /*Default */ ,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_OFFSETY,
      g_param_spec_int ("offset-y", "OffsetY",
          "Vertical offset from the origin to the region of interest (in pixels).",
          0 /*Min */ , 1080 /*Max */ , 0 /*Default */ ,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BINNINGSELECTOR,
      g_param_spec_string ("binning-selector", "BinningSelector",
          "Selects which binning engine is controlled by the BinningHorizontal and BinningVertical features. Possible values (sensor/region0/region1/region2)",
          "", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BINNINGHORIZONTALMODE,
      g_param_spec_string ("binning-horizontal-mode", "BinningHorizontalMode",
          "Sets the mode to use to combine horizontal photo-sensitive cells together when BinningHorizontal is used. Possible values (sum/average)",
          "", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BINNINGVERTICALMODE,
      g_param_spec_string ("binning-vertical-mode", "BinningVerticalMode",
          "Sets the mode to use to combine vertical photo-sensitive cells together when BinningHorizontal is used. Possible values (sum/average)",
          "", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BINNINGHORIZONTAL,
      g_param_spec_int ("binning-horizontal", "BinningHorizontal",
          "Number of horizontal photo-sensitive cells to combine together. This reduces the horizontal resolution (width) of the image. A value of 1 indicates that no horizontal binning is performed by the camera.",
          0 /*Min */ , 10 /*Max */ , 0 /*Default */ ,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BINNINGVERTICAL,
      g_param_spec_int ("binning-vertical", "BinningVertical",
          "Number of vertical photo-sensitive cells to combine together. This reduces the vertical resolution (height) of the image. A value of 1 indicates that no vertical binning is performed by the camera.",
          0 /*Min */ , 10 /*Max */ , 0 /*Default */ ,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_EXPOSUREMODE,
      g_param_spec_string ("exposure-mode", "ExposureMode",
          "Sets the operation mode of the Exposure. Possible values (off/timed/trigger-width/trigger-controlled)",
          "timed", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_EXPOSUREAUTO,
      g_param_spec_string ("exposure-auto", "ExposureAuto",
          "Sets the automatic exposure mode when ExposureMode is Timed. Possible values(off/once/continuous)",
          "Off", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_EXPOSURETIMESELECTOR,
      g_param_spec_string ("exposure-time-selector", "ExposureTimeMode",
          "Selects which exposure time is controlled by the ExposureTime feature. This allows for independent control over the exposure components. Possible values(common/red/green/stage1/...)",
          "", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_EXPOSURETIME,
      g_param_spec_float ("exposure-time", "ExposureTime",
          "Sets the Exposure time (in us) when ExposureMode is Timed and ExposureAuto is Off. This controls the duration where the photosensitive cells are exposed to light.",
          0 /*Min */ , 10000000 /*Max */ , 135896 /*uSec Default */ ,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FRAMERATE,
      g_param_spec_float ("frame-rate", "AcquisitionFrameRate",
          "Controls the acquisition rate (in Hertz) at which the frames are captured.",
          0 /*Min */ , 120 /*Max */ , 10 /*Default */ ,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_RESET,
      g_param_spec_boolean ("reset", "DeviceReset",
          "Resets the device to its power up state. After reset, the device must be rediscovered. Do not use unless absolutely required.",
          false /* Default */ ,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_gencamsrc_init (GstGencamsrc * gencamsrc)
{
  GencamParams *prop = &gencamsrc->properties;

  // Set following for live source
  gst_base_src_set_format (GST_BASE_SRC (gencamsrc), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (gencamsrc), TRUE);

  gst_base_src_set_do_timestamp (GST_BASE_SRC (gencamsrc), TRUE);

  // Initialize data members
  gencamsrc->frameNumber = 0;

  // Initialize plugin properties
  prop->deviceSerialNumber = NULL;
  prop->pixelFormat = "mono8\0";
  prop->width = WIDTH;
  prop->height = HEIGHT;
  prop->offsetX = 0;
  prop->offsetY = 0;
  prop->exposureMode = "Timed\0";
  prop->exposureAuto = "Off\0";
  prop->binningSelector = "";
  prop->binningHorizontalMode = "";
  prop->binningHorizontal = 0;
  prop->binningVerticalMode = "";
  prop->binningVertical = 0;
  prop->acquisitionFrameRate = 10;
  prop->deviceReset = false;

  // Initialize gencam base class and assign properties
  gencamsrc_init (&gencamsrc->properties);

  // TODO replace FIXME log with DEBUG log later
  GST_FIXME_OBJECT (gencamsrc, "The init function");
}

void
gst_gencamsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (object);
  GencamParams *prop = &gencamsrc->properties;

  GST_DEBUG_OBJECT (gencamsrc, "set_property");

  switch (property_id) {
    case PROP_SERIAL:
      prop->deviceSerialNumber = g_value_dup_string (value + '\0');
      break;
    case PROP_PIXELFORMAT:
      prop->pixelFormat = g_value_dup_string (value + '\0');
      break;
    case PROP_WIDTH:
      prop->width = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      prop->height = g_value_get_int (value);
      break;
    case PROP_OFFSETX:
      prop->offsetX = g_value_get_int (value);
      break;
    case PROP_OFFSETY:
      prop->offsetY = g_value_get_int (value);
      break;
    case PROP_BINNINGSELECTOR:
      prop->binningSelector = g_value_dup_string (value + '\0');
      break;
    case PROP_BINNINGHORIZONTALMODE:
      prop->binningHorizontalMode = g_value_dup_string (value + '\0');
      break;
    case PROP_BINNINGVERTICALMODE:
      prop->binningVerticalMode = g_value_dup_string (value + '\0');
      break;
    case PROP_BINNINGHORIZONTAL:
      prop->binningHorizontal = g_value_get_int (value);
      break;
    case PROP_BINNINGVERTICAL:
      prop->binningVertical = g_value_get_int (value);
      break;
    case PROP_EXPOSUREMODE:
      prop->exposureMode = g_value_dup_string (value + '\0');
      break;
    case PROP_EXPOSURETIME:
      prop->exposureTime = g_value_get_float (value);
      break;
    case PROP_EXPOSUREAUTO:
      prop->exposureAuto = g_value_dup_string (value + '\0');
      break;
    case PROP_EXPOSURETIMESELECTOR:
      prop->exposureTimeSelector = g_value_dup_string (value + '\0');
      break;
    case PROP_FRAMERATE:
      prop->acquisitionFrameRate = g_value_get_float (value);
      break;
    case PROP_RESET:
      prop->deviceReset = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_gencamsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (object);
  GencamParams *prop = &gencamsrc->properties;

  GST_DEBUG_OBJECT (gencamsrc, "get_property");

  switch (property_id) {
    case PROP_SERIAL:
      g_value_set_string (value, prop->deviceSerialNumber);
      break;
    case PROP_PIXELFORMAT:
      g_value_set_string (value, prop->pixelFormat);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, prop->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, prop->height);
      break;
    case PROP_OFFSETX:
      g_value_set_int (value, prop->offsetX);
      break;
    case PROP_OFFSETY:
      g_value_set_int (value, prop->offsetY);
      break;
    case PROP_BINNINGSELECTOR:
      g_value_set_string (value, prop->binningSelector);
      break;
    case PROP_BINNINGHORIZONTALMODE:
      g_value_set_string (value, prop->binningHorizontalMode);
      break;
    case PROP_BINNINGVERTICALMODE:
      g_value_set_string (value, prop->binningVerticalMode);
      break;
    case PROP_BINNINGHORIZONTAL:
      g_value_set_int (value, prop->binningHorizontal);
      break;
    case PROP_BINNINGVERTICAL:
      g_value_set_int (value, prop->binningVertical);
      break;
    case PROP_EXPOSUREMODE:
      g_value_set_string (value, prop->exposureMode);
      break;
    case PROP_EXPOSURETIME:
      g_value_set_float (value, prop->exposureTime);
      break;
    case PROP_EXPOSUREAUTO:
      g_value_set_string (value, prop->exposureAuto);
      break;
    case PROP_EXPOSURETIMESELECTOR:
      g_value_set_string (value, prop->exposureTimeSelector);
      break;
    case PROP_FRAMERATE:
      g_value_set_float (value, prop->acquisitionFrameRate);
      break;
    case PROP_RESET:
      g_value_set_boolean (value, prop->deviceReset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_gencamsrc_dispose (GObject * object)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (object);

  GST_DEBUG_OBJECT (gencamsrc, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_gencamsrc_parent_class)->dispose (object);
}

void
gst_gencamsrc_finalize (GObject * object)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (object);

  GST_DEBUG_OBJECT (gencamsrc, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_gencamsrc_parent_class)->finalize (object);
}

/* get caps from subclass */
static GstCaps *
gst_gencamsrc_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);
  GencamParams *prop = &gencamsrc->properties;

  GST_DEBUG_OBJECT (gencamsrc, "get_caps, src pad %" GST_PTR_FORMAT,
      src->srcpad);

  // if device is not opened, return base. Currently forcing to FALSE
  // TODO revisit this to return right capabilities
  if (FALSE /*TRUE*/) {
    return gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  }

  char *type = "";
  char *format = "";

  if (strcmp (prop->pixelFormat, "mono8") == 0) {
    type = "video/x-raw\0";
    format = "GRAY8\0";
  } else if (strcmp (prop->pixelFormat, "ycbcr411_8") == 0) {
    type = "video/x-raw\0";
    format = "I420\0";
  } else if (strcmp (prop->pixelFormat, "ycbcr422_8") == 0) {
    type = "video/x-raw\0";
    format = "YUY2\0";
  } else if (strcmp (prop->pixelFormat, "rgb8") == 0) {
    type = "video/x-raw\0";
    format = "RGB\0";
  } else if (strcmp (prop->pixelFormat, "bgr8") == 0) {
    type = "video/x-raw\0";
    format = "BGR\0";
  } else if (strcmp (prop->pixelFormat, "bayerbggr") == 0) {
    type = "video/x-bayer\0";
    format = "bggr\0";
  } else if (strcmp (prop->pixelFormat, "bayerrggb") == 0) {
    type = "video/x-bayer\0";
    format = "rggb\0";
  } else if (strcmp (prop->pixelFormat, "bayergrbg") == 0) {
    type = "video/x-bayer\0";
    format = "grbg\0";
  } else if (strcmp (prop->pixelFormat, "bayergbrg") == 0) {
    type = "video/x-bayer\0";
    format = "gbrg\0";
  } else {
    GST_WARNING_OBJECT (gencamsrc, "Unsupported format, defaulting to Mono8");
    free (prop->pixelFormat);
    prop->pixelFormat = "mono8\0";
    type = "video/x-raw\0";
    format = "GRAY8\0";
  }

  //If width or height not initliazed set it to WIDTH and HEIGHT respectively
  if (prop->width <= 0)
    prop->width = WIDTH;
  if (prop->height <= 0)
    prop->height = HEIGHT;

  GstCaps *caps = gst_caps_new_simple (type,
      "format", G_TYPE_STRING, format,
      "width", G_TYPE_INT, prop->width,
      "height", G_TYPE_INT, prop->height,
      "framerate", GST_TYPE_FRACTION, 120, 1,
      NULL);

  // TODO replace FIXME log with DEBUG log later
  GST_FIXME_OBJECT (gencamsrc, "The caps sent: %s, %s, %d x %d, variable fps.",
      type, format, prop->width, prop->height);

  return caps;
}

/* decide on caps */
/*static gboolean
gst_gencamsrc_negotiate (GstBaseSrc * src)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);

  GST_DEBUG_OBJECT (gencamsrc, "negotiate");

  return TRUE;
}*/

/* called if, in negotiation, caps need fixating */
/*static GstCaps *
gst_gencamsrc_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);
  GstStructure *structure;

  GST_DEBUG_OBJECT (gencamsrc, "fixate mainly for framerate");

  caps = gst_caps_make_writable (caps);
  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (structure, "width", 640);
  gst_structure_fixate_field_nearest_int (structure, "height", 480);

  if (gst_structure_has_field (structure, "framerate"))
    gst_structure_fixate_field_nearest_fraction (structure, "framerate", 30, 1);
  else
    gst_structure_set (structure, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (src, caps);
  return caps;

  //return NULL;
}*/

/* notify the subclass of new caps */
static gboolean
gst_gencamsrc_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);
  GstStructure *s = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (gencamsrc, "Setting caps to %" GST_PTR_FORMAT, caps);

  if (!g_str_equal ("video/x-bayer", gst_structure_get_name (s)) &&
      (!g_str_equal ("video/x-raw", gst_structure_get_name (s)) ||
          (!g_str_equal ("I420", gst_structure_get_string (s, "format")) &&
              !g_str_equal ("YUY2", gst_structure_get_string (s, "format")) &&
              !g_str_equal ("RGB", gst_structure_get_string (s, "format")) &&
              !g_str_equal ("BGR", gst_structure_get_string (s, "format")) &&
              !g_str_equal ("GRAY8", gst_structure_get_string (s,
                      "format"))))) {
    GST_ERROR_OBJECT (src, "unsupported caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_gencamsrc_start (GstBaseSrc * src)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);

  // TODO replace FIXME log with DEBUG log later
  GST_FIXME_OBJECT (gencamsrc, "camera open, set property and start");

  return gencamsrc_start ();
}

static gboolean
gst_gencamsrc_stop (GstBaseSrc * src)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);

  // TODO replace FIXME log with DEBUG log later
  GST_FIXME_OBJECT (gencamsrc, "stop camera and close");

  return gencamsrc_stop ();
}

/* given a buffer, return start and stop time when it should be pushed
 * out. The base class will sync on the clock using these times. */
static void
gst_gencamsrc_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);

  /* TODO use this if camera provides time stamp */
  {
    // TODO following is experimental hard coded to 5 fps
    GstClockTime timestamp = GST_BUFFER_PTS (buffer);
    *start = timestamp;
    *end =
        timestamp +
        (unsigned long) (1000000000UL /
        gencamsrc->properties.acquisitionFrameRate);
  }

  GST_DEBUG_OBJECT (gencamsrc, "get_times");
}

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
/*static gboolean
gst_gencamsrc_unlock (GstBaseSrc * src)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);

  GST_DEBUG_OBJECT (gencamsrc, "unlock");

  return TRUE;
}*/

/* Clear any pending unlock request, as we succeeded in unlocking */
/*static gboolean
gst_gencamsrc_unlock_stop (GstBaseSrc * src)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);

  GST_DEBUG_OBJECT (gencamsrc, "unlock_stop");

  return TRUE;
}*/

/* notify subclasses of a query */
/*static gboolean
gst_gencamsrc_query (GstBaseSrc * src, GstQuery * query)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);

  GST_DEBUG_OBJECT (gencamsrc, "query");

  return TRUE;
}*/

/* notify subclasses of an event */
/*static gboolean
gst_gencamsrc_event (GstBaseSrc * src, GstEvent * event)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);

  GST_DEBUG_OBJECT (gencamsrc, "event");

  return TRUE;
}*/

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn
gst_gencamsrc_create (GstPushSrc * src, GstBuffer ** buf)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);
  GstMapInfo mapInfo;

  GST_DEBUG_OBJECT (gencamsrc, "create frames");

  gencamsrc_create (buf, &mapInfo);

  // TODO This should get modified after timestamp feature implementation
  GST_BUFFER_PTS (*buf) =
      ((guint64) gencamsrc->frameNumber) * (unsigned long) (1000000000UL /
      gencamsrc->properties.acquisitionFrameRate);
  GST_BUFFER_DTS (*buf) = GST_CLOCK_TIME_NONE;
  gst_object_sync_values (GST_OBJECT (src), GST_BUFFER_PTS (*buf));

  gst_buffer_unmap (*buf, &mapInfo);

  ++gencamsrc->frameNumber;

  // TODO change to GST_DEBUG_OBJECT
  GST_FIXME_OBJECT (src, "Frame number: %u, Timestamp: %" GST_TIME_FORMAT,
      gencamsrc->frameNumber, GST_TIME_ARGS (GST_BUFFER_PTS (*buf)));

  return GST_FLOW_OK;
}

/* ask the subclass to allocate an output buffer. The default implementation
 * will use the negotiated allocator. */
/*static GstFlowReturn
gst_gencamsrc_alloc (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);

  GST_DEBUG_OBJECT (gencamsrc, "alloc");

  return GST_FLOW_OK;
}*/

/* ask the subclass to fill the buffer with data from offset and size */
/*static GstFlowReturn
gst_gencamsrc_fill (GstPushSrc * src, GstBuffer * buf)
{
  GstGencamsrc *gencamsrc = GST_GENCAMSRC (src);

  GST_DEBUG_OBJECT (gencamsrc, "fill");

  return GST_FLOW_OK;
}*/

static gboolean
plugin_init (GstPlugin * plugin)
{

  return gst_element_register (plugin, "gencamsrc", GST_RANK_NONE,
      GST_TYPE_GENCAMSRC);
}

#ifndef VERSION
#define VERSION "1.0.3"
#endif
#ifndef PACKAGE
#define PACKAGE "gst-gencamsrc"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gst-gencamsrc"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://www.intel.com/"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, gencamsrc,
    "Intel generic camera source elements", plugin_init, VERSION,
    "BSD", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
