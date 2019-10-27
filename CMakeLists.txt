cmake_minimum_required(VERSION 3.11)
project(video-ingestion)

# Define CMake options
option(WITH_PROFILE "Compile in profiling mode" OFF)

# Globals
set(EIS_COMMON_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}/../common/cmake")

# Set C standard to C11
set(CMAKE_C_STANDARD 11)

# Set CFLAGS
set(CMAKE_C_FLAGS "-fPIE -fPIC -O2 -Wall -pedantic -fstack-protector-strong -D_FORTIFY_SOURCE=2")

# Set LDFLAGS
set(CMAKE_SHARED_LINKER_FLAGS "-z noexecstack -z relro -z now")

# Add cmake module path
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${EIS_COMMON_CMAKE}")

if(WITH_PROFILE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
endif()

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
find_package(EISMessageBus REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED
    gstreamer-1.0>=1.14
    gstreamer-video-1.0>=1.14
    gstreamer-sdp-1.0>=1.14
    gstreamer-app-1.0>=1.14)

# Include Intel safestring library
configure_file(${EIS_COMMON_CMAKE}/IntelSafeString.txt.in safestring-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
                RESULT_VARIABLE ss_result
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/safestring-download)
if(ss_result)
    message(FATAL_ERROR "CMake step for safestring failed: ${ss_result}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE ss_result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/safestring-download)
if(result)
    message(FATAL_ERROR "Build step for safestring failed: ${ss_result}")
endif()

add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/safestring-src
                 ${CMAKE_CURRENT_BINARY_DIR}/safestring-build
                 EXCLUDE_FROM_ALL)

# Include header directories
include_directories(
    include/
    ${OpenCV_INCLUDE_DIRS}
    ${EISMsgEnv_INCLUDE}
    ${EISUtils_INCLUDE}
    ${EISMessageBus_INCLUDE}
    ${GST_INCLUDE_DIRS}
    ${safestring_SOURCE_DIR}/include)

# Find C++ sources
file(GLOB SOURCES "src/*.cpp")

add_executable(video-ingestion ${SOURCES})
target_link_libraries(video-ingestion
    PUBLIC
        ${OpenCV_LIBS}
        ${EISUtils_LIBRARIES}
        ${EISMsgEnv_LIBRARIES}
        ${EISMessageBus_LIBRARIES}
        ${GST_LIBRARIES}
    PRIVATE
        ${ZMQ_LIBRARIES}
        ${CJSON_LIBRARIeES}
        safestring_static)

# If compile in debug mode, set DEBUG flag for C code
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(video-ingestion PRIVATE DEBUG=1)
endif()

if(WITH_PROFILE)
    target_compile_definitions(video-ingestion PRIVATE WITH_PROFILE=1)
endif()
