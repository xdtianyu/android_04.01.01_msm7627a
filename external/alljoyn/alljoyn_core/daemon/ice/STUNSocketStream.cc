/**
 * @file STUNSocketStream.cc
 *
 * Sink/Source wrapper for STUN.
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

#include <qcc/platform.h>

#include <qcc/Socket.h>
#include <qcc/Stream.h>
#include <qcc/String.h>

#include <Status.h>
#include <STUNSocketStream.h>

using namespace std;
using namespace qcc;

#define QCC_MODULE "STUN_SOCKET_STREAM"

namespace ajn {

static SocketFd CopySock(const SocketFd& inFd)
{
    SocketFd outFd;
    QStatus status = SocketDup(inFd, outFd);
    return (status == ER_OK) ? outFd : -1;
}

STUNSocketStream::STUNSocketStream(Stun* stunPtr) :
    isConnected(true),
    stunPtr(stunPtr),
    sock(stunPtr->GetSocketFD()),
    sourceEvent(new Event(sock, Event::IO_READ, false)),
    sinkEvent(new Event(sock, Event::IO_WRITE, false)),
    isDetached(false)
{
}

STUNSocketStream::STUNSocketStream(const STUNSocketStream& other) :
    isConnected(other.isConnected),
    stunPtr(other.stunPtr),
    sock(CopySock(other.sock)),
    sourceEvent(new Event(sock, Event::IO_READ, false)),
    sinkEvent(new Event(*sourceEvent, Event::IO_WRITE, false)),
    isDetached(other.isDetached)
{
}

STUNSocketStream STUNSocketStream::operator=(const STUNSocketStream& other)
{
    Close();
    isConnected = other.isConnected;
    stunPtr = other.stunPtr;
    sock = CopySock(other.sock);
    delete sourceEvent;
    sourceEvent = new Event(sock, Event::IO_READ, false);
    delete sinkEvent;
    sinkEvent = new Event(*sourceEvent, Event::IO_WRITE, false);
    isDetached = other.isDetached;
    return *this;
}

STUNSocketStream::~STUNSocketStream()
{
    Close();
    delete sourceEvent;
    delete sinkEvent;
}

void STUNSocketStream::Close()
{
    if (isConnected) {
        if (!isDetached) {
            stunPtr->Shutdown();
        }
        isConnected = false;
    }
    if ((SOCKET_ERROR != sock) && !isDetached) {
        stunPtr->Close();
        sock = SOCKET_ERROR;
    }
}

QStatus STUNSocketStream::PullBytes(void* buf, size_t reqBytes, size_t& actualBytes, uint32_t timeout)
{
    if (!isConnected) {
        return ER_FAIL;
    }
    if (reqBytes == 0) {
        actualBytes = 0;
        return ER_OK;
    }
    QStatus status;
    while (true) {
        status = stunPtr->AppRecv(buf, reqBytes, actualBytes);
        if (ER_WOULDBLOCK == status) {
            status = Event::Wait(*sourceEvent, timeout);
            if (ER_OK != status) {
                break;
            }
        } else {
            break;
        }
    }

    if ((ER_OK == status) && (0 == actualBytes)) {
        /* Other end has closed */
        Close();
        status = ER_SOCK_OTHER_END_CLOSED;
    }
    return status;
}

QStatus STUNSocketStream::PushBytes(const void* buf, size_t numBytes, size_t& numSent)
{
    if (!isConnected) {
        return ER_FAIL;
    }
    if (numBytes == 0) {
        numSent = 0;
        return ER_OK;
    }
    QStatus status;
    while (true) {
        status = stunPtr->AppSend(buf, numBytes, numSent);
        if (ER_WOULDBLOCK == status) {
            status = Event::Wait(*sinkEvent);
            if (ER_OK != status) {
                break;
            }
        } else {
            break;
        }
    }

    return status;
}

} // namespace ajn
