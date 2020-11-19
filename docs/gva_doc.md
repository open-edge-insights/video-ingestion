### GVA (GStreamer Video Analytics)

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
       "pipeline": "gencamsrc serial=<DEVICE_SERIAL_NUMBER> pixel-format=ycbcr422_8 width=1920 height=1080 exposure-time=3250 ! vaapipostproc format=bgrx ! gvadetect model=models/<DETECTION_MODEL> ! videoconvert !  video/x-raw,format=BGR ! appsink"
     }
    ```

 * `RTSP camera - Gstreamer ingestor with GVA elements`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://admin:intel123@<RTSP CAMERA IP>:554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect model=models/<DETECTION_MODEL> ! videoconvert ! video/x-raw,format=BGR ! appsink"
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
        "pipeline": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect model=models/<DETECTION_MODEL> ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```


**Note**:

* GVA elements can only be used with `gstreamer` ingestor
* In case one needs to use CPU/GPU/HDDL device with GVA elements it
  can be set using the device property of gvadetect and gvaclassify elements.
  By default the device property is set to CPU.

* HDDL daemon needs to be started on the host m/c by following the steps in #Using video accelerators section in [../../README.md](../../README.md).

    **Example pipeline to run the Safety Gear Detection Sample using GVA plugins on HDDL device**:

    ```javascript
    {
      "type": "gstreamer",
      "pipeline": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect device=HDDL  model=models/frozen_inference_graph.xml ! videoconvert ! video/x-raw,format=BGR ! appsink"
    }
    ```

* The gvaclassify element is typically inserted into pipeline after gvadetect and
  executes inference on all objects detected by gvadetect with input on crop area
  specified by GstVideoRegionOfInterestMeta. In case gvaclassify needs to be used
  without gvadetect then one can use the `gvametaconvert` element (before gvaclassify
  in the gstreamer pipleine) with converter `add-fullframe-roi` which will add a region
  of interest covering the full frame.

  **Example pipeline to use `gvametaconvert` element with gvaclassify**:
  ```javascript
  {
  "type": "gstreamer",
          "pipeline": "multifilesrc loop=TRUE stop-index=0 location=./test_videos/Safety_Full_Hat_and_Vest.avi ! h264parse ! decodebin ! videoconvert ! video/x-raw,format=BGR ! gvametaconvert converter=add-fullframe-roi ! gvaclassify model=models/age-gender-recognition-retail-0013/FP32/age-gender-recognition-retail-0013.xml model-proc=models/model_proc/age-gender-recognition-retail-0013.json ! gvawatermark ! appsink"
  }
  ```
  **Please note that the above pipeline is an example for the usage of `gvametaconvert` only and the models used are not provided as part of the repo.**

