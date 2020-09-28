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

  triggerMode.assign ("Off\0");
  deviceLinkThroughputLimitMode.assign ("Off\0");

  return TRUE;
}


bool
Genicam::Start (void)
{
  /* Enumerate & open device.
     Set the property (resolution, pixel format, etc.,)
     allocate buffers and start streaming from the camera */

  /* Get Serial Number */
  if (gencamParams->deviceSerialNumber == NULL) {
    getCameraSerialNumber ();
  }

  dev = rcg::getDevice (gencamParams->deviceSerialNumber);

  if (dev) {
    //TODO : Logging 

    dev->open (rcg::Device::CONTROL);
    std::cout << "Camera: " << gencamParams->deviceSerialNumber <<
        " opened successfully.\n";

    nodemap = dev->getRemoteNodeMap ();

    getCameraInfo ();

    // get chunk adapter (this switches chunk mode on if possible and
    // returns a null pointer if this is not possible)
    std::shared_ptr < GenApi::CChunkAdapter > chunkadapter = 0;

    try {
      // DeviceReset feature
      if (gencamParams->deviceReset == true) {
        return resetDevice ();
      }
      // Device Clock Freequency feature
      if (gencamParams->deviceClockFrequency > -1) {
        setDeviceClockFrequency ();
      }
      // Binning selector feature
      if (gencamParams->binningSelector) {
        setBinningSelector ();
      }
      // Binning horizontal mode feature
      if (gencamParams->binningHorizontalMode) {
        setBinningHorizontalMode ();
      }
      // Binning Horizontal feature
      if (gencamParams->binningHorizontal > 0) {
        setBinningHorizontal ();
      }
      // Binning Vertical mode feature
      if (gencamParams->binningVerticalMode) {
        setBinningVerticalMode ();
      }
      // Binning Vertical feature
      if (gencamParams->binningVertical > 0) {
        setBinningVertical ();
      }
      // Decimation Horizontal feature
      if (gencamParams->decimationHorizontal > 0) {
        setDecimationHorizontal ();
      }
      // Decimation Vertical feature
      if (gencamParams->decimationVertical > 0) {
        setDecimationVertical ();
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

    /* Configure other features below,
       failure of which doesn't require pipeline to be reconnected
     */
    {
      // OffsetX and OffsetY feature
      if (offsetXYwritable) {
        setOffsetXY ();
      }
      /* DeviceLinkThroughputLimit and Mode features */
      if (gencamParams->deviceLinkThroughputLimit > 0) {
        setDeviceLinkThroughputLimit ();
      }
      // Acquisition Frame Rate feature
      // This has to be after throughput limit as that has impact on this
      setAcquisitionFrameRate ();
      // Acquisition Mode feature
      if (gencamParams->acquisitionMode) {
        setAcquisitionMode ();
      }
      // Trigger Selector feature
      if (gencamParams->triggerSelector) {
        setTriggerSelector ();
      }
      // Trigger Activation feature
      if (gencamParams->triggerActivation) {
        setTriggerActivation ();
      }
      // Trigger Source feature, needs trigger mode on
      if (gencamParams->triggerSource) {
        setTriggerSource ();
      }
      // Trigger Multiplier feature
      if (gencamParams->triggerMultiplier > 0) {
        setTriggerMultiplier ();
      }
      // Trigger Divider feature
      if (gencamParams->triggerDivider > 0) {
        setTriggerDivider ();
      }
      // TriggerDelay feature
      if (gencamParams->triggerDelay > -1) {
        setTriggerDelay ();
      }
      // Trigger overlap feature
      if (gencamParams->triggerOverlap) {
        setTriggerOverlap ();
      }
      // Exposure Mode feature
      if (gencamParams->exposureMode)
        setExposureMode ();

      // Exposure Auto feature
      if (gencamParams->exposureAuto)
        setExposureAuto ();

      // Balance White Auto Feature
      if (gencamParams->balanceWhiteAuto)
        setBalanceWhiteAuto ();

      // Balance Ratio Feature
      if (gencamParams->balanceRatio != 9999.0)
        setBalanceRatio ();

      // Exposure Time Selector feature
      if (gencamParams->exposureTimeSelector)
        setExposureTimeSelector ();

      // Exposure Time feature
      /* Needs ExposureMode = Timed and ExposureAuto = Off */
      if (gencamParams->exposureTime > -1)
        setExposureTime ();

      // Black Level Selector Feature
      if (gencamParams->blackLevelSelector)
        setBlackLevelSelector ();

      // Gamma Feature
      if (gencamParams->gamma > 0.0)
        setGamma ();

      // Black Level Auto Feature
      if (gencamParams->blackLevelAuto)
        setBlackLevelAuto ();

      // Black Level Feature
      if (gencamParams->blackLevel != 9999.0)
        setBlackLevel ();

      // Gain selector feature
      if (gencamParams->gainSelector) {
        setGainSelector ();
      }
      // Gain auto feature
      if (gencamParams->gainAuto) {
        setGainAuto ();
      }
      // Gain feature
      if (gencamParams->gain != 9999.0) {
        setGain ();
      }
      // Gain auto balance feature
      if (gencamParams->gainAutoBalance) {
        setGainAutoBalance ();
      }
      // StreamChannelPacketSize feature
      if (gencamParams->channelPacketSize > 0) {
        setChannelPacketSize ();
      }
      // StreamChannelPacketDelay feature
      if (gencamParams->channelPacketDelay > -1) {
        setChannelPacketDelay ();
      }
    }

    /* Check if AcquisitionStatus feature is present for
       non-continuous mode operation in "Create" */
    isAcquisitionStatusFeature = isFeature ("AcquisitionStatus\0", NULL);

    stream = dev->getStreams ();
    if (stream.size () > 0) {
      // opening first stream
      stream[0]->open ();
      stream[0]->startStreaming ();

      if (acquisitionMode != "Continuous" && triggerMode == "On") {
        if (triggerSource == "Software") {
          setTriggerSoftware ();
        } else {
          // validating the hw trigger timeout here, ensuring min value is 1 sec
          gencamParams->hwTriggerTimeout =
              (gencamParams->hwTriggerTimeout <=
              0) ? 10 : gencamParams->hwTriggerTimeout;
        }
      } else if (acquisitionMode == "Continuous" && triggerMode == "Off") {
        // Setting this to 0 in case user has configured it
        gencamParams->hwTriggerTimeout = 0;
      }
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
    // Stop and close the streams opened
    if (stream.size () > 0) {
      stream[0]->stopStreaming ();
      stream[0]->close ();
    }
    // Close the device opened
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

  // Clear the system
  rcg::System::clearSystems ();

  return TRUE;
}


bool Genicam::Create (GstBuffer ** buf, GstMapInfo * mapInfo)
{
  /* Grab the buffer, copy and release, set framenum */
  int
      hwTriggerCheck = 0;
  try {
#ifdef RANDOM_RGB_TEST
    /* TODO Remove later. It is dummy creating and filling buffer for RGB */
    guint
        globalSize = gencamParams->width * gencamParams->height * 3;
#else
    const
        rcg::Buffer *
        buffer;

    while (!(buffer = stream[0]->grab (1000))) {
      if (acquisitionMode != "Continuous" && triggerMode == "On"
          && triggerSource != "Software") {
        // If Hw trigger, wait for specified timeout
        std::
            cout << "Waiting for a Trigger (" << (gencamParams->hwTriggerTimeout
            - ++hwTriggerCheck) << " sec)...\n";
      }
      if (hwTriggerCheck == gencamParams->hwTriggerTimeout) {
        std::cerr << "No frame received from the camera\n";
        return FALSE;
      }
    }
    guint
        globalSize = buffer->getGlobalSize ();
    guint64
        timestampNS = buffer->getTimestampNS ();
#endif

    *buf = gst_buffer_new_allocate (NULL, globalSize, NULL);
    if (*buf == NULL) {
      std::cerr << "Buffer couldn't be allocated\n";
      return FALSE;
    }
    GST_BUFFER_PTS (*buf) = timestampNS;
    gst_buffer_map (*buf, mapInfo, GST_MAP_WRITE);

#ifdef RANDOM_RGB_TEST
    gen_random_rgb (mapInfo->data, mapInfo->size);
#else
    memcpy (mapInfo->data, buffer->getGlobalBase (), mapInfo->size);

    // For Non continuous modes, execute TriggerSoftware command
    if (acquisitionMode != "Continuous") {
      stream[0]->stopStreaming ();
      stream[0]->startStreaming ();

      // TODO handle multi frame, needs separate frame count for that
      if (triggerMode == "On" && triggerSource == "Software") {
        // If "AcquisitionStatus" feature is present, check the status
        while (!(rcg::getBoolean (nodemap, "AcquisitionStatus", false, false))
            && isAcquisitionStatusFeature);
        setTriggerSoftware ();
      }
    }
#endif
    return TRUE;
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
Genicam::isFeature (const char *featureName, featureType * fType)
{
  bool ret = true;
  char featureStr[32] = "Feature not found\0";
  char typeStr[MAX_TYPE][32] = { "\0",  // NO_TYPE
    "Feature not enumeration\0",        // TYPE_ENUM
    "Feature not integer\0",    // TYPE_INT
    "Feature not float\0",      // TYPE_FLOAT
    "Feature not boolean\0",    // TYPE_BOOL
    "Feature of unknown datatype\0",    // TYPE_STRING
    "\0"
  };

  if (fType != NULL)
    *fType = TYPE_NO;

  try {
    if (fType != NULL)
      *fType = TYPE_ENUM;
    rcg::getEnum (nodemap, featureName, true);
  }
  catch (const std::exception & ex)
  {
    if (strncmp (ex.what (), featureStr, strlen (featureStr)) == 0) {
      // if feature not present, then no need to check rest just return
      ret = false;
      if (fType != NULL)
        *fType = TYPE_NO;
      return ret;
    }
    if (strncmp (ex.what (), typeStr[TYPE_ENUM],
            strlen (typeStr[TYPE_ENUM])) == 0) {
      if (fType != NULL)
        *fType = TYPE_NO;
    }
  }
  if (fType && *fType != TYPE_NO) {
    // return as type found
    return ret;
  } else if (fType == NULL) {
    // only feature existence needs to be checked.
    // Feature type can be ignored, so return
    return ret;
  }

  /* Check if feature is not a Integer */
  try {
    if (fType != NULL)
      *fType = TYPE_INT;
    rcg::getInteger (nodemap, featureName, NULL, NULL, true, false);
  }
  catch (const std::exception & ex)
  {
    if (strncmp (ex.what (), typeStr[TYPE_INT],
            strlen (typeStr[TYPE_INT])) == 0) {
      if (fType != NULL)
        *fType = TYPE_NO;
    }
  }
  if (fType && *fType != TYPE_NO) {
    // return as type found
    return ret;
  }

  /* Check if feature is not a float */
  try {
    if (fType != NULL)
      *fType = TYPE_FLOAT;
    rcg::getFloat (nodemap, featureName, NULL, NULL, true, false);
  }
  catch (const std::exception & ex)
  {
    if (strncmp (ex.what (), typeStr[TYPE_FLOAT],
            strlen (typeStr[TYPE_FLOAT])) == 0) {
      if (fType != NULL)
        *fType = TYPE_NO;
    }
  }
  if (fType && *fType != TYPE_NO) {
    // return as type found
    return ret;
  }

  /* Check if feature is not a boolean */
  try {
    if (fType != NULL)
      *fType = TYPE_BOOL;
    rcg::getBoolean (nodemap, featureName, true, false);
  }
  catch (const std::exception & ex)
  {
    if (strncmp (ex.what (), typeStr[TYPE_BOOL],
            strlen (typeStr[TYPE_BOOL])) == 0) {
      if (fType != NULL)
        *fType = TYPE_NO;
    }
  }
  if (fType && *fType != TYPE_NO) {
    // return as type found
    return ret;
  }

  /* Check if feature is not a string */
  try {
    if (fType != NULL)
      *fType = TYPE_STRING;
    rcg::getString (nodemap, featureName, true, false);
  }
  catch (const std::exception & ex)
  {
    if (strncmp (ex.what (), typeStr[TYPE_STRING],
            strlen (typeStr[TYPE_STRING])) == 0) {
      if (fType != NULL)
        *fType = TYPE_NO;
    }
  }
  if (fType && *fType == TYPE_NO) {
    // Only option left is CMD type
    *fType = TYPE_CMD;
  }

  return ret;
}

bool
Genicam::setEnumFeature (const char *featureName, const char *str,
    const bool ex)
{
  bool isEnumFeatureSet = false, matchFound = false;
  std::vector < std::string > featureList;

  if (featureName == NULL || str == NULL) {
    std::cout << "ERROR:: Enter valid feature and mode \n";
    return isEnumFeatureSet;
  }
  try {
    // Read the featureName supported
    rcg::getEnum (nodemap, featureName, featureList, ex);

    // Check if list is empty
    if (featureList.size () == 0) {
      std::cout << "WARNING:: " << featureName <<
          ": list empty, writing not supported" << std::endl;
      return isEnumFeatureSet;
    }

    for (size_t k = 0; k < featureList.size (); k++) {
      // Iterate all possible values if it matches
      if (strcasecmp (str, featureList[k].c_str ()) == 0) {
        matchFound = true;
        isEnumFeatureSet =
            rcg::setEnum (nodemap, featureName, featureList[k].c_str (), ex);
        break;
      }
    }
  }
  catch (const std::exception & ex)
  {
    std::cout << "Exception: " << ex.what () << std::endl;
  } catch ( ...) {
    std::cerr << "Exception: unknown" << std::endl;
  }

  if (!matchFound) {
    // User parameter did not match
    std::cout << "WARNING:: Invalid " << featureName << ": \"" << str <<
        "\"" << ". Supported list below:" << std::endl;
    for (size_t k = 0; k < featureList.size (); k++) {
      std::cout << "    " << featureList[k] << std::endl;
    }
    std::cout << "  " << featureName << " is \"" << rcg::getEnum (nodemap,
        featureName, false) << "\"" << std::endl;
  } else if (matchFound && !isEnumFeatureSet) {
    // Command failed
    std::cout << "WARNING:: " << featureName << ": \"" << str <<
        "\" set failed.\n";
  } else {
    // Command passed
    std::string featureStr = rcg::getEnum (nodemap, featureName, false);
    std::cout << featureName << ": \"" << featureStr << "\" set successful.\n";
  }

  return isEnumFeatureSet;
}


bool
Genicam::setIntFeature (const char *featureName, int *val, const bool ex)
{
  bool isIntFeatureSet = false;
  int64_t vMin, vMax, vInc;
  int64_t diff;

  rcg::getInteger (nodemap, featureName, &vMin, &vMax, &vInc, false, false);

  /* Value of integer should be aligned such that difference of value and Min
   * should be a factor of vInc
   */
  if (*val > vMin) {
    diff = *val - vMin;
    *val -= (diff % vInc);
  }
  // check Range and cap it if needed
  if (*val < vMin) {
    std::cout << "WARNING:: " << featureName << ": value " << *val <<
        " capping near minimum " << vMin << std::endl;
    // Increase the value to vMin so that  "value-vMin" is a factor of "vInc".
    *val = vMin;
  } else if (*val > vMax) {
    std::cout << "WARNING:: " << featureName << ": value " << *val <<
        " capping near maximum " << vMax << std::endl;
    // Decrease the value if difference between "value-vMin" is not a factor of "vInc".
    diff = vMax - vMin;
    *val = vMax - (diff % vInc);
  }
  // Configure Int feature
  try {
    isIntFeatureSet = rcg::setInteger (nodemap, featureName, *val, ex);
  }
  catch (const std::exception & ex)
  {
    std::cout << "Exception: " << ex.what () << std::endl;
  }

  if (!isIntFeatureSet) {
    // Command failed
    std::cout << "WARNING:: " << featureName << ": " << *val <<
        " set failed.\n";
  } else {
    // Command passed
    int ret =
        rcg::getInteger (nodemap, featureName, NULL, NULL, NULL, false, false);
    std::cout << featureName << ": " << ret << " set successful.\n";
  }

  return isIntFeatureSet;
}


bool
Genicam::setFloatFeature (const char *featureName, float *val, const bool ex)
{
  bool isFloatFeatureSet = false;
  double vMin, vMax;

  rcg::getFloat (nodemap, featureName, &vMin, &vMax, false, false);

  // check Range and cap it if needed
  if (*val < vMin) {
    std::cout << "WARNING:: " << featureName << ": value " << *val <<
        " capping near minimum " << vMin << std::endl;
    *val = vMin;
  } else if (*val > vMax) {
    std::cout << "WARNING:: " << featureName << ": value " << *val <<
        " capping near maximum " << vMax << std::endl;
    *val = vMax;
  }
  // Configure Float feature
  try {
    isFloatFeatureSet = rcg::setFloat (nodemap, featureName, *val, ex);
  }
  catch (const std::exception & ex)
  {
    std::cout << "Exception: " << ex.what () << std::endl;
  }

  if (!isFloatFeatureSet) {
    // Command failed
    std::cout << "WARNING:: " << featureName << ": " << *val <<
        " set failed.\n";
  } else {
    // Command passed
    float ret = rcg::getFloat (nodemap, featureName, NULL, NULL, false, false);
    std::cout << featureName << ": " << ret << " set successful.\n";
  }

  return isFloatFeatureSet;
}


void
Genicam::getCameraInfo (void)
{
  if (isFeature ("DeviceVendorName\0", NULL)) {
    camInfo.vendorName = rcg::getString (nodemap, "DeviceVendorName", 0, 0);
    std::cout << "Camera Vendor: " << camInfo.vendorName << std::endl;
  }
  if (isFeature ("DeviceModelName\0", NULL)) {
    camInfo.modelName = rcg::getString (nodemap, "DeviceModelName", 0, 0);
    std::cout << "Camera Model: " << camInfo.modelName << std::endl;
  }
}


bool
Genicam::getCameraSerialNumber (void)
{
  // get all systems
  std::vector < std::shared_ptr < rcg::System > >system =
      rcg::System::getSystems ();

  for (size_t i = 0; i < system.size (); i++) {
    // Open systems, and get all interfaces
    system[i]->open ();
    std::vector < std::shared_ptr < rcg::Interface > >interf =
        system[i]->getInterfaces ();
    for (size_t k = 0; k < interf.size (); k++) {
      // Open interfaces, and get all cameras
      interf[k]->open ();
      std::vector < std::shared_ptr < rcg::Device > >device =
          interf[k]->getDevices ();
      for (size_t j = 0; j < device.size (); j++) {
        // Check for duplicate serials
        bool match = std::find (serials.begin (), serials.end (),
            device[j]->getSerialNumber ()) != serials.end ();
        if (!match) {
          serials.push_back (device[j]->getSerialNumber ());
          std::cout << "> Camera found with Serial# " <<
              device[j]->getSerialNumber () << std::endl;
        }
      }
      // Close interfaces
      interf[k]->close ();
    }
    // Close systems
    system[i]->close ();
  }

  if (serials.size () == 0) {
    // No cameras found
    std::cout << "ERROR: No Cameras found." << std::endl;
  } else {
    // Connect to the first camera found
    std::cout << "Connecting to camera: " << serials[0].c_str () << std::endl;
    gencamParams->deviceSerialNumber = serials[0].c_str ();
  }

  return (!serials.empty ());
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

  //Possible values : Sensor, Region0, Region1, Region2
  isBinningSelectorSet =
      setEnumFeature ("BinningSelector\0", gencamParams->binningSelector, true);
  return isBinningSelectorSet;
}


bool
Genicam::setBinningHorizontalMode (void)
{
  bool isBinningHorizontalModeSet = false;

  std::vector < std::string > binningHorizontalModes;

  // Read binning engines supported by the camera
  rcg::getEnum (nodemap, "BinningHorizontalMode", binningHorizontalModes,
      false);
  if (binningHorizontalModes.empty ()) {
    // Handle variations, deviations from SFNC standard
    rcg::getEnum (nodemap, "BinningModeHorizontal", binningHorizontalModes,
        false);
  }
  // Iterate the configured binning horizontal mode with camera supported list
  if (strcasecmp (gencamParams->binningHorizontalMode, "sum") == 0) {
    // Sum binning horizontal mode is supported?
    for (size_t k = 0; k < binningHorizontalModes.size (); k++) {
      // Summing is a deviation but some cameras use
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
      // Averaging is a deviation but some cameras use
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
    std::cout << "BinningHorizontalMode: \"" <<
        gencamParams->binningHorizontalMode << "\" set successful.\n";
  } else {
    // Binning horizontal modes not supported by the camera
    std::cout << "WARNING:: BinningHorizontalMode: \"" <<
        gencamParams->binningHorizontalMode << "\" not set. ";
    if (binningHorizontalModes.size () > 0) {
      std::cout << "Supported binning horizontal modes are," << std::endl;
      for (size_t k = 0; k < binningHorizontalModes.size (); k++) {
        std::cout << "    " << binningHorizontalModes[k] << std::endl;
      }
    } else {
      std::cout << "Feature not supported\n";
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
        setIntFeature ("BinningHorizontal\0", &gencamParams->binningHorizontal,
        false);
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
  if (binningVerticalModes.empty ()) {
    // Handle deviations from SFNC standard
    rcg::getEnum (nodemap, "BinningModeVertical", binningVerticalModes, false);
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

  } else if (strcasecmp (gencamParams->binningVerticalMode, "average") == 0) {
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
    std::cout << "BinningVerticalMode: \"" << gencamParams->binningVerticalMode
        << "\" set successful.\n";
  } else {
    // Binning vertical modes not supported by the camera
    std::cout << "WARNING:: BinningVerticalMode: \"" <<
        gencamParams->binningVerticalMode << "\" not set. ";
    if (binningVerticalModes.size () > 0) {
      std::cout << "Supported binning vertical modes are," << std::endl;
      for (size_t k = 0; k < binningVerticalModes.size (); k++) {
        std::cout << "    " << binningVerticalModes[k] << std::endl;
      }
    } else {
      std::cout << "Feature not supported\n";
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
        setIntFeature ("BinningVertical\0", &gencamParams->binningVertical,
        false);
  }

  return ret;
}


bool
Genicam::setDecimationHorizontal (void)
{
  bool isDecimationHorizontalSet = false;

  // Is DecimationHorizontal supported? if not then return
  if (!isFeature ("DecimationHorizontal\0", NULL)) {
    std::cout << "WARNING:: DecimationHorizontal: feature not supported\n";
    return isDecimationHorizontalSet;
  }
  // Configure Decimation
  isDecimationHorizontalSet =
      setIntFeature ("DecimationHorizontal\0",
      &gencamParams->decimationHorizontal, true);

  return isDecimationHorizontalSet;
}


bool
Genicam::setDecimationVertical (void)
{
  bool isDecimationVerticalSet = false;

  // Is DecimationVertical supported? if not then return
  if (!isFeature ("DecimationVertical\0", NULL)) {
    std::cout << "WARNING:: DecimationVertical: feature not supported\n";
    return isDecimationVerticalSet;
  }
  // Configure Decimation
  isDecimationVerticalSet =
      setIntFeature ("DecimationVertical\0",
      &gencamParams->decimationVertical, true);

  return isDecimationVerticalSet;
}


bool
Genicam::setPixelFormat (void)
{
  bool isPixelFormatSet = false;
  std::vector < std::string > pixelFormats;

  // Read the pixel formats supported by the camera
  rcg::getEnum (nodemap, "PixelFormat", pixelFormats, true);

  // Iterate the configured format with camera supported list
  // Mapping necessary from FOURCC to GenICam SFNC / PFNC

  if (strcasecmp (gencamParams->pixelFormat, "mono8") == 0) {
    // Check Mono8 / GRAY8 / Y8 is supported by the camera
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "Mono8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcasecmp (gencamParams->pixelFormat, "ycbcr411_8") == 0) {
    // I420 / YUV420 / YCbCr411 8 bit supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "YCbCr411_8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcasecmp (gencamParams->pixelFormat, "ycbcr422_8") == 0) {
    // YUY2 / YUV422 / Ycbcr422 8 bit supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "YUV422_8"
          || pixelFormats[k] == "YUV422_YUYV_Packed"
          || pixelFormats[k] == "YCbCr422_8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcasecmp (gencamParams->pixelFormat, "bayerbggr") == 0) {
    // BayerBG8 is supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "BayerBG8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcasecmp (gencamParams->pixelFormat, "bayerrggb") == 0) {
    // BayerRG8 supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "BayerRG8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcasecmp (gencamParams->pixelFormat, "bayergrbg") == 0) {
    // BayerBG8 supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "BayerGR8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcasecmp (gencamParams->pixelFormat, "bayergbrg") == 0) {
    // BayerGB8 supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "BayerGB8") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcasecmp (gencamParams->pixelFormat, "rgb8") == 0) {
    // RGB8, 24 bit supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "RGB8" || pixelFormats[k] == "RGB8Packed") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);
        isPixelFormatSet = true;
        break;
      }
    }

  } else if (strcasecmp (gencamParams->pixelFormat, "bgr8") == 0) {
    // BGR8, 24 bit supported by the camera?
    for (size_t k = 0; k < pixelFormats.size (); k++) {
      if (pixelFormats[k] == "BGR8" || pixelFormats[k] == "BGR8Packed") {
        rcg::setEnum (nodemap, "PixelFormat", pixelFormats[k].c_str (), true);
        isPixelFormatSet = true;
        break;
      }
    }
  }

  if (isPixelFormatSet) {
    // Format set success
    std::cout << "PixelFormat: \"" << rcg::getEnum (nodemap,
        "PixelFormat") << "\" set successful.\n";
    if (isFeature ("PixelSize\0", NULL)) {
      std::cout << "PixelSize: \"" << rcg::getEnum (nodemap, "PixelSize",
          false) << "\" set\n";
    }
  } else {
    // Format is not supported by the camera, terminate
    std::cout << "PixelFormat: \"" << gencamParams->pixelFormat <<
        "\" not supported by the camera\n";
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
  int64_t vMinX, vMaxX;
  int64_t vMinY, vMaxY;
  char str[32] = "Feature not writable";
  bool isWidthHeightSet = false;

  // Write Offsets = 0, so that resolution can be set first smoothly
  // Offsets will be configured later
  // Also, check if offset is a writable feature. If not, ignore setting it later
  try {
    offsetXYwritable = true;
    rcg::setInteger (nodemap, "OffsetX", 0, true);
    rcg::setInteger (nodemap, "OffsetY", 0, true);
  } catch (const std::exception & ex)
  {
    // Feature not writable
    if (strncmp (ex.what (), str, strlen (str)) == 0) {
      offsetXYwritable = false;
      std::cout << "WARNING:: OffsetX and OffsetY not writable" << std::endl;
    }
  }

  // Print Max resolution supported by camera
  widthMax = rcg::getInteger (nodemap, "WidthMax", NULL, NULL, false, 0);
  heightMax = rcg::getInteger (nodemap, "HeightMax", NULL, NULL, false, 0);

  std::cout << "Maximum resolution supported by Camera: " << widthMax << " x "
      << heightMax << std::endl;

  // Maximum Width check
  rcg::getInteger (nodemap, "Width", &vMinX, &vMaxX, false, false);
  if (gencamParams->width > vMaxX) {
    // Align the width to 4
    gencamParams->width = ROUNDED_DOWN (vMaxX, 0x4 - 1);
    std::
        cout << "WARNING:: Width: capping to maximum " << gencamParams->width <<
        std::endl;
  }
  // Maximum Height check
  rcg::getInteger (nodemap, "Height", &vMinY, &vMaxY, false, false);
  if (gencamParams->height > vMaxY) {
    // Align the height to 4
    gencamParams->height = ROUNDED_DOWN (vMaxY, 0x4 - 1);
    std::cout << "WARNING:: Height: capping to maximum " << gencamParams->height
        << std::endl;
  }

  isWidthHeightSet =
      rcg::setInteger (nodemap, "Width", gencamParams->width, true);
  isWidthHeightSet |=
      rcg::setInteger (nodemap, "Height", gencamParams->height, true);

  if (isWidthHeightSet) {
    std::cout << "Current resolution: " << rcg::getInteger (nodemap, "Width",
        NULL, NULL, false, true) << " x " << rcg::getInteger (nodemap, "Height",
        NULL, NULL, false, true) << " set successful.\n";
  } else {
    std::cout << "ERROR:: Width and Height set error" << std::endl;
    Stop ();
  }

  return isWidthHeightSet;
}


bool
Genicam::setOffsetXY (void)
{
  bool isOffsetXYset = false;

  isOffsetXYset = setIntFeature ("OffsetX\0", &gencamParams->offsetX, true);
  isOffsetXYset |= setIntFeature ("OffsetY\0", &gencamParams->offsetY, true);

  return isOffsetXYset;
}


bool
Genicam::setAcquisitionFrameRate (void)
{
  // AcquisitionFrameRateEnable and AcquisitionFrameRate feature
  bool isFrameRateSet = false;
  float frameRate;
  double vMin, vMax;
  char frameRateString[32];

  if (isFeature ("AcquisitionFrameRate\0", NULL)) {
    strncpy (frameRateString, "AcquisitionFrameRate\0",
        sizeof ("AcquisitionFrameRate\0"));
  } else if (isFeature ("AcquisitionFrameRateAbs\0", NULL)) {
    strncpy (frameRateString, "AcquisitionFrameRateAbs\0",
        sizeof ("AcquisitionFrameRateAbs\0"));
  } else {
    std::cout << "WARNING:: AcquisitionFrameRate: feature not supported" <<
        std::endl;
  }

  frameRate =
      rcg::getFloat (nodemap, frameRateString, &vMin, &vMax, false, false);

  // Incase of no input, read current framerate and set it again
  if (gencamParams->acquisitionFrameRate == 0) {
    gencamParams->acquisitionFrameRate = frameRate;
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
        setFloatFeature (frameRateString, &gencamParams->acquisitionFrameRate,
        true);
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

  // Possible values: Off, Timed, TriggerWidth, TriggerControlled
  isExposureModeSet =
      setEnumFeature ("ExposureMode\0", gencamParams->exposureMode, false);
  return isExposureModeSet;
}


bool
Genicam::setExposureTime (void)
{
  bool isExposureTimeSet = false;
  char exposureTimeStr[32];

  if (isFeature ("ExposureTime\0", NULL)) {
    strncpy (exposureTimeStr, "ExposureTime", sizeof ("ExposureTime\0"));
  } else if (isFeature ("ExposureTimeAbs\0", NULL)) {
    strncpy (exposureTimeStr, "ExposureTimeAbs", sizeof ("ExposureTimeAbs\0"));
  } else {
    std::cout << "WARNING:: ExposureTime: feature not supported\n";
    return isExposureTimeSet;
  }

  std::string exposureMode = rcg::getEnum (nodemap, "ExposureMode", false);
  std::string exposureAuto = rcg::getEnum (nodemap, "ExposureAuto", false);

  // Proceed only if ExposureMode = Timed and ExposureAuto = Off
  if (exposureMode != "Timed" || exposureAuto != "Off") {
    std::cout <<
        "WARNING:: ExposureTime not set, exposureMode must be \"Timed\"" <<
        " and exposureAuto must be \"Off\"" << std::endl;
    return isExposureTimeSet;
  }

  isExposureTimeSet =
      setFloatFeature (exposureTimeStr, &gencamParams->exposureTime, false);

  return isExposureTimeSet;
}


bool
Genicam::setBlackLevelSelector (void)
{
  bool isBlackLevelSelectorSet = false;

  // check if blackLevelSelector is present
  if (isFeature ("BlackLevelSelector\0", NULL) == false) {
    std::cout << "WARNING:: BlackLevelSelector: feature not supported\n";
    return isBlackLevelSelectorSet;
  }
  // Possible values: All, Red, Green, Blue, Y, U, V, Tap1, Tap2...
  isBlackLevelSelectorSet =
      setEnumFeature ("BlackLevelSelector\0", gencamParams->blackLevelSelector,
      false);
  return isBlackLevelSelectorSet;
}


bool
Genicam::setBlackLevelAuto (void)
{
  bool isBlackLevelAutoSet = false;

  // check if blackLevelSelector is present
  if (!isFeature ("BlackLevelAuto\0", NULL)) {
    std::cout << "WARNING:: BlackLevelAuto: feature not supported\n";
    return isBlackLevelAutoSet;
  }
  // Possible values: Off, Once, Continuous
  isBlackLevelAutoSet =
      setEnumFeature ("BlackLevelAuto\0", gencamParams->blackLevelAuto, false);
  // if success, assigned value can be later checked in BlackLevel
  std::string str = rcg::getEnum (nodemap, "BlackLevelAuto", false);
  blackLevelAuto.assign (str);

  return isBlackLevelAutoSet;
}


bool
Genicam::setBlackLevel (void)
{
  bool isBlackLevelSet = false;
  char blackLevelStr[32];
  featureType fType = TYPE_NO, fTypeTemp;

  // Proceed only if BlackLevelAuto is "Off"
  if ((isFeature ("BlackLevelAuto\0", NULL)) && (blackLevelAuto != "Off")
      && (blackLevelAuto != "")) {
    std::cout <<
        "WARNING: BlackLevel not set, BlackLevelAuto should be \"Off\"\n";
    return isBlackLevelSet;
  }
  // Enable the blacklevel enable bit in case if it is present
  if (isFeature ("BlackLevelEnabled\0", NULL)) {
    rcg::setBoolean (nodemap, "BlackLevelEnabled", 1, false);
  }
  // Check the feature string and if the feature is int or float
  if (isFeature ("BlackLevel\0", &fTypeTemp)) {
    strncpy (blackLevelStr, "BlackLevel\0", sizeof ("BlackLevel\0"));
    fType = fTypeTemp;
  } else if (isFeature ("BlackLevelRaw\0", &fTypeTemp)) {
    strncpy (blackLevelStr, "BlackLevelRaw\0", sizeof ("BlackLevelRaw\0"));
    fType = fTypeTemp;
  } else {
    // Feature not found return
    std::cout << "WARNING:: BlackLevel: feature not supported\n";
    return isBlackLevelSet;
  }

  // If the feature type is int
  if (fType == TYPE_INT) {
    isBlackLevelSet =
        setIntFeature (blackLevelStr, (int *) (&gencamParams->blackLevel),
        false);
  }
  // If the feature type is float
  if (fType == TYPE_FLOAT) {
    isBlackLevelSet =
        setFloatFeature (blackLevelStr, &gencamParams->blackLevel, false);
  }

  return isBlackLevelSet;
}


bool
Genicam::setGamma (void)
{
  bool isGammaSet = false;
  std::string gammaSelector;

  // Check if Gamma feature is present
  if (!isFeature ("Gamma\0", NULL)) {
    std::cout << "WARNING:: Gamma: feature not supported" << std::endl;
    return isGammaSet;
  }
  // Set GammaSelector feature
  if (gencamParams->gammaSelector) {
    if (isFeature ("GammaSelector\0", NULL)) {
      // Possible valuse: sRGB, User
      setEnumFeature ("GammaSelector\0", gencamParams->gammaSelector, false);
    } else {
      // Feature not found, still don't return and try to set Gamma
      std::
          cout << "WARNING:: GammaSelector: feature not supported" << std::endl;
    }
  }
  // Enable Gamma Feature
  rcg::setBoolean (nodemap, "GammaEnable", 1, false);
  rcg::setBoolean (nodemap, "GammaEnabled", 1, false);

  // Set Gamma when GammaSelector is anyways not present or
  // if Gammaselector is present, and it's valie should be User
  gammaSelector = rcg::getEnum (nodemap, "GammaSelector", false);
  if (isFeature ("GammaSelector\0", NULL) && gammaSelector != "User") {
    std::cout <<
        "WARNING:: Gamma set failed because GammaSelector is not \"User\"" <<
        std::endl;
    return isGammaSet;
  }
  // Set the feature
  isGammaSet = setFloatFeature ("Gamma\0", &gencamParams->gamma, false);

  return isGammaSet;
}

bool
Genicam::setBalanceRatio (void)
{
  bool isBalanceRatioSet = false;
  double vMin, vMax;
  char balanceRatioStr[32];

  // Check if Auto White Balance is "Off", if not then return
  std::string balanceWhiteAuto =
      rcg::getEnum (nodemap, "BalanceWhiteAuto", false);
  if (balanceWhiteAuto != "Off") {
    std::cout <<
        "WARNING:: Ignore setting \"BalanceRatio\" as \"BalanceWhiteAuto\" not \"Off\""
        << std::endl;
    return isBalanceRatioSet;
  }
  // Check the feature string if it exists or not
  if (isFeature ("BalanceRatio\0", NULL)) {
    strncpy (balanceRatioStr, "BalanceRatio\0", sizeof ("BalanceRatio\0"));
  } else if (isFeature ("BalanceRatioAbs\0", NULL)) {
    strncpy (balanceRatioStr, "BalanceRatioAbs\0",
        sizeof ("BalanceRatioAbs\0"));
  } else {
    // Feature does not exist
    std::cout << "WARNING:: BalanceRatio: feature not supported\n";
    return isBalanceRatioSet;
  }

  // Set BalanceRatioSelector feature
  std::string balanceRatioSelector;
  if (!isFeature ("BalanceRatioSelector\0", NULL)) {
    // Don't return, still set the BalanceRatio if present
    std::cout << "WARNING: BalanceRatioSelector: feature not supported" <<
        std::endl;
  } else {
    if (!gencamParams->balanceRatioSelector) {
      // If there is no input for "BalanceRatioSelector", read existing value
      balanceRatioSelector =
          rcg::getEnum (nodemap, "BalanceRatioSelector", false);
    } else {
      balanceRatioSelector.assign (gencamParams->balanceRatioSelector);
      // Possible values: All, Red, Green, Blue, Y, U, V, Tap1, Tap2...
      setEnumFeature ("BalanceRatioSelector\0", balanceRatioSelector.c_str (),
          false);
    }
  }
  // Set the BalanceRatio feature
  // Track min and max value
  rcg::getFloat (nodemap, balanceRatioStr, &vMin, &vMax, false, 0);
  if (gencamParams->balanceRatio < vMin) {
    std::cout << "WARNING:: BalanceRatio: capping to minimum " << vMin <<
        std::endl;
    gencamParams->balanceRatio = vMin;
  } else if (gencamParams->balanceRatio > vMax) {
    std::cout << "WARNING:: BalanceRatio: capping to maximum " << vMax <<
        std::endl;
    gencamParams->balanceRatio = vMax;
  }

  isBalanceRatioSet =
      rcg::setFloat (nodemap, balanceRatioStr, gencamParams->balanceRatio,
      false);

  // Failed to set the feature
  if (!isBalanceRatioSet) {
    std::cout << "WARNING:: BalanceRatio: " <<
        gencamParams->balanceRatio << " set failed.\n";
  } else {
    balanceRatioSelector =
        rcg::getEnum (nodemap, "BalanceRatioSelector", false);
    std::cout << "BalanceRatio[" << balanceRatioSelector << "]: " <<
        gencamParams->balanceRatio << " set successful.\n";
  }

  return isBalanceRatioSet;
}

bool
Genicam::setBalanceWhiteAuto (void)
{
  bool isBalanceWhiteAutoSet = false;

  // check if BalanceWhiteAuto is present
  if (isFeature ("BalanceWhiteAuto\0", NULL) == false) {
    std::cout << "WARNING:: BalanceWhiteAuto: feature not supported\n";
    return isBalanceWhiteAutoSet;
  }
  // Possible values: Off, Once, Continuous
  isBalanceWhiteAutoSet =
      setEnumFeature ("BalanceWhiteAuto\0", gencamParams->balanceWhiteAuto,
      false);
  return isBalanceWhiteAutoSet;
}


bool
Genicam::setExposureAuto (void)
{
  bool isExposureAutoSet = false;

  // check if ExposureAuto is present
  if (isFeature ("ExposureAuto\0", NULL) == false) {
    std::cout << "WARNING:: ExposureAuto: feature not supported\n";
    return isExposureAutoSet;
  }
  // Possible values: Off, Once, Continuous
  isExposureAutoSet =
      setEnumFeature ("ExposureAuto\0", gencamParams->exposureAuto, false);
  return isExposureAutoSet;
}


bool
Genicam::setExposureTimeSelector (void)
{
  bool isExposureTimeSelectorSet = false;

  // Check if ExposureTimeSelector feature is present.
  if (!isFeature ("ExposureTimeSelector\0", NULL)) {
    std::cout <<
        "WARNING: ExposureTimeSelector: feature not Supported" << std::endl;
    return isExposureTimeSelectorSet;
  }

  /* exposure time selector should be set in conjuction with
     exposure time mode. If common both should be common.
     For others, time mode should be individual */
  if (strcasecmp (gencamParams->exposureTimeSelector, "Common") == 0) {
    std::cout << "Setting ExposureTimeSelector to \"Common\"\n";
    rcg::setEnum (nodemap, "ExposureTimeMode", "Common", false);
  } else {
    std::cout << "Setting ExposureTimeSelector to \"Individual\"\n";
    rcg::setEnum (nodemap, "ExposureTimeMode", "Individual", false);
  }

  isExposureTimeSelectorSet =
      setEnumFeature ("ExposureTimeSelector\0",
      gencamParams->exposureTimeSelector, false);

  return isExposureTimeSelectorSet;
}


bool
Genicam::setGainSelector (void)
{
  bool isGainSelectorSet = false;
  std::vector < std::string > gainSelectors;

  // Check if GainSelector feature is present.
  rcg::getEnum (nodemap, "GainSelector", gainSelectors, false);
  if (gainSelectors.empty ()) {
    std::cout << "WARNING:: GainSelector: feature not supported" << std::endl;
    return isGainSelectorSet;
  }

  isGainSelectorSet =
      setEnumFeature ("GainSelector\0", gencamParams->gainSelector, true);
  return isGainSelectorSet;
}


bool
Genicam::setGain (void)
{
  bool isGainSet = false, isFloat = true;
  double vMin, vMax, gain;
  int64_t vMinInt, vMaxInt, gainInt;

  // Proceed only if GainAuto is "Off"
  if ((isFeature ("GainAuto\0", NULL)) && (gainAuto != "Off")
      && (gainAuto != "")) {
    std::cout << "WARNING: Gain not set, GainAuto should be \"Off\"\n";
    return isGainSet;
  }
  gain = rcg::getFloat (nodemap, "Gain", &vMin, &vMax, false);
  if (!gain && !vMin && !vMax) {
    // Either feature not supported or deviation from standard
    // Let's check if deviation
    gainInt = rcg::getInteger (nodemap, "GainRaw", &vMinInt, &vMaxInt, false);
    if (!gainInt && !vMinInt && !vMaxInt) {
      std::cout << "WARNING:: Gain: feature not supported\n";
      return isGainSet;
    }
    // Deviation it is, It's gain raw instead of gain and int instead of float
    isFloat = false;
  }

  if (isFloat) {
    isGainSet = setFloatFeature ("Gain\0", &gencamParams->gain, false);
  } else {
    int gainTemp = (int) gencamParams->gain;

    isGainSet = setIntFeature ("GainRaw\0", &gainTemp, false);
    gencamParams->gain = (float) gainTemp;
  }

  return isGainSet;
}


bool
Genicam::setGainAuto (void)
{
  bool isGainAutoSet = false;

  // check if feature is present
  if (!isFeature ("GainAuto\0", NULL)) {
    std::cout << "WARNING:: GainAuto: feature not supported\n";
    return isGainAutoSet;
  }
  //Possible values: Off, Once, Continuous
  isGainAutoSet = setEnumFeature ("GainAuto\0", gencamParams->gainAuto, false);
  std::string str = rcg::getEnum (nodemap, "GainAuto", false);
  gainAuto.assign (str);
  return isGainAutoSet;
}


bool
Genicam::setGainAutoBalance (void)
{
  bool isGainAutoBalanceSet = false;

  // Check if feature not supported
  if (!isFeature ("GainAutoBalance\0", NULL)) {
    std::cout << "WARNING:: GainAutoBalance: feature not supported\n";
    return isGainAutoBalanceSet;
  }
  // Possible values: Off, Once, Continuous
  isGainAutoBalanceSet =
      setEnumFeature ("GainAutoBalance\0", gencamParams->gainAutoBalance,
      false);
  return isGainAutoBalanceSet;
}


bool
Genicam::setTriggerDivider (void)
{
  bool ret = false;

  // check if feature is present
  if (!isFeature ("TriggerDivider\0", NULL)) {
    std::cout << "WARNING:: TriggerDivider: feature not supported\n";
    return ret;
  }
  // Set Trigger Divider for the incoming Trigger Pulses.
  ret =
      setIntFeature ("TriggerDivider\0", &gencamParams->triggerDivider, false);

  return ret;
}


bool
Genicam::setTriggerMultiplier (void)
{
  bool ret = false;

  // check if feature is present
  if (!isFeature ("TriggerMultiplier\0", NULL)) {
    std::cout << "WARNING:: TriggerMultiplier: feature not supported\n";
    return ret;
  }
  // Set Trigger Multiplier for the incoming Trigger Pulses.
  ret =
      setIntFeature ("TriggerMultiplier\0", &gencamParams->triggerMultiplier,
      false);

  return ret;
}


bool
Genicam::setTriggerDelay (void)
{
  bool ret = false;
  char triggerDelayStr[32];

  // Check the feature string if it exists or not
  if (isFeature ("TriggerDelay\0", NULL)) {
    strncpy (triggerDelayStr, "TriggerDelay\0", sizeof ("TriggerDelay\0"));
  } else if (isFeature ("TriggerDelayAbs\0", NULL)) {
    strncpy (triggerDelayStr, "TriggerDelayAbs\0",
        sizeof ("TriggerDelayAbs\0"));
  } else {
    // Feature does not exist
    std::cout << "WARNING:: TriggerDelay: feature not supported\n";
    return ret;
  }

  // Set Trigger Delay after trigger reception before activating it.
  ret = setFloatFeature (triggerDelayStr, &gencamParams->triggerDelay, false);

  return ret;
}


bool
Genicam::setTriggerMode (const char *tMode)
{
  bool ret = false;

  // Set the Trigger Mode.
  ret = rcg::setEnum (nodemap, "TriggerMode", tMode, false);

  if (!ret) {
    std::cout << "WARNING:: TriggerMode: " << tMode << " set failed.\n";
  } else {
    std::cout << "TriggerMode: " << tMode << " set successful.\n";
    triggerMode.assign (tMode);
  }

  return ret;
}


bool
Genicam::setTriggerOverlap (void)
{
  bool isTriggerOverlapSet = false;

  // Check if the feature is supported or not.
  if (!isFeature ("TriggerOverlap\0", NULL)) {
    std::cout << "WARNING: TriggerOverlap: feature not Supported" << std::endl;
    return isTriggerOverlapSet;
  }
  // Possible values: Off, ReadOut, PreviousFrame, PreviousLine
  isTriggerOverlapSet =
      setEnumFeature ("TriggerOverlap\0", gencamParams->triggerOverlap, false);
  return isTriggerOverlapSet;
}


bool
Genicam::setTriggerActivation (void)
{
  bool isTriggerActivationSet = false;

  // Check if the feature is supported or not.
  if (!isFeature ("TriggerActivation\0", NULL)) {
    std::cout << "WARNING: TriggerActivation: feature not Supported" <<
        std::endl;
    return isTriggerActivationSet;
  }
  // Possible values: RisingEdge, FallingEdge, AnyEdge. LevelHigh, LevelLow
  isTriggerActivationSet =
      setEnumFeature ("TriggerActivation\0", gencamParams->triggerActivation,
      false);
  return isTriggerActivationSet;
}


bool
Genicam::setAcquisitionMode (void)
{
  bool isAcquisitionModeSet = false;

  // Possible values: Continuous, MultiFrame, SingleFrame
  isAcquisitionModeSet =
      setEnumFeature ("AcquisitionMode\0", gencamParams->acquisitionMode,
      false);

  std::string aMode = rcg::getEnum (nodemap, "AcquisitionMode", false);
  acquisitionMode.assign (aMode);
  if (aMode == "Continuous") {
    // Set trigger mode Off for Continuous mode
    // TODO handle trigger mode on for continuous
    setTriggerMode ("Off");
  } else {
    // Set trigger mode On for Non-Continuous mode
    setTriggerMode ("On");

    // Set "FrameTriggerWait" to check AcquisitionStatus in Create for TriggerSource = Software
    std::cout << "Setting AcquisitionStatusSelector to \"FrameTriggerWait\"\n";
    rcg::setEnum (nodemap, "AcquisitionStatusSelector", "FrameTriggerWait",
        false);
  }

  return isAcquisitionModeSet;
}


bool
Genicam::setDeviceClockFrequency (void)
{
  bool isDeviceClockFrequencySet = false;
  double vMin, vMax;

  // Check if DeviceClockFrequency is present or not, if not then return
  if (!isFeature ("DeviceClockFrequency\0", NULL)) {
    std::cout << "WARNING:: DeviceClockFrequency: feature not supported\n";
    return isDeviceClockFrequencySet;
  }
  // Check if DeviceClockSelector is supported or not.
  std::string deviceClockSelector;
  if (!isFeature ("DeviceClockSelector\0", NULL)) {
    // Don't return, still set the DeviceClockFrequency if present
    std::cout << "WARNING: DeviceClockSelector: feature not supported" <<
        std::endl;
  } else {
    if (!gencamParams->deviceClockSelector) {
      deviceClockSelector =
          rcg::getEnum (nodemap, "DeviceClockSelector", false);
    } else {
      deviceClockSelector.assign (gencamParams->deviceClockSelector);
      // Possible values: Sensor, SensorDigitization, CameraLink, Device-specific
      setEnumFeature ("DeviceClockSelector\0", deviceClockSelector.c_str (),
          false);
    }
  }

  // Proceed with DeviceClockFrequency
  // Check min and max
  rcg::getFloat (nodemap, "DeviceClockFrequency", &vMin, &vMax, false, 0);
  if (gencamParams->deviceClockFrequency < vMin) {
    std::cout << "WARNING:: DeviceClockFrequency less than minimum " << vMin <<
        " Capping it to " << vMin << std::endl;
    gencamParams->deviceClockFrequency = vMin;
  }
  if (gencamParams->deviceClockFrequency > vMax) {
    std::cout << "WARNING:: DeviceClockFrequency greater than maximum " << vMax
        << " Capping it to " << vMax << std::endl;
    gencamParams->deviceClockFrequency = vMax;
  }

  isDeviceClockFrequencySet = rcg::setFloat (nodemap, "DeviceClockFrequency",
      gencamParams->deviceClockFrequency, false);

  // Failed to set the feature
  if (!isDeviceClockFrequencySet) {
    std::cout << "WARNING:: DeviceClockFrequency: " <<
        gencamParams->deviceClockFrequency << " set failed.\n";
  } else {
    deviceClockSelector = rcg::getEnum (nodemap, "DeviceClockSelector", false);
    std::cout << "DeviceClockFrequency[" << deviceClockSelector << "]: " <<
        gencamParams->deviceClockFrequency << " set successful.\n";
  }
  return isDeviceClockFrequencySet;
}


bool
Genicam::setTriggerSoftware (void)
{
  bool ret = false;

  // Proceed only when TriggerSource = Software
  if (triggerSource != "Software") {
    std::cout <<
        "WARNING:: TriggerSoftware: command not trigerred; TriggerSource is not \"Software\""
        << std::endl;
    return ret;
  }
  // Execute TriggerSoftware command
  ret = rcg::callCommand (nodemap, "TriggerSoftware", false);
  if (!ret) {
    std::cerr << "WARNING:: TriggerSoftware set failed." << std::endl;
  } else {
    std::cout << "Call Command: \"TriggerSoftware\"\n";
  }

  return ret;
}


bool
Genicam::setTriggerSelector (void)
{
  bool isTriggerSelectorSet = false;

  // Check if feature is supported or not.
  if (!isFeature ("TriggerSelector\0", NULL)) {
    std::
        cout << "WARNING:: TriggerSelector: feature not supported" << std::endl;
    return isTriggerSelectorSet;
  }
  // Possible values: AcquisitionStart, AcquisitionEnd, AcquisitionActive, FrameStart, FrameEnd,
  // FrameActive, FrameBurstStart, FrameBurstEnd, FrameBurstActive, LineStart, ExposureStart,
  // ExposureEnd, ExposureActive, MultiSlopeExposureLimit1
  isTriggerSelectorSet =
      setEnumFeature ("TriggerSelector\0", gencamParams->triggerSelector,
      false);
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
    std::cout << "WARNING:: TriggerSource: feature not Supported" << std::endl;
    return isTriggerSourceSet;
  }
  // Proceed only if TriggerMode is ON
  /* TODO By default, setting trigger source software is a good idea
     even if trigger mode is not on */
  if (triggerMode != "On") {
    std::cout <<
        "WARNING:: TriggerSource: not configured as TriggerMode is not \"On\""
        << std::endl;
    return isTriggerSourceSet;
  }
  // Possible values: Software, SoftwareSignal<n>, Line<n>, UserOutput<n>, Counter<n>Start,
  // Counter<n>End, Timer<n>Start, Timer<n>End, Encoder<n>, <LogicBlock<n>>, Action<n>,
  // LinkTrigger<n>, CC<n>, ...
  isTriggerSourceSet =
      setEnumFeature ("TriggerSource\0", gencamParams->triggerSource, false);

  std::string tSource = rcg::getEnum (nodemap, "TriggerSource", false);
  triggerSource.assign (tSource);

  return isTriggerSourceSet;
}


bool
Genicam::setDeviceLinkThroughputLimit (void)
{
  bool isThroughputLimitSet = false;
  std::vector < std::string > throughputLimitModes;

  // Is DeviceLinkThroughputLimit supported? if not then return
  if (!isFeature ("DeviceLinkThroughputLimit\0", NULL)) {
    std::cout << "WARNING:: DeviceLinkThroughputLimit: feature not supported\n";
    return isThroughputLimitSet;
  }
  // Is DeviceLinkThroughputLimitMode supported? Enable if supported
  rcg::getEnum (nodemap, "DeviceLinkThroughputLimitMode", throughputLimitModes,
      false);
  if (throughputLimitModes.size () > 0) {
    // Set DeviceLinkThroughputLimitMode On
    deviceLinkThroughputLimitMode.assign ("On\0");
    rcg::setEnum (nodemap, "DeviceLinkThroughputLimitMode",
        deviceLinkThroughputLimitMode.c_str (), false);
    std::cout << "Setting DeviceLinkThroughputLimitMode to \"On\"\n";
  }
  // Configure DeviceLinkThroughputLimit
  isThroughputLimitSet =
      setIntFeature ("DeviceLinkThroughputLimit\0",
      &gencamParams->deviceLinkThroughputLimit, true);

  return isThroughputLimitSet;
}


bool
Genicam::setChannelPacketSize (void)
{
  bool isChannelPacketSizeSet = false;

  // Is GevSCPSPacketSize supported? if not then return
  if (!isFeature ("GevSCPSPacketSize\0", NULL)) {
    std::cout << "WARNING:: GevSCPSPacketSize: feature not supported\n";
    return isChannelPacketSizeSet;
  }
  // Configure GevSCPSPacketSize
  isChannelPacketSizeSet =
      setIntFeature ("GevSCPSPacketSize\0", &gencamParams->channelPacketSize,
      true);

  return isChannelPacketSizeSet;
}


bool
Genicam::setChannelPacketDelay (void)
{
  bool isChannelPacketDelaySet = false;

  // Is GevSCPD supported? if not then return
  if (!isFeature ("GevSCPD\0", NULL)) {
    std::cout << "WARNING:: GevSCPD: feature not supported\n";
    return isChannelPacketDelaySet;
  }
  // Configure GevSCPD
  isChannelPacketDelaySet =
      setIntFeature ("GevSCPD\0", &gencamParams->channelPacketDelay, false);

  return isChannelPacketDelaySet;
}
