**Contents**

- [Using GVA elements in VideoIngestion](#using-gva-elements-in-videoingestion)

### Using GVA elements in VideoIngestion

* For creating a GVA based ingestion custom udf container, refer [custom-udfs-gva](https://github.com/open-edge-insights/video-custom-udfs/blob/master/README.md).

* Refer [GVASafetyGearIngestion](https://github.com/open-edge-insights/video-custom-udfs/blob/master/GVASafetyGearIngestion/README.md) for running the worker safety GVA sample.

* In case one needs to run the GVA pipeline in VideoIngestion container, follow the below steps:

  1. Copy the IR model files to `[WORKDIR]/IEdgeInsights/VideoIngestion/models` directory.

  2. Refer [docs/gva_doc.md](../docs/gva_doc.md) for the GVA configuration with the supported camera.

  3. Provision, Build and Run EII by refering [main-README.md](https://github.com/open-edge-insights/eii-core/blob/master/README.md).
