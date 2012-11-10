/**
 * @file
 * Utility classes for the BlueZ implementation of BT transport.
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
#ifndef _ALLJOYN_BLUEZUTILS_H
#define _ALLJOYN_BLUEZUTILS_H

#include <qcc/platform.h>

#include <qcc/Event.h>
#include <qcc/ManagedObj.h>
#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/String.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/ProxyBusObject.h>

#include "BDAddress.h"
#include "BTTransport.h"

#include <Status.h>



namespace ajn {
namespace bluez {

typedef qcc::ManagedObj<std::vector<qcc::String> > AdvertisedNamesList;


class BTSocketStream : public qcc::SocketStream {
  public:
    BTSocketStream(qcc::SocketFd sock);
    ~BTSocketStream() { if (buffer) delete[] buffer; }
    QStatus PullBytes(void* buf, size_t reqBytes, size_t& actualBytes, uint32_t timeout = qcc::Event::WAIT_FOREVER);
    QStatus PushBytes(const void* buf, size_t numBytes, size_t& numSent);

  private:
    uint8_t* buffer;
    size_t inMtu;
    size_t outMtu;
    size_t offset;
    size_t fill;
};


} // namespace bluez
} // namespace ajn


#endif
