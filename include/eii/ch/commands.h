// Copyright (c) 2020 Intel Corporation.
//
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
 * @brief Client Commands interface
 */

#ifndef _EIS_COMMAND_H
#define _EIS_COMMAND_H

namespace eis {
    enum AckToClient {
        REQ_HONORED=0,
        REQ_NOT_HONORED=1,
        REQ_ALREADY_RUNNING=2,
        REQ_ALREADY_STOPPED=3,
        REQ_COMMAND_NOT_REGISTERED
    };

    enum CommandsList {
        START_INGESTION,
        STOP_INGESTION,
        SNAPSHOT,
        COMMAND_INVALID
        // MORE COMMANDS TO BE ADDED BASED ON THE NEED
    };

} // eis namespace end
#endif
