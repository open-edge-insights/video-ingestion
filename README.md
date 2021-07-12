# `VideoIngestion Module`

The VideoIngestion(VI) module is mainly responsibly for ingesting the video frames
coming from a video source like video file or basler/RTSP/USB camera
into the EII stack for further processing. Additionally, by having VI run with
classifier and post-processing UDFs, VI can perform the job of VA(VideoAnalytics)
service also.

The high level logical flow of VideoIngestion pipeline is as below:

1. App reads the application configuration via EII Configuration Manager which
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
   on EII MessageBus.

---
> **NOTE**:
Below usecases are suitable for single node deployment where one can avoid the
overhead of VA(VideoAnalytics) service.

> 1. If VI(VideoIngestion) service is configured with an UDF that does the
   classification, then one may choose to not to have VA service as all
   pre-processing, classification and any post-processing can be handled in VI
   itself with the usage of multiple UDFs.
> 2. If VI(VideoIngestion) service is using GVA(Gstreamer Video Analytics)
   elements also, then one may choose to not to have VA service as all
   pre-processing, classification and any post-processing(using vappi gstreamer
   elements) can be done in gstreamer pipeline itself. Also, the post-processing
   here can be configured by having multiple UDFs in VI if needed.

---

## `Configuration`

1. [Udfs Configuration](../common/video/udfs/README.md)
2. [Etcd Secrets Configuration](../Etcd_Secrets_Configuration.md) and
3. [MessageBus Configuration](../common/libs/ConfigMgr/README.md#interfaces) respectively.
4. [JSON schema](schema.json)
---

All the app module configuration are added into distributed key-value store under `AppName` env, as mentioned in the environment section of this app's service definition in docker-compose. If `AppName` is `VideoIngestion`, then the app's config would be fetched from
`/VideoIngestion/config` key via EII Configuration Manager.


>**Note**:

> * Developer mode related overrides go into docker-compose-dev.override.yml

> * For `jpeg` encoding type, `level` is the quality from `0 to 100` (the higher is the better)

> * For `png` encoding type, `level` is the compression level from `0 to 9`. A higher value means a smaller size and longer compression time.

> * One can use [JSON validator tool](https://www.jsonschemavalidator.net/) for validating the app configuration against the above schema.

----

### `Ingestor config`

The following are the type of ingestors supported:

1. [OpenCV](https://opencv.org/)
2. [GStreamer](docs/gstreamer_ingestor_doc.md)
3. [RealSense](https://www.intelrealsense.com/)
  
   **For more information on Intel RealSense SDK refer [librealsense](https://github.com/IntelRealSense/librealsense)**

  ----

### `Video Ingestion Contents`

1. [Generic Server](docs/generic_server_doc.md)
2. [Gstreamer Video Analytics](docs/gva_doc.md)
3. [RUN GVA elements in VI or VA](models/README.md)
4. [GenICam GigE/USB3.0 Camera Support](#genicam-gige-or-usb3-camera)
5. [RTSP Camera Support](#rtsp-camera)
6. [USB Camera Support](#usb-camera)

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
#### GenICam GigE or USB3 Camera

  **Refer [GenICam GigE/USB3.0 Camera Support](docs/generic_plugin_doc.md) for information/configuration on GenICam GigE/USB3 camera support.**

  **Pre-requisities for working with GenICam compliant cameras:**

  The following are the pre-requisites for working with GeniCam compliant cameras. Please note that these changes need to be reverted while working with other cameras such as realsense, rtsp and usb(v4l2 driver compliant).

  Refer the below snip of `ia_video_ingestion` service to add the required changes in [docker-compose.yml](./docker-compose.yml) file of the respective Ingestion service(including custom udf services). Once the changes are made make sure [builder.py](../builder.py) is executed before building and running the services.


  **For GenICam GigE Camera:**

      ```yaml
      ia_video_ingestion:
        # Add root user
        user: root
        # Add network mode host
        network_mode: host
        # Please make sure that the above commands are not added under environment section and also take care about the indentations in the compose file.
        ...
        environment:
        ...
          # Add HOST_IP to no_proxy and ETCD_HOST
          no_proxy: "${RTSP_CAMERA_IP},<HOST_IP>"
          ETCD_HOST: "<HOST_IP>"
        ...
        # Comment networks section as below as it will throw an error when network mode host is used.
        # networks:
          # - eii
        # Comment ports section as below
        # ports:
        # - 64013:64013
      ```
  **For GenIcam USB3.0 Camera:**

      ```yaml
      ia_video_ingestion:
        # Add root user
        user: root
        ...
        environment:
          # refer [GenICam GigE/USB3.0 Camera Support](docs/generic_plugin_doc.md) to install the respective camera SDK
          # Setting GENICAM value to the respective camera/GenTL producer which needs to be used
          GENICAM: "<CAMERA/GenTL>"
        ...
      ```

    > **Note:**
    >  * In case one notices GenICam cameras not getting initialized during runtime then try executing `docker system prune` command on the host system and then removing the GenICam specific semaphore files under `/dev/shm/` path of the host system. `docker system prune` command will remove all stopped containers, networks not used by at least one container, dangling images and build cache which could prevent the plugin from accessing the device.
    >  * In case one notices `Feature not writable` while working with GenICam cameras please reset the device using camera software or using the reset property of [Generic Plugin README](src-gst-gencamsrc/README).
    >  * In `IPC` mode if Ingestion service is running with `root` privilige then `ia_video_analytics` and `ia_visualizer` service subscribing to it must also run with `root` privileges.
    >  * In multi-node scenario, replace <HOST_IP> in "no_proxy" with leader node IP address.
    >  * In TCP mode of communication, msgbus subscribers and clients of VideoIngestion are required to configure the "EndPoint" in config.json with host IP and port under "Subscribers" or "Clients" interfaces section.

 * `Gstreamer Ingestor`
    
    * `GenICam GigE/USB3.0 cameras`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "gencamsrc serial=<DEVICE_SERIAL_NUMBER> pixel-format=<PIXEL_FORMAT> ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```
      > **Note:** 
      
      > * Generic Plugin can work only with GenICam compliant cameras and only with gstreamer ingestor.
      > * The above gstreamer pipeline was tested with Basler and IDS GigE cameras. 
      > * If `serial` is not provided then the first connected camera in the device list will be used. 
      > * If `pixel-format` is not provided then the default `mono8` pixel format will be used.
      > * If `width` and `height` properies are not set then gencamsrc plugin will set the maximum resolution supported by the camera.

   * `Hardware trigger based ingestion with gstreamer ingestor`

     ```javascript
     {
      "type": "gstreamer",
      "pipeline": "gencamsrc serial=<DEVICE_SERIAL_NUMBER> pixel-format=<PIXEL_FORMAT> trigger-selector=FrameStart trigger-source=Line1 trigger-activation=RisingEdge hw-trigger-timeout=100 acquisition-mode=singleframe ! videoconvert ! video/x-raw,format=BGR ! appsink"
     }
     ```

    > **Note:**
    > * For PCB usecase use the `width` and `height` properties of gencamsrc to set the resolution to `1920x1200` and make sure it is pointing to the rotating pcb boards as seen in `pcb_d2000.avi` video file for pcb filter to work. 
    
    One can refer the below example pipeline:
      
      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "gencamsrc serial=<DEVICE_SERIAL_NUMBER> pixel-format=ycbcr422_8 width=1920 height=1200 ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```
    
    **Refer [docs/basler_doc.md](docs/basler_doc.md) for more information/configuration on basler camera.**

 ----

#### RTSP Camera

  **Refer [docs/rtsp_doc.md](docs/rtsp_doc.md) for information/configuration on rtsp camera.**

  * `OpenCV Ingestor`

      ```javascript
      {
        "type": "opencv",
        "pipeline": "rtsp://<USERNAME>:<PASSWORD>@<RTSP_CAMERA_IP>:<PORT>/<FEED>"
      }
      ```

    > **NOTE**: Opencv for rtsp will use software decoders

  * `Gstreamer Ingestor`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://<USERNAME>:<PASSWORD>@<RTSP_CAMERA_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

    > **Note:** The RTSP URI of the physical camera depends on how it is configured using the camera software. One can use VLC Network Stream to verify the RTSP URI to confirm the RTSP source.

 ----
* `RTSP simulated camera using cvlc`

  * `OpenCV Ingestor`

      ```javascript
      {
        "type": "opencv",
        "pipeline": "rtsp://<SOURCE_IP>:<PORT>/<FEED>"
      }
      ```

  * `Gstreamer Ingestor`

      ```javascript
      {
        "type": "gstreamer",
        "pipeline": "rtspsrc location=\"rtsp://<SOURCE_IP>:<PORT>/<FEED>\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! videoconvert ! video/x-raw,format=BGR ! appsink"
      }
      ```

    **Refer [docs/rtsp_doc.md](docs/rtsp_doc.md) for more information/configuration on rtsp simulated camera.**

 ----

#### USB Camera

**Refer [docs/usb_doc.md](docs/usb_doc.md) for information/configurations on usb camera.**

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
 ----

#### RealSense Depth Camera

  * `RealSense Ingestor`

    ```javascript
       "ingestor": {
            "type": "realsense",
            "serial": "<DEVICE_SERIAL_NUMBER>",
            "framerate": <FRAMERATE>,
            "imu_on": true
        },
     ```
    > **Note**
    > *  RealSense Ingestor was tested with Intel RealSense Depth Camera D435i
    > *  RealSense Ingestor does not support poll_interval. Please use framerate to reduce the ingestion fps if required.

    > *  If `serial` config is not provided then the first realsense camera in the device list will be connected.

    > *  If `framerate` config is not provided then the default framerate of 30 will be applied. Please make sure that the framerate provided is compatible with both the color and depth sensor of the realsense camera. With D435i camera only framerate 6,15,30 and 60 is supported and tested.

    > * IMU stream will work only if the realsense camera model supports the IMU feature. The default value for `imu_on` is set to false.

 ----

> **Note**: 

> For all video and camera streams please make sure you are using appropriate UDF configuration. One may not get the expected output in the Visualizer/WebVisualizer screen if the udf is not compatible with the video source.

> If one is not sure about the compatibility of the udf and video source then 'dummy' udf can be used. It won't do any analytics on the video and hence won't filter any of the video frames. You will, therefore, see the video streamed by the camera as it is on the video output screen in Visualizer/WebVisualizer.

> Refer below configuration for 'dummy' UDF:

  ```javascript
    "udfs": [{
        "name": "dummy",
        "type": "python"
    }]
  ```
> Same changes need to be applied in VideoAnalytics configuration if it is subscribing to VideoIngestion.