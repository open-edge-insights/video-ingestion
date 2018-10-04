/**
 * @brief Implementation of the @c BaslerVideoCapture class
 * @author Kevin Midkiff (kevin.midkiff@intel.com)
 */

#include <sstream>
#include "bvc/basler_video_capture.h"

namespace bvc {

namespace p = boost::python;
namespace np = boost::python::numpy;

void destroy_manager_cobj(PyObject* obj) {
    uint8_t* data = reinterpret_cast<uint8_t*>(PyCapsule_GetPointer(obj, NULL));
    if(data != NULL) delete data;
}

BaslerVideoCaptureError::BaslerVideoCaptureError(std::string message) :
    m_message(message)
{}

BaslerVideoCaptureError::~BaslerVideoCaptureError() throw() {}

const char* BaslerVideoCaptureError::what() const throw() {
    return m_message.c_str();
}

std::string BaslerVideoCaptureError::get_message() {
    return m_message;
}

BaslerVideoCapture::BaslerVideoCapture(std::string serial_number) { 
    m_format_converter.OutputPixelFormat= Pylon::PixelType_BGR8packed;

    // Get the transport layer factory.
    Pylon::CTlFactory& factory = Pylon::CTlFactory::GetInstance();

    // Get all attached devices and exit application if no device is found.
    Pylon::DeviceInfoList_t devices;
    if(factory.EnumerateDevices(devices) == 0) {
        throw BaslerVideoCaptureError("No cameras present");
    }

    bool found = false;

    // Initialize the array of devices
    for(size_t i = 0; i < devices.size(); i++) {
        std::string sn = std::string(&*devices[i].GetSerialNumber());
        if(sn == serial_number) {
            auto device = factory.CreateDevice(devices[i]);
            if(device == NULL) {
                throw BaslerVideoCaptureError(
                        "Failed to create device for camera");
            }
            try {
                m_cap.Attach(device);
            } catch(const Pylon::GenericException &e) {
                std::ostringstream os;
                os << "Failed to attach to device: " << e.GetDescription();
                throw BaslerVideoCaptureError(os.str());
            }
            found = true;
        }
    }
    if(!found) {
        throw BaslerVideoCaptureError("Cannot find the camera");
    }
    // Opening the camera
    try {
	m_cap.Open();
    } catch(const std::exception &ex) {
        std::cerr << "Error while opening camera:" << ex.what() << std::endl;
        throw;
    }
    // Start grabbing frames
    try {
	m_cap.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly);
    } catch(const std::exception &ex) {
	std::cerr << "Error while grabbing frames:" << ex.what() << std::endl;
	throw;
    }
}

boost::python::tuple BaslerVideoCapture::read() {
    try {
        // Retrieve the camera
        m_cap.RetrieveResult(
                5000, m_ptr_grab_res, Pylon::TimeoutHandling_ThrowException);

        if(!m_ptr_grab_res->GrabSucceeded()) {
            return make_tuple(false, p::object());
        }

        // Convert the grabbed result to a Pylon::PylonImage object
        m_format_converter.Convert(m_pylon_image, m_ptr_grab_res);

        size_t rows = m_ptr_grab_res->GetHeight(); 
        size_t cols = m_ptr_grab_res->GetWidth(); 
        np::dtype dtype = np::dtype::get_builtin<uint8_t>();
        
        // Copy Pylon frame data into a buffer
        size_t size = rows * cols * 3; 
        uint8_t* data = new uint8_t[size];
        uint8_t* raw_data = (uint8_t*) m_pylon_image.GetBuffer();

        for(size_t i = 0; i < size; i++) {
            data[i] = raw_data[i];
        } 

        // Releasing Pylon data
        m_pylon_image.Release();
        m_ptr_grab_res.Release();

        // Create destructor handle for underlying NumPy array data
        p::handle<> handle(::PyCapsule_New(
                    (void*) data, NULL, 
                    (PyCapsule_Destructor) &destroy_manager_cobj));

        // Create NumPy array from the frame data
        np::ndarray frame = np::from_data(
            data, dtype, 
            p::make_tuple(rows, cols, 3), 
            p::make_tuple(cols * 3, 3, 1), 
            p::object(handle));

        return make_tuple(true, frame);
    } catch(const Pylon::GenericException &e) {
        std::ostringstream os;
        os << "Error occurred while reading frame: " << e.GetDescription();
        throw BaslerVideoCaptureError(os.str());
    }
}

