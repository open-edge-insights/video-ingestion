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

#include "genicam.h"


bool Genicam::Init (GencamParams * params)
{
  gencamParams = params;

  return TRUE;
}


bool Genicam::Start (void)
{
  /* Enumerate & open device.
     Set the property (resolution, pixel format, etc.,)
     allocate buffers and start streaming from the camera */

  dev = rcg::getDevice (gencamParams->deviceSerialNumber);

  if (dev) {
    //TODO : Logging 

    dev->open (rcg::Device::CONTROL);
    std::cout << "Camera: " << gencamParams->deviceSerialNumber <<
        " opened succesfully.\n";

    nodemap = dev->getRemoteNodeMap ();

    // get chunk adapter (this switches chunk mode on if possible and
    // returns a null pointer if this is not possible)
    std::shared_ptr < GenApi::CChunkAdapter > chunkadapter = 0;

    try {
      // DeviceReset feature
      if (gencamParams->deviceReset == true) {
        return resetDevice ();
      }
      // Binning selector feature
      if (gencamParams->binningSelector[0] != '\0') {
        setBinningSelector ();
      }
      // Binning horizontal mode feature
      if (gencamParams->binningHorizontalMode[0] != '\0') {
        setBinningHorizontalMode ();
      }
      // Binning Horizontal feature
      if (gencamParams->binningHorizontal) {
        setBinningHorizontal ();
      }
      // Binning Vertical mode feature
      if (gencamParams->binningVerticalMode[0] != '\0') {
        setBinningVerticalMode ();
      }
      // Binning Vertical feature
      if (gencamParams->binningVertical) {
        setBinningVertical ();
      }
      // Width and Height features
      if (!setWidthHeight ()) {
        return FALSE;
      }
      // PixelFormat and PixelSize features
      if (!setPixelFormat ()) {
        return FALSE;
      }

    }
    catch (const std::exception & ex) {
      std::cerr << "Exception: " << ex.what () << std::endl;
      Stop ();
      return FALSE;
    }
    catch (const GENICAM_NAMESPACE::GenericException & ex) {
      std::cerr << "Exception: " << ex.what () << std::endl;
      Stop ();
      return FALSE;
    }
    catch ( ...) {
      std::cerr << "Exception: unknown" << std::endl;
      Stop ();
      return FALSE;
    }

    /* TODO Configure other features below,
       failure of which doesn't require pipeline to be reconnected
     */
    {
      // OffsetX and OffsetY feature
      setOffsetXY ();

      // Acquisition Frame Rate feature
      setAcquisitionFrameRate ();

      // Exposure Mode feature
      if (gencamParams->exposureMode)
        setExposureMode ();

      // Exposure Auto feature
      if (gencamParams->exposureAuto)
        setExposureAuto ();

      // Exposure Time Selector feature
      if (gencamParams->exposureTimeSelector)
        setExposureTimeSelector ();     /*Sets the configuration mode of the ExposureTime feature */

      // Exposure Time feature
      if (gencamParams->exposureTime)
        setExposureTime ();     /*Needs ExposureMode = Timed and ExposureAuto = Off */
    }

    stream = dev->getStreams ();
    if (stream.size () > 0) {
      // opening first stream
      stream[0]->open ();
      stream[0]->startStreaming ();

      //std::cout << "Package size: "
      //  << rcg::getString (nodemap, "GevSCPSPacketSize") << std::endl;

      //std::cout << std::endl;
    }

  } else {
    std::cerr << "Camera: " << gencamParams->deviceSerialNumber <<
        " not detected\n";
    return FALSE;
  }

  return TRUE;
}


bool Genicam::Stop (void)
{
  try {
    if (stream.size () > 0) {
      stream[0]->stopStreaming ();
      stream[0]->close ();
    }

    if (dev) {
      dev->close ();
    }
  }
  catch (const std::exception & ex)
  {
    std::cerr << "Exception: " << ex.what () << std::endl;
  } catch (const GENICAM_NAMESPACE::GenericException & ex)
  {
    std::cerr << "Exception: " << ex.what () << std::endl;
  } catch ( ...) {
    std::cerr << "Exception: unknown" << std::endl;
  }

  rcg::System::clearSystems ();

  return TRUE;
}


