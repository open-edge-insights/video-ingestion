import paho.mqtt.client as mqtt
import json


if __name__ == "__main__":
    data = {"camera_on": 1}
    datas = json.dumps(data)
    print(datas)
    mqtt_client = mqtt.Client()
    mqtt_client.connect("localhost", port=1883)
    mqtt_client.publish('proj_1/gw_1/camera0', payload=datas, qos=1)
