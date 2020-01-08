# `VideoIngestion Module`

The VideoIngestion(VI) module is mainly responsibly for ingesting the video frames
coming from a video source like video file or basler/RTSP/USB camera
into the EIS stack for further processing. Additionally, by having VI run with
classifier and post-processing UDFs, VI can perform the job of VA(VideoAnalytics)
service also.

The high level logical flow of VideoIngestion pipeline is as below:

1. App reads the application configuration via EIS Configuration Manager which
   has details of `ingestor`, `encoding` and `udfs`.
2. Based on the ingestor configuration, app reads the video frames from
   the video file or camera.
3. [`Optional`] The read frames are passed onto one or more chained native/python
   UDFs for doing any pre-processing (passing thru UDFs is an optional thing and
   not required if one doesn't want to perform any pre-processing on the
   ingested frames). With chaining of UDFs supported, one can also have
   classifier UDFs and any post-processing UDFs like resize etc., configured in
   `udfs` key to get the classified results. One can refer
   [UDFs README](../common/udfs/README.md) for more details.
4. App gets the msgbus endpoint configuration from system environment and
   based on the configuration, app publishes the data on the mentioned topic
   on EIS MessageBus.

---
**NOTE**:
Below usecases are suitable for single node deployment where one can avoid the
overhead of VA(VideoAnalytics) service.

1. If VI(VideoIngestion) service is configured with an UDF that does the
   classification, then one may choose to not to have VA service as all
   pre-processing, classification and any post-processing can be handled in VI
   itself with the usage of multiple UDFs.
2. If VI(VideoIngestion) service is using GVA(Gstreamer Video Analytics)
   elements also, then one may choose to not to have VA service as all
   pre-processing, classification and any post-processing(using vappi gstreamer
   elements) can be done in gstreamer pipeline itself. Also, the post-processing
   here can be configured by having multiple UDFs in VI if needed.

---

## `Configuration`

---
**NOTE**:

* The `max_jobs`, `max_workers` and `udfs` are configuration keys related to udfs.
  For more details on udf configuration, please visit
  [../common/udfs/README.md](../common/udfs/README.md)
* For details on Etcd and MessageBus endpoint configuration, visit
  [Etcd_Secrets_and_MsgBus_Endpoint_Configuration](../Etcd_Secrets_and_MsgBus_Endpoint_Configuration.md).

---

All the app module configuration are added into distributed key-value store
under `AppName` env, as mentioned in the environment section of this app's service
definition in docker-compose.

If `AppName` is `VideoIngestion`, then the app's config would be fetched from
`/VideoIngestion/config` key via EIS Configuration Manager.
Below is the JSON schema for app's config:

```javascript
{
  "type": "object",
  "additionalProperties": false,
  "required": [
    "ingestor"
  ],
  "properties": {
    "encoding": {
      "description": "Encoding object",
      "type": "object",
      "required": [
        "type",
        "level"
      ],
      "properties": {
        "type": {
          "description": "Encoding type",
          "type": "string",
          "enum": [
              "jpeg",
              "png"
            ]
        },
        "level": {
          "description": "Encoding value",
          "type": "integer",
          "default": 0
        }
      }
    },
    "ingestor": {
      "description": "Ingestor object",
      "type": "object",
      "required": [
        "type",
        "pipeline"
      ],
      "properties": {
        "type": {
          "description": "Ingestor type",
          "type": "string",
          "enum": [
              "opencv",
              "gstreamer"
            ]
        },
        "pipeline": {
          "description": "gstreamer pipeline",
          "type": "string"
        },
        "loop_video": {
          "description": "whether to loop vidoe or not",
          "type": "boolean",
          "default": false
        },
        "queue_size": {
          "description": "ingestor queue size for frames",
          "type": "integer"
        },
        "poll_interval": {
          "description": "polling interval for reading ingested frames",
          "type": "number",
          "default": 0.0
        }
      }
    },
    "max_jobs": {
      "description": "Number of queued UDF jobs",
      "type": "integer",
      "default": 20
    },
    "max_workers": {
      "description": "Number of threads acting on queued jobs",
      "type": "integer",
      "default": 4
    },
    "udfs": {
      "description": "Array of UDF config objects",
      "type": "array",
      "items": [
        {
          "description": "UDF config object",
          "type": "object",
          "properties": {
            "type": {
              "description": "UDF type",
              "type": "string",
              "enum": [
                "native",
                "python"
              ]
            },
            "name": {
              "description": "Unique UDF name",
              "type": "string"
            },
            "device": {
              "description": "Device on which inference occurs",
              "type": "string",
              "enum": [
                "CPU",
                "GPU",
                "HDDL",
                "MYRIAD"
              ]
            }
          },
          "additionalProperties": true,
          "required": [
            "type",
            "name"
          ]
        }
      ]
    }
  }
}
```

