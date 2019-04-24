# Video Ingestion Module:
This module injests the video data from a file or a basler/RTSP camera and sends it to Image Store and influx using Data Ingestion Lib.

This is the reference for all the algorithms used in IEI

## Configuration
[factory_video_file.json](https://gitlab.devtools.intel.com/Indu/IEdgeInsights/IEdgeInsights/blob/master/docker_setup/config/factory_video_file.json)
is the configuration file where algorithm related configurations have been made with the following entires:

### 1.Basler Camera:

If you are working with a Basler camera, then put the stream in 'capture_streams' using the below gstreamer pipeline.
The sample configurations is available in [factory_basler.json](../docker_setup/config/factory_basler.json).

Note: In case multiple Basler cameras are connected use serial parameter to specify the camera to be used in the gstreamer pipeline in the video config file
for camera mode. If multiple cameras are connected and the serial parameter is not specified then the source plugin by default connects to camera with device_index=0.

Example Pipeline to connect to basler camera with serial number 22573664 :
**"capture_streams":"pylonsrc serial=22573664 imageformat=yuv422 exposure=3250 interpacketdelay=1500 ! videoconvert ! appsink"**

### 2.USB Camera:

To work with USB Camera, then put the stream in 'capture_streams' using the below gstreamer pipeline.
The sample configuration is available in [factory_usb.json](../docker_setup/config/factory_usb.json).

### 3.RTSP Camera:

If you are working with a hikvision ds2 RTSP camera, then put the stream in 'capture_streams' using the below gstreamer pipeline.
The sample configuration is available in [factory_rtsp_hikvision_ds2.json](../docker_setup/config/factory_rtsp_hikvision_ds2.json).
Please add the IP address of the RSTP camera in .env file to add it to no_proxy.

If you are working with a stimulated RTSP stream generated using cvlc command , then put the stream in 'capture_streams' using the below gstreamer pipeline.
The sample configuration is available in [factory_rtsp_cvlc.json](../docker_setup/config/factory_rtsp_cvlc.json).

### Multiple camera configuration for RTSP cameras

If one wants to add multiple RTSP cameras, they can do so by having a json object for `capture_streams` key with each key under this being a `serial number` of the camera and the json object it is pointing to, has all the configuration details for that camera. For reference, one can use [factory_mutli_cam.json](../docker_setup/config/factory_multi_cam.json) and do the necessary tweaks.


> **Note**:
> In order to use the RTSP stream, the RTSP server must be started using VCL with the following command:
`cvlc -vvv file://${HOME}/Videos/test_videos/pcb_d2000.avi --sout '#gather:rtp{sdp=rtsp://localhost:8554/}' --loop --sout-keep`
Please ensure the `pcb_d2000.avi` video file must be at ~/Videos/test_videos/ on your system where the RTSP server will be started.

Gstreamer mediaSDK decoding commands requires there to be a parser and then the the decoder "h265parse ! mhevcdec".
parsers and decoders:
h264parse !  mfx264dec
h265parse ! mfxhevcdec
mfxmpegvideoparse ! mfxmpeg2dec

> **Note**:
> 1. In case encoding option needs to be enabled add the `encoding` key in the video config json files.
The supported encoding types are `jpg` and `png`.
The supported encoding level for `jpg` is `0-100`.
The supported encoding level for `png` is `0-9`.
> 2. In case resizing option needs to be enabled add the `resize_resolution ` key in the video config files.
The `resize_resolution` key takes frame width and frame height as, `resize_resolution = width x height`.
> 3. In case the `encoding` or `resize_resolution` key is not added encoding or resizing will not be enabled.
> 4. In case one needs to specify the image storage type, one can use the `img_store_type` key.
The supported storage types are `inmemory`, `persistent` and `inmemory_persistent`.
`inmemory_persistent` option will add the image to both the inmemory and persistent storage.
In case the `img_store_type` key is not specified, then by default the image will be added to inmemory storage.

The following sections describe each of the major components of the configuration
structure specified above.

### Classifiers

Classifiers represent the different classification algorithms to use on the
video frames that are ingested into the agent.

The the keys in the `classifiers` object in the configuration for the agent
represent the classifier you wish to have the agent load. The value associated
with that key is the configuration for that classifier.

It is important to note that if the specified classifier does not exist, then
the agent will fail to start. In addition, if the configuration for the
classifier is not correct, then the classifier will fail to be loaded.


#### Supported Classifiers

| Classifier | Reference |
| :--------: | :-----------: |
| pcbdemo     | [Link](https://gitlab.devtools.intel.com/Indu/IEdgeInsights/IEdgeInsights/tree/master/algos/dpm/classification/classifiers/pcbdemo) |

### Ingestion

The ingestion portion of the configuration represents the different ingestors
for the agent to load. The `ingestors` key inside of this configuration
represents which ingestors to load into the ingestion pipeline. Each key in
the `ingestors` object represents an ingestor to load and the value associated
with that key is the configuration for that ingestor.

It is important to note that if the specified ingestor does not exist, then
the agent will fail to start. In addition, if the configuration for the
ingestor is not correct, then the classifier will fail to be loaded.

See the documentation for the ingestors you wish to have loaded for how to
configure them.

#### Supported Ingestors

| Ingestor | Documentation |
| :------: | :-----------: |
| video_file    | [Link](https://gitlab.devtools.intel.com/Indu/IEdgeInsights/IEdgeInsights/blob/master/algos/dpm/ingestion/video_file.py ) |
| video         | [Link](https://gitlab.devtools.intel.com/Indu/IEdgeInsights/IEdgeInsights/blob/master/algos/dpm/ingestion/video.py ) |
