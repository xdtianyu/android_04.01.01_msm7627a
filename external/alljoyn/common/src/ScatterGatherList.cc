/**
 * @file
 *
 * Define non-platform specific functions for the ScatterGatherList class.
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

#include <algorithm>
#include <list>
#include <string.h>

#include <qcc/ScatterGatherList.h>
#include <qcc/SocketTypes.h>

#define QCC_MODULE "NETWORK"

using namespace std;
using namespace qcc;

size_t ScatterGatherList::CopyDataFrom(const_iterator begin, const_iterator end, size_t limit)
{
    if (sg.empty() || (begin == end)) {
        return 0;
    }
    iterator dest(sg.begin());
    const_iterator src(begin);
    uint8_t* srcBuf = reinterpret_cast<uint8_t*>(src->buf);
    uint8_t* destBuf = reinterpret_cast<uint8_t*>(dest->buf);
    size_t srcLen = src->len;
    size_t destLen = dest->len;
    size_t copyCnt = 0;
    size_t copyLimit = min(maxDataSize, limit);
    QCC_DbgTrace(("ScatterGatherList::CopyDataFrom(begin, end, limit = %u)", limit));

    QCC_DbgPrintf(("srcLen = %u  destLen = %u  copyLimit = %u", srcLen, destLen, copyLimit));

    while (copyLimit > 0 && dest != sg.end() && src != end) {
        size_t copyLen = min(copyLimit, min(destLen, srcLen));

        QCC_DbgPrintf(("srcLen = %u  destLen = %u  copyLimit = %u  copyLen = %u", srcLen, destLen, copyLimit, copyLen));

        memmove(destBuf, srcBuf, copyLen);
        copyCnt += copyLen;
        copyLimit -= copyLen;

        if (copyLen == destLen) {
            ++dest;
            destBuf = reinterpret_cast<uint8_t*>(dest->buf);
            destLen = dest->len;
        } else {
            destBuf += copyLen;
            destLen -= copyLen;
        }

        if (copyLen == srcLen) {
            ++src;
            srcBuf = reinterpret_cast<uint8_t*>(src->buf);
            srcLen = src->len;
        } else {
            srcBuf += copyLen;
            srcLen -= copyLen;
        }
    }

    dataSize = copyCnt;

    return copyCnt;
}
