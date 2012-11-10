#ifndef _STUNATTRIBUTELIFETIME_H
#define _STUNATTRIBUTELIFETIME_H
/**
 * @file
 *
 * This file defines the LIFETIME STUN message attribute.
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
#error Only include StunAttributeLifetime.h in C++ code.
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
 * Lifetime STUN attribute class.
 */
class StunAttributeLifetime : public StunAttribute {
  private:
    uint32_t lifetime;   ///< Lifetime in seconds.

  public:
    /**
     * This constructor sets the attribute type to STUN_ATTR_LIFETIME and
     * initializes the lifetime variable.
     *
     * @param lifetime  Number of seconds the server should maintain allocations
     *                  in the absence of a refresh (defaults to 0).
     */
    StunAttributeLifetime(uint32_t lifetime = 0) :
        StunAttribute(STUN_ATTR_LIFETIME, "LIFETIME"),
        lifetime(lifetime)
    { }

    QStatus Parse(const uint8_t*& buf, size_t& bufSize)
    {
        ReadNetToHost(buf, bufSize, lifetime);
        return StunAttribute::Parse(buf, bufSize);
    }

    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const
    {
        QStatus status = StunAttribute::RenderBinary(buf, bufSize, sg);
        if (status == ER_OK) {
            WriteHostToNet(buf, bufSize, lifetime, sg);
        }
        return status;
    }

#if !defined(NDEBUG)
    String ToString(void) const
    {
        String oss;

        oss.append(StunAttribute::ToString());
        oss.append(": ");
        oss.append(U32ToString(lifetime, 10));
        oss.append(" seconds");

        return oss;
    }
#endif
    size_t RenderSize(void) const { return Size(); }
    uint16_t AttrSize(void) const { return sizeof(lifetime); }

    /**
     * Get the lifetime value.
     *
     * @return  Number of seconds the server should maintain allocations in the
     *          absence of a refresh.
     */
    uint32_t GetLifetime(void) const { return lifetime; }

    /**
     * Sets the lifetime value.
     *
     * @param lifetime  Number of seconds server should maintain allocations in
     *                  the absence of a refresh.
     */
    void SetLifetime(uint32_t lifetime) { this->lifetime = lifetime; }
};


#undef QCC_MODULE
#endif
