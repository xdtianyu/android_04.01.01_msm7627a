/**
 * @file
 *
 * This file implements the STUN Attribute Even Port class
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
#include <StunAttributeEvenPort.h>
#include "Status.h"

using namespace qcc;

#define QCC_MODULE "STUN_ATTRIBUTE"


QStatus StunAttributeEvenPort::Parse(const uint8_t*& buf, size_t& bufSize)
{
    QStatus status;

    nextPort = static_cast<bool>(*buf & 0x80);
    buf += AttrSize();
    bufSize -= AttrSize();

    status = StunAttribute::Parse(buf, bufSize);

    return status;
}


QStatus StunAttributeEvenPort::RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const
{
    QStatus status;

    status = StunAttribute::RenderBinary(buf, bufSize, sg);
    if (status != ER_OK) {
        goto exit;
    }

    // While RFC indicates 1 byte, empirical testing against server shows 4 bytes
    WriteHostToNet(buf, bufSize, static_cast<uint32_t>(nextPort ? 0x80000000 : 0x00), sg);

    //WriteHostToNet(buf, bufSize, static_cast<uint8_t>(nextPort ? 0x80 : 0x00), sg);

    // pad with empty bytes
    //WriteHostToNet(buf, bufSize, static_cast<uint8_t>(0), sg);
    //WriteHostToNet(buf, bufSize, static_cast<uint16_t>(0), sg);

exit:
    return status;
}

#if !defined(NDEBUG)
String StunAttributeEvenPort::ToString(void) const
{
    String oss;

    oss.append(StunAttribute::ToString());
    if (nextPort) {
        oss.append(" (and next port)");
    }

    return oss;
}
#endif

QStatus StunAttributeHexSeventeen::Parse(const uint8_t*& buf, size_t& bufSize)
{
    QStatus status;

    buf += AttrSize();
    bufSize -= AttrSize();

    status = StunAttribute::Parse(buf, bufSize);

    return status;
}


QStatus StunAttributeHexSeventeen::RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const
{
    QStatus status;

    status = StunAttribute::RenderBinary(buf, bufSize, sg);
    if (status != ER_OK) {
        goto exit;
    }

    WriteHostToNet(buf, bufSize, static_cast<uint32_t>(0x01000000), sg);

exit:
    return status;
}

#if !defined(NDEBUG)
String StunAttributeHexSeventeen::ToString(void) const
{
    String oss;

    return StunAttribute::ToString();

}
#endif
