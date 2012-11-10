/**
 * @file
 * Bluetooth bus address class definition.
 */

/******************************************************************************
 * Copyright 2009-2011, Qualcomm Innovation Center, Inc.
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
#ifndef _ALLJOYN_BTBUSADDRESS_H
#define _ALLJOYN_BTBUSADDRESS_H

#include <qcc/platform.h>


#include <qcc/String.h>

#include "BDAddress.h"
#include "BTTransportConsts.h"
#include "Transport.h"


namespace ajn {

struct BTBusAddress {

    BDAddress addr;    /**< BDAddress part of the bus address. */
    uint16_t psm;      /**< L2CAP PSM part of the bus address. */

    /**
     * default constructor
     */
    BTBusAddress() : psm(bt::INVALID_PSM) { }

    /**
     * Copy constructor
     *
     * @param other     reference to other instance to intialize from
     */
    BTBusAddress(const BTBusAddress& other) : addr(other.addr), psm(other.psm) { }

    /**
     * Constructor that initializes from separate BDAddress and PSM.
     *
     * @param addr  BDAddress part of the bus address
     * @param psm   PSM part of the bus address
     */
    BTBusAddress(const BDAddress addr, uint16_t psm) : addr(addr), psm(psm) { }

    /**
     * Constructor that initializes from a bus address spec string.
     *
     * @param addrSpec  bus address encoded in a string: "bluetooth:addr=XX:XX:XX:XX:XX:XX,psm=0xXXXX"
     */
    BTBusAddress(const qcc::String& addrSpec) { FromSpec(addrSpec); }

    /**
     * The the bus address from a bus address spec string.
     *
     * @param addrSpec  bus address encoded in a string: "bluetooth:addr=XX:XX:XX:XX:XX:XX,psm=0xXXXX"
     */
    void FromSpec(const qcc::String& addrSpec)
    {
        std::map<qcc::String, qcc::String> argMap;
        Transport::ParseArguments("bluetooth", addrSpec.c_str(), argMap);
        addr.FromString(argMap["addr"]);
        psm = StringToU32(argMap["psm"], 0, bt::INVALID_PSM);
    }

    /**
     * Create a bus address spec string from the bus address.
     *
     * @return  a string representation of the bus address: "bluetooth:addr=XX:XX:XX:XX:XX:XX,psm=0xXXXX"
     */
    qcc::String ToSpec() const
    {
        return qcc::String("bluetooth:addr=" + addr.ToString() + ",psm=0x" + qcc::U32ToString(psm, 16, 4, '0'));
    }

    /**
     * Create a bus address string from the bus address in a human readable form.
     *
     * @return  a string representation of the bus address: "XX:XX:XX:XX:XX:XX-XXXX"
     */
    qcc::String ToString() const
    {
        return qcc::String(addr.ToString() + "-" + qcc::U32ToString(psm, 16, 4, '0'));
    }

    /**
     * Check is the bus address is valid.
     *
     * @return  true if the bus address is valid, false otherwise
     */
    bool IsValid() const { return psm != bt::INVALID_PSM; }

    /**
     * Less than operator.
     *
     * @param other     reference to the rhs of "<" for comparison
     *
     * @return  true if this is < other, false otherwise
     */
    bool operator<(const BTBusAddress& other) const
    {
        return (addr < other.addr) || ((addr == other.addr) && (psm < other.psm));
    }

    /**
     * Equivalence operator.
     *
     * @param other     reference to the rhs of "==" for comparison
     *
     * @return  true if this is == other, false otherwise
     */
    bool operator==(const BTBusAddress& other) const { return (addr == other.addr) && (psm == other.psm); }

    /**
     * Inequality operator.
     *
     * @param other     reference to the rhs of "==" for comparison
     *
     * @return  true if this is != other, false otherwise
     */
    bool operator!=(const BTBusAddress& other) const { return !(*this == other); }
};

}

#endif
