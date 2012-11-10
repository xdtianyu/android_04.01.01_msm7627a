#ifndef _STUNATTRIBUTECHANNELNUMBER_H
#define _STUNATTRIBUTECHANNELNUMBER_H
/**
 * @file
 *
 * This file defines the CHANNEL-NUMBER STUN message attribute.
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
#error Only include StunAttributeChannelNumber.h in C++ code.
#endif

#include <qcc/platform.h>
#include <string>
#include <StunAttributeBase.h>
#include <types.h>
#include "Status.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"


/**
 * Channel Number STUN attribute class.
 */
class StunAttributeChannelNumber : public StunAttribute {
  private:
    /// Size of the Channel Number STUN attribute.
    uint16_t channelNumber;  ///< Channel Number.

  public:
    /**
     * This constructor sets the attribute type to STUN_ATTR_CHANNEL_NUMBER
     * and initializes the channel number.
     *
     * @param channelNumber The channel number (defaults to 0).
     */
    StunAttributeChannelNumber(uint16_t channelNumber = 0) :
        StunAttribute(STUN_ATTR_CHANNEL_NUMBER, "CHANNEL-NUMBER"),
        channelNumber(channelNumber)
    { }


    QStatus Parse(const uint8_t*& buf, size_t& bufSize);
    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const;
#if !defined(NDEBUG)
    String ToString(void) const;
#endif
    size_t RenderSize(void) const { return Size(); }
    uint16_t AttrSize(void) const
    {
        // The TURN draft-13 spec section 14.1 specifies the RFFU as part of
        // the attribute so include it in the size.
        return (sizeof(channelNumber) +
                sizeof(uint16_t));  // size of RFFU
    }

    /**
     * Retrieve the channel number.
     *
     * @return The channel number.
     */
    uint16_t GetChannelNumber(void) const { return channelNumber; }

    /**
     * Set the channel number.
     *
     * @param channelNumber  The channel number.
     */
    void SetChannelNumber(uint16_t channelNumber) { this->channelNumber = channelNumber; }
};


#undef QCC_MODULE
#endif
