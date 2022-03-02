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
ARG OPENVINO_IMAGE
FROM ia_video_common:$EII_VERSION as video_common
FROM ia_openvino_base:$EII_VERSION as openvino_base
FROM ia_eiibase:$EII_VERSION as builder
LABEL description="VideoIngestion image"

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends \
    automake \
    libglib2.0-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libusb-1.0-0-dev \
    libtool \
    zlib1g-dev \
    make && \
    rm -rf /var/lib/apt/lists/*

ARG CMAKE_INSTALL_PREFIX

# Install libzmq
RUN rm -rf deps && \
    mkdir -p deps && \
    cd deps && \
    wget -q --show-progress https://github.com/zeromq/libzmq/releases/download/v4.3.4/zeromq-4.3.4.tar.gz -O zeromq.tar.gz && \
    tar xf zeromq.tar.gz && \
    cd zeromq-4.3.4 && \
    ./configure --prefix=${CMAKE_INSTALL_PREFIX} && \
    make install

# Install cjson
RUN rm -rf deps && \
    mkdir -p deps && \
    cd deps && \
    wget -q --show-progress https://github.com/DaveGamble/cJSON/archive/v1.7.12.tar.gz -O cjson.tar.gz && \
    tar xf cjson.tar.gz && \
    cd cJSON-1.7.12 && \
    mkdir build && cd build && \
    cmake -DCMAKE_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_PREFIX}/include -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} .. && \
    make install

COPY --from=video_common ${CMAKE_INSTALL_PREFIX}/include ${CMAKE_INSTALL_PREFIX}/include
COPY --from=video_common ${CMAKE_INSTALL_PREFIX}/lib ${CMAKE_INSTALL_PREFIX}/lib
COPY --from=video_common ${CMAKE_INSTALL_PREFIX}/bin ${CMAKE_INSTALL_PREFIX}/bin
COPY --from=video_common /root/.local/bin/cythonize /root/.local/bin/cythonize
COPY --from=video_common /root/.local/lib/python3.8/site-packages/ /root/.local/lib/python3.8/site-packages
COPY --from=video_common /eii/common/cmake ./common/cmake
COPY --from=video_common /eii/common/libs ./common/libs
COPY --from=video_common /eii/common/util ./common/util
COPY --from=openvino_base /opt/intel /opt/intel

ENV LD_LIBRARY_PATH ${LD_LIBRARY_PATH}:${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/lib/udfs

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

# Installing Generic Plugin
RUN cd VideoIngestion && \
     ./install_gencamsrc_gstreamer_plugin.sh

FROM ${OPENVINO_IMAGE} AS runtime

USER root

ARG EII_UID
ARG EII_USER_NAME
RUN useradd -r -u ${EII_UID} -G video ${EII_USER_NAME}

WORKDIR /app

ENV DEBIAN_FRONTEND="noninteractive" \
    LD_RUN_PATH="/usr/lib" \
    LIBRARY_PATH=$LD_RUN_PATH:$LIBVA_DRIVERS_PATH \
    LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$LIBVA_DRIVERS_PATH:"usr/local/lib" \
    PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig" \
    TERM="xterm" \
    GST_DEBUG="1"

# Installing apt packages for Matrix Vision Camera SDK
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    iproute2 \
    net-tools \
    wget && \
    rm -rf /var/lib/apt/lists/*

ARG CMAKE_INSTALL_PREFIX
COPY --from=builder ${CMAKE_INSTALL_PREFIX}/lib ${CMAKE_INSTALL_PREFIX}/lib
COPY --from=builder /app/common/util/__init__.py common/util/
COPY --from=builder /app/common/util/*.py common/util/
COPY --from=builder /app/VideoIngestion/build/video-ingestion ./VideoIngestion/build/
COPY --from=builder /app/VideoIngestion/schema.json ./VideoIngestion/
COPY --from=builder /app/VideoIngestion/*.sh ./VideoIngestion/
COPY --from=builder /app/VideoIngestion/models ./models
COPY --from=builder /app/VideoIngestion/test_videos ./test_videos
COPY --from=builder /root/.local/lib/python3.8/site-packages .local/lib/python3.8/site-packages
COPY --from=builder /app/VideoIngestion/src-gst-gencamsrc/plugins/genicam-core/genicam/bin/*.so /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/local/lib/gstreamer-1.0 /usr/local/lib/gstreamer-1.0
COPY --from=video_common /eii/common/video/udfs/python ./common/video/udfs/python
COPY --from=video_common /root/.local/lib .local/lib
COPY --from=builder /app/VideoIngestion/mvGenTL_Acquire-x86_64_ABI2-2.44.1.tgz ./VideoIngestion/

### Note: In case one cannot non-interactively download the camera SDK from the
### web then first download the camera SDK onto to the system, place it under
### VideoIngestion directory and use the COPY instruction to use it in the build context.

# Installing New Matrix Vision SDK
RUN cd ./VideoIngestion && \
    ./install_mvGenTL_Acquire.sh && \
    rm -rf matrix_vision_downloads

# Set environment variables required for Matrix Vision SDK
ENV MVIMPACT_ACQUIRE_DIR="/opt/mvIMPACT_Acquire" \
    MVIMPACT_ACQUIRE_DATA_DIR="/opt/mvIMPACT_Acquire/data" \
    GENICAM_ROOT="/opt/mvIMPACT_Acquire/runtime" \
    MVIMPACT_ACQUIRE_FAVOUR_SYSTEMS_LIBUSB="1"

### To install other/newer Genicam camera SDKs add the installation steps here

# Installing Intel® Graphics Compute Runtime for OpenCL™
RUN /bin/bash -c "source /opt/intel/openvino/bin/setupvars.sh && \
                 cd /opt/intel/openvino/install_dependencies && \
                 yes | ./install_NEO_OCL_driver.sh --auto || true"

ENV PYTHONPATH ${PYTHONPATH}:/app/common/video/udfs/python:/app/common/:/app:/app/.local/lib/python3.8/site-packages
ENV LD_LIBRARY_PATH ${LD_LIBRARY_PATH}:${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/lib/udfs
RUN chown ${EII_USER_NAME}:${EII_USER_NAME} /app /var/tmp
RUN usermod -a -G users ${EII_USER_NAME}

# Needed to not show the warnings while executing sample_onnx UDF
RUN mkdir -p /home/${EII_USER_NAME} && \
    chown -R ${EII_USER_NAME}:${EII_USER_NAME} /home/${EII_USER_NAME}/
ARG SOCKET_DIR
RUN mkdir -p ${SOCKET_DIR}
ENV SOCK_DIR=${SOCKET_DIR}
RUN chown -R ${EII_USER_NAME}:${EII_USER_NAME} ${SOCKET_DIR}
ENV EIIUSER=${EII_USER_NAME}

USER ${EII_USER_NAME}

HEALTHCHECK NONE
ENTRYPOINT ["./VideoIngestion/vi_start.sh"]
