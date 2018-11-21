"""
Copyright (c) 2018 Intel Corporation.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""

import cv2
import logging
import numpy as np
from influxdb import InfluxDBClient
from DataAgent.da_grpc.client.py.client_internal.client import GrpcInternalClient
from ImageStore.py.imagestore import ImageStore

measurement_name = 'stream1'

logging.basicConfig(level=logging.INFO)
log = logging.getLogger("read_frames")


def retrieve_data_from_influx(measurement, tag):
    client = GrpcInternalClient()
    config = client.GetConfigInt("InfluxDBCfg")
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
                try:
                    frame = img_store.read(img_handles[idx])
                except Exception:
                    log.error('Frame read failed')
                if frame is not None:
                    # Convert the buffer into np array.
                    Frame = np.frombuffer(frame, dtype=np.uint8)
                    # Reshape the array.
                    img_height = elem['Height']
                    img_width = elem['Width']
                    img_channels = elem['Channels']
                    reshape_frame = np.reshape(Frame, (img_height, img_width,
                                                       img_channels))
                    # cv2.imwrite(str(frame_num)+".jpg", reshape_frame)
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
