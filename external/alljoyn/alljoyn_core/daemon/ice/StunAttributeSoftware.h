#ifndef _STUNATTRIBUTESOFTWARE_H
#define _STUNATTRIBUTESOFTWARE_H
/**
 * @file
 *
 * This file defines the SOFTWARE STUN message attribute.
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
#error Only include StunAttributeSoftware.h in C++ code.
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
 * Software STUN attribute class.
 */
class StunAttributeSoftware : public StunAttributeStringBase {
  public:
    /**
     * This constructor just sets the attribute type to STUN_ATTR_SOFTWARE.
     */
    StunAttributeSoftware(void) : StunAttributeStringBase(STUN_ATTR_SOFTWARE, "SOFTWARE") { }

    /**
     * This constructor just sets the attribute type to STUN_ATTR_SOFTWARE.
     *
     * @param software  The software description as std::string.
     */
    StunAttributeSoftware(const String& software) :
        StunAttributeStringBase(STUN_ATTR_SOFTWARE, "SOFTWARE", software)
    { }

    /**
     * Retrieve the software.
     *
     * @param software  OUT: A copy the software std::string.
     */
    void GetSoftware(String& software) const { GetStr(software); }

    /**
     * Set the software.
     *
     * @param software A reference to the software std::string.
     */
    void SetSoftware(const String& software) { SetStr(software); }
};


#undef QCC_MODULE
#endif
