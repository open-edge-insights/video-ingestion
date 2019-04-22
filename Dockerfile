# Dockerfile for VideoIngestion
ARG IEI_VERSION
FROM ia_pybase:$IEI_VERSION
LABEL description="VideoIngestion image"

ARG IEI_UID
RUN adduser -S -u ${IEI_UID} -G video ieiuser  

RUN apk add --no-cache --virtual .build-deps \
    wget \
    curl \
    unzip \
    bash \
    vim \
    git \
    build-base \
    autoconf \ 
    automake \
    cmake

# Adding basler camera's essentials by referring it's repo's README
RUN wget https://www.baslerweb.com/media/downloads/software/pylon_software/pylon-5.1.0.12682-x86_64.tar.gz && \
    tar xvf pylon-5.1.0.12682-x86_64.tar.gz && \
    cd pylon-5.1.0.12682-x86_64 && \
    pip3.6 install numpy==1.14.5 && \
    pip3.6 install setuptools==40.7.3 && \
    mkdir /opt && \
    tar -C /opt -zxf pylonSDK-5.1.0.12682-x86_64.tar.gz

# Removing unwanted files
RUN rm -rf pylon-5.1.0.12682-x86_64.tar.gz && \
    rm -rf pylon-5.1.0.12682-x86_64/pylonSDK-5.1.0.12682-x86_64.tar.gz

# Installing dependent python modules
ADD VideoIngestion/vi_requirements.txt .
RUN pip3.6 install -r vi_requirements.txt && \
    rm -rf vi_requirements.txt

ENV PYLON_CAMEMU 1
# Adding gstreamer capabilities
ENV TERM xterm
ENV DEBIAN_FRONTEND=noninteractive

ENV PACKAGES "\
    ffmpeg-dev \
    v4l-utils-dev \
    xvidcore-dev \
    x264-dev \
    libsm \
    libxext \    
    libxrender-dev \
    eudev-dev \
    wayland-dev \
    glfw-dev \
    mesa-dev \
    libtool \
    libdrm-dev \
    xorg-server \
    xorg-server-dev \
    openbox \
    libx11-dev \
    mesa-gl \
    boost-dev \
    gcc \
    jpeg-dev \
    tiff-dev \
    jasper-dev \
    libpng-dev \
    gstreamer \
    gst-plugins-base \
    gst-plugins-good \
    gstreamer-dev \
    gst-plugins-base-dev \
    gst-plugins-bad \
    gst-plugins-bad-dev \
    openssl-dev \
    libva-dev \
    "

RUN apk add --no-cache $PACKAGES

# Installing opencv
ENV OPENCV_VERSION 3.4.3
RUN wget -O opencv.zip https://github.com/Itseez/opencv/archive/${OPENCV_VERSION}.zip
RUN unzip opencv.zip
RUN mkdir opencv-${OPENCV_VERSION}/build && cd opencv-${OPENCV_VERSION}/build && cmake -DCMAKE_BUILD_TYPE=Release -DPYTHON3_EXECUTABLE=`which python3.6` \
	-DPYTHON_DEFAULT_EXECUTABLE=`which python3.6` -DENABLE_PRECOMPILED_HEADERS=OFF -DCMAKE_CXX_FLAGS=-std=c++11 -D WITH_GSTREAMER=ON ..
RUN cd opencv-${OPENCV_VERSION}/build && make -j6
RUN cd opencv-${OPENCV_VERSION}/build && make install

# gmmlib
RUN git clone https://github.com/intel/gmmlib.git && cd gmmlib && mkdir build && cd build && cmake .. && make -j8 && make install

# libva
RUN mkdir /opt/src && \
    cd /opt/src && \
    curl -o libva-master.zip -sSL https://github.com/intel/libva/archive/master.zip && \
    unzip libva-master.zip && \
    cd libva-master && \
    ./autogen.sh --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu && \
    make -j2 && \
    make install

RUN cd /opt/src && \
    curl -sSLO https://www.samba.org/ftp/ccache/ccache-3.2.8.tar.bz2 && \
    tar xf ccache-3.2.8.tar.bz2 && \
    cd ccache-3.2.8 && \
    ./configure --prefix=/usr && \
    make && \
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
RUN git clone https://github.com/intel/media-driver.git && cd media-driver && mkdir build && cd build && cmake .. && make -j8 && make install
# Media SDK
RUN git clone https://github.com/Intel-Media-SDK/MediaSDK.git msdk && cd msdk && mkdir build && cd build && cmake -DENABLE_OPENCL=OFF .. && make -j8 && make install
RUN mv /opt/intel/mediasdk/lib64/ /opt/intel/mediasdk/lib/
# gstreamer-media-sdk
RUN git clone https://github.com/intel/gstreamer-media-SDK.git
RUN sed -i "/^[ ]*parsers/i    /opt/intel/mediasdk/include/mfx" gstreamer-media-SDK/CMakeLists.txt
RUN sed -i "s/libmfx.a/libmfx.so/" gstreamer-media-SDK/cmake/FindMediaSDK.cmake
RUN cd /opt/intel/mediasdk/lib/ && mkdir lin_x64 && cd lin_x64 && ln -s ../* .
RUN cd gstreamer-media-SDK && mkdir build && cd build && cmake .. && make -j8 && make install

# Adding capabilities to install basler source plugin
ADD basler-source-plugin ./basler-source-plugin
RUN cd basler-source-plugin && ./autogen.sh && make && make install
RUN cd basler-source-plugin && \
    cp ./plugins/libgstpylonsrc.la /usr/lib/gstreamer-1.0/ && \
    cp ./plugins/.libs/libgstpylonsrc.so /usr/lib/gstreamer-1.0/

# Adding cert dirs
RUN mkdir -p /etc/ssl/imagestore \
    && mkdir -p /etc/ssl/ca \
    && chown -R ${IEI_UID} /etc/ssl/
ENV PYTHONPATH ${PYTHONPATH}:./DataAgent/da_grpc/protobuff/py:./DataAgent/da_grpc/protobuff/py/pb_internal:./ImageStore/protobuff/py/

# Set graphics driver ownership
RUN rm /usr/lib/x86_64-linux-gnu/libva.so
RUN ln -s /usr/lib/x86_64-linux-gnu/libva.so.1.3900.0 /usr/lib/x86_64-linux-gnu/libva.so
RUN chown ${IEI_UID} /usr/lib/libva.so && \
    apk del .build-deps

# Adding project depedency modules
ADD DataAgent/__init__.py ./DataAgent/__init__.py
ADD DataAgent ./DataAgent
ADD DataIngestionLib ./DataIngestionLib
ADD ImageStore ./ImageStore
ADD Util ./Util
# Adding VideoIngestion & test program
ADD VideoIngestion/VideoIngestion.py .
ADD VideoIngestion/test .
ADD algos ./algos

ENTRYPOINT ["python3.6", "VideoIngestion.py", "--log-dir", "/IEI/video_ingestion_logs"]
HEALTHCHECK NONE
