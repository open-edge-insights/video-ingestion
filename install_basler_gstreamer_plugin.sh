#!/bin/bash +e

source /opt/intel/openvino/bin/setupvars.sh

echo "Installing Basler Plugin"
cd basler-source-plugin && \
   autoreconf -i

  ./autogen.sh && \
   make && \
   make install

echo "Removing Basler Plugin Sources"
rm -rf basler-source-plugin/
