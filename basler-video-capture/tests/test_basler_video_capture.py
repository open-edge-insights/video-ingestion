"""Unit tests for the basler_video_capture library.
"""
import os
import unittest
import basler_video_capture as bvc


class BaslerVideoCaptureTest(unittest.TestCase):
    """Unit tests for the BaslerVideoCapture object.
    """
    def setUp(self):
        # Add emulated devices to the environment
        os.environ['PYLON_CAMEMU'] = '1'
        # Initialize the bvc library
        bvc.initialize()

    def test_capture(self):
        """Test capturing a frame from a BaslerVideoCapture device.
        """
        cap = self.get_capture()
        ret, frame = cap.read()
        self.assertEqual(ret, True, "Failed to get frame")
        self.assertNotEqual(
                ret, None, "Problem with code, ret = True, but frame is None")
    
    def test_set_exposure(self):
        """Test setting the exposure time.
        """
        cap = self.get_capture()
        cap.set_exposure(7000)
        self.assertEqual(cap.get_exposure(), 7000)

    def test_no_camera(self):
        """Test BaslerVideoCaptureError.
        """
        self.assertRaises(
                bvc.BaslerVideoCaptureError, 
                bvc.BaslerVideoCapture,
                'asdf') 

    def test_set_gain(self):
        """Test setting the gain property.
        """
        cap = self.get_capture()
        cap.set_gain(200)
        self.assertEqual(cap.get_gain(), 200)

    def get_capture(self):
        """Helper function to get the video capture device with the serial 
        number: 0815-000. Note that this method will cause the test to fail if
        the camera does not exist in the enumerated devices, and if there are
        more devices than 1.
        """
        devices = bvc.enumerate_devices()
        self.assertEqual(len(devices), 1, 
                'Incorrect number of enumerate devices')
        self.assertEqual(
                devices[0], '0815-0000', 'Incorrect camera serial number')
        return bvc.BaslerVideoCapture(devices[0])