void
Genicam::Create (GstBuffer ** buf, GstMapInfo * mapInfo)
{
  /* TODO. Grab the buffer, copy and release, set framenum */

  try {
    /* TODO Remove later. It is dummy creating and filling buffer for now */
#ifdef RANDOM_RGB_TEST
    guint payloadSize = gencamParams->width * gencamParams->height * 3; // BGR
#else
    const rcg::Buffer * buffer = stream[0]->grab (300);
    guint payloadSize = buffer->getGlobalSize ();
#endif
    // TODO : Add appropriate debug message for the payload size
    //std::cout << "Payload size = " << payloadSize << std::endl;
    //std::cout << "Number of parts = " << buffer->getNumberOfParts() << std::endl;

    *buf = gst_buffer_new_allocate (NULL, payloadSize, NULL);
    gst_buffer_map (*buf, mapInfo, GST_MAP_WRITE);

#ifdef RANDOM_RGB_TEST
    gen_random_rgb (mapInfo->data, mapInfo->size);
#else
    memcpy (mapInfo->data, buffer->getGlobalBase (), mapInfo->size);
#endif
  } catch (const std::exception & ex)
  {
    std::cerr << "Exception: " << ex.what () << std::endl;
  } catch (const GENICAM_NAMESPACE::GenericException & ex)
  {
    std::cerr << "Exception: " << ex.what () << std::endl;
  } catch ( ...) {
    std::cerr << "Exception: unknown" << std::endl;
  }
}


#ifdef RANDOM_RGB_TEST
static void
gen_random_rgb (guint8 * buf, guint size)
{
  static guint val = 0;
  memset (buf, val, size);
  val = (val > 250) ? 0 : val + 5;
}
#endif


bool Genicam::resetDevice (void)
{
  // WARNING:: Do not modify unless absolutely sure
  rcg::callCommand (nodemap, "DeviceReset", true);

  // Device will poweroff immediately
  std::cout << "DeviceReset: " << gencamParams->deviceReset << " triggered\n";
  std::cout << "Device will take a few seconds to reset to factory default\n";

  // Stop gracefully if poweroff taking time
  Stop ();
  return FALSE;
}


bool Genicam::setBinningSelector (void)
{
  bool
      isBinningSelectorSet = false;
  std::vector < std::string > binningEngines;

  // Read binning engines supported by the camera
  rcg::getEnum (nodemap, "BinningSelector", binningEngines, false);

  // Iterate the configured binning engine with camera supported list
  if (strcmp (gencamParams->binningSelector, "sensor") == 0) {
    // Sensor binning engine is supported?
    for (size_t k = 0; k < binningEngines.size (); k++) {
      if (binningEngines[k] == "Sensor") {
        isBinningSelectorSet =
            rcg::setEnum (nodemap, "BinningSelector",
            binningEngines[k].c_str (), false);
        break;
      }
    }

  } else if (strcmp (gencamParams->binningSelector, "region0") == 0) {
    // Region0 binning engine is supported?
    for (size_t k = 0; k < binningEngines.size (); k++) {
      if (binningEngines[k] == "Region0") {
        isBinningSelectorSet =
            rcg::setEnum (nodemap, "BinningSelector",
            binningEngines[k].c_str (), false);
        break;
      }
    }

  } else if (strcmp (gencamParams->binningSelector, "region1") == 0) {
    // Region1 binning engine is supported?
    for (size_t k = 0; k < binningEngines.size (); k++) {
      if (binningEngines[k] == "Region1") {
        isBinningSelectorSet =
            rcg::setEnum (nodemap, "BinningSelector",
            binningEngines[k].c_str (), false);
        break;
      }
    }

  } else if (strcmp (gencamParams->binningSelector, "region2") == 0) {
    // Region2 binning engine is supported?
    for (size_t k = 0; k < binningEngines.size (); k++) {
      if (binningEngines[k] == "Region2") {
        isBinningSelectorSet =
            rcg::setEnum (nodemap, "BinningSelector",
            binningEngines[k].c_str (), false);
        break;
      }
    }

  } else {
    std::cout << "WARNING:: Invalid BinningSelector: " <<
        gencamParams->binningSelector << std::endl;
    return FALSE;
  }

  if (isBinningSelectorSet) {
    // Binning engine set success
    std::cout << "BinningSelector: " << gencamParams->binningSelector <<
        " set\n";
  } else {
    // Binning selector not supported by the camera
    std::cout << "WARNING:: BinningSelector: " << gencamParams->binningSelector
        << " not set. ";
    if (binningEngines.size () > 0) {
      std::cout << "Supported binning selector values are," << std::endl;
      for (size_t k = 0; k < binningEngines.size (); k++) {
        std::cout << "    " << binningEngines[k] << std::endl;
      }
    } else {
      std::cout << "Feature not supported by the camera.\n";
    }
  }

  return TRUE;
}


