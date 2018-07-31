from collections import namedtuple

defaults = {
    "host" : "localhost",
    "port" : 1883,
    "cam_on_topic" : "proj_1/gw_1/camera0",
    "camera_on" : 1,
    "camera_off" : 0,
    "cam_on_time" : 10
}

value = namedtuple('Config', defaults.keys())(**defaults)