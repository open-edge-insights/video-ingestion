#### `Generic Plugin`
Generic Plugin is a gstreamer generic source plugin that communicates and streams from a GenICam based camera which provides a GenTL producer. In order to use
the generic plugin with VI one must install the respective GenICam camera SDK and make sure the compatible GenTL producer for the camera is installed.

**Refer [src-gst-gencamsrc/README](../src-gst-gencamsrc/README) for Element Properties of Generic Plugin**.

**Refer [src-gst-gencamsrc/README.md](../src-gst-gencamsrc/README.md) for more information on Generic Plugin**.
 ----

**For working with Genicam USB3 Vision camera please install the respective camera SDK by referring the below section**

#### `Adding new GigE camera support to VideoIngestion`
In order to use the generic plugin with newer Genicam camera SDK follow the below steps:

1. Install the respective GenICam camera SDK by adding the camera SDK installation steps in [VI-Dockerfile](../Dockerfile). The camera SDK will be installed during docker build and one can install multiple camera SDKs in [VI-Dockerfile](../Dockerfile). Refer Dokerfile reference documentation (https://docs.docker.com/engine/reference/builder/) for Dockerfile Instructions. Typically one would need to use `RUN <command>` instrcution to execute shell commands for installing newer camera sdk. In case the camera SDK cannot be downloaded in a non-interactive manner then one would need to place the camera SDK under `[WORKDIR]/IEdgeInsights/VideoIngestion/` directory, use the `COPY` instruction in [VI-Dockerfile](../Dockerfile) to use the camera SDK in the build context and then use the `RUN` instruction for installing the camera SDK during docker build.

2. After making sure the compatible GenTL producer is successfully installed one must add a case statement in [gentl_producer_env.sh](../gentl_producer_env.sh) scipt to export the GenTL producer path to `GENICAM_GENTL64_PATH` env variable. In order to verify the GenTL producer path one can search for the `.cti` file under the installation path. Typically GenTL provider is characterized by a file with `.cti` extension. Path to cti library containing folder must be exported to env variable named `GENICAM_GENTL64_PATH` (`GENICAM_GENTL32_PATH` for 32 bit providers).

3. Set `GENICAM` env variable in [docker-compose.yml](../docker-compose.yml) to execute the corresponding case statement in [gentl_producer_env.sh](../gentl_producer_env.sh) and export the GenTL producer path accordingly. GenTL producer path will be exported during docker runtime. Hence one can choose to add install multiple Genicam camera SDK during docker build and then switch between GenTL providers during docker runtime by modifying the `GENICAM` env variable in [docker-compose.yml](../docker-compose.yml).

**Refer the below snippet example to select `Basler` camera and export its GenTL producer path in [docker-compose.yml](../docker-compose.yml) assuming the corresponding Basler Camera SDK and GenTL producer is installed.**
```bash
 ia_video_ingestion:
 ...
   environment:
   ...
   # Setting GENICAM value to the respective camera which needs to be used
   GENICAM: "Basler"
```

**Note:** Please make sure to run [builder.py](../../build/builder.py) script after making changes in [docker-compose.yml](../docker-compose.yml)