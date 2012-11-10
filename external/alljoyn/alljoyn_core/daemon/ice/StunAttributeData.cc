/**
 * @file
 *
 * This file implements the STUN Attribute Data class
 */

/******************************************************************************
 * Copyright 2009,2012 Qualcomm Innovation Center, Inc.
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
#include <StunAttributeData.h>
#include "Status.h"

using namespace qcc;

#define QCC_MODULE "STUN_ATTRIBUTE"


QStatus StunAttributeData::Parse(const uint8_t*& buf, size_t& bufSize)
{
    QStatus status;
    QCC_DbgTrace(("StunAttributeData::Parse(*buf, bufSize = %u)", bufSize));
    QCC_DbgRemoteData(buf, bufSize);

    data.AddBuffer(&buf[0], bufSize);
    data.SetDataSize(bufSize);

    buf += bufSize;
    bufSize = 0;

    status = StunAttribute::Parse(buf, bufSize);

    return status;
}


QStatus StunAttributeData::RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const
{
    QStatus status;
    size_t dataLen;

    status = StunAttribute::RenderBinary(buf, bufSize, sg);
    if (status != ER_OK) {
        goto exit;
    }

    dataLen = data.DataSize();
    sg.AddSG(data);
    sg.IncDataSize(dataLen);

    // Data does not end on a 32-bit boundary so add some 0 padding.
    if ((dataLen & 0x3) != 0) {
        // pad with empty bytes
        if ((dataLen & 0x3) < 3) {
            WriteHostToNet(buf, bufSize, static_cast<uint16_t>(0), sg);
        }
        if ((dataLen & 0x1) == 0x1) {
            WriteHostToNet(buf, bufSize, static_cast<uint8_t>(0), sg);
        }
    }

exit:
    return status;
}
