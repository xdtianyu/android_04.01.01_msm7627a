#ifndef _STUNATTRIBUTEERRORCODE_H
#define _STUNATTRIBUTEERRORCODE_H
/**
 * @file
 *
 * This file defines the ERROR-CODE STUN message attribute.
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
#error Only include StunAttributeBase.h in C++ code.
#endif

#include <string>
#include <qcc/platform.h>
#include <qcc/unicode.h>
#include <StunAttributeStringBase.h>
#include <types.h>
#include "Status.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"


/**
 * Error Code STUN attribute class.
 */
class StunAttributeErrorCode : public StunAttributeStringBase {
  private:
    StunErrorCodes error;       ///< Error code number.

  public:
    /**
     * This constructor just sets the attribute type to STUN_ATTR_ERROR_CODE.
     */
    StunAttributeErrorCode(void) : StunAttributeStringBase(STUN_ATTR_ERROR_CODE, "ERROR-CODE") { }

    /**
     * This constructor sets the attribute type to STUN_ATTR_ERROR_CODE and
     * initializes the error code and reason phrase.
     *
     * @param error     The error code.
     * @param reason    The reason phrase as a std::string.
     */
    StunAttributeErrorCode(StunErrorCodes error, const String& reason) :
        StunAttributeStringBase(STUN_ATTR_ERROR_CODE, "ERROR-CODE", reason),
        error(error)
    { }


    QStatus Parse(const uint8_t*& buf, size_t& bufSize);
    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const;
#if !defined(NDEBUG)
    String ToString(void) const;
#endif
    size_t RenderSize(void) const { return StunAttributeStringBase::RenderSize() + sizeof(uint32_t); }
    uint16_t AttrSize(void) const { return StunAttributeStringBase::AttrSize() + sizeof(uint32_t); }

    /**
     * Retreive the error information.
     *
     * @param error  OUT: A reference to where to copy the error code.
     * @param reason OUT: A reference to where to copy the reason std::string.
     */
    void GetError(StunErrorCodes& error, String& reason) const
    {
        error = this->error;
        GetStr(reason);
    }

    /**
     * Set the error information.
     *
     * @param error  The error code.
     * @param reason A reference to the reason std::string.
     */
    void SetError(StunErrorCodes error, const String& reason)
    {
        this->error = error;
        SetStr(reason);
    }
};

#undef QCC_MODULE
#endif
