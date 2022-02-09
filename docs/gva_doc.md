# Contents

- [Contents](#contents)
  - [GStreamer Video Analytics](#gstreamer-video-analytics)

## GStreamer Video Analytics

The VideoIngestion module supports the usage of the Gstreamer Video Analytics (GVA) plugins with the GStreamer ingestor. [GVA](https://github.com/openvinotoolkit/dlstreamer_gst) is
a collection of the GStreamer elements and it enables the CNN model-based video analytics capabilities such as object detection, classification, and recognition in the GStreamer framework.

Refer to [CustomUdfs-GVASafetyGearIngestion](../CustomUdfs/GVASafetyGearIngestion/README.md) for more information on the GVA-based CustomUdf container added for the SafetyGear sample.

The GVA use case configurations with different cameras are as follows:

- Video file - Gstreamer ingestor with GVA elements

```javascript
  {
    "type": "gstreamer",
    "pipeline": "multifilesrc loop=TRUE stop-index=0 location=./test_videos/<VIDEO_FILE> ! h264parse ! decodebin ! videoconvert ! video/x-raw,format=BGR ! gvadetect model=models/<DETECTION_MODEL> ! appsink"
  }
```

  > Note
  >
  > To use GVA with a video file using the `multifilesrc` element, copy the video file to `[WORKDIR]/IEdgeInsights/VideoIngestion/test_videos` and provide the location of the video file in the GStreamer pipeline.

- Generic plugin - Gstreamer ingestor with GVA elements

```javascript
  {
   "type": "gstreamer",
   "pipeline": "gencamsrc serial=<DEVICE_SERIAL_NUMBER> pixel-format=<PIXEL_FORMAT> ! vaapipostproc format=bgrx ! gvadetect model=models/<DETECTION_MODEL> ! videoconvert !  video/x-raw,format=BGR ! appsink"
  }  
```

- RTSP camera - Gstreamer ingestor with GVA elements

```javascript
  {
   "type": "gstreamer",
   "pipeline": "rtspsrc location=\"rtsp://<USERNAME>:<PASSWORD>@<RTSP_CAMERA_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect model=models/<DETECTION_MODEL> ! videoconvert ! video/x-raw,format=BGR ! appsink"
  }
```

- USB camera - Gstreamer ingestor with GVA elements

```javascript
  {
   "type": "gstreamer",
   "pipeline": "v4l2src ! decodebin ! videoconvert ! video/x-raw,format=BGR ! gvadetect model=models/<DETECTION_MODEL> ! appsink"
  }
```

- RTSP simulated - Gstreamer ingestor with GVA elements

```javascript
  {
   "type": "gstreamer",
   "pipeline": "rtspsrc location=\"rtsp://<SOURCE_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect model=models/<DETECTION_MODEL> ! videoconvert ! video/x-raw,format=BGR ! appsink"
  }
```

- For generic full frame inference, use the `gvainference` element. For more information, refer [gvainference](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvainference).

  The following is an example pipeline to run the PCB classification using the `gvainference` element. When you run the following pipeline, you may not see the PCB defects and bounding boxes because there is no provision to provide a reference image and config ROI (which the PCB classifier expects) with the gvaelements.

```javascript
  {
   "type": "gstreamer",
   "pipeline": "rtspsrc location=\"rtsp://<SOURCE_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! gvainference device=CPU model=common/video/udfs/python/pcb/ref/model_2.xml !    vaapipostproc format=bgrx height=600 width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink",
  }
```

> Note:
>
> - The GVA elements can only be used with the `gstreamer` ingestor
> - Using the device property of the `gvadetect` and `gvaclassify` elements you can set CPU, GPU, or HDDL device to use with the GVA elements.
> - By default, the device property is set to CPU.
> - Start the HDDL daemon on the host machine by following the steps mentioned in the [Using video accelerators] section in the [../../README.md](https://github.com/open-edge-insights/eii-core/blob/master/README.md#using-video-accelerators-in-ingestionanalytics-containers).
> The following is an example pipeline to run the Safety Gear Detection Sample using the GVA plugins on an HDDL device:
>
> ```javascript
>  {
>   "type": "gstreamer",
>   "pipeline": "rtspsrc location=\"rtsp://<SOURCE_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect device=HDDL  model=models/frozen_inference_graph.xml ! videoconvert ! video/x-raw,format=BGR ! appsink"
>  }
> ```
