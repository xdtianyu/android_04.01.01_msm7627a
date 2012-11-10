#ifndef _STUNATTRIBUTEICECONTROLLED_H
#define _STUNATTRIBUTEICECONTROLLED_H
/**
 * @file
 *
 * This file defines the ICE-CONTROLLED STUN message attribute.
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
#error Only include StunAttributeIceControlled.h in C++ code.
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
 * ICE Controlled STUN attribute class.
 */
class StunAttributeIceControlled : public StunAttribute {
  private:
    uint64_t value;

  public:
    /**
     * This constructor sets the attribute type to STUN_ATTR_ICE_CONTROLLED
     * and initializes the value.
     *
     * @param value The value value (defaults to 0).
     */
    StunAttributeIceControlled(uint64_t value = 0) :
        StunAttribute(STUN_ATTR_ICE_CONTROLLED, "ICE-CONTROLLED"),
        value(value)
    { }

    QStatus Parse(const uint8_t*& buf, size_t& bufSize)
    {
        ReadNetToHost(buf, bufSize, value);
        return StunAttribute::Parse(buf, bufSize);
    }

    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const
    {
        QStatus status = StunAttribute::RenderBinary(buf, bufSize, sg);
        if (status == ER_OK) {
            WriteHostToNet(buf, bufSize, value, sg);
        }
        return status;
    }

#if !defined(NDEBUG)
    String ToString(void) const
    {
        String oss;

        oss.append(StunAttribute::ToString());
        oss.append(": ");

        oss.append(U32ToString(static_cast<uint32_t>(value >> 32), 16, 8, '0'));
        oss.push_back('-');
        oss.append(U32ToString(static_cast<uint32_t>(value & 0xffffffff), 16, 8, '0'));

        return oss;
    }
#endif
    size_t RenderSize(void) const { return Size(); }
    uint16_t AttrSize(void) const { return sizeof(value); }

    /**
     * Set the value.
     */
    void SetValue(uint64_t value) { this->value = value; }

    /**
     * Get the value
     */
    uint64_t GetValue(void) const { return value; }

};


#undef QCC_MODULE
#endif
