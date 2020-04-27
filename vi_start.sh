#!/bin/bash

source /opt/intel/openvino/bin/setupvars.sh

udevadm control --reload-rules
udevadm trigger

# FPGA environment
source ~/fpga_env.sh

# Adding path of libcpu_extension.so to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$GO_WORK_DIR/common/udfs/native/build/ie_cpu_extension

# Adding path of basler source plugin
export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:"/usr/local/lib/gstreamer-1.0"

./VideoIngestion/build/video-ingestion
