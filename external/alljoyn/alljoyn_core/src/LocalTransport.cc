/**
 * @file
 * LocalTransport is a special type of Transport that is responsible
 * for all communication of all endpoints that terminate at registered
 * AllJoynObjects residing within this BusAttachment instance.
 */

/******************************************************************************
 * Copyright 2009-2012, Qualcomm Innovation Center, Inc.
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

#include <list>

#include <qcc/Debug.h>
#include <qcc/GUID.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Thread.h>
#include <qcc/atomic.h>

#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/Message.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/ProxyBusObject.h>

#include "LocalTransport.h"
#include "Router.h"
#include "MethodTable.h"
#include "SignalTable.h"
#include "AllJoynPeerObj.h"
#include "BusUtil.h"
#include "BusInternal.h"

#if defined(QCC_OS_ANDROID)
#include "PermissionDB.h"
#endif

#define QCC_MODULE "LOCAL_TRANSPORT"

using namespace std;
using namespace qcc;

namespace ajn {

LocalTransport::~LocalTransport()
{
    Stop();
    Join();
}

QStatus LocalTransport::Start()
{
    isStoppedEvent.ResetEvent();
    return localEndpoint.Start();
}

QStatus LocalTransport::Stop()
{
    QStatus status = localEndpoint.Stop();
    isStoppedEvent.SetEvent();
    return status;
}

QStatus LocalTransport::Join()
{
    QStatus status = localEndpoint.Join();
    /* Pend caller until transport is stopped */
    Event::Wait(isStoppedEvent);
    return status;
}

bool LocalTransport::IsRunning()
{
    return !isStoppedEvent.IsSet();
}

LocalEndpoint::LocalEndpoint(BusAttachment& bus) :
    BusEndpoint(BusEndpoint::ENDPOINT_TYPE_LOCAL),
    dispatcher(this),
    running(false),
    refCount(1),
    bus(bus),
    objectsLock(),
    replyMapLock(),
    dbusObj(NULL),
    alljoynObj(NULL),
    alljoynDebugObj(NULL),
    peerObj(NULL)
{
}

LocalEndpoint::~LocalEndpoint()
{
    QCC_DbgHLPrintf(("LocalEndpoint~LocalEndpoint"));

    running = false;

    assert(refCount > 0);
    /*
     * We can't complete the destruction if we have calls out the application.
     */
    if (DecrementAndFetch(&refCount) != 0) {
        while (refCount) {
            qcc::Sleep(1);
        }
    }
    if (dbusObj) {
        delete dbusObj;
        dbusObj = NULL;
    }
    if (alljoynObj) {
        delete alljoynObj;
        alljoynObj = NULL;
    }
    if (alljoynDebugObj) {
        delete alljoynDebugObj;
        alljoynDebugObj = NULL;
    }
    if (peerObj) {
        delete peerObj;
        peerObj = NULL;
    }
}

QStatus LocalEndpoint::Start()
{
    QStatus status = ER_OK;

    /* Start the dispatcher */
    status = dispatcher.Start();

    /* Set the local endpoint's unique name */
    SetUniqueName(bus.GetInternal().GetRouter().GenerateUniqueName());

    if (!dbusObj) {
        /* Register well known org.freedesktop.DBus remote object */
        const InterfaceDescription* intf = bus.GetInterface(org::freedesktop::DBus::InterfaceName);
        if (intf) {
            dbusObj = new ProxyBusObject(bus, org::freedesktop::DBus::WellKnownName, org::freedesktop::DBus::ObjectPath, 0);
            dbusObj->AddInterface(*intf);
        } else {
            status = ER_BUS_NO_SUCH_INTERFACE;
        }
    }

    if (!alljoynObj && (ER_OK == status)) {
        /* Register well known org.alljoyn.Bus remote object */
        const InterfaceDescription* mintf = bus.GetInterface(org::alljoyn::Bus::InterfaceName);
        if (mintf) {
            alljoynObj = new ProxyBusObject(bus, org::alljoyn::Bus::WellKnownName, org::alljoyn::Bus::ObjectPath, 0);
            alljoynObj->AddInterface(*mintf);
        } else {
            status = ER_BUS_NO_SUCH_INTERFACE;
        }
    }

    /* Initialize the peer object */
    if (!peerObj && (ER_OK == status)) {
        peerObj = new AllJoynPeerObj(bus);
        status = peerObj->Init();
    }

    /* Start the peer object */
    if (peerObj && (ER_OK == status)) {
        status = peerObj->Start();
    }

    /* Local endpoint is up and running, register with router */
    if (ER_OK == status) {
        running = true;
        bus.GetInternal().GetRouter().RegisterEndpoint(*this, true);
    }
#if defined(QCC_OS_ANDROID)
    if (!bus.GetInternal().GetRouter().IsDaemon()) {
        permVerifyThread.Start(this, NULL);
    }
#endif
    return status;
}

