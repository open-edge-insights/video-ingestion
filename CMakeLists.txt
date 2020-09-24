# Copyright (c) 2019 Intel Corporation.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM,OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.12)
project(video-ingestion VERSION 1.0.0 LANGUAGES C CXX)

# Define CMake options
option(WITH_PROFILE "Compile in profiling mode" OFF)

# Globals
set(EIS_COMMON_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}/../common/cmake")

# Set C++ version
set(CMAKE_CXX_STANDARD 14)

# Set CFLAGS for C/C++
set(CMAKE_C_FLAGS "-fPIE -fPIC -O2 -Wall -pedantic -fstack-protector-strong -D_FORTIFY_SOURCE=2")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")

# Set LDFLAGS
set(CMAKE_SHARED_LINKER_FLAGS "-z noexecstack -z relro -z now")

# Add cmake module path
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${EIS_COMMON_CMAKE}")

#if(WITH_PROFILE)
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
#endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json")
    EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
    )
endif()

# Finding packages
find_package(OpenCV REQUIRED)
find_package(EISMsgEnv REQUIRED)
find_package(EISUtils REQUIRED)
find_package(EISConfigMgr REQUIRED)
find_package(EISConfigMgrNew REQUIRED)
find_package(EISMessageBus REQUIRED)
find_package(UDFLoader REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(IntelSafeString REQUIRED)
find_package(WJE REQUIRED)

pkg_check_modules(GST REQUIRED
    gstreamer-1.0>=1.14
    gstreamer-video-1.0>=1.14
    gstreamer-sdp-1.0>=1.14
    gstreamer-app-1.0>=1.14)

# Include header directories
include_directories(
    include/
    ${OpenCV_INCLUDE_DIRS}
    ${EISMsgEnv_INCLUDE}
    ${EISUtils_INCLUDE}
    ${EISConfigMgr_INCLUDE}
    ${EISConfigMgrNew_INCLUDE}
    ${EISMessageBus_INCLUDE}
    ${UDFLoader_INCLUDE}
    ${GST_INCLUDE_DIRS}
    ${IntelSafeString_INCLUDE})

# Find C++ sources
file(GLOB SOURCES "src/*.cpp")

add_executable(video-ingestion ${SOURCES})
target_link_libraries(video-ingestion
    PUBLIC
        ${OpenCV_LIBS}
        ${EISUtils_LIBRARIES}
        ${EISMsgEnv_LIBRARIES}
        eisconfigmgr
        ${EISConfigMgrNew_LIBRARIES}
        ${EISMessageBus_LIBRARIES}
        ${UDFLoader_LIBRARIES}
        ${GST_LIBRARIES}
    PRIVATE
        ${ZMQ_LIBRARIES}
        ${CJSON_LIBRARIEES}
        ${IntelSafeString_LIBRARIES}
        wjelement
        wjreader)

# If compile in debug mode, set DEBUG flag for C code
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(video-ingestion PRIVATE DEBUG=1)
endif()

if(WITH_PROFILE)
    target_compile_definitions(video-ingestion PRIVATE WITH_PROFILE=1)
endif()
