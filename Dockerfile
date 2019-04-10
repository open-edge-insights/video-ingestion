# Dockerfile for VideoIngestion
ARG IEI_VERSION
FROM ia_pybase:$IEI_VERSION
LABEL description="VideoIngestion image"

ARG IEI_UID
RUN useradd -r -u ${IEI_UID} -G video ieiuser

# Adding basler camera's essentials by referring it's repo's README
RUN wget https://www.baslerweb.com/media/downloads/software/pylon_software/pylon-5.1.0.12682-x86_64.tar.gz && \
    tar xvf pylon-5.1.0.12682-x86_64.tar.gz && \
    cd pylon-5.1.0.12682-x86_64 && \
    pip3.6 install numpy==1.14.5 && \
    pip3.6 install setuptools==40.7.3 && \
    tar -C /opt -zxf pylonSDK-5.1.0.12682-x86_64.tar.gz

# Removing unwanted files
RUN rm -rf pylon-5.1.0.12682-x86_64.tar.gz && \
    rm -rf pylon-5.1.0.12682-x86_64/pylonSDK-5.1.0.12682-x86_64.tar.gz

RUN apt-get update

# Installing python boost dependencies
RUN apt-get install -y libboost-python-dev

# Installing dependent python modules
ADD VideoIngestion/vi_requirements.txt .
RUN pip3.6 install -r vi_requirements.txt && \
    rm -rf vi_requirements.txt

ENV PYLON_CAMEMU 1
# Adding gstreamer capabilities
ENV TERM xterm
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y install curl unzip vim wget gcc
RUN apt-get -y install libjpeg8-dev libtiff5-dev libjasper-dev libpng12-dev
RUN apt-get -y install libavcodec-dev libavformat-dev libswscale-dev libv4l-dev
RUN apt-get -y install libxvidcore-dev libx264-dev
RUN apt-get -y install libsm6 libxext6 libxrender-dev
RUN apt-get -y install libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libudev-dev libwayland-dev libglfw3-dev \
    libgles2-mesa-dev libgstreamer-plugins-bad1.0-dev gstreamer1.0-plugins-bad
RUN apt-get -y install git
RUN apt-get -y install dh-autoreconf
RUN apt-get -y  install autoconf libtool libdrm-dev xorg xorg-dev openbox libx11-dev libgl1-mesa-glx libgl1-mesa-dev
RUN apt -y remove cmake
#Install cmake
RUN wget -O cmake.sh https://github.com/Kitware/CMake/releases/download/v3.13.1/cmake-3.13.1-Linux-x86_64.sh
RUN chmod 777 cmake.sh
RUN mkdir /opt/cmake
RUN ./cmake.sh --skip-license --prefix=/opt/cmake
RUN ln -s /opt/cmake/bin/cmake /usr/bin/cmake

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
RUN mkdir /opt/src
RUN cd /opt/src && \
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
# gstreamer-media-sdk
RUN git clone https://github.com/intel/gstreamer-media-SDK.git
RUN sed -i "/^[ ]*parsers/i    /opt/intel/mediasdk/include/mfx" gstreamer-media-SDK/CMakeLists.txt
RUN sed -i "s/libmfx.a/libmfx.so/" gstreamer-media-SDK/cmake/FindMediaSDK.cmake
RUN cd /opt/intel/mediasdk/lib/ && mkdir lin_x64 && cd lin_x64 && ln -s ../* .
RUN cd gstreamer-media-SDK && mkdir build && cd build && cmake .. && make -j8 && make install

# Adding capabilities to install basler source plugin
RUN apt-get -y install automake
ADD basler-source-plugin ./basler-source-plugin
RUN cd basler-source-plugin && ./autogen.sh && make && make install
RUN cd basler-source-plugin && \
    cp ./plugins/libgstpylonsrc.la /usr/lib/x86_64-linux-gnu/gstreamer-1.0/ && \
    cp ./plugins/.libs/libgstpylonsrc.so /usr/lib/x86_64-linux-gnu/gstreamer-1.0/

# Adding cert dirs
RUN mkdir -p /etc/ssl/imagestore

# Adding project depedency modules
ADD algos ./algos
ADD DataAgent/__init__.py ./DataAgent/__init__.py
ADD DataAgent ./DataAgent
ADD DataIngestionLib ./DataIngestionLib
ADD ImageStore ./ImageStore
ADD Util ./Util
# Adding VideoIngestion & test program
ADD VideoIngestion/VideoIngestion.py .
ADD VideoIngestion/test .

RUN mkdir -p /etc/ssl/ca
ENV PYTHONPATH ${PYTHONPATH}:./DataAgent/da_grpc/protobuff/py:./DataAgent/da_grpc/protobuff/py/pb_internal:./ImageStore/protobuff/py/

RUN chown -R ${IEI_UID} /etc/ssl/

# Set graphics driver ownership
RUN rm /usr/lib/x86_64-linux-gnu/libva.so
RUN ln -s /usr/lib/x86_64-linux-gnu/libva.so.1.3900.0 /usr/lib/x86_64-linux-gnu/libva.so
RUN chown ${IEI_UID} /usr/lib/x86_64-linux-gnu/libva.so.1.3900.0
RUN chown ${IEI_UID} /usr/lib/x86_64-linux-gnu/libva.so

ENTRYPOINT ["python3.6", "VideoIngestion.py", "--log-dir", "/IEI/video_ingestion_logs"]
HEALTHCHECK NONE