QStatus LocalEndpoint::Stop(void)
{
    QCC_DbgTrace(("LocalEndpoint::Stop"));

    if (running) {
        bus.GetInternal().GetRouter().UnregisterEndpoint(*this);
    }
    /* Local endpoint not longer running */
    running = false;

    IncrementAndFetch(&refCount);

    dispatcher.Stop();

    /*
     * Unregister all registered bus objects
     */
    objectsLock.Lock(MUTEX_CONTEXT);
    unordered_map<const char*, BusObject*, Hash, PathEq>::iterator it = localObjects.begin();
    while (it != localObjects.end()) {
        BusObject* obj = it->second;
        objectsLock.Unlock(MUTEX_CONTEXT);
        UnregisterBusObject(*obj);
        objectsLock.Lock(MUTEX_CONTEXT);
        it = localObjects.begin();
    }
    if (peerObj) {
        peerObj->Stop();
    }
    objectsLock.Unlock(MUTEX_CONTEXT);
    DecrementAndFetch(&refCount);
#if defined(QCC_OS_ANDROID)
    permVerifyThread.Stop();
#endif
    return ER_OK;
}

QStatus LocalEndpoint::Join(void)
{
    dispatcher.Join();

    if (peerObj) {
        peerObj->Join();
    }
#if defined(QCC_OS_ANDROID)
    permVerifyThread.Join();
#endif
    return ER_OK;
}

QStatus LocalEndpoint::Diagnose(Message& message)
{
    QStatus status;
    BusObject* obj = FindLocalObject(message->GetObjectPath());

    /*
     * Try to figure out what went wrong
     */
    if (obj == NULL) {
        status = ER_BUS_NO_SUCH_OBJECT;
        QCC_LogError(status, ("No such object %s", message->GetObjectPath()));
    } else if (!obj->ImplementsInterface(message->GetInterface())) {
        status = ER_BUS_OBJECT_NO_SUCH_INTERFACE;
        QCC_LogError(status, ("Object %s has no interface %s (member=%s)", message->GetObjectPath(), message->GetInterface(), message->GetMemberName()));
    } else {
        status = ER_BUS_OBJECT_NO_SUCH_MEMBER;
        QCC_LogError(status, ("Object %s has no member %s", message->GetObjectPath(), message->GetMemberName()));
    }
    return status;
}

QStatus LocalEndpoint::PeerInterface(Message& message)
{
    if (strcmp(message->GetMemberName(), "Ping") == 0) {
        QStatus status = message->UnmarshalArgs("", "");
        if (ER_OK != status) {
            return status;
        }
        message->ReplyMsg(message, NULL, 0);
        return bus.GetInternal().GetRouter().PushMessage(message, *this);
    }
    if (strcmp(message->GetMemberName(), "GetMachineId") == 0) {
        QStatus status = message->UnmarshalArgs("", "s");
        if (ER_OK != status) {
            return status;
        }
        MsgArg replyArg(ALLJOYN_STRING);
        // @@TODO Need OS specific support for returning a machine id GUID use the bus id for now
        qcc::String guidStr = bus.GetInternal().GetGlobalGUID().ToString();
        replyArg.v_string.str = guidStr.c_str();
        replyArg.v_string.len = guidStr.size();
        message->ReplyMsg(message, &replyArg, 1);
        return bus.GetInternal().GetRouter().PushMessage(message, *this);
    }
    return ER_BUS_OBJECT_NO_SUCH_MEMBER;
}

LocalEndpoint::Dispatcher::Dispatcher(LocalEndpoint* endpoint) :
    Timer("lepDisp", true, 4, true),
    AlarmListener(),
    endpoint(endpoint)
{
}

QStatus LocalEndpoint::Dispatcher::DispatchMessage(Message& msg)
{
    return AddAlarm(Alarm(0, this, 0, new Message(msg)));
}

QStatus LocalEndpoint::PushMessage(Message& message)
{
    /* Determine if the source of this message is local to the process */
    bool isLocalSender = bus.GetInternal().GetRouter().FindEndpoint(message->GetSender()) == this;

    if (isLocalSender) {
        return DoPushMessage(message);
    } else {
        return dispatcher.DispatchMessage(message);
    }
}

void LocalEndpoint::Dispatcher::AlarmTriggered(const Alarm& alarm, QStatus reason)
{
    Message* msg = static_cast<Message*>(alarm.GetContext());
    if (msg) {
        if (reason == ER_OK) {
            QStatus status = endpoint->DoPushMessage(*msg);
            if (status != ER_OK) {
                QCC_LogError(status, ("LocalEndpoint::DoPushMessage failed"));
            }
        }
        delete msg;
    }
}

