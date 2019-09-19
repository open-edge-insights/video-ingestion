# `VideoIngestion Module`

This module ingests video frames from a video source like video file or
basler/RTSP/USB camera using gstreamer pipeline and publishes the
`(metadata, frame)` tuple to messagebus.

The high level logical flow of VideoIngestion pipeline is as below:
1. VideoIngestion main program reads the ingestor and filter configuration
2. After reading the config, it starts the messagebus publisher thread,
   single/multiple filter threads per filter configuration and ingestor thread
   based on ingestor configuration. It exits whenever an exception occurs during
   this startup sequence.
3. Ingestor thread reads from the ingestor configuration and adds
   data to ingestor queue
4. Based on the filter configuration, single or multiple filter
   threads consume ingestor queue and passes only the key frames with its
   metadata to publisher queue
5. Publisher thread reads from the publisher queue and publishes it
   over the message bus

## `Configuration`

All the VideoIngestion module configuration are added into etcd (distributed
key-value data store) under `AppName` as mentioned in the
environment section of this app's service definition in docker-compose.

If `AppName` is `VideoIngestion`, then the app's config would look like as below
 for `/VideoIngestion/config` key with `ingestor` and `filter` configs in Etcd:
```
{
        "ingestor": {
            "video_src": "./test_videos/pcb_d2000.avi",
            "encoding": {
                "type": "jpg",
                "level": 100
            },
            "loop_video": "true"
        },
        "filter": {
            "name": "pcb_filter",
            "queue_size": 10,
            "max_workers": 5,
            "training_mode": "false",
            "n_total_px": 300000,
            "n_left_px": 1000,
            "n_right_px": 1000
        }
}
```

> **NOTE**: The above `ingestor` and `filter` config correspond to PCB demo
> usecase

For more details on Etcd and MessageBus endpoint configuration, visit [Etcd_and_MsgBus_Endpoint_Configuration](../Etcd_and_MsgBus_Endpoint_Configuration.md).


### `Ingestor config`

Gstreamer based pipeline is supported for reading from basler/rtsp/usb
cameras through OpenCV.

