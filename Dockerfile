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
    pip3.6 install numpy==1.14.5 && \
    pip3.6 install setuptools==40.7.3 && \
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
    make -j$(nproc) && \
    make install

# Build YASM
ARG YASM_VER=1.3.0
ARG YASM_REPO=https://www.tortall.net/projects/yasm/releases/yasm-${YASM_VER}.tar.gz
RUN wget -O - ${YASM_REPO} | tar xz && \
    cd yasm-${YASM_VER} && \
    sed -i "s/) ytasm.*/)/" Makefile.in && \
    ./configure --prefix="/usr" --libdir=/usr/lib/x86_64-linux-gnu && \
    make -j$(nproc) && \
    make install

# Build x264
ARG X264_VER=stable
ARG X264_REPO=https://github.com/mirror/x264

RUN git clone ${X264_REPO} && \
    cd x264 && \
    git checkout ${X264_VER} && \
    ./configure --prefix="/usr" --libdir=/usr/lib/x86_64-linux-gnu --enable-shared && \
    make -j$(nproc) && \
    make install DESTDIR="/home/build" && \
    make install

# Build x265
ARG X265_VER=2.9
ARG X265_REPO=https://github.com/videolan/x265/archive/${X265_VER}.tar.gz

RUN apt-get install -y libnuma-dev

RUN wget -O - ${X265_REPO} | tar xz && mv x265-${X265_VER} x265 && \
    cd x265/build/linux && \
    cmake -DBUILD_SHARED_LIBS=ON -DENABLE_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/usr -DLIB_INSTALL_DIR=/usr/lib/x86_64-linux-gnu ../../source && \
    make -j$(nproc) && \
    make install DESTDIR="/home/build" && \
    make install

# Fetch SVT-HEVC
ARG SVT_HEVC_VER=20a47b0d904e9d99e089d93d7c33af92788cbfdb
ARG SVT_HEVC_REPO=https://github.com/intel/SVT-HEVC

RUN git clone ${SVT_HEVC_REPO} && \
    cd SVT-HEVC/Build/linux && \
    git checkout ${SVT_HEVC_VER} && \
    mkdir -p ../../Bin/Release && \
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu -DCMAKE_ASM_NASM_COMPILER=yasm ../.. && \
    make -j$(nproc) && \
    make install DESTDIR=/home/build && \
    make install

# Build libdrm
ARG LIBDRM_VER=2.4.96
ARG LIBDRM_REPO=https://dri.freedesktop.org/libdrm/libdrm-${LIBDRM_VER}.tar.gz

RUN apt-get install -y libpciaccess-dev

RUN wget -O - ${LIBDRM_REPO} | tar xz && \
    cd libdrm-${LIBDRM_VER} && \
    ./configure --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu && \
    make -j$(nproc) && \
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

# Build the gstreamer core
ARG GST_VER=1.16.0
ARG GST_REPO=https://gstreamer.freedesktop.org/src/gstreamer/gstreamer-${GST_VER}.tar.xz

RUN apt-get install -y libglib2.0-dev \
    gobject-introspection \
    libgirepository1.0-dev \
    libpango-1.0-0 \
    libpangocairo-1.0-0 \
    autopoint

RUN wget -O - ${GST_REPO} | tar xJ && \
    cd gstreamer-${GST_VER} && \
    ./autogen.sh \
    --prefix=/usr \
    --libdir=/usr/lib/x86_64-linux-gnu \
    --libexecdir=/usr/lib/x86_64-linux-gnu \
    --enable-shared \
    --enable-introspection \
    --disable-examples  \
    --disable-gtk-doc && \
    make -j $(nproc) && \
    make install DESTDIR=/home/build && \
    make install;

RUN apt-get install -y libxrandr-dev \
    libegl1-mesa-dev \
    bison \
    flex \
    libudev-dev

# Build the gstreamer plugin base
ARG GST_PLUGIN_BASE_REPO=https://gstreamer.freedesktop.org/src/gst-plugins-base/gst-plugins-base-${GST_VER}.tar.xz

RUN apt-get install -y libxv-dev \
    libvisual-0.4-dev \
    libtheora-dev \
    libglib2.0-dev \
    libasound2-dev \
    libcdparanoia-dev \
    libgl1-mesa-dev \
    libpango1.0-dev

RUN wget -O - ${GST_PLUGIN_BASE_REPO} | tar xJ && \
    cd gst-plugins-base-${GST_VER} && \
    ./autogen.sh \
    --prefix=/usr \
    --libdir=/usr/lib/x86_64-linux-gnu \
    --libexecdir=/usr/lib/x86_64-linux-gnu \
    --enable-introspection \
    --enable-shared \
    --disable-examples  \
    --disable-gtk-doc && \
    make -j $(nproc) && \
    make install DESTDIR=/home/build && \
    make install

