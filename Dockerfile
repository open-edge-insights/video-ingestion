# Dockerfile for VideoIngestion
ARG EIS_VERSION
FROM ia_openvino_base:$EIS_VERSION as openvino
LABEL description="VideoIngestion image"

WORKDIR ${PY_WORK_DIR}
ARG EIS_UID
ARG EIS_USER_NAME
RUN useradd -r -u ${EIS_UID} -G video ${EIS_USER_NAME}

# Adding basler camera's essentials by referring it's repo's README and Removing unwanted files
RUN wget https://www.baslerweb.com/media/downloads/software/pylon_software/pylon-5.1.0.12682-x86_64.tar.gz && \
    tar xvf pylon-5.1.0.12682-x86_64.tar.gz && \
    cd pylon-5.1.0.12682-x86_64 && \
    tar -C /opt -zxf pylonSDK-5.1.0.12682-x86_64.tar.gz && \
    rm -rf pylon-5.1.0.12682-x86_64.tar.gz && \
    rm -rf pylon-5.1.0.12682-x86_64/pylonSDK-5.1.0.12682-x86_64.tar.gz

# Installing python boost dependencies
RUN apt-get update && \
    apt-get install -y libboost-python-dev unzip

ENV PYLON_CAMEMU 1
# Adding gstreamer capabilities
ENV TERM=xterm \
    DEBIAN_FRONTEND=noninteractive
RUN apt -y remove cmake

# COMMON BUILD TOOLS
RUN apt-get install -y build-essential \
    autoconf make pciutils cpio libtool lsb-release \
    ca-certificates pkg-config bison flex libcurl4-gnutls-dev zlib1g-dev

# Install automake, use version 1.14 on CentOS
ARG AUTOMAKE_VER=1.14
ARG AUTOMAKE_REPO=https://ftp.gnu.org/pub/gnu/automake/automake-${AUTOMAKE_VER}.tar.xz
    RUN apt-get install -y -q automake

# Build NASM
ARG NASM_VER=2.13.03
ARG NASM_REPO=https://www.nasm.us/pub/nasm/releasebuilds/${NASM_VER}/nasm-${NASM_VER}.tar.bz2
RUN wget ${NASM_REPO} && \
    tar -xaf nasm* && \
    cd nasm-${NASM_VER} && \
    ./autogen.sh && \
    ./configure --prefix="/usr" --libdir=/usr/lib/x86_64-linux-gnu && \
    make -j$(nproc --ignore=2) && \
    make install

# Build libdrm
ARG LIBDRM_VER=2.4.96
ARG LIBDRM_REPO=https://dri.freedesktop.org/libdrm/libdrm-${LIBDRM_VER}.tar.gz

RUN apt-get install -y libpciaccess-dev

RUN wget -O - ${LIBDRM_REPO} | tar xz && \
    cd libdrm-${LIBDRM_VER} && \
    ./configure --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu && \
    make -j$(nproc --ignore=2) && \
    make install DESTDIR=/home/build && \
    make install ;

RUN apt-get install -y libx11-dev \
    xorg-dev \
    libgl1-mesa-dev \
    openbox

# Build Intel(R) Media SDK
ARG MSDK_REPO=https://github.com/Intel-Media-SDK/MediaSDK/releases/download/intel-mediasdk-19.1.0/MediaStack.tar.gz

RUN wget -O - ${MSDK_REPO} | tar xz && \
    cd MediaStack && \
    cp -r opt/ /home/build && \
    cp -r etc/ /home/build && \
    cp -a opt/. /opt/ && \
    cp -a etc/. /opt/ && \
    ldconfig

ENV LIBVA_DRIVERS_PATH=/opt/intel/mediasdk/lib64
ENV LIBVA_DRIVER_NAME=iHD
ENV PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/opt/intel/mediasdk/lib64/pkgconfig
ENV GST_VAAPI_ALL_DRIVERS=1
ENV LIBRARY_PATH=/usr/lib
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/intel/mediasdk/lib64

RUN apt-get install -y \
    libusb-1.0-0-dev \
    libboost-all-dev \
    libgtk-3-dev \
    python-yaml


RUN apt-get install -y libglib2.0-dev \
    gobject-introspection \
    libgirepository1.0-dev \
    libpango-1.0-0 \
    libpangocairo-1.0-0 \
    autopoint

