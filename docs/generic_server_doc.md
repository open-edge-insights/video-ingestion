**VideoIngestion micro-service exposes the functionality of software trigger in the following ways:**

  1) It can accept software trigger to "START_INGESTION"/ "STOP_INGESTION"/ "SNAPSHOT" from any client utility which uses the EIS messagebus over server-client model.

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


