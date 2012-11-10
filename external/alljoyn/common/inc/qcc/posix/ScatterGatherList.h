/**
 * @file
 *
 * Define inline ScatterGatherList functions for GCC.
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
#ifndef _OS_QCC_SCATTERGATHER_H
#define _OS_QCC_SCATTERGATHER_H

#include <qcc/platform.h>

namespace qcc {

inline void ScatterGatherList::AddBuffer(void* buffer, size_t length)
{
    maxDataSize += length;
    QCC_DbgTrace(("ScatterGatherList::AddBuffer(buffer, length = %u) [maxDataSize = %u]",
                  length, maxDataSize));

    if (Size() == 0) {
        IOVec iov;
        iov.buf = buffer;
        iov.len = length;
        sg.push_back(iov);
    } else {
        if (!sg.empty()) {
            IOVec& last = sg.back();

            if (reinterpret_cast<uint8_t*>(last.buf) + last.len == buffer) {
                last.len += length;
            } else {
                IOVec iov;
                iov.buf = buffer;
                iov.len = length;
                sg.push_back(iov);
            }
        } else {
            IOVec iov;
            iov.buf = buffer;
            iov.len = length;
            sg.push_back(iov);
        }
    }
}

}

#endif
