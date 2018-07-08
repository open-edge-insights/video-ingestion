/**
 * @brief Simple example main to test compilation.
 * @author Kevin Midkiff (kevin.midkiff@intel.com)
 */

#include <pylon/PylonIncludes.h>

int main(int /*argc*/, char** /*argv*/) {
    // Initializing Pylon SDK
    Pylon::PylonAutoInitTerm autoInitTerm;

    // Create an instant camera object with the camera device found first.
    Pylon::CInstantCamera camera(Pylon::CTlFactory::GetInstance().CreateFirstDevice());

    printf("-- Opening camera\n");
    camera.Open();

    printf("-- Starting fetching\n");
    camera.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly);

    // This smart pointer will receive the grab result data.
    Pylon::CGrabResultPtr ptr_grab_res;

    printf("-- Grabbing frames...\n");
    while(camera.IsGrabbing()) {
        camera.RetrieveResult(5000, ptr_grab_res, Pylon::TimeoutHandling_ThrowException);
    }

    return 0;
}
