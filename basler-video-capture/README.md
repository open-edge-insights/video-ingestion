# Basler Video Capture Library
Implementation of a video capturing object using the Basler Pylon 5 SDK.

## Environment Setup
To compile this library run the following command in Ubuntu to install its dependencies:

```sh
$ sudo apt install cmake g++ build-essential python3-dev python3-pip
```

The library depends on the following requirements:

1. Pylon 5 SDK
2. NumPy Python library
2. Boost with Boost Python support (version 1.63 or greater)

Please see the following sections for details on installing these dependencies.

### Pylon 5
The Pylon dependency is in the `libs` directory. To install it execute the 
following commands:

```sh
$ cd libs
$ tar xvf pylon-5.0.11.10914-x86_64.tar.gz
$ cd pylon-5.0.11.10914-x86_64
$ tar -C /opt -zxf pylonSDK-5.0.11.10914-x86_64.tar.gz
```

This should successfully install the Basler Pylon 5 SDK.

### NumPy
To install NumPy execute the following pip command:

```sh
$ sudo -H pip3 install numpy
```

### Boost with Python Support
To install the Boost dependency, download the following Boost source code 
[here](https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.tar.gz).
Once you have downloaded the boost tar.gz file, decompress it with the following
command:

```sh
$ tar xvf boost_1_63_0.tar.gz
```

After extracting the code, execute the following commands to build and install
boost.

```sh
$ cd boost_1_63_0
$ ./bootstrap.sh --with-python=/usr/bin/python3
$ ./b2 --with-python
$ sudo ./b2 install
```

## Compilation and Installation
To install and compile the library, execute the following Python command:

```sh
$ python3 setup.py install
```

This command will compiling the C++ library, and install the library into your
Python installation. This allows the library to directly imported from any
script. In addition, the `basler-capture` tool is also installed. See below
for more information on this tool.

### Only Compiling the Library
If you wish to only compile the C++ library, run the following commands:

```sh
$ mkdir build
$ cd build
$ cmake ..
$ make
```

The output of the build is a `.so` file for the `basler_videoc_apture` library, 
as well as an executable named `basler-camera`, which is a simple program which 
uses the library to get frames from the first camera it can find.

## Unit Tests
To run the unit tests associated with the library, execute the following
command:

```sh
$ python3 setup.py test
```

The unit tests are located in the `tests/` directory.

## `basler-capture` Tool
This library comes with a tool for enumerating available Basler cameras on your
network, viewing a live feed from the camera, as well as capturing a video of
the video feed of a given Basler camera. This tool is located in the `bin/`
directory. Executing it requires that the `basler_video_capture` Python 
library be install on your system. The tool is also automatically installed when
you install this library, and is available in your path as `basler-capture`.

The command line for this tool is as follows:

```
usage: basler-capture [-h] {enumerate,video-capture,viewer} ...

positional arguments:
  {enumerate,video-capture,viewer}
    enumerate           Enumerate all available cameras
    video-capture       Capture the stream into a video file
    viewer              View live Basler camera feed

optional arguments:
  -h, --help            show this help message and exit
```

### Example Usages

**Enumerate all available Basler cameras:**
```sh
$ basler-capture enumerate
```

**View the feed from Basler camera:**
```sh
$ basler-capture viewer -d 0815-0000
```
> **NOTE:** In this example, `0815-0000` is the camera's serial number.

**Record the video feed from a Basler camera to an `avi` file using the `MJPG`
codec:**
```sh
$ basler-capture video-capture -d 0815-0000 -o example.avi
```
> **NOTE:** In this example, `0815-0000` is the camera's serial number. Also,
> `MPJG` is the default codec.

**Record the video feed from a Basler camera to an `mp4` file using the `mp4v`
codec:**
```sh
$ basler-capture video-capture -d 0815-0000 -o example.mp4 -c mp4v 
```
> **NOTE:** In this example, `0815-0000` is the camera's serial number.
