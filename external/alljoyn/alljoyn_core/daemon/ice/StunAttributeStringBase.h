#ifndef _STUNATTRIBUTESTRINGBASE_H
#define _STUNATTRIBUTESTRINGBASE_H
/**
 * @file
 *
 * This file defines the base string message attribute.
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
#error Only include StunAttributeStringBase.h in C++ code.
#endif

#include <qcc/String.h>
#include <qcc/platform.h>
#include <StunAttributeBase.h>
#include <types.h>
#include "Status.h"
#include "unicode.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"

/**
 * Base String STUN attribute class.
 */
class StunAttributeStringBase : public StunAttribute {
  private:
    const static uint32_t MAX_LENGTH = 513; ///< Max str length as defined in RFC 5389.

    String str;  ///< String data

  protected:
    /**
     * Renders just the string portion of the attribute (not the header) into
     * the buffer/SG list.
     *
     * @param buf       Reference to memory where the rendering may be done.
     * @param bufSize   Amount of space left in the buffer.
     * @param sg        SG List where rendering may be done.
     */
    void RenderBinaryString(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const;

    /**
     * Retrieves the parsed UTF-8 str.
     *
     * @param str OUT: A reference to where to copy the str.
     */
    void GetStr(String& str) const
    {
        str = this->str;
    }

    /**
     * Sets the UTF-8 str.
     *
     * @param str A reference the str.
     */
    void SetStr(const String& str)
    {
        QCC_DbgTrace(("StunAttributeStringBase::SetStr(string str = %s)", str.c_str()));
        this->str = str;
    }


  public:
    /**
     * This constructor just sets the attribute type to STUN_ATTR_USERNAME.
     *
     * @param attrType  Attribute Type
     * @param attrName  Attribute Name
     */
    StunAttributeStringBase(StunAttrType attrType, const char* const attrName) :
        StunAttribute(attrType, attrName)
    { }

    /**
     * This constructor just sets the attribute type to STUN_ATTR_USERNAME.
     *
     * @param attrType  Attribute Type
     * @param attrName  Attribute Name
     * @param str       The str as std::string.
     */
    StunAttributeStringBase(StunAttrType attrType, const char* const attrName,
                            const String& str) :
        StunAttribute(attrType, attrName),
        str(str)
    {
        QCC_DbgTrace(("StunAttributeStringBase::StunAttributeStringBase(attrType, attrName = %s, string str = %s)", attrName, str.c_str()));
    }

    QStatus Parse(const uint8_t*& buf, size_t& bufSize);
    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const;
#ifndef NDEBUG
    String ToString(void) const;
#endif
    size_t RenderSize(void) const
    {
        return StunAttribute::RenderSize() + (static_cast<size_t>(-AttrSize() & 0x3));
    }
    uint16_t AttrSize(void) const { return str.length(); }
};


#undef QCC_MODULE
#endif
