#### `Camera independent Software Trigger way of video ingestion`

  * Software triggering way of video ingestion is a solution which enables to control video ingestion from all the supported ingestors and camera configurations.

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

**VideoIngestion micro-service exposes the functionality of software trigger in the following ways:**

  1) It can accept software trigger to "START_INGESTION"/ "STOP_INGESTION"/ "SNAPSHOT" from any client utility which uses the EII messagebus over server-client model.

  2) The software trigger functionality of VI is demonstrated using an sample baremetal utility called "SW_Trigger_utility", which is shipped with the VideoIngestion code in tools repo, the details of the usage of this utility is mentioned in [../../tools/SWTriggerUtility/README.md](../../tools/SWTriggerUtility/README.md).

>**Note**: When the `init_state` value is `running` then ingestor is started without any sw trigger from the client. In order to control the ingestor using the sw trigger utility change the value to `stopped`. To refer the available option to generate the sw trigger refer [../tools/SWTriggerUtility/README.md](../tools/SWTriggerUtility/README.md)

**The supported commands in the VI Gerenic Server are:**

1. START_INGESTION: to start the ingestor

    Payload format:

    ```javascript
      {
        "command" : "START_INGESTION"
      }
    ```

2. STOP_INGESTION: to stop the ingestor

    Payload format:

    ```javascript
      {
        "command" : "STOP_INGESTION"
      }
    ```

3. SNAPSHOT: to get the frame snapshot and feed in one frame into the video data pipeline

    Payload format:
    ```javascript
      {
        "command" : "SNAPSHOT"
      }
    ```

  >**Note**: In order to use `SNAPSHOT` functionality one needs to enable the sw trigger mode and make sure ingestion should be stopped before getting the frame snapshot capture.


