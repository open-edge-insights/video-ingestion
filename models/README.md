# Contents

- [Contents](#contents)
  - [Using GVA elements in VideoIngestion](#using-gva-elements-in-videoingestion)

## Using GVA elements in VideoIngestion

For creating a GVA-based ingestion custom udf container, refer to the [custom-udfs-gva Readme](https://github.com/open-edge-insights/video-custom-udfs/blob/master/README.md).

For running the worker safety GVA sample, refer to the [GVASafetyGearIngestion Readme](https://github.com/open-edge-insights/video-custom-udfs/blob/master/GVASafetyGearIngestion/README.md).

To run the GVA pipeline in the VideoIngestion container, perform the following:

 1. Copy the IR model files to the `[WORKDIR]/IEdgeInsights/VideoIngestion/models` directory.
 2. Refer the [docs/gva_doc.md](../docs/gva_doc.md) for the GVA configuration with the supported camera.
 3. Provision, build and run OEI. Refer to the [Readme](https://github.com/open-edge-insights/eii-core/blob/master/README.md).
