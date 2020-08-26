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


bool
Genicam::Init (GencamParams * params)
{
  gencamParams = params;

  return TRUE;
}


bool
Genicam::Start (void)
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
      if (gencamParams->offsetX || gencamParams->offsetY) {
        setOffsetXY ();
      }
      // Acquisition Mode feature
      if (gencamParams->acquisitionMode[0] != '\0') {
        setAcquisitionMode ();
      }
      // Trigger Selector feature
      if (gencamParams->triggerSelector[0] != '\0') {
        setTriggerSelector ();
      }
      // Trigger Activation feature
      if (gencamParams->triggerActivation[0] != '\0') {
        setTriggerActivation ();
      }
      // Trigger Source feature, needs trigger mode on
      if (gencamParams->triggerSource[0] != '\0') {
        setTriggerSource ();
      }
      // Trigger Multiplier feature
      if (gencamParams->triggerMultiplier) {
        setTriggerMultiplier ();
      }
      // Trigger Divider feature
      if (gencamParams->triggerDivider) {
        setTriggerDivider ();
      }
      // TriggerDelay feature
      if (gencamParams->triggerDelay) {
        setTriggerDelay ();
      }
      // Trigger overlap feature
      if (gencamParams->triggerOverlap[0] != '\0') {
        setTriggerOverlap ();
      }
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
        setExposureTimeSelector ();
      // Exposure Time feature
      /* Needs ExposureMode = Timed and ExposureAuto = Off */
      if (gencamParams->exposureTime)
        setExposureTime ();

      // Check if AcquisitionStatus feature is present for Non-Continuous mode operation in "Create"
      isAcquisitionStatusFeature = isFeature ("AcquisitionStatus");
    }

    stream = dev->getStreams ();
    if (stream.size () > 0) {
      // opening first stream
      stream[0]->open ();
      stream[0]->startStreaming ();

      if (acquisitionMode != "Continuous" && triggerMode == "On"
          && triggerSource == "Software") {
        setTriggerSoftware ();
      }
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


bool
Genicam::Stop (void)
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


bool Genicam::Create (GstBuffer ** buf, GstMapInfo * mapInfo)
{
  /* Grab the buffer, copy and release, set framenum */
  int
      hwTriggerCheck = 0;
  try {
    do {
#ifdef RANDOM_RGB_TEST
      /* TODO Remove later. It is dummy creating and filling buffer for RGB */
      guint
          payloadSize = gencamParams->width * gencamParams->height * 3;
#else
      const
          rcg::Buffer *
          buffer = stream[0]->grab (1000);
      if (buffer != NULL) {
        guint
            payloadSize = buffer->getGlobalSize ();
#endif
        // TODO : Add appropriate debug message for the payload size
        //std::cout << "Payload size = " << payloadSize << std::endl;
        //std::cout << "Number of parts = " << buffer->getNumberOfParts() << std::endl;

        *buf = gst_buffer_new_allocate (NULL, payloadSize, NULL);
        gst_buffer_map (*buf, mapInfo, GST_MAP_WRITE);

#ifdef RANDOM_RGB_TEST
        gen_random_rgb (mapInfo->data, mapInfo->size);

        return TRUE;
#else
        memcpy (mapInfo->data, buffer->getGlobalBase (), mapInfo->size);

        // For Non continuous modes, execute TriggerSoftware command
        if (acquisitionMode != "Continuous") {
          stream[0]->stopStreaming ();
          stream[0]->startStreaming ();
          // TODO handle multi frame, needs separate frame count for that
          if (triggerMode == "On" && triggerSource == "Software") {
            // If "AcquisitionStatus" feature is present, check the status
            while (!(rcg::getBoolean (nodemap, "AcquisitionStatus", false,
                        false)) && isAcquisitionStatusFeature);
            setTriggerSoftware ();
          }
        }
        return TRUE;
      } else {
        if (acquisitionMode != "Continuous" && triggerMode == "On"
            && triggerSource != "Software") {
          // If Hw trigger, wait for 10 secs
          std::
              cout << "Waiting for a Trigger (" << (gencamParams->hwTriggerRetry
              - hwTriggerCheck) << "s)..\n";
          hwTriggerCheck += 1;
        }
      }
    } while (hwTriggerCheck < gencamParams->hwTriggerRetry
        && acquisitionMode != "Continuous" && triggerMode == "On"
        && triggerSource != "Software");

    std::cerr << "No frame received from the camera\n";
    return FALSE;
#endif
  }

  catch (const std::exception & ex) {
    std::cerr << "Exception: " << ex.what () << std::endl;
  } catch (const GENICAM_NAMESPACE::GenericException & ex)
  {
    std::cerr << "Exception: " << ex.what () << std::endl;
  } catch ( ...) {
    std::cerr << "Exception: unknown" << std::endl;
  }

  return FALSE;
}


#ifdef RANDOM_RGB_TEST
static void
gen_random_rgb (guint8 * buf, guint size)
{
  static guint
      val = 0;
  memset (buf, val, size);
  val = (val > 250) ? 0 : val + 5;
}
#endif


bool
Genicam::isFeature (const char *featureName)
{
  bool ret = true;
  char str[32] = "Feature not found\0";
  try {
    rcg::getEnum (nodemap, featureName, true);
  }
  catch (const std::exception & ex)
  {
    if (strncmp (ex.what (), str, strlen (str)) == 0) {
      ret = false;
    }
  }
  return ret;
}


bool
Genicam::resetDevice (void)
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


bool
Genicam::setBinningSelector (void)
{
  bool isBinningSelectorSet = false;
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


bool
Genicam::setBinningHorizontalMode (void)
{
  bool isBinningHorizontalModeSet = false;

  std::vector < std::string > binningHorizontalModes;

  // Read binning engines supported by the camera
  rcg::getEnum (nodemap, "BinningHorizontalMode", binningHorizontalModes,
      false);
  if (binningHorizontalModes.empty()) {
    // Handle variations, deviations from SFNC standard
    rcg::getEnum (nodemap, "BinningModeHorizontal", binningHorizontalModes,
        false);
  }

  // Iterate the configured binning horizontal mode with camera supported list
  if (strcasecmp (gencamParams->binningHorizontalMode, "sum") == 0) {
    // Sum binning horizontal mode is supported?
    for (size_t k = 0; k < binningHorizontalModes.size (); k++) {
      if ((binningHorizontalModes[k] == "Sum") ||
          (binningHorizontalModes[k] == "Summing")) {
        isBinningHorizontalModeSet =
            rcg::setEnum (nodemap, "BinningHorizontalMode",
            binningHorizontalModes[k].c_str (), false);
        if (!isBinningHorizontalModeSet) {
          // Deviation from SFNC, handle it
          isBinningHorizontalModeSet = 
              rcg::setEnum (nodemap, "BinningModeHorizontal",
              binningHorizontalModes[k].c_str (), false);
        }
        break;
      }
    }

  } else if (strcasecmp (gencamParams->binningHorizontalMode, "average") == 0) {
    // Average binning horizontal mode is supported?
    for (size_t k = 0; k < binningHorizontalModes.size (); k++) {
      if ((binningHorizontalModes[k] == "Average") ||
          (binningHorizontalModes[k] == "Averaging")) {
        isBinningHorizontalModeSet =
            rcg::setEnum (nodemap, "BinningHorizontalMode",
            binningHorizontalModes[k].c_str (), false);
        if (!isBinningHorizontalModeSet) {
          // Deviation from SFNC, handle it
          isBinningHorizontalModeSet = 
              rcg::setEnum (nodemap, "BinningModeHorizontal",
              binningHorizontalModes[k].c_str (), false);
        }
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
    std::cout << "BinningHorizontalMode: " << gencamParams->
        binningHorizontalMode << " set\n";
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


bool
Genicam::setBinningHorizontal (void)
{
  bool ret = false;
  int64_t vMin, vMax;

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


bool
Genicam::setBinningVerticalMode (void)
{
  bool isBinningVerticalModeSet = false;

  std::vector < std::string > binningVerticalModes;

  // Read binning engines supported by the camera
  rcg::getEnum (nodemap, "BinningVerticalMode", binningVerticalModes, false);
  if (binningVerticalModes.empty()) {
    // Handle deviations from SFNC standard
    rcg::getEnum (nodemap, "BinningModeVertical", binningVerticalModes,
        false);
  }

  // Iterate the configured binning vertical mode with camera supported list
  if (strcasecmp (gencamParams->binningVerticalMode, "sum") == 0) {
    // Sum binning vertical mode is supported?
    for (size_t k = 0; k < binningVerticalModes.size (); k++) {
      if ((binningVerticalModes[k] == "Sum") ||
          (binningVerticalModes[k] == "Summing")) {
        isBinningVerticalModeSet =
            rcg::setEnum (nodemap, "BinningVerticalMode",
            binningVerticalModes[k].c_str (), false);
        if (!isBinningVerticalModeSet) {
          // Deviation from SFNC, handle it
          isBinningVerticalModeSet = 
              rcg::setEnum (nodemap, "BinningModeVertical",
              binningVerticalModes[k].c_str (), false);
        }
        break;
      }
    }

  } else if (strcmp (gencamParams->binningVerticalMode, "average") == 0) {
    // Average binning vertical mode is supported?
    for (size_t k = 0; k < binningVerticalModes.size (); k++) {
      if ((binningVerticalModes[k] == "Average") ||
          (binningVerticalModes[k] == "Averaging")) {
        isBinningVerticalModeSet =
            rcg::setEnum (nodemap, "BinningVerticalMode",
            binningVerticalModes[k].c_str (), false);
        if (!isBinningVerticalModeSet) {
          // Deviation from SFNC, handle it
          isBinningVerticalModeSet = 
              rcg::setEnum (nodemap, "BinningModeVertical",
              binningVerticalModes[k].c_str (), false);
        }
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


bool
Genicam::setBinningVertical (void)
{
  bool ret = false;
  int64_t vMin, vMax;

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


bool
Genicam::setPixelFormat (void)
{
  bool isPixelFormatSet = false;
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


bool
Genicam::setWidthHeight (void)
{
  int64_t vMin, vMax;

  bool isWidthHeightSet = false;

  int widthMax = rcg::getInteger (nodemap, "WidthMax", &vMin, &vMax, false, 0);
  int heightMax =
      rcg::getInteger (nodemap, "HeightMax", &vMin, &vMax, false, 0);

  if (gencamParams->width > widthMax) {
    std::cout << "WARNING:: Width: " << gencamParams->width <<
        " higher than max width " << widthMax << " supported by camera\n";
    // Align the width to 4
    gencamParams->width = ROUNDED_DOWN (widthMax, 0x4 - 1);
  }

  if (gencamParams->height > heightMax) {
    std::cout << "WARNING:: Height: " << gencamParams->height <<
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
    std::cout << "Height: " << rcg::getInteger (nodemap, "Height", NULL,
        NULL, false, true) << " set\n";

    gencamParams->widthMax = widthMax;
    gencamParams->heightMax = heightMax;
  } else {
    std::cout << "WARNING:: Width and Height set fail " << std::endl;
    Stop ();
  }

  return isWidthHeightSet;
}


bool
Genicam::setOffsetXY (void)
{
  bool isOffsetXYset = false;

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


bool
Genicam::setAcquisitionFrameRate (void)
{
  // AcquisitionFrameRateEnable and AcquisitionFrameRate feature
  bool isFrameRateSet = false;

  /*Needs TriggerMode = Off */
  if (triggerMode == "On") {
    std::cout <<
        "Ignore setting AcquisitionFrameRate as TriggerMode is \"On\"" <<
        std::endl;
    return isFrameRateSet;
  }

  double vMin, vMax;
  float frameRate = 0;
  char frameRateString[32];

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


bool
Genicam::setExposureMode (void)
{
  bool isExposureModeSet = false;

  double vMin, vMax, expTime;
  (expTime =
      rcg::getFloat (nodemap, "ExposureTime", &vMin, &vMax, false,
          0)) ? expTime : rcg::getFloat (nodemap, "ExposureTimeAbs", &vMin,
      &vMax, false, 0);

  // Set the limits for Exposure Modes
  rcg::setFloat (nodemap, "AutoExposureTimeAbsLowerLimit", vMin, false);
  rcg::setFloat (nodemap, "AutoExposureTimeLowerLimit", vMin, false);
  rcg::setFloat (nodemap, "AutoExposureTimeAbsUpperLimit", vMax, false);
  rcg::setFloat (nodemap, "AutoExposureTimeUpperLimit", vMax, false);

  // Read the Exposure Modes aupported
  std::vector < std::string > exposureModes;
  rcg::getEnum (nodemap, "ExposureMode", exposureModes, false);

  if (strcasecmp (gencamParams->exposureMode, "Off") == 0) {
    for (size_t k = 0; k < exposureModes.size (); k++) {
      if (exposureModes[k] == "Off") {
        isExposureModeSet =
            rcg::setEnum (nodemap, "ExposureMode",
            exposureModes[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->exposureMode, "Timed") == 0) {
    for (size_t k = 0; k < exposureModes.size (); k++) {
      if (exposureModes[k] == "Timed") {
        isExposureModeSet =
            rcg::setEnum (nodemap, "ExposureMode",
            exposureModes[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->exposureMode, "TriggerWidth") == 0) {
    for (size_t k = 0; k < exposureModes.size (); k++) {
      if (exposureModes[k] == "TriggerWidth") {
        isExposureModeSet =
            rcg::setEnum (nodemap, "ExposureMode",
            exposureModes[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->exposureMode, "TriggerControlled") == 0) {
    for (size_t k = 0; k < exposureModes.size (); k++) {
      if (exposureModes[k] == "TriggerControlled") {
        isExposureModeSet =
            rcg::setEnum (nodemap, "ExposureMode",
            exposureModes[k].c_str (), false);
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


bool
Genicam::setExposureTime (void)
{
  bool isExposureTimeSet = false;
  double vMin, vMax, expTime;

  (expTime =
      rcg::getFloat (nodemap, "ExposureTime", &vMin, &vMax, false,
          0)) ? expTime : rcg::getFloat (nodemap, "ExposureTimeAbs", &vMin,
      &vMax, false, 0);

  std::string exposureMode = rcg::getEnum (nodemap, "ExposureMode", false);
  std::string exposureAuto = rcg::getEnum (nodemap, "ExposureAuto", false);

  // Proceed only if ExposureMode = Timed and ExposureAuto = Off
  if (exposureMode == "Timed" && exposureAuto == "Off") {

    if (gencamParams->exposureTime < vMin || gencamParams->exposureTime > vMax) {
      std::cout << "WARNING:: Valid ExposureTime is between " << vMin <<
          " and " << vMax << std::endl;
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
              0)) ? expTime : rcg::getFloat (nodemap, "ExposureTimeAbs",
          &vMin, &vMax, false, 0)) << std::endl;

  return isExposureTimeSet;
}


bool
Genicam::setExposureAuto (void)
{
  bool isExposureAutoSet = false;

  // Read supported ExposureAuto modes
  std::vector < std::string > exposureAutos;
  rcg::getEnum (nodemap, "ExposureAuto", exposureAutos, false);

  if (strcasecmp (gencamParams->exposureAuto, "Off") == 0) {
    for (size_t k = 0; k < exposureAutos.size (); k++) {
      if (exposureAutos[k] == "Off") {
        isExposureAutoSet =
            rcg::setEnum (nodemap, "ExposureAuto",
            exposureAutos[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->exposureAuto, "Once") == 0) {
    for (size_t k = 0; k < exposureAutos.size (); k++) {
      if (exposureAutos[k] == "Once") {
        isExposureAutoSet =
            rcg::setEnum (nodemap, "ExposureAuto",
            exposureAutos[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->exposureAuto, "Continuous") == 0) {
    for (size_t k = 0; k < exposureAutos.size (); k++) {
      if (exposureAutos[k] == "Continuous") {
        isExposureAutoSet =
            rcg::setEnum (nodemap, "ExposureAuto",
            exposureAutos[k].c_str (), false);
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


bool
Genicam::setExposureTimeSelector (void)
{
  bool isExposureTimeSelectorSet = false;
  std::vector < std::string > exposureTimeSelectors;

  // Check if ExposureTimeSelector feature is present.
  rcg::getEnum (nodemap, "ExposureTimeSelector", exposureTimeSelectors, false);
  if (exposureTimeSelectors.size () == 0) {
    std::cout <<
        "WARNING: ExposureTimeSelector and ExposureTimeMode not Supported"
        << std::endl;
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


bool
Genicam::setTriggerDivider (void)
{
  bool ret = false;

  // Set Trigger Divider for the incoming Trigger Pulses.
  ret =
      rcg::setInteger (nodemap, "TriggerDivider",
      gencamParams->triggerDivider, false);
  if (!ret)
    std::cout << "WARNING: Trigger Divider set fail" << std::endl;
  else
    std::cout << "Trigger Divider set to " << gencamParams->triggerDivider <<
        std::endl;

  return ret;
}


bool
Genicam::setTriggerMultiplier (void)
{
  bool ret = false;

  // Set Trigger Multiplier for the incoming Trigger Pulses.
  ret =
      rcg::setInteger (nodemap, "TriggerMultiplier",
      gencamParams->triggerMultiplier, false);
  if (!ret)
    std::cout << "WARNING: Trigger Multiplier set fail" << std::endl;
  else
    std::cout << "Trigger Multiplier set to " << gencamParams->triggerMultiplier
        << std::endl;
  return ret;
}


bool
Genicam::setTriggerDelay (void)
{
  bool ret = false;

  // Set Trigger Delay after trigger reception before activating it.
  ret =
      rcg::setFloat (nodemap, "TriggerDelay", gencamParams->triggerDelay, false)
      || rcg::setFloat (nodemap, "TriggerDelayAbs",
      gencamParams->triggerDelay, false);
  if (!ret)
    std::cout << "WARNING: Trigger Delay set fail" << std::endl;
  else
    std::cout << "Trigger Delay set to " << gencamParams->
        triggerDelay << std::endl;
  return ret;
}


bool
Genicam::setTriggerMode (const char *tMode)
{
  bool ret = false;

  // Set the Trigger Mode.
  ret = rcg::setEnum (nodemap, "TriggerMode", tMode, false);

  if (!ret) {
    std::cout << "WARNING:: TriggerMode: " << tMode << " not set\n";
  } else {
    std::cout << "TriggerMode: " << tMode << " set\n";
    triggerMode.assign (tMode);
  }

  return ret;
}


bool
Genicam::setTriggerOverlap (void)
{
  bool isTriggerOverlapSet = false;
  std::vector < std::string > triggerOverlaps;

  // Check if the feature is supported or not.
  rcg::getEnum (nodemap, "TriggerOverlap", triggerOverlaps, false);
  if (triggerOverlaps.size () == 0) {
    std::cout << "WARNING: TriggerOverlap not Supported" << std::endl;
    return isTriggerOverlapSet;
  }

  if (strcasecmp (gencamParams->triggerOverlap, "Off") == 0) {
    for (size_t k = 0; k < triggerOverlaps.size (); k++) {
      if (triggerOverlaps[k] == "Off") {
        rcg::setEnum (nodemap, "TriggerOverlap",
            triggerOverlaps[k].c_str (), false);
        isTriggerOverlapSet = true;
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerOverlap, "ReadOut") == 0) {
    for (size_t k = 0; k < triggerOverlaps.size (); k++) {
      if (triggerOverlaps[k] == "ReadOut") {
        rcg::setEnum (nodemap, "TriggerOverlap",
            triggerOverlaps[k].c_str (), false);
        isTriggerOverlapSet = true;
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerOverlap, "PreviousFrame") == 0) {
    for (size_t k = 0; k < triggerOverlaps.size (); k++) {
      if (triggerOverlaps[k] == "PreviousFrame") {
        rcg::setEnum (nodemap, "TriggerOverlap",
            triggerOverlaps[k].c_str (), false);
        isTriggerOverlapSet = true;
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerOverlap, "PreviousLine") == 0) {
    for (size_t k = 0; k < triggerOverlaps.size (); k++) {
      if (triggerOverlaps[k] == "PreviousLine") {
        rcg::setEnum (nodemap, "TriggerOverlap",
            triggerOverlaps[k].c_str (), false);
        isTriggerOverlapSet = true;
        break;
      }
    }
  } else {
    std::cout << "TriggerOverlap \"" << gencamParams->triggerOverlap <<
        "\" not supported by the Plugin.\n";
    std::cout << "TriggerOverlap supported are below," << std::endl;
    for (size_t k = 0; k < triggerOverlaps.size (); k++)
      std::cout << "    " << triggerOverlaps[k] << std::endl;
  }
  if (!isTriggerOverlapSet) {
    std::cout << "WARNING:: TriggerOverlap set fail" << std::endl;
  }

  std::string triggerOverlap = rcg::getEnum (nodemap, "TriggerOverlap", false);
  std::cout << "TriggerOverlap is \"" << triggerOverlap << "\"" << std::endl;

  return isTriggerOverlapSet;
}


bool
Genicam::setTriggerActivation (void)
{
  bool isTriggerActivationSet = false;
  std::vector < std::string > triggerActivations;

  // Check if the feature is present or not.
  rcg::getEnum (nodemap, "TriggerActivation", triggerActivations, false);
  if (triggerActivations.size () == 0) {
    std::cout << "WARNING: TriggerActivation not Supported" << std::endl;
    return isTriggerActivationSet;
  }

  if (strcasecmp (gencamParams->triggerActivation, "RisingEdge") == 0) {
    for (size_t k = 0; k < triggerActivations.size (); k++) {
      if (triggerActivations[k] == "RisingEdge") {
        rcg::setEnum (nodemap, "TriggerActivation",
            triggerActivations[k].c_str (), false);
        isTriggerActivationSet = true;
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerActivation, "FallingEdge") == 0) {
    for (size_t k = 0; k < triggerActivations.size (); k++) {
      if (triggerActivations[k] == "FallingEdge") {
        rcg::setEnum (nodemap, "TriggerActivation",
            triggerActivations[k].c_str (), false);
        isTriggerActivationSet = true;
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerActivation, "AnyEdge") == 0) {
    for (size_t k = 0; k < triggerActivations.size (); k++) {
      if (triggerActivations[k] == "AnyEdge") {
        rcg::setEnum (nodemap, "TriggerActivation",
            triggerActivations[k].c_str (), false);
        isTriggerActivationSet = true;
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerActivation, "LevelHigh") == 0) {
    for (size_t k = 0; k < triggerActivations.size (); k++) {
      if (triggerActivations[k] == "LevelHigh") {
        rcg::setEnum (nodemap, "TriggerActivation",
            triggerActivations[k].c_str (), false);
        isTriggerActivationSet = true;
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerActivation, "LevelLow") == 0) {
    for (size_t k = 0; k < triggerActivations.size (); k++) {
      if (triggerActivations[k] == "LevelLow") {
        rcg::setEnum (nodemap, "TriggerActivation",
            triggerActivations[k].c_str (), false);
        isTriggerActivationSet = true;
        break;
      }
    }
  } else {
    std::cout << "TriggerActivation \"" << gencamParams->triggerActivation <<
        "\" not supported by the Plugin.\n";
    std::cout << "TriggerActivation supported are below," << std::endl;
    for (size_t k = 0; k < triggerActivations.size (); k++)
      std::cout << "    " << triggerActivations[k] << std::endl;
  }
  if (!isTriggerActivationSet) {
    std::cout << "WARNING:: TriggerActivation set fail" << std::endl;
  }

  std::string triggerActivation =
      rcg::getEnum (nodemap, "TriggerActivation", false);
  std::cout << "TriggerActivation is \"" << triggerActivation << "\"" <<
      std::endl;

  return isTriggerActivationSet;
}


bool
Genicam::setAcquisitionMode (void)
{
  bool isAcquisitionModeSet = false;
  std::vector < std::string > acquisitionModes;

  // std::cout << "frame count " << rcg::setInteger (nodemap, "AcquisitionFrameCount", 20, false) << "\n";

  // Read acquisition modes supported by the camera
  rcg::getEnum (nodemap, "AcquisitionMode", acquisitionModes, false);

  // Iterate the configured acquisition mode with camera supported list
  if (strcasecmp (gencamParams->acquisitionMode, "continuous") == 0) {
    // Continuous mode is default
    for (size_t k = 0; k < acquisitionModes.size (); k++) {
      if (acquisitionModes[k] == "Continuous") {
        isAcquisitionModeSet =
            rcg::setEnum (nodemap, "AcquisitionMode",
            acquisitionModes[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->acquisitionMode, "singleframe") == 0) {
    // Single frame acquisition mode is supported?
    for (size_t k = 0; k < acquisitionModes.size (); k++) {
      if (acquisitionModes[k] == "SingleFrame") {
        isAcquisitionModeSet =
            rcg::setEnum (nodemap, "AcquisitionMode",
            acquisitionModes[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->acquisitionMode, "multiframe") == 0) {
    // Multi frame acquisition mode is supported?
    for (size_t k = 0; k < acquisitionModes.size (); k++) {
      if (acquisitionModes[k] == "MultiFrame") {
        isAcquisitionModeSet =
            rcg::setEnum (nodemap, "AcquisitionMode",
            acquisitionModes[k].c_str (), false);
        break;
      }
    }
  } else {
    std::cout << "WARNING:: Invalid AcquisitionMode: " <<
        gencamParams->acquisitionMode << std::endl;
    return FALSE;
  }

  if (!isAcquisitionModeSet) {
    // Acquisition modes not supported by the camera
    std::cout << "WARNING:: AcquisitionMode: " <<
        gencamParams->acquisitionMode << " not set. ";
    if (acquisitionModes.size () > 0) {
      std::cout << "Supported acquisition modes are," << std::endl;
      for (size_t k = 0; k < acquisitionModes.size (); k++) {
        std::cout << "    " << acquisitionModes[k] << std::endl;
      }
    } else {
      std::cout << "AcquisitionMode feature not supported by the camera.\n";
    }

  }
  // Acquisition Mode set success
  std::string aMode = rcg::getEnum (nodemap, "AcquisitionMode", false);
  std::cout << "AcquisitionMode: \"" << aMode << "\" set\n";
  acquisitionMode.assign (aMode);

  if (aMode == "Continuous") {
    // Set trigger mode Off for Continuous mode
    setTriggerMode ("Off");
  } else {
    // Set trigger mode On for Non-Continuous mode
    setTriggerMode ("On");

    // Set "FrameTriggerWait" to check AcquisitionStatus in Create for TriggerSource = Software
    std::cout << "Set AcquisitionStatusSelector to \"FrameTriggerWait\"" <<
        std::endl;
    rcg::setEnum (nodemap, "AcquisitionStatusSelector",
        "FrameTriggerWait", false);
  }
  return isAcquisitionModeSet;
}


bool
Genicam::setTriggerSoftware (void)
{
  bool ret = false;

  // Procedd only when TriggerSource = Software
  if (triggerSource == "Software") {
    // Execute TriggerSoftware command
    ret = rcg::callCommand (nodemap, "TriggerSoftware", false);
    if (!ret) {
      std::cout << "WARNING:: TriggerSoftware set failed" << std::endl;
    } else {
      std::cout << "Call Command: \"TriggerSoftware\"\n";
    }
  } else {
    std::cout <<
        "WARNING: TriggerSoftware command not trigerred; TriggerSource is not \"Software\""
        << std::endl;
  }

  return ret;
}


bool
Genicam::setTriggerSelector (void)
{
  bool isTriggerSelectorSet = false;
  std::vector < std::string > triggerSelectors;

  // Check if feature is supported or not.
  rcg::getEnum (nodemap, "TriggerSelector", triggerSelectors, false);
  if (triggerSelectors.size () == 00) {
    std::cout << "WARNING: TriggerSelector not Supported" << std::endl;
    return isTriggerSelectorSet;
  }
  // Iterate the configured trigger selector with camera supported list
  if (strcasecmp (gencamParams->triggerSelector, "acquisitionstart") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "AcquisitionStart") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector, "FrameStart") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "FrameStart") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector, "AcquisitionEnd") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "AcquisitionEnd") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector,
          "AcquisitionActive") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "AcquisitionActive") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector, "FrameEnd") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "FrameEnd") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector, "FrameActive") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "FrameActive") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector, "FrameBurstStart") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "FrameBurstStart") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector, "FrameBurstEnd") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "FrameBurstEnd") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector,
          "FrameBurstActive") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "FrameBurstActive") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector, "LineStart") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "LineStart") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector, "ExposureStart") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "ExposureStart") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector, "ExposureEnd") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "ExposureEnd") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector, "ExposureActive") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "ExposureActive") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }
  } else if (strcasecmp (gencamParams->triggerSelector,
          "MultiSlopeExposureLimit1") == 0) {
    for (size_t k = 0; k < triggerSelectors.size (); k++) {
      if (triggerSelectors[k] == "MultiSlopeExposureLimit1") {
        isTriggerSelectorSet =
            rcg::setEnum (nodemap, "TriggerSelector",
            triggerSelectors[k].c_str (), false);
        break;
      }
    }

  } else {
    std::cout << "WARNING: TriggerSelector \"" << gencamParams->triggerSelector
        << "\" not supported by the camera.\n";
    std::cout << "TriggerSelector supported are below," << std::endl;
    for (size_t k = 0; k < triggerSelectors.size (); k++)
      std::cout << "    " << triggerSelectors[k] << std::endl;
  }

  if (!isTriggerSelectorSet) {
    std::cout << "WARNING:: TriggerSelector set fail" << std::endl;
  }

  std::string triggerSelector =
      rcg::getEnum (nodemap, "TriggerSelector", false);
  std::cout << "TriggerSelector is \"" << triggerSelector << "\"" << std::endl;

  return isTriggerSelectorSet;
}


bool
Genicam::setTriggerSource (void)
{
  bool isTriggerSourceSet = false;
  std::vector < std::string > triggerSources;

  // Check if feature is supported or not.
  rcg::getEnum (nodemap, "TriggerSource", triggerSources, false);
  if (triggerSources.size () == 0) {
    std::cout << "WARNING:: TriggerSource not Supported" << std::endl;
    return isTriggerSourceSet;
  }
  // Proceed only if TriggerMode is ON
  if (triggerMode == "On") {
    isTriggerSourceSet =
        rcg::setEnum (nodemap, "TriggerSource", gencamParams->triggerSource,
        false);
    if (!isTriggerSourceSet) {
      std::cout << "WARNING:: TriggerSource \"" << gencamParams->
          triggerSource << "\" not supported by the camera\n";
      std::cout << "TriggerSource supported are below," << std::endl;
      for (size_t k = 0; k < triggerSources.size (); k++)
        std::cout << "    " << triggerSources[k] << std::endl;
    }
  } else {
    std::cout <<
        "WARNING: Trigger Source not configured; TriggerMode is not \"On\""
        << std::endl;
  }

  std::string tSource = rcg::getEnum (nodemap, "TriggerSource", false);
  std::cout << "TriggerSource is \"" << tSource << "\"" << std::endl;
  triggerSource.assign (tSource);

  return isTriggerSourceSet;
}
