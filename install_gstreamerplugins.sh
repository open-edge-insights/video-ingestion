#!/bin/bash -e

# Building & Making Gstreamer Good Plugins
# This script helps to install the gstreamer plugins 
# and removes the source code of plugins after installtion.

# Usage install_gstreamerplugins.sh {USER_ID} {WORK_DIR}

gstreamer_version=$(gst-inspect-1.0 --version | grep -Po "(?<=GStreamer )([0-9]|\.)*(?=\s|$)")
WORK_DIR=$2
echo "Building Gstreamer Plugins For Gstreamer Version : " $gstreamer_version
echo "Cloning Good Plugins"
cd $WORK_DIR && git clone https://gitlab.freedesktop.org/gstreamer/gst-plugins-good.git

# Cloning Common gstreamer dependency file for Gstreamer 
cd gst-plugins-good/ && git clone https://gitlab.freedesktop.org/gstreamer/common.git

echo "Switching Plugin Branch to Gstreamer Version : " $gstreamer_version
git checkout $gstreamer_version
cd $WORK_DIR/gst-plugins-good/

echo "Generating Libraries"
set +e
autoreconf -i
./autogen.sh --disable-gtk-doc
set -e

echo "Installing V4l2Src Plugin"
# Installing v4l2src plugin 
cd $WORK_DIR/gst-plugins-good/sys/v4l2 && make && make install

echo "Installing RTSP & Dependent Plugins"
# Installing rtp plugins
cd $WORK_DIR/gst-plugins-good/gst/rtp && make && make install
cd $WORK_DIR/gst-plugins-good/gst/rtpmanager && make && make install
cd $WORK_DIR/gst-plugins-good/gst/rtsp && make && make install
cd $WORK_DIR/gst-plugins-good/gst/udp && make && make install

echo "Cloning Bad Plugins"
cd $WORK_DIR && git clone https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad.git
# Cloning Common gstreamer dependency file for Gstreamer 
cd gst-plugins-bad/ && git clone https://gitlab.freedesktop.org/gstreamer/common.git

echo "Switching Plugin Branch to Gstreamer Version : " $gstreamer_version
git checkout $gstreamer_version

cd $WORK_DIR/gst-plugins-bad/
echo "Generating Libraries"
set +e
autoreconf -i
./autogen.sh --disable-gtk-doc --libexecdir=/usr/local/lib/gstreamer-1.0 --libdir=/usr/local/lib/gstreamer-1.0 --enable-shared 
set -e

echo "Installing RTSP Dependent Codecs"
cd $WORK_DIR/gst-plugins-bad/gst-libs/gst/codecparsers && make && make install
cd $WORK_DIR/gst-plugins-bad/gst/videoparsers && make && make install

echo "Installing Basler Plugin"
set +e
cd $WORK_DIR/basler-source-plugin && autoreconf -i 
cd $WORK_DIR/basler-source-plugin && ./autogen.sh && make && make install
set -e

echo "Removing Plugin Sources"
rm -rf $WORK_DIR/gst-plugins-bad/
rm -rf $WORK_DIR/gst-plugins-good/
rm -rf $WORK_DIR/basler-source-plugin/