RUN apt-get install -y libxrandr-dev \
    libegl1-mesa-dev \
    bison \
    flex \
    libudev-dev

RUN apt-get install -y libxv-dev \
    libvisual-0.4-dev \
    libtheora-dev \
    libglib2.0-dev \
    libasound2-dev \
    libcdparanoia-dev \
    libgl1-mesa-dev \
    libpango1.0-dev

RUN apt-get install -y libssl-dev

RUN apt-get -y install automake
COPY basler-source-plugin ./basler-source-plugin
COPY install_basler_gstreamer_plugin.sh .
RUN chmod +x install_basler_gstreamer_plugin.sh && \
    ./install_basler_gstreamer_plugin.sh


RUN apt-get install -y \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev

ENV InferenceEngine_DIR=/opt/intel/dldt/inference-engine/share

ENV PYTHONPATH ${PYTHONPATH}:.

ENV DEBIAN_FRONTEND="noninteractive" \
    MFX_HOME=$MFX_HOME:"/opt/intel/mediasdk/" \
    PKG_CONFIG_PATH=$PKG_CONFIG_PATH:"/opt/intel/mediasdk" \
    LIBVA_DRIVERS_PATH=$LIBVA_DRIVERS_PATH:"/usr/lib/x86_64-linux-gnu/dri/" \
    LIBVA_DRIVER_NAME="iHD" \
    LD_RUN_PATH="/usr/lib" \
    LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/opt/intel/mediasdk/lib/:/opt/intel/mediasdk/share/mfx/samples:/usr/local/lib" \
    TERM="xterm" \
    GST_DEBUG="1" \
    LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libxcb-dri3.so" \
    MODELS_PATH=$MODELS_PATH:"${PY_WORK_DIR}/VideoIngestion/models/"

# Installing dependent python modules - needed by opencv
COPY vi_requirements.txt .
RUN pip3.6 install -r vi_requirements.txt && \
    rm -rf vi_requirements.txt

FROM ia_common:$EIS_VERSION as common
FROM ia_video_common:$EIS_VERSION as video_common

FROM openvino

WORKDIR ${GO_WORK_DIR}

COPY --from=common /usr/local/include /usr/local/include
COPY --from=common /usr/local/lib /usr/local/lib
COPY --from=common ${GO_WORK_DIR}/common/cmake ./common/cmake
COPY --from=common ${GO_WORK_DIR}/common/libs ./common/libs
COPY --from=common ${GO_WORK_DIR}/common/util ${GO_WORK_DIR}/common/util
COPY --from=common /usr/local/lib/python3.6/dist-packages/ /usr/local/lib/python3.6/dist-packages

ARG CMAKE_BUILD_TYPE

COPY --from=video_common ${GO_WORK_DIR}/common/UDFLoader ./common/libs/UDFLoader
COPY --from=video_common ${GO_WORK_DIR}/common/udfs ./common/udfs

# Build UDF loader lib
RUN /bin/bash -c "source /opt/intel/openvino/bin/setupvars.sh && \
    cd ./common/libs/UDFLoader && \
    rm -rf build && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} .. && \
    make install"

COPY --from=video_common ${GO_WORK_DIR}/common/udfs/native ./common/udfs/native

# Build native UDF samples
RUN /bin/bash -c "source /opt/intel/openvino/bin/setupvars.sh && \
    cd ./common/udfs/native && \
    rm -rf build && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} .. && \
    make && \
    make install"

ENV LD_LIBRARY_PATH ${LD_LIBRARY_PATH}:/usr/local/lib/udfs/

# Adding project depedency modules
COPY . ./VideoIngestion/
RUN mv VideoIngestion/models . && mv VideoIngestion/test_videos .

ARG WITH_PROFILE
RUN /bin/bash -c "source /opt/intel/openvino/bin/setupvars.sh && \
    cd ./VideoIngestion && \
    rm -rf build && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DWITH_PROFILE=${WITH_PROFILE} .. && \
    make"

# Removing build dependencies
RUN apt-get remove -y wget && \
    apt-get remove -y git && \
    apt-get remove -y curl && \
    apt-get autoremove -y

COPY --from=video_common ${GO_WORK_DIR}/common/udfs/python ./common/udfs/python

ENV PYTHONPATH ${PYTHONPATH}:${GO_WORK_DIR}/common/udfs/python:${GO_WORK_DIR}/common/

ENTRYPOINT ["VideoIngestion/vi_start.sh"]
