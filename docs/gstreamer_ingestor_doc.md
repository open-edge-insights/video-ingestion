# Contents

- [Contents](#contents)
  - [GStreamer ingestor](#gstreamer-ingestor)
    - [Using the GStreamer ingestor](#using-the-gstreamer-ingestor)

## GStreamer ingestor

> Note
>
> The Gstreamer elements used are being leveraged from the OpenVINO DL Streamer Framework. For debugging, use the `gst-inspect-1.0` and `gst-launch-1.0` tool with the GStreamer elements. To use the tool with the VideoIngestion container, refer the following steps:

  ```sh

  # 1. Use `docker exec` to run a command in a running container

  $ docker exec -it ia_video_ingestion bash

  # 2. Source the OpenVINO setupvars.sh script inside the VideoIngestion container

  $ source /opt/intel/openvino/bin/setupvars.sh

  **Note**: For VAAPI elements, few additional env variables should be exported. Refer the VideoIngestion/vi_start.sh for exporting the additional variables.

  # 3. Run the GStreamer command using the tool.

  # For example, to print information about a GStreamer element like the `gvadetect`, use the gst-inspect.1.0 tool

  $ gst-inspect-1.0 gvadetect

  # To use or view information about the Generic Plugin, update the GST_PLUGIN_PATH to include the following path, and then, use the `gst-inspect-1.0` tool

  $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:"/usr/local/lib/gstreamer-1.0"

  $ gst-inspect-1.0 gencamsrc

  ```
>  
> For more information on the GStreamer tool, refer the following:
>
><https://gstreamer.freedesktop.org/documentation/tutorials/basic/gstreamer-tools.html?gi-language=c>

### Using the GStreamer ingestor

For using the GStreamer ingestor, consider the following key points:

- It is recommended to use `opencv` ingestor, if the VideoIngestion is running on the non-gfx systems or older systems such as Xeon machines that doesn't have hardware media decoders.
- The GStreamer ingestor expects the image format to be in the `BGR` format. The output image format should also be in the `BGR` format.
- The `poll_interval` key is not applicable for the GStreamer ingestor. Refer the usage of the `videorate` element in the following example to control the framerate in case of the GStreamer. ingestor.
- To reduce the ingestion rate, with the GStreamer ingestor use the `videorate` element to control the frame rate in the GStreamer pipeline.

  The following is an example pipeline to use the `videorate` element:

  ```javascript
  {
  "type": "gstreamer",
  "pipeline": "multifilesrc loop=TRUE stop-index=0 location=./test_videos/pcb_d2000.avi ! h264parse ! decodebin ! videoconvert ! video/x-raw,format=BGR ! videorate ! video/x-raw,framerate=5/1 ! appsink"
  }
  ```

  > Note
  >
  > In the example, after completing the stream decoding and the color space conversion, the incoming stream frame rate is adjusted from 20 frames per second (fps) to 5 fps. If the frame rate is adjusted to lower than the input stream then the frame rate is corrected by dropping the frames. If the frame rate is adjusted to a higher fps compared to the input stream then frames are duplicated.
  > For more information on `videorate` element, refer the following link:
  > <https://gstreamer.freedesktop.org/documentation/videorate/index.html?gi-language=c>

- In case of extended run with the Gstreamer ingestor, you can consider the properties of `appsink` element, such as `max-buffers` and `drop` to overcome issues like ingestion of frames getting blocked. The `appsink` element internally uses a queue to collect buffers from the streaming thread. The `max-buffers` property can be used to limit the queue size. The `drop` property is used to specify whether to block the streaming thread or to drop the old buffers when the maximum size of queue is reached.

  The following is an example pipeline to use the `max-buffers` and `drop` properties of `appsink` element:

  ```javascript
    {
      "type": "gstreamer",
      "pipeline": "rtspsrc location=\"rtsp://<USERNAME>:<PASSWORD>@<RTSP_CAMERA_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! videoconvert ! video/x-raw,format=BGR ! appsink max-buffers=10 drop=TRUE"
    }
  ```

  > Note
  >
  > Using the `max-buffers` and `drop` properties is helpful in scenarios, when the camera should not be disconnected, in case of slow downstream processing of buffers.

- For the GVA use case, if the VideoIngestion does not publish any frames then the `queue` element of Gstreamer can be used to limit the max size of the buffers. The upstreaming or downstreaming can be set to leak to drop the buffers.

  The following is an example pipeline to use the `queue` element:

  ```javascript
  {
    "type": "gstreamer",
    "pipeline": "rtspsrc location=\"rtsp://<USERNAME>:<PASSWORD>@<RTSP_CAMERA_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! queue max-size-buffers=10 leaky=downstream ! gvadetect model=<DETECTION_MODEL> ! videoconvert ! video/x-raw,format=BGR ! appsink",
  }
  ```

  For more information about the queue element, refer the following link:
  <https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gstreamer-plugins/html/gstreamer-plugins-queue.html>

- To enable the debug information for the Gstreamer elements use the `GST_DEBUG` env variable in [../docker-compose.yml](../docker-compose.yml).

  The following is an example snippet to set the `GST_DEBUG` env variable in [../docker-compose.yml](../docker-compose.yml):

   ```yml
   services:
     ia_video_ingestion:
     ----snip----
       environment:
           GST_DEBUG: "1,gencamsrc:4"
     ----snip----
    ```

   `GST_DEBUG: "1,gencamsrc:4"` env variable  will set the GST log level of `gencamsrc` element to 4 and all other elements to 1.

   For more information on the Gstreamer debug log levels, refer the following link:
   <https://gstreamer.freedesktop.org/documentation/tutorials/basic/debugging-tools.html?gi-language=c>
