/**
 * @file
 *
 * This file implements qcc::Stream.
 */

/******************************************************************************
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

#include <qcc/String.h>
#include <qcc/Stream.h>

#include <Status.h>

#define QCC_MODULE "STREAM"

using namespace std;
using namespace qcc;

Source Source::nullSource;

QStatus Source::GetLine(qcc::String& outStr, uint32_t timeout)
{
    QStatus status;
    uint8_t c;
    size_t actual;
    bool hasBytes = false;

    while (true) {
        status = PullBytes(&c, 1, actual, timeout);
        if (ER_OK != status) {
            break;
        }
        hasBytes = true;
        if ('\r' == c) {
            continue;
        } else if ('\n' == c) {
            break;
        } else {
            outStr.push_back(c);
        }
    }
    return ((status == ER_NONE) && hasBytes) ? ER_OK : status;
}
