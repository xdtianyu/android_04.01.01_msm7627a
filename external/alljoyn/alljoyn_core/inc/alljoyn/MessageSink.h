/**
 * @file
 * Abstract interface implemented by objects that wish to consume Message Bus
 * messages.
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
#ifndef _ALLJOYN_MESSAGESINK_H
#define _ALLJOYN_MESSAGESINK_H

#ifndef __cplusplus
#error Only include MessageSink.h in C++ code.
#endif

#include <qcc/platform.h>

#include <alljoyn/Message.h>

#include <Status.h>

namespace ajn {

/**
 * Abstract interface implemented by objects that wish to consume Message Bus
 * messages.
 */
class MessageSink {
  public:
    /**
     * Virtual destructor for derivable class.
     */
    virtual ~MessageSink() { }

    /**
     * Consume a message bus message.
     *
     * @param msg   Message to be consumed.
     * @return #ER_OK if successful.
     */
    virtual QStatus PushMessage(Message& msg) = 0;
};

}

#endif
