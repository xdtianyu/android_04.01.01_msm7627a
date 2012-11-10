#ifndef _STUNATTRIBUTEXORMAPPEDADDRESS_H
#define _STUNATTRIBUTEXORMAPPEDADDRESS_H
/**
 * @file
 *
 * This file defines the XOR-MAPPED-ADDRESS STUN message attribute.
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
#error Only include attr/StunAttributeXorMappedAddress.h in C++ code.
#endif

#include <string>
#include <qcc/platform.h>
#include <StunAttributeMappedAddress.h>
#include <types.h>
#include "Status.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"

/**
 * XOR Mapped Address STUN attribute class.  This is nearly identical to
 * StunAttributeMappedAddress except for the attribute type number and the
 * parser and renderer which apply the XOR algorithm defined in RFC 5389.
 */
class StunAttributeXorMappedAddress : public StunAttributeMappedAddress {
  private:
    const StunMessage& message; ///< Reference to containing message.

  protected:
    /**
     * This constructor just sets the attribute type to passed in value.
     *
     * @param attrType The STUN Attribute Type.
     * @param attrName The STUN Attribute name in human readable form.
     * @param msg Reference to the containing message.
     */
    StunAttributeXorMappedAddress(StunAttrType attrType,
                                  const char* const attrName,
                                  const StunMessage& msg) :
        StunAttributeMappedAddress(attrType, attrName),
        message(msg)
    { }

    /**
     * This constructor just sets the attribute type to passed in value and
     * initializes the IP address and port.
     *
     * @param attrType The STUN Attribute Type.
     * @param attrName The STUN Attribute name in human readable form.
     * @param msg Reference to the containing message.
     * @param addr  IP Address.
     * @param port  IP Port.
     */
    StunAttributeXorMappedAddress(StunAttrType attrType,
                                  const char* const attrName,
                                  const StunMessage& msg,
                                  const IPAddress& addr,
                                  uint16_t port) :
        StunAttributeMappedAddress(attrType, attrName, addr, port),
        message(msg)
    { }

  public:

    static const uint16_t MIN_ATTR_SIZE = (sizeof(uint8_t) +        // Unused octet.
                                           sizeof(uint8_t) +        // Address family.
                                           sizeof(uint16_t) +       // Port.
                                           IPAddress::IPv4_SIZE);   // IPv4 address.

    /**
     * This constructor just sets the attribute type to STUN_ATTR_XOR_MAPPED_ADDRESS.
     *
     * @param msg Reference to the containing message.
     */
    StunAttributeXorMappedAddress(const StunMessage& msg) :
        StunAttributeMappedAddress(STUN_ATTR_XOR_MAPPED_ADDRESS, "XOR_MAPPED_ADDRESS"),
        message(msg)
    { }

    /**
     * This constructor just sets the attribute type to STUN_ATTR_XOR_MAPPED_ADDRESS
     * and initializes the IP address and port.
     *
     * @param msg Reference to the containing message.
     * @param addr  IP Address.
     * @param port  IP Port.
     */
    StunAttributeXorMappedAddress(const StunMessage& msg, const IPAddress& addr, uint16_t port) :
        StunAttributeMappedAddress(STUN_ATTR_XOR_MAPPED_ADDRESS, "XOR_MAPPED_ADDRESS", addr, port),
        message(msg)
    { }

    QStatus Parse(const uint8_t*& buf, size_t& bufSize);
    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const;

};


#undef QCC_MODULE
#endif
