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
   [../common/video/udfs/README.md](../common/video/udfs/README.md) for more details.
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
  [../common/video/udfs/README.md](../common/video/udfs/README.md)
* For details on Etcd and MessageBus endpoint configuration, visit
  [Etcd_Secrets_and_MsgBus_Endpoint_Configuration](../Etcd_Secrets_and_MsgBus_Endpoint_Configuration.md).

---

All the app module configuration are added into distributed key-value store
under `AppName` env, as mentioned in the environment section of this app's service
definition in docker-compose.

Developer mode related overrides go into docker-compose-dev.override.yml

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
          "description": "whether to loop video or not",
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
                "MYRIAD",
                "HETERO:FPGA,CPU",
                "HETERO:FPGA,GPU"
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

**Note**:

* For `jpeg` encoding type, `level` is the quality from `0 to 100` (the higher is the better)
* For `png` encoding type, `level` is the compression level from `0 to 9`. A higher value means a smaller size and longer compression time.


**Note**: `poll_interval` key is not applicable for `gstreamer` ingestor. Refer the usage of `videorate` element in the below section to control the framerate in case of `gstreamer` ingestor.

**Note**: Heterogeneous Execution needs to be used with FPGA plugin. One must still point to the CPU plugin or the GPU plugin as fallback devices for heterogeneous plugin. HETERO does not support VPU. For more information refer the below link:
https://docs.openvinotoolkit.org/latest/_docs_IE_DG_supported_plugins_FPGA.html

**Note**: GVA elements have compatibility issue with FPGA plugin. One may notice issue when `HETERO:FPGA,CPU` or `HETERO:FPGA,GPU` is used with the `device` property of GVA elements.

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

  * The Gstreamer elements used are being leveraged from OpenVINO DL Streamer Framework.
    For debugging purpose one can use the `gst-inspect-1.0` and `gst-launch-1.0` tool with
    the gstreamer elements. In order to use the tool with the VideoIngestion container refer
    the below steps.

    ```sh

    # 1. Use `docker exec` to run a command in a running container

    $ docker exec -it ia_video_ingestion bash

    # 2. Source the OpenVINO setupvars.sh script inside the VideoIngestion container

    $ source /opt/intel/openvino/bin/setupvars.sh

    # 3. Run the gstreamer command using the tool.

    # For e.g inorder to print info about a gstreamer element like `gvadetect` use the gst-inspect.1.0 tool

    $ gst-inspect-1.0 gvadetect

    # To view information about the basler source plugin or exercise it then update the GST_PLUGIN_PATH to include below path

    $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:"/usr/local/lib/gstreamer-1.0"

    ```

    For more information on the gstreamer tool refer the below link:
    https://gstreamer.freedesktop.org/documentation/tutorials/basic/gstreamer-tools.html?gi-language=c

  * If running on non-gfx systems or older systems which doesn't have hardware
    media decoders (like in Xeon m/c) it is recommended to use `opencv` ingestor
  * GVA elements can only be used with `gstreamer` ingestor
  * In case one needs to use CPU/GPU/HDDL device with GVA elements it
    can be set using the device property of gvadetect and gvaclassify elements.
    By default the device property is set to CPU.

    **NOTE**:

     * HDDL daemon needs to be started on the host m/c by following the steps in #Using video accelerators section in [../README.md](../README.md).

     **Example pipeline to run the Safety Gear Detection Sample using GVA plugins on HDDL device**:

     ```javascript
     {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! gvadetect device=HDDL  model=models/frozen_inference_graph.xml ! videoconvert ! video/x-raw,format=BGR ! appsink"
     }
     ```

  ----
  **NOTE**:

  * Gstreamer Ingestor expects the image format to be in `BGR` format so the output image format should be in `BGR`

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
    Please note that the above pipeline is an example for the usage of `gvametaconvert` only and the models used are not provided as part of the repo.

  * In case of extended run with gstreamer ingestor one can consider the properties of `appsink` element such as `max-buffers` and `drop` to overcome issues like ingestion of frames getting blocked. The `appsink` element internally uses a queue to collect buffers from the streaming thread. The `max-buffers` property can be used to limit the queue size. The `drop` property is used to specify whether to block the streaming thread or to drop the old buffers when maximum size of queue is reached.

    ** Example pipline to use `max-buffers` and `drop` properties of `appsink` element**:
    ```javascript
     {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://admin:intel123@<RTSP CAMERA IP>:554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! videoconvert ! video/x-raw,format=BGR ! appsink max-buffers=10 drop=TRUE"
      }
    ```

    **Note**:  The usage of `max-buffers` and `drop` properties are helpful when the camera should not be disconnected in case of slow downstream processing of buffers.

  * In case one wants to reduce the ingestion rate with `gstreamer` ingestor the `videorate` element can be used to control the framerate in the gstreamer pipeline.

  ** Example pipeline to use the `videorate` element **:
  ```javascript
   {
      "type": "gstreamer",
      "pipeline": "multifilesrc loop=TRUE stop-index=0 location=./test_videos/pcb_d2000.avi ! h264parse ! decodebin ! videoconvert ! video/x-raw,format=BGR ! videorate ! video/x-raw,framerate=5/1 ! appsink"
   }
  ```

  **Note**: In the above example after the stream is decoded and the color space conversion is done the framerate is adjusted to 5 FPS (The incoming stream framerate is 20 which is adjusted to 5 FPS). The correction in framerate is performed by dropping the frames in case the framerate is adjusted to a lower ethan the input stream. In case the framerate is adjusted to a higher FPS compared to the input stream then frames will be duplicated.

   For more information on `videorate` element refer the below link:
   https://gstreamer.freedesktop.org/documentation/videorate/index.html?gi-language=c

