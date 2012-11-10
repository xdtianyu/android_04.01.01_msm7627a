/**
 * @file Crypto.cc
 *
 * Implementation for methods from Crypto.h
 */

/******************************************************************************
 * Copyright 2009-2012, Qualcomm Innovation Center, Inc.
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

#include <assert.h>
#include <ctype.h>

#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/md5.h>

#include <qcc/platform.h>
#include <qcc/Debug.h>
#include <qcc/Crypto.h>
#include <qcc/Util.h>

#include <Status.h>

using namespace std;
using namespace qcc;

#define QCC_MODULE "CRYPTO"

namespace qcc {

class Crypto_Hash::Context {
  public:

    Context(bool MAC) : MAC(MAC) { }

    /// Union of context storage for HMAC or MD.
    union {
        HMAC_CTX hmac;    ///< Storage for the HMAC context.
        EVP_MD_CTX md;    ///< Storage for the MD context.
        uint8_t pad[512]; ///< Ensure we have enough storage for openssl 1.0
    };

    bool MAC;
};

QStatus Crypto_Hash::Init(Algorithm alg, const uint8_t* hmacKey, size_t keyLen)
{
    /*
     * Protect the open ssl APIs.
     */
    Crypto_ScopedLock lock;

    QStatus status = ER_OK;

    if (ctx) {
        delete ctx;
        ctx = NULL;
        initialized = false;
    }

    MAC = hmacKey != NULL;

    if (MAC && (keyLen == 0)) {
        status = ER_CRYPTO_ERROR;
        QCC_LogError(status, ("HMAC key length cannot be zero"));
        delete ctx;
        ctx = NULL;
        return status;
    }

    const EVP_MD* mdAlgorithm;

    switch (alg) {
    case qcc::Crypto_Hash::SHA1:
        mdAlgorithm = EVP_sha1();
        break;

    case qcc::Crypto_Hash::MD5:
        mdAlgorithm = EVP_md5();
        break;

    case qcc::Crypto_Hash::SHA256:
        mdAlgorithm = EVP_sha256();
        break;
    }

    ctx = new Crypto_Hash::Context(MAC);

    if (MAC) {
        HMAC_CTX_init(&ctx->hmac);
        HMAC_Init_ex(&ctx->hmac, hmacKey, keyLen, mdAlgorithm, NULL);
    } else if (EVP_DigestInit(&ctx->md, mdAlgorithm) == 0) {
        status = ER_CRYPTO_ERROR;
        QCC_LogError(status, ("Initializing hash digest"));
    }
    if (status == ER_OK) {
        initialized = true;
    } else {
        delete ctx;
        ctx = NULL;
    }
    return status;
}

Crypto_Hash::~Crypto_Hash(void)
{
    /*
     * Protect the open ssl APIs.
     */
    Crypto_ScopedLock lock;

    if (ctx) {
        if (initialized) {
            if (MAC) {
                HMAC_CTX_cleanup(&ctx->hmac);
            } else {
                EVP_MD_CTX_cleanup(&ctx->md);
            }
        }
        delete ctx;
    }
}

QStatus Crypto_Hash::Update(const uint8_t* buf, size_t bufSize)
{
    /*
     * Protect the open ssl APIs.
     */
    Crypto_ScopedLock lock;

    QStatus status = ER_OK;

    if (!buf) {
        return ER_BAD_ARG_1;
    }
    if (initialized) {
        if (MAC) {
            HMAC_Update(&ctx->hmac, buf, bufSize);
        } else if (EVP_DigestUpdate(&ctx->md, buf, bufSize) == 0) {
            status = ER_CRYPTO_ERROR;
            QCC_LogError(status, ("Updating hash digest"));
        }
    } else {
        status = ER_CRYPTO_HASH_UNINITIALIZED;
        QCC_LogError(status, ("Hash function not initialized"));
    }
    return status;
}

QStatus Crypto_Hash::Update(const qcc::String& str)
{
    return Update((const uint8_t*)str.data(), str.size());
}

QStatus Crypto_Hash::GetDigest(uint8_t* digest, bool keepAlive)
{
    /*
     * Protect the open ssl APIs.
     */
    Crypto_ScopedLock lock;

    QStatus status = ER_OK;

    if (!digest) {
        return ER_BAD_ARG_1;
    }
    if (initialized) {
        if (MAC) {
            /* keep alive is not allowed for HMAC */
            if (keepAlive) {
                status = ER_CRYPTO_ERROR;
                QCC_LogError(status, ("Keep alive is not allowed for HMAC"));
                keepAlive = false;
            }
            HMAC_Final(&ctx->hmac, digest, NULL);
            HMAC_CTX_cleanup(&ctx->hmac);
            initialized = false;
        } else {
            Context* keep = NULL;
            /* To keep the hash alive we need to copy the context before calling EVP_DigestFinal */
            if (keepAlive) {
                keep = new Context(false);
                EVP_MD_CTX_copy(&keep->md, &ctx->md);
            }
            if (EVP_DigestFinal(&ctx->md, digest, NULL) == 0) {
                status = ER_CRYPTO_ERROR;
                QCC_LogError(status, ("Finalizing hash digest"));
            }
            EVP_MD_CTX_cleanup(&ctx->md);
            if (keep) {
                delete ctx;
                ctx = keep;
            } else {
                initialized = false;
            }
        }
    } else {
        status = ER_CRYPTO_HASH_UNINITIALIZED;
        QCC_LogError(status, ("Hash function not initialized"));
    }
    return status;
}

}
