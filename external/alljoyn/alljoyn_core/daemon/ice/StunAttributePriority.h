#ifndef _STUNATTRIBUTEPRIORITY_H
#define _STUNATTRIBUTEPRIORITY_H
/**
 * @file
 *
 * This file defines the PRIORITY STUN message attribute.
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
#error Only include StunAttributePriority.h in C++ code.
#endif

#include <string>
#include <qcc/platform.h>
#include <qcc/StringUtil.h>
#include <StunAttributeBase.h>
#include <types.h>
#include "Status.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"


/**
 * Priority STUN attribute class.
 */
class StunAttributePriority : public StunAttribute {
  private:
    uint32_t priority;   ///< Priority of peer reflexive address

  public:
    /**
     * This constructor sets the attribute type to STUN_ATTR_PRIORITY and
     * initializes the priority variable.
     *
     * @param priority  Priority level of the peer reflexive address.
     */
    StunAttributePriority(uint32_t priority = 0) :
        StunAttribute(STUN_ATTR_PRIORITY, "PRIORITY"),
        priority(priority)
    { }

    QStatus Parse(const uint8_t*& buf, size_t& bufSize)
    {
        ReadNetToHost(buf, bufSize, priority);
        return StunAttribute::Parse(buf, bufSize);
    }

    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const
    {
        QStatus status = StunAttribute::RenderBinary(buf, bufSize, sg);
        if (status == ER_OK) {
            WriteHostToNet(buf, bufSize, priority, sg);
        }
        return status;
    }

#if !defined(NDEBUG)
    String ToString(void) const
    {
        String oss;

        oss.append(StunAttribute::ToString());
        oss.append(": ");
        oss.append(U32ToString(priority, 10));

        return oss;
    }
#endif
    size_t RenderSize(void) const { return Size(); }
    uint16_t AttrSize(void) const { return sizeof(priority); }

    /**
     * Get the priority value.
     *
     * @return  Number of seconds the server should maintain allocations in the
     *          absence of a refresh.
     */
    uint32_t GetPriority(void) const { return priority; }

    /**
     * Sets the priority value.
     *
     * @param priority  Number of seconds server should maintain allocations in
     *                  the absence of a refresh.
     */
    void SetPriority(uint32_t priority) { this->priority = priority; }
};


#undef QCC_MODULE
#endif
