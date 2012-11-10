#ifndef _STUNTRANSACTIONID_H
#define _STUNTRANSACTIONID_H
/**
 * @file
 *
 * This file defines the STUN Transaction ID class.
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

#ifndef __cplusplus
#error Only include stun.h in C++ code.
#endif

#include <assert.h>
#include <string>
#include <qcc/platform.h>
#include <qcc/Crypto.h>
#include <StunIOInterface.h>
#include <qcc/Socket.h>

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_TRANSACTION"

/**
 * STUN Transaction ID class.
 *
 * This class deals with the transaction ID portion of the STUN message.
 */
class StunTransactionID : public StunIOInterface {
  public:

    /**
     * Constant defined as the size of a STUN Transaction ID value.
     */
    const static size_t SIZE = sizeof(uint32_t[3]);

    QStatus Parse(const uint8_t*& buf, size_t& bufSize);
    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const;
    String ToString(void);
    size_t Size(void) const { return SIZE; }
    size_t RenderSize(void) const { return Size(); }

    /**
     * Overload the == operator for comparing transaction IDs.
     *
     * @param other Reference to another transaction ID.
     *
     * @return true if other is the same as us.
     */
    bool operator==(const StunTransactionID& other) const
    {
        return memcmp(other.id, id, SIZE) == 0;
    }

    /**
     * Overload the == operator for comparing transaction IDs.
     *
     * @param other Reference to another transaction ID.
     *
     * @return true if other is the same as us.
     */
    bool operator!=(const StunTransactionID& other) const
    {
        return memcmp(other.id, id, SIZE) != 0;
    }

    bool operator<(const StunTransactionID& other) const
    {
        return memcmp(other.id, id, SIZE) < 0;
    }

    /**
     * Set the transaction ID to a cryptographically random value.  This
     * should be used for all requests and indications.
     */
    void SetValue(void) { Crypto_GetRandomBytes(id, SIZE); }

    /**
     * Set the transaction ID to same value as another transaction ID.  This
     * should be used for responses.
     *
     * @param other The transaction ID value to copy.
     */
    void SetValue(StunTransactionID& other);

  private:

    uint8_t id[SIZE];      ///< The transaction ID

    mutable qcc::String value;
};


#undef QCC_MODULE
#endif