QStatus LocalEndpoint::DoPushMessage(Message& message)
{
    QStatus status = ER_OK;

    if (!running) {
        status = ER_BUS_STOPPING;
        QCC_DbgHLPrintf(("Local transport not running discarding %s", message->Description().c_str()));
    } else {
        if (IncrementAndFetch(&refCount) > 1) {
            Thread* thread = Thread::GetThread();

            QCC_DbgPrintf(("Pushing %s into local endpoint", message->Description().c_str()));

            switch (message->GetType()) {
            case MESSAGE_METHOD_CALL:
                status = HandleMethodCall(message);
                break;

            case MESSAGE_SIGNAL:
                status = HandleSignal(message);
                break;

            case MESSAGE_METHOD_RET:
            case MESSAGE_ERROR:
                status = HandleMethodReply(message);
                break;

            default:
                status = ER_FAIL;
                break;
            }
        }
        DecrementAndFetch(&refCount);
    }
    return status;
}

QStatus LocalEndpoint::RegisterBusObject(BusObject& object)
{
    QStatus status = ER_OK;

    const char* objPath = object.GetPath();

    QCC_DbgPrintf(("RegisterObject %s", objPath));

    if (!IsLegalObjectPath(objPath)) {
        status = ER_BUS_BAD_OBJ_PATH;
        QCC_LogError(status, ("Illegal object path \"%s\" specified", objPath));
        return status;
    }

    objectsLock.Lock(MUTEX_CONTEXT);

    /* Register placeholder parents as needed */
    size_t off = 0;
    qcc::String pathStr(objPath);
    BusObject* lastParent = NULL;
    if (1 < pathStr.size()) {
        while (qcc::String::npos != (off = pathStr.find_first_of('/', off))) {
            qcc::String parentPath = pathStr.substr(0, max((size_t)1, off));
            off++;
            BusObject* parent = FindLocalObject(parentPath.c_str());
            if (!parent) {
                parent = new BusObject(bus, parentPath.c_str(), true);
                QStatus status = DoRegisterBusObject(*parent, lastParent, true);
                if (ER_OK != status) {
                    delete parent;
                    QCC_LogError(status, ("Failed to register default object for path %s", parentPath.c_str()));
                    break;
                }
                defaultObjects.push_back(parent);
            }
            lastParent = parent;
        }
    }

    /* Now register the object itself */
    if (ER_OK == status) {
        status = DoRegisterBusObject(object, lastParent, false);
    }

    objectsLock.Unlock(MUTEX_CONTEXT);

    return status;
}

QStatus LocalEndpoint::DoRegisterBusObject(BusObject& object, BusObject* parent, bool isPlaceholder)
{
    QCC_DbgPrintf(("RegisterBusObject %s", object.GetPath()));
    const char* objPath = object.GetPath();

    /* objectsLock is already obtained */

    /* If an object with this path already exists, replace it */
    BusObject* existingObj = FindLocalObject(objPath);
    if (NULL != existingObj) {
        existingObj->Replace(object);
        UnregisterBusObject(*existingObj);
    }

    /* Register object. */
    QStatus status = object.DoRegistration();
    if (ER_OK == status) {
        /* Link new object to its parent */
        if (parent) {
            parent->AddChild(object);
        }
        /* Add object to list of objects */
        localObjects[object.GetPath()] = &object;

        /* Register handler for the object's methods */
        methodTable.AddAll(&object);

        /* Notify object of registration. Defer if we are not connected yet. */
        if (bus.GetInternal().GetRouter().IsBusRunning()) {
            BusIsConnected();
        }
    }

    return status;
}

void LocalEndpoint::UnregisterBusObject(BusObject& object)
{
    QCC_DbgPrintf(("UnregisterBusObject %s", object.GetPath()));

    /* Remove members */
    methodTable.RemoveAll(&object);

    /* Remove from object list */
    objectsLock.Lock(MUTEX_CONTEXT);
    localObjects.erase(object.GetPath());
    objectsLock.Unlock(MUTEX_CONTEXT);

    /* Notify object and detach from bus*/
    object.ObjectUnregistered();

    /* Detach object from parent */
    objectsLock.Lock(MUTEX_CONTEXT);
    if (NULL != object.parent) {
        object.parent->RemoveChild(object);
    }

    /* If object has children, unregister them as well */
    while (true) {
        BusObject* child = object.RemoveChild();
        if (!child) {
            break;
        }
        UnregisterBusObject(*child);
    }
    /* Delete the object if it was a default object */
    vector<BusObject*>::iterator dit = defaultObjects.begin();
    while (dit != defaultObjects.end()) {
        if (*dit == &object) {
            defaultObjects.erase(dit);
            delete &object;
            break;
        } else {
            ++dit;
        }
    }
    objectsLock.Unlock(MUTEX_CONTEXT);
}

BusObject* LocalEndpoint::FindLocalObject(const char* objectPath) {
    objectsLock.Lock(MUTEX_CONTEXT);
    unordered_map<const char*, BusObject*, Hash, PathEq>::iterator iter = localObjects.find(objectPath);
    BusObject* ret = (iter == localObjects.end()) ? NULL : iter->second;
    objectsLock.Unlock(MUTEX_CONTEXT);
    return ret;
}

