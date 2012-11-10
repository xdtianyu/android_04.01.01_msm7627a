#ifndef _STUNATTRIBUTEUSERNAME_H
#define _STUNATTRIBUTEUSERNAME_H
/**
 * @file
 *
 * This file defines the USERNAME STUN message attribute.
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
 * Username STUN attribute class.
 */
class StunAttributeUsername : public StunAttributeStringBase {
  public:
    /**
     * This constructor just sets the attribute type to STUN_ATTR_USERNAME.
     */
    StunAttributeUsername(void) : StunAttributeStringBase(STUN_ATTR_USERNAME, "USERNAME") { }

    /**
     * This constructor just sets the attribute type to STUN_ATTR_USERNAME.
     *
     * @param username  The username as std::string.
     */
    StunAttributeUsername(const String& username) :
        StunAttributeStringBase(STUN_ATTR_USERNAME, "USERNAME", username)
    { }

    /**
     * Retrieves the parsed UTF-8 username.
     *
     * @param username OUT: A reference to where to copy the username.
     */
    void GetUsername(String& username) const { GetStr(username); }

    /**
     * Sets the UTF-8 username.
     *
     * @param username A reference the username.
     */
    void SetUsername(const String& username) { SetStr(username); }
};


#undef QCC_MODULE
#endif