One can use [JSON validator tool](https://www.jsonschemavalidator.net/) for
validating the app configuration against the above schema.

### `Ingestor config`

The following are the type of ingestors supported:

1. OpenCV
2. GStreamer

We are also supporting usage of GVA (Gstreamer Video Analytics) plugins with
our Gstreamer ingestor. [GVA](https://github.com/opencv/gst-video-analytics) is
a collection of GStreamer elements to enable CNN model based video analytics
capabilities (such as object detection, classification, recognition) in
GStreamer framework.

 ---
  **Note**:

  * If running on non-gfx systems or older systems which doesn't have hardware
    media decoders (like in Xeon m/c) it is recommended to use `opencv` ingestor
  * GVA elements can only be used with `gstreamer` ingestor
  * In case one needs to use CPU/GPU/HDDL device with GVA elements it
    can be set using the device property of gvadetect and gvaclassify elements.
    By default the device property is set to CPU. 
    
    >**NOTE**: 
    > HDDL daemon needs to be started on the host m/c by following the steps in #Using video accelerators section in [../README.md](../README.md).

    **Example pipeline to run the Safety Gear Detection Sample using GVA plugins on HDDL device**:

    ```javascript
    {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect device=HDDL  model=models/frozen_inference_graph.xml ! videoconvert ! video/x-raw,format=BGR ! appsink"
    }
    ```
  > **NOTE**:
  > * Gstreamer Ingestor excepts the imageformat to be in `BGR` format so the output image format should be in `BGR`.

  ---

#### `Camera Configuration`

* `Video file`

  * `OpenCV Ingestor`

      ```javascript
      {
        "type": "opencv",
        "pipeline": "./test_videos/pcb_d2000.avi",
        "poll_interval": 0.2
        "loop_video": "true"
      }
      ```

  * `Gstreamer Ingestor`

      ```javascript
      {
          "type": "gstreamer",
          "pipeline": "multifilesrc loop=TRUE location=./test_videos/pcb_d2000.avi ! h264parse ! decodebin ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

  * `GVA - Gstreamer ingestor with GVA elements`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "multifilesrc loop=TRUE location=./test_videos/Safety_Full_Hat_and_Vest.mp4 ! decodebin ! videoconvert ! video/x-raw,format=BGR ! gvadetect model=models/frozen_inference_graph.xml ! appsink"
      }

    ---

    **NOTE**:
    * Use video `./test_videos/Safety_Full_Hat_and_Vest.mp4` in the pipeline
      for safety gear demo
    * Looping of videos is not happening when `./test_videos/Safety_Full_Hat_and_Vest.mp4`
      video is used in the gstreamer pipeline with multifilesrc plugin.

    ---

* `Basler Camera`

  * `OpenCV Ingestor`

      ```javascript
      {
        "type": "opencv",
        "pipeline": "pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=6000 ! videoconvert ! appsink"
      }
      ```

  * `Gstreamer Ingestor`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=1500 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```
  
  * `GVA - Gstreamer ingestor with GVA elements`

    ```javascript
    {
      "type": "gstreamer",
      "pipeline": "pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=1500 ! video/x-raw,format=YUY2 ! vaapipostproc format=bgrx ! gvadetect model=models/frozen_inference_graph.xml ! videoconvert !  video/x-raw,format=BGR ! appsink"
    }
    ```    

    ---

    **NOTE**:
    * In case multiple Basler cameras are connected use serial parameter to
      specify the camera to be used in the gstreamer pipeline in the video
      config file for camera mode. If multiple cameras are connected and the
      serial parameter is not specified then the source plugin by default
      connects to camera with device_index=0.

      Eg: pipeline value to connect to basler camera with
      serial number 22573664:

      `"pipeline":"pylonsrc serial=22573664 imageformat=yuv422 exposureGigE=3250 interpacketdelay=1500 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

    * In case you want to enable resizing with basler camera use the
      vaapipostproc element and specify the height and width parameter in the
      gstreamer pipeline.

      Eg: Example pipeline to enable resizing with basler camera

      `"pipeline":"pylonsrc serial=22573664 imageformat=yuv422 exposureGigE=3250 interpacketdelay=1500 ! video/x-raw,format=YUY2 ! vaapipostproc height=600 width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

    * In case frame read is failing when multiple basler cameras are used, use
      the interpacketdelay property to increase the delay between the
      transmission of each packet for the selected stream channel.
      Depending on the number of cameras, use an appropriate delay can be set.

      Eg: pipeline value to increase the interpacket delay to 3000(default
      value for interpacket delay is 1500):

      `"pipeline":"pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=3000 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

    * To work with monochrome Basler camera, please use `OpenCV` Ingestor and change the
      image format to mono8 in the Pipeline.

      Eg:pipeline value to connect to monochrome basler camera with serial number 22773747 :

      `"pipeline":"pylonsrc serial=22773747 imageformat=mono8 exposureGigE=3250 interpacketdelay=1500 ! videoconvert ! appsink"`


    * To work with USB Basler camera, please change the
      exposure parameter to exposureUsb in the Pipeline.

      `"pipeline":"pylonsrc serial=22573650 imageformat=yuv422 exposureUsb=3250 interpacketdelay=1500 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

    ---

* `RTSP Camera`

  * `OpenCV Ingestor`

      ```javascript
      {
        "type": "opencv",
        "pipeline": "rtsp://admin:intel123@<RTSP CAMERA IP>:554"
      }
      ```

    > **NOTE**: Opencv for rtsp will use software decoders

  * `Gstreamer Ingestor`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://admin:intel123@<RTSP CAMERA IP>:554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

  * `GVA - Gstreamer ingestor with GVA elements`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://admin:intel123@<RTSP CAMERA IP>:554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect model=models/frozen_inference_graph.xml ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

  ---

    **NOTE**:

    * In case you want to enable resizing with RTSP camera use the
     `vaapipostproc` element and specifiy the `height` and `width`
      parameter in the          gstreamer pipeline.

        **Eg**: Example pipeline to enable resizing with RTSP camera

        `"pipeline": "rtspsrc location=\"rtsp://admin:intel123@<RTSP CAMERA IP>:554/\" latency=100  ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx height=600 width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

    * If working behind a proxy, RTSP camera IP need to be updated
  *   to RTSP_CAMERA_IP in [../docker_setup/.env](../docker_setup/.env)

    * For working both with simulated RTSP server via cvlc or
      direct streaming from RTSP camera, we can use the below Gstreamer
      MediaSDK parsers and decoders based on the input stream type
        **Eg**: parsers and decoders:
        * h264parse !  vaapih264dec
        * h265parse ! vaapih265dec

        ---
        **NOTE**: If running on non-gfx systems or older systems where we don't
        have hardware media decoders, the above parsers and decoders may not
        work. In those cases, one can use `opencv` ingestor and refer the below steps.

        In case RTSP stream ingestion needs to be used on Xeon machine with no
        GPU then refer the following ingestor config,

        * If a physical RTSP camera is used:

            `"pipeline": "rtsp://admin:intel123@<RTSP CAMERA_IP>:554"`

        * If a simulated RTSP stream needs to be used:

          * Run the following command to create a RTSP stream:

              ```sh
              docker run --rm -e RTSP_RESOLUTION='1920'x'1080' -e RTSP_FRAMERATE=25 -p 8554:8554 ullaakut/rtspatt
              ```

              If more options are required to generate a RTSP stream refer
              the following link:

              ```
              https://hub.docker.com/r/ullaakut/rtspatt/
              ```

          * Use the following config to read from the RTSP stream generated
            from the above command"
              ```
              "pipeline": "rtsp://localhost:8554/live.sdp"
              ```

        >**NOTE** : Some issues are observed with cvlc based camera simulation
        on a Xeon Machine with no GPU. In that case refer the above
        commands to generate a RTSP stream.
        ---
    ---

* `USB Camera`

  * `OpenCV Ingestor`

      ```javascript
      {
        "type": "opencv",
        "pipeline": "/dev/video0"
      }
      ```

  * `Gstreamer Ingestor`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "v4l2src ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

  * `GVA - Gstreamer ingestor with GVA elements`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "v4l2src ! decodebin ! videoconvert ! video/x-raw,format=BGR ! gvadetect model=models/frozen_inference_graph.xml ! appsink"
      }
      ```

  ---

    **NOTE**:
    * In case you want to enable resizing with USB camera use the
      `videoscale` element and specify the `height` and `width`  parameter in the gstreamer pipeline.

        **Eg**: Example pipeline to enable resizing with USB camera

        `"pipeline":"v4l2src ! videoscale ! video/x-raw,format=YUY2,height=600,width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

    * In case, multiple USB cameras are connected specify the
      camera using the `device` property in the configuration file.

        **Eg:**

        `"pipeline": "v4l2src device=/dev/video0 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

    ---

* `RTSP simulated camera using cvlc`

  * `OpenCV Ingestor`

      ```javascript
      {
        "type": "opencv",
        "pipeline": "rtsp://localhost:8554/"
      }
      ```

    > **NOTE**:
    > * It has been observed that VI is unable to read frames from cvlc rtsp server with
    >   opencv ingestor over long runs.

  * `Gstreamer Ingestor`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

  * `GVA - Gstreamer ingestor with GVA elements`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect model=models/frozen_inference_graph.xml ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

  ---

    **NOTE**:
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

          **Eg**: Example pipeline to enable resizing with RTSP camera

          `"pipeline": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx height=600 width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

    ---

## `Installation`

* Follow [provision/README.md](../README#provision-eis.md) for EIS provisioning
  if not done already as part of EIS stack setup

* Run VideoIngestion

  Present working directory to try out below commands is: `[repo]/VideoIngestion`

    1. Build and Run VideoIngestion as container
        ```
        $ cd [repo]/docker_setup
        $ docker-compose up --build ia_video_ingestion
        ```
    2. Update EIS VideoIngestion config key in distributed key-value
       store using UI's like `EtcdKeeper` or programmatically, the container
       restarts to pick the new changes.
