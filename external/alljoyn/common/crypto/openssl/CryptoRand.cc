/**
 * @file
 *
 * Platform-specific secure random number generator
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
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

#include <openssl/bn.h>
#include <qcc/Crypto.h>

#include <Status.h>


#define QCC_MODULE  "CRYPTO"

QStatus qcc::Crypto_GetRandomBytes(uint8_t* data, size_t len)
{
    /*
     * Protect the open ssl APIs.
     */
    Crypto_ScopedLock lock;

    QStatus status = ER_OK;
    BIGNUM* rand = BN_new();
    if (BN_rand(rand, len * 8, -1, 0)) {
        BN_bn2bin(rand, data);
    } else {
        status = ER_CRYPTO_ERROR;
    }
    BN_free(rand);
    return status;
}
