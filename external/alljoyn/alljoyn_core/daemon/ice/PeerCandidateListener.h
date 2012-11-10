#ifndef _PEERCANDIDATELISTENER_H
#define _PEERCANDIDATELISTENER_H

/**
 * @file PeerCandidateListener.h
 *
 */

/******************************************************************************
 * Copyright 2012 Qualcomm Innovation Center, Inc.
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

#include "RendezvousServerInterface.h"

namespace ajn {

/**
 * DiscoveryManager registered listeners must implement this abstract class in order
 * to receive notifications from DiscoveryManager when Peer candidates are available.
 */
class PeerCandidateListener {
  public:

    /**
     * Virtual Destructor
     */
    virtual ~PeerCandidateListener() { };

    /**
     * Notify listener that Peer candidates are available
     */
    virtual void SetPeerCandiates(list<ICECandidates>& candidates, const String& frag, const String& pwd) = 0;
};

} //namespace ajn

#endif