QStatus LocalEndpoint::RegisterReplyHandler(MessageReceiver* receiver,
                                            MessageReceiver::ReplyHandler replyHandler,
                                            const InterfaceDescription::Member& method,
                                            uint32_t serial,
                                            bool secure,
                                            void* context,
                                            uint32_t timeout)
{
    QStatus status = ER_OK;
    if (!running) {
        status = ER_BUS_STOPPING;
        QCC_LogError(status, ("Local transport not running"));
    } else {
        ReplyContext reply = {
            receiver,
            replyHandler,
            &method,
            secure,
            context,
            Alarm(timeout, this, 0, (void*)(size_t)serial)
        };
        QCC_DbgPrintf(("LocalEndpoint::RegisterReplyHandler - Adding serial=%u", serial));
        replyMapLock.Lock(MUTEX_CONTEXT);
        replyMap.insert(pair<uint32_t, ReplyContext>(serial, reply));
        replyMapLock.Unlock(MUTEX_CONTEXT);

        /* Set a timeout */
        status = bus.GetInternal().GetTimer().AddAlarm(reply.alarm);
        if (status != ER_OK) {
            UnregisterReplyHandler(serial);
        }
    }
    return status;
}

bool LocalEndpoint::UnregisterReplyHandler(uint32_t serial)
{
    replyMapLock.Lock(MUTEX_CONTEXT);
    map<uint32_t, ReplyContext>::iterator iter = replyMap.find(serial);
    if (iter != replyMap.end()) {
        QCC_DbgPrintf(("LocalEndpoint::UnregisterReplyHandler - Removing serial=%u", serial));
        ReplyContext rc = iter->second;
        replyMap.erase(iter);
        replyMapLock.Unlock(MUTEX_CONTEXT);
        bus.GetInternal().GetTimer().RemoveAlarm(rc.alarm);
        return true;
    } else {
        replyMapLock.Unlock(MUTEX_CONTEXT);
        return false;
    }
}

QStatus LocalEndpoint::ExtendReplyHandlerTimeout(uint32_t serial, uint32_t extension)
{
    QStatus status;
    replyMapLock.Lock();
    map<uint32_t, ReplyContext>::iterator iter = replyMap.find(serial);
    if (iter != replyMap.end()) {
        QCC_DbgPrintf(("LocalEndpoint::ExtendReplyHandlerTimeout - extending timeout for serial=%u", serial));
        Alarm newAlarm(Timespec(iter->second.alarm.GetAlarmTime() + extension), this, 0, (void*)(size_t)serial);
        status = bus.GetInternal().GetTimer().ReplaceAlarm(iter->second.alarm, newAlarm, false);
        if (status == ER_OK) {
            iter->second.alarm = newAlarm;
        }
    } else {
        status = ER_BUS_UNKNOWN_SERIAL;
    }
    replyMapLock.Unlock();
    return status;
}

QStatus LocalEndpoint::RegisterSignalHandler(MessageReceiver* receiver,
                                             MessageReceiver::SignalHandler signalHandler,
                                             const InterfaceDescription::Member* member,
                                             const char* srcPath)
{
    if (!receiver) {
        return ER_BAD_ARG_1;
    }
    if (!signalHandler) {
        return ER_BAD_ARG_2;
    }
    if (!member) {
        return ER_BAD_ARG_3;
    }
    signalTable.Add(receiver, signalHandler, member, srcPath ? srcPath : "");
    return ER_OK;
}

QStatus LocalEndpoint::UnregisterSignalHandler(MessageReceiver* receiver,
                                               MessageReceiver::SignalHandler signalHandler,
                                               const InterfaceDescription::Member* member,
                                               const char* srcPath)
{
    if (!receiver) {
        return ER_BAD_ARG_1;
    }
    if (!signalHandler) {
        return ER_BAD_ARG_2;
    }
    if (!member) {
        return ER_BAD_ARG_3;
    }
    signalTable.Remove(receiver, signalHandler, member, srcPath ? srcPath : "");
    return ER_OK;
}

QStatus LocalEndpoint::UnregisterAllHandlers(MessageReceiver* receiver)
{
    /*
     * Remove all the signal handlers for this receiver.
     */
    signalTable.RemoveAll(receiver);
    /*
     * Remove any reply handlers for this receiver
     */
    replyMapLock.Lock(MUTEX_CONTEXT);
    bool removed;
    do {
        removed = false;
        for (map<uint32_t, ReplyContext>::iterator iter = replyMap.begin(); iter != replyMap.end(); ++iter) {
            if (iter->second.object == receiver) {
                bus.GetInternal().GetTimer().RemoveAlarm(iter->second.alarm);
                replyMap.erase(iter);
                removed = true;
                break;
            }
        }
    } while (removed);
    replyMapLock.Unlock(MUTEX_CONTEXT);
    return ER_OK;
}

