#ifndef _STUNATTRIBUTEEVENPORT_H
#define _STUNATTRIBUTEEVENPORT_H
/**
 * @file
 *
 * This file defines the EVEN-PORT STUN message attribute.
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
#error Only include StunAttributeEvenPort.h in C++ code.
#endif

#include <string>
#include <qcc/platform.h>
#include <StunAttributeBase.h>
#include "Status.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"


/**
 * Even Port STUN attribute class.
 */
class StunAttributeEvenPort : public StunAttribute {
  private:
    bool nextPort;   ///< @brief Flag indicating the next higher port should
                     //   be allocated as well.

  public:
    /**
     * This constructor sets the attribute type to STUN_ATTR_EVEN_PORT and
     * initializes the next port flag.
     *
     * @param np    Flag indicating if the next port should be reserved as well
     *              (defaults to 'false').
     */
    StunAttributeEvenPort(bool np = false) :
        StunAttribute(STUN_ATTR_EVEN_PORT, "EVEN-PORT"),
        nextPort(np)
    { }

    QStatus Parse(const uint8_t*& buf, size_t& bufSize);
    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const;
#if !defined(NDEBUG)
    String ToString(void) const;
#endif
    size_t RenderSize(void) const { return Size(); }
    uint16_t AttrSize(void) const
    {
        // The TURN draft-13 spec section 14.6 only specifies 1 octet for the
        // attribute, so that will be the attribute size.
        // While RFC shows 1 byte, empirical testing against server shows 4 bytes
        return sizeof(uint32_t);
    }

    /**
     * Get the next port flag value.  "true" indicates that the next port
     * should be allocated.
     *
     * @return Value of the next port flag.
     */
    bool GetNextPort(void) const { return nextPort; }

    /**
     * Set the next port flag to "true" or "false" to indicate if the TURN server
     * should allocate the next higher port.
     *
     * @param v     "true" if the next port should be allocated.
     */
    void SetNextPort(bool v) { nextPort = v; }
};


class StunAttributeHexSeventeen : public StunAttribute {
  private:

  public:
    /**
     * This constructor sets the attribute type to 0x0017
     *
     */
    StunAttributeHexSeventeen() :
        StunAttribute((StunAttrType) 0x0017, "HEXSEVENTEEN")
    { }

    QStatus Parse(const uint8_t*& buf, size_t& bufSize);
    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const;
#if !defined(NDEBUG)
    String ToString(void) const;
#endif
    size_t RenderSize(void) const { return Size(); }
    uint16_t AttrSize(void) const
    {
        return sizeof(uint32_t);
    }
};


#undef QCC_MODULE
#endif
