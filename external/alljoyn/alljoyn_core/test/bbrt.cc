/**
 * @file
 *
 * This file tests AllJoyn use of the DBus wire protocol
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


#include "qcc/platform.h"
#include "qcc/Util.h"
#include "qcc/Debug.h"
#include "Status.h"
#include "qcc/string.h"
#include "qcc/Thread.h"
#include "qcc/Environ.h"
#include "qcc/Socket.h"
#include "qcc/SocketStream.h"
#include "qcc/GUID.h"
#include "alljoyn/AuthMechanism.h"
#include "alljoyn/AuthMechDBusCookieSHA1.h"
#include "alljoyn/AuthMechAnonymous.h"
#include "alljoyn/EndpointAuth.h"
#include "alljoyn/Message.h"
#include "alljoyn/version.h"
#include "alljoyn/Bus.h"

using namespace qcc;
using namespace std;
using namespace ajn;

static const char SockName[] = "@alljoyn";



ThreadReturn STDCALL ServerThread(void* arg)
{
    SocketFd sockfd;
    QStatus status;
    qcc::GUID128 serverGUID;
    Bus bus(true);
    QCC_SyncPrintf("Starting server thread\n");

    status = Socket(QCC_AF_UNIX, QCC_SOCK_STREAM, sockfd);
    if (status == ER_OK) {
        status = Bind(sockfd, SockName);
        if (status == ER_OK) {
            status = Listen(sockfd, 0);
            if (status == ER_OK) {
                SocketFd newSockfd;
                status = Accept(sockfd, newSockfd);
                if (status == ER_OK) {
                    string authName;
                    SocketStream sockStream(newSockfd);
                    EndpointAuth endpoint(bus, sockStream, serverGUID, "test");
                    status = endpoint.Establish(authName, 5);
                }
            }
        }
    }
    QCC_SyncPrintf("Server thread %s\n", QCC_StatusText(status));
    return (ThreadReturn) 0;
}


ThreadReturn STDCALL ClientThread(void* arg)
{
    QStatus status;
    string authName;
    Bus bus(false);

    QCC_SyncPrintf("Starting client thread\n");
    Stream* stream = reinterpret_cast<Stream*>(arg);
    EndpointAuth endpoint(bus, *stream);
    status = endpoint.Establish(authName, 5);
    if (status == ER_OK) {
        QCC_SyncPrintf("Established connection using %s\n", authName.c_str());
    }
    QCC_SyncPrintf("Leaving client thread %s\n", QCC_StatusText(status));
    return (ThreadReturn) 0;
}


int main(int argc, char** argv)
{
    // Environ *env = Environ::GetAppEnviron();
    SocketStream sockStream(QCC_AF_UNIX, QCC_SOCK_STREAM);
    QStatus status;

    printf("AllJoyn Library version: %s\n", alljoyn::GetVersion());
    printf("AllJoyn Library build info: %s\n", alljoyn::GetBuildInfo());

    /*
     * Register the authentication methods
     */
    AuthManager::RegisterMechanism(AuthMechDBusCookieSHA1::Instantiator, AuthMechDBusCookieSHA1::AuthName());
    AuthManager::RegisterMechanism(AuthMechAnonymous::Instantiator, AuthMechAnonymous::AuthName());

    Thread srvThread("server", ServerThread);
    srvThread.Start(NULL);


    string busAddr = SockName;
    status = sockStream.Connect(busAddr);
    if (status != ER_OK) {
        QCC_SyncPrintf("Error: failed to connect socket %s", QCC_StatusText(status));
        return -1;
    }
    QCC_SyncPrintf("Connected to %s\n", busAddr.c_str());

    ClientThread(&sockStream);

    return 0;
}
