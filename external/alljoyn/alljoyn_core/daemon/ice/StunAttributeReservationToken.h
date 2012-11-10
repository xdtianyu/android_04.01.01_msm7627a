#ifndef _STUNATTRIBUTERESERVATIONTOKEN_H
#define _STUNATTRIBUTERESERVATIONTOKEN_H
/**
 * @file
 *
 * This file defines the RESERVATION-TOKEN STUN message attribute.
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
#error Only include StunAttributeReservationToken.h in C++ code.
#endif

#include <string>
#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <StunAttributeBase.h>
#include <types.h>
#include "Status.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"


/**
 * Reservation Token STUN attribute class.
 */
class StunAttributeReservationToken : public StunAttribute {
  private:
    uint64_t token;   ///< TURN resource allocation token.

  public:
    /**
     * This constructor sets the attribute type to STUN_ATTR_RESERVATION_TOKEN
     * and initializes the token value.
     *
     * @param token The token value (defaults to 0).
     */
    StunAttributeReservationToken(const uint64_t token = 0) :
        StunAttribute(STUN_ATTR_RESERVATION_TOKEN, "RESERVATION-TOKEN"),
        token(token)
    { }

    QStatus Parse(const uint8_t*& buf, size_t& bufSize)
    {
        ReadNetToHost(buf, bufSize, token);
        return StunAttribute::Parse(buf, bufSize);
    }

    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const
    {
        QStatus status = StunAttribute::RenderBinary(buf, bufSize, sg);
        if (status == ER_OK) {
            WriteHostToNet(buf, bufSize, token, sg);
        }
        return status;
    }

#if !defined(NDEBUG)
    String ToString(void) const
    {
        String oss;

        oss.append(StunAttribute::ToString());
        oss.append(": ");

        oss.append(U32ToString(static_cast<uint32_t>(token >> 32), 16, 8, '0'));
        oss.push_back('-');
        oss.append(U32ToString(static_cast<uint32_t>(token & 0xffffffff), 16, 8, '0'));

        return oss;
    }
#endif
    size_t RenderSize(void) const { return Size(); }
    uint16_t AttrSize(void) const { return sizeof(token); }

    /**
     * Retrieve the token.
     *
     * @return The TURN server reservation token.
     */
    uint64_t GetToken(void) const { return token; }

    /**
     * Set the token.
     *
     * @param token The TURN server reservation token.
     */
    void SetToken(const uint64_t& token) { this->token = token; }
};


#undef QCC_MODULE
#endif
