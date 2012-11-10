/**
 * @file
 *
 * OS specific utility functions
 */

/******************************************************************************
 * Copyright 2011, Qualcomm Innovation Center, Inc.
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

#include <windows.h>
#include <bcrypt.h>

#include <qcc/Crypto.h>
#include <qcc/Util.h>

#define QCC_MODULE  "CRYPTO"

QStatus qcc::Crypto_GetRandomBytes(uint8_t* data, size_t len)
{
    if (BCryptGenRandom(NULL, data, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG) <= 0) {
        return ER_OK;
    } else {
        return ER_FAIL;
    }
}
