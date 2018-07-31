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
