### RTSP Camera

**NOTE**:

* For more information on the RTSP URI please refer the website/tool of the camera software which is used to configure the RTSP camera. For information on RTSP protocol refer https://en.wikipedia.org/wiki/Real_Time_Streaming_Protocol

* In case you want to enable resizing with RTSP camera use the
  `vaapipostproc` element and specifiy the `height` and `width`
  parameter in the          gstreamer pipeline.

    **Example pipeline to enable resizing with RTSP camera:**
    ```javascript
    `"pipeline": "rtspsrc location=\"rtsp://<USERNAME>:<PASSWORD>@<RTSP_CAMERA_IP>:<PORT>/<FEED>\" latency=100  ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx height=600 width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink"`
    ```

* If working behind a proxy, RTSP_CAMERA_IP/simulated SOURCE_IP need to be updated to RTSP_CAMERA_IP in [../../build/.env](../../build/.env) and [../../build/builder.py](../../build/builder.py) needs to be executed.

* For working both with simulated RTSP server via cvlc or
  direct streaming from RTSP camera, we can use the below Gstreamer
  MediaSDK parsers and decoders based on the input stream type
    **Eg**: parsers and decoders:
    * h264parse !  vaapih264dec
    * h265parse ! vaapih265dec

    ---
    >**NOTE**: If running on non-gfx systems or older systems where we don't
    have hardware media decoders, the above parsers and decoders may not
    work. In those cases, one can use `opencv` ingestor and refer the below steps.

    In case RTSP stream ingestion needs to be used on Xeon machine with no
    GPU then refer the following ingestor config,

    * If a physical RTSP camera is used use the below config:
        ```javascript
        "pipeline": "rtsp://<USERNAME>:<PASSWORD>@<RTSP_CAMERA_IP>:<PORT>/<FEED>"
        ```

    * If a simulated RTSP stream needs to be used:

      * Run the following command to create a RTSP stream:

          ```sh
          $ docker run --rm -e RTSP_RESOLUTION='1920'x'1080' -e RTSP_FRAMERATE=25 -p 8554:8554 ullaakut/rtspatt:latest
          ```

          If more options are required to generate a RTSP stream refer
          the following link:
          https://hub.docker.com/r/ullaakut/rtspatt/


      * Use the following config to read from the RTSP stream generated
        from the above command"
          ```javascript
          "pipeline": "rtsp://<SOURCE_IP>:8554/live.sdp"
          ```

    >**NOTE** : Some issues are observed with cvlc based camera simulation
    on a Xeon Machine with no GPU. In that case refer the above
    commands to generate a RTSP stream.
----

### RTSP Simulated Camera

**NOTE**:

  * It has been observed that VI is unable to read frames from cvlc rtsp server with
    opencv ingestor over long runs.

  * If VI is unable to read frames from cvlc rtsp server with gstreamer
    ingestor, then we need to force `appsink` to drop the queued buffers. This
    can be achieved by using `max-buffers` and `drop` properties for `appsink`.

  * Start cvlc based RTSP stream

    * Install VLC if not installed already: `sudo apt install vlc`
    * In order to use the RTSP stream from cvlc, the RTSP server
        must be started using VLC with the following command:

        `cvlc -vvv file://<absolute_path_to_video_file> --sout '#gather:rtp{sdp=rtsp://<SOURCE_IP>:<PORT>/<FEED>}' --loop --sout-keep`

        **Note:** `<FEED>` in the cvlc command can be `live.sdp` or it can also be avoided. But make sure the same RTSP URI given here is
        used in the ingestor pipeline config.

  * RTSP cvlc based camera simulation

    * In case you want to enable resizing with RTSP cvlc based camera use the
      `vaapipostproc` element and specifiy the `height` and `width` parameter
        in the gstreamer pipeline.

        **Example pipeline to enable resizing with RTSP camera:**

        `"pipeline": "rtspsrc location=\"rtsp://<SOURCE_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx height=600 width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

