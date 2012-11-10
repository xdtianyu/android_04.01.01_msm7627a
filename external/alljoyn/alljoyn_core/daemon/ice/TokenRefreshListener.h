#ifndef _TOKENREFRESHLISTENER_H
#define _TOKENREFRESHLISTENER_H

/**
 * @file TokenRefreshListener.h
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
 * to receive notifications from DiscoveryManager when new refreshed tokens are available.
 */
class TokenRefreshListener {
  public:

    /**
     * Virtual Destructor
     */
    virtual ~TokenRefreshListener() { };

    /**
     * Notify listener that new tokens are available
     */

    virtual void SetTokens(String newAcct, String newPwd, uint32_t recvtime, uint32_t expTime) = 0;
};

} //namespace ajn

#endif
