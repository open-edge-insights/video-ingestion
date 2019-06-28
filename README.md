# Video Ingestion Module

This module ingests the video data from a file or a basler/RTSP/USB camera using gstreamer pipeline and stores the frame blob in ImageStore and frame metadata in influx using DataIngestionLib.

This is the reference for all the algorithms used in IEI

## Configuration

### Ingestion

The ingestion portion of the configuration represents the different ingestors
for the agent to load. The `ingestors` key inside of this configuration
represents which ingestors to load into the ingestion pipeline. Each key in
the `ingestors` object represents an ingestor to load and the value associated
with that key is the configuration for that ingestor.

It is important to note that if the specified ingestor does not exist and if the configuration for the
ingestor is not correct, then the ingestor will fail to be loaded.

* Supported Ingestors

    | Ingestor | Documentation |
    | :------: | :-----------: |
    | video_file    | [Link](../algos/dpm/ingestion/video_file.py ) |
    | video         | [Link](../algos/dpm/ingestion/video.py ) |

* The `trigger_threads` key in the configuration file is the maximum number of threads that the thread pool executor class uses at most for the trigger  process and the data ingestion process. The maximum value for trigger_threads has been limited to `10` in VideoIngestion as beyond this the max limit  for the number of HTTP connection opened by influxdb is reached and urllib3(python's HTTP client) discards the connection and issues a warning.

* The `poll_interval` in the configuration file which is taken as `seconds`, sets the ingestion speed of frames from camera. Essentially it determines the number of ingested frames from camera per second i.e., fps read from camera.

* The `queue_size` in the configuration file is the maximum size for the `trigger queue` and `dil (data ingestion lib) queue`. The frames captured by the camera will pass through the trigger_queue and dil queue, i.e., frames will enter the trigger_queue and output of the trigger_queue will then be the input of dil queue. Then frames are taken out from the dil queue for processing.

* The `vi_queue_size` in the configuration file is the maximum size for the the queue added to continuously read from gstreamer's appsink.

* In case any lag is observed in the end-to-end flow when working with physical cameras, set the appropriate values for `poll_interval`, `trigger_threads`, `queue_size` and `vi_queue_size` in configuration file.
    * Use appropriate `poll_interval` value depending on the CPU clock speed.
        **_Example_** configuration for basler camera with `poll_interval: 0.2` (i.e 0.2 seconds)

    * Increase the `trigger_threads` accordingly(maximum is 10).
        **_Example_** `trigger_threads : 10`

    * Based on the priority between the delay of the frames received and the dropping of frames, user can select the `queue_size` in the configuration file.
        * If dropping frames are tolerable but delay should be minimal, then `queue_size` should be set to minimal number.
            **_Example_** `queue_size : 2`
        * If certain delay (around 10 seconds) is tolerable but frames should not be dropped, then `queue_size` should be   set accordingly. **REMEMBER** : Increasing the `queue_size` will increase the delay.
            **_Example_** `queue_size : 10` 
    

```
    "streams": {
            "capture_streams": {
                "cam_serial1": {
                    "video_src": "pylonsrc imageformat=yuv422 exposure=3250 interpacketdelay=1500 ! videoconvert ! appsink drop=TRUE max-buffers=10",
                    "encoding": {
                        "type": "jpg",
                        "level": 100
                    },
                    "img_store_type": "inmemory_persistent",
                    "poll_interval": 0.2
                }
            }
        }

```

* Supported cameras via gstreamer pipeline

    * Basler Camera

        If you are working with a Basler camera, then put the stream in 'capture_streams' using the below gstreamer pipeline.
        The sample configurations is available in [factory_basler.json](../docker_setup/config/algo_config/factory_basler.json).

        ---
        **Note**:
        * In case multiple Basler cameras are connected use serial parameter to specify the camera to be used in the gstreamer pipeline in the video config file
        for camera mode. If multiple cameras are connected and the serial parameter is not specified then the source plugin by default connects to camera with device_index=0.

            Example Pipeline to connect to basler camera with serial number 22573664 :
            `"capture_streams":"pylonsrc serial=22573664 imageformat=yuv422 exposure=3250 interpacketdelay=1500 ! videoconvert ! appsink"`

        * In case frame read is failing when multiple basler cameras are used, use the interpacketdelay property to increase the delay between the
        transmission of each packet for the selected stream channel. Depending on the number of cameras used an appropriate delay can be set.

            Example Pipeline to increase the interpacket delay to 3000 (default value for interpacket delay is 1500):
            `"capture_streams":"pylonsrc imageformat=yuv422 exposure=3250 interpacketdelay=3000 ! videoconvert ! appsink"`

        * To work with monochrome Basler camera, please change the image format to `mono8` in the Pipeline.

            Example Pipeline to connect to monochrome basler camera with serial number 22773747 :
            `"capture_streams":"pylonsrc serial=22773747 imageformat=mono8 exposure=3250 interpacketdelay=1500 ! videoconvert ! appsink"`

        ---

    * USB Camera

        To work with USB Camera, then put the stream in 'capture_streams' using the below gstreamer pipeline.
        The sample configuration is available in [factory_usb.json](../docker_setup/config/algo_config/factory_usb.json).

        In case multiple USB cameras are connected specify the camera using the `device` property in the configuration file.

        Example configuration to work with two USB cameras,

        ```
        "data_ingestion_manager": {
                "ingestors": {
                    "video": {
                        "streams": {
                            "capture_streams": {
                                "cam_serial1": {
                                    "video_src": "v4l2src device=/dev/video0 ! videoconvert ! appsink drop=TRUE max-buffers=10",
                                    "encoding": {
                                        "type": "jpg",
                                        "level": 95
                                    },
                                    "img_store_type": "inmemory_persistent"
                                },
                                "cam_serial2": {
                                    "video_src": "v4l2src device=/dev/video1 ! videoconvert ! appsink drop=TRUE max-buffers=10",
                                    "encoding": {
                                        "type": "jpg",
                                        "level": 95
                                    },
                                    "img_store_type": "inmemory_persistent"
                                }
                            }
                        }
                    }
                }
            }
        ```

    * RTSP Camera

        If you are working with a hikvision ds2 RTSP camera, then put the stream in 'capture_streams' using the below gstreamer pipeline.

        The sample configuration is available in [factory_rtsp_hikvision_ds2.json](../docker_setup/config/algo_config/factory_rtsp_hikvision_ds2.json).
        Please update the value for `RTSP_CAMERA_IP` in [.env](.env) file with the IP address of the RSTP camera, so that it gets added to no_proxy env.

        If you are working with a stimulated RTSP stream generated using cvlc command , then put the stream in 'capture_streams' using the below gstreamer pipeline.
        The sample configuration is available in [factory_rtsp_cvlc.json](../docker_setup/config/algo_config/factory_rtsp_cvlc.json).

        #### Multiple camera configuration for RTSP cameras

        If one wants to add multiple RTSP cameras, they can do so by having a json object for `capture_streams` key with each key under this being a `serial number` of the camera and the json object it is pointing to, has all the configuration details for that camera. For reference, one can use [factory_mutli_cam.json](../docker_setup/config/algo_config/factory_multi_cam.json) and do the necessary tweaks.

        > **Note**:
        > In order to use the RTSP stream, the RTSP server must be started using VCL with the following command:
        `cvlc -vvv file://<absolute_path_to_video_file> --sout '#gather:rtp{sdp=rtsp://localhost:8554/}' --loop --sout-keep`
        
        > In order to start RTSP server vlc must be installed using apt command. Please install vlc using the following commmand:
        `sudo apt install vlc`

        Gstreamer MediaSDK decoding commands requires there to be a parser and then the decoder.
        Eg: parsers and decoders:
        * parseh264parse !  mfx264dec
        * h265parse ! mfxhevcdec
        * mfxmpegvideoparse ! mfxmpeg2dec

        ---

    **Note**:
    1. In case encoding option needs to be enabled add the `encoding` key in the video config json files.
    The supported encoding types are `jpg` and `png`.
    The supported encoding level for `jpg` is `0-100`.
    The supported encoding level for `png` is `0-9`.
    2. In case resizing option needs to be enabled add the `resize_resolution ` key in the video config files.
    The `resize_resolution` key takes frame width and frame height as, `resize_resolution = width x height`.
    3. In case the `encoding` or `resize_resolution` key is not added encoding or resizing will not be enabled.
    4. In case one needs to specify the image storage type, one can use the `img_store_type` key.
    The supported storage types are `inmemory`, `persistent` and `inmemory_persistent`.
    `inmemory_persistent` option will add the image to both the inmemory and persistent storage.
    In case the `img_store_type` key is not specified, then by default the image will be added to inmemory storage.
    ---

### Triggers

Triggers represent the different trigger algorithms to use on the
video frames that are ingested.

It is important to note that if the specified trigger does not exist, then all the frames are ingested without choosing any key frames.

* Supported Triggers

    | Trigger | Reference |
    | :--------: | :-----------: |
    | pcb_trigger     | [Link](../algos/dpm/triggers/pcb_triger.py)|
    | bypass_trigger  | [Link](../algos/dpm/triggers/bypass_trigger.py)|

### Classifiers

Classifiers represent the different classification algorithms to use on the
video frames that are ingested.

The keys in the `classifiers` object in the configuration
represent the classifier you wish to load. The value associated
with that key is the configuration for that classifier.

It is important to note that if the specified classifier does not exist and the configuration for the
classifier is not correct, then the classifier will fail to be loaded.

* Supported Classifiers

    | Classifier | Reference |
    | :--------: | :-----------: |
    | pcbdemo     | [Link](../algos/dpm/classification/classifiers/pcbdemo) |
    | classification_sample     | [Link](../algos/dpm/classification/classifiers/classification_sample) |
    | dummy     | [Link](../algos/dpm/classification/classifiers/dummy.py) |