Sample Ingestor configuration(forms the `ingestor` value in app's config) for
each of the video sources below:

1. **Video file** (No Gstreamer pipeline involved before reading from OpenCV
   read API)
   ```
   {
        "video_src": "./test_videos/pcb_d2000.avi",
        "encoding": {
            "type": "jpg",
            "level": 100
        },
        "loop_video": "true"
   }
   ```
   **NOTE**: Change the "video_src" to classification sample
             `./test_videos/classification_vid.avi` for classification sample
             use case with `Bypass filter`

2. **Basler camera**
   ```
    {
        "video_src": "pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=6000 ! videoconvert ! appsink",
        "encoding": {
            "type": "jpg",
            "level": 100
        },
        "poll_interval": 0.2
    }
   ```
   --------
   **NOTE**:

   * In case multiple Basler cameras are connected use serial parameter to
     specify the camera to be used in the gstreamer pipeline in the video
     config file for camera mode. If multiple cameras are connected and the
     `serial` parameter is not specified then the source plugin by default
     connects to camera with device_index=0.

     **Eg**: `video_src` value to connect to basler camera with
     serial number `22573664`:
     `"video_src":"pylonsrc serial=22573664 imageformat=yuv422 exposureGigE=3250 interpacketdelay=1500 ! videoconvert ! appsink"`

   * In case you want to enable resizing with basler camera use the `vaapipostproc` element and specify the `height` and `width` parameter in the          gstreamer pipeline.

        **Eg**: Example pipeline to enable resizing with basler camera
        `"video_src":"pylonsrc serial=22573664 imageformat=yuv422 exposureGigE=3250 interpacketdelay=1500 ! vaapipostproc height=600 width=600 ! videoconvert ! appsink"`

   * In case frame read is failing when multiple basler cameras are used, use
     the `interpacketdelay` property to increase the delay between the
     transmission of each packet for the selected stream channel.
     Depending on the number of cameras, use an appropriate delay can be set.

     **Eg**: `video_src` value to increase the interpacket delay to 3000(default
     value for interpacket delay is 1500):
     `"video_src":"pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=3000 ! videoconvert ! appsink"`

   * To work with monochrome Basler camera, please change the
     image format to `mono8` in the Pipeline.

     **Eg**:`video_src` value to connect to monochrome basler camera with serial
     number 22773747 :
     `"video_src":"pylonsrc serial=22773747 imageformat=mono8   exposureGigE=3250 interpacketdelay=1500 ! videoconvert ! appsink"`

   * To work with USB Basler camera, please change the
     exposure parameter to `exposureUsb` in the Pipeline.

     `"video_src":"pylonsrc serial=22773747 imageformat=mono8 exposureUsb=3250 interpacketdelay=1500 ! videoconvert ! appsink"`

    ---
3. **RTSP cvlc based camera simulation**
   ```
    {
        "video_src": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! videoconvert ! appsink",
        "encoding": {
            "type": "jpg",
            "level": 100
        },
        "poll_interval": 0.2
    }
   ```
   ------
   **NOTE**:

    * In case you want to enable resizing with RTSP cvlc based camera use the `vaapipostproc` element and specifiy the `height` and `width` parameter in the          gstreamer pipeline.

        **Eg**: Example pipeline to enable resizing with RTSP camera
        `"video_src": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx height=600 width=600 ! videoconvert ! appsink"`

   * Install VLC if not installed already: `sudo apt install vlc`
   * In order to use the RTSP stream from cvlc, the RTSP server
     must be started using VLC with the following command:
     `cvlc -vvv file://<absolute_path_to_video_file> --sout '#gather:rtp{sdp=rtsp://localhost:8554/}' --loop --sout-keep`
   ------

4. **RTSP camera**
   ```
    {
        "video_src": "rtspsrc location=\"rtsp://admin:intel123@<RTSP CAMERA IP>:554/\" latency=100 ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx ! videoconvert ! appsink",
        "encoding": {
            "type": "jpg",
            "level": 100
        },
        "poll_interval": 0.2
    }

   ```
   ------
   **NOTE**:

    * In case you want to enable resizing with RTSP camera use the `vaapipostproc` element and specifiy the `height` and `width` parameter in the          gstreamer pipeline.

        **Eg**: Example pipeline to enable resizing with RTSP camera
        `"video_src": "rtspsrc location=\"rtsp://admin:intel123@<RTSP CAMERA IP>:554/\" latency=100  ! rtph264depay ! h264parse ! vaapih264dec ! vaapipostproc format=bgrx height=600 width=600 ! videoconvert ! appsink"`

   * If working behind a proxy, RTSP camera IP need to be updated to RTSP_CAMERA_IP in GlobalEnv in the etcd config.
   * For working both with simulated RTSP server via cvlc or direct streaming
     from RTSP camera, we can use the below Gstreamer MediaSDK parsers and
     decoders based on the input stream type
     **Eg**: parsers and decoders:
      * parseh264parse !  mfx264dec
      * h265parse ! mfxhevcdec
      * mfxmpegvideoparse ! mfxmpeg2dec
     **NOTE**: If running on older systems where we don't have hardware media
     decoders, the above parsers and decoders may not work. In those cases,
     one can use `h24parse | avdec_h264` which is a software decoder.
   ------

5. **USB camera**
   ```
    {
        "video_src": "v4l2src ! videoconvert ! appsink",
        "encoding": {
            "type": "jpg",
            "level": 100
        },
        "poll_interval": 0.2
    }
   ```
   -------
   **NOTE**:

     * In case you want to enable resizing with USB camera use the `vaapipostproc` element and specifiy the `height` and `width` parameter in the          gstreamer pipeline.

        **Eg**: Example pipeline to enable resizing with basler camera
        `"video_src":"v4l2src ! vaapipostproc height=600 width=600 ! videoconvert ! appsink"`

   * In case, multiple USB cameras are connected specify the
     camera using the `device` property in the configuration file.
     Eg:
     `"video_src": "v4l2src device=/dev/video0 ! videoconvert ! appsink"`
   -------

#### `Detailed description on each of the keys used`

|  Key	        | Description 	                        | Possible Values  	                                            | Required/Optional	|
|---	        |---	                                |---	                                                        |---	            |
|  video_src    |  Video source                         | Video file or gstreamer based pipeline 	                    | Required 	        |
|  encoding     |  Encodes the video frame	            | Supported encoding types: `jpg` or `png`. For `jpg`,encoding level is between `0-100` and or `png`, it's `0-9`)                                   | Optional          |
|  poll_interval|  Determines fps read rate from camera | floating number  	                                            | Optional  	    |
|  loop_video	|  Would loop through the video file    | "true" or "false"	(By default, it's false)                    | Optional          |


### `Filter config`

The Filter (user defined function) is responsible for doing pre-processing of the
ingested video frames. It uses the filter configuration to do the selection of
key frames(frames of interest for further processing).

------
**NOTE**: Please note if there is no `filter` key in the app's config, then
          it's as good as running the VI(VideoIngestion) pipeline without any
          filter thread/s. Functionally, it is equivalent to running with
          "bypass_filter" where the ingested frames are passed to classifier
          module as is without any pre-processing with added advantage of no
          filter threads.
------

**Sample configuration(forms the `filter` value in app's config) for filters used:**
1. **PCB filter**

   Works well with all PCB video file ingestors. To work with physical camera,
   proper setup is required with good lighting conditions. Proper training and
   tweaking filter and classifier logic may be required.
   ```

    {
        "name": "pcb_filter",
        "queue_size": 10,
        "max_workers": 1,
        "training_mode": "false",
        "n_total_px": 300000,
        "n_left_px": 1000,
        "n_right_px": 1000
    }
   ```
2. **Bypass filter**

   Works well with PCB or sample classfication video file ingestor. In general,
   works for any usecase where the ingested frames had be passed on to the
   classifier module as is without any pre-processing involved to select key
   frames.
   ```
    {
        "name": "bypass_filter",
        "queue_size": 10,
        "max_workers": 1,
        "training_mode": "false"
    }
   ```

**Sample filters code**


|  File	           | Description 	            | Link  	                       |
|---	           |---	                        |---	                           |
| base_filter.py   | Base class for all filters | [Link](..libs/base_filter.py)    |
| pcb_filter.py    | PCB Demo filter            | [Link](filters/pcb_filter.py)    |
| bypass_filter.py | Bypass filter              | [Link](filters/bypass_filter.py) |

#### `Detailed description on each of the keys used`

|  Key	        | Description 	                                                    | Possible Values  	                      | Required/Optional |
|---	        |---	                                                            |---	                                  |---	              |
|  name 	    |   File name of the filter	| "pcb_filter" or "bypass_filter"       | Required	                              |                   |
|  queue_size 	|   Determines the size of the input and output filter queue	    | any value that suits platform resources |   Required	      |
|  max_workers 	|   Number of threads to perform filter operation                   | any value that suits platform resources                                                                                                               (Not more than 5 * number of cpu cores) |   Required        |
|  training_mode|  If "true", used to capture images for training and building model| "true" or "false" (default is false)    |   Optional        |

**Note**: The other keys used are specific to filter usecase

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
    2. Update EIS VideoIngestion config key value in `etcd` using UI
       like `EtcdKeeper` or programmatically. Please note that the dynamic
       update of the config only works for the "ingestor" key value without
       container restarts. If "filter" key value is changed, then the changes
       are picked by restarting the container.
