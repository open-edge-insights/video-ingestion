### Using GVA elements in VideoIngestion

* For creating a GVA based ingestion custom udf container, refer [custom-udfs-gva](../../CustomUdfs/README.md).

* Refer [GVASafetyGearIngestion](../../CustomUdfs/GVASafetyGearIngestion/README.md) for running the worker safety GVA sample.

* In case one needs to run the GVA pipeline in VideoIngestion container, follow the below steps:

  1. Copy the IR model files to `[WORKDIR]/IEdgeInsights/VideoIngestion/models` directory.

  2. Refer [docs/gva_doc.md](../docs/gva_doc.md) for the GVA configuration with the supported camera.

  3. Provision, Build and Run EII by refering [main-README.md](../../README.md).
