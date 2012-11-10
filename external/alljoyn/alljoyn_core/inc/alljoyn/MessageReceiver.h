#ifndef _ALLJOYN_MESSAGERECEIVER_H
#define _ALLJOYN_MESSAGERECEIVER_H

#include <qcc/platform.h>

#include <alljoyn/Message.h>

/**
 * @file
 * MessageReceiver is a base class implemented by any class
 * which wishes to receive AllJoyn messages
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
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

namespace ajn {

/**
 * %MessageReceiver is a pure-virtual base class that is implemented by any
 * class that wishes to receive AllJoyn messages from the AllJoyn library.
 *
 * Received messages can be either signals, method_replies or errors.
 */
class MessageReceiver {
  public:
    /** Destructor */
    virtual ~MessageReceiver() { }

    /**
     * MethodHandlers are %MessageReceiver methods which are called by AllJoyn library
     * to forward AllJoyn method_calls to AllJoyn library users.
     *
     * @param member    Method interface member entry.
     * @param message   The received method call message.
     */
    typedef void (MessageReceiver::* MethodHandler)(const InterfaceDescription::Member* member, Message& message);

    /**
     * ReplyHandlers are %MessageReceiver methods which are called by AllJoyn library
     * to forward AllJoyn method_reply and error responses to AllJoyn library users.
     *
     * @param message   The received message.
     * @param context   User-defined context passed to MethodCall and returned upon reply.
     */
    typedef void (MessageReceiver::* ReplyHandler)(Message& message, void* context);

    /**
     * SignalHandlers are %MessageReceiver methods which are called by AllJoyn library
     * to forward AllJoyn received signals to AllJoyn library users.
     *
     * @param member    Method or signal interface member entry.
     * @param srcPath   Object path of signal emitter.
     * @param message   The received message.
     */
    typedef void (MessageReceiver::* SignalHandler)(const InterfaceDescription::Member* member, const char* srcPath, Message& message);

};

}

#endif
