/**
 * @file
 * SessionPortListener is an abstract base class (interface) implemented by users of the
 * AllJoyn API in order to receive session port related event information.
 */

/******************************************************************************
 * Copyright 2009-2011, Qualcomm Innovation Center, Inc.
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
#ifndef _ALLJOYN_SESSIONPORTLISTENER_H
#define _ALLJOYN_SESSIONPORTLISTENER_H

#ifndef __cplusplus
#error Only include SessionPortListener.h in C++ code.
#endif

#include <alljoyn/Session.h>
#include <alljoyn/SessionListener.h>

namespace ajn {

/**
 * Abstract base class implemented by AllJoyn users and called by AllJoyn to inform
 * users of session related events.
 */
class SessionPortListener {
  public:
    /**
     * Virtual destructor for derivable class.
     */
    virtual ~SessionPortListener() { }

    /**
     * Accept or reject an incoming JoinSession request. The session does not exist until this
     * after this function returns.
     *
     * This callback is only used by session creators. Therefore it is only called on listeners
     * passed to BusAttachment::BindSessionPort.
     *
     * @param sessionPort    Session port that was joined.
     * @param joiner         Unique name of potential joiner.
     * @param opts           Session options requested by the joiner.
     * @return   Return true if JoinSession request is accepted. false if rejected.
     */
    virtual bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts) { return false; }

    /**
     * Called by the bus when a session has been successfully joined. The session is now fully up.
     *
     * This callback is only used by session creators. Therefore it is only called on listeners
     * passed to BusAttachment::BindSessionPort.
     *
     * @param sessionPort    Session port that was joined.
     * @param id             Id of session.
     * @param joiner         Unique name of the joiner.
     */
    virtual void SessionJoined(SessionPort sessionPort, SessionId id, const char* joiner) {  }
};

}

#endif
