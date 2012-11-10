/**
 * @file
 * * This file implements the org.alljoyn.Bus and org.alljoyn.Daemon interfaces
 */

/******************************************************************************
 * Copyright 2010-2012, Qualcomm Innovation Center, Inc.
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

#include <algorithm>
#include <assert.h>
#include <limits>
#include <map>
#include <set>
#include <vector>
#include <errno.h>

#include <qcc/Debug.h>
#include <qcc/Logger.h>
#include <qcc/ManagedObj.h>
#include <qcc/String.h>
#include <qcc/Thread.h>
#include <qcc/Util.h>
#include <qcc/SocketStream.h>
#include <qcc/StreamPump.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/Message.h>
#include <alljoyn/MessageReceiver.h>
#include <alljoyn/ProxyBusObject.h>

#include "DaemonRouter.h"
#include "AllJoynObj.h"
#include "TransportList.h"
#include "BusUtil.h"
#include "SessionInternal.h"
#include "BusController.h"

#define QCC_MODULE "ALLJOYN_OBJ"

using namespace std;
using namespace qcc;

namespace ajn {

int AllJoynObj::JoinSessionThread::jstCount = 0;

void AllJoynObj::AcquireLocks()
{
    /*
     * Locks must be acquired in the following order since tha caller of
     * this method may already have the name table lock
     */
    router.LockNameTable();
    stateLock.Lock(MUTEX_CONTEXT);
}

void AllJoynObj::ReleaseLocks()
{
    stateLock.Unlock(MUTEX_CONTEXT);
    router.UnlockNameTable();
}

AllJoynObj::AllJoynObj(Bus& bus, BusController* busController) :
    BusObject(bus, org::alljoyn::Bus::ObjectPath, false),
    bus(bus),
    router(reinterpret_cast<DaemonRouter&>(bus.GetInternal().GetRouter())),
    foundNameSignal(NULL),
    lostAdvNameSignal(NULL),
    sessionLostSignal(NULL),
    mpSessionChangedSignal(NULL),
    mpSessionJoinedSignal(NULL),
    guid(bus.GetInternal().GetGlobalGUID()),
    exchangeNamesSignal(NULL),
    detachSessionSignal(NULL),
    nameMapReaper(this),
    isStopping(false),
    busController(busController)
{
}

AllJoynObj::~AllJoynObj()
{
    bus.UnregisterBusObject(*this);

    /* Wait for any outstanding JoinSessionThreads */
    joinSessionThreadsLock.Lock(MUTEX_CONTEXT);
    isStopping = true;
    vector<JoinSessionThread*>::iterator it = joinSessionThreads.begin();
    while (it != joinSessionThreads.end()) {
        (*it)->Stop();
        ++it;
    }
    while (!joinSessionThreads.empty()) {
        joinSessionThreadsLock.Unlock(MUTEX_CONTEXT);
        qcc::Sleep(50);
        joinSessionThreadsLock.Lock(MUTEX_CONTEXT);
    }
    joinSessionThreadsLock.Unlock(MUTEX_CONTEXT);
}

QStatus AllJoynObj::Init()
{
    QStatus status;

    /* Make this object implement org.alljoyn.Bus */
    const InterfaceDescription* alljoynIntf = bus.GetInterface(org::alljoyn::Bus::InterfaceName);
    if (!alljoynIntf) {
        status = ER_BUS_NO_SUCH_INTERFACE;
        QCC_LogError(status, ("Failed to get %s interface", org::alljoyn::Bus::InterfaceName));
        return status;
    }

    /* Hook up the methods to their handlers */
    const MethodEntry methodEntries[] = {
        { alljoynIntf->GetMember("AdvertiseName"),            static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::AdvertiseName) },
        { alljoynIntf->GetMember("CancelAdvertiseName"),      static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::CancelAdvertiseName) },
        { alljoynIntf->GetMember("FindAdvertisedName"),       static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::FindAdvertisedName) },
        { alljoynIntf->GetMember("CancelFindAdvertisedName"), static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::CancelFindAdvertisedName) },
        { alljoynIntf->GetMember("BindSessionPort"),          static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::BindSessionPort) },
        { alljoynIntf->GetMember("UnbindSessionPort"),        static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::UnbindSessionPort) },
        { alljoynIntf->GetMember("JoinSession"),              static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::JoinSession) },
        { alljoynIntf->GetMember("LeaveSession"),             static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::LeaveSession) },
        { alljoynIntf->GetMember("GetSessionFd"),             static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::GetSessionFd) },
        { alljoynIntf->GetMember("SetLinkTimeout"),           static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::SetLinkTimeout) },
        { alljoynIntf->GetMember("AliasUnixUser"),            static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::AliasUnixUser) }
    };

    AddInterface(*alljoynIntf);
    status = AddMethodHandlers(methodEntries, ArraySize(methodEntries));
    if (ER_OK != status) {
        QCC_LogError(status, ("AddMethods for %s failed", org::alljoyn::Bus::InterfaceName));
    }

    foundNameSignal = alljoynIntf->GetMember("FoundAdvertisedName");
    lostAdvNameSignal = alljoynIntf->GetMember("LostAdvertisedName");
    sessionLostSignal = alljoynIntf->GetMember("SessionLost");
    mpSessionChangedSignal = alljoynIntf->GetMember("MPSessionChanged");

    const InterfaceDescription* busSessionIntf = bus.GetInterface(org::alljoyn::Bus::Peer::Session::InterfaceName);
    if (!busSessionIntf) {
        status = ER_BUS_NO_SUCH_INTERFACE;
        QCC_LogError(status, ("Failed to get %s interface", org::alljoyn::Bus::Peer::Session::InterfaceName));
        return status;
    }

    mpSessionJoinedSignal = busSessionIntf->GetMember("SessionJoined");

    /* Make this object implement org.alljoyn.Daemon */
    daemonIface = bus.GetInterface(org::alljoyn::Daemon::InterfaceName);
    if (!daemonIface) {
        status = ER_BUS_NO_SUCH_INTERFACE;
        QCC_LogError(status, ("Failed to get %s interface", org::alljoyn::Daemon::InterfaceName));
        return status;
    }

    /* Hook up the methods to their handlers */
    const MethodEntry daemonMethodEntries[] = {
        { daemonIface->GetMember("AttachSession"),     static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::AttachSession) },
        { daemonIface->GetMember("GetSessionInfo"),    static_cast<MessageReceiver::MethodHandler>(&AllJoynObj::GetSessionInfo) }
    };
    AddInterface(*daemonIface);
    status = AddMethodHandlers(daemonMethodEntries, ArraySize(daemonMethodEntries));
    if (ER_OK != status) {
        QCC_LogError(status, ("AddMethods for %s failed", org::alljoyn::Daemon::InterfaceName));
    }

    exchangeNamesSignal = daemonIface->GetMember("ExchangeNames");
    assert(exchangeNamesSignal);
    detachSessionSignal = daemonIface->GetMember("DetachSession");
    assert(detachSessionSignal);

    /* Register a signal handler for ExchangeNames */
    if (ER_OK == status) {
        status = bus.RegisterSignalHandler(this,
                                           static_cast<MessageReceiver::SignalHandler>(&AllJoynObj::ExchangeNamesSignalHandler),
                                           daemonIface->GetMember("ExchangeNames"),
                                           NULL);
        if (status != ER_OK) {
            QCC_LogError(status, ("Failed to register ExchangeNamesSignalHandler"));
        }
    }

    /* Register a signal handler for NameChanged bus-to-bus signal */
    if (ER_OK == status) {
        status = bus.RegisterSignalHandler(this,
                                           static_cast<MessageReceiver::SignalHandler>(&AllJoynObj::NameChangedSignalHandler),
                                           daemonIface->GetMember("NameChanged"),
                                           NULL);
        if (status != ER_OK) {
            QCC_LogError(status, ("Failed to register NameChangedSignalHandler"));
        }
    }

    /* Register a signal handler for DetachSession bus-to-bus signal */
    if (ER_OK == status) {
        status = bus.RegisterSignalHandler(this,
                                           static_cast<MessageReceiver::SignalHandler>(&AllJoynObj::DetachSessionSignalHandler),
                                           daemonIface->GetMember("DetachSession"),
                                           NULL);
        if (status != ER_OK) {
            QCC_LogError(status, ("Failed to register DetachSessionSignalHandler"));
        }
    }


    /* Register a name table listener */
    router.AddBusNameListener(this);

    /* Register as a listener for all the remote transports */
    if (ER_OK == status) {
        TransportList& transList = bus.GetInternal().GetTransportList();
        status = transList.RegisterListener(this);
    }

    /* Start the name reaper */
    if (ER_OK == status) {
        status = nameMapReaper.Start();
    }

    if (ER_OK == status) {
        status = bus.RegisterBusObject(*this);
    }

    return status;
}

void AllJoynObj::ObjectRegistered(void)
{
    QStatus status;

    /* Acquire org.alljoyn.Bus name */
    uint32_t disposition = DBUS_REQUEST_NAME_REPLY_EXISTS;
    status = router.AddAlias(org::alljoyn::Bus::WellKnownName,
                             bus.GetInternal().GetLocalEndpoint().GetUniqueName(),
                             DBUS_NAME_FLAG_DO_NOT_QUEUE,
                             disposition,
                             NULL,
                             NULL);
    if ((ER_OK != status) || (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != disposition)) {
        status = (ER_OK == status) ? ER_FAIL : status;
        QCC_LogError(status, ("Failed to register well-known name \"%s\" (disposition=%d)", org::alljoyn::Bus::WellKnownName, disposition));
    }

    /* Acquire org.alljoyn.Daemon name */
    disposition = DBUS_REQUEST_NAME_REPLY_EXISTS;
    status = router.AddAlias(org::alljoyn::Daemon::WellKnownName,
                             bus.GetInternal().GetLocalEndpoint().GetUniqueName(),
                             DBUS_NAME_FLAG_DO_NOT_QUEUE,
                             disposition,
                             NULL,
                             NULL);
    if ((ER_OK != status) || (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != disposition)) {
        status = (ER_OK == status) ? ER_FAIL : status;
        QCC_LogError(status, ("Failed to register well-known name \"%s\" (disposition=%d)", org::alljoyn::Daemon::WellKnownName, disposition));
    }

    /* Add a broadcast Rule rule to receive org.alljoyn.Daemon signals */
    if (status == ER_OK) {
        status = bus.AddMatch("type='signal',interface='org.alljoyn.Daemon'");
        if (status != ER_OK) {
            QCC_LogError(status, ("Failed to add match rule for org.alljoyn.Daemon"));
        }
    }

    if (status == ER_OK) {
        /* Must call base class */
        BusObject::ObjectRegistered();

        /* Notify parent */
        busController->ObjectRegistered(this);
    }
}

QStatus AllJoynObj::CheckTransportsPermission(const qcc::String& sender, TransportMask& transports, const char* callerName)
{
    QStatus status = ER_OK;
#if defined(QCC_OS_ANDROID)
    AcquireLocks();
    BusEndpoint* srcEp = router.FindEndpoint(sender);
    uint32_t uid = srcEp ? srcEp->GetUserId() : -1;
    if (srcEp != NULL) {
        if (transports & TRANSPORT_BLUETOOTH && (uid != static_cast<uint32_t>(-1))) {
            bool allowed = PermissionDB::GetDB().IsBluetoothAllowed(uid);
            if (!allowed) {
                transports ^= TRANSPORT_BLUETOOTH;
                QCC_LogError(ER_ALLJOYN_ACCESS_PERMISSION_WARNING, ("AllJoynObj::%s() WARNING: No permission to use Bluetooth", (callerName == NULL) ? "" : callerName));
            }
        }
        if (transports & TRANSPORT_WLAN && (uid != static_cast<uint32_t>(-1))) {
            bool allowed = PermissionDB::GetDB().IsWifiAllowed(uid);
            if (!allowed) {
                transports ^= TRANSPORT_WLAN;
                QCC_LogError(ER_ALLJOYN_ACCESS_PERMISSION_WARNING, ("AllJoynObj::%s() WARNING: No permission to use Wifi", ((callerName == NULL) ? "" : callerName)));
            }
        }
        if (transports & TRANSPORT_ICE && (uid != static_cast<uint32_t>(-1))) {
            bool allowed = PermissionDB::GetDB().IsWifiAllowed(uid);
            if (!allowed) {
                transports ^= TRANSPORT_ICE;
                QCC_LogError(ER_ALLJOYN_ACCESS_PERMISSION_WARNING, ("AllJoynObj::%s() WARNING: No permission to use Wifi for ICE", ((callerName == NULL) ? "" : callerName)));
            }
        }
        if (transports == 0) {
            status = ER_BUS_NO_TRANSPORTS;
        }
    } else {
        status = ER_BUS_NO_ENDPOINT;
        QCC_LogError(ER_BUS_NO_ENDPOINT, ("AllJoynObj::CheckTransportsPermission No Bus Endpoint found for Sender %s", sender.c_str()));
    }
    ReleaseLocks();
#endif
    return status;
}

void AllJoynObj::BindSessionPort(const InterfaceDescription::Member* member, Message& msg)
{
    uint32_t replyCode = ALLJOYN_BINDSESSIONPORT_REPLY_SUCCESS;
    size_t numArgs;
    const MsgArg* args;
    SessionOpts opts;

    msg->GetArgs(numArgs, args);
    SessionPort sessionPort = args[0].v_uint16;
    QStatus status = GetSessionOpts(args[1], opts);

    /* Get the sender */
    String sender = msg->GetSender();

    if (status == ER_OK) {
        status = CheckTransportsPermission(sender, opts.transports, "BindSessionPort");
    }

    if (status != ER_OK) {
        QCC_DbgTrace(("AllJoynObj::BindSessionPort(<bad args>) from %s", sender.c_str()));
        replyCode = ALLJOYN_BINDSESSIONPORT_REPLY_FAILED;
    } else {
        QCC_DbgTrace(("AllJoynObj::BindSession(%s, %d, %s, <%x, %x, %x>)", sender.c_str(), sessionPort,
                      opts.isMultipoint ? "true" : "false", opts.traffic, opts.proximity, opts.transports));

        /* Validate some Session options */
        if ((opts.traffic == SessionOpts::TRAFFIC_RAW_UNRELIABLE) ||
            ((opts.traffic == SessionOpts::TRAFFIC_RAW_RELIABLE) && opts.isMultipoint)) {
            replyCode = ALLJOYN_BINDSESSIONPORT_REPLY_INVALID_OPTS;
        }
    }

    if (replyCode == ALLJOYN_BINDSESSIONPORT_REPLY_SUCCESS) {
        /* Assign or check uniqueness of sessionPort */
        AcquireLocks();
        if (sessionPort == SESSION_PORT_ANY) {
            sessionPort = 9999;
            while (++sessionPort) {
                SessionMapType::iterator it = SessionMapLowerBound(sender, 0);
                while ((it != sessionMap.end()) && (it->first.first == sender)) {
                    if (it->second.sessionPort == sessionPort) {
                        break;
                    }
                    ++it;
                }
                /* If no existing sessionMapEntry for sessionPort, then we are done */
                if ((it == sessionMap.end()) || (it->first.first != sender)) {
                    break;
                }
            }
            if (sessionPort == 0) {
                replyCode = ALLJOYN_BINDSESSIONPORT_REPLY_FAILED;
            }
        } else {
            SessionMapType::iterator it = SessionMapLowerBound(sender, 0);
            while ((it != sessionMap.end()) && (it->first.first == sender) && (it->first.second == 0)) {
                if (it->second.sessionPort == sessionPort) {
                    replyCode = ALLJOYN_BINDSESSIONPORT_REPLY_ALREADY_EXISTS;
                    break;
                }
                ++it;
            }
        }

        if (replyCode == ALLJOYN_BINDSESSIONPORT_REPLY_SUCCESS) {
            /* Assign a session id and store the session information */
            SessionMapEntry entry;
            entry.sessionHost = sender;
            entry.sessionPort = sessionPort;
            entry.endpointName = sender;
            entry.fd = -1;
            entry.streamingEp = NULL;
            entry.opts = opts;
            entry.id = 0;
            SessionMapInsert(entry);
        }
        ReleaseLocks();
    }

    /* Reply to request */
    MsgArg replyArgs[2];
    replyArgs[0].Set("u", replyCode);
    replyArgs[1].Set("q", sessionPort);
    status = MethodReply(msg, replyArgs, ArraySize(replyArgs));
    QCC_DbgPrintf(("AllJoynObj::BindSessionPort(%s, %d) returned %d (status=%s)", sender.c_str(), sessionPort, replyCode, QCC_StatusText(status)));

    /* Log error if reply could not be sent */
    if (ER_OK != status) {
        QCC_LogError(status, ("Failed to respond to org.alljoyn.Bus.BindSessionPort"));
    }
}

void AllJoynObj::UnbindSessionPort(const InterfaceDescription::Member* member, Message& msg)
{
    uint32_t replyCode = ALLJOYN_UNBINDSESSIONPORT_REPLY_FAILED;
    size_t numArgs;
    const MsgArg* args;
    SessionOpts opts;

    msg->GetArgs(numArgs, args);
    SessionPort sessionPort = args[0].v_uint16;

    QCC_DbgTrace(("AllJoynObj::UnbindSession(%d)", sessionPort));

    /* Remove session map entry */
    String sender = msg->GetSender();
    AcquireLocks();
    SessionMapType::iterator it = SessionMapLowerBound(sender, 0);
    while ((it != sessionMap.end()) && (it->first.first == sender) && (it->first.second == 0)) {
        if (it->second.sessionPort == sessionPort) {
            sessionMap.erase(it);
            replyCode = ALLJOYN_UNBINDSESSIONPORT_REPLY_SUCCESS;
            break;
        }
        ++it;
    }
    ReleaseLocks();

    /* Reply to request */
    MsgArg replyArgs[1];
    replyArgs[0].Set("u", replyCode);
    QStatus status = MethodReply(msg, replyArgs, ArraySize(replyArgs));
    QCC_DbgPrintf(("AllJoynObj::UnbindSessionPort(%s, %d) returned %d (status=%s)", sender.c_str(), sessionPort, replyCode, QCC_StatusText(status)));

    /* Log error if reply could not be sent */
    if (ER_OK != status) {
        QCC_LogError(status, ("Failed to respond to org.alljoyn.Bus.UnbindSessionPort"));
    }
}

