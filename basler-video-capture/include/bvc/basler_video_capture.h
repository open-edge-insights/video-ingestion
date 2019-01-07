/**
 * @file
 * @brief Basler Video Capture object
 * @author Kevin Midkiff (kevin.midkiff@intel.com)
 */

#ifndef _BASLER_VIDEO_CAPTURE_H_
#define _BASLER_VIDEO_CAPTURE_H_

#include <string>
#include <exception>
#include <pylon/PylonIncludes.h>
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <boost/python/exception_translator.hpp>

using namespace boost::python;

namespace bvc {

class BaslerVideoCaptureError : public std::exception {
private:
    std::string m_message;

public:
    /**
     * Constructor.
     *
     * @param message - Exception message
     */
    BaslerVideoCaptureError(std::string message);

    /**
     * Override from std::exception.
     *
     * @return Error message
     */
    const char* what() const throw();

    /**
     * Destructor.
     */
    ~BaslerVideoCaptureError() throw();

    /**
     * Get the error message associated with the exception.
     *
     * @return Error message
     */
    std::string get_message();
};

/**
 * @c BaslerVideoCapture - Abstracts connection to a Basler camera using the
 * Pylon 5 APIs. This object tries to enumerate the given number of cameras.
 * Once the cameras are enumerated it subscribes to the events for when the
 * camera has a new frame which it then packeges into an @c cv::Mat and gives
 * to the registered callback.
 */
class BaslerVideoCapture {
private:
    // Array of attached cameras
    Pylon::CInstantCamera m_cap;

    // Internal frame converter for helping convert into a cv::Mat object
    Pylon::CImageFormatConverter m_format_converter;

    // Internal result pointers for frames read from the camera
    Pylon::CGrabResultPtr m_ptr_grab_res;
    Pylon::CPylonImage m_pylon_image;

public:
    /**
     * Constructor.
     *
     * @param serial_number - Serial number of the camera to connect to
     * @throws @c BaslerVideoCaptureError if there are no available cameras on
     *  the network, if the specified camera does not exist, or if an error
     *  occurs with Pylon connecting to the device.
     */
    BaslerVideoCapture(std::string serial_number);

    /**
     * Read the current frame from the video capture.
     *
     * @return Python NumPy array
     */
    boost::python::tuple read();

    /**
     * Get the camera's current exposure time.
     *
     * @return int64_t
     * @throws @c BaslerVideoCaptureError if the camera does not have an 
     *  exposure property.
     */
    int64_t get_exposure();

    /**
     * Set the exposure time in microseconds for the capture device.
     *
     * @param us_exposure_time - Microsecond exposure time
     * @throws @c BaslerVideoCaptureError if the camera does not have an 
     *  exposure property, or if the property is not writable.
     */
    void set_exposure(int64_t us_exposure_time);


    void set_inter_packet_delay(int64_t packet_delay);

    /**
     * Get the camera's current gain value.
     *
     * @return int64_t
     * @throws @c BaslerVideoCaptureError if the camera does not have a 
     *  gain property.
     */
    int64_t get_gain();

    /**
     * Set the camera's gain.
     *
     * @param gain - New gain value
     * @throws @c BaslerVideoCaptureError if the camera does not have a gain
     *  property, or if the property is not writable.
     */
    void set_gain(int64_t gain);

    /**
     * Release the connection to the camera. 
     *
     * \note This currently does nothing, only there to support the interface.
     */
    void release();
};

/**
 * Enumerate available devices to connect to.
 */
boost::python::list enumerate_devices();

/**
 * Initialize the Pylon library.
 */
void initialize();

/**
 * Global needed for BaslerVideoCaptureError translation into a Python 
 * exception. This should never be used outside of the translate_bvc_error
 * function.
 */
PyObject* g_bvc_error_type = NULL;

/**
 * Method to translate the BaslerVideoCaptureError type into the assocuated
 * Python exception type. This method should never be used out side of the
 * definition of the Boost.Python mapping.
 *
 * @param ex - Exception to translate
 */
void translate_bvc_error(const BaslerVideoCaptureError &ex);

/**
 * Creates an exception class in the Python module. Used for fully mapping
 * a C++ exception into a Python exception. This should never be used outside
 * of the Boost.Python mapping.
 */
PyObject* create_exc_class(
        const char* name, PyObject* base_type_obj=PyExc_Exception);

} // bvc


BOOST_PYTHON_MODULE(basler_video_capture) {
    // Initialize the Boost.Python NumPy wrapper
    boost::python::numpy::initialize();
    
    // Define the BaslerVideoError exception and its C++ to Python translation
    class_<bvc::BaslerVideoCaptureError> bvc_error(
            "BaslerVideoCaptureError", init<std::string>());
    bvc_error.add_property("message", &bvc::BaslerVideoCaptureError::get_message);
    bvc::g_bvc_error_type = bvc::create_exc_class("BaslerVideoCaptureError");
    register_exception_translator<bvc::BaslerVideoCaptureError>(
            &bvc::translate_bvc_error);

    // Define the BaslerVideoCapture Python object
    class_<bvc::BaslerVideoCapture, boost::noncopyable>(
            "BaslerVideoCapture", init<std::string>())
        .def("read", &bvc::BaslerVideoCapture::read)
        .def("set_exposure", &bvc::BaslerVideoCapture::set_exposure)
        .def("set_inter_packet_delay", &bvc::BaslerVideoCapture::set_inter_packet_delay)
        .def("get_exposure", &bvc::BaslerVideoCapture::get_exposure)
        .def("set_gain", &bvc::BaslerVideoCapture::set_gain)
        .def("get_gain", &bvc::BaslerVideoCapture::get_gain)
        .def("release", &bvc::BaslerVideoCapture::release);

    // Define the enumerate_devices method
    def("enumerate_devices", bvc::enumerate_devices);

    // Define the library initialization method
    def("initialize", bvc::initialize);
}

#endif // _BASLER_VIDEO_CAPTURE_H_

