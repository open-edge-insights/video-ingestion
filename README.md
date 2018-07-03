# Video Ingestion Module:
This module injests the video data from a file or basler's webcam and sends it to Image Store and influx using Data Ingestion Lib.

## Steps to run this module

* Run the DataAgent with the config file having right configs for InfluxDB and Redis.
  
  ```sh
  go run DataAgent/DataAgent.go -config=<config_file> -log_dir=<log_dir>
  ```
  
* To install python dependencies for this module, use cmd:
  
  ```sh
  pip3 install -r vi_requirements.txt
  ```

* In another terminal export the PYTHONPATH as done for DataAgent and run the command.
  
  ```sh
  python3 VideoIngestion/VideoIngestion.py --config factory.json
  ```
  
  In place of factory.json one can give another json file too with the required configuration.

* In case one is using the robotic arm trigger, this trigger waits until it receives a camera ON message over mqtt. To publish this message run the command.
  
  ```sh
  python3 VideoIngestion/test/mqtt_publish.py
  ```

* To read the frames from ImageStore and view it, run the command.
  
  ```sh
  python3 VideoIngestion/test/read_frames.py
  ```
