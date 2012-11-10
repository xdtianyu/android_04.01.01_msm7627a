/**
 * @file
 *
 * This file implements the STUN Attribute base class
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
#include <StunAttribute.h>
#include "Status.h"

using namespace qcc;

#define QCC_MODULE "STUN_ATTRIBUTE"

QStatus StunAttribute::RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const
{
    QStatus status = ER_OK;

    uint16_t attrSize = AttrSize();

    QCC_DbgTrace(("StunAttribute::RenderBinary(buf, bufSize = %u, sg) [%s: %u/%u]", bufSize, name, RenderSize(), AttrSize()));

    assert(!parsed);

    if (bufSize < RenderSize()) {
        status = ER_BUFFER_TOO_SMALL;
        QCC_LogError(status, ("Rendering %s attribute (%u short)", name, RenderSize() - bufSize));
        goto exit;
    }

    WriteHostToNet(buf, bufSize, static_cast<uint16_t>(type), sg);
    WriteHostToNet(buf, bufSize, static_cast<uint16_t>(attrSize), sg);

exit:
    return status;
}
