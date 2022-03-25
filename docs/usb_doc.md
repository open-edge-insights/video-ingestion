**Contents**

- [USB (V4l2) Camera](#usb-v4l2-camera)
- [Set Securiry Context to Enable USB Camera in helm environment](#updating-security-context-of-videoingestion-helm-charts-for-enabling-k8s-environment-to-accessdetect-USB-camera)

### USB (V4l2) Camera

**NOTE**:

- In case you want to enable resizing with USB camera use the
  `videoscale` element and specify the `height` and `width`  parameter in the gstreamer pipeline.

    **Example pipeline to enable resizing with USB camera:**

    `"pipeline":"v4l2src ! videoscale ! video/x-raw,format=YUY2,height=600,width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

- In case, multiple USB cameras are connected specify the
  camera using the `device` property in the configuration file.

    **Example pipeline to use the device property:**

    `"pipeline": "v4l2src device=/dev/video0 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

    **Note**: Typically a device node gets created when a USB device is connected to the system. When multiple USB cameras are connected then one needs to identify which device node is mapped to the camera and use that with the `device` property. Device nodes for the cameras usually gets created in sequence of video0, video1, video2 etc.

### Updating Security Context of VideoIngestion Helm Charts for enabling K8s environment to access/detect USB Camera

Please follow the steps to update helm charts for enabling K8s environment to access/detect USB Camera

* Open `EII_HOME_DIR/IEdgeInsights/VideoIngestion/helm/templates/video-ingestion.yaml` file

* Update below security context snippet
  ```yml
        securityContext:
          privileged: true
  ```
  in the yaml file as:
  ```yaml
    ...
    ...
    ...
        imagePullPolicy: {{ $global.Values.imagePullPolicy }}
        securityContext:
          privileged: true
        volumeMounts:
        - name: dev
          mountPath: /dev
    ...
  ```
  * Re-run `builder.py` to apply these changes to your deployment helm charts.