void LocalEndpoint::AlarmTriggered(const Alarm& alarm, QStatus reason)
{
    /*
     * Alarms are used for two unrelated purposes within LocalEnpoint:
     *
     * When context is non-NULL, the alarm indicates that a method call
     * with serial == *context has timed out.
     *
     * When context is NULL, the alarm indicates that the BusAttachment that this
     * LocalEndpoint is a part of is connected to a daemon and any previously
     * unregistered BusObjects should be registered
     */
    if (NULL != alarm.GetContext()) {
        uint32_t serial = reinterpret_cast<uintptr_t>(alarm.GetContext());
        Message msg(bus);
        QCC_DbgPrintf(("Timed out waiting for METHOD_REPLY with serial %d", serial));

        if (reason == ER_TIMER_EXITING) {
            msg->ErrorMsg("org.alljoyn.Bus.Exiting", serial);
        } else {
            msg->ErrorMsg("org.alljoyn.Bus.Timeout", serial);
        }
        HandleMethodReply(msg);
    } else {
        /* Call ObjectRegistered for any unregistered bus object */
        objectsLock.Lock(MUTEX_CONTEXT);
        unordered_map<const char*, BusObject*, Hash, PathEq>::iterator iter = localObjects.begin();
        while (iter != localObjects.end()) {
            if (!iter->second->isRegistered) {
                BusObject* bo = iter->second;
                bo->isRegistered = true;
                bo->InUseIncrement();
                objectsLock.Unlock(MUTEX_CONTEXT);
                bo->ObjectRegistered();
                objectsLock.Lock(MUTEX_CONTEXT);
                bo->InUseDecrement();
                iter = localObjects.begin();
            } else {
                ++iter;
            }
        }
        objectsLock.Unlock(MUTEX_CONTEXT);

        /* Decrement refcount to indicate we are done calling out */
        DecrementAndFetch(&refCount);
    }
}

QStatus LocalEndpoint::HandleMethodCall(Message& message)
{
    QStatus status = ER_OK;

    /* Look up the member */
    MethodTable::SafeEntry* safeEntry = methodTable.Find(message->GetObjectPath(),
                                                         message->GetInterface(),
                                                         message->GetMemberName());
    const MethodTable::Entry* entry = safeEntry ? safeEntry->entry : NULL;

    if (entry == NULL) {
        if (strcmp(message->GetInterface(), org::freedesktop::DBus::Peer::InterfaceName) == 0) {
            /*
             * Special case the Peer interface
             */
            status = PeerInterface(message);
        } else {
            /*
             * Figure out what error to report
             */
            status = Diagnose(message);
        }
    } else if (entry->member->iface->IsSecure() && !message->IsEncrypted()) {
        status = ER_BUS_MESSAGE_NOT_ENCRYPTED;
        QCC_LogError(status, ("Method call to secure interface was not encrypted"));
    } else {
        status = message->UnmarshalArgs(entry->member->signature, entry->member->returnSignature.c_str());
    }
    if (status == ER_OK) {
        /* Call the method handler */
        if (entry) {
            if (bus.GetInternal().GetRouter().IsDaemon() || entry->member->accessPerms.size() == 0) {
                entry->object->CallMethodHandler(entry->handler, entry->member, message, entry->context);
            } else {
#if defined(QCC_OS_ANDROID)
                QCC_DbgPrintf(("Method(%s::%s) requires permission %s", message->GetInterface(), message->GetMemberName(), entry->member->accessPerms.c_str()));
                chkMsgListLock.Lock(MUTEX_CONTEXT);
                PermCheckedEntry permChkEntry(message->GetSender(), message->GetObjectPath(), message->GetInterface(), message->GetMemberName());
                std::map<PermCheckedEntry, bool>::const_iterator it = permCheckedCallMap.find(permChkEntry);
                if (it != permCheckedCallMap.end()) {
                    if (permCheckedCallMap[permChkEntry]) {
                        entry->object->CallMethodHandler(entry->handler, entry->member, message, entry->context);
                    } else {
                        QCC_LogError(ER_ALLJOYN_ACCESS_PERMISSION_ERROR, ("Endpoint(%s) has no permission to call method (%s::%s)",
                                                                          message->GetSender(), message->GetInterface(), message->GetMemberName()));
                        if (!(message->GetFlags() & ALLJOYN_FLAG_NO_REPLY_EXPECTED)) {
                            qcc::String errStr;
                            qcc::String errMsg;
                            errStr += "org.alljoyn.Bus.";
                            errStr += QCC_StatusText(ER_ALLJOYN_ACCESS_PERMISSION_ERROR);
                            errMsg = message->Description();
                            message->ErrorMsg(message, errStr.c_str(), errMsg.c_str());
                            bus.GetInternal().GetRouter().PushMessage(message, *this);
                        }
                    }
                } else {
                    ChkPendingMsg msgInfo(message, entry, entry->member->accessPerms);
                    chkPendingMsgList.push_back(msgInfo);
                    wakeEvent.SetEvent();
                }
                chkMsgListLock.Unlock(MUTEX_CONTEXT);
#else
                QCC_LogError(ER_FAIL, ("Peer permission verification is not Supported!"));
#endif
            }
        }
    } else if (message->GetType() == MESSAGE_METHOD_CALL && !(message->GetFlags() & ALLJOYN_FLAG_NO_REPLY_EXPECTED)) {
        /* We are rejecting a method call that expects a response so reply with an error message. */
        qcc::String errStr;
        qcc::String errMsg;
        switch (status) {
        case ER_BUS_MESSAGE_NOT_ENCRYPTED:
            errStr = "org.alljoyn.Bus.SecurityViolation";
            errMsg = "Expected secure method call";
            peerObj->HandleSecurityViolation(message, status);
            status = ER_OK;
            break;

        case ER_BUS_MESSAGE_DECRYPTION_FAILED:
            errStr = "org.alljoyn.Bus.SecurityViolation";
            errMsg = "Unable to authenticate method call";
            peerObj->HandleSecurityViolation(message, status);
            status = ER_OK;
            break;

        case ER_BUS_NOT_AUTHORIZED:
            errStr = "org.alljoyn.Bus.SecurityViolation";
            errMsg = "Method call not authorized";
            peerObj->HandleSecurityViolation(message, status);
            status = ER_OK;
            break;

        case ER_BUS_NO_SUCH_OBJECT:
            errStr = "org.freedesktop.DBus.Error.ServiceUnknown";
            errMsg = QCC_StatusText(status);
            break;

        default:
            errStr += "org.alljoyn.Bus.";
            errStr += QCC_StatusText(status);
            errMsg = message->Description();
            break;
        }
        message->ErrorMsg(message, errStr.c_str(), errMsg.c_str());
        status = bus.GetInternal().GetRouter().PushMessage(message, *this);
    } else {
        QCC_LogError(status, ("Ignoring message %s", message->Description().c_str()));
        status = ER_OK;
    }

    // safeEntry might be null here
    delete safeEntry;
    return status;
}

