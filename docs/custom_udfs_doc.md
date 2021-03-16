### Adding custom udfs in VideoIngestion/VideoAnalytics:

1. **For python udfs:** Copy udf code to `[WORKDIR]/IEdgeInsights/common/video/udfs/python` directory

    Refer the below example to copy the python safety gear udf (For this example python safety_gear udf can be copied from [WORKDIR]/IEdgeInsights/CustomUdfs/PySafetyGearAnalytics):

    $ cp -r <PATH>/safety_gear [WORKDIR]/IEdgeInsights/common/video/udfs/python


    **For native udfs:** Copy udf code to `[WORKDIR]/IEdgeInsights/common/video/udfs/native` directory

    Refer the below example to copy the native safety gear udf (For this example native safety_gear_demo udf can be copied from [WORKDIR]/IEdgeInsights/CustomUdfs/NativeSafetyGearAnalytics):

    $ cp -r <PATH>/safety_gear [WORKDIR]/IEdgeInsights/common/video/udfs/native


2. Make sure the appropriate ingestion config for the udf is set. In case one needs to use a video file then copy the video file to `[WORKDIR]/IEdgeInsights/VideoIngestion/test_videos`  and make the config changes.

    Refer the below example to copy `Safety_Full_Hat_and_Vest.avi` video to `[WORKDIR]/IEdgeInsights/VideoIngestion/test_videos`

    $ cp -r <PATH>Safety_Full_Hat_and_Vest.avi [WORKDIR]/IEdgeInsights/VideoIngestion/test_videos

    In [WORKDIR]/IEdgeInsights/VideoIngestion/config.json make the below changes to use `Safety_Full_Hat_and_Vest.avi` video file for safety_gear udf

	```javascript
	----snip----
	 "ingestor": {
            "type": "opencv",
            "pipeline": "./test_videos/Safety_Full_Hat_and_Vest.avi",
            "loop_video": true,
            "queue_size": 10,
            "poll_interval": 0.2
        },
    ```

3. Make the required config changes to add the udfs.

   **For python udfs:**

   Refer the below example to make the required config changes for python safety gear udf:

   In `[WORKDIR]/IEdgeInsights/VideoIngestion/config.json` or `[WORKDIR]/IEdgeInsights/VideoAnalytics/config.json` make the below changes to use safety_gear udf

   ```javascript
   ----snip----
     "udfs": [
         {
            "device": "CPU",
            "model_bin": "common/video/udfs/python/safety_gear/ref/frozen_inference_graph.bin",
            "model_xml": "common/video/udfs/python//safety_gear/ref/frozen_inference_graph.xml",
            "name": "safety_gear.safety_classifier",
            "type": "python"

        }
     ]
   ```

   **For native udfs:**

   Refer the below example to make the required config changes for native safety gear udf:

   In `[WORKDIR]/IEdgeInsights/VideoIngestion/config.json` or `[WORKDIR]/IEdgeInsights/VideoAnalytics/config.json` make the below changes to use safety_gear udf

   ```javascript
   ----snip----
     "udfs": [
         {
            "device": "CPU",
            "model_bin": "common/video/udfs/native/safety_gear_demo/ref/frozen_inference_graph.bin",
            "model_xml": "common/video/udfs/native/safety_gear_demo/ref/frozen_inference_graph.xml",
            "name": "safety_gear_demo",
            "type": "native"

        }
     ]
   ```
   Note: The IR model files must be copied along with udfs

4. For native udfs one needs to build it by modifying the `[WORKDIR]/IEdgeInsights/common/video/udfs/native/CMakeLists.txt` file to add the subdirectory of the udf for build.

   Refer the below example to modify the `[WORKDIR]/IEdgeInsights/common/video/udfs/native/CMakeLists.txt` to build the native safety_gear_demo

   ```text
   ----snip----
   add_subdirectory(safety_gear_demo/)
   ----snip----
   ```

   Note: For custom udfs a CMakeLists.txt file needs to be created and must reside in the udf directory which is added as a subdirectory to `[WORKDIR]/IEdgeInsights/common/video/udfs/native/CMakeLists.txt` file.

5. Run builder and provision by refering `[WORKDIR]/IEdgeInsights/README.md`

6. Build and run VideoIngestion & VideoAnalytics services.
