# VideoIngestion Module

This module ingests video frames from a video source like video file or 
basler/RTSP/USB camera using gstreamer pipeline and publishes the 
`(topic, metadata,frame)` tuple to ZMQ bus.

The high level logical flow of VideoIngestion pipeline is as below:
1. VideoIngestion main program reads the ingestor and filter configuration
2. After reading the config, it starts the zmq publisher thread, single/multiple
   filter threads per filter configuration and ingestor thread
   based on ingestor configuration. It exits whenever an exception occurs during
   this startup sequence.
3. Ingestor thread reads from the ingestor configuration and adds
   data to ingestor queue
4. Based on the filter configuration, single or multiple filter
   threads consume ingestor queue and passes only the key frames with its 
   metadata to publisher queue
5. Publisher thread reads from the publisher queue and publishes it
   over the ZMQ bus
    
## Configuration

All the VideoIngestion module configurations (ingestor and filter) are added 
into etcd (distributed key-value data store) under `AppName` as mentioned in the
service definition in docker-compose.

### Ingestor config

Gstreamer based pipeline is supported for reading from basler/rtsp/usb 
cameras.using OpenCV directly to read from the video file. We are calling 
these different video sources.

Sample Ingestor configuration for each of the video sources below:
1. Video file (No Gstreamer pipeline involved before reading from OpenCV read
   API)
   ```
   {
        "video_src": "./test_videos/pcb_d2000.avi",
        "encoding": {
            "type": "jpg",
            "level": 100
        },
        "resolution": "1280x720",
        "loop_video": "true"
   }

   ```
2. Basler camera
   ```
    {
        "video_src": "pylonsrc imageformat=yuv422 exposureGigE=3250 interpacketdelay=6000 ! videoconvert ! appsink",
        "encoding": {
            "type": "jpg",
            "level": 100
        },
        "resolution": "1280x720",
        "poll_interval": 0.2        
    }
   ```
3. RTSP cvlc based camera simulation
   ```
    {
        "video_src": "rtspsrc location=\"rtsp://localhost:8554/\" latency=100 ! rtph264depay ! h264parse ! mfxdecode ! videoconvert ! appsink",
        "encoding": {
            "type": "jpg",
            "level": 100
        },
        "resolution": "1280x720",
        "poll_interval": 0.2        
    }
   ```
4. RTSP
   ```
    {
        "video_src": "rtspsrc location=\"rtsp://admin:intel123@<RTSP CAMERA IP>:554/\" latency=100 ! rtph264depay ! h264parse ! mfxdecode ! videoconvert ! appsink",
        "encoding": {
            "type": "jpg",
            "level": 100
        },
        "resolution": "1280x720",
        "poll_interval": 0.2        
    }

   ```
5. USB
   ```
    {
        "video_src": "v4l2src! videoconvert ! appsink",
        "encoding": {
            "type": "jpg",
            "level": 100
        },
        "resolution": "1280x720",
        "poll_interval": 0.2        
    }
   ```

[TODO: Add camera related configurations and update the possible values for 
each of the keys used above]

### Filter config

The Filter (user defined function) is responsible for doing pre-processing of the 
video frames. It uses the filter congiruation to do the selection of key frames
(frames of interest for further processing)

Sample configuration for filters used:
1. PCB filter
   ```
    {
        "input_queue_size": 10,
        "output_queue_size": 10,
        "max_workers": 5,
        "training_mode": "false",
        "n_total_px": 300000,
        "n_left_px": 1000,
        "n_right_px": 1000
    }
   ```
2. Bypass filter
   ```
    {
        "input_queue_size": 10,
        "output_queue_size": 10,
        "max_workers": 5,
        "training_mode": "false"
    }
   ```

[TODO: Add filter related configuration details]


## Installation

* Follow [Etcd/README.md](../Etcd/README.md) to have EIS pre-loaded data in
  etcd

* Run VideoIngestion

  Present working directory to try out below commands is: `[repo]/VideoIngestion`

    1. Build and Run VideoIngestion as container
        ```
        $ cd [repo]/docker_setup
        $ ln -sf VideoIngestion/.dockerignore ../.dockerignore
        $ docker-compose up --build ia_video_ingestion
        ```
    2. Update EIS VideoIngestion keys(ingestor and filter) in `etcd` using UI's
       like `EtcdKeeper` or programmatically and see if it picks it up 
       automatically without any container restarts. The important keys here
       would be `ingestor_name` and `filter_name` which would allow one to
       choose the available ingestor and filter configs. So whenever the values
       of above keys or the values of the ones that are pointed by them change, the 
       VI pipeline restarts automatically.
       Eg: <br>
       
       **Sample Etcd config:**
       ```
       "/../ingestor_name" : "pcb_video_file_ingestor"
       "/../pcb_video_file_ingestor": {...}
       ```
