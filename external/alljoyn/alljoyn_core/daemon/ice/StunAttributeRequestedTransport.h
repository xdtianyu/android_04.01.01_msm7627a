#ifndef _STUNATTRIBUTEREQUESTEDTRANSPORT_H
#define _STUNATTRIBUTEREQUESTEDTRANSPORT_H
/**
 * @file
 *
 * This file defines the REQUESTED-TRANSPORT STUN message attribute.
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
#error Only include StunAttributeRequestedTransport.h in C++ code.
#endif

#include <string>
#include <qcc/platform.h>
#include <StunAttributeBase.h>
#include <types.h>
#include "Status.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"


/**
 * Requested Transport STUN attribute class.
 */
class StunAttributeRequestedTransport : public StunAttribute {
  private:
    uint8_t protocol;   ///< IP protocol.

  public:
    /**
     * This constructor sets the attribute type to STUN_ATTR_REQUESTED_TRANSPORT
     * and initializes the protocol.
     *
     * @param protocol  The protocol (defaults to 0).
     */
    StunAttributeRequestedTransport(uint8_t protocol = 0) :
        StunAttribute(STUN_ATTR_REQUESTED_TRANSPORT, "REQUESTED-TRANSPORT"),
        protocol(protocol)
    { }

    QStatus Parse(const uint8_t*& buf, size_t& bufSize);
    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const;
#if !defined(NDEBUG)
    String ToString(void) const;
#endif
    size_t RenderSize(void) const { return Size(); }
    uint16_t AttrSize(void) const
    {
        // The TURN draft-13 spec section 14.7 specifies the RFFU as part of
        // the attribute so include it in the size.
        return (sizeof(uint8_t) +     // size of protocol
                3 * sizeof(uint8_t)); // size of RFFU;
    }

    /**
     * Retrieve the protocol.
     *
     * @return The requested protocol.
     */
    uint8_t GetProtocol(void) const { return protocol; }

    /**
     * Set the protocol.
     *
     * @param protocol  The requested protocol.
     */
    void SetProtocol(uint8_t protocol) { this->protocol = protocol; }
};


#undef QCC_MODULE
#endif
