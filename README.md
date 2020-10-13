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

    * Heterogeneous Execution needs to be used with FPGA plugin. One must still point to the CPU plugin or the GPU plugin as fallback devices for heterogeneous plugin. HETERO does not support VPU. For more information refer the below link:

  https://docs.openvinotoolkit.org/latest/_docs_IE_DG_supported_plugins_FPGA.html

    * GVA elements have compatibility issue with FPGA plugin. One may notice issue when `HETERO:FPGA,CPU` or `HETERO:FPGA,GPU` is used with the `device` property of GVA elements.

One can use [JSON validator tool](https://www.jsonschemavalidator.net/) for validating the app configuration against the above schema.

### `Ingestor config`

The following are the type of ingestors supported:

1. OpenCV
2. GStreamer

**Refer [docs/gstreamer_ingestor_doc.md](docs/gstreamer_ingestor_doc.md) for more information/configuration on gstreamer ingestor.**

  ----
#### `Camera independent Software Trigger way of video ingestion`

  * Software triggering way of video ingestion is a solution which enables to control video ingestion from all types of cameras, any configuration (Gstreamer & Opencv) / video files.

  * The regular way of video ingestion is autonomous i.e. as soon as the Video Ingestion micro-service is started the ingestion starts automatically using the video source (file/camera). There is no way to control the ingestion without stopping the ingestion micro-service itself. Software trigger feature provides a mechanism to start & stop video ingestion using software triggers sent by the client application.

  ----
#### `Generic Server in VideoIngestion`

  * VI generic server responds back to the client with the return values specific to the command. There is a total flexibility in sending the number & type of arguments back to client which is totally dependent on the command.


  * Example JSON format for incoming payload from client to server to initialize software trigger:
  ```javascript
  {
   "init_state" : "running"
  }
  ```

**Refer [docs/gerneric_server_doc.md](docs/generic_server_doc.md) for more information/configuration on software trigger and generic server.**

  ----
#### `Gstreamer Video Analytics`
VI module supports the usage of GVA (Gstreamer Video Analytics) plugins with `gstreamer` ingestor. [GVA](https://github.com/opencv/gst-video-analytics) is
a collection of GStreamer elements to enable CNN model based video analytics capabilities (such as object detection, classification, recognition) in GStreamer framework.

**Refer [docs/gva_doc.md](docs/gva_doc.md) for more information on GVA elements.**

  ----
#### `Generic Plugin`
Generic Plugin is a gstreamer generic source plugin that communicates and streams from a GenICam based camera which provides a GenTL producer. In order to use
the generic plugin with VI one must install the respective GenICam camera SDK and make sure the compatible GenTL producer for the camera is installed. All of the mentioned camera SDKs in [install_genicam_sdk.sh](install_genicam_sdk.sh) will be installed during build. Set `GENICAM` environment variable for exporting the GenTL producer path in [../build/docker-compose.yml](../build/docker-compose.yml) accordingly.

**Refer the below snippet example to select `Basler` camera and export its GenTL producer path in [../build/docker-compose.yml](../build/docker-compose.yml)**
```bash
 ia_video_ingestion:
 ...
   environment:
   ...
   # Setting GENICAM value to the respective camera which needs to be used
   GENICAM: "Basler"
```
**Refer [src-gst-gencamsrc/README.md](src-gst-gencamsrc/README.md) for more information on Generic Plugin**.

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
  **Refer [docs/multifilesrc_doc.md](docs/multifilesrc_doc.md) for more information/configuration on multifilesrc element.**

  ----
* `Basler Camera`

  * `Gstreamer Ingestor`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "gencamsrc serial=<DEVICE_SERIAL_NUMBER> pixel-format=ycbcr422_8 width=1920 height=1080 exposure-time=3250 ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

  * `Basler USB3.0 Interface camera with Gstreamer Ingestor`

     ```javascript
     {
       "type": "gstreamer",
       "pipeline": "gencamsrc serial=<DEVICE_SERIAL_NUMBER> pixel-format=rgb8 width=1920 height=1080 exposure-time=3250 ! videoconvert !  video/x-raw,format=BGR ! appsink"
     }
     ```

   * `Hardware trigger based ingestion with gstreamer ingestor`

     ```javascript
     {
      "type": "gstreamer",
      "pipeline": "gencamsrc serial=<DEVICE_SERIAL_NUMBER> pixel-format=ycbcr422_8 width=1920 height=1080 exposure-time=3250 trigger-selector=FrameStart trigger-source=Line1 trigger-activation=RisingEdge hw-trigger-timeout=100 acquisition-mode=singleframe ! videoconvert ! video/x-raw,format=BGR ! appsink"
     }
     ```

  **Refer [docs/basler_doc.md](docs/basler_doc.md) for more information/configuration on basler camera.**

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

  **Refer [docs/rtsp_doc.md](docs/rtsp_doc.md) for more information/configuration on rtsp camera.**

  ----
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

  **Refer [docs/usb_doc.md](docs/usb_doc.md) for more information/configurations on usb camera.**

  ----
* `RTSP simulated camera using cvlc`

  * `OpenCV Ingestor`

      ```javascript
      {
        "type": "opencv",
        "pipeline": "rtsp://localhost:8554/"
      }
      ```

  * `Gstreamer Ingestor`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

  **Refer [docs/rtsp_doc.md](docs/rtsp_doc.md) for more information/configuration on rtsp simulated camera.**

  ----


