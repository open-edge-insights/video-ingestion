### GStreamer Ingestor

**NOTE**:

* `gstreamer` ingestor expects the image format to be in `BGR` format so the output image format should be in `BGR`

* `poll_interval` key is not applicable for `gstreamer` ingestor. Refer the usage of `videorate` element in the below section to control the framerate in case of `gstreamer` ingestor.
* In case one wants to reduce the ingestion rate with `gstreamer` ingestor the `videorate` element can be used to control the framerate in the gstreamer pipeline.

  **Example pipeline to use the `videorate` element**:
  ```javascript
  {
  "type": "gstreamer",
  "pipeline": "multifilesrc loop=TRUE stop-index=0 location=./test_videos/pcb_d2000.avi ! h264parse ! decodebin ! videoconvert ! video/x-raw,format=BGR ! videorate ! video/x-raw,framerate=5/1 ! appsink"
  }
  ```

  >**Note**: In the above example after the stream is decoded and the color space conversion is done the framerate is adjusted to 5 FPS (The incoming stream framerate is 20 which is adjusted to 5 FPS). The correction in framerate is performed by dropping the frames in case the framerate is adjusted to a lower ethan the input stream. In case the framerate is adjusted to a higher FPS compared to the input stream then frames will be duplicated.

  For more information on `videorate` element refer the below link:
  https://gstreamer.freedesktop.org/documentation/videorate/index.html?gi-language=c


* In case of extended run with gstreamer ingestor one can consider the properties of `appsink` element such as `max-buffers` and `drop` to overcome issues like ingestion of frames getting blocked. The `appsink` element internally uses a queue to collect buffers from the streaming thread. The `max-buffers` property can be used to limit the queue size. The `drop` property is used to specify whether to block the streaming thread or to drop the old buffers when maximum size of queue is reached.

  **Example pipline to use `max-buffers` and `drop` properties of `appsink` element**:
  ```javascript
    {
      "type": "gstreamer",
      "pipeline": "rtspsrc location=\"rtsp://admin:intel123@<RTSP CAMERA IP>:554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! videoconvert ! video/x-raw,format=BGR ! appsink max-buffers=10 drop=TRUE"
    }
  ```

  >**Note**:  The usage of `max-buffers` and `drop` properties are helpful when the camera should not be disconnected in case of slow downstream processing of buffers.


* In case one notices the VideoIngestion not publishing any frames when working with GVA use case the `queue` element of Gstramer can be used to limit the     max size of the buffers and the upstreaming/downstreaming can be set to leak to drop the buffers

  **Example pipeline to use the `queue` element**:

  ```javascript
  {
    "type": "gstreamer",
    "pipeline": "rtspsrc location=\"rtsp://admin:intel123@<RTSP_CAMERA_IP>:554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! queue max-size-buffers=10 leaky=downstream ! gvadetect model=models/frozen_inference_graph.xml ! videoconvert ! video/x-raw,format=BGR ! appsink",
  }
  ```

  For more information reagarding the queue element refer the below link:
  https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gstreamer-plugins/html/gstreamer-plugins-queue.html

