# Contents

- [Contents](#contents)
  - [Software Trigger Utility for VideoIngestion](#software-trigger-utility-for-videoingestion)
  - [Generic server in VideoIngestion](#generic-server-in-videoingestion)

## Software Trigger Utility for VideoIngestion

The video ingestion process is autonomous. When the VideoIngestion microservice starts, the ingestion process starts automatically using the video source such as a video file or a camera. You cannot control the ingestion without stopping the ingestion microservice. The Software Trigger Utility allows you to start and stop video ingestion using the software triggers sent by the client application. You can control video ingestion from all the supported ingestors and camera configurations using the Software Trigger Utility.

>**Note:** In this document, you will find labels of 'Edge Insights for Industrial (EII)' for filenames, paths, code snippets, and so on. Consider the references of EII as Open Edge Insights (OEI). This is due to the product name change of EII as OEI.

## Generic server in VideoIngestion

The VI generic server responds to the client with the return values specific to a command. Based on a command, there is a total flexibility in sending the number and type of arguments back to a client.

The example JSON format for the incoming payload from a client to server to initialize software trigger is as follows:

  ```javascript
  {
  "init_state":"running"
  }
  ```

The VideoIngestion microservice exposes the functionality of software trigger in the following ways:

- The VideoIngestion microservice can accept software trigger to "START_INGESTION"/ "STOP_INGESTION"/ "SNAPSHOT" from any client utility which uses the Message Bus over the server-client model.
- The software trigger functionality of VI is demonstrated using an sample baremetal utility called "SW_Trigger_utility", which is shipped with the VideoIngestion code in tools repo, the details of the usage of this utility is mentioned in [../../tools/SWTriggerUtility/README.md](https://github.com/open-edge-insights/eii-tools/blob/master/SWTriggerUtility/README.md).

>**Note**: When the `init_state` value is `running` then ingestor is started without any sw trigger from the client. In order to control the ingestor using the sw trigger utility change the value to `stopped`. To refer the available option to generate the sw trigger refer [../tools/SWTriggerUtility/README.md](https://github.com/open-edge-insights/eii-tools/blob/master/SWTriggerUtility/README.md)

The VI Gerenic Server supports the following commands:

- START_INGESTION — Use this command to start the ingestor. The payload format is as follows:

    ```javascript
      {
        "command" : "START_INGESTION"
      }
    ```

- STOP_INGESTION — Use this command to stop the ingestor. The payload format is as follows:

    ```javascript
      {
        "command" : "STOP_INGESTION"
      }
    ```

- SNAPSHOT — Use this command to get the frame snapshot and feed one frame into the video data pipeline. The payload format is as follows:

    ```javascript
      {
        "command" : "SNAPSHOT"
      }
    ```

  >Note
  >
  > Enable the software trigger mode to use the `SNAPSHOT` functionality. Ensure that the ingestion is stopped before getting the frame snapshot capture.
