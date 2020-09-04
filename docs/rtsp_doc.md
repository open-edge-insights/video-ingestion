### RTSP Camera

**NOTE**:

* In case you want to enable resizing with RTSP camera use the
  `vaapipostproc` element and specifiy the `height` and `width`
  parameter in the          gstreamer pipeline.

    **Example pipeline to enable resizing with RTSP camera:**
    ```javascript
    `"pipeline": "rtspsrc location=\"rtsp://admin:intel123@<RTSP CAMERA IP>:554/\" latency=100  ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx height=600 width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink"`
    ```

* If working behind a proxy, RTSP camera IP need to be updated to RTSP_CAMERA_IP in [../../build/.env](../../build/.env)

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
        `"pipeline": "rtsp://admin:intel123@<RTSP CAMERA_IP>:554"`
        ```

    * If a simulated RTSP stream needs to be used:

      * Run the following command to create a RTSP stream:

          ```sh
          $ docker run --rm -e RTSP_RESOLUTION='1920'x'1080' -e RTSP_FRAMERATE=25 -p 8554:8554 ullaakut/rtspatt
          ```

          If more options are required to generate a RTSP stream refer
          the following link:
          https://hub.docker.com/r/ullaakut/rtspatt/


      * Use the following config to read from the RTSP stream generated
        from the above command"
          ```javascript
          "pipeline": "rtsp://localhost:8554/live.sdp"
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

        `cvlc -vvv file://<absolute_path_to_video_file> --sout '#gather:rtp{sdp=rtsp://localhost:8554/}' --loop --sout-keep`

  * RTSP cvlc based camera simulation

    * In case you want to enable resizing with RTSP cvlc based camera use the
      `vaapipostproc` element and specifiy the `height` and `width` parameter
        in the gstreamer pipeline.

        **Example pipeline to enable resizing with RTSP camera:**

        `"pipeline": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx height=600 width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

