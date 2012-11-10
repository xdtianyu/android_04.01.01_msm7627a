/**
 * @file
 *
 * This file implements the STUN Credential class
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
#include <qcc/Crypto.h>
#include <qcc/Debug.h>
#include <StunCredential.h>

using namespace qcc;

#define QCC_MODULE "STUN_CREDENTIAL"


void StunCredential::ComputeShortTermKey(void)
{
    String key = SASLprep(password);

    hmacKey = (uint8_t*)malloc(key.size());
    if (!hmacKey) {
        QCC_DbgPrintf(("Malloc failed ComputeShortTermKey"));
    } else {
        keyLength = key.size();
        memcpy(hmacKey, key.data(), keyLength);
    }
}

String StunCredential::SASLprep(const String& in) const
{
    // ToDo validate chars per RFC 4013 Section 2.3
    return in;
}


QStatus StunCredential::GetKey(uint8_t* keyOut, size_t& len) const
{
    QStatus status = ER_OK;

    if (len < keyLength) {
        status = ER_BUFFER_TOO_SMALL;
    } else {
        memcpy(keyOut, hmacKey, keyLength);
    }
    len = keyLength;

    return status;
}