----

#### `Camera Configuration`

* `Video file`

  * `OpenCV Ingestor`

      ```javascript
      {
        "type": "opencv",
        "pipeline": "./test_videos/pcb_d2000.avi",
        "poll_interval": 0.2
        "loop_video": true
      }
      ```

  * `Gstreamer Ingestor`

      ```javascript
      {
          "type": "gstreamer",
          "pipeline": "multifilesrc loop=TRUE stop-index=0 location=./test_videos/pcb_d2000.avi ! h264parse ! decodebin ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```
    ---

      **NOTE**:
      * In case one does not want to loop the video with multifilesrc element then set the `loop` property to `FALSE`
      * In case one wants to play the video for specific number of iterations set the `loop` property to `FALSE` and `stop-index` property to the number of iteration starting with 0 which would play it once. Setting the `loop` property to `TRUE` will override the `stop-index` property.
      * The `loop` property of `multifilesrc` plugin does not support video files of MP4 format. Hence MP4 video files will not loop and the recommendation is to transcode the video file to AVI format.
      * In case one notices general stream error with multifilesrc element when certain video files are used then transcode the video file to `H264` video with `.avi` container format to ensure the compatibity of the format of the video file.

    ---

  * `GVA - Gstreamer ingestor with GVA elements`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "multifilesrc loop=TRUE stop-index=0 location=./test_videos/Safety_Full_Hat_and_Vest.avi ! h264parse ! decodebin ! videoconvert ! video/x-raw,format=BGR ! gvadetect model=models/frozen_inference_graph.xml ! appsink"
      }

    ---

    **NOTE**:
    * Use `./test_videos/Safety_Full_Hat_and_Vest.avi` video file for safety gear demo use case.

    ---

