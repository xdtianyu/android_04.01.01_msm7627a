#ifndef _STUNATTRIBUTEALTERNATESERVER_H
#define _STUNATTRIBUTEALTERNATESERVER_H
/**
 * @file
 *
 * This file defines the ALTERNATE-SERVER STUN message attribute.
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
#error Only include StunAttributeAlternateServer.h in C++ code.
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
 * Alternate Server STUN attribute class.
 */
class StunAttributeAlternateServer : public StunAttributeMappedAddress {
  public:
    /**
     * This constructor just sets the attribute type to STUN_ATTR_ALTERNATE_SERVER.
     */
    StunAttributeAlternateServer(void) :
        StunAttributeMappedAddress(STUN_ATTR_ALTERNATE_SERVER, "ALTERNATE_SERVER")
    { }

    /**
     * This constructor sets the attribute type to STUN_ATTR_ALTERNATE_SERVER
     * and initializes the IP address and port.
     *
     * @param addr  IP Address.
     * @param port  IP Port.
     */
    StunAttributeAlternateServer(const IPAddress& addr, uint16_t port) :
        StunAttributeMappedAddress(STUN_ATTR_ALTERNATE_SERVER, "ALTERNATE_SERVER", addr, port)
    { }

};


#undef QCC_MODULE
#endif
