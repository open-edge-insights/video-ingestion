# Generic Camera Plugin

1. [Overview](#overview)

2. [Versioning](#versioning)

2. [Build](#build)

3. [Clean](#clean)

4. [Usage](#usage)

5. [Troubleshooting](#troubleshooting)

## Overview
This is the Gstreamer source plugin for camera devices compliant to GenICam. The design is scalable to other machine vision standards. The plugin uses interface technology driver - Gig E Vision driver or USB 3 Vision driver - by the camera device vendor wrapped under GenICam standard as GenTL producer. The plugin has a library that acts as a GenTL consumer. GenTL consumer interprets the GenICam compliant camera capabilities via camera description file in XML format and configures as desired via GenAPI.

## Versioning
The source code is versioned with the format of 3 numbers separated by points. The first number is major version, which in this case is 1. The second number is minor version, which increments for every release like engineering releases, alpha or PV etc., The third number is the revision number, which increments when a feature gets merged from a feature branch. It resets when the minor version number increments for a release.
First engineering release version is v1.0.0
Second engineering release version is v1.1.0
Alpha release version is v1.2.0
PV release version is v1.3.0

## Build and Install
Following is the command to build the plugin.
```
$ ./setup.sh
```
This script does the following
1.	Downloads the GenICam runtime binaries (verision 3.1) from EMVA website
2.	Unzips the binary and except Linux64 for x86_64 tar ball, deletes all other files
3.	Untars Linux64 for x86_64 tar ball and copies to standard library path in Ubuntu, i.e., /usr/lib/x86_64-linux-gnu/
4.	Runs configure command to configure the project generating Makefile
5.	Calls make to build
6.	Installs the generated gencamsrc shared library to /usr/local/lib/gstreamer-1.0
7.	Sets the environment variable GST_PLUGIN_PATH to /usr/local/lib/gstreamer-1.0

If plugin is installed successsfully, should be able to inspect it. 
```
$ gst-inspect-1.0 gencamsrc
```
If it returns information about the plugin, then it's installed successfully 
and can be used like any other gstreamer source.

## Clean
To remove the program binaries and object files from the source code directory
```
$ make clean
```
To also remove the files project Makefile that 'configure' created
```
$ make distclean
```

## Usage
A few example pipelines with this plugin below. The serial number of the Basler camera in PMCE BA lab is 22034422.
```
$ gst-launch-1.0 gencamsrc serial=22034422 ! videoconvert ! ximagesink
$ gst-launch-1.0 gencamsrc serial=22034422 pixel-format=bayerbggr ! bayer2rgb ! ximagesink
```



## Troubleshooting
### GenICam runtime binaries error
If the pipeline returns error similar below, then GenICam runtime dependency is not resolved.
```
$ module_open failed: libGenApi_gcc42_v3_1.so: cannot open shared object file: No such file or directory
```
In that case, download and copy the GenICam runtime binaries to standard path. The copied files are stored in the system after reboot.
1.	Download this file - https://www.emva.org/wp-content/uploads/GenICam_V3_1_0_public_data.zip
2.	Unzip to get runtime binaries and SDK files for multiple platforms
3.	Untar GenICam_Runtime_gcc42_Linux32_i86_v3_1_0.tgz file
4.	Under bin/Linux64_x64 directory, there will be shared libraries, copy all of them to usr/lib/x86_64-linux-gnu path

### GenTL producer error
If the pipeline returns error similar below, then GenTL producer is not found.
```
$ No transport layers found in path
```
In that case, set GENICAM_GENTL64_PATH environment variable to the GenTL producer installation path. Please install the compatible GenTL producer for the camera if not already done.

For Basler camera, the GenTL producer can be downloaded from https://www.baslerweb.com/en/sales-support/downloads/software-downloads/pylon-5-0-12-linux-x86-64-bit-debian/

Upon installation of this software, GenTL will be present in “/opt/pylon5/lib64/gentlproducer/gtl”, accordingly set the environment variable.
```
$ export GENICAM_GENTL64_PATH=/opt/pylon5/lib64/gentlproducer/gtl/
```
This variable may be set variable in .bashrc file so that it is one-time and need not be set every time when the terminal is opened.
