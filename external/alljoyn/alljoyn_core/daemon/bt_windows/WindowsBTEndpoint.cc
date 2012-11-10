/**
 * @file
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
#include "WindowsBTEndpoint.h"

#define QCC_MODULE "ALLJOYN_BT"

namespace ajn {

WindowsBTEndpoint::~WindowsBTEndpoint()
{
    QCC_DbgTrace(("WindowsBTEndpoint::~WindowsBTEndpoint()"));

    ajn::BTTransport::BTAccessor* accessor = btStream.GetAccessor();

    if (accessor) {
        accessor->EndPointsRemove(this);
    }

    if (connectionCompleteEvent) {
        ::CloseHandle(connectionCompleteEvent);
        connectionCompleteEvent = NULL;
    }  else {
        QCC_LogError(ER_INIT_FAILED, ("connectionCompleteEvent is NULL!"));
    }

    connectionStatus = ER_FAIL;
}

QStatus WindowsBTEndpoint::WaitForConnectionComplete(bool incoming)
{
    QCC_DbgTrace(("WindowsBTEndpoint::WaitForConnectionComplete(address = 0x%012I64X)",
                  GetRemoteDeviceAddress()));

    connectionStatus = ER_INIT_FAILED;

    if (connectionCompleteEvent) {
        const DWORD waitTimeInMilliseconds = 30000;
        DWORD waitStatus = WaitForSingleObject(connectionCompleteEvent, waitTimeInMilliseconds);

        switch (waitStatus) {
        case WAIT_OBJECT_0:
            if (incoming) {
                uint8_t nul = 255;
                size_t recvd;
                connectionStatus = btStream.PullBytes(&nul, sizeof(nul), recvd, waitTimeInMilliseconds);
                if ((connectionStatus != ER_OK) || (nul != 0)) {
                    connectionStatus = (connectionStatus == ER_OK) ? ER_FAIL : connectionStatus;
                    QCC_LogError(connectionStatus, ("Did not receive initial nul byte"));
                }
            } else {
                uint8_t nul = 0;
                size_t sent;
                connectionStatus = btStream.PushBytes(&nul, sizeof(nul), sent);
            }
            break;

        case WAIT_TIMEOUT:
            QCC_DbgPrintf(("WaitForConnectionComplete() timeout! (%u mS)", waitTimeInMilliseconds));
            connectionStatus = ER_TIMEOUT;
            break;

        default:
            connectionStatus = ER_FAIL;
            break;
        }
    } else {
        QCC_LogError(connectionStatus, ("connectionCompleteEvent is NULL!"));
    }

    return connectionStatus;
}

void WindowsBTEndpoint::SetConnectionComplete(QStatus status)
{
    QCC_DbgTrace(("WindowsBTEndpoint::SetConnectionComplete(handle = %p, status = %s)",
                  GetChannelHandle(), QCC_StatusText(status)));

    connectionStatus = status;

    if (GetChannelHandle()) {
        if (connectionCompleteEvent) {
            ::SetEvent(connectionCompleteEvent);
        } else {
            QCC_LogError(ER_INIT_FAILED, ("connectionCompleteEvent is NULL!"));
        }
    } else {
        QCC_LogError(ER_INIT_FAILED, ("connectionCompleteEvent orphaned (channel is NULL)"));
    }
}



} // namespace ajn
