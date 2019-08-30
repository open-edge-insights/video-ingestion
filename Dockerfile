# Dockerfile for VideoIngestion
ARG EIS_VERSION
FROM ia_pybase:$EIS_VERSION as pybase
LABEL description="VideoIngestion image"

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
    apt-get install -y libboost-python-dev

ENV PYLON_CAMEMU 1
# Adding gstreamer capabilities
ENV TERM xterm
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y install curl unzip vim wget gcc libjpeg8-dev libtiff5-dev libpng-dev \
    libavcodec-dev libavformat-dev libswscale-dev libv4l-dev libxvidcore-dev libx264-dev \
    libsm6 libxext6 libxrender-dev libgstreamer1.0-0 gstreamer1.0-plugins-base \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libudev-dev libwayland-dev libglfw3-dev \
    libgles2-mesa-dev libgstreamer-plugins-bad1.0-dev git dh-autoreconf autoconf libtool libdrm-dev \
    xorg xorg-dev openbox libx11-dev libgl1-mesa-glx libgl1-mesa-dev
RUN apt -y remove cmake
#Install cmake
RUN wget -O cmake.sh https://github.com/Kitware/CMake/releases/download/v3.13.1/cmake-3.13.1-Linux-x86_64.sh && \
    chmod 777 cmake.sh && \
    mkdir /opt/cmake && \
    ./cmake.sh --skip-license --prefix=/opt/cmake && \
    ln -s /opt/cmake/bin/cmake /usr/bin/cmake

# Installing opencv
ENV OPENCV_VERSION 4.1.1
RUN wget -O opencv.zip https://github.com/Itseez/opencv/archive/${OPENCV_VERSION}.zip
RUN unzip opencv.zip
RUN mkdir opencv-${OPENCV_VERSION}/build && cd opencv-${OPENCV_VERSION}/build && cmake -DCMAKE_BUILD_TYPE=Release -DPYTHON3_EXECUTABLE=`which python3.6` \
	-DPYTHON_DEFAULT_EXECUTABLE=`which python3.6` -DENABLE_PRECOMPILED_HEADERS=OFF -DCMAKE_CXX_FLAGS=-std=c++11 -D WITH_GSTREAMER=ON ..
RUN cd opencv-${OPENCV_VERSION}/build && make -j$(nproc)
RUN cd opencv-${OPENCV_VERSION}/build && make install
# gmmlib
RUN git clone https://github.com/intel/gmmlib.git && cd gmmlib && mkdir build && cd build && cmake .. && make -j$(nproc) && make install
# libva
RUN mkdir /opt/src
RUN cd /opt/src && \
    curl -o libva-master.zip -sSL https://github.com/intel/libva/archive/master.zip && \
    unzip libva-master.zip && \
    cd libva-master && \
    ./autogen.sh --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu && \
    make -j$(nproc) && \
    make install

RUN cd /opt/src && \
    curl -sSLO https://www.samba.org/ftp/ccache/ccache-3.2.8.tar.bz2 && \
    tar xf ccache-3.2.8.tar.bz2 && \
    cd ccache-3.2.8 && \
    ./configure --prefix=/usr && \
    make -j$(nproc) && \
    make install

RUN mkdir -p /usr/lib/ccache && \
    cd /usr/lib/ccache && \
    ln -sf /usr/bin/ccache gcc && \
    ln -sf /usr/bin/ccache g++ && \
    ln -sf /usr/bin/ccache cc && \
    ln -sf /usr/bin/ccache c++ && \
    ln -sf /usr/bin/ccache clang && \
    ln -sf /usr/bin/ccache clang++ && \
    ln -sf /usr/bin/ccache clang-4.0 && \
    ln -sf /usr/bin/ccache clang++-4.0
ENV PATH /usr/lib/ccache:$PATH
# VAAPI driver
RUN git clone https://github.com/intel/media-driver.git && cd media-driver && mkdir build && cd build && cmake .. && make -j$(nproc) && make install
# Media SDK
RUN git clone https://github.com/Intel-Media-SDK/MediaSDK.git msdk && cd msdk && mkdir build && cd build && cmake -DENABLE_OPENCL=OFF .. && make -j$(nproc) && make install
# gstreamer-media-sdk
RUN git clone https://github.com/intel/gstreamer-media-SDK.git 
RUN sed -i "/^[ ]*parsers/i    /opt/intel/mediasdk/include/mfx" gstreamer-media-SDK/CMakeLists.txt 
RUN sed -i "s/libmfx.a/libmfx.so/" gstreamer-media-SDK/cmake/FindMediaSDK.cmake 
RUN cd /opt/intel/mediasdk/lib/ && mkdir lin_x64 && cd lin_x64 && ln -s ../* . 
RUN cd gstreamer-media-SDK && mkdir build && cd build && cmake .. && make -j$(nproc) && make install

# Adding Gstreamer Plugin Installation Dependencies
RUN apt-get -y install automake gstreamer1.0-tools
COPY basler-source-plugin ./basler-source-plugin
COPY install_gstreamerplugins.sh .
RUN chmod 777 install_gstreamerplugins.sh . 
RUN ./install_gstreamerplugins.sh ${EIS_UID} /EIS

# Set graphics driver ownership
RUN rm /usr/lib/x86_64-linux-gnu/libva.so && \
    ln -s /usr/lib/x86_64-linux-gnu/libva.so.2.600.0 /usr/lib/x86_64-linux-gnu/libva.so && \
    chown ${EIS_UID} /usr/lib/x86_64-linux-gnu/libva.so.2.600.0 && \
    chown ${EIS_UID} /usr/lib/x86_64-linux-gnu/libva.so

# Installing dependent python modules
COPY vi_requirements.txt .
RUN pip3.6 install -r vi_requirements.txt && \
    rm -rf vi_requirements.txt 

ENV PYTHONPATH ${PYTHONPATH}:.

FROM ia_common:$EIS_VERSION as common

FROM pybase

COPY --from=common /libs ${PY_WORK_DIR}/libs
COPY --from=common /Util ${PY_WORK_DIR}/Util

RUN cd ./libs/EISMessageBus && \
    rm -rf build deps && \
    mkdir build && \
    cd build && \
    cmake -DWITH_PYTHON=ON .. && \
    make && \
    make install

ENV DEBIAN_FRONTEND="noninteractive" \
    MFX_HOME="/opt/intel/mediasdk/" \
    PKG_CONFIG_PATH="/opt/intel/mediasdk" \
    LIBVA_DRIVERS_PATH="/usr/lib/x86_64-linux-gnu/dri/" \
    LIBVA_DRIVER_NAME="iHD" \
    LD_RUN_PATH="/usr/lib" \
    LD_LIBRARY_PATH="/opt/intel/mediasdk/lib/:/opt/intel/mediasdk/share/mfx/samples:/usr/local/lib" \
    TERM="xterm" \
    GST_PLUGIN_PATH="/usr/local/lib/gstreamer-1.0" \
    GST_DEBUG="1"

# Adding project depedency modules
COPY . ./VideoIngestion/

ENTRYPOINT ["python3.6", "VideoIngestion/video_ingestion.py"]
HEALTHCHECK NONE