bool Genicam::setBinningHorizontalMode (void)
{
  bool
      isBinningHorizontalModeSet = false;

  std::vector < std::string > binningHorizontalModes;

  // Read binning engines supported by the camera
  rcg::getEnum (nodemap, "BinningHorizontalMode", binningHorizontalModes,
      false);

  // Iterate the configured binning horizontal mode with camera supported list
  if (strcmp (gencamParams->binningHorizontalMode, "sum") == 0) {
    // Sum binning horizontal mode is supported?
    for (size_t k = 0; k < binningHorizontalModes.size (); k++) {
      if (binningHorizontalModes[k] == "Sum") {
        isBinningHorizontalModeSet =
            rcg::setEnum (nodemap, "BinningHorizontalMode",
            binningHorizontalModes[k].c_str (), false);
        break;
      }
    }

  } else if (strcmp (gencamParams->binningHorizontalMode, "average") == 0) {
    // Average binning horizontal mode is supported?
    for (size_t k = 0; k < binningHorizontalModes.size (); k++) {
      if (binningHorizontalModes[k] == "Average") {
        isBinningHorizontalModeSet =
            rcg::setEnum (nodemap, "BinningHorizontalMode",
            binningHorizontalModes[k].c_str (), false);
        break;
      }
    }

  } else {
    std::cout << "WARNING:: Invalid BinningHorizontalMode: " <<
        gencamParams->binningHorizontalMode << std::endl;
    return FALSE;
  }


  if (isBinningHorizontalModeSet) {
    // Binning Horizontal Mode set success
    std::
        cout << "BinningHorizontalMode: " << gencamParams->binningHorizontalMode
        << " set\n";
  } else {
    // Binning horizontal modes not supported by the camera
    std::cout << "WARNING:: BinningHorizontalMode: " <<
        gencamParams->binningHorizontalMode << " not set. ";
    if (binningHorizontalModes.size () > 0) {
      std::cout << "Supported binning horizontal modes are," << std::endl;
      for (size_t k = 0; k < binningHorizontalModes.size (); k++) {
        std::cout << "    " << binningHorizontalModes[k] << std::endl;
      }
    } else {
      std::cout << "Feature not supported by the camera.\n";
    }
  }

  return TRUE;
}


bool Genicam::setBinningHorizontal (void)
{
  bool
      ret = false;
  int64_t
      vMin,
      vMax;

  if (rcg::getInteger (nodemap, "BinningHorizontal", &vMin, &vMax, false, true)) {
    ret =
        rcg::setInteger (nodemap, "BinningHorizontal",
        gencamParams->binningHorizontal, false);
  }

  if (ret) {
    std::cout << "BinningHorizontal: " << gencamParams->binningHorizontal <<
        " set\n";
  } else {
    std::cout << "WARNING:: BinningHorizontal: " <<
        gencamParams->binningHorizontal << " not set.\n";
  }

  return ret;
}


