// Copyright (c) 2019 Intel Corporation.  //
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM,OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

/**
 * @file
 * @brief VideoIngestion main program
 */


#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <eis/utils/logger.h>
#include <eis/utils/json_config.h>
#include <eis/msgbus/msgbus.h>
#if 0
#include "eis/udf/udf_manager.h"
#endif
#include "eis/vi/video_ingestion.h"

#if 0
using namespace eis::udf;
#endif
using namespace eis::utils;
using namespace eis::msgbus;
using namespace eis::vi;

void usage(const char* name) {
    printf("usage: %s [-h|--help]\n", name);
}

void cleanup(VideoIngestion* vi) {
    if(vi) {
        vi->stop();
        delete vi;
    }
}

int main(int argc, char** argv) {
    set_log_level(LOG_LVL_INFO);
    VideoIngestion* vi = NULL;
    try {
        if(argc >= 2) {
            if(strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
                usage(argv[0]);
            }
        }
        vi = new VideoIngestion();
        vi->start();
    } catch(const std::exception& ex) {
        LOG_ERROR("Exception '%s' occurred", ex.what());
        cleanup(vi);
        return -1;
    }
    return 0;
}