QStatus LocalEndpoint::HandleSignal(Message& message)
{
    QStatus status = ER_OK;

    signalTable.Lock();

    /* Look up the signal */
    pair<SignalTable::const_iterator, SignalTable::const_iterator> range =
        signalTable.Find(message->GetObjectPath(), message->GetInterface(), message->GetMemberName());

    /*
     * Quick exit if there are no handlers for this signal
     */
    if (range.first == range.second) {
        signalTable.Unlock();
        return ER_OK;
    }
    /*
     * Build a list of all signal handlers for this signal
     */
    list<SignalTable::Entry> callList;
    const InterfaceDescription::Member* signal = range.first->second.member;
    do {
        callList.push_back(range.first->second);
    } while (++range.first != range.second);
    /*
     * We have our callback list so we can unlock the signal table.
     */
    signalTable.Unlock();
    /*
     * Validate and unmarshal the signal
     */
    if (signal->iface->IsSecure() && !message->IsEncrypted()) {
        status = ER_BUS_MESSAGE_NOT_ENCRYPTED;
        QCC_LogError(status, ("Signal from secure interface was not encrypted"));
    } else {
        status = message->UnmarshalArgs(signal->signature);
    }
    if (status != ER_OK) {
        if ((status == ER_BUS_MESSAGE_DECRYPTION_FAILED) || (status == ER_BUS_MESSAGE_NOT_ENCRYPTED) || (status == ER_BUS_NOT_AUTHORIZED)) {
            peerObj->HandleSecurityViolation(message, status);
            status = ER_OK;
        }
    } else {
        list<SignalTable::Entry>::const_iterator first = callList.begin();
        if (bus.GetInternal().GetRouter().IsDaemon() || first->member->accessPerms.size() == 0) {
            list<SignalTable::Entry>::const_iterator callit;
            for (callit = callList.begin(); callit != callList.end(); ++callit) {
                /* Don't multithread signals originating locally.  See comment in similar code in MethodCallHandler */
                (callit->object->*callit->handler)(callit->member, message->GetObjectPath(), message);
            }
        } else {
#if defined(QCC_OS_ANDROID)
            QCC_DbgPrintf(("Signal(%s::%s) requires permission %s", message->GetInterface(), message->GetMemberName(), first->member->accessPerms.c_str()));
            PermCheckedEntry permChkEntry(message->GetSender(), message->GetObjectPath(), message->GetInterface(), message->GetMemberName());
            chkMsgListLock.Lock(MUTEX_CONTEXT);
            std::map<PermCheckedEntry, bool>::const_iterator it = permCheckedCallMap.find(permChkEntry);
            if (it == permCheckedCallMap.end()) {
                ChkPendingMsg msgInfo(message, callList, first->member->accessPerms);
                chkPendingMsgList.push_back(msgInfo);
                wakeEvent.SetEvent();
            } else {
                if (permCheckedCallMap[permChkEntry]) {
                    list<SignalTable::Entry>::const_iterator callit;
                    for (callit = callList.begin(); callit != callList.end(); ++callit) {
                        (callit->object->*callit->handler)(callit->member, message->GetObjectPath(), message);
                    }
                } else {
                    /* Do not return Error message because signal does not require reply */
                    QCC_LogError(ER_ALLJOYN_ACCESS_PERMISSION_ERROR, ("Endpoint(%s) has no permission to issue signal (%s::%s)",
                                                                      message->GetSender(), message->GetInterface(), message->GetMemberName()));
                }
            }
            chkMsgListLock.Unlock(MUTEX_CONTEXT);
#else
            QCC_LogError(ER_FAIL, ("Peer permission verification is not Supported!"));
#endif
        }
    }
    return status;
}