bool Genicam::setBinningVerticalMode (void)
{
  bool
      isBinningVerticalModeSet = false;

  std::vector < std::string > binningVerticalModes;

  // Read binning engines supported by the camera
  rcg::getEnum (nodemap, "BinningVerticalMode", binningVerticalModes, false);

  // Iterate the configured binning vertical mode with camera supported list
  if (strcmp (gencamParams->binningVerticalMode, "sum") == 0) {
    // Sum binning vertical mode is supported?
    for (size_t k = 0; k < binningVerticalModes.size (); k++) {
      if (binningVerticalModes[k] == "Sum") {
        isBinningVerticalModeSet =
            rcg::setEnum (nodemap, "BinningVerticalMode",
            binningVerticalModes[k].c_str (), false);
        break;
      }
    }

  } else if (strcmp (gencamParams->binningVerticalMode, "average") == 0) {
    // Average binning vertical mode is supported?
    for (size_t k = 0; k < binningVerticalModes.size (); k++) {
      if (binningVerticalModes[k] == "Average") {
        isBinningVerticalModeSet =
            rcg::setEnum (nodemap, "BinningVerticalMode",
            binningVerticalModes[k].c_str (), false);
        break;
      }
    }

  } else {
    std::cout << "WARNING:: Invalid BinningVerticalMode: " <<
        gencamParams->binningVerticalMode << std::endl;
    return FALSE;
  }

  if (isBinningVerticalModeSet) {
    // Binning Vertical Mode set success
    std::cout << "BinningVerticalMode: " << gencamParams->binningVerticalMode <<
        " set\n";
  } else {
    // Binning vertical modes not supported by the camera
    std::cout << "WARNING:: BinningVerticalMode: " <<
        gencamParams->binningVerticalMode << " not set. ";
    if (binningVerticalModes.size () > 0) {
      std::cout << "Supported binning vertical modes are," << std::endl;
      for (size_t k = 0; k < binningVerticalModes.size (); k++) {
        std::cout << "    " << binningVerticalModes[k] << std::endl;
      }
    } else {
      std::cout << "Feature not supported by the camera.\n";
    }
  }

  return TRUE;
}


bool Genicam::setBinningVertical (void)
{
  bool
      ret = false;
  int64_t
      vMin,
      vMax;

  if (rcg::getInteger (nodemap, "BinningVertical", &vMin, &vMax, false, true)) {
    ret =
        rcg::setInteger (nodemap, "BinningVertical",
        gencamParams->binningVertical, false);
  }

  if (ret) {
    std::cout << "BinningVertical: " << gencamParams->binningVertical <<
        " set\n";
  } else {
    std::cout << "WARNING:: BinningVertical: " << gencamParams->binningVertical
        << " not set.\n";
  }

  return ret;
}