ThreadReturn STDCALL AllJoynObj::JoinSessionThread::Run(void* arg)
{
    if (isJoin) {
        return RunJoin();
    } else {
        return RunAttach();
    }
}

ThreadReturn STDCALL AllJoynObj::JoinSessionThread::RunJoin()
{
    uint32_t replyCode = ALLJOYN_JOINSESSION_REPLY_SUCCESS;
    SessionId id = 0;
    SessionOpts optsOut(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, 0);
    size_t numArgs;
    const MsgArg* args;
    SessionMapEntry sme;
    String b2bEpName;
    String sender = msg->GetSender();
    String vSessionEpName;

    /* Parse the message args */
    msg->GetArgs(numArgs, args);
    const char* sessionHost = NULL;
    SessionPort sessionPort;
    SessionOpts optsIn;
    QStatus status = MsgArg::Get(args, 2, "sq", &sessionHost, &sessionPort);

    if (status == ER_OK) {
        status = GetSessionOpts(args[2], optsIn);
    }

    if (status == ER_OK) {
        status = ajObj.CheckTransportsPermission(sender, optsIn.transports, "JoinSessionThread.Run");
    }

    ajObj.AcquireLocks();

    // do not let a session creator join itself
    SessionMapType::iterator it = ajObj.SessionMapLowerBound(sender, 0);
    BusEndpoint* hostEp = ajObj.router.FindEndpoint(sessionHost);
    if (hostEp != NULL) {
        while ((it != ajObj.sessionMap.end()) && (it->first.first == sender) && (it->first.second == 0)) {
            BusEndpoint* sessionEp = ajObj.router.FindEndpoint(it->second.sessionHost);
            if (hostEp == sessionEp) {
                QCC_DbgTrace(("JoinSession(): cannot join your own session"));
                replyCode = ALLJOYN_JOINSESSION_REPLY_ALREADY_JOINED;
                break;
            }
            ++it;
        }
    }


    if (status != ER_OK) {
        if (replyCode != ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
            replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
            QCC_DbgTrace(("JoinSession(<bad_args>"));
        }
    } else {
        QCC_DbgTrace(("JoinSession(%d, <%u, 0x%x, 0x%x>)", sessionPort, optsIn.traffic, optsIn.proximity, optsIn.transports));

        /* Decide how to proceed based on the session endpoint existence/type */
        RemoteEndpoint* b2bEp = NULL;
        BusEndpoint* ep = sessionHost ? ajObj.router.FindEndpoint(sessionHost) : NULL;
        VirtualEndpoint* vSessionEp = (ep && (ep->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) ? static_cast<VirtualEndpoint*>(ep) : NULL;
        BusEndpoint* rSessionEp = (ep && ((ep->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_REMOTE) ||
                                          (ep->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_NULL))) ? ep : NULL;

        if (rSessionEp) {
            /* Session is with another locally connected attachment */

            /* Find creator in session map */
            String creatorName = rSessionEp->GetUniqueName();
            bool foundSessionMapEntry = false;
            SessionMapType::iterator sit = ajObj.SessionMapLowerBound(creatorName, 0);
            while ((sit != ajObj.sessionMap.end()) && (creatorName == sit->first.first)) {
                if ((sit->first.second == 0) && (sit->second.sessionPort == sessionPort)) {
                    sme = sit->second;
                    foundSessionMapEntry = true;
                    if (!sme.opts.isMultipoint) {
                        break;
                    }
                } else if ((sit->first.second != 0) && (sit->second.sessionPort == sessionPort)) {
                    /* Check if this joiner has already joined and reject in that case */
                    vector<String>::iterator mit = sit->second.memberNames.begin();
                    while (mit != sit->second.memberNames.end()) {
                        if (*mit == sender) {
                            foundSessionMapEntry = false;
                            replyCode = ALLJOYN_JOINSESSION_REPLY_ALREADY_JOINED;
                            break;
                        }
                        ++mit;
                    }
                    sme = sit->second;
                }
                ++sit;
            }

            BusEndpoint* joinerEp = ajObj.router.FindEndpoint(sender);
            if (joinerEp && foundSessionMapEntry) {
                bool isAccepted = false;
                SessionId newSessionId = sme.id;
                if (!sme.opts.IsCompatible(optsIn)) {
                    replyCode = ALLJOYN_JOINSESSION_REPLY_BAD_SESSION_OPTS;
                } else {
                    /* Create a new sessionId if needed */
                    while (newSessionId == 0) {
                        newSessionId = qcc::Rand32();
                    }

                    /* Add an entry to sessionMap here (before sending accept session) since accept session
                     * may trigger a call to GetSessionFd or LeaveSession which must be aware of the new session's
                     * existence in order to complete successfully.
                     */
                    bool hasSessionMapPlaceholder = false;
                    sme.id = newSessionId;

                    if (!ajObj.SessionMapFind(sme.endpointName, sme.id)) {
                        ajObj.SessionMapInsert(sme);
                        hasSessionMapPlaceholder = true;
                    }

                    /* Ask creator to accept session */
                    ajObj.ReleaseLocks();
                    status = ajObj.SendAcceptSession(sme.sessionPort, newSessionId, sessionHost, sender.c_str(), optsIn, isAccepted);
                    if (status != ER_OK) {
                        QCC_LogError(status, ("SendAcceptSession failed"));
                        replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                    }

                    /* Re-lock and reacquire */
                    ajObj.AcquireLocks();
                    if (status == ER_OK) {
                        joinerEp = ajObj.router.FindEndpoint(sender);
                        if (!joinerEp) {
                            replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                            QCC_LogError(ER_FAIL, ("Joiner %s disappeared while joining", sender.c_str()));
                        }
                    }

                    /* Cleanup failed raw session entry in sessionMap */
                    if (hasSessionMapPlaceholder && ((status != ER_OK) || !isAccepted)) {
                        ajObj.SessionMapErase(sme);
                    }
                }
                if (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
                    if (!isAccepted) {
                        replyCode = ALLJOYN_JOINSESSION_REPLY_REJECTED;
                    } else if (sme.opts.traffic == SessionOpts::TRAFFIC_MESSAGES) {
                        /* setup the forward and reverse routes through the local daemon */
                        RemoteEndpoint* tEp = NULL;
                        status = ajObj.router.AddSessionRoute(newSessionId, *joinerEp, NULL, *rSessionEp, tEp);
                        if (status != ER_OK) {
                            replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                            QCC_LogError(status, ("AddSessionRoute(%u, %s, NULL, %s, tEp) failed", newSessionId, sender.c_str(), rSessionEp->GetUniqueName().c_str()));
                        }
                        if (status == ER_OK) {
                            /* Add (local) joiner to list of session members since no AttachSession will be sent */
                            SessionMapEntry* smEntry = ajObj.SessionMapFind(sme.endpointName, newSessionId);
                            if (smEntry) {
                                smEntry->memberNames.push_back(sender);
                                sme = *smEntry;
                            } else {
                                replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                                status = ER_FAIL;
                                QCC_LogError(status, ("Failed to find sessionMap entry"));
                            }

                            /* Create a joiner side entry in sessionMap */
                            SessionMapEntry joinerSme = sme;
                            joinerSme.endpointName = sender;
                            joinerSme.id = newSessionId;
                            ajObj.SessionMapInsert(joinerSme);
                            id = joinerSme.id;
                            optsOut = sme.opts;

                            if (status == ER_OK) {
                                ajObj.ReleaseLocks();
                                ajObj.SendJoinSession(sme.sessionPort, newSessionId, sender.c_str(), sme.endpointName.c_str());
                                ajObj.AcquireLocks();
                            }

                            /* Send session changed notification */
                            if (sme.opts.isMultipoint && (status == ER_OK)) {
                                ajObj.ReleaseLocks();
                                ajObj.SendMPSessionChanged(newSessionId, sender.c_str(), true, sme.endpointName.c_str());
                                ajObj.AcquireLocks();
                            }
                        }
                    } else if ((sme.opts.traffic != SessionOpts::TRAFFIC_MESSAGES) && !sme.opts.isMultipoint) {
                        /* Create a raw socket pair for the two local session participants */
                        SocketFd fds[2];
                        status = SocketPair(fds);
                        if (status == ER_OK) {
                            /* Update the creator-side entry in sessionMap */
                            SessionMapEntry* smEntry = ajObj.SessionMapFind(sme.endpointName, sme.id);
                            if (smEntry) {
                                smEntry->fd = fds[0];
                                smEntry->memberNames.push_back(sender);

                                /* Create a joiner side entry in sessionMap */
                                SessionMapEntry sme2 = sme;
                                sme2.memberNames.push_back(sender);
                                sme2.endpointName = sender;
                                sme2.fd = fds[1];
                                ajObj.SessionMapInsert(sme2);
                                id = sme2.id;
                                optsOut = sme.opts;

                                // send joined signal
                                ajObj.ReleaseLocks();
                                ajObj.SendJoinSession(sme2.sessionPort, id, sender.c_str(), sme.endpointName.c_str());
                                ajObj.AcquireLocks();
                            } else {
                                qcc::Close(fds[0]);
                                qcc::Close(fds[1]);
                                status = ER_FAIL;
                                QCC_LogError(status, ("Failed to find sessionMap entry"));
                                replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                            }
                        } else {
                            QCC_LogError(status, ("SocketPair failed"));
                            replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                        }
                    } else {
                        /* QosInfo::TRAFFIC_RAW_UNRELIABLE is not currently supported */
                        replyCode = ALLJOYN_JOINSESSION_REPLY_BAD_SESSION_OPTS;
                    }
                }
            } else {
                if (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
                    replyCode = ALLJOYN_JOINSESSION_REPLY_NO_SESSION;
                }
            }
        } else {
            /* Session is with a connected or unconnected remote device */
            MsgArg membersArg;

            /* Check for existing multipoint session */
            if (vSessionEp && optsIn.isMultipoint) {
                vSessionEpName = vSessionEp->GetUniqueName();
                SessionMapType::iterator it = ajObj.sessionMap.begin();
                while (it != ajObj.sessionMap.end()) {
                    if ((it->second.sessionHost == vSessionEpName) && (it->second.sessionPort == sessionPort)) {
                        if (it->second.opts.IsCompatible(optsIn)) {
                            b2bEp = vSessionEp->GetBusToBusEndpoint(it->second.id);
                            if (b2bEp) {
                                b2bEp->IncrementRef();
                                b2bEpName = b2bEp->GetUniqueName();
                                replyCode = ALLJOYN_JOINSESSION_REPLY_SUCCESS;
                            }
                        } else {
                            /* Cannot support more than one connection to the same destination with the same sessionId */
                            replyCode = ALLJOYN_JOINSESSION_REPLY_BAD_SESSION_OPTS;
                        }
                        break;
                    }
                    ++it;
                }
            }

            String busAddr;
            if (!b2bEp) {
                /* Step 1: If there is a busAddr from advertisement use it to (possibly) create a physical connection */
                vector<String> busAddrs;
                multimap<String, NameMapEntry>::iterator nmit = ajObj.nameMap.lower_bound(sessionHost);
                while (nmit != ajObj.nameMap.end() && (nmit->first == sessionHost)) {
                    if (nmit->second.transport & optsIn.transports) {
                        busAddrs.push_back(nmit->second.busAddr);
                        break;
                    }
                    ++nmit;
                }
                ajObj.ReleaseLocks();

                /*
                 * Step 1b: If no advertisement (busAddr) and we are connected to the sesionHost, then ask it directly
                 * for the busAddr
                 */
                if (vSessionEp && busAddrs.empty()) {
                    status = ajObj.SendGetSessionInfo(sessionHost, sessionPort, optsIn, busAddrs);
                    if (status != ER_OK) {
                        busAddrs.clear();
                        QCC_LogError(status, ("GetSessionInfo failed"));
                    }
                }

                if (!busAddrs.empty()) {
                    /* Try busAddrs in priority order until connect succeeds */
                    for (size_t i = 0; i < busAddrs.size(); ++i) {
                        /* Ask the transport that provided the advertisement for an endpoint */
                        TransportList& transList = ajObj.bus.GetInternal().GetTransportList();
                        Transport* trans = transList.GetTransport(busAddrs[i]);
                        if (trans != NULL) {
                            if ((optsIn.transports & trans->GetTransportMask()) == 0) {
                                QCC_DbgPrintf(("AllJoynObj:JoinSessionThread() skip unpermitted transport(%s)", trans->GetTransportName()));
                                continue;
                            }

                            BusEndpoint* ep;
                            status = trans->Connect(busAddrs[i].c_str(), optsIn, &ep);
                            if (status == ER_OK) {
                                b2bEp = static_cast<RemoteEndpoint*>(ep);
                                b2bEp->IncrementRef();
                                b2bEpName = b2bEp->GetUniqueName();
                                busAddr = busAddrs[i];
                                replyCode = ALLJOYN_JOINSESSION_REPLY_SUCCESS;
                                optsIn.transports  = trans->GetTransportMask();
                                break;
                            } else {
                                QCC_LogError(status, ("trans->Connect(%s) failed", busAddrs[i].c_str()));
                                replyCode = ALLJOYN_JOINSESSION_REPLY_CONNECT_FAILED;
                            }
                        }
                    }
                } else {
                    /* No advertisment or existing route to session creator */
                    replyCode = ALLJOYN_JOINSESSION_REPLY_NO_SESSION;
                }

                if (busAddr.empty()) {
                    replyCode = ALLJOYN_JOINSESSION_REPLY_UNREACHABLE;
                }
                ajObj.AcquireLocks();
            }

            /* Step 2: Wait for the new b2b endpoint to have a virtual ep for nextController */
            uint32_t startTime = GetTimestamp();
            b2bEp = static_cast<RemoteEndpoint*>(ajObj.router.FindEndpoint(b2bEpName));
            while (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
                /* Do we route through b2bEp? If so, we're done */
                ep = b2bEp ? ajObj.router.FindEndpoint(b2bEp->GetRemoteName()) : NULL;

                VirtualEndpoint* vep = (ep && (ep->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) ? static_cast<VirtualEndpoint*>(ep) : NULL;
                if (!b2bEp) {
                    QCC_LogError(ER_FAIL, ("B2B endpoint %s disappeared during JoinSession", b2bEpName.c_str()));
                    replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                    break;
                } else if (vep && vep->CanUseRoute(*b2bEp)) {
                    break;
                }

                /* Otherwise wait */
                uint32_t now = GetTimestamp();
                if (now > (startTime + 30000)) {
                    replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                    QCC_LogError(ER_FAIL, ("JoinSession timed out waiting for %s to appear on %s",
                                           sessionHost, b2bEp->GetUniqueName().c_str()));
                    break;
                } else {
                    /* Give up the locks while waiting */
                    ajObj.ReleaseLocks();
                    qcc::Sleep(10);
                    ajObj.AcquireLocks();
                    /* Re-acquire b2bEp now that we have the lock again */
                    b2bEp = static_cast<RemoteEndpoint*>(ajObj.router.FindEndpoint(b2bEpName));
                }
            }

            /* Step 3: Send a session attach */
            if (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
                const String nextControllerName = b2bEp->GetRemoteName();
                ajObj.ReleaseLocks();
                status = ajObj.SendAttachSession(sessionPort, sender.c_str(), sessionHost, sessionHost, b2bEpName.c_str(),
                                                 nextControllerName.c_str(), 0, busAddr.c_str(), optsIn, replyCode,
                                                 id, optsOut, membersArg);
                if (status != ER_OK) {
                    QCC_LogError(status, ("AttachSession to %s failed", nextControllerName.c_str()));
                    replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                }
                /* Re-acquire locks and endpoints */
                ajObj.AcquireLocks();
                vSessionEp = static_cast<VirtualEndpoint*>(ajObj.router.FindEndpoint(sessionHost));
                vSessionEpName = vSessionEp ? vSessionEp->GetUniqueName() : "";
                if (!vSessionEp) {
                    replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                    QCC_LogError(ER_FAIL, ("SessionHost endpoint (%s) not found", sessionHost));
                }

                b2bEp = static_cast<RemoteEndpoint*>(ajObj.router.FindEndpoint(b2bEpName));
                if (!b2bEp) {
                    replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                    QCC_LogError(ER_FAIL, ("SessionHost b2bEp (%s) disappeared during join", b2bEpName.c_str()));
                }
            }

            /* If session was successful, Add two-way session routes to the table */
            if (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
                BusEndpoint* joinerEp = ajObj.router.FindEndpoint(sender);
                if (joinerEp) {
                    status = ajObj.router.AddSessionRoute(id, *joinerEp, NULL, *vSessionEp, b2bEp, b2bEp ? NULL : &optsOut);
                    if (status != ER_OK) {
                        replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                        QCC_LogError(status, ("AddSessionRoute(%u, %s, NULL, %s, %s, %s) failed", id, sender.c_str(), vSessionEp->GetUniqueName().c_str(), b2bEp ? b2bEp->GetUniqueName().c_str() : "NULL", b2bEp ? "NULL" : "opts"));
                    }
                } else {
                    replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                    QCC_LogError(ER_BUS_NO_ENDPOINT, ("Cannot find joiner endpoint %s", sender.c_str()));
                }
            }

            /* Create session map entry */
            bool sessionMapEntryCreated = false;
            if (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
                const MsgArg* sessionMembers;
                size_t numSessionMembers = 0;
                membersArg.Get("as", &numSessionMembers, &sessionMembers);
                sme.endpointName = sender;
                sme.id = id;
                sme.sessionHost = vSessionEp->GetUniqueName();
                sme.sessionPort = sessionPort;
                sme.opts = optsOut;
                for (size_t i = 0; i < numSessionMembers; ++i) {
                    sme.memberNames.push_back(sessionMembers[i].v_string.str);
                }
                ajObj.SessionMapInsert(sme);
                sessionMapEntryCreated = true;
            }

            /* If a raw sesssion was requested, then teardown the new b2bEp to use it for a raw stream */
            if ((replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) && (optsOut.traffic != SessionOpts::TRAFFIC_MESSAGES)) {
                /*
                 * TODO - it looks to like sme is already the session map entry we are looking for
                 * so the find is redundant.
                 */
                SessionMapEntry* smEntry = ajObj.SessionMapFind(sender, id);
                if (smEntry) {
                    status = ajObj.ShutdownEndpoint(*b2bEp, smEntry->fd);
                    if (status != ER_OK) {
                        QCC_LogError(status, ("Failed to shutdown remote endpoint for raw usage"));
                        replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                    }
                } else {
                    QCC_LogError(ER_FAIL, ("Failed to find session id=%u for %s, %d", id, sender.c_str(), id));
                    replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                }
            }

            /* If session was unsuccessful, cleanup sessionMap */
            if (sessionMapEntryCreated && (replyCode != ALLJOYN_JOINSESSION_REPLY_SUCCESS)) {
                ajObj.SessionMapErase(sme);
            }

            /* Cleanup b2bEp if its ref hasn't been incremented */
            if (b2bEp) {
                b2bEp->DecrementRef();
            }
        }
    }

    /* Send AttachSession to all other members of the multicast session */
    if ((replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) && sme.opts.isMultipoint) {
        for (size_t i = 0; i < sme.memberNames.size(); ++i) {
            const String& member = sme.memberNames[i];
            /* Skip this joiner since it is attached already */
            if (member == sender) {
                continue;
            }
            BusEndpoint* joinerEp = ajObj.router.FindEndpoint(sender);
            BusEndpoint* memberEp = ajObj.router.FindEndpoint(member);
            RemoteEndpoint* memberB2BEp = NULL;
            if (memberEp && (memberEp->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) {
                /* Endpoint is not served directly by this daemon so forward the attach using existing b2bEp connection with session creator */
                VirtualEndpoint* vMemberEp = static_cast<VirtualEndpoint*>(memberEp);
                if (b2bEpName.empty()) {
                    /* Local session creator */
                    memberB2BEp = vMemberEp->GetBusToBusEndpoint(id);
                    if (memberB2BEp) {
                        b2bEpName = memberB2BEp->GetUniqueName();
                    }
                } else {
                    /* Remote session creator */
                    memberB2BEp = static_cast<RemoteEndpoint*>(ajObj.router.FindEndpoint(b2bEpName));
                }
                if (memberB2BEp) {
                    MsgArg tMembersArg;
                    SessionId tId;
                    SessionOpts tOpts;
                    const String nextControllerName = memberB2BEp->GetRemoteName();
                    uint32_t tReplyCode;
                    ajObj.ReleaseLocks();
                    status = ajObj.SendAttachSession(sessionPort,
                                                     sender.c_str(),
                                                     sessionHost,
                                                     member.c_str(),
                                                     memberB2BEp->GetUniqueName().c_str(),
                                                     nextControllerName.c_str(),
                                                     id,
                                                     "",
                                                     sme.opts,
                                                     tReplyCode,
                                                     tId,
                                                     tOpts,
                                                     tMembersArg);
                    ajObj.AcquireLocks();

                    /* Reacquire endpoints since locks were given up */
                    joinerEp = ajObj.router.FindEndpoint(sender);
                    memberEp = ajObj.router.FindEndpoint(member);
                    memberB2BEp = static_cast<RemoteEndpoint*>(ajObj.router.FindEndpoint(b2bEpName));
                    if (status != ER_OK) {
                        QCC_LogError(status, ("Failed to attach session %u to %s", id, member.c_str()));
                    } else if (tReplyCode != ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
                        status = ER_FAIL;
                        QCC_LogError(status, ("Failed to attach session %u to %s (reply=%d)", id, member.c_str(), tReplyCode));
                    } else if (id != tId) {
                        status = ER_FAIL;
                        QCC_LogError(status, ("Session id mismatch (expected=%u, actual=%u)", id, tId));
                    } else if (!joinerEp || !memberB2BEp || !memberB2BEp) {
                        status = ER_FAIL;
                        QCC_LogError(status, ("joiner, memberEp or memberB2BEp disappeared during join"));
                    }
                } else {
                    status = ER_BUS_BAD_SESSION_OPTS;
                    QCC_LogError(status, ("Unable to add existing member %s to session %u", vMemberEp->GetUniqueName().c_str(), id));
                }
            } else if (memberEp && (memberEp->GetEndpointType() != BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) {
                /* Add joiner to any local member's sessionMap entry  since no AttachSession is sent */
                SessionMapEntry* smEntry = ajObj.SessionMapFind(member, id);
                if (smEntry) {
                    smEntry->memberNames.push_back(sender);
                }

                /* Multipoint session member is local to this daemon. Send MPSessionChanged */
                if (optsOut.isMultipoint) {
                    ajObj.ReleaseLocks();
                    ajObj.SendMPSessionChanged(id, sender.c_str(), true, member.c_str());
                    ajObj.AcquireLocks();
                    joinerEp = ajObj.router.FindEndpoint(sender);
                    memberEp = ajObj.router.FindEndpoint(member);
                    memberB2BEp = static_cast<RemoteEndpoint*>(ajObj.router.FindEndpoint(b2bEpName));
                }
            }
            /* Add session routing */
            if (memberEp && joinerEp && (status == ER_OK)) {
                status = ajObj.router.AddSessionRoute(id, *joinerEp, NULL, *memberEp, memberB2BEp);
                if (status != ER_OK) {
                    QCC_LogError(status, ("AddSessionRoute(%u, %s, NULL, %s, %s) failed", id, sender.c_str(), memberEp->GetUniqueName().c_str(), memberB2BEp ? memberB2BEp->GetUniqueName().c_str() : "<none>"));
                }
            }
        }
    }
    ajObj.ReleaseLocks();

    /* Reply to request */
    MsgArg replyArgs[3];
    replyArgs[0].Set("u", replyCode);
    replyArgs[1].Set("u", id);
    SetSessionOpts(optsOut, replyArgs[2]);
    status = ajObj.MethodReply(msg, replyArgs, ArraySize(replyArgs));
    QCC_DbgPrintf(("AllJoynObj::JoinSession(%d) returned (%d,%u) (status=%s)", sessionPort, replyCode, id, QCC_StatusText(status)));

    /* Log error if reply could not be sent */
    if (ER_OK != status) {
        QCC_LogError(status, ("Failed to respond to org.alljoyn.Bus.JoinSession"));
    }

    /* Send a series of MPSessionChanged to "catch up" the new joiner */
    if ((replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) && optsOut.isMultipoint) {
        ajObj.AcquireLocks();
        SessionMapEntry* smEntry = ajObj.SessionMapFind(sender, id);
        if (smEntry) {
            String sessionHost = smEntry->sessionHost;
            vector<String> memberVector = smEntry->memberNames;
            ajObj.ReleaseLocks();
            ajObj.SendMPSessionChanged(id, sessionHost.c_str(), true, sender.c_str());
            vector<String>::const_iterator mit = memberVector.begin();
            while (mit != memberVector.end()) {
                if (sender != *mit) {
                    ajObj.SendMPSessionChanged(id, mit->c_str(), true, sender.c_str());
                }
                mit++;
            }
        } else {
            ajObj.ReleaseLocks();
        }
    }

    return 0;
}

void AllJoynObj::JoinSessionThread::ThreadExit(Thread* thread)
{
    ajObj.joinSessionThreadsLock.Lock(MUTEX_CONTEXT);
    vector<JoinSessionThread*>::iterator it = ajObj.joinSessionThreads.begin();
    JoinSessionThread* deleteMe = NULL;
    while (it != ajObj.joinSessionThreads.end()) {
        if (*it == thread) {
            deleteMe = *it;
            ajObj.joinSessionThreads.erase(it);
            break;
        }
        ++it;
    }
    ajObj.joinSessionThreadsLock.Unlock(MUTEX_CONTEXT);
    if (deleteMe) {
        delete deleteMe;
    } else {
        QCC_LogError(ER_FAIL, ("Internal error: JoinSessionThread not found on list"));
    }
}

void AllJoynObj::JoinSession(const InterfaceDescription::Member* member, Message& msg)
{
    /* Handle JoinSession on another thread since JoinThread can block waiting for NameOwnerChanged */
    joinSessionThreadsLock.Lock(MUTEX_CONTEXT);
    if (!isStopping) {
        JoinSessionThread* jst = new JoinSessionThread(*this, msg, true);
        QStatus status = jst->Start(NULL, jst);
        if (status == ER_OK) {
            joinSessionThreads.push_back(jst);
        } else {
            QCC_LogError(status, ("Join: Failed to start JoinSessionThread"));
        }
    }
    joinSessionThreadsLock.Unlock(MUTEX_CONTEXT);
}

void AllJoynObj::AttachSession(const InterfaceDescription::Member* member, Message& msg)
{
    /* Handle AttachSession on another thread since AttachSession can block when connecting through an intermediate node */
    joinSessionThreadsLock.Lock(MUTEX_CONTEXT);
    if (!isStopping) {
        JoinSessionThread* jst = new JoinSessionThread(*this, msg, false);
        QStatus status = jst->Start(NULL, jst);
        if (status == ER_OK) {
            joinSessionThreads.push_back(jst);
        } else {
            QCC_LogError(status, ("Attach: Failed to start JoinSessionThread"));
        }
    }
    joinSessionThreadsLock.Unlock(MUTEX_CONTEXT);
}

void AllJoynObj::LeaveSession(const InterfaceDescription::Member* member, Message& msg)
{
    uint32_t replyCode = ALLJOYN_LEAVESESSION_REPLY_SUCCESS;

    size_t numArgs;
    const MsgArg* args;

    /* Parse the message args */
    msg->GetArgs(numArgs, args);
    assert(numArgs == 1);
    SessionId id = static_cast<SessionId>(args[0].v_uint32);

    QCC_DbgTrace(("AllJoynObj::LeaveSession(%u)", id));

    /* Find the session with that id */
    AcquireLocks();
    SessionMapEntry* smEntry = SessionMapFind(msg->GetSender(), id);
    if (!smEntry || (id == 0)) {
        replyCode = ALLJOYN_LEAVESESSION_REPLY_NO_SESSION;
        ReleaseLocks();
    } else {
        /* Send DetachSession signal to daemons of all session participants */
        MsgArg detachSessionArgs[2];
        detachSessionArgs[0].Set("u", id);
        detachSessionArgs[1].Set("s", msg->GetSender());

        QStatus status = Signal(NULL, 0, *detachSessionSignal, detachSessionArgs, ArraySize(detachSessionArgs), 0, ALLJOYN_FLAG_GLOBAL_BROADCAST);
        if (status != ER_OK) {
            QCC_LogError(status, ("Error sending org.alljoyn.Daemon.DetachSession signal"));
        }

        /* Close any open fd for this session */
        if (smEntry->fd != -1) {
            qcc::Shutdown(smEntry->fd);
            qcc::Close(smEntry->fd);
        }

        /* Locks must be released before calling RemoveSessionRefs since that method calls out to user (SessionLost) */
        ReleaseLocks();

        /* Remove entries from sessionMap */
        RemoveSessionRefs(msg->GetSender(), id);

        /* Remove session routes */
        router.RemoveSessionRoutes(msg->GetSender(), id);
    }

    /* Reply to request */
    MsgArg replyArgs[1];
    replyArgs[0].Set("u", replyCode);
    QStatus status = MethodReply(msg, replyArgs, ArraySize(replyArgs));

    /* Log error if reply could not be sent */
    if (ER_OK != status) {
        QCC_LogError(status, ("Failed to respond to org.alljoyn.Bus.LeaveSession"));
    }
}

qcc::ThreadReturn STDCALL AllJoynObj::JoinSessionThread::RunAttach()
{
    SessionId id = 0;
    String creatorName;
    MsgArg replyArgs[4];
    SessionOpts optsOut;
    uint32_t replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
    bool destIsLocal = false;

    /* Default member list to empty */
    replyArgs[3].Set("as", 0, NULL);

    /* Received a daemon request to establish a session route */

    /* Parse message args */
    SessionPort sessionPort;
    const char* src;
    const char* sessionHost;
    const char* dest;
    const char* srcB2B;
    const char* busAddr;
    SessionOpts optsIn;
    RemoteEndpoint* srcB2BEp = NULL;
    String b2bEpName;
    String srcStr;
    String destStr;
    bool newSME = false;
    SessionMapEntry sme;

    size_t na;
    const MsgArg* args;
    msg->GetArgs(na, args);
    QStatus status = MsgArg::Get(args, 6, "qsssss", &sessionPort, &src, &sessionHost, &dest, &srcB2B, &busAddr);
    const String srcB2BStr = srcB2B;

    if (status == ER_OK) {
        status = GetSessionOpts(args[6], optsIn);
    }

    if (status != ER_OK) {
        QCC_DbgTrace(("AllJoynObj::AttachSession(<bad args>)"));
        replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
        ajObj.AcquireLocks();
    } else {
        srcStr = src;
        destStr = dest;

        QCC_DbgTrace(("AllJoynObj::AttachSession(%d, %s, %s, %s, %s, %s, <%x, %x, %x>)", sessionPort, src, sessionHost,
                      dest, srcB2B, busAddr, optsIn.traffic, optsIn.proximity, optsIn.transports));

        ajObj.AcquireLocks();
        BusEndpoint* destEp = ajObj.router.FindEndpoint(destStr);

        /*
         * If there is an outstanding join involving (sessionHost,port), then destEp may not be valid yet.
         * Essentially, someone else might know we are a multipoint session member before we do.
         */
        if (!destEp || ((destEp->GetEndpointType() != BusEndpoint::ENDPOINT_TYPE_REMOTE) &&
                        (destEp->GetEndpointType() != BusEndpoint::ENDPOINT_TYPE_NULL) &&
                        (destEp->GetEndpointType() != BusEndpoint::ENDPOINT_TYPE_LOCAL))) {
            qcc::Sleep(500);
            destEp = ajObj.router.FindEndpoint(destStr);
        }

        /* Determine if the dest is local to this daemon */
        if (destEp && ((destEp->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_REMOTE) ||
                       (destEp->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_NULL) ||
                       (destEp->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_LOCAL))) {
            /* This daemon serves dest directly */
            /* Check for a session in the session map */
            bool foundSessionMapEntry = false;
            String destUniqueName = destEp->GetUniqueName();
            BusEndpoint* sessionHostEp = ajObj.router.FindEndpoint(sessionHost);
            SessionMapType::iterator sit = ajObj.SessionMapLowerBound(destUniqueName, 0);
            replyCode = ALLJOYN_JOINSESSION_REPLY_SUCCESS;
            while ((sit != ajObj.sessionMap.end()) && (sit->first.first == destUniqueName)) {
                BusEndpoint* creatorEp = ajObj.router.FindEndpoint(sit->second.sessionHost);
                sme = sit->second;
                if ((sme.sessionPort == sessionPort) && sessionHostEp && (creatorEp == sessionHostEp)) {
                    if (sit->second.opts.isMultipoint && (sit->first.second == 0)) {
                        /* Session is multipoint. Look for an existing (already joined) session */
                        while ((sit != ajObj.sessionMap.end()) && (sit->first.first == destUniqueName)) {
                            creatorEp = ajObj.router.FindEndpoint(sit->second.sessionHost);
                            if ((sit->first.second != 0) && (sit->second.sessionPort == sessionPort) && (creatorEp == sessionHostEp)) {
                                sme = sit->second;
                                foundSessionMapEntry = true;
                                /* make sure session is not already joined by this joiner */
                                vector<String>::const_iterator mit = sit->second.memberNames.begin();
                                while (mit != sit->second.memberNames.end()) {
                                    if (*mit++ == srcStr) {
                                        replyCode = ALLJOYN_JOINSESSION_REPLY_ALREADY_JOINED;
                                        foundSessionMapEntry = false;
                                        break;
                                    }
                                }
                                break;
                            }
                            ++sit;
                        }
                    } else if (sme.opts.isMultipoint && (sit->first.second == msg->GetSessionId())) {
                        /* joiner to joiner multipoint attach message */
                        foundSessionMapEntry = true;
                    } else if (!sme.opts.isMultipoint && (sit->first.second != 0)) {
                        /* Cannot join a non-multipoint session more than once */
                        replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                    }
                    if ((replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) && !foundSessionMapEntry) {
                        /* Assign a session id and insert entry */
                        while (sme.id == 0) {
                            sme.id = qcc::Rand32();
                        }
                        sme.isInitializing = true;
                        foundSessionMapEntry = true;
                        ajObj.SessionMapInsert(sme);
                        newSME = true;
                    }
                    break;
                }
                ++sit;
            }
            if (!foundSessionMapEntry) {
                if (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
                    replyCode = ALLJOYN_JOINSESSION_REPLY_NO_SESSION;
                }
            } else if (!sme.opts.IsCompatible(optsIn)) {
                replyCode = ALLJOYN_JOINSESSION_REPLY_BAD_SESSION_OPTS;
                optsOut = sme.opts;
            } else {
                optsOut = sme.opts;
                BusEndpoint* ep = ajObj.router.FindEndpoint(srcB2BStr);
                srcB2BEp = (ep && (ep->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_BUS2BUS)) ? static_cast<RemoteEndpoint*>(ep) : NULL;

                if (srcB2BEp) {
                    VirtualEndpoint* srcEp = &(ajObj.AddVirtualEndpoint(srcStr, *srcB2BEp));
                    if (status == ER_OK) {
                        /* Store ep for raw sessions (for future close and fd extract) */
                        if (optsOut.traffic != SessionOpts::TRAFFIC_MESSAGES) {
                            SessionMapEntry* smEntry = ajObj.SessionMapFind(sme.endpointName, sme.id);
                            if (smEntry) {
                                smEntry->streamingEp = srcB2BEp;
                            }
                        }

                        /* If this node is the session creator, give it a chance to accept or reject the new member */
                        bool isAccepted = true;
                        BusEndpoint* creatorEp = ajObj.router.FindEndpoint(sme.sessionHost);

                        if (creatorEp && (destEp == creatorEp)) {
                            ajObj.ReleaseLocks();
                            status = ajObj.SendAcceptSession(sme.sessionPort, sme.id, dest, src, optsIn, isAccepted);
                            if (ER_OK != status) {
                                replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                                QCC_LogError(status, ("SendAcceptSession failed"));
                            }
                            /* Re-lock and re-acquire */
                            ajObj.AcquireLocks();
                            destEp = ajObj.router.FindEndpoint(destStr);
                            ep = ajObj.router.FindEndpoint(srcB2BStr);
                            srcB2BEp = (ep && (ep->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_BUS2BUS)) ? static_cast<RemoteEndpoint*>(ep) : NULL;
                            srcEp = srcB2BEp ? &(ajObj.AddVirtualEndpoint(srcStr,  *srcB2BEp)) : NULL;

                            if (!destEp || !srcEp) {
                                QCC_LogError(ER_FAIL, ("%s (%s) disappeared during JoinSession", !destEp ? "destEp" : "srcB2BEp", !destEp ? destStr.c_str() : srcB2BStr.c_str()));
                                replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                            }
                        }

                        /* Add new joiner to members */
                        if (isAccepted && creatorEp && (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS)) {
                            SessionMapEntry* smEntry = ajObj.SessionMapFind(sme.endpointName, sme.id);
                            /* Update sessionMap */
                            if (smEntry) {
                                smEntry->memberNames.push_back(srcStr);
                                id = smEntry->id;
                                destIsLocal = true;
                                creatorName = creatorEp->GetUniqueName();

                                /* AttachSession response will contain list of members */
                                vector<const char*> nameVec(smEntry->memberNames.size());
                                for (size_t i = 0; i < nameVec.size(); ++i) {
                                    nameVec[i] = smEntry->memberNames[i].c_str();
                                }
                                replyArgs[3].Set("as", nameVec.size(), nameVec.empty() ? NULL : &nameVec.front());
                            } else {
                                replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                            }

                            /* Add routes for new session */
                            if (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
                                if (optsOut.traffic == SessionOpts::TRAFFIC_MESSAGES) {
                                    RemoteEndpoint* tEp = NULL;
                                    status = ajObj.router.AddSessionRoute(id, *destEp, tEp, *srcEp, srcB2BEp);
                                    if (ER_OK != status) {
                                        QCC_LogError(status, ("AddSessionRoute(%u, %s, NULL, %s, %s) failed", id, dest, srcEp->GetUniqueName().c_str(), srcB2BEp ? srcB2BEp->GetUniqueName().c_str() : "NULL"));
                                    }
                                }

                                // only send to creator!
                                if (ER_OK == status && creatorEp && (destEp == creatorEp)) {
                                    ajObj.ReleaseLocks();
                                    ajObj.SendJoinSession(sme.sessionPort, sme.id, src, sme.endpointName.c_str());
                                    ajObj.AcquireLocks();
                                }
                            }
                        } else {
                            replyCode =  ALLJOYN_JOINSESSION_REPLY_REJECTED;
                        }
                    }
                } else {
                    status = ER_FAIL;
                    replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                    QCC_LogError(status, ("Cannot locate srcB2BEp(%p, src=%s)", srcB2BEp, srcB2BStr.c_str()));
                }
            }
        } else {
            /* This daemon will attempt to route indirectly to dest */
            RemoteEndpoint* b2bEp = NULL;
            if ((busAddr[0] == '\0') && (msg->GetSessionId() != 0) && destEp && (destEp->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) {
                /* This is a secondary (multipoint) attach.
                 * Forward the attach to the dest over the existing session id's B2BEp */
                VirtualEndpoint*vep = static_cast<VirtualEndpoint*>(destEp);
                b2bEp = vep->GetBusToBusEndpoint(msg->GetSessionId());
                b2bEpName = b2bEp ? b2bEp->GetUniqueName() : "";
                if (b2bEp) {
                    b2bEp->IncrementRef();
                }
            } else if (busAddr[0] != '\0') {
                /* Ask the transport for an endpoint */
                TransportList& transList = ajObj.bus.GetInternal().GetTransportList();
                Transport* trans = transList.GetTransport(busAddr);
                if (trans == NULL) {
                    replyCode = ALLJOYN_JOINSESSION_REPLY_UNREACHABLE;
                } else {
                    ajObj.ReleaseLocks();
                    BusEndpoint* ep;
                    status = trans->Connect(busAddr, optsIn, &ep);
                    ajObj.AcquireLocks();
                    if (status == ER_OK) {
                        b2bEp = static_cast<RemoteEndpoint*>(ep);
                        b2bEp->IncrementRef();
                        b2bEpName = b2bEp->GetUniqueName();
                    } else {
                        QCC_LogError(status, ("trans->Connect(%s) failed", busAddr));
                        replyCode = ALLJOYN_JOINSESSION_REPLY_CONNECT_FAILED;
                    }
                }
            }

            if (b2bEpName.empty()) {
                replyCode = ALLJOYN_JOINSESSION_REPLY_NO_SESSION;
            } else {
                /* Forward AttachSession to next hop */
                SessionId tempId;
                SessionOpts tempOpts;
                const String nextControllerName = b2bEp->GetRemoteName();

                /* Send AttachSession */
                ajObj.ReleaseLocks();
                status = ajObj.SendAttachSession(sessionPort, src, sessionHost, dest, b2bEpName.c_str(), nextControllerName.c_str(),
                                                 msg->GetSessionId(), busAddr, optsIn, replyCode, tempId, tempOpts, replyArgs[3]);
                ajObj.AcquireLocks();
                b2bEp = NULL;

                /* If successful, add bi-directional session routes */
                if ((status == ER_OK) && (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS)) {

                    /* Wait for dest to appear with a route through b2bEp */
                    uint32_t startTime = GetTimestamp();
                    VirtualEndpoint* vDestEp = NULL;
                    while (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
                        /* Does vSessionEp route through b2bEp? If so, we're done */
                        BusEndpoint* ep = ajObj.router.FindEndpoint(destStr);
                        vDestEp = (ep && (ep->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) ? static_cast<VirtualEndpoint*>(ep) : NULL;
                        b2bEp = static_cast<RemoteEndpoint*>(ajObj.router.FindEndpoint(b2bEpName));
                        if (!b2bEp) {
                            QCC_LogError(ER_FAIL, ("B2B endpoint disappeared during AttachSession"));
                            replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                            break;
                        } else if (vDestEp && vDestEp->CanUseRoute(*b2bEp)) {
                            break;
                        }
                        /* Otherwise wait */
                        uint32_t now = GetTimestamp();
                        if (now > (startTime + 30000)) {
                            replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                            QCC_LogError(ER_FAIL, ("AttachSession timed out waiting for destination to appear"));
                            break;
                        } else {
                            /* Give up the locks while waiting */
                            ajObj.ReleaseLocks();
                            qcc::Sleep(10);
                            ajObj.AcquireLocks();
                        }
                    }
                    BusEndpoint* ep = ajObj.router.FindEndpoint(srcB2BStr);
                    RemoteEndpoint* srcB2BEp2 = (ep && (ep->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_BUS2BUS)) ? static_cast<RemoteEndpoint*>(ep) : NULL;
                    VirtualEndpoint* srcEp = srcB2BEp2 ? &(ajObj.AddVirtualEndpoint(srcStr, *srcB2BEp2)) : NULL;
                    /* Add bi-directional session routes */
                    if (srcB2BEp2 && srcEp && vDestEp && b2bEp) {
                        id = tempId;
                        optsOut = tempOpts;
                        status = ajObj.router.AddSessionRoute(id, *vDestEp, b2bEp, *srcEp, srcB2BEp2);
                        if (status != ER_OK) {
                            QCC_LogError(status, ("AddSessionRoute(%u, %s, %s, %s) failed", id, dest, b2bEp->GetUniqueName().c_str(), srcEp->GetUniqueName().c_str(), srcB2BEp2->GetUniqueName().c_str()));
                        }
                    } else {
                        replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                    }
                } else {
                    if (status == ER_OK) {
                        status = ER_BUS_REPLY_IS_ERROR_MESSAGE;
                    }
                    replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
                    QCC_LogError(status, ("AttachSession failed"));
                }
            }
            if (b2bEp) {
                b2bEp->DecrementRef();
            }
        }
    }

    /* Reply to request */
    replyArgs[0].Set("u", replyCode);
    replyArgs[1].Set("u", id);
    SetSessionOpts(optsOut, replyArgs[2]);

    /* On success, ensure that reply goes over the new b2b connection. Otherwise a race condition
     * related to shutting down endpoints that are to become raw will occur.
     */
    srcB2BEp = srcB2B ? static_cast<RemoteEndpoint*>(ajObj.router.FindEndpoint(srcB2BStr)) : NULL;
    if (srcB2BEp) {
        srcB2BEp->IncrementWaiters();
    }
    ajObj.ReleaseLocks();
    if (srcB2BEp) {
        status = msg->ReplyMsg(msg, replyArgs, ArraySize(replyArgs));
        if (status == ER_OK) {
            status = srcB2BEp->PushMessage(msg);
        }
    } else {
        status = ajObj.MethodReply(msg, replyArgs, ArraySize(replyArgs));
    }
    if (srcB2BEp) {
        srcB2BEp->DecrementWaiters();
    }
    ajObj.AcquireLocks();
    srcB2BEp  = !srcB2BStr.empty() ? static_cast<RemoteEndpoint*>(ajObj.router.FindEndpoint(srcB2BStr)) : NULL;

    /* Log error if reply could not be sent */
    if (ER_OK != status) {
        QCC_LogError(status, ("Failed to respond to org.alljoyn.Daemon.AttachSession."));
    }

    /* Special handling for successful raw session creation. (Must occur after reply is sent) */
    if (srcB2BEp && (optsOut.traffic != SessionOpts::TRAFFIC_MESSAGES)) {
        if (b2bEpName.empty()) {
            if (!creatorName.empty()) {
                /* Destination for raw session. Shutdown endpoint and preserve the fd for future call to GetSessionFd */
                SessionMapEntry* smEntry = ajObj.SessionMapFind(creatorName, id);
                if (smEntry) {
                    if (smEntry->streamingEp) {
                        status = ajObj.ShutdownEndpoint(*smEntry->streamingEp, smEntry->fd);
                        if (status != ER_OK) {
                            QCC_LogError(status, ("Failed to shutdown raw endpoint"));
                        }
                        smEntry->streamingEp = NULL;
                    }
                } else {
                    QCC_LogError(ER_FAIL, ("Failed to find SessionMapEntry \"%s\",%08x", creatorName.c_str(), id));
                }
            }
        } else {
            /* Indirect raw route (middle-man). Create a pump to copy raw data between endpoints */
            BusEndpoint* ep = ajObj.router.FindEndpoint(b2bEpName);
            RemoteEndpoint* b2bEp = ep ? static_cast<RemoteEndpoint*>(ep) : NULL;
            if (b2bEp) {
                QStatus tStatus;
                SocketFd srcB2bFd, b2bFd;
                status = ajObj.ShutdownEndpoint(*srcB2BEp, srcB2bFd);
                tStatus = ajObj.ShutdownEndpoint(*b2bEp, b2bFd);
                status = (status == ER_OK) ? tStatus : status;
                if (status == ER_OK) {
                    SocketStream* ss1 = new SocketStream(srcB2bFd);
                    SocketStream* ss2 = new SocketStream(b2bFd);
                    size_t chunkSize = 4096;
                    String threadNameStr = id;
                    threadNameStr.append("-pump");
                    const char* threadName = threadNameStr.c_str();
                    bool isManaged = true;
                    ManagedObj<StreamPump> pump(ss1, ss2, chunkSize, threadName, isManaged);
                    status = pump->Start();
                }
                if (status != ER_OK) {
                    QCC_LogError(status, ("Raw relay creation failed"));
                }
            }
        }
    }

    /* Clear the initializing state (or cleanup) any initializing sessionMap entry */
    if (newSME) {
        SessionMapEntry* smEntry = ajObj.SessionMapFind(sme.endpointName, sme.id);
        if (smEntry) {
            if (replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) {
                smEntry->isInitializing = false;
            } else {
                ajObj.SessionMapErase(sme);
            }
        } else {
            QCC_LogError(ER_BUS_NO_SESSION, ("Error clearing initializing entry in sessionMap"));
        }
    }

    ajObj.ReleaseLocks();

    /* Send SessionChanged if multipoint */
    if ((replyCode == ALLJOYN_JOINSESSION_REPLY_SUCCESS) && optsOut.isMultipoint && (id != 0) && destIsLocal) {
        ajObj.SendMPSessionChanged(id, srcStr.c_str(), true, destStr.c_str());
    }

    QCC_DbgPrintf(("AllJoynObj::AttachSession(%d) returned (%d,%u) (status=%s)", sessionPort, replyCode, id, QCC_StatusText(status)));

    return 0;
}

void AllJoynObj::RemoveSessionRefs(const char* epName, SessionId id)
{
    QCC_DbgTrace(("AllJoynObj::RemoveSessionRefs(%s, %u)", epName, id));

    AcquireLocks();

    BusEndpoint* endpoint = router.FindEndpoint(epName);

    if (!endpoint) {
        ReleaseLocks();
        return;
    }

    String epNameStr = endpoint->GetUniqueName();
    vector<pair<String, SessionId> > changedSessionMembers;
    SessionMapType::iterator it = sessionMap.begin();
    /* Look through sessionMap for entries matching id */
    while (it != sessionMap.end()) {
        if (it->first.second == id) {
            if (it->first.first == epNameStr) {
                /* Exact key matches are removed */
                sessionMap.erase(it++);
            } else {
                if (endpoint == router.FindEndpoint(it->second.sessionHost)) {
                    /* Modify entry to remove matching sessionHost */
                    it->second.sessionHost.clear();
                    if (it->second.opts.isMultipoint) {
                        changedSessionMembers.push_back(it->first);
                    }
                } else {
                    /* Remove matching session members */
                    vector<String>::iterator mit = it->second.memberNames.begin();
                    while (mit != it->second.memberNames.end()) {
                        if (epNameStr == *mit) {
                            mit = it->second.memberNames.erase(mit);
                            if (it->second.opts.isMultipoint) {
                                changedSessionMembers.push_back(it->first);
                            }
                        } else {
                            ++mit;
                        }
                    }
                }
                /* Session is lost when members + sessionHost together contain only one entry */
                if ((it->second.fd == -1) && (it->second.memberNames.empty() || ((it->second.memberNames.size() == 1) && it->second.sessionHost.empty()))) {
                    SessionMapEntry tsme = it->second;
                    pair<String, SessionId> key = it->first;
                    if (!it->second.isInitializing) {
                        sessionMap.erase(it);
                    }
                    ReleaseLocks();
                    SendSessionLost(tsme);
                    AcquireLocks();
                    it = sessionMap.upper_bound(key);
                } else {
                    ++it;
                }
            }
        } else {
            ++it;
        }
    }
    ReleaseLocks();

    /* Send MPSessionChanged for each changed session involving alias */
    vector<pair<String, SessionId> >::const_iterator csit = changedSessionMembers.begin();
    while (csit != changedSessionMembers.end()) {
        SendMPSessionChanged(csit->second, epNameStr.c_str(), false, csit->first.c_str());
        csit++;
    }
}

void AllJoynObj::RemoveSessionRefs(const String& vepName, const String& b2bEpName)
{
    QCC_DbgTrace(("AllJoynObj::RemoveSessionRefs(%s, %s)",  vepName.c_str(), b2bEpName.c_str()));

    AcquireLocks();
    const VirtualEndpoint* vep = static_cast<const VirtualEndpoint*>(router.FindEndpoint(vepName));
    const RemoteEndpoint* b2bEp = static_cast<const RemoteEndpoint*>(router.FindEndpoint(b2bEpName));

    if (!vep) {
        QCC_LogError(ER_FAIL, ("Virtual endpoint %s disappeared during RemoveSessionRefs", vepName.c_str()));
        ReleaseLocks();
        return;
    }
    if (!b2bEp) {
        QCC_LogError(ER_FAIL, ("B2B endpoint %s disappeared during RemoveSessionRefs", b2bEpName.c_str()));
        ReleaseLocks();
        return;
    }


    vector<pair<String, SessionId> > changedSessionMembers;
    SessionMapType::iterator it = sessionMap.begin();
    while (it != sessionMap.end()) {
        int count;
        /* Skip binding reservations */
        if (it->first.second == 0) {
            ++it;
            continue;
        }
        /* Examine sessions with ids that are affected by removal of vep through b2bep */
        /* Only sessions that route through a single (matching) b2bEp are affected */
        if ((vep->GetBusToBusEndpoint(it->first.second, &count) == b2bEp) && (count == 1)) {
            if (it->first.first == vepName) {
                /* Key matches can be removed from sessionMap */
                sessionMap.erase(it++);
            } else {
                if (vep == router.FindEndpoint(it->second.sessionHost)) {
                    /* If the session's sessionHost is vep, then clear it out of the session */
                    it->second.sessionHost.clear();
                    if (it->second.opts.isMultipoint) {
                        changedSessionMembers.push_back(it->first);
                    }
                } else {
                    /* Clear vep from any session members */
                    vector<String>::iterator mit = it->second.memberNames.begin();
                    while (mit != it->second.memberNames.end()) {
                        if (vepName == *mit) {
                            mit = it->second.memberNames.erase(mit);
                            if (it->second.opts.isMultipoint) {
                                changedSessionMembers.push_back(it->first);
                            }
                        } else {
                            ++mit;
                        }
                    }
                }
                /* A session with only one member and no sessionHost or only a sessionHost are "lost" */
                if ((it->second.fd == -1) && (it->second.memberNames.empty() || ((it->second.memberNames.size() == 1) && it->second.sessionHost.empty()))) {
                    SessionMapEntry tsme = it->second;
                    pair<String, SessionId> key = it->first;
                    if (!it->second.isInitializing) {
                        sessionMap.erase(it);
                    }
                    ReleaseLocks();
                    SendSessionLost(tsme);
                    AcquireLocks();
                    it = sessionMap.upper_bound(key);
                } else {
                    ++it;
                }
            }
        } else {
            ++it;
        }
    }
    ReleaseLocks();

    /* Send MPSessionChanged for each changed session involving alias */
    vector<pair<String, SessionId> >::const_iterator csit = changedSessionMembers.begin();
    while (csit != changedSessionMembers.end()) {
        SendMPSessionChanged(csit->second, vepName.c_str(), false, csit->first.c_str());
        csit++;
    }
}

void AllJoynObj::GetSessionInfo(const InterfaceDescription::Member* member, Message& msg)
{
    /* Received a daemon request for session info */

    /* Parse message args */
    const char* creatorName;
    SessionPort sessionPort;
    SessionOpts optsIn;
    vector<String> busAddrs;

    size_t na;
    const MsgArg* args;
    msg->GetArgs(na, args);
    QStatus status = MsgArg::Get(args, 2, "sq", &creatorName, &sessionPort);
    if (status == ER_OK) {
        status = GetSessionOpts(args[2], optsIn);
    }

    if (status == ER_OK) {
        QCC_DbgTrace(("AllJoynObj::GetSessionInfo(%s, %u, <%x, %x, %x>)", creatorName, sessionPort, optsIn.traffic, optsIn.proximity, optsIn.transports));

        /* Ask the appropriate transport for the listening busAddr */
        TransportList& transList = bus.GetInternal().GetTransportList();
        for (size_t i = 0; i < transList.GetNumTransports(); ++i) {
            Transport* trans = transList.GetTransport(i);
            if (trans && (trans->GetTransportMask() & optsIn.transports)) {
                trans->GetListenAddresses(optsIn, busAddrs);
            } else if (!trans) {
                QCC_LogError(ER_BUS_TRANSPORT_NOT_AVAILABLE, ("NULL transport pointer found in transportList"));
            }
        }
    } else {
        QCC_LogError(status, ("AllJoynObj::GetSessionInfo cannot parse args"));
    }

    if (busAddrs.empty()) {
        status = MethodReply(msg, ER_BUS_NO_SESSION);
    } else {
        MsgArg replyArg("as", busAddrs.size(), NULL, &busAddrs[0]);
        status = MethodReply(msg, &replyArg, 1);
    }

    if (status != ER_OK) {
        QCC_LogError(status, ("GetSessionInfo failed"));
    }
}

QStatus AllJoynObj::SendAttachSession(SessionPort sessionPort,
                                      const char* src,
                                      const char* sessionHost,
                                      const char* dest,
                                      const char* remoteB2BName,
                                      const char* remoteControllerName,
                                      SessionId outgoingSessionId,
                                      const char* busAddr,
                                      const SessionOpts& optsIn,
                                      uint32_t& replyCode,
                                      SessionId& id,
                                      SessionOpts& optsOut,
                                      MsgArg& members)
{
    QStatus status = ER_OK;
    Message reply(bus);
    MsgArg attachArgs[7];
    attachArgs[0].Set("q", sessionPort);
    attachArgs[1].Set("s", src);
    attachArgs[2].Set("s", sessionHost);
    attachArgs[3].Set("s", dest);
    attachArgs[4].Set("s", remoteB2BName);
    attachArgs[5].Set("s", busAddr);
    SetSessionOpts(optsIn, attachArgs[6]);
    ProxyBusObject controllerObj(bus, remoteControllerName, org::alljoyn::Daemon::ObjectPath, outgoingSessionId);
    controllerObj.AddInterface(*daemonIface);

    /* Get a stable reference to the b2bEp */
    AcquireLocks();
    BusEndpoint* ep = router.FindEndpoint(remoteB2BName);
    RemoteEndpoint* b2bEp = (ep && ep->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_BUS2BUS) ?  static_cast<RemoteEndpoint*>(ep) : NULL;
    if (b2bEp) {
        b2bEp->IncrementWaiters();
    } else {
        status = ER_BUS_NO_ENDPOINT;
        QCC_LogError(status, ("Cannot find B2BEp for %s", remoteB2BName));
    }
    ReleaseLocks();

    /* If the new session is raw, then arm the endpoint's RX thread to stop after reading one more message */
    if ((status == ER_OK) && (optsIn.traffic != SessionOpts::TRAFFIC_MESSAGES)) {
        status = b2bEp->PauseAfterRxReply();
    }

    /* Make the method call */
    if (status == ER_OK) {
        QCC_DbgPrintf(("Sending AttachSession(%u, %s, %s, %s, %s, %s, <%x, %x, %x>) to %s",
                       attachArgs[0].v_uint16,
                       attachArgs[1].v_string.str,
                       attachArgs[2].v_string.str,
                       attachArgs[3].v_string.str,
                       attachArgs[4].v_string.str,
                       attachArgs[5].v_string.str,
                       optsIn.proximity, optsIn.traffic, optsIn.transports,
                       remoteControllerName));

        controllerObj.SetB2BEndpoint(b2bEp);
        status = controllerObj.MethodCall(org::alljoyn::Daemon::InterfaceName,
                                          "AttachSession",
                                          attachArgs,
                                          ArraySize(attachArgs),
                                          reply,
                                          30000);
    }

    /* Free the stable reference */
    if (b2bEp) {
        b2bEp->DecrementWaiters();
    }

    if (status != ER_OK) {
        replyCode = ALLJOYN_JOINSESSION_REPLY_FAILED;
        QCC_LogError(status, ("SendAttachSession failed"));
    } else {
        const MsgArg* replyArgs;
        size_t numReplyArgs;
        reply->GetArgs(numReplyArgs, replyArgs);
        replyCode = replyArgs[0].v_uint32;
        id = replyArgs[1].v_uint32;
        status = GetSessionOpts(replyArgs[2], optsOut);
        if (status == ER_OK) {
            members = *reply->GetArg(3);
            QCC_DbgPrintf(("Received AttachSession response: replyCode=%d, sessionId=%u, opts=<%x, %x, %x>",
                           replyCode, id, optsOut.proximity, optsOut.traffic, optsOut.transports));
        } else {
            QCC_DbgPrintf(("Received AttachSession response: <bad_args>"));
        }
    }

    return status;
}

QStatus AllJoynObj::SendJoinSession(SessionPort sessionPort,
                                    SessionId sessionId,
                                    const char* joinerName,
                                    const char* creatorName)
{
    MsgArg args[3];
    args[0].Set("q", sessionPort);
    args[1].Set("u", sessionId);
    args[2].Set("s", joinerName);

    QCC_DbgPrintf(("Calling JoinSession(%u, %u, %s) to %s",
                   args[0].v_uint16,
                   args[1].v_uint32,
                   args[2].v_string.str,
                   creatorName));

    QStatus status = Signal(creatorName, sessionId, *mpSessionJoinedSignal, args, ArraySize(args));
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to send SessionJoined to %s", creatorName));
    }

    return status;
}

QStatus AllJoynObj::SendAcceptSession(SessionPort sessionPort,
                                      SessionId sessionId,
                                      const char* creatorName,
                                      const char* joinerName,
                                      const SessionOpts& inOpts,
                                      bool& isAccepted)
{
    /* Give the receiver a chance to accept or reject the new member */
    Message reply(bus);
    MsgArg acceptArgs[4];
    acceptArgs[0].Set("q", sessionPort);
    acceptArgs[1].Set("u", sessionId);
    acceptArgs[2].Set("s", joinerName);
    SetSessionOpts(inOpts, acceptArgs[3]);
    ProxyBusObject peerObj(bus, creatorName, org::alljoyn::Bus::Peer::ObjectPath, 0);
    const InterfaceDescription* sessionIntf = bus.GetInterface(org::alljoyn::Bus::Peer::Session::InterfaceName);
    assert(sessionIntf);
    peerObj.AddInterface(*sessionIntf);

    QCC_DbgPrintf(("Calling AcceptSession(%d, %u, %s, <%x, %x, %x> to %s",
                   acceptArgs[0].v_uint16,
                   acceptArgs[1].v_uint32,
                   acceptArgs[2].v_string.str,
                   inOpts.proximity, inOpts.traffic, inOpts.transports,
                   creatorName));

    QStatus status = peerObj.MethodCall(org::alljoyn::Bus::Peer::Session::InterfaceName,
                                        "AcceptSession",
                                        acceptArgs,
                                        ArraySize(acceptArgs),
                                        reply);
    if (status == ER_OK) {
        size_t na;
        const MsgArg* replyArgs;
        reply->GetArgs(na, replyArgs);
        replyArgs[0].Get("b", &isAccepted);
    } else {
        isAccepted = false;
    }
    return status;
}

void AllJoynObj::SendSessionLost(const SessionMapEntry& sme)
{
    /* Send SessionLost to the endpoint mentioned in sme */
    Message sigMsg(bus);
    MsgArg args[1];
    args[0].Set("u", sme.id);
    QCC_DbgPrintf(("Sending SessionLost(%u) to %s", sme.id, sme.endpointName.c_str()));
    QStatus status = Signal(sme.endpointName.c_str(), sme.id, *sessionLostSignal, args, ArraySize(args));

    if (ER_OK != status) {
        QCC_LogError(status, ("Failed to send SessionLost to %s", sme.endpointName.c_str()));
    }
}

void AllJoynObj::SendMPSessionChanged(SessionId sessionId, const char* name, bool isAdd, const char* dest)
{
    Message msg(bus);
    MsgArg args[3];
    args[0].Set("u", sessionId);
    args[1].Set("s", name);
    args[2].Set("b", isAdd);
    QCC_DbgPrintf(("Sending MPSessionChanged(%u, %s, %s) to %s", sessionId, name, isAdd ? "true" : "false", dest));
    QStatus status = Signal(dest, 0, *mpSessionChangedSignal, args, ArraySize(args));
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to send MPSessionChanged to %s", dest));
    }
}

QStatus AllJoynObj::SendGetSessionInfo(const char* creatorName,
                                       SessionPort sessionPort,
                                       const SessionOpts& opts,
                                       vector<String>& busAddrs)
{
    QStatus status = ER_BUS_NO_ENDPOINT;

    /* Send GetSessionInfo to creatorName */
    Message reply(bus);
    MsgArg sendArgs[3];
    sendArgs[0].Set("s", creatorName);
    sendArgs[1].Set("q", sessionPort);
    SetSessionOpts(opts, sendArgs[2]);

    BusEndpoint* creatorEp = router.FindEndpoint(creatorName);
    if (creatorEp) {
        String controllerName = creatorEp->GetControllerUniqueName();
        ProxyBusObject rObj(bus, controllerName.c_str(), org::alljoyn::Daemon::ObjectPath, 0);
        const InterfaceDescription* intf = bus.GetInterface(org::alljoyn::Daemon::InterfaceName);
        assert(intf);
        rObj.AddInterface(*intf);
        QCC_DbgPrintf(("Calling GetSessionInfo(%s, %u, <%x, %x, %x>) on %s",
                       sendArgs[0].v_string.str,
                       sendArgs[1].v_uint16,
                       opts.proximity, opts.traffic, opts.transports,
                       controllerName.c_str()));

        status = rObj.MethodCall(org::alljoyn::Daemon::InterfaceName,
                                 "GetSessionInfo",
                                 sendArgs,
                                 ArraySize(sendArgs),
                                 reply);
        if (status == ER_OK) {
            size_t na;
            const MsgArg* replyArgs;
            const MsgArg* busAddrArgs;
            size_t numBusAddrs;
            reply->GetArgs(na, replyArgs);
            replyArgs[0].Get("as", &numBusAddrs, &busAddrArgs);
            for (size_t i = numBusAddrs; i > 0; --i) {
                busAddrs.push_back(busAddrArgs[i - 1].v_string.str);
            }
        }
    }
    return status;
}

QStatus AllJoynObj::ShutdownEndpoint(RemoteEndpoint& b2bEp, SocketFd& sockFd)
{
    SocketStream& ss = static_cast<SocketStream&>(b2bEp.GetStream());
    /* Grab the file descriptor for the B2B endpoint and close the endpoint */
    ss.DetachSocketFd();
    SocketFd epSockFd = ss.GetSocketFd();
    if (!epSockFd) {
        return ER_BUS_NOT_CONNECTED;
    }
    QStatus status = SocketDup(epSockFd, sockFd);
    if (status == ER_OK) {
        status = b2bEp.StopAfterTxEmpty();
        if (status == ER_OK) {
            status = b2bEp.Join();
            if (status != ER_OK) {
                QCC_LogError(status, ("Failed to join RemoteEndpoint used for streaming"));
                sockFd = -1;
            }
        } else {
            QCC_LogError(status, ("Failed to stop RemoteEndpoint used for streaming"));
            sockFd = -1;
        }
    } else {
        QCC_LogError(status, ("Failed to dup remote endpoint's socket"));
        sockFd = -1;
    }
    return status;
}

void AllJoynObj::DetachSessionSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg)
{
    size_t numArgs;
    const MsgArg* args;

    /* Parse message args */
    msg->GetArgs(numArgs, args);
    SessionId id = args[0].v_uint32;
    const char* src = args[1].v_string.str;

    QCC_DbgTrace(("AllJoynObj::DetachSessionSignalHandler(src=%s, id=%u)", src, id));

    /* Do not process our own detach message signals */
    if (::strncmp(guid.ToShortString().c_str(), msg->GetSender() + 1, qcc::GUID128::SHORT_SIZE) == 0) {
        return;
    }

    /* Remove session info from sessionMap */
    RemoveSessionRefs(src, id);

    /* Remove session info from router */
    router.RemoveSessionRoutes(src, id);
}

void AllJoynObj::GetSessionFd(const InterfaceDescription::Member* member, Message& msg)
{
    /* Parse args */
    size_t numArgs;
    const MsgArg* args;
    msg->GetArgs(numArgs, args);
    SessionId id = args[0].v_uint32;
    QStatus status;
    SocketFd sockFd = -1;

    QCC_DbgTrace(("AllJoynObj::GetSessionFd(%u)", id));

    /* Wait for any join related operations to complete before returning fd */
    AcquireLocks();
    SessionMapEntry* smEntry = SessionMapFind(msg->GetSender(), id);
    if (smEntry && (smEntry->opts.traffic != SessionOpts::TRAFFIC_MESSAGES)) {
        uint32_t ts = GetTimestamp();
        while (smEntry && ((sockFd = smEntry->fd) == -1) && ((ts + 5000) > GetTimestamp())) {
            ReleaseLocks();
            qcc::Sleep(5);
            AcquireLocks();
            smEntry = SessionMapFind(msg->GetSender(), id);
        }
        /* sessionMap entry removal was delayed waiting for sockFd to become available. Delete it now. */
        if (sockFd != -1) {
            assert(smEntry);
            SessionMapErase(*smEntry);
        }
    }
    ReleaseLocks();

    if (sockFd != -1) {
        /* Send the fd and transfer ownership */
        MsgArg replyArg;
        replyArg.Set("h", sockFd);
        status = MethodReply(msg, &replyArg, 1);
        qcc::Close(sockFd);
    } else {
        /* Send an error */
        status = MethodReply(msg, ER_BUS_NO_SESSION);
    }

    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to respond to org.alljoyn.Bus.GetSessionFd"));
    }
}

AllJoynObj::SessionMapEntry* AllJoynObj::SessionMapFind(const qcc::String& name, SessionId session)
{
    pair<String, SessionId> key(name, session);
    AllJoynObj::SessionMapType::iterator it = sessionMap.find(key);
    if (it == sessionMap.end()) {
        return NULL;
    } else {
        return &(it->second);
    }
}

AllJoynObj::SessionMapType::iterator AllJoynObj::SessionMapLowerBound(const qcc::String& name, SessionId session)
{
    pair<String, SessionId> key(name, session);
    return sessionMap.lower_bound(key);
}

void AllJoynObj::SessionMapInsert(SessionMapEntry& sme)
{
    pair<String, SessionId> key(sme.endpointName, sme.id);
    sessionMap.insert(pair<pair<String, SessionId>, SessionMapEntry>(key, sme));
}

void AllJoynObj::SessionMapErase(SessionMapEntry& sme)
{
    pair<String, SessionId> key(sme.endpointName, sme.id);
    sessionMap.erase(key);
}

void AllJoynObj::SetLinkTimeout(const InterfaceDescription::Member* member, Message& msg)
{
    /* Parse args */
    size_t numArgs;
    const MsgArg* args;
    msg->GetArgs(numArgs, args);
    SessionId id = args[0].v_uint32;
    uint32_t reqLinkTimeout = args[1].v_uint32;
    uint32_t actLinkTimeout = reqLinkTimeout;
    bool foundEp = false;
    uint32_t disposition;
    QStatus status = ER_OK;

    /* Set the link timeout on all endpoints that are involved in this session */
    AcquireLocks();
    SessionMapType::iterator it = SessionMapLowerBound(msg->GetSender(), id);

    while ((it != sessionMap.end()) && (it->first.first == msg->GetSender()) && (it->first.second == id)) {
        SessionMapEntry& entry = it->second;
        if (entry.opts.traffic == SessionOpts::TRAFFIC_MESSAGES) {
            vector<String> memberNames = entry.memberNames;
            memberNames.push_back(entry.sessionHost);
            for (size_t i = 0; i < memberNames.size(); ++i) {
                BusEndpoint* memberEp = router.FindEndpoint(memberNames[i]);
                if (memberEp && (memberEp->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) {
                    VirtualEndpoint* vMemberEp = static_cast<VirtualEndpoint*>(memberEp);
                    RemoteEndpoint* b2bEp = vMemberEp->GetBusToBusEndpoint(id);
                    if (b2bEp) {
                        uint32_t tTimeout = reqLinkTimeout;
                        QStatus tStatus = b2bEp->SetLinkTimeout(tTimeout);
                        status = (status == ER_OK) ? tStatus : status;
                        actLinkTimeout = ((tTimeout == 0) || (actLinkTimeout == 0)) ? 0 : max(actLinkTimeout, tTimeout);
                        foundEp = true;
                    }
                } else if (memberEp && ((memberEp->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_REMOTE) ||
                                        (memberEp->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_NULL))) {
                    /*
                     * This is a locally connected client. These clients do not have per-session connecions
                     * therefore we silently allow this as if we had granted the user's request
                     */
                    foundEp = true;
                }
            }
        }
        ++it;
    }
    ReleaseLocks();

    /* Set disposition */
    if (status == ER_ALLJOYN_SETLINKTIMEOUT_REPLY_NO_DEST_SUPPORT) {
        disposition = ALLJOYN_SETLINKTIMEOUT_REPLY_NO_DEST_SUPPORT;
    } else if (!foundEp) {
        disposition = ALLJOYN_SETLINKTIMEOUT_REPLY_NO_SESSION;
        actLinkTimeout = 0;
    } else if (status != ER_OK) {
        disposition = ALLJOYN_SETLINKTIMEOUT_REPLY_FAILED;
        actLinkTimeout = 0;
    } else {
        disposition = ALLJOYN_SETLINKTIMEOUT_REPLY_SUCCESS;
    }

    /* Send response */
    MsgArg replyArgs[2];
    replyArgs[0].Set("u", disposition);
    replyArgs[1].Set("u", actLinkTimeout);
    status = MethodReply(msg, replyArgs, ArraySize(replyArgs));
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to respond to org.alljoyn.Bus.SetLinkTimeout"));
    }
    QCC_DbgTrace(("AllJoynObj::SetLinkTimeout(%u, %d) (status=%s, disp=%d, lto=%d)", id, reqLinkTimeout,
                  QCC_StatusText(status), disposition, actLinkTimeout));
}

void AllJoynObj::AliasUnixUser(const InterfaceDescription::Member* member, Message& msg)
{
    uint32_t replyCode = ALLJOYN_ALIASUNIXUSER_REPLY_SUCCESS;
    /* Parse args */
    size_t numArgs;
    const MsgArg* args;
    msg->GetArgs(numArgs, args);
    uint32_t aliasUID = args[0].v_uint32;

#if defined(QCC_OS_ANDROID)
    QStatus status = ER_OK;
    uint32_t origUID = 0;
    qcc::String sender = msg->GetSender();
    BusEndpoint* srcEp = router.FindEndpoint(sender);

    if (!srcEp) {
        status = ER_BUS_NO_ENDPOINT;
        QCC_LogError(status, ("AliasUnixUser Failed to find endpoint for sender=%s", sender.c_str()));
        replyCode = ALLJOYN_ALIASUNIXUSER_REPLY_FAILED;
    } else {
        origUID = srcEp->GetUserId();
        if (origUID == (uint32_t)-1 || aliasUID == (uint32_t)-1) {
            QCC_LogError(ER_FAIL, ("AliasUnixUser Invalid user id origUID=%d aliasUID=%d", origUID, aliasUID));
            replyCode = ALLJOYN_ALIASUNIXUSER_REPLY_FAILED;
        }
    }

    if (replyCode == ALLJOYN_ALIASUNIXUSER_REPLY_SUCCESS) {
        if (PermissionDB::GetDB().AddAliasUnixUser(origUID, aliasUID) != ER_OK) {
            replyCode = ALLJOYN_ALIASUNIXUSER_REPLY_FAILED;
        }
    }
#else
    replyCode = ALLJOYN_ALIASUNIXUSER_REPLY_NO_SUPPORT;
#endif
    /* Send response */
    MsgArg replyArg;
    replyArg.Set("u", replyCode);
    MethodReply(msg, &replyArg, 1);
    QCC_DbgPrintf(("AllJoynObj::AliasUnixUser(%d) returned %d", aliasUID, replyCode));
}

void AllJoynObj::AdvertiseName(const InterfaceDescription::Member* member, Message& msg)
{
    uint32_t replyCode = ALLJOYN_ADVERTISENAME_REPLY_SUCCESS;
    size_t numArgs;
    const MsgArg* args;
    MsgArg replyArg;
    const char* advertiseName;
    TransportMask transports = 0;

    /* Get AdvertiseName args */
    msg->GetArgs(numArgs, args);
    QStatus status = MsgArg::Get(args, numArgs, "sq", &advertiseName, &transports);
    QCC_DbgTrace(("AllJoynObj::AdvertiseName(%s, %x)", (status == ER_OK) ? advertiseName : "", transports));

    /* Get the sender name */
    qcc::String sender = msg->GetSender();

    if (status == ER_OK) {
        status = CheckTransportsPermission(sender, transports, "AdvertiseName");
    }

    /* Check to see if the advertise name is valid and well formed */
    if (IsLegalBusName(advertiseName)) {

        /* Check to see if advertiseName is already being advertised */
        AcquireLocks();
        String advertiseNameStr = advertiseName;
        multimap<qcc::String, pair<TransportMask, qcc::String> >::iterator it = advertiseMap.find(advertiseNameStr);

        while ((it != advertiseMap.end()) && (it->first == advertiseNameStr)) {
            if (it->second.second == sender) {
                if ((it->second.first & transports) != 0) {
                    replyCode = ALLJOYN_ADVERTISENAME_REPLY_ALREADY_ADVERTISING;
                }
                break;
            }
            ++it;
        }

        if (ALLJOYN_ADVERTISENAME_REPLY_SUCCESS == replyCode) {
            /* Add to advertise map */
            if (it == advertiseMap.end()) {
                advertiseMap.insert(pair<qcc::String, pair<TransportMask, qcc::String> >(advertiseNameStr, pair<TransportMask, String>(transports, sender)));
            } else {
                it->second.first |= transports;
            }

            /* Advertise on transports specified */
            TransportList& transList = bus.GetInternal().GetTransportList();
            status = ER_BUS_BAD_SESSION_OPTS;
            for (size_t i = 0; i < transList.GetNumTransports(); ++i) {
                Transport* trans = transList.GetTransport(i);
                if (trans && trans->IsBusToBus() && (trans->GetTransportMask() & transports)) {
                    status = trans->EnableAdvertisement(advertiseNameStr);
                    if ((status != ER_OK) && (status != ER_NOT_IMPLEMENTED)) {
                        QCC_LogError(status, ("EnableAdvertisment failed for transport %s - mask=0x%x", trans->GetTransportName(), transports));
                    }
                } else if (!trans) {
                    QCC_LogError(ER_BUS_TRANSPORT_NOT_AVAILABLE, ("NULL transport pointer found in transportList"));
                }
            }
        }
        ReleaseLocks();
    } else {
        replyCode = ALLJOYN_ADVERTISENAME_REPLY_FAILED;
    }

    /* Reply to request */
    String advNameStr = advertiseName;   /* Needed since advertiseName will be corrupt after MethodReply */
    replyArg.Set("u", replyCode);
    status = MethodReply(msg, &replyArg, 1);

    QCC_DbgPrintf(("AllJoynObj::Advertise(%s) returned %d (status=%s)", advNameStr.c_str(), replyCode, QCC_StatusText(status)));

    /* Add advertisement to local nameMap so local discoverers can see this advertisement */
    if ((replyCode == ALLJOYN_ADVERTISENAME_REPLY_SUCCESS) && (transports & TRANSPORT_LOCAL)) {
        vector<String> names;
        names.push_back(advNameStr);
        FoundNames("local:", bus.GetGlobalGUIDString(), TRANSPORT_LOCAL, &names, numeric_limits<uint8_t>::max());
    }

    /* Log error if reply could not be sent */
    if (ER_OK != status) {
        QCC_LogError(status, ("Failed to respond to org.alljoyn.Bus.Advertise"));
    }
}

void AllJoynObj::CancelAdvertiseName(const InterfaceDescription::Member* member, Message& msg)
{
    const MsgArg* args;
    size_t numArgs;

    /* Get the name being advertised */
    msg->GetArgs(numArgs, args);
    const char* advertiseName;
    TransportMask transports = 0;
    QStatus status = MsgArg::Get(args, numArgs, "sq", &advertiseName, &transports);
    if (status != ER_OK) {
        QCC_LogError(status, ("CancelAdvertiseName: bad arg types"));
        return;
    }

    QCC_DbgTrace(("AllJoynObj::CancelAdvertiseName(%s, 0x%x)", advertiseName, transports));

    /* Cancel advertisement */
    status = ProcCancelAdvertise(msg->GetSender(), advertiseName, transports);
    uint32_t replyCode = (ER_OK == status) ? ALLJOYN_CANCELADVERTISENAME_REPLY_SUCCESS : ALLJOYN_CANCELADVERTISENAME_REPLY_FAILED;

    /* Reply to request */
    String advNameStr = advertiseName;   /* Needed since advertiseName will be corrupt after MethodReply */
    MsgArg replyArg("u", replyCode);
    status = MethodReply(msg, &replyArg, 1);

    /* Remove advertisement from local nameMap so local discoverers are notified of advertisement going away */
    if ((replyCode == ALLJOYN_ADVERTISENAME_REPLY_SUCCESS) && (transports & TRANSPORT_LOCAL)) {
        vector<String> names;
        names.push_back(advNameStr);
        FoundNames("local:", bus.GetGlobalGUIDString(), TRANSPORT_LOCAL, &names, 0);
    }

    /* Log error if reply could not be sent */
    if (ER_OK != status) {
        QCC_LogError(status, ("Failed to respond to org.alljoyn.Bus.CancelAdvertise"));
    }
}

QStatus AllJoynObj::ProcCancelAdvertise(const qcc::String& sender, const qcc::String& advertiseName, TransportMask transports)
{
    QCC_DbgTrace(("AllJoynObj::ProcCancelAdvertise(%s, %s, %x)",
                  sender.c_str(),
                  advertiseName.c_str(),
                  transports));

    QStatus status = ER_OK;

    /* Check to see if this advertised name exists and delete it */
    bool foundAdvert = false;
    bool advertHasRefs = false;

    AcquireLocks();
    multimap<qcc::String, pair<TransportMask, qcc::String> >::iterator it = advertiseMap.find(advertiseName);
    while ((it != advertiseMap.end()) && (it->first == advertiseName)) {
        if (it->second.second == sender) {
            foundAdvert = true;
            it->second.first &= ~transports;
            if (it->second.first == 0) {
                advertiseMap.erase(it++);
            } else {
                ++it;
            }
        } else {
            advertHasRefs = true;
            ++it;
        }
    }
    ReleaseLocks();

    /* Cancel transport advertisement if no other refs exist */
    if (foundAdvert && !advertHasRefs) {
        TransportList& transList = bus.GetInternal().GetTransportList();
        for (size_t i = 0; i < transList.GetNumTransports(); ++i) {
            Transport* trans = transList.GetTransport(i);
            if (trans && (trans->GetTransportMask() & transports)) {
                trans->DisableAdvertisement(advertiseName, advertiseMap.empty());
            } else if (!trans) {
                QCC_LogError(ER_BUS_TRANSPORT_NOT_AVAILABLE, ("NULL transport pointer found in transportList"));
            }
        }

        if (discoverMap.empty() && advertiseMap.empty()) {
            std::multimap<qcc::String, NameMapEntry>::iterator nmit = nameMap.begin();
            while (nmit != nameMap.end()) {
                if ((*nmit).second.transport & (TRANSPORT_WLAN | TRANSPORT_WWAN | TRANSPORT_LAN)) {
                    nameMap.erase(nmit++);
                } else {
                    ++nmit;
                }
            }
        }

    } else if (!foundAdvert) {
        status = ER_FAIL;
    }
    return status;
}

void AllJoynObj::GetAdvertisedNames(std::vector<qcc::String>& names)
{
    AcquireLocks();
    multimap<qcc::String, pair<TransportMask, qcc::String> >::const_iterator it(advertiseMap.begin());
    while (it != advertiseMap.end()) {
        const qcc::String& name(it->first);
        QCC_DbgPrintf(("AllJoynObj::GetAdvertisedNames - Name[%u] = %s", names.size(), name.c_str()));
        names.push_back(name);
        // skip to next name
        it = advertiseMap.upper_bound(name);
    }
    ReleaseLocks();
}

void AllJoynObj::FindAdvertisedName(const InterfaceDescription::Member* member, Message& msg)
{
    uint32_t replyCode;
    size_t numArgs;
    const MsgArg* args;
    TransportMask transForbidden = 0;
    /* Get the name prefix */
    msg->GetArgs(numArgs, args);
    assert((numArgs == 1) && (args[0].typeId == ALLJOYN_STRING));
    String namePrefix = args[0].v_string.str;

    QCC_DbgTrace(("AllJoynObj::FindAdvertisedName(%s)", namePrefix.c_str()));

    /* Check to see if this endpoint is already discovering this prefix */
    qcc::String sender = msg->GetSender();
    replyCode = ALLJOYN_FINDADVERTISEDNAME_REPLY_SUCCESS;
    AcquireLocks();
    BusEndpoint* srcEp = router.FindEndpoint(sender);
    uint32_t uid = srcEp ? srcEp->GetUserId() : -1;
    multimap<qcc::String, qcc::String>::const_iterator it = discoverMap.find(namePrefix);
    while ((it != discoverMap.end()) && (it->first == namePrefix)) {
        if (it->second == sender) {
            replyCode = ALLJOYN_FINDADVERTISEDNAME_REPLY_ALREADY_DISCOVERING;
            break;
        }
        ++it;
    }
    if (ALLJOYN_FINDADVERTISEDNAME_REPLY_SUCCESS == replyCode) {
        /* Notify transports if this is a new prefix */
        bool notifyTransports = (discoverMap.find(namePrefix) == discoverMap.end());

        /* Add to discover map */
        discoverMap.insert(pair<String, String>(namePrefix, sender));

        /* Add entry to denote that the sender is not allowed to discover the service over the forbidden transports*/
        if (transForbidden) {
            transForbidMap.insert(pair<qcc::String, pair<TransportMask, qcc::String> >(namePrefix, pair<TransportMask, String>(transForbidden, sender)));
        }

        /* Find name on all remote transports */
        ReleaseLocks();
        if (notifyTransports) {
            TransportList& transList = bus.GetInternal().GetTransportList();
            for (size_t i = 0; i < transList.GetNumTransports(); ++i) {
                Transport* trans = transList.GetTransport(i);
                if (trans && (uid != static_cast<uint32_t>(-1))) {
#if defined(QCC_OS_ANDROID)
                    if (trans->GetTransportMask() & TRANSPORT_BLUETOOTH && !PermissionDB::GetDB().IsBluetoothAllowed(uid)) {
                        QCC_LogError(ER_ALLJOYN_ACCESS_PERMISSION_WARNING, ("AllJoynObj::FindAdvertisedName WARNING: No permission to use Bluetooth"));
                        transForbidden |= TRANSPORT_BLUETOOTH;
                        continue;
                    }
                    if (trans->GetTransportMask() & TRANSPORT_WLAN && !PermissionDB::GetDB().IsWifiAllowed(uid)) {
                        QCC_LogError(ER_ALLJOYN_ACCESS_PERMISSION_WARNING, ("AllJoynObj::FindAdvertisedName WARNING: No permission to use Wifi"));
                        transForbidden |= TRANSPORT_WLAN;
                        continue;
                    }
#endif
                    trans->EnableDiscovery(namePrefix.c_str());
                } else {
                    QCC_LogError(ER_BUS_TRANSPORT_NOT_AVAILABLE, ("NULL transport pointer found in transportList"));
                }
            }
        }
    } else {
        ReleaseLocks();
    }

    /* Reply to request */
    MsgArg replyArg("u", replyCode);
    QStatus status = MethodReply(msg, &replyArg, 1);
    QCC_DbgPrintf(("AllJoynObj::FindAdvertisedName(%s) returned %d (status=%s)", namePrefix.c_str(), replyCode, QCC_StatusText(status)));

    /* Log error if reply could not be sent */
    if (ER_OK != status) {
        QCC_LogError(status, ("Failed to respond to org.alljoyn.Bus.Discover"));
    }

    /* Send FoundAdvertisedName signals if there are existing matches for namePrefix */
    if (ALLJOYN_FINDADVERTISEDNAME_REPLY_SUCCESS == replyCode) {
        AcquireLocks();
        multimap<String, NameMapEntry>::iterator it = nameMap.lower_bound(namePrefix);
        set<pair<String, TransportMask> > sentSet;
        while ((it != nameMap.end()) && (0 == strncmp(it->first.c_str(), namePrefix.c_str(), namePrefix.size()))) {
            /* If this transport is forbidden to use, then skip the advertised name */
            if ((it->second.transport & transForbidden) != 0) {
                QCC_DbgPrintf(("AllJoynObj::FindAdvertisedName(%s): forbid to send existing advertised name %s over transport %d to %s due to lack of permission",
                               namePrefix.c_str(), it->first.c_str(), it->second.transport, sender.c_str()));
                ++it;
                continue;
            }
            pair<String, TransportMask> sentSetEntry(it->first, it->second.transport);
            if (sentSet.find(sentSetEntry) == sentSet.end()) {
                String foundName = it->first;
                NameMapEntry nme = it->second;
                ReleaseLocks();
                status = SendFoundAdvertisedName(sender, foundName, nme.transport, namePrefix);
                AcquireLocks();
                it = nameMap.lower_bound(namePrefix);
                sentSet.insert(sentSetEntry);
                if (ER_OK != status) {
                    QCC_LogError(status, ("Cannot send FoundAdvertisedName to %s for name=%s", sender.c_str(), foundName.c_str()));
                }
            } else {
                ++it;
            }
        }
        ReleaseLocks();
    }
}

void AllJoynObj::CancelFindAdvertisedName(const InterfaceDescription::Member* member, Message& msg)
{
    size_t numArgs;
    const MsgArg* args;

    /* Get the name prefix to be removed from discovery */
    msg->GetArgs(numArgs, args);
    assert((numArgs == 1) && (args[0].typeId == ALLJOYN_STRING));

    /* Cancel advertisement */
    QCC_DbgPrintf(("Calling ProcCancelFindName from CancelFindAdvertisedName [%s]", Thread::GetThread()->GetName()));
    QStatus status = ProcCancelFindName(msg->GetSender(), args[0].v_string.str);
    uint32_t replyCode = (ER_OK == status) ? ALLJOYN_CANCELFINDADVERTISEDNAME_REPLY_SUCCESS : ALLJOYN_CANCELFINDADVERTISEDNAME_REPLY_FAILED;

    /* Reply to request */
    MsgArg replyArg("u", replyCode);
    status = MethodReply(msg, &replyArg, 1);
    // QCC_DbgPrintf(("AllJoynObj::CancelDiscover(%s) returned %d (status=%s)", args[0].v_string.str, replyCode, QCC_StatusText(status)));

    /* Log error if reply could not be sent */
    if (ER_OK != status) {
        QCC_LogError(status, ("Failed to respond to org.alljoyn.Bus.CancelDiscover"));
    }
}

QStatus AllJoynObj::ProcCancelFindName(const qcc::String& sender, const qcc::String& namePrefix)
{
    QCC_DbgTrace(("AllJoynObj::ProcCancelFindName(sender = %s, namePrefix = %s)", sender.c_str(), namePrefix.c_str()));
    QStatus status = ER_OK;

    /* Check to see if this prefix exists and delete it */
    bool foundNamePrefix = false;
    AcquireLocks();
    multimap<qcc::String, qcc::String>::iterator it = discoverMap.lower_bound(namePrefix);
    while ((it != discoverMap.end()) && (it->first == namePrefix)) {
        if (it->second == sender) {
            discoverMap.erase(it);
            foundNamePrefix = true;
            break;
        }
        ++it;
    }

    /* Check and delete the transport restriction info on this sender and prefix*/
    multimap<String, std::pair<TransportMask, String> >::iterator forbidIt = transForbidMap.lower_bound(namePrefix);
    while ((forbidIt != transForbidMap.end()) && (forbidIt->first == namePrefix)) {
        if (forbidIt->second.second == sender) {
            transForbidMap.erase(forbidIt);
            break;
        }
        ++forbidIt;
    }

    /* Disable discovery if we removed the last discoverMap entry with a given prefix */
    bool isLastEntry = (discoverMap.find(namePrefix) == discoverMap.end());
    if (foundNamePrefix && isLastEntry) {
        TransportList& transList = bus.GetInternal().GetTransportList();
        for (size_t i = 0; i < transList.GetNumTransports(); ++i) {
            Transport* trans =  transList.GetTransport(i);
            if (trans) {
                trans->DisableDiscovery(namePrefix.c_str());
            } else {
                QCC_LogError(ER_BUS_TRANSPORT_NOT_AVAILABLE, ("NULL transport pointer found in transportList"));
            }
        }

        if (discoverMap.empty() && advertiseMap.empty()) {
            std::multimap<qcc::String, NameMapEntry>::iterator nmit = nameMap.begin();
            while (nmit != nameMap.end()) {
                if ((*nmit).second.transport & (TRANSPORT_WLAN | TRANSPORT_WWAN | TRANSPORT_LAN)) {
                    nameMap.erase(nmit++);
                } else {
                    ++nmit;
                }
            }
        }

    } else if (!foundNamePrefix) {
        status = ER_FAIL;
    }
    ReleaseLocks();
    return status;
}

QStatus AllJoynObj::AddBusToBusEndpoint(RemoteEndpoint& endpoint)
{
    QCC_DbgTrace(("AllJoynObj::AddBusToBusEndpoint(%s)", endpoint.GetUniqueName().c_str()));

    const qcc::String& shortGuidStr = endpoint.GetRemoteGUID().ToShortString();

    /* Add b2b endpoint */
    AcquireLocks();
    b2bEndpoints[endpoint.GetUniqueName()] = &endpoint;
    ReleaseLocks();

    /* Create a virtual endpoint for talking to the remote bus control object */
    /* This endpoint will also carry broadcast messages for the remote bus */
    String remoteControllerName(":", 1, 16);
    remoteControllerName.append(shortGuidStr);
    remoteControllerName.append(".1");
    AddVirtualEndpoint(remoteControllerName, endpoint);

    /* Exchange existing bus names if connected to another daemon */
    return ExchangeNames(endpoint);
}

void AllJoynObj::RemoveBusToBusEndpoint(RemoteEndpoint& endpoint)
{
    QCC_DbgTrace(("AllJoynObj::RemoveBusToBusEndpoint(%s)", endpoint.GetUniqueName().c_str()));

    /* Be careful to lock the name table before locking the virtual endpoints since both locks are needed
     * and doing it in the opposite order invites deadlock
     */
    AcquireLocks();
    String b2bEpName = endpoint.GetUniqueName();

    /* Get session ids affected by loss of this B2B endpoint */
    set<SessionId> idSet;
    map<qcc::String, VirtualEndpoint*>::iterator it = virtualEndpoints.begin();
    while (it != virtualEndpoints.end()) {
        it->second->GetSessionIdsForB2B(endpoint, idSet);
        ++it;
    }

    /* Remove any virtual endpoints associated with a removed bus-to-bus endpoint */
    it = virtualEndpoints.begin();
    while (it != virtualEndpoints.end()) {
        /* Clean sessionMap and report lost sessions */

        /*
         * Remove the sessionMap entries involving endpoint
         * This call must be made without holding locks since it can trigger LostSession callback
         */
        String vepName = it->first;
        ReleaseLocks();
        RemoveSessionRefs(vepName, b2bEpName);
        AcquireLocks();
        it = virtualEndpoints.find(vepName);
        if (it == virtualEndpoints.end()) {
            break;
        }

        /* Remove endpoint (b2b) reference from this vep */
        if (it->second->RemoveBusToBusEndpoint(endpoint)) {
            String exitingEpName = it->second->GetUniqueName();

            /* Let directly connected daemons know that this virtual endpoint is gone. */
            map<qcc::StringMapKey, RemoteEndpoint*>::iterator it2 = b2bEndpoints.begin();
            const qcc::GUID128& otherSideGuid = endpoint.GetRemoteGUID();
            while ((it2 != b2bEndpoints.end()) && (it != virtualEndpoints.end())) {
                if ((it2->second != &endpoint) && (it2->second->GetRemoteGUID() != otherSideGuid)) {
                    Message sigMsg(bus);
                    MsgArg args[3];
                    args[0].Set("s", exitingEpName.c_str());
                    args[1].Set("s", exitingEpName.c_str());
                    args[2].Set("s", "");

                    QStatus status = sigMsg->SignalMsg("sss",
                                                       org::alljoyn::Daemon::WellKnownName,
                                                       0,
                                                       org::alljoyn::Daemon::ObjectPath,
                                                       org::alljoyn::Daemon::InterfaceName,
                                                       "NameChanged",
                                                       args,
                                                       ArraySize(args),
                                                       0,
                                                       0);
                    if (ER_OK == status) {
                        String key = it->first;
                        String key2 = it2->first.c_str();
                        RemoteEndpoint*ep = it2->second;
                        ep->IncrementWaiters();
                        ReleaseLocks();
                        status = ep->PushMessage(sigMsg);
                        if (ER_OK != status) {
                            QCC_LogError(status, ("Failed to send NameChanged to %s", ep->GetUniqueName().c_str()));
                        }
                        ep->DecrementWaiters();
                        AcquireLocks();
                        it2 = b2bEndpoints.lower_bound(key2);
                        if ((it2 != b2bEndpoints.end()) && (it2->first == key2)) {
                            ++it2;
                        }
                        it = virtualEndpoints.lower_bound(key);
                    } else {
                        ++it2;;
                    }
                } else {
                    ++it2;
                }
            }

            /* Remove virtual endpoint with no more b2b eps */
            if (it != virtualEndpoints.end()) {
                RemoveVirtualEndpoint(*(it++->second));
            }

        } else {
            ++it;
        }
    }

    /* Remove the B2B endpoint itself */
    b2bEndpoints.erase(endpoint.GetUniqueName());
    ReleaseLocks();
}

QStatus AllJoynObj::ExchangeNames(RemoteEndpoint& endpoint)
{
    QCC_DbgTrace(("AllJoynObj::ExchangeNames(endpoint = %s)", endpoint.GetUniqueName().c_str()));

    vector<pair<qcc::String, vector<qcc::String> > > names;
    QStatus status;

    /* Send local name table info to remote bus controller */
    AcquireLocks();
    router.GetUniqueNamesAndAliases(names);

    MsgArg argArray(ALLJOYN_ARRAY);
    MsgArg* entries = new MsgArg[names.size()];
    size_t numEntries = 0;
    vector<pair<qcc::String, vector<qcc::String> > >::const_iterator it = names.begin();

    /* Send all endpoint info except for endpoints related to destination */
    while (it != names.end()) {
        BusEndpoint* ep = router.FindEndpoint(it->first);
        VirtualEndpoint* vep = (ep && (ep->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) ? static_cast<VirtualEndpoint*>(ep) : NULL;
        if (ep && (!vep || vep->CanRouteWithout(endpoint.GetRemoteGUID()))) {
            MsgArg* aliasNames = new MsgArg[it->second.size()];
            vector<qcc::String>::const_iterator ait = it->second.begin();
            size_t numAliases = 0;
            while (ait != it->second.end()) {
                /* Send exportable endpoints */
                // if ((ait->size() > guidLen) && (0 == ait->compare(ait->size() - guidLen, guidLen, guidStr))) {
                if (true) {
                    aliasNames[numAliases++].Set("s", ait->c_str());
                }
                ++ait;
            }
            if (0 < numAliases) {
                entries[numEntries].Set("(sa*)", it->first.c_str(), numAliases, aliasNames);
                /*
                 * Set ownwership flag so entries array destructor will free inner message args.
                 */
                entries[numEntries].SetOwnershipFlags(MsgArg::OwnsArgs, true);
            } else {
                entries[numEntries].Set("(sas)", it->first.c_str(), 0, NULL);
                delete[] aliasNames;
            }
            ++numEntries;
        }
        ++it;
    }
    status = argArray.Set("a(sas)", numEntries, entries);
    if (ER_OK == status) {
        Message exchangeMsg(bus);
        status = exchangeMsg->SignalMsg("a(sas)",
                                        org::alljoyn::Daemon::WellKnownName,
                                        0,
                                        org::alljoyn::Daemon::ObjectPath,
                                        org::alljoyn::Daemon::InterfaceName,
                                        "ExchangeNames",
                                        &argArray,
                                        1,
                                        0,
                                        0);
        if (ER_OK == status) {
            endpoint.IncrementWaiters();
            ReleaseLocks();
            status = endpoint.PushMessage(exchangeMsg);
            endpoint.DecrementWaiters();
            AcquireLocks();
        }
    }
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to send ExchangeName signal"));
    }
    ReleaseLocks();

    /*
     * This will also free the inner MsgArgs.
     */
    delete [] entries;
    return status;
}

void AllJoynObj::ExchangeNamesSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg)
{
    QCC_DbgTrace(("AllJoynObj::ExchangeNamesSignalHandler(msg sender = \"%s\")", msg->GetSender()));

    bool madeChanges = false;
    size_t numArgs;
    const MsgArg* args;
    msg->GetArgs(numArgs, args);
    assert((1 == numArgs) && (ALLJOYN_ARRAY == args[0].typeId));
    const MsgArg* items = args[0].v_array.GetElements();
    const String& shortGuidStr = guid.ToShortString();

    /* Create a virtual endpoint for each unique name in args */
    /* Be careful to lock the name table before locking the virtual endpoints since both locks are needed
     * and doing it in the opposite order invites deadlock
     */
    AcquireLocks();
    map<qcc::StringMapKey, RemoteEndpoint*>::iterator bit = b2bEndpoints.find(msg->GetRcvEndpointName());
    const size_t numItems = args[0].v_array.GetNumElements();
    if (bit != b2bEndpoints.end()) {
        qcc::GUID128 otherGuid = bit->second->GetRemoteGUID();
        bit = b2bEndpoints.begin();;
        while (bit != b2bEndpoints.end()) {
            if (bit->second->GetRemoteGUID() == otherGuid) {
                for (size_t i = 0; i < numItems; ++i) {
                    assert(items[i].typeId == ALLJOYN_STRUCT);
                    qcc::String uniqueName = items[i].v_struct.members[0].v_string.str;
                    if (!IsLegalUniqueName(uniqueName.c_str())) {
                        QCC_LogError(ER_FAIL, ("Invalid unique name \"%s\" in ExchangeNames message", uniqueName.c_str()));
                        continue;
                    } else if (0 == ::strncmp(uniqueName.c_str() + 1, shortGuidStr.c_str(), shortGuidStr.size())) {
                        /* Cant accept a request to change a local name */
                        continue;
                    }

                    bool madeChange;
                    VirtualEndpoint& vep = AddVirtualEndpoint(uniqueName, *(bit->second), &madeChange);

                    if (madeChange) {
                        madeChanges = true;
                    }

                    /* Add virtual aliases (remote well-known names) */
                    const MsgArg* aliasItems = items[i].v_struct.members[1].v_array.GetElements();
                    const size_t numAliases = items[i].v_struct.members[1].v_array.GetNumElements();
                    for (size_t j = 0; j < numAliases; ++j) {
                        assert(ALLJOYN_STRING == aliasItems[j].typeId);
                        bool madeChange = router.SetVirtualAlias(aliasItems[j].v_string.str, &vep, vep);
                        if (madeChange) {
                            madeChanges = true;
                        }
                    }
                }
            }
            ++bit;
        }
    } else {
        QCC_LogError(ER_BUS_NO_ENDPOINT, ("Cannot find b2b endpoint %s", msg->GetRcvEndpointName()));
    }
    ReleaseLocks();

    /* If there were changes, forward message to all directly connected controllers except the one that
     * sent us this ExchangeNames
     */
    if (madeChanges) {
        AcquireLocks();
        map<qcc::StringMapKey, RemoteEndpoint*>::const_iterator bit = b2bEndpoints.find(msg->GetRcvEndpointName());
        map<qcc::StringMapKey, RemoteEndpoint*>::iterator it = b2bEndpoints.begin();
        while (it != b2bEndpoints.end()) {
            if ((bit == b2bEndpoints.end()) || (bit->second->GetRemoteGUID() != it->second->GetRemoteGUID())) {
                QCC_DbgPrintf(("Propagating ExchangeName signal to %s", it->second->GetUniqueName().c_str()));
                StringMapKey key = it->first;
                RemoteEndpoint*ep = it->second;
                ep->IncrementWaiters();
                ReleaseLocks();
                QStatus status = ep->PushMessage(msg);
                if (ER_OK != status) {
                    QCC_LogError(status, ("Failed to forward ExchangeNames to %s", ep->GetUniqueName().c_str()));
                }
                ep->DecrementWaiters();
                AcquireLocks();
                it = b2bEndpoints.lower_bound(key);
                if ((it != b2bEndpoints.end()) && (it->first == key)) {
                    ++it;
                }
            } else {
                ++it;
            }
        }
        ReleaseLocks();
    }
}

void AllJoynObj::NameChangedSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg)
{
    size_t numArgs;
    const MsgArg* args;
    msg->GetArgs(numArgs, args);

    assert(daemonIface);

    const qcc::String alias = args[0].v_string.str;
    const qcc::String oldOwner = args[1].v_string.str;
    const qcc::String newOwner = args[2].v_string.str;

    const String& shortGuidStr = guid.ToShortString();
    bool madeChanges = false;

    QCC_DbgPrintf(("AllJoynObj::NameChangedSignalHandler: alias = \"%s\"   oldOwner = \"%s\"   newOwner = \"%s\"  sent from \"%s\"",
                   alias.c_str(), oldOwner.c_str(), newOwner.c_str(), msg->GetSender()));

    /* Don't allow a NameChange that attempts to change a local name */
    if ((!oldOwner.empty() && (0 == ::strncmp(oldOwner.c_str() + 1, shortGuidStr.c_str(), shortGuidStr.size()))) ||
        (!newOwner.empty() && (0 == ::strncmp(newOwner.c_str() + 1, shortGuidStr.c_str(), shortGuidStr.size())))) {
        return;
    }

    if (alias[0] == ':') {
        AcquireLocks();
        map<qcc::StringMapKey, RemoteEndpoint*>::iterator bit = b2bEndpoints.find(msg->GetRcvEndpointName());
        if (bit != b2bEndpoints.end()) {
            /* Change affects a remote unique name (i.e. a VirtualEndpoint) */
            if (newOwner.empty()) {
                VirtualEndpoint* vep = FindVirtualEndpoint(oldOwner.c_str());
                if (vep) {
                    madeChanges = vep->CanUseRoute(*(bit->second));
                    if (vep->RemoveBusToBusEndpoint(*(bit->second))) {
                        RemoveVirtualEndpoint(*vep);
                    }
                }
            } else {
                /* Add a new virtual endpoint */
                if (bit != b2bEndpoints.end()) {
                    AddVirtualEndpoint(alias, *(bit->second), &madeChanges);
                }
            }
        } else {
            QCC_LogError(ER_BUS_NO_ENDPOINT, ("Cannot find bus-to-bus endpoint %s", msg->GetRcvEndpointName()));
        }
        ReleaseLocks();
    } else {
        /* Change affects a well-known name (name table only) */
        VirtualEndpoint* remoteController = FindVirtualEndpoint(msg->GetSender());
        if (remoteController) {
            VirtualEndpoint* newOwnerEp = newOwner.empty() ? NULL : FindVirtualEndpoint(newOwner.c_str());
            madeChanges = router.SetVirtualAlias(alias, newOwnerEp, *remoteController);
        } else {
            QCC_LogError(ER_BUS_NO_ENDPOINT, ("Cannot find virtual endpoint %s", msg->GetSender()));
        }
    }

    if (madeChanges) {
        /* Forward message to all directly connected controllers except the one that sent us this NameChanged */
        AcquireLocks();
        map<qcc::StringMapKey, RemoteEndpoint*>::const_iterator bit = b2bEndpoints.find(msg->GetRcvEndpointName());
        map<qcc::StringMapKey, RemoteEndpoint*>::iterator it = b2bEndpoints.begin();
        while (it != b2bEndpoints.end()) {
            if ((bit == b2bEndpoints.end()) || (bit->second->GetRemoteGUID() != it->second->GetRemoteGUID())) {
                QCC_DbgPrintf(("Propagating NameChanged signal to %s", it->second->GetUniqueName().c_str()));
                String key = it->first.c_str();
                RemoteEndpoint*ep = it->second;
                ep->IncrementWaiters();
                ReleaseLocks();
                QStatus status = ep->PushMessage(msg);
                if (ER_OK != status) {
                    QCC_LogError(status, ("Failed to forward NameChanged to %s", ep->GetUniqueName().c_str()));
                }
                ep->DecrementWaiters();
                AcquireLocks();
                it = b2bEndpoints.lower_bound(key);
                if ((it != b2bEndpoints.end()) && (it->first == key)) {
                    ++it;
                }
            } else {
                ++it;
            }
        }
        ReleaseLocks();
    }
}

