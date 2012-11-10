/**
 * @file CryptoKeyBlob.cc
 *
 * Class for managing key blobs
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

#include <algorithm>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#include <qcc/KeyBlob.h>
#include <qcc/Crypto.h>
#include <qcc/String.h>
#include <qcc/Util.h>
#include <Status.h>

using namespace std;
using namespace qcc;

#define QCC_MODULE "CRYPTO"

namespace qcc {

void KeyBlob::Erase()
{
    if (blobType != EMPTY) {
        tag.clear();
        memset(data, 0, size);
        delete [] data;
        blobType = EMPTY;
        data = NULL;
        size = 0;
        expiration.seconds = 0;
        role = NO_ROLE;
    }
}

KeyBlob::KeyBlob(const qcc::String& secret, size_t len, const Type initType) : blobType(initType)
{
    if (blobType != EMPTY) {
        size = (uint16_t)len;
        data = new uint8_t[len];
        role = NO_ROLE;
        uint8_t* p = data;

        while (len) {
            static const char kb[] = "keyblob";
            uint8_t digest[Crypto_SHA1::DIGEST_SIZE];
            Crypto_SHA1 sha1;
            sha1.Init((const uint8_t*)secret.data(), secret.size());
            sha1.Update((const uint8_t*)kb, sizeof(kb));
            sha1.Update((const uint8_t*)&len, sizeof(len));
            sha1.Update((const uint8_t*)&blobType, sizeof(blobType));
            sha1.GetDigest(digest);
            if (len < sizeof(digest)) {
                memcpy(p, digest, len);
                len = 0;
            } else {
                memcpy(p, digest, sizeof(digest));
                len -= sizeof(digest);
                p += sizeof(digest);
            }
        }
    } else {
        size = 0;
        data = NULL;
        role = NO_ROLE;
    }
}

void KeyBlob::Rand(const size_t len, const Type initType)
{
    Erase();
    if (initType != EMPTY) {
        blobType = initType;
        size = (uint16_t)len;
        data = new uint8_t[len];
        Crypto_GetRandomBytes(data, len);
    }
}

QStatus KeyBlob::Set(const uint8_t* key, size_t len, Type initType)
{
    if (!key) {
        return ER_BAD_ARG_1;
    }
    if (!len) {
        return ER_BAD_ARG_2;
    }
    if (initType >= INVALID) {
        return ER_BAD_ARG_3;
    }
    Erase();
    if (initType != EMPTY) {
        blobType = initType;
        data = new uint8_t[len];
        size = (uint16_t)len;
        memcpy(data, key, len);
    }
    return ER_OK;
}

#define EXPIRES_FLAG  0x80
#define UNuSED_FLAG   0x40
#define MAx_TAG_LEN   0x3F

QStatus KeyBlob::Store(qcc::Sink& sink) const
{
    QStatus status;
    size_t pushed;
    uint16_t flags = (uint16_t)(blobType << 8) | tag.size();

    if (expiration.seconds) {
        flags |= EXPIRES_FLAG;
    }
    status = sink.PushBytes(&flags, sizeof(flags), pushed);
    if ((status == ER_OK) && (blobType != EMPTY)) {
        if (flags & EXPIRES_FLAG) {
            status = sink.PushBytes(&expiration.seconds, sizeof(expiration.seconds), pushed);
            if (status == ER_OK) {
                status = sink.PushBytes(&expiration.mseconds, sizeof(expiration.mseconds), pushed);
            }
        }
        if (status == ER_OK) {
            status = sink.PushBytes(tag.data(), tag.size(), pushed);
        }
        if (status == ER_OK) {
            status = sink.PushBytes(&size, sizeof(size), pushed);
        }
        if (status == ER_OK) {
            status = sink.PushBytes(data, size, pushed);
        }
    }
    return status;
}

QStatus KeyBlob::Load(qcc::Source& source)
{
    QStatus status;
    size_t pulled;
    uint16_t flags;
    /*
     * Clear out stale key data
     */
    Erase();
    status = source.PullBytes(&flags, sizeof(flags), pulled);
    blobType = (Type)(flags >> 8);
    if (status == ER_OK) {
        if (blobType >= INVALID) {
            status = ER_CORRUPT_KEYBLOB;
        }
        if ((status == ER_OK) && (flags & EXPIRES_FLAG)) {
            status = source.PullBytes(&expiration.seconds, sizeof(expiration.seconds), pulled);
            if (status == ER_OK) {
                status = source.PullBytes(&expiration.mseconds, sizeof(expiration.mseconds), pulled);
            }
        }
        if (status == ER_OK) {
            char tagBytes[MAx_TAG_LEN + 1];
            status = source.PullBytes(tagBytes, flags & 0x3F, pulled);
            if (status == ER_OK) {
                tagBytes[pulled] = 0;
                tag.insert(0, tagBytes, pulled);
            }
        }
        if (status == ER_OK) {
            /*
             * Get size and check it makes sense
             */
            status = source.PullBytes(&size, sizeof(size), pulled);
            if (size > 4096) {
                status = ER_CORRUPT_KEYBLOB;
            }
            if (status == ER_OK) {
                data = new uint8_t[size];
                status = source.PullBytes(data, size, pulled);
                if (status != ER_OK) {
                    delete [] data;
                    data = NULL;
                }
            }
        }
    }
    if (status != ER_OK) {
        blobType = EMPTY;
    }
    return status;
}

KeyBlob::KeyBlob(const KeyBlob& other)
{
    if (other.blobType != EMPTY) {
        data = new uint8_t[other.size];
        memcpy(data, other.data, other.size);
        size = other.size;
        expiration = other.expiration;
        tag = other.tag;
        role = other.role;
    } else {
        data = NULL;
        size = 0;
        role = NO_ROLE;
    }
    blobType = other.blobType;
}

KeyBlob& KeyBlob::operator=(const KeyBlob& other)
{
    if (this != &other) {
        assert(other.blobType < INVALID);
        Erase();
        if (other.blobType != EMPTY) {
            data = new uint8_t[other.size];
            memcpy(data, other.data, other.size);
            size = other.size;
            blobType = other.blobType;
            expiration = other.expiration;
            tag = other.tag;
            role = other.role;
        }
    }
    return *this;
}

size_t KeyBlob::Xor(const uint8_t* data, size_t len)
{
    if ((blobType != EMPTY) && data && len) {
        size_t sz = min(static_cast<size_t>(size), len);
        for (size_t i = 0; i < sz; ++i) {
            this->data[i] ^= data[i];
        }
        return sz;
    } else {
        return 0;
    }
}

KeyBlob& KeyBlob::operator^=(const KeyBlob& other)
{
    if ((other.blobType != EMPTY) && (blobType != EMPTY)) {
        Xor(other.data, other.size);
    }
    return *this;
}

bool KeyBlob::HasExpired()
{
    if (expiration.seconds == 0) {
        return false;
    }
    Timespec now;
    GetTimeNow(&now);
    return expiration <= now;
}


}
