#ifndef _STUNATTRIBUTEMESSAGEINTEGRITY_H
#define _STUNATTRIBUTEMESSAGEINTEGRITY_H
/**
 * @file
 *
 * This file defines the MESSAGE-INTEGRITY STUN message attribute.
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
#error Only include StunAttributeBase.h in C++ code.
#endif

#include <string>
#include <qcc/platform.h>
#include <qcc/Crypto.h>
#include <StunAttributeBase.h>
#include <types.h>
#include "Status.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"

class StunMessage;

/**
 * Message Integrity STUN attribute class.
 */
class StunAttributeMessageIntegrity : public StunAttribute {
  public:
    enum MessageIntegrityStatus {
        NOT_CHECKED,
        VALID,
        INVALID,
        NO_HMAC
    };

    /**
     * StunAttributeMessageIntegrity constructor.  Message integrity only
     * works for the message this instance is contained in.  Therefore, the
     * message this attribute belongs to must be provided to the constructor.
     *
     * @param msg Reference to the containing message.
     */
    StunAttributeMessageIntegrity(const StunMessage& msg) :
        StunAttribute(STUN_ATTR_MESSAGE_INTEGRITY, "MESSAGE-INTEGRITY"),
        message(msg),
        digest(NULL),
        miStatus(NOT_CHECKED)
    { }

    QStatus Parse(const uint8_t*& buf, size_t& bufSize);
    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const;
    size_t RenderSize(void) const { return Size(); }
    uint16_t AttrSize(void) const { return static_cast<uint16_t>(Crypto_SHA1::DIGEST_SIZE); }

    MessageIntegrityStatus GetMessageIntegrityStatus(void) { return miStatus; }

    static const uint16_t ATTR_SIZE = static_cast<uint16_t>(Crypto_SHA1::DIGEST_SIZE);

    static const uint16_t ATTR_SIZE_WITH_HEADER = ((StunAttribute::ATTR_HEADER_SIZE + ATTR_SIZE + 3) & 0xfffc);

  private:
    const StunMessage& message;         ///< Reference to containing message.
    const uint8_t* digest;              ///< HMAC-SHA1 value for containing message.
    MessageIntegrityStatus miStatus;    ///< Parsed Message Integrity status.
};


#undef QCC_MODULE
#endif
