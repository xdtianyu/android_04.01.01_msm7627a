#ifndef _STUNATTRIBUTEUSECANDIDATE_H
#define _STUNATTRIBUTEUSECANDIDATE_H
/**
 * @file
 *
 * This file defines the USE-CANDIDATE STUN message attribute.
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
#error Only include StunAddr/StunAttributeUseCandidate.h in C++ code.
#endif

#include <string>
#include <qcc/platform.h>
#include <StunAttributeBase.h>
#include <types.h>
#include "Status.h"

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"

/**
 * Use Candidate STUN attribute class.
 */
class StunAttributeUseCandidate : public StunAttribute {
  private:

  public:
    /**
     * This constructor just sets the attribute type to STUN_ATTR_USE_CANDIDATE.
     */
    StunAttributeUseCandidate(void) :
        StunAttribute(STUN_ATTR_USE_CANDIDATE, "USE-CANDIDATE") { }

    uint16_t AttrSize(void) const { return 0; /* Attribute has no data */ }
};


#undef QCC_MODULE
#endif
