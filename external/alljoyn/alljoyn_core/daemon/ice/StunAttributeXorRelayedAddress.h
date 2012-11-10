#ifndef _STUNATTRIBUTEXORRELAYEDADDRESS_H
#define _STUNATTRIBUTEXORRELAYEDADDRESS_H
/**
 * @file
 *
 * This file defines the XOR-RELAYED-ADDRESS STUN message attribute.
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
#error Only include StunAddr/StunAttributeXorRelayedAddress.h in C++ code.
#endif

#include <string>
#include <qcc/platform.h>
#include <StunAttributeXorMappedAddress.h>
#include <types.h>
#include "Status.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"


/**
 * XOR Relayed Address STUN attribute class.
 */
class StunAttributeXorRelayedAddress : public StunAttributeXorMappedAddress {
  public:
    /**
     * This constructor just sets the attribute type to
     * STUN_ATTR_XOR_RELAYED_ADDRESS.
     *
     * @param msg Reference to the containing message.
     */
    StunAttributeXorRelayedAddress(const StunMessage& msg) :
        StunAttributeXorMappedAddress(STUN_ATTR_XOR_RELAYED_ADDRESS,
                                      "XOR_RELAYED_ADDRESS",
                                      msg)
    { }

    /**
     * This constructor just sets the attribute type to
     * STUN_ATTR_XOR_RELAYED_ADDRESS and initializes the IP address and port.
     *
     * @param msg Reference to the containing message.
     *
     * @param addr  IP Address.
     * @param port  IP Port.
     */
    StunAttributeXorRelayedAddress(const StunMessage& msg, const IPAddress& addr, uint16_t port) :
        StunAttributeXorMappedAddress(STUN_ATTR_XOR_RELAYED_ADDRESS,
                                      "XOR_RELAYED_ADDRESS",
                                      msg, addr, port)
    { }
};


#undef QCC_MODULE
#endif