# Build the gstreamer plugin bad set
ARG GST_PLUGIN_BAD_REPO=https://gstreamer.freedesktop.org/src/gst-plugins-bad/gst-plugins-bad-${GST_VER}.tar.xz

RUN apt-get install -y libssl-dev

RUN wget -O - ${GST_PLUGIN_BAD_REPO} | tar xJ && \
    cd gst-plugins-bad-${GST_VER} && \
    ./autogen.sh \
    --prefix=/usr \
    --libdir=/usr/lib/x86_64-linux-gnu \
    --libexecdir=/usr/lib/x86_64-linux-gnu \
    --enable-shared \
    --disable-examples  \
    --disable-gtk-doc && \
    cd gst-libs/gst/codecparsers/ && make && make install DESTDIR=/home/build && make install && cd ../../../ \
    cd gst/videoparsers && make && make install DESTDIR=/home/build && make install

# Adding Gstreamer Plugin Installation Dependencies
RUN apt-get -y install automake
COPY basler-source-plugin ./basler-source-plugin
COPY install_gstreamerplugins.sh .
RUN chmod 777 install_gstreamerplugins.sh .
RUN ./install_gstreamerplugins.sh ${EIS_UID} /EIS

# Build gstreamer plugin for svt
RUN cd SVT-HEVC/gstreamer-plugin && \
    cmake . && \
    make -j$(nproc) && \
    make install DESTDIR=/home/build && \
    make install

# Build gstreamer plugin vaapi
ARG GST_PLUGIN_VAAPI_REPO=https://gstreamer.freedesktop.org/src/gstreamer-vaapi/gstreamer-vaapi-${GST_VER}.tar.xz

RUN wget -O - ${GST_PLUGIN_VAAPI_REPO} | tar xJ && \
    cd gstreamer-vaapi-${GST_VER} && \
     ./autogen.sh \
        --prefix=/usr \
        --libdir=/usr/local/lib/gstreamer-1.0 \
        --libexecdir=/usr/local/lib/gstreamer-1.0 \
        --libdir=/usr/lib/x86_64-linux-gnu \
        --libexecdir=/usr/lib/x86_64-linux-gnu \
        --enable-shared \
        --disable-examples \
        --disable-gtk-doc  && \
     make -j $(nproc) && \
     make install DESTDIR=/home/build && \
     make install

RUN apt-get install -y \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev

RUN apt-get install -y gtk-doc-tools

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
    GST_PLUGIN_PATH="/usr/local/lib/gstreamer-1.0" \
    GST_DEBUG="1" \
    LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libxcb-dri3.so"

# Installing GVA plugins
RUN mkdir gva && \
    cd gva && \
    git clone https://github.com/opencv/gst-video-analytics.git

RUN apt-get update && apt install -y --no-install-recommends \
       gcc \
       mesa-utils \
       ocl-icd-libopencl1 \
       clinfo \
       vainfo

# Build GVA plugin
RUN /bin/bash -c "source /opt/intel/openvino/bin/setupvars.sh && \
      mkdir gva/gst-video-analytics/build && \
      cd gva/gst-video-analytics/build && \
      cmake .. && \
      make -j $(nproc)"

# Export environment variables
ENV MODELS_PATH="${PY_WORK_DIR}/VideoIngestion/models/" \
    GST_PLUGIN_PATH=$GST_PLUGIN_PATH:"${PY_WORK_DIR}/gva/gst-video-analytics/build/intel64/Release/lib"

FROM ia_common:$EIS_VERSION as common

FROM openvino

WORKDIR ${GO_WORK_DIR}

COPY --from=common /usr/local/include /usr/local/include
COPY --from=common /usr/local/lib /usr/local/lib
COPY --from=common ${GO_WORK_DIR}/common/cmake ./common/cmake
COPY --from=common ${GO_WORK_DIR}/common/libs ./common/libs

# Build UDF loader lib
RUN /bin/bash -c "source /opt/intel/openvino/bin/setupvars.sh && \
    cd ./common/libs/UDFLoader && \
    rm -rf build && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make install"

# Adding project depedency modules
COPY . ./VideoIngestion/
RUN mv VideoIngestion/models .

RUN /bin/bash -c "source /opt/intel/openvino/bin/setupvars.sh && \
    cd ./VideoIngestion && \
    rm -rf build && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make"

ENTRYPOINT ["VideoIngestion/vi_start.sh"]