* `Basler Camera`

  * `Gstreamer Ingestor`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=1500 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```
    * Hardware trigger based ingestion with gstreamer ingestor :

    ```javascrpit
    {
     "type": "gstreamer",
     "pipeline": "pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=6000 continuous=false triggersource=Line1 hwtriggertimeout=50000 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink",
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

    * `Image Formats`

    | Camera Model | Tested Image Formats |
    |:------------:|:--------------------:|
    | Basler acA1920-40gc | mono8<br>yuv422<br>bayer8 |

    * In case one wants to use `bayer8` imageformat then `bayer2rgb` element needs to be used to covert raw bayer data to RGB. Refer the below pipeline.

      ```javascript
      {
       "type": "gstreamer",
       "pipeline": "pylonsrc imageformat=bayer8 exposureGigE=3250 interpacketdelay=1500 ! bayer2rgb ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
     ```

    * In case one wants to use `mono8` image format or wants to work with monochrome camera then change the image format to mono8 in the pipeline. Since `gstreamer` ingestor expects a `BGR` image format, a single channel GRAY8 format would be converted to 3 channel BGR format.

     Example pipeline to use `mono8` imageformat or work with monochrome basler camera

     ```javascript
     {
      "type": "gstreamer",
      "pipeline": "pylonsrc imageformat=mono8 exposureGigE=3250 interpacketdelay=1500 ! video/x-raw,format=GRAY8 ! videoconvert ! video/x-raw,format=BGR ! appsink"
     }
     ```

    * In case one wants to enable resizing with basler camera `vaapipostproc` element can be used to specify the height and width parameter in the gstreamer pipeline.

      **Example pipeline to enable resizing  the frame to `600x600` with basler camera:**

      ```javascript
      {
       "type": "gstreamer",
       "pipeline": "pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=1500 ! video/x-raw,format=YUY2 ! vaapipostproc height=600 width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

     * In case one wants to divert the colour space conversion to `GPU` for basler camera `vaapipostproc` can be used. This can be useful when the CPU% is maxing out due to the input ingestion stream.

      **Example pipeline to perform color space conversion from `YUY2 to BGRx` in GPU with `vaapipostproc` with basler camera:**

      ```javascript
      {
       "type": "gstreamer",
       "pipeline": "pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=1500 ! video/x-raw,format=YUY2 ! vaapipostproc format=bgrx ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }


    * In case multiple Basler cameras are connected use serial parameter to specify the camera to be used in the gstreamer pipeline in the video config file for camera mode. If multiple cameras are connected and the serial parameter is not specified then the source plugin by default
connects to camera with device_index=0.

      **Example pipeline to connect to basler camera with serial number 22573664:**

      ```javascript
      {
       "type": "gstreamer",
       "pipeline": "pylonsrc serial=22573664 imageformat=yuv422 exposureGigE=3250 interpacketdelay=1500 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

    * In case frame read is failing when multiple basler cameras are used, use the interpacketdelay property to increase the delay between the
transmission of each packet for the selected stream channel. Depending on the number of cameras, use an appropriate delay can be set.

      **Example pipeline value to increase the interpacket delay to 3000(default value for interpacket delay is 1500 which can be overridden by setting the required delay in the gstreamer pipeline):**

      ```javascript
      {
       "type": "gstreamer",
       "pipeline": "pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=3000 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

    * To work with USB Basler camera, please change the exposure parameter to exposureUsb in the Pipeline.

      ```javascript
      {
       "type": "gstreamer",
       "pipeline":"pylonsrc imageformat=yuv422 exposureUsb=3250 interpacketdelay=1500 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }

    ##### Basler camera hardware triggering

    * If the camera is configured for triggered image acquisition, one can trigger image captures at particular points in time.

    * With respect to hardware triggering if the camera supports it then an electrical signal can be applied to one of the camera's input lines which can act as a trigger signal.

    * In order to configure the camera for hardware triggering, trigger mode must be enabled and the right trigger source depending on the Hardware Setup must be specified.

    * Trigger mode is enabled by setting the `continuous` property to `false` and based on the h/w setup, the right trigger source needs to be set for `triggersource` property

    ##### Validated test setup for basler camera hardware triggering

    * In case of trigger mode the maximum time to wait for the hardware trigger to get generated can be set (in milliseconds) using the ` hwtriggertimeout` property.

    * In our test setup a python script was used to control a ModBus I/O module to generate a digital output to Opto-insulated input line(Line1) of the basler camera.

    * For testing the hardware trigger functionality Basler `acA1920-40gc` camera model had been used.

    **Note**: Other triggering capabilities with different camera models are not tested.

  ##### Camera independent Software Trigger way of video ingestion

  * Software triggering way of video ingestion is a solution which enables to control video ingestion from all types of cameras, any configuration (Gstreamer & Opencv) / video files.

  * The regular way of video ingestion is autonomous i.e. as soon as the Video Ingestion micro-service is started the ingestion starts automatically using the video source (file/camera). There is no way to control the ingestion without stopping the ingestion micro-service itself. Software trigger
  feature provides a mechanism to start & stop video ingestion using software triggers sent by the client application.

  VideoIngestion micro-service exposes the functionality of software trigger in the following ways:

  1) It can accept software trigger to "START_INGESTION"/ "STOP_INGESTION"/ "SNAPSHOT" from any client utility which uses the EIS messagebus over server-client model.

  2) The software trigger functionality of VI is demonstrated using an sample baremetal utility called "SW_Trigger_utility", which is shipped with the VideoIngestion code in tools repo, the details of the usage of this utility is mentioned in [../tools/SWTriggerUtility/README.md](../tools/SWTriggerUtility/README.md).

  ##### Generic Server in VideoIngestion

  * VI generic server responds back to the client with the return values specific to the command. There is a total flexibility in sending the number & type of arguments back to client which is totally dependent on the command.


  * Example JSON format for incoming payload from client to server to initialize software trigger:
  ```javascript
  {
    "init_state" : "running"
  }
  ```

  **Note**: When the `init_state` value is `running` then ingestor is started without any sw trigger from the client. In order to control the ingestor using the sw trigger utility change the value to `stopped`. To refer the available option to generate the sw trigger refer [../tools/SWTriggerUtility/README.md](../tools/SWTriggerUtility/README.md)

  * The supported commands in the VI Gerenic Server are:

    1. START_INGESTION: to start the ingestor

       Payload format:
       {
         "command" : "START_INGESTION"
       }

    2. STOP_INGESTION: to stop the ingestor

       Payload format:
       {
        "command" : "STOP_INGESTION"
       }

    3. SNAPSHOT: to get the frame snapshot and feed in one frame into the video data pipeline

       Payload format:
       {
       "command" : "SNAPSHOT"
       }

       In order to use `SNAPSHOT` functionality one needs to enable the sw trigger mode and make sure ingestion should be stopped before getting the frame snapshot capture.
   ----

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
  *   to RTSP_CAMERA_IP in [../build/.env](../build/.env)

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

