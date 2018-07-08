FROM ubuntu:16.04

# Setup working dictories
ADD ./libs/pylon-5.0.11.10914-x86_64.tar.gz /

# Add apt proxy configuration
RUN echo "Acquire::http::proxy \"http://proxy.jf.intel.com:911/\";" > /etc/apt/apt.conf
RUN echo "Acquire::https::proxy \"http://proxy.jf.intel.com:912/\";" >> /etc/apt/apt.conf
RUN echo "Acquire::ftp::proxy \"ftp://proxy.jf.intel.com:911/\";" >> /etc/apt/apt.conf
RUN echo "Acquire::socks::proxy \"socks://proxy.jf.intel.com:1081/\";" >> /etc/apt/apt.conf

# Install dependencies
RUN apt update
RUN apt -y install cmake gcc g++ build-essential git pkg-config
RUN apt -y install libjpeg8-dev libtiff4-dev libjasper-dev libpng12-dev
RUN apt -y install libavcodec-dev libavformat-dev libswscale-dev libv4l-dev
RUN apt -y install libgtk2.0-dev
RUN apt -y install libatlas-base-dev gfortran
RUN apt -y install python3.5-dev
RUN wget https://bootstrap.pypa.io/get-pip.py
RUN python3 get-pip.py
RUN pip install numpy
RUN git clone https://github.com/Itseez/opencv.git
RUN cd opencv && mkdir build && cd build \
    && cmake -DCMAKE_BUILE_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=/usr/local \\
    && make \
    && make install

# Install Pylon library
# RUN tar xvf /setup/pylon-5.0.11.10914-x86_64.tar.gz
RUN tar -C /opt -xzf /pylon-5.0.11.10914-x86_64/pylonSDK-5.0.11.10914-x86_64.tar.gz
