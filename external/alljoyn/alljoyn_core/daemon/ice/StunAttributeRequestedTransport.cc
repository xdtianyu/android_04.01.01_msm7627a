/**
 * @file
 *
 * This file implements the STUN Attribute Requested Transport class
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

#include <string>
#include <qcc/platform.h>
#include <qcc/StringUtil.h>
#include <qcc/String.h>
#include <StunAttributeRequestedTransport.h>
#include "Status.h"

using namespace qcc;

#define QCC_MODULE "STUN_ATTRIBUTE"


QStatus StunAttributeRequestedTransport::Parse(const uint8_t*& buf, size_t& bufSize)
{
    QStatus status;

    protocol = *buf;
    ++buf;
    --bufSize;

    // "Read" the RFFU (if included in the attribute size)
    buf += bufSize;
    bufSize = 0;

    status = StunAttribute::Parse(buf, bufSize);

    return status;
}


QStatus StunAttributeRequestedTransport::RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const
{
    QStatus status;

    status = StunAttribute::RenderBinary(buf, bufSize, sg);
    if (status != ER_OK) {
        goto exit;
    }


    WriteHostToNet(buf, bufSize, static_cast<uint8_t>(protocol), sg);

    // Pad with empty bytes.
    WriteHostToNet(buf, bufSize, static_cast<uint8_t>(0), sg);
    WriteHostToNet(buf, bufSize, static_cast<uint16_t>(0), sg);

exit:
    return status;
}


#if !defined(NDEBUG)
String StunAttributeRequestedTransport::ToString(void) const
{
    String oss;

    oss.append(StunAttribute::ToString());
    oss.append(": ");
    oss.append(U32ToString(static_cast<uint32_t>(protocol), 10));

    return oss;
}
#endif
