/**
 * @file StunActivity.cc
 *
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

#include <ICECandidate.h>
#include <StunActivity.h>


/** @internal */
#define QCC_MODULE "STUNACTIVITY"

namespace ajn {

StunActivity::StunActivity(Stun* stun) :
    stun(stun),
    candidate(),
    retransmit()
{
}

StunActivity::~StunActivity()
{

}

void StunActivity::SetCandidate(const ICECandidate& candidate)
{
    QCC_DbgTrace(("%s(%p): ", __FUNCTION__, this));

    this->candidate = candidate;

    retransmit = Retransmit();

    switch (candidate->GetType()) {
    case _ICECandidate::Host_Candidate:
        // retransmit will maintain count of retries and timeouts as we perform the
        // (one and only) Bind/Allocate request to the STUN/TURN server for this local interface
        break;

    case _ICECandidate::ServerReflexive_Candidate:
    case _ICECandidate::PeerReflexive_Candidate:
        // Reflexive candidates use 'retransmit' only for NAT keepalive time stamping.
        // They do NOT timeout responses, nor perform retransmits.
        retransmit.RecordKeepaliveTime();  // stamp time now
        break;

    case _ICECandidate::Relayed_Candidate:
        // For refreshing Allocations/CreatePermissions on the TURN server.
        retransmit.RecordKeepaliveTime();  // stamp time now
        break;

    default:
        assert(false);
        break;
    }
}

} //namespace ajn