bool Genicam::setPixelFormat (void)
{
  bool
      isPixelFormatSet = false;
  std::vector < std::string > pixelFormats;
  std::vector < std::string > pixelSizes;
  std::string pixelSize;

  // Read the pixel formats and size supported by the camera
  rcg::getEnum (nodemap, "PixelFormat", pixelFormats, true);
  rcg::getEnum (nodemap, "PixelSize", pixelSizes, true);

  // Iterate the configured format with camera supported list
  // Mapping necessary from FOURCC to GenICam SFNC / PFNC

  if (strcmp (gencamParams->pixelFormat, "mono8") == 0) {
    // Check Mono8 / GRAY8 / Y8 is supported by the camera
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "Mono8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);

        // Check if corresponding pixel size supported
        for (size_t l = 0; l < pixelSizes.size (); l++) {
          if (pixelSizes[l] == "Bpp8") {
            pixelSize.assign (pixelSizes[l]);
            break;
          }
        }
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcmp (gencamParams->pixelFormat, "ycbcr411_8") == 0) {
    // I420 / YUV420 / YCbCr411 8 bit supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "YCbCr411_8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);

        // Check if corresponding pixel size supported
        for (size_t l = 0; l < pixelSizes.size (); l++) {
          if (pixelSizes[l] == "Bpp12") {
            pixelSize.assign (pixelSizes[l]);
            break;
          }
        }
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcmp (gencamParams->pixelFormat, "ycbcr422_8") == 0) {
    // YUY2 / YUV422 / Ycbcr422 8 bit supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "YUV422_8"
          || pixelFormats[k] == "YUV422_YUYV_Packed") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);

        // Check if corresponding pixel size supported
        for (size_t l = 0; l < pixelSizes.size (); l++) {
          if (pixelSizes[l] == "Bpp12") {
            pixelSize.assign (pixelSizes[l]);
            break;
          }
        }
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcmp (gencamParams->pixelFormat, "bayerbggr") == 0) {
    // BayerBG8 is supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "BayerBG8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);
        for (size_t l = 0; l < pixelSizes.size (); l++) {

          // Check if corresponding pixel size supported
          if (pixelSizes[l] == "Bpp8") {
            pixelSize.assign (pixelSizes[l]);
            break;
          }
        }
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcmp (gencamParams->pixelFormat, "bayerrggb") == 0) {
    // BayerRG8 supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "BayerRG8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);

        // Check if corresponding pixel size supported
        for (size_t l = 0; l < pixelSizes.size (); l++) {
          if (pixelSizes[l] == "Bpp8") {
            pixelSize.assign (pixelSizes[l]);
            break;
          }
        }
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcmp (gencamParams->pixelFormat, "bayergrbg") == 0) {
    // BayerBG8 supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "BayerGR8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);

        // Check if corresponding pixel size supported
        for (size_t l = 0; l < pixelSizes.size (); l++) {
          if (pixelSizes[l] == "Bpp8") {
            pixelSize.assign (pixelSizes[l]);
            break;
          }
        }
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcmp (gencamParams->pixelFormat, "bayergbrg") == 0) {
    // BayerGB8 supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "BayerGB8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);

        // Check if corresponding pixel size supported
        for (size_t l = 0; l < pixelSizes.size (); l++) {
          if (pixelSizes[l] == "Bpp8") {
            pixelSize.assign (pixelSizes[l]);
            break;
          }
        }
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcmp (gencamParams->pixelFormat, "rgb8") == 0) {
    // RGB8, 24 bit supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "RGB8" || pixelFormats[k] == "RGB8Packed") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);

        // Check if corresponding pixel size supported
        for (size_t l = 0; l < pixelSizes.size (); l++) {
          if (pixelSizes[l] == "Bpp24") {
            pixelSize.assign (pixelSizes[l]);
            break;
          }
        }
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcmp (gencamParams->pixelFormat, "bgr8") == 0) {
    // BGR8, 24 bit supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "BGR8" || pixelFormats[k] == "BGR8Packed") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);

        // Check if corresponding pixel size supported
        for (size_t l = 0; l < pixelSizes.size (); l++) {
          if (pixelSizes[l] == "Bpp24") {
            pixelSize.assign (pixelSizes[l]);
            break;
          }
        }
        isPixelFormatSet = true;
        break;
      }
    }
  }

  if (isPixelFormatSet) {
    // Format set success
    std::cout << "PixelFormat: " << gencamParams->pixelFormat << " set\n";
    std::cout << "PixelSize: " << pixelSize << " set\n";
  } else {
    // Format is not supported by the camera, terminate
    std::cout << "PixelFormat: " << gencamParams->pixelFormat <<
        " not supported by the camera\n";
    std::cout << "Pixel formats supported are below," << std::endl;
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      std::cout << "    " << pixelFormats[k] << std::endl;
    }
    Stop ();
    return FALSE;
  }

  return TRUE;
}

bool Genicam::setWidthHeight (void)
{
  int64_t
      vMin,
      vMax;

  bool
      isWidthHeightSet = false;

  int
      widthMax = rcg::getInteger (nodemap, "WidthMax", &vMin, &vMax, false, 0);
  int
      heightMax =
      rcg::getInteger (nodemap, "HeightMax", &vMin, &vMax, false, 0);

  if (gencamParams->width > widthMax) {
    std::
        cout << "WARNING:: Width: " << gencamParams->width <<
        " higher than max width " << widthMax << " supported by camera\n";
    // Align the width to 4
    gencamParams->width = ROUNDED_DOWN (widthMax, 0x4 - 1);
  }

  if (gencamParams->height > heightMax) {
    std::
        cout << "WARNING:: Height: " << gencamParams->height <<
        " higher than max height " << heightMax << " supported by camera\n";
    // Align the height to 4
    gencamParams->height = ROUNDED_DOWN (heightMax, 0x4 - 1);
  }

  isWidthHeightSet =
      rcg::setInteger (nodemap, "Width", gencamParams->width, true);
  isWidthHeightSet |=
      rcg::setInteger (nodemap, "Height", gencamParams->height, true);

  if (isWidthHeightSet) {
    std::cout << "Width: " << rcg::getInteger (nodemap, "Width", NULL, NULL,
        false, true) << " set\n";
    std::cout << "Height: " << rcg::getInteger (nodemap, "Height", NULL, NULL,
        false, true) << " set\n";

    gencamParams->widthMax = widthMax;
    gencamParams->heightMax = heightMax;
  } else {
    std::cout << "WARNING:: Width and Height set fail " << std::endl;
    Stop ();
  }

  return isWidthHeightSet;
}