QStatus LocalEndpoint::HandleMethodReply(Message& message)
{
    QStatus status = ER_OK;

    replyMapLock.Lock(MUTEX_CONTEXT);
    map<uint32_t, ReplyContext>::iterator iter = replyMap.find(message->GetReplySerial());
    if (iter != replyMap.end()) {
        ReplyContext rc = iter->second;
        replyMap.erase(iter);
        replyMapLock.Unlock(MUTEX_CONTEXT);
        bus.GetInternal().GetTimer().RemoveAlarm(rc.alarm);
        if (rc.secure && !message->IsEncrypted()) {
            /*
             * If the response was an internally generated error response just keep that error.
             * Otherwise if reply was not encrypted so return an error to the caller. Internally
             * generated messages can be identified by their sender field.
             */
            if ((message->GetType() == MESSAGE_METHOD_RET) || (bus.GetInternal().GetLocalEndpoint().GetUniqueName() != message->GetSender())) {
                status = ER_BUS_MESSAGE_NOT_ENCRYPTED;
            }
        } else {
            QCC_DbgPrintf(("Matched reply for serial #%d", message->GetReplySerial()));
            if (message->GetType() == MESSAGE_METHOD_RET) {
                status = message->UnmarshalArgs(rc.method->returnSignature);
            } else {
                status = message->UnmarshalArgs("*");
            }
        }
        if (status != ER_OK) {
            switch (status) {
            case ER_BUS_MESSAGE_DECRYPTION_FAILED:
            case ER_BUS_MESSAGE_NOT_ENCRYPTED:
            case ER_BUS_NOT_AUTHORIZED:
                message->ErrorMsg(status, message->GetReplySerial());
                peerObj->HandleSecurityViolation(message, status);
                break;

            default:
                message->ErrorMsg(status, message->GetReplySerial());
                break;
            }
            QCC_LogError(status, ("Reply message replaced with an internally generated error"));
            status = ER_OK;
        }
        ((rc.object)->*(rc.handler))(message, rc.context);
    } else {
        replyMapLock.Unlock(MUTEX_CONTEXT);
        status = ER_BUS_UNMATCHED_REPLY_SERIAL;
        QCC_DbgHLPrintf(("%s does not match any current method calls: %s", message->Description().c_str(), QCC_StatusText(status)));
    }
    return status;
}

void LocalEndpoint::BusIsConnected()
{
    if (!bus.GetInternal().GetTimer().HasAlarm(Alarm(0, this))) {
        if (IncrementAndFetch(&refCount) > 1) {
            /* Call ObjectRegistered callbacks on another thread */
            QStatus status = bus.GetInternal().GetTimer().AddAlarm(Alarm(0, this));
            if (status != ER_OK) {
                DecrementAndFetch(&refCount);
            }
        } else {
            DecrementAndFetch(&refCount);
        }
    }
}