int64_t BaslerVideoCapture::get_exposure() {
    GenApi::INodeMap &control = m_cap.GetNodeMap();
    const GenApi::CIntegerPtr exposure = control.GetNode("ExposureTimeRaw");
    if(exposure == nullptr) {
        throw BaslerVideoCaptureError(
                "Capture device does not have 'ExposureTimeRaw' property");
    }
    return exposure->GetValue();
}

void BaslerVideoCapture::set_exposure(int64_t us_exposure_time) {
    GenApi::INodeMap &control = m_cap.GetNodeMap();
    const GenApi::CIntegerPtr exposure = control.GetNode("ExposureTimeRaw");
    if(exposure == nullptr) {
        throw BaslerVideoCaptureError(
                "Capture device does not have 'ExposureTimeRaw' property");
    }

    try {
        if(GenApi::IsWritable(exposure)) {
            exposure->SetValue(us_exposure_time);
        } else {
            throw BaslerVideoCaptureError(
                    "'ExposureTimeRaw' property is not writable");
        }
    } catch(const Pylon::GenericException &e) {
        std::ostringstream os;
        os << "Error setting ExposureTimeRaw property: " << e.GetDescription();
        throw BaslerVideoCaptureError(os.str());
    }
}

int64_t BaslerVideoCapture::get_gain() {
    GenApi::INodeMap &control = m_cap.GetNodeMap();
    const GenApi::CIntegerPtr gain = control.GetNode("GainRaw");
    if(gain == nullptr) {
        throw BaslerVideoCaptureError(
                "Capture device does not have 'GainRaw' property");
    }
    return gain->GetValue();
}

void BaslerVideoCapture::set_gain(int64_t gain) {
    GenApi::INodeMap &control = m_cap.GetNodeMap();
    const GenApi::CIntegerPtr gain_raw = control.GetNode("GainRaw");
    if(gain_raw == nullptr) {
        throw BaslerVideoCaptureError(
                "Capture device does not have 'GainRaw' property");
    }

    try {
        if(GenApi::IsWritable(gain_raw)) {
            gain_raw->SetValue(gain);
        } else {
            throw BaslerVideoCaptureError(
                    "'GainRaw' property is not writable");
        }
    } catch(const Pylon::GenericException &e) {
        std::ostringstream os;
        os << "Error setting GainRaw property: " << e.GetDescription();
        throw BaslerVideoCaptureError(os.str());
    }
}

void BaslerVideoCapture::release() {
    // This method is just a placeholder to support the interface
    return;
}

boost::python::list enumerate_devices() {
    p::list p_devices;

    // Get the transport layer factory.
    Pylon::CTlFactory& factory = Pylon::CTlFactory::GetInstance();

    // Get all attached devices and exit application if no device is found.
    Pylon::DeviceInfoList_t devices;
    factory.EnumerateDevices(devices);
    
    // Populate Python array with serial numbers of the enumerated cameras
    for(auto dev : devices) {
        p_devices.append(std::string(&*dev.GetSerialNumber()));
    }

    return p_devices;
}

void initialize() {
    Pylon::PylonInitialize();
}

void translate_bvc_error(const BaslerVideoCaptureError &ex) {
    assert(g_bvc_error_type != NULL);
    boost::python::object exc_inst(ex);
    boost::python::object exc_t(
            boost::python::handle<>(boost::python::borrowed(g_bvc_error_type)));
    exc_t.attr("cause") = exc_inst; // add the wrapped exception to the Python exception
    PyErr_SetString(g_bvc_error_type, ex.what());
}

PyObject* create_exc_class(const char* name, PyObject* base_type_obj) {
    std::string scope_name = extract<std::string>(scope().attr("__name__"));
    std::string str_qualified_name = scope_name + "." + name;
    char* qualified_name = const_cast<char*>(str_qualified_name.c_str());
    PyObject* type_obj = PyErr_NewException(qualified_name, base_type_obj, 0);
    if(!type_obj) throw_error_already_set();
    scope().attr(name) = handle<>(borrowed(type_obj));
    return type_obj;
}

}