VirtualEndpoint& AllJoynObj::AddVirtualEndpoint(const qcc::String& uniqueName, RemoteEndpoint& busToBusEndpoint, bool* wasAdded)
{
    QCC_DbgTrace(("AllJoynObj::AddVirtualEndpoint(name=%s, b2b=%s)", uniqueName.c_str(), busToBusEndpoint.GetUniqueName().c_str()));

    bool added = false;
    VirtualEndpoint* vep = NULL;

    AcquireLocks();
    map<qcc::String, VirtualEndpoint*>::iterator it = virtualEndpoints.find(uniqueName);
    if (it == virtualEndpoints.end()) {
        /* Add new virtual endpoint */
        pair<map<qcc::String, VirtualEndpoint*>::iterator, bool> ret =

            virtualEndpoints.insert(pair<qcc::String, VirtualEndpoint*>(uniqueName,
                                                                        new VirtualEndpoint(uniqueName.c_str(), busToBusEndpoint)));
        vep = ret.first->second;
        added = true;

        /* Register the endpoint with the router */
        router.RegisterEndpoint(*vep, false);
    } else {
        /* Add the busToBus endpoint to the existing virtual endpoint */
        vep = it->second;
        added = vep->AddBusToBusEndpoint(busToBusEndpoint);
    }
    ReleaseLocks();


    if (wasAdded) {
        *wasAdded = added;
    }

    return *vep;
}

