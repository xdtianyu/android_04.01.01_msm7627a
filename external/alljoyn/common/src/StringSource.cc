/**
 * @file
 *
 * Source wrapper for std::string
 */

/******************************************************************************
 *
 *
 * Copyright 2009-2011, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/

#include <qcc/platform.h>

#include <cstring>

#include <qcc/Stream.h>
#include <qcc/StringSource.h>

#include <Status.h>

using namespace std;
using namespace qcc;

#define QCC_MODULE "STREAM"

QStatus StringSource::PullBytes(void* buf, size_t reqBytes, size_t& actualBytes, uint32_t timeout)
{
    QStatus status = ER_OK;
    actualBytes = (std::min)(reqBytes, str.size() - outIdx);
    if (0 < actualBytes) {
        memcpy(buf, str.data() + outIdx, actualBytes);
        outIdx += actualBytes;
    } else if (outIdx == str.size()) {
        status = ER_NONE;
    }
    return status;
}


