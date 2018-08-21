"""
Copyright (c) 2018 Intel Corporation.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""

import paho.mqtt.client as mqtt
import time
import json
import config


def main():

    client = mqtt.Client()
    host = config.value.host
    print("HOST: ", host)
    port = config.value.port
    print("PORT: ", port)
    client.connect(host, port=port)

    topic = config.value.cam_on_topic
    cam_on = {"camera_on": config.value.camera_on}
    cam_on_s = json.dumps(cam_on)
    print("TOPIC: ", topic, "CAM_ON: ", cam_on_s)
    client.publish(topic, payload=cam_on_s, qos=1)

    delay = config.value.cam_on_time
    print("DELAY: ", delay)
    time.sleep(delay)

    cam_off = {"camera_on": config.value.camera_off}
    cam_off_s = json.dumps(cam_off)
    print("TOPIC: ", topic, "CAM_OFF: ", cam_off_s)
    client.publish(topic, payload=cam_off_s, qos=1)

if __name__ == '__main__':
    main()