void AllJoynObj::RemoveVirtualEndpoint(VirtualEndpoint& vep)
{
    QCC_DbgTrace(("RemoveVirtualEndpoint: %s", vep.GetUniqueName().c_str()));

    /* Remove virtual endpoint along with any aliases that exist for this uniqueName */
    /* Be careful to lock the name table before locking the virtual endpoints since both locks are needed
     * and doing it in the opposite order invites deadlock
     */
    AcquireLocks();
    router.RemoveVirtualAliases(vep);
    router.UnregisterEndpoint(vep);
    virtualEndpoints.erase(vep.GetUniqueName());
    ReleaseLocks();
    delete &vep;
}

VirtualEndpoint* AllJoynObj::FindVirtualEndpoint(const qcc::String& uniqueName)
{
    VirtualEndpoint* ret = NULL;
    AcquireLocks();
    map<qcc::String, VirtualEndpoint*>::iterator it = virtualEndpoints.find(uniqueName);
    if (it != virtualEndpoints.end()) {
        ret = it->second;
    }
    ReleaseLocks();
    return ret;
}

void AllJoynObj::NameOwnerChanged(const qcc::String& alias, const qcc::String* oldOwner, const qcc::String* newOwner)
{
    QStatus status;
    const String& shortGuidStr = guid.ToShortString();

    /* Validate that there is either a new owner or an old owner */
    const qcc::String* un = oldOwner ? oldOwner : newOwner;
    if (NULL == un) {
        QCC_LogError(ER_BUS_NO_ENDPOINT, ("Invalid NameOwnerChanged without oldOwner or newOwner"));
        return;
    }

    /* Validate format of unique name */
    size_t guidLen = un->find_first_of('.');
    if ((qcc::String::npos == guidLen) || (guidLen < 3)) {
        QCC_LogError(ER_FAIL, ("Invalid unique name \"%s\"", un->c_str()));
    }

    /* Ignore well-known name changes that involve any bus controller endpoint */
    if ((::strcmp(un->c_str() + guidLen, ".1") == 0) && (alias[0] != ':')) {
        return;
    }

    /* Remove unique names from sessionMap entries */
    if (!newOwner && (alias[0] == ':')) {
        AcquireLocks();
        vector<pair<String, SessionId> > changedSessionMembers;
        SessionMapType::iterator it = sessionMap.begin();
        while (it != sessionMap.end()) {
            if (it->first.first == alias) {
                /* If endpoint has gone then just delete the session map entry */
                sessionMap.erase(it++);
            } else if (it->first.second != 0) {
                /* Remove member entries from existing sessions */
                if (it->second.sessionHost == alias) {
                    if (it->second.opts.isMultipoint) {
                        changedSessionMembers.push_back(it->first);
                    }
                    it->second.sessionHost.clear();
                } else {
                    vector<String>::iterator mit = it->second.memberNames.begin();
                    while (mit != it->second.memberNames.end()) {
                        if (*mit == alias) {
                            it->second.memberNames.erase(mit);
                            if (it->second.opts.isMultipoint) {
                                changedSessionMembers.push_back(it->first);
                            }
                            break;
                        }
                        ++mit;
                    }
                }
                /*
                 * Remove empty session entry.
                 * Preserve raw sessions until GetSessionFd is called.
                 */
                /*
                 * If the session is point-to-point and the memberNames are empty.
                 * if the sessionHost is not empty (implied) and there are no member names send
                 * the  sessionLost signal as long as the session is not a raw session
                 */
                bool noMemberSingleHost = it->second.memberNames.empty();
                /*
                 * If the session is a Multipoint session it will list its own unique
                 * name in the list of memberNames. If There is only one name in the
                 * memberNames list and there is no session host it is safe to send
                 * the session lost signal as long as the session does not contain a
                 * raw session.
                 */
                bool singleMemberNoHost = ((it->second.memberNames.size() == 1) && it->second.sessionHost.empty());
                /*
                 * as long as the file descriptor is -1 this is not a raw session
                 */
                bool noRawSession = (it->second.fd == -1);
                if ((noMemberSingleHost || singleMemberNoHost) && noRawSession) {
                    SessionMapEntry tsme = it->second;
                    pair<String, SessionId> key = it->first;
                    if (!it->second.isInitializing) {
                        sessionMap.erase(it);
                    }
                    ReleaseLocks();
                    SendSessionLost(tsme);
                    AcquireLocks();
                    it = sessionMap.upper_bound(key);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
        ReleaseLocks();

        /* Send MPSessionChanged for each changed session involving alias */
        vector<pair<String, SessionId> >::const_iterator csit = changedSessionMembers.begin();
        while (csit != changedSessionMembers.end()) {
            SendMPSessionChanged(csit->second, alias.c_str(), false, csit->first.c_str());
            csit++;
        }
    }

    /* Only if local name */
    if (0 == ::strncmp(shortGuidStr.c_str(), un->c_str() + 1, shortGuidStr.size())) {

        /* Send NameChanged to all directly connected controllers */
        AcquireLocks();
        map<qcc::StringMapKey, RemoteEndpoint*>::iterator it = b2bEndpoints.begin();
        while (it != b2bEndpoints.end()) {
            Message sigMsg(bus);
            MsgArg args[3];
            args[0].Set("s", alias.c_str());
            args[1].Set("s", oldOwner ? oldOwner->c_str() : "");
            args[2].Set("s", newOwner ? newOwner->c_str() : "");

            status = sigMsg->SignalMsg("sss",
                                       org::alljoyn::Daemon::WellKnownName,
                                       0,
                                       org::alljoyn::Daemon::ObjectPath,
                                       org::alljoyn::Daemon::InterfaceName,
                                       "NameChanged",
                                       args,
                                       ArraySize(args),
                                       0,
                                       0);
            if (ER_OK == status) {
                StringMapKey key = it->first;
                RemoteEndpoint*ep = it->second;
                ep->IncrementWaiters();
                ReleaseLocks();
                status = ep->PushMessage(sigMsg);
                ep->DecrementWaiters();
                AcquireLocks();
                it = b2bEndpoints.lower_bound(key);
                if ((it != b2bEndpoints.end()) && (it->first == key)) {
                    ++it;
                }
            } else {
                ++it;
            }
            if (ER_OK != status) {
                QCC_LogError(status, ("Failed to send NameChanged"));
            }
        }

        /* If a local well-known name dropped, then remove any nameMap entry */
        if ((NULL == newOwner) && (alias[0] != ':')) {
            multimap<String, NameMapEntry>::const_iterator it = nameMap.lower_bound(alias);
            while ((it != nameMap.end()) && (it->first == alias)) {
                if (it->second.transport & TRANSPORT_LOCAL) {
                    vector<String> names;
                    names.push_back(alias);
                    FoundNames("local:", it->second.guid, TRANSPORT_LOCAL, &names, 0);
                    break;
                }
                ++it;
            }
        }
        ReleaseLocks();

        /* If a local unique name dropped, then remove any refs it had in the connnect, advertise and discover maps */
        if ((NULL == newOwner) && (alias[0] == ':')) {
            /* Remove endpoint refs from connect map */
            qcc::String last;
            AcquireLocks();
            multimap<qcc::String, qcc::String>::iterator it = connectMap.begin();
            while (it != connectMap.end()) {
                if (it->second == *oldOwner) {
                    bool isFirstSpec = (last != it->first);
                    qcc::String lastOwner;
                    do {
                        last = it->first;
                        connectMap.erase(it++);
                    } while ((connectMap.end() != it) && (last == it->first) && (*oldOwner == it->second));
                    if (isFirstSpec && ((connectMap.end() == it) || (last != it->first))) {
                        QStatus status = bus.Disconnect(last.c_str());
                        if (ER_OK != status) {
                            QCC_LogError(status, ("Failed to disconnect connect spec %s", last.c_str()));
                        }
                    }
                } else {
                    last = it->first;
                    ++it;
                }
            }

            /* Remove endpoint refs from advertise map */
            multimap<String, pair<TransportMask, String> >::const_iterator ait = advertiseMap.begin();
            while (ait != advertiseMap.end()) {
                if (ait->second.second == *oldOwner) {
                    String name = ait->first;
                    TransportMask mask = ait->second.first;
                    ++ait;
                    QStatus status = ProcCancelAdvertise(*oldOwner, name, mask);
                    if (ER_OK != status) {
                        QCC_LogError(status, ("Failed to cancel advertise for name \"%s\"", name.c_str()));
                    }
                } else {
                    ++ait;
                }
            }

            /* Remove endpoint refs from discover map */
            it = discoverMap.begin();
            while (it != discoverMap.end()) {
                if (it->second == *oldOwner) {
                    last = it++->first;
                    QCC_DbgPrintf(("Calling ProcCancelFindName from NameOwnerChanged [%s]", Thread::GetThread()->GetName()));
                    QStatus status = ProcCancelFindName(*oldOwner, last);
                    if (ER_OK != status) {
                        QCC_LogError(status, ("Failed to cancel discover for name \"%s\"", last.c_str()));
                    }
                } else {
                    ++it;
                }
            }
            ReleaseLocks();
        }
    }
}

struct FoundNameEntry {
  public:
    String name;
    String prefix;
    String dest;
    FoundNameEntry(const String& name, const String& prefix, const String& dest) : name(name), prefix(prefix), dest(dest) { }
    bool operator<(const FoundNameEntry& other) const {
        return (name < other.name) || ((name == other.name) && ((prefix < other.prefix) || ((prefix == other.prefix) && (dest < other.dest))));
    }
};

void AllJoynObj::FoundNames(const qcc::String& busAddr,
                            const qcc::String& guid,
                            TransportMask transport,
                            const vector<qcc::String>* names,
                            uint8_t ttl)
{
    QCC_DbgTrace(("AllJoynObj::FoundNames(busAddr = \"%s\", guid = \"%s\", names = %s, ttl = %d)",
                  busAddr.c_str(), guid.c_str(), StringVectorToString(names, ",").c_str(), ttl));

    if (NULL == foundNameSignal) {
        return;
    }
    set<FoundNameEntry> foundNameSet;
    set<String> lostNameSet;
    AcquireLocks();
    if (names == NULL) {
        /* If name is NULL expire all names for the given bus address. */
        if (ttl == 0) {
            multimap<String, NameMapEntry>::iterator it = nameMap.begin();
            while (it != nameMap.end()) {
                NameMapEntry& nme = it->second;
                if ((nme.guid == guid) && (nme.busAddr == busAddr)) {
                    lostNameSet.insert(it->first);
                    nameMap.erase(it++);
                } else {
                    it++;
                }
            }
        }
    } else {
        /* Generate a list of name deltas */
        vector<String>::const_iterator nit = names->begin();
        while (nit != names->end()) {
            multimap<String, NameMapEntry>::iterator it = nameMap.find(*nit);
            bool isNew = true;
            while ((it != nameMap.end()) && (*nit == it->first)) {
                if ((it->second.guid == guid) && (it->second.transport & transport)) {
                    isNew = false;
                    break;
                }
                ++it;
            }
            if (0 < ttl) {
                if (isNew) {
                    /* Add new name to map */
                    nameMap.insert(pair<String, NameMapEntry>(*nit, NameMapEntry(busAddr,
                                                                                 guid,
                                                                                 transport,
                                                                                 (ttl == numeric_limits<uint8_t>::max()) ? numeric_limits<uint32_t>::max() : (1000 * ttl))));

                    /* Send FoundAdvertisedName to anyone who is discovering *nit */
                    if (0 < discoverMap.size()) {
                        multimap<String, String>::const_iterator dit = discoverMap.begin();
                        while ((dit != discoverMap.end()) && (dit->first.compare(*nit) <= 0)) {

                            if (nit->compare(0, dit->first.size(), dit->first) == 0) {
                                /* Check whether the discoverer is allowed to use the transport over which the advertised name if found*/
                                bool forbidden = false;
                                multimap<String, std::pair<TransportMask, String> >::const_iterator forbidIt = transForbidMap.lower_bound((*nit)[0]);
                                while ((forbidIt != transForbidMap.end()) && (forbidIt->first.compare(*nit) <= 0)) {
                                    if (nit->compare(0, forbidIt->first.size(), forbidIt->first) == 0 &&
                                        forbidIt->second.second.compare(dit->second) == 0 &&
                                        (forbidIt->second.first & transport) != 0) {
                                        forbidden = true;
                                        QCC_DbgPrintf(("FoundNames: Forbid to send advertised name %s over transport %d to %s due to lack of permission", (*nit).c_str(), transport, forbidIt->second.second.c_str()));
                                        break;
                                    }
                                    ++forbidIt;
                                }
                                if (!forbidden) {
                                    foundNameSet.insert(FoundNameEntry(*nit, dit->first, dit->second));
                                }
                            }
                            ++dit;
                        }
                    }
                } else {
                    /*
                     * If the busAddr doesn't match, then this is actually a new but redundant advertisement.
                     * Don't track it. Don't updated the TTL for the existing advertisement with the same name
                     * and don't tell clients about this alternate way to connect to the name
                     * since it will look like a duplicate to the client (that doesn't receive busAddr).
                     */
                    if (busAddr == it->second.busAddr) {
                        it->second.timestamp = GetTimestamp();
                    }
                }
                nameMapReaper.Alert();
            } else {
                /* 0 == ttl means flush the record */
                if (!isNew) {
                    lostNameSet.insert(it->first);
                    nameMap.erase(it);
                }
            }
            ++nit;
        }
    }
    ReleaseLocks();

    /* Send FoundAdvertisedName signals without holding locks */
    set<FoundNameEntry>::const_iterator fit = foundNameSet.begin();
    while (fit != foundNameSet.end()) {
        QStatus status = SendFoundAdvertisedName(fit->dest, fit->name, transport, fit->prefix);
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to send FoundAdvertisedName to %s (name=%s)", fit->dest.c_str(), fit->name.c_str()));
        }
        ++fit;
    }

    /* Send LostAdvetisedName signals */
    set<String>::const_iterator lit = lostNameSet.begin();
    while (lit != lostNameSet.end()) {
        SendLostAdvertisedName(*lit++, transport);
    }
}

QStatus AllJoynObj::SendFoundAdvertisedName(const String& dest,
                                            const String& name,
                                            TransportMask transport,
                                            const String& namePrefix)
{
    QCC_DbgTrace(("AllJoynObj::SendFoundAdvertisedName(%s, %s, 0x%x, %s)", dest.c_str(), name.c_str(), transport, namePrefix.c_str()));

    MsgArg args[3];
    args[0].Set("s", name.c_str());
    args[1].Set("q", transport);
    args[2].Set("s", namePrefix.c_str());
    return Signal(dest.c_str(), 0, *foundNameSignal, args, ArraySize(args));
}

QStatus AllJoynObj::SendLostAdvertisedName(const String& name, TransportMask transport)
{
    QCC_DbgTrace(("AllJoynObj::SendLostAdvertisdName(%s, 0x%x)", name.c_str(), transport));

    QStatus status = ER_OK;

    /* Send LostAdvertisedName to anyone who is discovering name */
    AcquireLocks();
    vector<pair<String, String> > sigVec;
    if (0 < discoverMap.size()) {
        multimap<String, String>::const_iterator dit = discoverMap.lower_bound(name[0]);
        while ((dit != discoverMap.end()) && (dit->first.compare(name) <= 0)) {
            if (name.compare(0, dit->first.size(), dit->first) == 0) {
                sigVec.push_back(pair<String, String>(dit->first, dit->second));
            }
            ++dit;
        }
    }
    ReleaseLocks();

    /* Send the signals now that we aren't holding the lock */
    vector<pair<String, String> >::const_iterator it = sigVec.begin();
    while (it != sigVec.end()) {
        MsgArg args[3];
        args[0].Set("s", name.c_str());
        args[1].Set("q", transport);
        args[2].Set("s", it->first.c_str());
        QCC_DbgPrintf(("Sending LostAdvertisedName(%s, 0x%x, %s) to %s", name.c_str(), transport, it->first.c_str(), it->second.c_str()));
        QStatus tStatus = Signal(it->second.c_str(), 0, *lostAdvNameSignal, args, ArraySize(args));
        if (ER_OK != tStatus) {
            status = (ER_OK == status) ? tStatus : status;
            QCC_LogError(tStatus, ("Failed to send LostAdvertisedName to %s (name=%s)", it->second.c_str(), name.c_str()));
        }
        ++it;
    }
    return status;
}

ThreadReturn STDCALL AllJoynObj::NameMapReaperThread::Run(void* arg)
{
    uint32_t waitTime(Event::WAIT_FOREVER);
    Event evt(waitTime);
    while (!IsStopping()) {
        ajnObj->AcquireLocks();
        multimap<String, NameMapEntry>::iterator it = ajnObj->nameMap.begin();
        uint32_t now = GetTimestamp();
        waitTime = Event::WAIT_FOREVER;
        while (it != ajnObj->nameMap.end()) {
            // it->second.timestamp is an absolute time value
            // it->second.ttl is a relative time value relative to it->second.timestamp
            // now is an absolute time value for "right now" - may have rolled over relative to it->second.timestamp

            uint32_t timeSinceTimestamp = now - it->second.timestamp;     // relative time value - 2's compliment math solves rollover

            if (timeSinceTimestamp >= it->second.ttl) {
                QCC_DbgPrintf(("Expiring discovered name %s for guid %s", it->first.c_str(), it->second.guid.c_str()));
                ajnObj->SendLostAdvertisedName(it->first, it->second.transport);
                ajnObj->nameMap.erase(it++);
            } else {
                if (it->second.ttl != numeric_limits<uint32_t>::max()) {
                    // The TTL for this name map entry is less than infinte so we need to consider it

                    uint32_t nextTime = it->second.ttl - timeSinceTimestamp;     // relative time when name map entry expires
                    if (nextTime < waitTime) {
                        // This name map entry expires before the time in waitTime so update waitTime.
                        waitTime = nextTime;
                    }
                }
                ++it;
            }
        }
        ajnObj->ReleaseLocks();

        evt.ResetTime(waitTime, 0);
        QStatus status = Event::Wait(evt);
        if (status == ER_ALERTED_THREAD) {
            stopEvent.ResetEvent();
        }
    }
    return 0;
}

void AllJoynObj::BusConnectionLost(const qcc::String& busAddr)
{
    /* Clear the connection map of this busAddress */
    AcquireLocks();
    multimap<String, String>::iterator it = connectMap.lower_bound(busAddr);
    while ((it != connectMap.end()) && (0 == busAddr.compare(it->first))) {
        connectMap.erase(it++);
    }
    ReleaseLocks();
}

}
