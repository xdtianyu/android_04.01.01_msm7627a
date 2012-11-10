#ifndef _STUNACTIVITY_H
#define _STUNACTIVITY_H
/**
 * @file StunActivity.h
 *
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

#include <qcc/platform.h>

#include <StunRetry.h>
#include <ICECandidate.h>

/** @internal */
#define QCC_MODULE "STUNACTIVITY"

namespace ajn {

struct StunActivity {

    StunActivity(Stun* stun);

    ~StunActivity();

    void SetCandidate(const ICECandidate& candidate);

    Stun* stun;
    ICECandidate candidate;
    Retransmit retransmit;    // Used by host candidate during gathering to bind/allocate
                              // from STUN/TURN server.
                              // Used by reflexive candidate just to keep time of last indication.
                              // Used by relayed candidate to refresh Allocations from TURN server.
                              // Used by relayed candidate to refresh Permissions from TURN server.
};

} //namespace ajn

#undef QCC_MODULE

#endif
