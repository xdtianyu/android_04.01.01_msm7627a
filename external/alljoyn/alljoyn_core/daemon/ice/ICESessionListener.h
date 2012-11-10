#ifndef _ICESESSIONLISTENER_H
#define _ICESESSIONLISTENER_H

/**
 * @file ICESessionListener.h
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

#include "ICESession.h"

namespace ajn {

// Forward Declaration
class ICESession;

/**
 * ICESession registered listeners must implement this abstract class in order
 * to receive notifications from ICESession.
 */
class ICESessionListener {
  public:

    /**
     * Virtual Destructor
     */
    virtual ~ICESessionListener() { };

    /**
     * Notify listener that ICESession has changed state.
     * @param session    Session whose state has changed.
     */

    virtual void ICESessionChanged(ICESession* session) = 0;
};

} //namespace ajn

#endif
