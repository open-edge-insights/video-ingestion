# VideoIngestion Module

This module ingests video frames from a video source like video file or 
basler/RTSP/USB camera using gstreamer pipeline and publishes the 
`[topic, metadata, frame_blob]` to ZMQ bus.

The high level logical flow of VideoIngestion pipeline is as below:
1. VideoIngestion main program reads the ingestor and filter configuration
2. After reading, it starts the zmq publisher thread, filter (single
   or multiple threads per filter configuration) and ingestor thread
   based on ingestor configuration. It exits whenever an exception occurs
3. Ingestor thread reads from the ingestor configuration and adds
   data to ingestor queue
4. Based on the filter configuration, single or multiple filter
   threads consume ingestor queue and adds the key frames data to
   publisher queue
5. Publisher thread reads from the publisher queue and publishes it
   to the ZMQ bus
    
## Configuration

All the VideoIngestion module configurations (ingestor and filter) are added 
into etcd (distributed key-value data store) under `/VideoIngestion/` tree.

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

[TODO: Add camera releated configurations and update the possible values for 
each of the keys used above]

### Filter config

The Filter(user defined function) responsible for doing pre-processing of the 
video frames uses the filter congiruation to do the selection of key frames
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
3. No filter (For this configuration, please take off the )

[TODO: Add filter releated configurations details]


## Installation

* Run etcd daemon as container

    Below script starts `etcd` as a container
    ```
    $ ./test/start_etcd_container.sh
    ```

* Add pre-loaded EIS data to etcd store

    Below script adds [etcd_pre_load.json](test/etcd_pre_load.json) into `etcd`.
    All the configuration gets stored under '/VideoIngestion/` 

    1. Install `etcd3-python`
       
        ```
        $ export PY_ETCD3_VERSION=cdc4c48bde88a795230a02aa574df84ed9ccfa52
        $ git clone https://github.com/kragniz/python-etcd3 && \
          cd python-etcd3 && \
          git checkout ${PY_ETCD3_VERSION} && \
          python3.6 setup.py install && \
          cd .. && \
          rm -rf python-etcd3
        
        ```
    2. Add pre-loaded json data to etcd
       
        ```
         $ cd test
         $ python3.6 etcd_put.py
        ```

* Run VideoIngestion

  Present working directory to try out below commands is: `[repo]/VideoIngestion`

    1. Build and Run VideoIngestion as container
        ```
        $ cd [repo]/docker_setup
        $ docker-compose up --build ia_video_ingestion
        ```
    2. Update EIS VideoIngestion data in `etcd` and see if VideoIngestion    
       picks it up automatically without any container restarts


