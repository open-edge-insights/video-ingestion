#!/usr/bin/env python3
import cv2
import logging
import numpy as np
from influxdb import InfluxDBClient
from DataAgent.da_grpc.client.client import GrpcClient
from ImageStore.py.imagestore import ImageStore

measurement_name = 'basler_cam'

logging.basicConfig(level=logging.INFO)
log = logging.getLogger("read_frames")


def retrieve_data_from_influx(measurement, tag):
    config = GrpcClient.GetConfigInt("InfluxDBCfg")
    influx_c = InfluxDBClient(config["Host"],
                              config["Port"],
                              config["UserName"],
                              config["Password"],
                              config["DBName"])
    result = influx_c.query('select ' + tag + ' from ' + measurement + ' ;')
    # print("Result: {0}".format(result))
    return result


def retrieve_frames(data_points):
    # Initialize Image Store.
    img_store = ImageStore()
    # Loop over the data points to retrive the frames using frame handles and
    # write the frames.
    frame_num = 0
    for elem in data_points:
        if elem['ImageStore'] == '1':
            img_handles = elem['ImgHandle'].split(',')
            for idx in range(len(img_handles)):
                print(img_handles[idx])
                ret, frame = img_store.read(img_handles[idx])
                if ret is True:
                    # Convert the buffer into np array.
                    Frame = np.frombuffer(frame, dtype=np.uint8)
                    # Reshape the array.
                    img_height = elem['Height']
                    img_width = elem['Width']
                    img_channels = elem['Channels']
                    reshape_frame = np.reshape(Frame, (img_height, img_width,
                                                       img_channels))
                    cv2.imshow(str(frame_num), reshape_frame)
                    cv2.waitKey(0)
                    frame_num += 1
                else:
                    log.error("Frame read unsuccessfull.")
    cv2.destroyAllWindows()


if __name__ == '__main__':
    # Retrieve data from database.
    data_points = retrieve_data_from_influx(measurement_name, '*')

    # Get the data points in a list.
    data_pts_list = list(data_points.get_points())

    # Retrive the frames from Imagestore and write a video file.
    retrieve_frames(data_pts_list)
