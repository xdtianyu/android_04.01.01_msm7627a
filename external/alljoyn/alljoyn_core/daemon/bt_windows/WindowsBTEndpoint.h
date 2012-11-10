/**
 * @file
 * BTEndpoint declaration for Windows
 * BTEndpoint implementation for Windows.
 */

/******************************************************************************
 * Copyright 2011, Qualcomm Innovation Center, Inc.
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
#ifndef _ALLJOYN_WINDOWSBTENDPOINT_H
#define _ALLJOYN_WINDOWSBTENDPOINT_H

#include <qcc/platform.h>

#include "WindowsBTStream.h"
#include <qcc/SocketStream.h>

#include "BTEndpoint.h"

#include <alljoyn/BusAttachment.h>

namespace ajn {

class WindowsBTEndpoint : public BTEndpoint {
  public:

    /**
     * Windows Bluetooth endpoint constructor
     *
     * @param bus
     * @param incoming
     * @param node
     * @param accessor
     * @param address
     */
    WindowsBTEndpoint(BusAttachment& bus,
                      bool incoming,
                      const BTNodeInfo& node,
                      ajn::BTTransport::BTAccessor* accessor,
                      BTH_ADDR address,
                      const BTBusAddress& redirect) :
        BTEndpoint(bus, incoming, btStream, node, redirect),
        connectionStatus(ER_FAIL),
        btStream(address, accessor)
    {
        connectionCompleteEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    /**
     * Windows Bluetooth endpoint destructor
     */
    ~WindowsBTEndpoint();

    /**
     * Get the channel handle associated with this endpoint.
     * @return The channel handle.
     */
    L2CAP_CHANNEL_HANDLE GetChannelHandle() const
    {
        return btStream.GetChannelHandle();
    }

    /**
     * Set the channel handle for this endpoint.
     *
     * @param channel The channel handle associated with this endpoint.
     */
    void SetChannelHandle(L2CAP_CHANNEL_HANDLE channel)
    {
        btStream.SetChannelHandle(channel);
    }

    /**
     * Get the bluetooth address for this endpoint.
     *
     * @return The address.
     */
    BTH_ADDR GetRemoteDeviceAddress() const
    {
        return btStream.GetRemoteDeviceAddress();
    }

    /**
     * Set number of bytes waiting in the kernel buffer.
     *
     * @param bytesWaiting The number of bytes.
     */
    void SetSourceBytesWaiting(size_t bytesWaiting, QStatus status)
    {
        connectionStatus = status;
        btStream.SetSourceBytesWaiting(bytesWaiting, status);
    }

    /**
     * Wait for the kernel to indicate the connection attempt has been completed.
     *
     * @return ER_OK if successful, ER_TIMEOUT the wait was not successful, or ER_FAIL for
     * other failures while waiting for the completion event to fire.
     * To determine the connection status after the completion event has fired call
     * GetConnectionStatus().
     */
    QStatus WaitForConnectionComplete(bool incoming);

    /**
     * Called via a message from the kernel to indicate the connection attempt has been
     * completed.
     *
     * @param status The status of the connection attempt.
     */
    void SetConnectionComplete(QStatus status);

    /**
     * Get the connection status for this endpoint.
     *
     * @return The status.
     */
    QStatus GetConnectionStatus(void) const { return connectionStatus; }

    /**
     * Set the pointer in the stream to the BTAccessor which created this endpoint to NULL.
     * This is needed when the endpoint has not yet been deleted but the BTAccessor is
     * in the process of being deleted.
     */
    void OrphanEndpoint(void) { btStream.OrphanStream(); }

  private:
    ajn::WindowsBTStream btStream;
    HANDLE connectionCompleteEvent;    // Used to signal the channel connection is completed.
    QStatus connectionStatus;
};

} // namespace ajn

#endif
