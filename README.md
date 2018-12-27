# Video Ingestion Module:
This module injests the video data from a file or basler's webcam and sends it to Image Store and influx using Data Ingestion Lib.

## Steps to run this module

* Run the DataAgent with the config file having right configs for InfluxDB and Redis.
  
  ```sh
  go run DataAgent/DataAgent.go -config=<config_file> -log_dir=<log_dir>
  ```
  
* To install python dependencies for this module, use cmd:
  
  ```sh
  sudo -H pip3 install -r vi_requirements.txt
  ```

* In another terminal export the PYTHONPATH as done for DataAgent and run the command.
  
  ```sh
  python3 VideoIngestion/VideoIngestion.py --config factory.json
  ```
  
  In place of factory.json one can give another json file too with the required configuration.

* In case one is trying with robotic arm trigger/Basler's camera, please follow the below steps:

  **Pre-requisites**:
  Refer `basler-video-capture/README.md` till `Compilation and Installation` section having command `python3 setup.py install`. If everything installed successfully, one should see `basler-capture` tool accessible in terminal. This works with python3.6/3.5. If one faces any issue while running with python3.6, please install `sudo apt-get install python3.6-dev` package and try.

  * In one terminal, run VideoIngestion module: `python3 VideoIngestion/VideoIngestion.py --config factory_prod.json`
  * The `VideoIngestion.py` waits until it receives a camera ON message over mqtt. To publish this message run: `python3 VideoIngestion/test/mqtt_publish.py` in another terminal
  * To read the frames from ImageStore and view it, run the command: `python3 VideoIngestion/test/read_frames.py`
