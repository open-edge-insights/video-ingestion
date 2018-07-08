cmake_minimum_required(VERSION 3.2)
project(basler-camera)

find_package(PythonInterp 3 REQUIRED)
find_package(PythonLibs 3 REQUIRED)
find_package(Boost COMPONENTS python3 numpy3 REQUIRED)

set(CMAKE_CXX_STANDARD 11)

set(LIB_SOURCES 
    "src/basler_video_capture.cpp")

PYTHON_ADD_MODULE(basler_video_capture ${LIB_SOURCES})
add_executable(basler-test "src/main.cpp")

if(${APPLE})
    message("-- Compiling for MacOS")
    # Find the pylon.framework on the mac OS system
    find_library(PYLON Pylon)

    # Adding the include directories
    include_directories(
        include/
        ${PYLON}/Headers/
        ${PYLON}/Headers/GenICam/
        ${Boost_INCLUDE_DIRS} 
        ${PYTHON_INCLUDE_DIRS})

    # Link libraries for the basler_video_capture library
    target_link_libraries(basler_video_capture ${PYLON} ${Boost_LIBRARIES} ${PYTHON_LIBRARIES})

    # Link libraries for basler-test application
    target_link_libraries(basler-test ${PYLON})
    set_target_properties(basler-test PROPERTIES LINK_FLAGS "${PYLON_LDFLAGS}")
elseif(${UNIX})
    message("-- Compiling for Linux")

    set(PYLON_ROOT "/opt/pylon5")
    set(PYLON_CONFIG "${PYLON_ROOT}/bin/pylon-config")
    set(PYLON_INCLUDE "${PYLON_ROOT}/include")

    if(NOT EXISTS "${PYLON_ROOT}")
        message(FATAL_ERROR "Pylon root directory does not exist: ${PYLON_ROOT}")
    elseif(NOT EXISTS "${PYLON_CONFIG}")
        message(FATAL_ERROR "pylon-config command is missing: ${PYLON_CONFIG}")
    elseif(NOT EXISTS "${PYLON_INCLUDE}")
        message(FATAL_ERROR "Pylon include directory does not exist: ${PYLON_INCLUDE}")
    endif()

    # Getting all Pylon config values
    message(STATUS "Getting Pylon CFLAGS")
    execute_process(COMMAND ${PYLON_CONFIG} --cflags OUTPUT_VARIABLE PYLON_CFLAGS)

    message(STATUS "Getting Pylon LDFLAGS")
    execute_process(COMMAND ${PYLON_CONFIG} --libs-rpath OUTPUT_VARIABLE PYLON_LDFLAGS)

    message(STATUS "Getting Pylon Libraries")
    execute_process(COMMAND ${PYLON_CONFIG} --libs OUTPUT_VARIABLE PYLON_LIBS)

    # Cleaning up output of the commands
    string(REGEX REPLACE "\n$" "" PYLON_CFLAGS "${PYLON_CFLAGS}")
    string(REGEX REPLACE "\n$" "" PYLON_LDFLAGS "${PYLON_LDFLAGS}")
    string(REGEX REPLACE "\n$" "" PYLON_LIBS "${PYLON_LIBS}")

    # Setting the CXX Flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PYLON_CFLAGS}")

    # Adding the include directories
    include_directories(
        include/
        ${PYLON_INCLUDE}
        ${Boost_INCLUDE_DIRS} 
        ${PYTHON_INCLUDE_DIRS})

    # PYTHON_ADD_MODULE(basler_video_capture ${LIB_SOURCES})
    target_link_libraries(basler_video_capture ${PYLON_LIBS} ${Boost_LIBRARIES} ${PYTHON_LIBRARIES})
    set_target_properties(basler_video_capture PROPERTIES LINK_FLAGS "${PYLON_LDFLAGS}")

    # add_executable(basler-test src/main.cpp)
    target_link_libraries(basler-test ${PYLON_LIBS})
    set_target_properties(basler-test PROPERTIES LINK_FLAGS "${PYLON_LDFLAGS}")
else()
    message(FATAL_ERROR "Unsupported platform for the library")
endif()