bool Genicam::setOffsetXY (void)
{
  bool
      isOffsetXYset = false;

  // Check and handle Border Pixels.
  if (gencamParams->offsetX > gencamParams->widthMax - gencamParams->width) {
    gencamParams->offsetX = gencamParams->widthMax - gencamParams->width;
    std::cout << "WARNING:: OffsetX limit exceeded. Adjust OffsetX to " <<
        gencamParams->offsetX << std::endl;
  }

  if (gencamParams->offsetY > gencamParams->heightMax - gencamParams->height) {
    gencamParams->offsetY = gencamParams->heightMax - gencamParams->height;
    std::cout << "WARNING:: OffsetY limit exceeded. Adjust OffsetY to " <<
        gencamParams->offsetY << std::endl;
  }

  isOffsetXYset =
      rcg::setInteger (nodemap, "OffsetX", gencamParams->offsetX, false);
  isOffsetXYset |=
      rcg::setInteger (nodemap, "OffsetY", gencamParams->offsetY, false);

  if (!isOffsetXYset) {
    /*
     * if unable to set the offset-X and offset-Y probably they are RO.
     * Don't abort the code, but continue.
     */
    std::cout << "WARNING:: Unable to set Offset-X and Offset-Y" << std::endl;
  } else {
    std::cout << "OffsetX: " << gencamParams->offsetX << " set\n";
    std::cout << "OffsetY: " << gencamParams->offsetY << " set\n";
  }

  return isOffsetXYset;
}

bool Genicam::setAcquisitionFrameRate (void)
{
  bool
      isFrameRateSet = false;
  // AcquisitionFrameRateEnable and AcquisitionFrameRate feature
  // TODO: /*Needs TriggerMode = Off*/
  std::string triggerMode = rcg::getEnum (nodemap, "TriggerMode", false);
  if (triggerMode == "On") {
    std::cout << "Ignore setting AcquisitionFrameRate as TriggerMode is \"On\""
        << std::endl;
    return isFrameRateSet;
  }

  double
      vMin,
      vMax;
  float
      frameRate = 0;
  char
      frameRateString[32];

  frameRate =
      rcg::getFloat (nodemap, "AcquisitionFrameRate", &vMin, &vMax, false, 0);
  if (frameRate)
    strcpy (frameRateString, "AcquisitionFrameRate");
  else {
    frameRate =
        rcg::getFloat (nodemap, "AcquisitionFrameRateAbs", &vMin, &vMax,
        false, 0);
    if (frameRate)
      strcpy (frameRateString, "AcquisitionFrameRateAbs");
  }

  // Handle if the Input FrameRate > Maximum supported framerate
  if (gencamParams->acquisitionFrameRate > vMax) {
    std::cout << "WARNING:: Maximum supported Framerate is: " << vMax <<
        std::endl;
    gencamParams->acquisitionFrameRate = vMax;
  }

  isFrameRateSet =
      (rcg::setBoolean (nodemap, "AcquisitionFrameRateEnable", 1, false)
      || rcg::setBoolean (nodemap, "AcquisitionFrameRateEnabled", 1, false));

  if (!isFrameRateSet) {
    gencamParams->acquisitionFrameRate = frameRate;

    std::cout <<
        "WARNING:: AcquisitionFrameRate not configurable, current FrameRate = "
        << frameRate << std::endl;
  } else {
    isFrameRateSet =
        rcg::setFloat (nodemap, frameRateString,
        gencamParams->acquisitionFrameRate, false);

    if (!isFrameRateSet)
      std::cout << "WARNING:: AcquisitionFrameRate: " <<
          gencamParams->acquisitionFrameRate << " set failed" << std::endl;
    else
      std::cout << "AcquisitionFrameRate: " <<
          gencamParams->acquisitionFrameRate << " set\n";
  }

  return isFrameRateSet;
}

