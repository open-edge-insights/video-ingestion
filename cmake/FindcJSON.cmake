# Copyright (c) 2021 Intel Corporation.
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

# Find wjelement
find_path(CJSON_INCLUDE_DIR
    NAMES cJSON.h
    HINTS
        /usr/local/include/cjson
        /usr/include/cjson)
if(NOT CJSON_INCLUDE_DIR)
    message(FATAL_ERROR "-- Failed to find cJSON include path")
endif()

find_library(CJSON_LIBRARY NAMES cjson)
if(NOT CJSON_LIBRARY)
    message(FATAL_ERROR "-- Failed to find cJSON library")
endif()

set(CJSON_LIBRARIES ${CJSON_LIBRARY})
set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    CJSON DEFAULT_MSG CJSON_LIBRARIES CJSON_INCLUDE_DIRS)
