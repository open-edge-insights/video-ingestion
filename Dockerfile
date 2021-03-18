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

# Dockerfile for VideoIngestion

ARG EII_VERSION
ARG DOCKER_REGISTRY
FROM ${DOCKER_REGISTRY}ia_video_common:$EII_VERSION as video_common
FROM ${DOCKER_REGISTRY}ia_openvino_base:$EII_VERSION as openvino_base
FROM ${DOCKER_REGISTRY}ia_eiibase:$EII_VERSION as builder
LABEL description="VideoIngestion image"

WORKDIR /app
ARG CMAKE_INSTALL_PREFIX
COPY --from=video_common ${CMAKE_INSTALL_PREFIX}/include ${CMAKE_INSTALL_PREFIX}/include
COPY --from=video_common ${CMAKE_INSTALL_PREFIX}/lib ${CMAKE_INSTALL_PREFIX}/lib
COPY --from=video_common ${CMAKE_INSTALL_PREFIX}/bin ${CMAKE_INSTALL_PREFIX}/bin
COPY --from=video_common /root/.local/bin/cythonize /root/.local/bin/cythonize
COPY --from=video_common /root/.local/lib/python3.6/site-packages/ /root/.local/lib/python3.6/site-packages
COPY --from=video_common /eii/common/cmake ./common/cmake
COPY --from=video_common /eii/common/libs ./common/libs
COPY --from=video_common /eii/common/util ./common/util
COPY --from=openvino_base /opt/intel /opt/intel

ENV LD_LIBRARY_PATH ${LD_LIBRARY_PATH}:${CMAKE_INSTALL_PREFIX}/lib

# Build Intel® RealSense™ SDK 2.0
ARG LIBREALSENSE_VER=https://github.com/IntelRealSense/librealsense/archive/v2.41.0.tar.gz

RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    libssl-dev \
    libusb-1.0-0-dev \
    libgtk-3-dev \
    libglfw3-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    pkg-config && \
    rm -rf /var/lib/apt/lists/*

RUN wget -O - ${LIBREALSENSE_VER} | tar xz && \
    cd librealsense-2.41.0 && \
    mkdir build && \
    cd build && \
    cmake ../ -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} && \
    make && \
    make install

# Copy VideoIngestion source code
COPY . ./VideoIngestion
ARG WITH_PROFILE

# Build VideoIngestion application
RUN /bin/bash -c "source /opt/intel/openvino/bin/setupvars.sh && \
                  cd VideoIngestion && \
                  rm -rf build && \
                  mkdir build && \
                  cd build && \
                  cmake -DCMAKE_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_PREFIX}/include -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DWITH_PROFILE=${WITH_PROFILE} .. && \
                  make"

FROM openvino_base as runtime
ARG EII_UID
ARG EII_USER_NAME
RUN useradd -r -u ${EII_UID} -G video ${EII_USER_NAME}

RUN apt-get update && apt-get install -y --no-install-recommends \
    automake \
    iproute2 \
    libtool \
    net-tools \
    wget && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
ARG CMAKE_INSTALL_PREFIX
COPY --from=builder ${CMAKE_INSTALL_PREFIX}/lib ${CMAKE_INSTALL_PREFIX}/lib
COPY --from=builder /app/common/util/__init__.py common/util/
COPY --from=builder /app/common/util/*.py common/util/
COPY --from=builder /app/VideoIngestion/build/video-ingestion ./VideoIngestion/build/
COPY --from=builder /app/VideoIngestion/schema.json ./VideoIngestion/
COPY --from=builder /app/VideoIngestion/*.sh ./VideoIngestion/
COPY --from=builder /app/VideoIngestion/models ./models
COPY --from=builder /app/VideoIngestion/test_videos ./test_videos
COPY --from=builder /root/.local/lib/python3.6/site-packages .local/lib/python3.6/site-packages
COPY --from=builder /app/VideoIngestion/src-gst-gencamsrc ./VideoIngestion/src-gst-gencamsrc
COPY --from=builder /app/VideoIngestion/install_gencamsrc_gstreamer_plugin.sh ./VideoIngestion/install_gencamsrc_gstreamer_plugin.sh

# Installing Generic Plugin
RUN cd VideoIngestion && \
     ./install_gencamsrc_gstreamer_plugin.sh

# Build Intel(R) Media SDK
ARG MSDK_REPO=https://github.com/Intel-Media-SDK/MediaSDK/releases/download/intel-mediasdk-19.1.0/MediaStack.tar.gz

RUN wget -O - ${MSDK_REPO} | tar xz && \
    cd MediaStack && \
    cp -a opt/. /opt/ && \
    cp -a etc/. /opt/ && \
    ldconfig

ENV DEBIAN_FRONTEND="noninteractive" \
    MFX_HOME=$MFX_HOME:"/opt/intel/mediasdk/" \
    PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig:/opt/intel/mediasdk" \
    GST_VAAPI_ALL_DRIVERS="1" \
    LIBVA_DRIVERS_PATH=$LIBVA_DRIVERS_PATH:"/opt/intel/mediasdk/lib64:/usr/lib/x86_64-linux-gnu/dri/" \
    LIBVA_DRIVER_NAME="iHD" \
    LD_RUN_PATH="/usr/lib" \
    LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/opt/intel/mediasdk/lib64:/opt/intel/mediasdk/lib/:/opt/intel/mediasdk/share/mfx/samples:/usr/local/lib" \
    LIBRARY_PATH="/usr/lib" \
    TERM="xterm" \
    GST_DEBUG="1"

### Note: In case one cannot non-interactively download the camera SDK from the
### web then first download the camera SDK onto to the system, place it under
### VideoIngestion directory and use the COPY instruction to use it in the build context.

# Installing Matrix Vision Camera SDK
ARG MATRIX_VISION_SDK_VER=2.38.0

RUN mkdir -p matrix_vision_downloads && \
    cd matrix_vision_downloads && \
    wget -q --show-progress http://static.matrix-vision.com/mvIMPACT_Acquire/${MATRIX_VISION_SDK_VER}/mvGenTL_Acquire-x86_64_ABI2-${MATRIX_VISION_SDK_VER}.tgz && \
    wget -q --show-progress http://static.matrix-vision.com/mvIMPACT_Acquire/${MATRIX_VISION_SDK_VER}/install_mvGenTL_Acquire.sh && \
    chmod +x install_mvGenTL_Acquire.sh && \
    ./install_mvGenTL_Acquire.sh && \
    rm -rf matrix_vision_downloads

### To install other/newer Genicam camera SDKs add the installation steps here

COPY --from=video_common /eii/common/video/udfs/python ./common/video/udfs/python
ENV PYTHONPATH ${PYTHONPATH}:/app/common/video/udfs/python:/app/common/:/app:/app/.local/lib/python3.6/site-packages
ENV LD_LIBRARY_PATH ${LD_LIBRARY_PATH}:${CMAKE_INSTALL_PREFIX}/lib

HEALTHCHECK NONE
ENTRYPOINT ["./VideoIngestion/vi_start.sh"]
