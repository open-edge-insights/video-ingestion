### USB (V4l2) Camera

**NOTE**:

* In case you want to enable resizing with USB camera use the
  `videoscale` element and specify the `height` and `width`  parameter in the gstreamer pipeline.

    **Example pipeline to enable resizing with USB camera:**

    `"pipeline":"v4l2src ! videoscale ! video/x-raw,format=YUY2,height=600,width=600 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

* In case, multiple USB cameras are connected specify the
  camera using the `device` property in the configuration file.

    **Example pipeline to use the device property:**

    `"pipeline": "v4l2src device=/dev/video0 ! video/x-raw,format=YUY2 ! videoconvert ! video/x-raw,format=BGR ! appsink"`

