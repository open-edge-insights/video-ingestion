**Contents**

- [GVA (GStreamer Video Analytics)](#gva-gstreamer-video-analytics)

### GVA (GStreamer Video Analytics)

VI module supports the usage of GVA (Gstreamer Video Analytics) plugins with `gstreamer` ingestor. [GVA](https://github.com/openvinotoolkit/dlstreamer_gst) is
a collection of GStreamer elements to enable CNN model based video analytics capabilities (such as object detection, classification, recognition) in GStreamer framework.

**Refer [CustomUdfs-GVASafetyGearIngestion](../CustomUdfs/GVASafetyGearIngestion/README.md) to refer GVA based CustomUdf container added for SafetyGear sample.**

GVA use case configurations with different cameras:

* `Video File - Gstreamer ingestor with GVA elements`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "multifilesrc loop=TRUE stop-index=0 location=./test_videos/<VIDEO_FILE> ! h264parse ! decodebin ! videoconvert ! video/x-raw,format=BGR ! gvadetect model=models/<DETECTION_MODEL> ! appsink"
      }
      ```

      **Note:** In case one needs to use GVA with a video file using multifilesrc element then copy the video file to [WORKDIR]/IEdgeInsights/VideoIngestion/test_videos and provide the location of the video file accordingly in the gstreamer pipeline.


 * `Generic Plugin - Gstreamer ingestor with GVA elements`

    ```javascript
     {
       "type": "gstreamer",
       "pipeline": "gencamsrc serial=<DEVICE_SERIAL_NUMBER> pixel-format=<PIXEL_FORMAT> ! vaapipostproc format=bgrx ! gvadetect model=models/<DETECTION_MODEL> ! videoconvert !  video/x-raw,format=BGR ! appsink"
     }
    ```

 * `RTSP camera - Gstreamer ingestor with GVA elements`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://<USERNAME>:<PASSWORD>@<RTSP_CAMERA_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect model=models/<DETECTION_MODEL> ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

 * `USB camera - Gstreamer ingestor with GVA elements`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "v4l2src ! decodebin ! videoconvert ! video/x-raw,format=BGR ! gvadetect model=models/<DETECTION_MODEL> ! appsink"
      }
      ```
 * `RTSP simulated - Gstreamer ingestor with GVA elements`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://<SOURCE_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect model=models/<DETECTION_MODEL> ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```
 * For generic full frame inference one can use the `gvainference` element. For more information refer [gvainference](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvainference).

  **Example pipeline to run the PCB classification using gvainference element**:

  ```javascript
  {
   "type": "gstreamer",
   "pipeline": "rtspsrc location=\"rtsp://<SOURCE_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! gvainference device=CPU model=common/video/udfs/python/pcb/ref/model_2.xml ! vaapipostproc format=bgrx height=600 width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink",
  }
  ```

  **Note:** When the above pipeline is run, one may not see the PCB defects and bounding boxes since there is no provision to provide a reference image and config ROI(which the PCB classifier expects) with the gvaelements.

**Note**:

```javascript
  {
   "type": "gstreamer",
   "pipeline": "rtspsrc location=\"rtsp://<SOURCE_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! gvainference device=CPU model=common/video/udfs/python/pcb/ref/model_2.xml ! vaapipostproc format=bgrx height=600 width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink",
  }
```

> Note:
>
> - The GVA elements can only be used with the `gstreamer` ingestor
> - Using the device property of the `gvadetect` and `gvaclassify` elements you can set CPU, GPU, or HDDL device to use with the GVA elements.
> - By default, the device property is set to CPU.
> - Start the HDDL daemon on the host machine by following the steps mentioned in the [Using video accelerators] section in the [../../README.md](https://github.com/open-edge-insights/eii-core/blob/master/README.md#using-video-accelerators-in-ingestionanalytics-containers).
> The following is an example pipeline to run sample detection model using the GVA plugins on an HDDL device(ensure that the detection model IR files are copied to models path):
>
> ```javascript
>  {
>   "type": "gstreamer",
>   "pipeline": "rtspsrc location=\"rtsp://<SOURCE_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect device=HDDL  model=models/<DETECTION_MODEL> ! videoconvert ! video/x-raw,format=BGR ! appsink"
>  }
> ```