#if defined(QCC_OS_ANDROID)
void*  LocalEndpoint::PermVerifyThread::Run(void* arg)
{
    QStatus status = ER_OK;
    LocalEndpoint* localEp = reinterpret_cast<LocalEndpoint*>(arg);
    vector<Event*> checkEvents, signaledEvents;
    checkEvents.push_back(&stopEvent);
    checkEvents.push_back(&(localEp->wakeEvent));
    const uint32_t MAX_PERM_CHECKEDCALL_SIZE = 500;
    while (!IsStopping()) {
        signaledEvents.clear();
        status = Event::Wait(checkEvents, signaledEvents);
        if (ER_OK != status) {
            QCC_LogError(status, ("Event::Wait failed"));
            break;
        }

        for (vector<Event*>::iterator i = signaledEvents.begin(); i != signaledEvents.end(); ++i) {
            (*i)->ResetEvent();
            if (*i == &stopEvent) {
                continue;
            }

            while (!localEp->chkPendingMsgList.empty()) {
                localEp->chkMsgListLock.Lock(MUTEX_CONTEXT);
                ChkPendingMsg& msgInfo = localEp->chkPendingMsgList.front();
                Message& message = msgInfo.msg;
                qcc::String& permsStr = msgInfo.perms;

                /* Split permissions that are concated by ";". The permission string is in form of "PERM0;PERM1;..." */
                std::set<qcc::String> permsReq;
                size_t pos;
                while ((pos = permsStr.find_first_of(";")) != String::npos) {
                    qcc::String tmp = permsStr.substr(0, pos);
                    permsReq.insert(tmp);
                    permsStr.erase(0, pos + 1);
                }
                if (permsStr.size() > 0) {
                    permsReq.insert(permsStr);
                }

                bool allowed = true;
                uint32_t userId = -1;
                /* Ask daemon about the user id of the sender */
                MsgArg arg("s", message->GetSender());
                Message reply(localEp->GetBus());
                status = localEp->GetDBusProxyObj().MethodCall(org::freedesktop::DBus::InterfaceName,
                                                               "GetConnectionUnixUser",
                                                               &arg,
                                                               1,
                                                               reply);

                if (status == ER_OK) {
                    userId = reply->GetArg(0)->v_uint32;
                }
                /* The permission check is only required for UnixEndpoint */
                if (userId != (uint32_t)-1) {
                    allowed = PermissionDB::GetDB().VerifyPeerPermissions(userId, permsReq);
                }

                QCC_DbgPrintf(("VerifyPeerPermissions result: allowed = %d", allowed));
                localEp->chkMsgListLock.Lock(MUTEX_CONTEXT);
                /* Be defensive. Limit the map cache size to be no more than MAX_PERM_CHECKEDCALL_SIZE */
                if (localEp->permCheckedCallMap.size() > MAX_PERM_CHECKEDCALL_SIZE) {
                    localEp->permCheckedCallMap.clear();
                }
                PermCheckedEntry permChkEntry(message->GetSender(), message->GetObjectPath(), message->GetInterface(), message->GetMemberName());
                localEp->permCheckedCallMap[permChkEntry] = allowed; /* Cache the result */
                localEp->chkMsgListLock.Unlock(MUTEX_CONTEXT);

                /* Handle the message based on the message type. */
                AllJoynMessageType msgType = message->GetType();
                if (msgType == MESSAGE_METHOD_CALL) {
                    if (allowed) {
                        const MethodTable::Entry* entry = msgInfo.methodEntry;
                        entry->object->CallMethodHandler(entry->handler, entry->member, msgInfo.msg, entry->context);
                    } else {
                        QCC_LogError(ER_ALLJOYN_ACCESS_PERMISSION_ERROR, ("Endpoint(%s) has no permission to call method (%s::%s)",
                                                                          message->GetSender(), message->GetInterface(), message->GetMemberName()));
                        if (!(message->GetFlags() & ALLJOYN_FLAG_NO_REPLY_EXPECTED)) {
                            status = ER_ALLJOYN_ACCESS_PERMISSION_ERROR;
                            qcc::String errStr;
                            qcc::String errMsg;
                            errStr += "org.alljoyn.Bus.";
                            errStr += QCC_StatusText(status);
                            errMsg = message->Description();
                            message->ErrorMsg(message, errStr.c_str(), errMsg.c_str());
                            localEp->bus.GetInternal().GetRouter().PushMessage(message, *localEp);
                        }
                    }
                } else if (msgType == MESSAGE_SIGNAL) {
                    if (allowed) {
                        list<SignalTable::Entry>& callList = msgInfo.signalCallList;
                        list<SignalTable::Entry>::const_iterator callit;
                        for (callit = callList.begin(); callit != callList.end(); ++callit) {
                            (callit->object->*callit->handler)(callit->member, (msgInfo.msg)->GetObjectPath(), message);
                        }
                    } else {
                        QCC_LogError(ER_ALLJOYN_ACCESS_PERMISSION_ERROR, ("Endpoint(%s) has no permission to issue signal (%s::%s)",
                                                                          message->GetSender(), message->GetInterface(), message->GetMemberName()));
                    }
                } else {
                    QCC_LogError(status, ("PermVerifyThread::Wrong Message Type %d", msgType));
                }
                localEp->chkPendingMsgList.pop_front();
                localEp->chkMsgListLock.Unlock(MUTEX_CONTEXT);
            }
        }
    }
    return (void*)status;
}
#endif

const ProxyBusObject& LocalEndpoint::GetAllJoynDebugObj() {
    if (!alljoynDebugObj) {
        /* Register well known org.alljoyn.Bus.Debug remote object */
        alljoynDebugObj = new ProxyBusObject(bus, org::alljoyn::Daemon::WellKnownName, org::alljoyn::Daemon::Debug::ObjectPath, 0);
        const InterfaceDescription* intf;
        intf = bus.GetInterface(org::alljoyn::Daemon::Debug::InterfaceName);
        if (intf) {
            alljoynDebugObj->AddInterface(*intf);
        }
        intf = bus.GetInterface(org::freedesktop::DBus::Properties::InterfaceName);
        if (intf) {
            alljoynDebugObj->AddInterface(*intf);
        }
    }

    return *alljoynDebugObj;
}


}
