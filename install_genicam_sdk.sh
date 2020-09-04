#!/bin/bash -e

# Copyright (c) 2020 Intel Corporation.

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


# Adding basler camera's SDK and removing unwanted files

PYLON_SDK_VER=5.1.0.12682

wget https://www.baslerweb.com/media/downloads/software/pylon_software/pylon-${PYLON_SDK_VER}-x86_64.tar.gz && \
tar xvf pylon-${PYLON_SDK_VER}-x86_64.tar.gz && \
cd pylon-${PYLON_SDK_VER}-x86_64 && \
tar -C /opt -zxf pylonSDK-${PYLON_SDK_VER}-x86_64.tar.gz && \
rm -rf pylon-${PYLON_SDK_VER}-x86_64.tar.gz && \
rm -rf pylon-${PYLON_SDK_VER}-x86_64/pylonSDK-${PYLON_SDK_VER}-x86_64.tar.gz

# Add camera SDK installation or the respecitive Genicam needed
