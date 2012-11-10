/**
 * @file NetworkNetworkInterface.h
 *
 * This file defines a class that are used to perform some network interface related operations
 * that are required by the ICE transport.
 *
 */

/******************************************************************************
 * Copyright 2012 Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/

#ifndef _NETWORKINTERFACE_H
#define _NETWORKINTERFACE_H

#ifndef __cplusplus
#error Only include NetworkInterface.h in C++ code.
#endif

#include <vector>
#include <list>

#include <qcc/String.h>
#include <qcc/IfConfig.h>

#include "Status.h"

using namespace std;
using namespace qcc;

namespace ajn {

class NetworkInterface {

  public:
    /**
     * @internal
     * @brief Bit masks used to specify the interface type.
     */
    /* None */
    static const uint8_t NONE = 0x00;

    /* Any of the available interface types */
    static const uint8_t ANY = 0xFF;

    /**
     * @brief List of available live Ethernet interfaces
     */
    std::vector<qcc::IfConfigEntry> liveInterfaces;

    /**
     * @brief Flag used to indicate if interfaces with IPV6 addresses are to be
     * used
     */
    bool EnableIPV6;

    /**
     * @internal
     *
     * @brief Constructor.
     *
     * @param enableIPV6 - Flag used to indicate if interfaces with IPV6 addresses
     *                     are to be used
     */
    NetworkInterface(bool enableIPV6);

    /**
     * @internal
     *
     * @brief Destructor.
     *
     */
    ~NetworkInterface();

    /**
     * @internal
     * @brief Utility function to print the interface type.
     */
    String PrintNetworkInterfaceType(uint8_t type);

    /**
     * @internal
     * @brief Update the interfaces to get a list of the current live interfaces
     */
    QStatus UpdateNetworkInterfaces(void);

    /**
     * @internal
     * @brief Function used to find if any live network interfaces are available
     */
    bool IsAnyNetworkInterfaceUp(void);
};

} // namespace ajn

#endif //_NETWORKINTERFACE_H
