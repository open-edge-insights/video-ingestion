#!/bin/bash
arg=`echo $1`
source /opt/intel/openvino/bin/setupvars.sh
./VideoIngestion/build/video-ingestion $arg