bool Genicam::setExposureMode (void)
{
  bool
      isExposureModeSet = false;

  double
      vMin,
      vMax,
      expTime;
  (expTime =
      rcg::getFloat (nodemap, "ExposureTime", &vMin, &vMax, false,
          0)) ? expTime : rcg::getFloat (nodemap, "ExposureTimeAbs", &vMin,
      &vMax, false, 0);

  rcg::setFloat (nodemap, "AutoExposureTimeAbsLowerLimit", vMin, false);
  rcg::setFloat (nodemap, "AutoExposureTimeLowerLimit", vMin, false);
  rcg::setFloat (nodemap, "AutoExposureTimeAbsUpperLimit", vMax, false);
  rcg::setFloat (nodemap, "AutoExposureTimeUpperLimit", vMax, false);

  std::vector < std::string > exposureModes;
  rcg::getEnum (nodemap, "ExposureMode", exposureModes, false);

  if (strcasecmp (gencamParams->exposureMode, "Off") == 0) {
    for (size_t k = 0; k < exposureModes.size (); k++) {
      if (exposureModes[k] == "Off") {
        isExposureModeSet =
            rcg::setEnum (nodemap, "ExposureMode", exposureModes[k].c_str (),
            false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->exposureMode, "Timed") == 0) {
    for (size_t k = 0; k < exposureModes.size (); k++) {
      if (exposureModes[k] == "Timed") {
        isExposureModeSet =
            rcg::setEnum (nodemap, "ExposureMode", exposureModes[k].c_str (),
            false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->exposureMode, "TriggerWidth") == 0) {
    for (size_t k = 0; k < exposureModes.size (); k++) {
      if (exposureModes[k] == "TriggerWidth") {
        isExposureModeSet =
            rcg::setEnum (nodemap, "ExposureMode", exposureModes[k].c_str (),
            false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->exposureMode, "TriggerControlled") == 0) {
    for (size_t k = 0; k < exposureModes.size (); k++) {
      if (exposureModes[k] == "TriggerControlled") {
        isExposureModeSet =
            rcg::setEnum (nodemap, "ExposureMode", exposureModes[k].c_str (),
            false);
        break;
      }
    }
  } else {
    std::cout << "Invalid ExposureMode : \"" << gencamParams->exposureMode
        << "\"" << std::endl;
    std::cout << "ExposureModes supported are below," << std::endl;
    for (size_t k = 0; k < exposureModes.size (); k++)
      std::cout << "    " << exposureModes[k] << std::endl;
  }

  if (!isExposureModeSet) {
    std::cout << "WARNING:: ExposureMode \"" << gencamParams->exposureMode <<
        "\" not set. Current ";
  }

  std::string exposureMode = rcg::getEnum (nodemap, "ExposureMode", false);
  std::cout << "ExposureMode: " << exposureMode << " set\n";
  return isExposureModeSet;
}


bool Genicam::setExposureTime (void)
{
  bool
      isExposureTimeSet = false;
  double
      vMin,
      vMax,
      expTime;

  (expTime =
      rcg::getFloat (nodemap, "ExposureTime", &vMin, &vMax, false,
          0)) ? expTime : rcg::getFloat (nodemap, "ExposureTimeAbs", &vMin,
      &vMax, false, 0);

  std::string exposureMode = rcg::getEnum (nodemap, "ExposureMode", false);
  std::string exposureAuto = rcg::getEnum (nodemap, "ExposureAuto", false);

  if (exposureMode == "Timed" && exposureAuto == "Off") {

    if (gencamParams->exposureTime < vMin || gencamParams->exposureTime > vMax) {
      std::cout << "WARNING:: Valid ExposureTime is between " << vMin << " and "
          << vMax << std::endl;
      if (gencamParams->exposureTime < vMin)
        gencamParams->exposureTime = vMin;
      if (gencamParams->exposureTime > vMax)
        gencamParams->exposureTime = vMax;
    }

    isExposureTimeSet = rcg::setFloat (nodemap, "ExposureTimeAbs",
        gencamParams->exposureTime, false) ||
        rcg::setFloat (nodemap, "ExposureTime", gencamParams->exposureTime,
        false);

    if (!isExposureTimeSet)
      std::cout << "WARNING:: ExposureTime set Failed to " <<
          gencamParams->exposureTime << std::endl;
  } else
    std::cout <<
        "WARNING:: ExposureTime not set, exposureMode must be \"Timed\"" <<
        " & exposureAuto must be \"Off\"" << std::endl;

  std::cout << "Exposure Time = " << ((expTime =
          rcg::getFloat (nodemap, "ExposureTime", &vMin, &vMax, false,
              0)) ? expTime : rcg::getFloat (nodemap, "ExposureTimeAbs", &vMin,
          &vMax, false, 0)) << std::endl;

  return isExposureTimeSet;
}

bool Genicam::setExposureAuto (void)
{
  bool
      isExposureAutoSet = false;

  std::vector < std::string > exposureAutos;
  rcg::getEnum (nodemap, "ExposureAuto", exposureAutos, false);

  if (strcasecmp (gencamParams->exposureAuto, "Off") == 0) {
    for (size_t k = 0; k < exposureAutos.size (); k++) {
      if (exposureAutos[k] == "Off") {
        isExposureAutoSet =
            rcg::setEnum (nodemap, "ExposureAuto", exposureAutos[k].c_str (),
            false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->exposureAuto, "Once") == 0) {
    for (size_t k = 0; k < exposureAutos.size (); k++) {
      if (exposureAutos[k] == "Once") {
        isExposureAutoSet =
            rcg::setEnum (nodemap, "ExposureAuto", exposureAutos[k].c_str (),
            false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->exposureAuto, "Continuous") == 0) {
    for (size_t k = 0; k < exposureAutos.size (); k++) {
      if (exposureAutos[k] == "Continuous") {
        isExposureAutoSet =
            rcg::setEnum (nodemap, "ExposureAuto", exposureAutos[k].c_str (),
            false);
        break;
      }
    }
  } else {
    std::cout << "Invalid ExposureAuto : \"" << gencamParams->exposureAuto
        << "\"" << std::endl;
    std::cout << "ExposureAutos supported are below," << std::endl;
    for (size_t k = 0; k < exposureAutos.size (); k++)
      std::cout << "    " << exposureAutos[k] << std::endl;
  }

  if (!isExposureAutoSet)
    std::cout << "ExposureAuto \"" << gencamParams->exposureAuto <<
        " not set.\n";
  else
    std::cout << "ExposureAuto \"" << gencamParams->exposureAuto << "\" set" <<
        std::endl;

  return isExposureAutoSet;
}

bool Genicam::setExposureTimeSelector (void)
{
  bool
      isExposureTimeSelectorSet = false;
  std::string eTimeSelector;

  std::vector < std::string > exposureTimeSelectors;
  eTimeSelector =
      rcg::getEnum (nodemap, "ExposureTimeSelector", exposureTimeSelectors,
      false);

  if (strcasecmp (eTimeSelector.c_str (), "") == 0) {
    std::cout <<
        "WARNING: ExposureTimeSelector and ExposureTimeMode not Supported" <<
        std::endl;
    return isExposureTimeSelectorSet;
  }

  if (strcasecmp (gencamParams->exposureTimeSelector, "Common") == 0) {
    rcg::setEnum (nodemap, "ExposureTimeMode", "Common", false);
    isExposureTimeSelectorSet =
        rcg::setEnum (nodemap, "ExposureTimeSelector",
        gencamParams->exposureTimeSelector, false);
  } else {
    rcg::setEnum (nodemap, "ExposureTimeMode", "Individual", false);
    isExposureTimeSelectorSet =
        rcg::setEnum (nodemap, "ExposureTimeSelector",
        gencamParams->exposureTimeSelector, false);
  }

  if (!isExposureTimeSelectorSet) {
    std::cout << "WARNING: Invalid ExposureTimeSelector : \"" <<
        gencamParams->exposureTimeSelector << "\"" << std::endl;
    std::cout << "ExposureTimeSelectors supported are below," << std::endl;
    for (size_t k = 0; k < exposureTimeSelectors.size (); k++)
      std::cout << "    " << exposureTimeSelectors[k] << std::endl;
  }

  std::cout << "ExposureTimeMode is \"" << rcg::getEnum (nodemap,
      "ExposureTimeMode", false) << "\"" << std::endl;

  std::cout << "ExposureTimeSelector is \"" << rcg::getEnum (nodemap,
      "ExposureTimeSelector", false) << "\"" << std::endl;
  return isExposureTimeSelectorSet;
}
