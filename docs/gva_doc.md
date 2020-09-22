### GVA (GStreamer Video Analytics)

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

