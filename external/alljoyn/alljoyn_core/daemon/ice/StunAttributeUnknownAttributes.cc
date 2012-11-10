/**
 * @file
 *
 * This file implements the STUN Attribute Unknown Attributes class
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
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <StunAttributeUnknownAttributes.h>
#include "Status.h"

using namespace qcc;

#define QCC_MODULE "STUN_ATTRIBUTE"


QStatus StunAttributeUnknownAttributes::Parse(const uint8_t*& buf, size_t& bufSize)
{
    QStatus status;

    while (bufSize > 0) {
        uint16_t attr = 0;
        ReadNetToHost(buf, bufSize, attr);
        AddAttribute(attr);
    }

    status = StunAttribute::Parse(buf, bufSize);

    return status;
}



QStatus StunAttributeUnknownAttributes::RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const

{
    std::vector<uint16_t>::const_iterator iter;
    QStatus status;
    QCC_DbgTrace(("StunAttributeUnknownAttributes::RenderBinary(*buf, bufSize = %u, sg = <>)", bufSize));

    status = StunAttribute::RenderBinary(buf, bufSize, sg);
    if (status != ER_OK) {
        goto exit;
    }

    for (iter = Begin(); iter != End(); ++iter) {
        QCC_DbgPrintf(("Adding %04x (%u bytes - space: %u)...", *iter, sizeof(*iter), bufSize));
        WriteHostToNet(buf, bufSize, *iter, sg);
    }

    if ((attrTypes.size() & 0x1) == 1) {
        // pad with an empty bytes.
        WriteHostToNet(buf, bufSize, static_cast<uint16_t>(0), sg);
    }

exit:
    return status;
}


#if !defined(NDEBUG)
String StunAttributeUnknownAttributes::ToString(void) const
{
    std::vector<uint16_t>::const_iterator iter;
    String oss;

    oss.append(StunAttribute::ToString());
    oss.append(": ");

    iter = Begin();
    while (iter != End()) {
        oss.append(U32ToString(*iter, 16, 4, '0'));
        ++iter;
        if (iter != End()) {
            oss.append(", ");
        }
    }

    return oss;
}
#endif
