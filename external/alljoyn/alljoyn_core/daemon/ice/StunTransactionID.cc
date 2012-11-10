/**
 * @file
 *
 * This file implements the STUN Attribute Generic Address base class
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
#include <qcc/Socket.h>
#include "Status.h"
#include <StunTransactionID.h>

using namespace qcc;

#define QCC_MODULE "STUN_TRANSACTION_ID"

QStatus StunTransactionID::Parse(const uint8_t*& buf, size_t& bufSize)
{
    QStatus status = ER_OK;

    QCC_DbgTrace(("StunTransactionID::Parse(*buf, bufSize = %u)", bufSize));

    if (bufSize < Size()) {
        status = ER_BUFFER_TOO_SMALL;
        QCC_LogError(status, ("Parsing Transaction (missing %u)", Size() - bufSize));
        goto exit;
    }

    memcpy(id, buf, sizeof(id));

    buf += Size();
    bufSize -= Size();

exit:
    return status;
}


QStatus StunTransactionID::RenderBinary(uint8_t*& buf, size_t& bufSize,
                                        ScatterGatherList& sg) const
{
    memcpy(buf, id, sizeof(id));

    sg.AddBuffer(&buf[0], Size());
    sg.IncDataSize(Size());

    buf += Size();
    bufSize -= Size();

    return ER_OK;
}

String StunTransactionID::ToString(void)
{
    if (value.empty()) {
        value = BytesToHexString(id, SIZE, true);
    }
    return value;
}

void StunTransactionID::SetValue(StunTransactionID& other)
{
    memcpy(id, other.id, sizeof(id));
    value.clear();
}



