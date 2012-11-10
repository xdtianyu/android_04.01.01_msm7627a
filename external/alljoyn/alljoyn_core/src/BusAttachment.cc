/**
 * @file
 * BusAttachment is the top-level object responsible for connecting to and optionally managing a message bus.
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
#include <qcc/Debug.h>
#include <qcc/Util.h>
#include <qcc/Event.h>
#include <qcc/String.h>
#include <qcc/Timer.h>
#include <qcc/atomic.h>
#include <qcc/XmlElement.h>
#include <qcc/StringSource.h>
#include <qcc/FileStream.h>

#include <assert.h>
#include <algorithm>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusListener.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include "AuthMechanism.h"
#include "AuthMechAnonymous.h"
#include "AuthMechDBusCookieSHA1.h"
#include "AuthMechExternal.h"
#include "AuthMechSRP.h"
#include "AuthMechRSA.h"
#include "AuthMechLogon.h"
#include "SessionInternal.h"
#include "Transport.h"
#include "TransportList.h"
#include "BusUtil.h"
#include "BusEndpoint.h"
#include "LocalTransport.h"
#include "PeerState.h"
#include "KeyStore.h"
#include "BusInternal.h"
#include "AllJoynPeerObj.h"
#include "XmlHelper.h"
#include "ClientTransport.h"
#include "NullTransport.h"

#define QCC_MODULE "ALLJOYN"


using namespace std;
using namespace qcc;

// declare these in the anonymous namespace so that the symbols will not be
// visible outside this translation unit
namespace {
using namespace ajn;

struct JoinSessionAsyncCBContext {
    BusAttachment::JoinSessionAsyncCB* callback;
    SessionListener* sessionListener;
    void* context;

    JoinSessionAsyncCBContext(BusAttachment::JoinSessionAsyncCB* callback, SessionListener* sessionListener, void* context) :
        callback(callback),
        sessionListener(sessionListener),
        context(context)
    { }
};

struct SetLinkTimeoutAsyncCBContext {
    BusAttachment::SetLinkTimeoutAsyncCB* callback;
    void* context;

    SetLinkTimeoutAsyncCBContext(BusAttachment::SetLinkTimeoutAsyncCB* callback, void* context) :
        callback(callback),
        context(context)
    { }
};

}

namespace ajn {

BusAttachment::Internal::Internal(const char* appName,
                                  BusAttachment& bus,
                                  TransportFactoryContainer& factories,
                                  Router* router,
                                  bool allowRemoteMessages,
                                  const char* listenAddresses) :
    application(appName ? appName : "unknown"),
    bus(bus),
    listenersLock(),
    listeners(),
    transportList(bus, factories),
    keyStore(application),
    authManager(keyStore),
    globalGuid(qcc::GUID128()),
    msgSerial(1),
    router(router ? router : new ClientRouter),
    localEndpoint(transportList.GetLocalTransport()->GetLocalEndpoint()),
    timer("BusTimer", true),
    allowRemoteMessages(allowRemoteMessages),
    listenAddresses(listenAddresses ? listenAddresses : ""),
    stopLock(),
    stopCount(0)
{
    /*
     * Bus needs a pointer to this internal object.
     */
    bus.busInternal = this;

    /*
     * Create the standard interfaces
     */
    QStatus status = org::freedesktop::DBus::CreateInterfaces(bus);
    if (ER_OK != status) {
        QCC_LogError(status, ("Cannot create %s interface", org::freedesktop::DBus::InterfaceName));
    }
    status = org::alljoyn::CreateInterfaces(bus);
    if (ER_OK != status) {
        QCC_LogError(status, ("Cannot create %s interface", org::alljoyn::Bus::InterfaceName));
    }
    /* Register bus client authentication mechanisms */
    authManager.RegisterMechanism(AuthMechDBusCookieSHA1::Factory, AuthMechDBusCookieSHA1::AuthName());
    authManager.RegisterMechanism(AuthMechExternal::Factory, AuthMechExternal::AuthName());
    authManager.RegisterMechanism(AuthMechAnonymous::Factory, AuthMechAnonymous::AuthName());
}

BusAttachment::Internal::~Internal()
{
    /*
     * Make sure that all threads that might possibly access this object have been joined.
     */
    timer.Join();
    transportList.Join();
    delete router;
    router = NULL;
}

class LocalTransportFactoryContainer : public TransportFactoryContainer {
  public:
    void Init()
    {
        if (ClientTransport::IsAvailable()) {
            Add(new TransportFactory<ClientTransport>(ClientTransport::TransportName, true));
        }
        if (NullTransport::IsAvailable()) {
            Add(new TransportFactory<NullTransport>(NullTransport::TransportName, true));
        }
    }
} localTransportsContainer;
volatile int32_t transportContainerInit = 0;

BusAttachment::BusAttachment(const char* applicationName, bool allowRemoteMessages, uint32_t concurrency) :
    hasStarted(false),
    isStarted(false),
    isStopping(false),
    concurrency(concurrency),
    busInternal(new Internal(applicationName, *this, localTransportsContainer, NULL, allowRemoteMessages, NULL)),
    joinObj(this)
{
    if (IncrementAndFetch(&transportContainerInit) == 1) {
        localTransportsContainer.Init();
    } else {
        DecrementAndFetch(&transportContainerInit);         // adjust the count
    }
    QCC_DbgTrace(("BusAttachment client constructor (%p)", this));
}

BusAttachment::BusAttachment(Internal* busInternal, uint32_t concurrency) :
    hasStarted(false),
    isStarted(false),
    isStopping(false),
    concurrency(concurrency),
    busInternal(busInternal),
    joinObj(this)
{
    if (IncrementAndFetch(&transportContainerInit) == 1) {
        localTransportsContainer.Init();
    } else {
        DecrementAndFetch(&transportContainerInit);         // adjust the count
    }
    QCC_DbgTrace(("BusAttachment daemon constructor"));
}

BusAttachment::~BusAttachment(void)
{
    QCC_DbgTrace(("BusAttachment Destructor (%p)", this));

    StopInternal(true);

    /*
     * Other threads may be attempting to stop the bus. We need to wait for ALL
     * callers of BusAttachment::StopInternal() to exit before deleting the
     * object
     */
    while (busInternal->stopCount) {
        /*
         * We want to allow other calling threads to complete.  This means we
         * need to yield the CPU.  Sleep(0) yields the CPU to all threads of
         * equal or greater priority.  Other callers may be of lesser priority
         * so We need to yield the CPU to them, too.  We need to get ourselves
         * off of the ready queue, so we need to really execute a sleep.  The
         * Sleep(1) will translate into a mimimum sleep of one scheduling quantum
         * which is, for example, one Jiffy in Liux which is 1/250 second or
         * 4 ms.  It's not as arbitrary as it might seem.
         */
        qcc::Sleep(1);
    }

    delete busInternal;
    busInternal = NULL;
}

uint32_t BusAttachment::GetConcurrency()
{
    return concurrency;
}

QStatus BusAttachment::Start()
{
    QStatus status;

    QCC_DbgTrace(("BusAttachment::Start()"));

    /*
     * The variable isStarted indicates that the bus has been Start()ed, and has
     * not yet been Stop()ed.  As soon as a Join is completed, isStarted is set
     * to false.  We want to prevent the bus attachment from being started
     * multiple times to prevent very hard to debug problems where users try to
     * reuse bus attachments in the mistaken belief that it will somehow be more
     * efficient.  There are three state variables here and we check them all
     * separately (in order to be specific with error messages) before
     * continuing to allow a Start.
     */
    if (hasStarted) {
        status = ER_BUS_BUS_ALREADY_STARTED;
        QCC_LogError(status, ("BusAttachment::Start(): Start may not ever be called more than once"));
        return status;
    }

    if (isStarted) {
        status = ER_BUS_BUS_ALREADY_STARTED;
        QCC_LogError(status, ("BusAttachment::Start(): Start called, but currently started."));
        return status;
    }

    if (isStopping) {
        status = ER_BUS_STOPPING;
        QCC_LogError(status, ("BusAttachment::Start(): Start called while stopping"));
        return status;
    }

    isStarted = hasStarted = true;

    /* Start the timer */
    status = busInternal->timer.Start();

    if (ER_OK == status) {
        /* Start the transports */
        status = busInternal->transportList.Start(busInternal->GetListenAddresses());
    }

    if ((status == ER_OK) && isStopping) {
        status = ER_BUS_STOPPING;
        QCC_LogError(status, ("BusAttachment::Start bus was stopped while starting"));
    }

    if (status != ER_OK) {
        QCC_LogError(status, ("BusAttachment::Start failed to start"));
        busInternal->timer.Stop();
        busInternal->transportList.Stop();
        WaitStopInternal();
    }
    return status;
}

QStatus BusAttachment::TryConnect(const char* connectSpec, BusEndpoint** newep)
{
    QCC_DbgTrace(("BusAttachment::TryConnect to %s", connectSpec));
    QStatus status = ER_OK;
    /* Get or create transport for connection */
    Transport* trans = busInternal->transportList.GetTransport(connectSpec);
    if (trans) {
        SessionOpts emptyOpts;
        status = trans->Connect(connectSpec, emptyOpts, newep);
    } else {
        status = ER_BUS_TRANSPORT_NOT_AVAILABLE;
    }
    return status;
}

QStatus BusAttachment::Connect(const char* connectSpec, BusEndpoint** newep)
{
    QStatus status;
    bool isDaemon = busInternal->GetRouter().IsDaemon();

    if (!isStarted) {
        status = ER_BUS_BUS_NOT_STARTED;
    } else if (isStopping) {
        status = ER_BUS_STOPPING;
        QCC_LogError(status, ("BusAttachment::Connect cannot connect while bus is stopping"));
    } else if (IsConnected() && !isDaemon) {
        status = ER_BUS_ALREADY_CONNECTED;
    } else {
        this->connectSpec = connectSpec;
        status = TryConnect(connectSpec, newep);
        /*
         * Try using the null transport to connect to a bundled daemon if there is one
         */
        if (status != ER_OK && !isDaemon) {
            qcc::String bundledConnectSpec = "null:";
            if (bundledConnectSpec != connectSpec) {
                status = TryConnect(bundledConnectSpec.c_str(), newep);
                if (ER_OK == status) {
                    this->connectSpec = bundledConnectSpec;
                }
            }
        }
        /* If this is a client (non-daemon) bus attachment, then register signal handlers for BusListener */
        if ((ER_OK == status) && !isDaemon) {
            const InterfaceDescription* iface = GetInterface(org::freedesktop::DBus::InterfaceName);
            assert(iface);
            status = RegisterSignalHandler(busInternal,
                                           static_cast<MessageReceiver::SignalHandler>(&BusAttachment::Internal::AllJoynSignalHandler),
                                           iface->GetMember("NameOwnerChanged"),
                                           NULL);

            if (ER_OK == status) {
                Message reply(*this);
                MsgArg arg("s", "type='signal',interface='org.freedesktop.DBus'");
                const ProxyBusObject& dbusObj = this->GetDBusProxyObj();
                status = dbusObj.MethodCall(org::freedesktop::DBus::InterfaceName, "AddMatch", &arg, 1, reply);
            }

            /* Register org.alljoyn.Bus signal handler */
            const InterfaceDescription* ajIface = GetInterface(org::alljoyn::Bus::InterfaceName);
            if (ER_OK == status) {
                assert(ajIface);
                status = RegisterSignalHandler(busInternal,
                                               static_cast<MessageReceiver::SignalHandler>(&BusAttachment::Internal::AllJoynSignalHandler),
                                               ajIface->GetMember("FoundAdvertisedName"),
                                               NULL);
            }
            if (ER_OK == status) {
                assert(ajIface);
                status = RegisterSignalHandler(busInternal,
                                               static_cast<MessageReceiver::SignalHandler>(&BusAttachment::Internal::AllJoynSignalHandler),
                                               ajIface->GetMember("LostAdvertisedName"),
                                               NULL);
            }
            if (ER_OK == status) {
                assert(ajIface);
                status = RegisterSignalHandler(busInternal,
                                               static_cast<MessageReceiver::SignalHandler>(&BusAttachment::Internal::AllJoynSignalHandler),
                                               ajIface->GetMember("SessionLost"),
                                               NULL);
            }
            if (ER_OK == status) {
                assert(ajIface);
                status = RegisterSignalHandler(busInternal,
                                               static_cast<MessageReceiver::SignalHandler>(&BusAttachment::Internal::AllJoynSignalHandler),
                                               ajIface->GetMember("MPSessionChanged"),
                                               NULL);
            }
            if (ER_OK == status) {
                Message reply(*this);
                MsgArg arg("s", "type='signal',interface='org.alljoyn.Bus'");
                const ProxyBusObject& dbusObj = this->GetDBusProxyObj();
                status = dbusObj.MethodCall(org::freedesktop::DBus::InterfaceName, "AddMatch", &arg, 1, reply);
            } else {
                /*
                 * We connected but failed to fully realize the connection so disconnect to cleanup.
                 */
                Transport* trans = busInternal->transportList.GetTransport(connectSpec);
                if (trans) {
                    trans->Disconnect(connectSpec);
                }
            }
        }
    }
    if (ER_OK != status) {
        QCC_LogError(status, ("BusAttachment::Connect failed"));
    }
    return status;
}

QStatus BusAttachment::Disconnect(const char* connectSpec)
{
    QStatus status;
    bool isDaemon = busInternal->GetRouter().IsDaemon();

    if (!isStarted) {
        status = ER_BUS_BUS_NOT_STARTED;
    } else if (isStopping) {
        status = ER_BUS_STOPPING;
        QCC_LogError(status, ("BusAttachment::Disconnect cannot disconnect while bus is stopping"));
    } else if (!isDaemon && !IsConnected()) {
        status = ER_BUS_NOT_CONNECTED;
    } else {
        /* Terminate transport for connection */
        Transport* trans = busInternal->transportList.GetTransport(this->connectSpec.c_str());

        if (trans) {
            status = trans->Disconnect(this->connectSpec.c_str());
        } else {
            status = ER_BUS_TRANSPORT_NOT_AVAILABLE;
        }

        /* Unregister signal handlers if this is a client-side bus attachment */
        if ((ER_OK == status) && !isDaemon) {
            const InterfaceDescription* dbusIface = GetInterface(org::freedesktop::DBus::InterfaceName);
            if (dbusIface) {
                UnregisterSignalHandler(busInternal,
                                        static_cast<MessageReceiver::SignalHandler>(&BusAttachment::Internal::AllJoynSignalHandler),
                                        dbusIface->GetMember("NameOwnerChanged"),
                                        NULL);
            }
            const InterfaceDescription* alljoynIface = GetInterface(org::alljoyn::Bus::InterfaceName);
            if (alljoynIface) {
                UnregisterSignalHandler(busInternal,
                                        static_cast<MessageReceiver::SignalHandler>(&BusAttachment::Internal::AllJoynSignalHandler),
                                        alljoynIface->GetMember("FoundAdvertisedName"),
                                        NULL);
            }
            if (alljoynIface) {
                UnregisterSignalHandler(busInternal,
                                        static_cast<MessageReceiver::SignalHandler>(&BusAttachment::Internal::AllJoynSignalHandler),
                                        alljoynIface->GetMember("LostAdvertisedName"),
                                        NULL);
            }
            if (alljoynIface) {
                UnregisterSignalHandler(busInternal,
                                        static_cast<MessageReceiver::SignalHandler>(&BusAttachment::Internal::AllJoynSignalHandler),
                                        alljoynIface->GetMember("SessionLost"),
                                        NULL);
            }
            if (alljoynIface) {
                UnregisterSignalHandler(busInternal,
                                        static_cast<MessageReceiver::SignalHandler>(&BusAttachment::Internal::AllJoynSignalHandler),
                                        alljoynIface->GetMember("MPSessionChanged"),
                                        NULL);
            }
        }
    }

    if (ER_OK != status) {
        QCC_LogError(status, ("BusAttachment::Disconnect failed"));
    }
    return status;
}

QStatus BusAttachment::Stop(void)
{
    return StopInternal(false);
}

/*
 * Note if called with blockUntilStopped == false this function must not do anything that might block.
 * Because we don't know what kind of cleanup various transports may do on Stop() the transports are
 * stopped on the ThreadExit callback for the dispatch thread.
 */
QStatus BusAttachment::StopInternal(bool blockUntilStopped)
{
    QStatus status = ER_OK;
    if (isStarted) {
        isStopping = true;
        /*
         * Let bus listeners know the bus is stopping.
         */
        busInternal->listenersLock.Lock(MUTEX_CONTEXT);
        Internal::ListenerSet::iterator it = busInternal->listeners.begin();
        while (it != busInternal->listeners.end()) {
            Internal::ProtectedBusListener l = *it;
            busInternal->listenersLock.Unlock(MUTEX_CONTEXT);
            (*l)->BusStopping();
            busInternal->listenersLock.Lock(MUTEX_CONTEXT);
            it = busInternal->listeners.upper_bound(l);
        }
        busInternal->listenersLock.Unlock(MUTEX_CONTEXT);
        /* Stop the timer thread */
        status = busInternal->timer.Stop();
        if (ER_OK != status) {
            QCC_LogError(status, ("Timer::Stop() failed"));
        }
        /* Stop the transport list */
        status = busInternal->transportList.Stop();
        if (ER_OK != status) {
            QCC_LogError(status, ("TransportList::Stop() failed"));
        }

        if ((status == ER_OK) && blockUntilStopped) {
            WaitStopInternal();
        }
    }
    return status;
}

QStatus BusAttachment::Join()
{
    QCC_DbgTrace(("BusAttachment::Join"));
    WaitStopInternal();
    return ER_OK;
}

void BusAttachment::WaitStopInternal()
{
    QCC_DbgTrace(("BusAttachment::WaitStopInternal"));
    if (isStarted) {
        /*
         * We use a combination of a mutex and a counter to ensure that all threads that are
         * blocked waiting for the bus attachment to stop are actually blocked.
         */
        IncrementAndFetch(&busInternal->stopCount);
        busInternal->stopLock.Lock(MUTEX_CONTEXT);

        /*
         * In the case where more than one thread has called WaitStopInternal() the first thread in will
         * clear the isStarted flag.
         */
        if (isStarted) {
            busInternal->timer.Join();
            busInternal->transportList.Join();

            /* Clear peer state */
            busInternal->peerStateTable.Clear();

            /* Persist keystore */
            busInternal->keyStore.Store();

            isStarted = false;
            isStopping = false;

            busInternal->listeners.clear();

            busInternal->sessionPortListeners.clear();

            busInternal->sessionListeners.clear();
        }

        busInternal->stopLock.Unlock(MUTEX_CONTEXT);
        DecrementAndFetch(&busInternal->stopCount);
    }
}

QStatus BusAttachment::CreateInterface(const char* name, InterfaceDescription*& iface, bool secure)
{
    if (NULL != GetInterface(name)) {
        iface = NULL;
        return ER_BUS_IFACE_ALREADY_EXISTS;
    }
    StringMapKey key = String(name);
    InterfaceDescription intf(name, secure);
    iface = &(busInternal->ifaceDescriptions.insert(pair<StringMapKey, InterfaceDescription>(key, intf)).first->second);
    return ER_OK;
}

QStatus BusAttachment::DeleteInterface(InterfaceDescription& iface)
{
    /* Get the (hopefully) unactivated interface */
    map<StringMapKey, InterfaceDescription>::iterator it = busInternal->ifaceDescriptions.find(StringMapKey(iface.GetName()));
    if ((it != busInternal->ifaceDescriptions.end()) && !it->second.isActivated) {
        busInternal->ifaceDescriptions.erase(it);
        return ER_OK;
    } else {
        return ER_BUS_NO_SUCH_INTERFACE;
    }
}

size_t BusAttachment::GetInterfaces(const InterfaceDescription** ifaces, size_t numIfaces) const
{
    size_t count = 0;
    map<qcc::StringMapKey, InterfaceDescription>::const_iterator it;
    for (it = busInternal->ifaceDescriptions.begin(); it != busInternal->ifaceDescriptions.end(); it++) {
        if (it->second.isActivated) {
            if (ifaces && (count < numIfaces)) {
                ifaces[count] = &(it->second);
            }
            ++count;
        }
    }
    return count;
}

const InterfaceDescription* BusAttachment::GetInterface(const char* name) const
{
    map<StringMapKey, InterfaceDescription>::const_iterator it = busInternal->ifaceDescriptions.find(StringMapKey(name));
    if ((it != busInternal->ifaceDescriptions.end()) && it->second.isActivated) {
        return &(it->second);
    } else {
        return NULL;
    }
}

QStatus BusAttachment::RegisterKeyStoreListener(KeyStoreListener& listener)
{
    return busInternal->keyStore.SetListener(listener);
}

void BusAttachment::ClearKeyStore()
{
    busInternal->keyStore.Clear();
}

const qcc::String BusAttachment::GetUniqueName() const
{
    /*
     * Cannot have a valid unique name if not connected to the bus.
     */
    if (!IsConnected()) {
        return "";
    }
    return busInternal->localEndpoint.GetUniqueName();
}

const qcc::String& BusAttachment::GetGlobalGUIDString() const
{
    return busInternal->GetGlobalGUID().ToString();
}

const ProxyBusObject& BusAttachment::GetDBusProxyObj()
{
    return busInternal->localEndpoint.GetDBusProxyObj();
}

const ProxyBusObject& BusAttachment::GetAllJoynProxyObj()
{
    return busInternal->localEndpoint.GetAllJoynProxyObj();
}

const ProxyBusObject& BusAttachment::GetAllJoynDebugObj()
{
    return busInternal->localEndpoint.GetAllJoynDebugObj();
}

QStatus BusAttachment::RegisterSignalHandler(MessageReceiver* receiver,
                                             MessageReceiver::SignalHandler signalHandler,
                                             const InterfaceDescription::Member* member,
                                             const char* srcPath)
{
    return busInternal->localEndpoint.RegisterSignalHandler(receiver, signalHandler, member, srcPath);
}

QStatus BusAttachment::UnregisterSignalHandler(MessageReceiver* receiver,
                                               MessageReceiver::SignalHandler signalHandler,
                                               const InterfaceDescription::Member* member,
                                               const char* srcPath)
{
    return busInternal->localEndpoint.UnregisterSignalHandler(receiver, signalHandler, member, srcPath);
}

QStatus BusAttachment::UnregisterAllHandlers(MessageReceiver* receiver)
{
    return busInternal->localEndpoint.UnregisterAllHandlers(receiver);
}

bool BusAttachment::IsConnected() const {
    return busInternal->router->IsBusRunning();
}

QStatus BusAttachment::RegisterBusObject(BusObject& obj) {
    return busInternal->localEndpoint.RegisterBusObject(obj);
}

void BusAttachment::UnregisterBusObject(BusObject& object)
{
    busInternal->localEndpoint.UnregisterBusObject(object);
}

QStatus BusAttachment::EnablePeerSecurity(const char* authMechanisms,
                                          AuthListener* listener,
                                          const char* keyStoreFileName,
                                          bool isShared)
{
    QStatus status = ER_OK;

    /* If there are no auth mechanisms peer security is being disabled. */
    if (authMechanisms) {
        status = busInternal->keyStore.Init(keyStoreFileName, isShared);
        if (status == ER_OK) {
            /* Register peer-to-peer authentication mechanisms */
            busInternal->authManager.RegisterMechanism(AuthMechSRP::Factory, AuthMechSRP::AuthName());
            busInternal->authManager.RegisterMechanism(AuthMechRSA::Factory, AuthMechRSA::AuthName());
            busInternal->authManager.RegisterMechanism(AuthMechLogon::Factory, AuthMechLogon::AuthName());
            /* Validate the list of auth mechanisms */
            status =  busInternal->authManager.CheckNames(authMechanisms);
        }
    }
    if (status == ER_OK) {
        AllJoynPeerObj* peerObj = busInternal->localEndpoint.GetPeerObj();
        if (peerObj) {
            peerObj->SetupPeerAuthentication(authMechanisms, authMechanisms ? listener : NULL);
        } else {
            return ER_BUS_SECURITY_NOT_ENABLED;
        }
    }
    return status;
}

bool BusAttachment::IsPeerSecurityEnabled()
{
    AllJoynPeerObj* peerObj = busInternal->localEndpoint.GetPeerObj();
    if (peerObj) {
        return peerObj->AuthenticationEnabled();
    } else {
        return false;
    }
}

QStatus BusAttachment::AddLogonEntry(const char* authMechanism, const char* userName, const char* password)
{
    if (!authMechanism) {
        return ER_BAD_ARG_2;
    }
    if (!userName) {
        return ER_BAD_ARG_3;
    }
    if (strcmp(authMechanism, "ALLJOYN_SRP_LOGON") == 0) {
        return AuthMechLogon::AddLogonEntry(busInternal->keyStore, userName, password);
    } else {
        return ER_BUS_INVALID_AUTH_MECHANISM;
    }
}

QStatus BusAttachment::RequestName(const char* requestedName, uint32_t flags)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg args[2];
    size_t numArgs = ArraySize(args);

    MsgArg::Set(args, numArgs, "su", requestedName, flags);

    const ProxyBusObject& dbusObj = this->GetDBusProxyObj();
    QStatus status = dbusObj.MethodCall(org::freedesktop::DBus::InterfaceName, "RequestName", args, numArgs, reply);
    if (ER_OK == status) {
        uint32_t disposition;
        status = reply->GetArgs("u", &disposition);
        if (ER_OK == status) {
            switch (disposition) {
            case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER :
                break;

            case DBUS_REQUEST_NAME_REPLY_IN_QUEUE :
                status = ER_DBUS_REQUEST_NAME_REPLY_IN_QUEUE;
                break;

            case DBUS_REQUEST_NAME_REPLY_EXISTS :
                status = ER_DBUS_REQUEST_NAME_REPLY_EXISTS;
                break;

            case DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER:
                status = ER_DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER;
                break;

            default:
                status = ER_BUS_UNEXPECTED_DISPOSITION;
                break;
            }
        }
    } else {
        QCC_LogError(status, ("%s.RequestName returned ERROR_MESSAGE (error=%s)", org::freedesktop::DBus::InterfaceName, reply->GetErrorDescription().c_str()));
    }
    return status;
}

QStatus BusAttachment::ReleaseName(const char* name)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg args[1];
    size_t numArgs = ArraySize(args);

    MsgArg::Set(args, numArgs, "s", name);

    const ProxyBusObject& dbusObj = this->GetDBusProxyObj();
    QStatus status = dbusObj.MethodCall(org::freedesktop::DBus::InterfaceName, "ReleaseName", args, numArgs, reply);
    if (ER_OK == status) {
        uint32_t disposition;
        status = reply->GetArgs("u", &disposition);
        if (ER_OK == status) {
            switch (disposition) {
            case DBUS_RELEASE_NAME_REPLY_RELEASED:
                break;

            case DBUS_RELEASE_NAME_REPLY_NON_EXISTENT:
                status = ER_DBUS_RELEASE_NAME_REPLY_NON_EXISTENT;
                break;

            case DBUS_RELEASE_NAME_REPLY_NOT_OWNER:
                status = ER_DBUS_RELEASE_NAME_REPLY_NOT_OWNER;
                break;

            default:
                status = ER_BUS_UNEXPECTED_DISPOSITION;
                break;
            }
        }
    } else {
        QCC_LogError(status, ("%s.ReleaseName returned ERROR_MESSAGE (error=%s)", org::freedesktop::DBus::InterfaceName, reply->GetErrorDescription().c_str()));
    }
    return status;
}

QStatus BusAttachment::AddMatch(const char* rule)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg args[1];
    size_t numArgs = ArraySize(args);

    MsgArg::Set(args, numArgs, "s", rule);

    const ProxyBusObject& dbusObj = this->GetDBusProxyObj();
    QStatus status = dbusObj.MethodCall(org::freedesktop::DBus::InterfaceName, "AddMatch", args, numArgs, reply);
    if (ER_OK != status) {
        QCC_LogError(status, ("%s.AddMatch returned ERROR_MESSAGE (error=%s)", org::freedesktop::DBus::InterfaceName, reply->GetErrorDescription().c_str()));
    }
    return status;
}

QStatus BusAttachment::RemoveMatch(const char* rule)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg args[1];
    size_t numArgs = ArraySize(args);

    MsgArg::Set(args, numArgs, "s", rule);

    const ProxyBusObject& dbusObj = this->GetDBusProxyObj();
    QStatus status = dbusObj.MethodCall(org::freedesktop::DBus::InterfaceName, "RemoveMatch", args, numArgs, reply);
    if (ER_OK != status) {
        QCC_LogError(status, ("%s.RemoveMatch returned ERROR_MESSAGE (error=%s)", org::freedesktop::DBus::InterfaceName, reply->GetErrorDescription().c_str()));
    }
    return status;
}

QStatus BusAttachment::FindAdvertisedName(const char* namePrefix)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    if (!namePrefix) {
        return ER_BAD_ARG_1;
    }

    Message reply(*this);
    MsgArg args[1];
    size_t numArgs = ArraySize(args);

    MsgArg::Set(args, numArgs, "s", namePrefix);

    const ProxyBusObject& alljoynObj = this->GetAllJoynProxyObj();
    QStatus status = alljoynObj.MethodCall(org::alljoyn::Bus::InterfaceName, "FindAdvertisedName", args, numArgs, reply);
    if (ER_OK == status) {
        uint32_t disposition;
        status = reply->GetArgs("u", &disposition);
        if (ER_OK == status) {
            switch (disposition) {
            case ALLJOYN_FINDADVERTISEDNAME_REPLY_SUCCESS:
                break;

            case ALLJOYN_FINDADVERTISEDNAME_REPLY_ALREADY_DISCOVERING:
                status = ER_ALLJOYN_FINDADVERTISEDNAME_REPLY_ALREADY_DISCOVERING;
                break;

            case ALLJOYN_FINDADVERTISEDNAME_REPLY_FAILED:
                status = ER_ALLJOYN_FINDADVERTISEDNAME_REPLY_FAILED;
                break;

            default:
                status = ER_BUS_UNEXPECTED_DISPOSITION;
                break;
            }
        }
    } else {
        QCC_LogError(status, ("%s.FindAdvertisedName returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
    }
    return status;
}

QStatus BusAttachment::CancelFindAdvertisedName(const char* namePrefix)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg args[1];
    size_t numArgs = ArraySize(args);

    MsgArg::Set(args, numArgs, "s", namePrefix);

    const ProxyBusObject& alljoynObj = this->GetAllJoynProxyObj();
    QStatus status = alljoynObj.MethodCall(org::alljoyn::Bus::InterfaceName, "CancelFindAdvertisedName", args, numArgs, reply);
    if (ER_OK == status) {
        uint32_t disposition;
        status = reply->GetArgs("u", &disposition);
        if (ER_OK == status) {
            switch (disposition) {
            case ALLJOYN_CANCELFINDADVERTISEDNAME_REPLY_SUCCESS:
                break;

            case ALLJOYN_CANCELFINDADVERTISEDNAME_REPLY_FAILED:
                status = ER_ALLJOYN_CANCELFINDADVERTISEDNAME_REPLY_FAILED;
                break;

            default:
                status = ER_BUS_UNEXPECTED_DISPOSITION;
                break;
            }
        }
    } else {
        QCC_LogError(status, ("%s.CancelFindAdvertisedName returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
    }
    return status;
}

QStatus BusAttachment::AdvertiseName(const char* name, TransportMask transports)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg args[2];
    size_t numArgs = ArraySize(args);

    MsgArg::Set(args, numArgs, "sq", name, transports);

    const ProxyBusObject& alljoynObj = this->GetAllJoynProxyObj();
    QStatus status = alljoynObj.MethodCall(org::alljoyn::Bus::InterfaceName, "AdvertiseName", args, numArgs, reply);
    if (ER_OK == status) {
        int32_t disposition;
        status = reply->GetArgs("u", &disposition);
        if (ER_OK == status) {
            switch (disposition) {
            case ALLJOYN_ADVERTISENAME_REPLY_SUCCESS:
                break;

            case ALLJOYN_ADVERTISENAME_REPLY_ALREADY_ADVERTISING:
                status = ER_ALLJOYN_ADVERTISENAME_REPLY_ALREADY_ADVERTISING;
                break;

            case ALLJOYN_ADVERTISENAME_REPLY_FAILED:
                status = ER_ALLJOYN_ADVERTISENAME_REPLY_FAILED;
                break;

            default:
                status = ER_BUS_UNEXPECTED_DISPOSITION;
                break;
            }
        }
    } else {
        QCC_LogError(status, ("%s.AdvertiseName returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
    }
    return status;
}

QStatus BusAttachment::CancelAdvertiseName(const char* name, TransportMask transports)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg args[2];
    size_t numArgs = ArraySize(args);

    MsgArg::Set(args, numArgs, "sq", name, transports);

    const ProxyBusObject& alljoynObj = this->GetAllJoynProxyObj();
    QStatus status = alljoynObj.MethodCall(org::alljoyn::Bus::InterfaceName, "CancelAdvertiseName", args, numArgs, reply);
    if (ER_OK == status) {
        uint32_t disposition;
        status = reply->GetArgs("u", &disposition);
        if (ER_OK == status) {
            switch (disposition) {
            case ALLJOYN_CANCELADVERTISENAME_REPLY_SUCCESS:
                break;

            case ALLJOYN_CANCELADVERTISENAME_REPLY_FAILED:
                status = ER_ALLJOYN_CANCELADVERTISENAME_REPLY_FAILED;
                break;

            default:
                status = ER_BUS_UNEXPECTED_DISPOSITION;
                break;
            }
        }
    } else {
        QCC_LogError(status, ("%s.CancelAdvertiseName returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
    }
    return status;
}

void BusAttachment::RegisterBusListener(BusListener& listener)
{
    busInternal->listenersLock.Lock(MUTEX_CONTEXT);
    // push front so that we can easily get an iterator pointing to the new element
    BusListener* pListener = &listener;
    Internal::ProtectedBusListener protectedListener(pListener);
    busInternal->listeners.insert(protectedListener);

    /* Let listener know which bus attachment it has been registered on */
    busInternal->listenersLock.Unlock(MUTEX_CONTEXT);
    (*protectedListener)->ListenerRegistered(this);
}

void BusAttachment::UnregisterBusListener(BusListener& listener)
{
    busInternal->listenersLock.Lock(MUTEX_CONTEXT);

    /* Look for listener on ListenerSet */
    Internal::ListenerSet::iterator it = busInternal->listeners.begin();
    while (it != busInternal->listeners.end()) {
        if (**it == &listener) {
            break;
        }
        ++it;
    }

    /* Wait for all refs to ProtectedBusListener to exit */
    while ((it != busInternal->listeners.end()) && (it->GetRefCount() > 1)) {
        Internal::ProtectedBusListener l = *it;
        busInternal->listenersLock.Unlock(MUTEX_CONTEXT);
        qcc::Sleep(5);
        busInternal->listenersLock.Lock(MUTEX_CONTEXT);
        it = busInternal->listeners.find(l);
    }

    /* Delete the listeners entry and call user's callback (unlocked) */
    if (it != busInternal->listeners.end()) {
        Internal::ProtectedBusListener l = *it;
        busInternal->listeners.erase(it);
        busInternal->listenersLock.Unlock(MUTEX_CONTEXT);
        (*l)->ListenerUnregistered();
    } else {
        busInternal->listenersLock.Unlock(MUTEX_CONTEXT);
    }
}

QStatus BusAttachment::NameHasOwner(const char* name, bool& hasOwner)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg arg("s", name);
    QStatus status = this->GetDBusProxyObj().MethodCall(org::freedesktop::DBus::InterfaceName, "NameHasOwner", &arg, 1, reply);
    if (ER_OK == status) {
        status = reply->GetArgs("b", &hasOwner);
    } else {
        QCC_LogError(status, ("%s.NameHasOwner returned ERROR_MESSAGE (error=%s)", org::freedesktop::DBus::InterfaceName, reply->GetErrorDescription().c_str()));
    }
    return status;
}

QStatus BusAttachment::SetDaemonDebug(const char* module, uint32_t level)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg args[2];
    size_t argsSize = ArraySize(args);
    MsgArg::Set(args, argsSize, "su", module, level);
    QStatus status = this->GetAllJoynDebugObj().MethodCall(org::alljoyn::Daemon::Debug::InterfaceName, "SetDebugLevel", args, argsSize, reply);
    if (status != ER_OK) {
        String errMsg;
        reply->GetErrorName(&errMsg);
        if (errMsg == "ER_BUS_NO_SUCH_OBJECT") {
            status = ER_BUS_NO_SUCH_OBJECT;
        }
    }
    return status;
}

QStatus BusAttachment::BindSessionPort(SessionPort& sessionPort, const SessionOpts& opts, SessionPortListener& listener)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg args[2];

    args[0].Set("q", sessionPort);
    SetSessionOpts(opts, args[1]);

    QStatus status = this->GetAllJoynProxyObj().MethodCall(org::alljoyn::Bus::InterfaceName, "BindSessionPort", args, ArraySize(args), reply);
    if (status != ER_OK) {
        QCC_LogError(status, ("%s.BindSessionPort returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
    } else {
        SessionPort tempPort;
        uint32_t disposition;
        status = reply->GetArgs("uq", &disposition, &tempPort);
        if (status == ER_OK) {
            switch (disposition) {
            case ALLJOYN_BINDSESSIONPORT_REPLY_SUCCESS:
                sessionPort = tempPort;
                break;

            case ALLJOYN_BINDSESSIONPORT_REPLY_ALREADY_EXISTS:
                status = ER_ALLJOYN_BINDSESSIONPORT_REPLY_ALREADY_EXISTS;
                break;

            case ALLJOYN_BINDSESSIONPORT_REPLY_INVALID_OPTS:
                status = ER_ALLJOYN_BINDSESSIONPORT_REPLY_INVALID_OPTS;
                break;

            default:
            case ALLJOYN_BINDSESSIONPORT_REPLY_FAILED:
                status = ER_ALLJOYN_BINDSESSIONPORT_REPLY_FAILED;
                break;
            }
        }
        if (status == ER_OK) {
            busInternal->sessionListenersLock.Lock(MUTEX_CONTEXT);
            SessionPortListener* pListener = &listener;
            pair<SessionPort, Internal::ProtectedSessionPortListener> elem(sessionPort, Internal::ProtectedSessionPortListener(pListener));
            busInternal->sessionPortListeners.insert(elem);
            busInternal->sessionListenersLock.Unlock(MUTEX_CONTEXT);
        }
    }
    return status;
}

QStatus BusAttachment::UnbindSessionPort(SessionPort sessionPort)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg args[1];

    args[0].Set("q", sessionPort);

    QStatus status = this->GetAllJoynProxyObj().MethodCall(org::alljoyn::Bus::InterfaceName, "UnbindSessionPort", args, ArraySize(args), reply);
    if (status != ER_OK) {
        QCC_LogError(status, ("%s.UnbindSessionPort returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
    } else {
        uint32_t disposition;
        status = reply->GetArgs("u", &disposition);
        if (status == ER_OK) {
            switch (disposition) {
            case ALLJOYN_UNBINDSESSIONPORT_REPLY_SUCCESS:
                status = ER_OK;
                break;

            case ALLJOYN_UNBINDSESSIONPORT_REPLY_BAD_PORT:
                status = ER_ALLJOYN_UNBINDSESSIONPORT_REPLY_BAD_PORT;
                break;

            case ALLJOYN_UNBINDSESSIONPORT_REPLY_FAILED:
            default:
                status = ER_ALLJOYN_UNBINDSESSIONPORT_REPLY_FAILED;
                break;
            }
        }
        if (status == ER_OK) {
            busInternal->sessionListenersLock.Lock(MUTEX_CONTEXT);
            Internal::SessionPortListenerMap::iterator it =
                busInternal->sessionPortListeners.find(sessionPort);

            if (it != busInternal->sessionPortListeners.end()) {
                while (it->second.GetRefCount() > 1) {
                    busInternal->sessionListenersLock.Unlock(MUTEX_CONTEXT);
                    qcc::Sleep(5);
                    busInternal->sessionListenersLock.Lock(MUTEX_CONTEXT);
                }
                busInternal->sessionPortListeners.erase(sessionPort);
            }
            busInternal->sessionListenersLock.Unlock(MUTEX_CONTEXT);
        }
    }
    return status;
}

QStatus BusAttachment::JoinSessionAsync(const char* sessionHost, SessionPort sessionPort, SessionListener* sessionListener,
                                        const SessionOpts& opts, BusAttachment::JoinSessionAsyncCB* callback, void* context)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }
    if (!IsLegalBusName(sessionHost)) {
        return ER_BUS_BAD_BUS_NAME;
    }

    MsgArg args[3];
    size_t numArgs = 2;

    MsgArg::Set(args, numArgs, "sq", sessionHost, sessionPort);
    SetSessionOpts(opts, args[2]);

    const ProxyBusObject& alljoynObj = this->GetAllJoynProxyObj();
    JoinSessionAsyncCBContext* cbCtx = new JoinSessionAsyncCBContext(callback, sessionListener, context);

    QStatus status = alljoynObj.MethodCallAsync(org::alljoyn::Bus::InterfaceName,
                                                "JoinSession",
                                                busInternal,
                                                static_cast<MessageReceiver::ReplyHandler>(&BusAttachment::Internal::JoinSessionAsyncCB),
                                                args,
                                                ArraySize(args),
                                                cbCtx,
                                                90000);
    if (status != ER_OK) {
        delete cbCtx;
    }
    return status;
}


void BusAttachment::Internal::JoinSessionAsyncCB(Message& reply, void* context)
{
    JoinSessionAsyncCBContext* ctx = reinterpret_cast<JoinSessionAsyncCBContext*>(context);

    QStatus status = ER_FAIL;
    SessionId sessionId = 0;
    SessionOpts opts;
    if (reply->GetType() == MESSAGE_METHOD_RET) {
        status = bus.GetJoinSessionResponse(reply, sessionId, opts);
    } else if (reply->GetType() == MESSAGE_ERROR) {
        status = ER_BUS_REPLY_IS_ERROR_MESSAGE;
        QCC_LogError(status, ("%s.JoinSession returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
    }
    if (ctx->sessionListener && (status == ER_OK)) {
        sessionListenersLock.Lock(MUTEX_CONTEXT);
        sessionListeners[sessionId] = ProtectedSessionListener(ctx->sessionListener);
        sessionListenersLock.Unlock(MUTEX_CONTEXT);
    }

    /* Call the callback */
    ctx->callback->JoinSessionCB(status, sessionId, opts, ctx->context);
    delete ctx;
}

QStatus BusAttachment::GetJoinSessionResponse(Message& reply, SessionId& sessionId, SessionOpts& opts)
{
    QStatus status = ER_OK;
    const MsgArg* replyArgs;
    size_t na;
    reply->GetArgs(na, replyArgs);
    assert(na == 3);
    uint32_t disposition = replyArgs[0].v_uint32;
    sessionId = replyArgs[1].v_uint32;
    status = GetSessionOpts(replyArgs[2], opts);
    if (status != ER_OK) {
        sessionId = 0;
    } else {
        switch (disposition) {
        case ALLJOYN_JOINSESSION_REPLY_SUCCESS:
            break;

        case ALLJOYN_JOINSESSION_REPLY_NO_SESSION:
            status = ER_ALLJOYN_JOINSESSION_REPLY_NO_SESSION;
            break;

        case ALLJOYN_JOINSESSION_REPLY_UNREACHABLE:
            status = ER_ALLJOYN_JOINSESSION_REPLY_UNREACHABLE;
            break;

        case ALLJOYN_JOINSESSION_REPLY_CONNECT_FAILED:
            status = ER_ALLJOYN_JOINSESSION_REPLY_CONNECT_FAILED;
            break;

        case ALLJOYN_JOINSESSION_REPLY_REJECTED:
            status = ER_ALLJOYN_JOINSESSION_REPLY_REJECTED;
            break;

        case ALLJOYN_JOINSESSION_REPLY_BAD_SESSION_OPTS:
            status = ER_ALLJOYN_JOINSESSION_REPLY_BAD_SESSION_OPTS;
            break;

        case ALLJOYN_JOINSESSION_REPLY_ALREADY_JOINED:
            status = ER_ALLJOYN_JOINSESSION_REPLY_ALREADY_JOINED;
            break;

        case ALLJOYN_JOINSESSION_REPLY_FAILED:
            status = ER_ALLJOYN_JOINSESSION_REPLY_FAILED;
            break;

        default:
            status = ER_BUS_UNEXPECTED_DISPOSITION;
            break;
        }
    }

    return status;
}

QStatus BusAttachment::JoinSession(const char* sessionHost, SessionPort sessionPort, SessionListener* listener, SessionId& sessionId, SessionOpts& opts)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }
    if (!IsLegalBusName(sessionHost)) {
        return ER_BUS_BAD_BUS_NAME;
    }

    Message reply(*this);
    MsgArg args[3];
    size_t numArgs = 2;

    MsgArg::Set(args, numArgs, "sq", sessionHost, sessionPort);
    SetSessionOpts(opts, args[2]);

    const ProxyBusObject& alljoynObj = this->GetAllJoynProxyObj();
    QStatus status = alljoynObj.MethodCall(org::alljoyn::Bus::InterfaceName, "JoinSession", args, ArraySize(args), reply);

    if (ER_OK == status) {
        status = GetJoinSessionResponse(reply, sessionId, opts);
    } else {
        sessionId = 0;
        QCC_LogError(status, ("%s.JoinSession returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
    }

    if (listener && (status == ER_OK)) {
        busInternal->sessionListenersLock.Lock(MUTEX_CONTEXT);
        busInternal->sessionListeners[sessionId] = Internal::ProtectedSessionListener(listener);
        busInternal->sessionListenersLock.Unlock(MUTEX_CONTEXT);
    }
    return status;
}

QStatus BusAttachment::LeaveSession(const SessionId& sessionId)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg arg("u", sessionId);
    const ProxyBusObject& alljoynObj = this->GetAllJoynProxyObj();
    QStatus status = alljoynObj.MethodCall(org::alljoyn::Bus::InterfaceName, "LeaveSession", &arg, 1, reply);
    if (ER_OK == status) {
        uint32_t disposition;
        status = reply->GetArgs("u", &disposition);
        if (ER_OK == status) {
            switch (disposition) {
            case ALLJOYN_LEAVESESSION_REPLY_SUCCESS:
                break;

            case ALLJOYN_LEAVESESSION_REPLY_NO_SESSION:
                status = ER_ALLJOYN_LEAVESESSION_REPLY_NO_SESSION;
                break;

            case ALLJOYN_LEAVESESSION_REPLY_FAILED:
                status = ER_ALLJOYN_LEAVESESSION_REPLY_FAILED;
                break;

            default:
                status = ER_BUS_UNEXPECTED_DISPOSITION;
                break;
            }
        }
    } else {
        QCC_LogError(status, ("%s.LeaveSession returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
    }

    if (status == ER_OK) {
        busInternal->sessionListenersLock.Lock(MUTEX_CONTEXT);
        Internal::SessionListenerMap::iterator it = busInternal->sessionListeners.find(sessionId);
        if (it != busInternal->sessionListeners.end()) {
            busInternal->sessionListeners.erase(it);
        }
        busInternal->sessionListenersLock.Unlock(MUTEX_CONTEXT);
    }

    return status;
}

QStatus BusAttachment::GetSessionFd(SessionId sessionId, SocketFd& sockFd)
{
    QCC_DbgTrace(("BusAttachment::GetSessionFd sessionId:%d", sessionId));
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    sockFd = qcc::INVALID_SOCKET_FD;

    Message reply(*this);
    MsgArg arg("u", sessionId);
    const ProxyBusObject& alljoynObj = this->GetAllJoynProxyObj();
    QStatus status = alljoynObj.MethodCall(org::alljoyn::Bus::InterfaceName, "GetSessionFd", &arg, 1, reply);
    if (ER_OK == status) {
        status = reply->GetArgs("h", &sockFd);
        if (status == ER_OK) {
            status = qcc::SocketDup(sockFd, sockFd);
            if (status == ER_OK) {
                status = qcc::SetBlocking(sockFd, false);
                if (status != ER_OK) {
                    qcc::Close(sockFd);
                }
            }
        }
    } else {
        QCC_LogError(status, ("%s.GetSessionFd returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
    }
    return status;
}

QStatus BusAttachment::SetLinkTimeoutAsync(SessionId sessionid, uint32_t linkTimeout, BusAttachment::SetLinkTimeoutAsyncCB* callback, void* context)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    MsgArg args[2];
    args[0].Set("u", sessionid);
    args[1].Set("u", linkTimeout);

    const ProxyBusObject& alljoynObj = this->GetAllJoynProxyObj();
    SetLinkTimeoutAsyncCBContext* cbCtx = new SetLinkTimeoutAsyncCBContext(callback, context);
    QStatus status = alljoynObj.MethodCallAsync(
        org::alljoyn::Bus::InterfaceName,
        "SetLinkTimeout",
        busInternal,
        static_cast<MessageReceiver::ReplyHandler>(&BusAttachment::Internal::SetLinkTimeoutAsyncCB),
        args,
        ArraySize(args),
        cbCtx,
        90000);
    if (status != ER_OK) {
        delete cbCtx;
    }
    return status;
}

void BusAttachment::Internal::SetLinkTimeoutAsyncCB(Message& reply, void* context)
{
    SetLinkTimeoutAsyncCBContext* ctx = static_cast<SetLinkTimeoutAsyncCBContext*>(context);
    uint32_t timeout = 0;

    QStatus status = ER_OK;
    if (reply->GetType() == MESSAGE_METHOD_RET) {
        status = bus.GetLinkTimeoutResponse(reply, timeout);
    } else if (reply->GetType() == MESSAGE_ERROR) {
        status = ER_BUS_REPLY_IS_ERROR_MESSAGE;
        QCC_LogError(status, ("%s.JoinSession returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
    }

    /* Call the user's callback */
    ctx->callback->SetLinkTimeoutCB(status, timeout, ctx->context);
    delete ctx;
}

QStatus BusAttachment::GetLinkTimeoutResponse(Message& reply, uint32_t& timeout)
{
    QStatus status = ER_OK;
    const MsgArg* replyArgs;
    size_t na;
    reply->GetArgs(na, replyArgs);
    assert(na == 2);

    switch (replyArgs[0].v_uint32) {
    case ALLJOYN_SETLINKTIMEOUT_REPLY_SUCCESS:
        timeout = replyArgs[1].v_uint32;
        break;

    case ALLJOYN_SETLINKTIMEOUT_REPLY_NO_DEST_SUPPORT:
        status = ER_ALLJOYN_SETLINKTIMEOUT_REPLY_NO_DEST_SUPPORT;
        break;

    case ALLJOYN_SETLINKTIMEOUT_REPLY_NO_SESSION:
        status = ER_BUS_NO_SESSION;
        break;

    default:
    case ALLJOYN_SETLINKTIMEOUT_REPLY_FAILED:
        status = ER_ALLJOYN_SETLINKTIMEOUT_REPLY_FAILED;
        break;
    }

    return status;
}

QStatus BusAttachment::SetLinkTimeout(SessionId sessionId, uint32_t& linkTimeout)
{
    if (!IsConnected()) {
        return ER_BUS_NOT_CONNECTED;
    }

    Message reply(*this);
    MsgArg args[2];

    args[0].Set("u", sessionId);
    args[1].Set("u", linkTimeout);

    QStatus status = this->GetAllJoynProxyObj().MethodCall(org::alljoyn::Bus::InterfaceName, "SetLinkTimeout", args, ArraySize(args), reply);

    if (status == ER_OK) {
        status = GetLinkTimeoutResponse(reply, linkTimeout);
    } else {
        QCC_LogError(status, ("%s.SetLinkTimeout returned ERROR_MESSAGE (error=%s)", org::alljoyn::Bus::InterfaceName, reply->GetErrorDescription().c_str()));
        status = ER_ALLJOYN_SETLINKTIMEOUT_REPLY_NOT_SUPPORTED;
    }

    return status;
}

void BusAttachment::Internal::LocalEndpointDisconnected()
{
    listenersLock.Lock(MUTEX_CONTEXT);
    ListenerSet::iterator it = listeners.begin();
    while (it != listeners.end()) {
        ProtectedBusListener l = *it;
        listenersLock.Unlock(MUTEX_CONTEXT);
        (*l)->BusDisconnected();
        listenersLock.Lock(MUTEX_CONTEXT);
        it = listeners.upper_bound(l);
    }
    listenersLock.Unlock(MUTEX_CONTEXT);
}

void BusAttachment::EnableConcurrentCallbacks()
{
    busInternal->localEndpoint.GetDispatcher().EnableReentrancy();
}

void BusAttachment::Internal::AllJoynSignalHandler(const InterfaceDescription::Member* member,
                                                   const char* srcPath,
                                                   Message& msg)
{
    /* Dispatch thread for BusListener callbacks */
    size_t numArgs;
    const MsgArg* args;
    msg->GetArgs(numArgs, args);

    if (msg->GetType() == MESSAGE_SIGNAL) {
        if (0 == strcmp("FoundAdvertisedName", msg->GetMemberName())) {
            listenersLock.Lock(MUTEX_CONTEXT);
            ListenerSet::iterator it = listeners.begin();
            while (it != listeners.end()) {
                ProtectedBusListener pl = *it;
                listenersLock.Unlock(MUTEX_CONTEXT);
                (*pl)->FoundAdvertisedName(args[0].v_string.str, args[1].v_uint16, args[2].v_string.str);
                listenersLock.Lock(MUTEX_CONTEXT);
                it = listeners.upper_bound(pl);
            }
            listenersLock.Unlock(MUTEX_CONTEXT);
        } else if (0 == strcmp("LostAdvertisedName", msg->GetMemberName())) {
            listenersLock.Lock(MUTEX_CONTEXT);
            ListenerSet::iterator it = listeners.begin();
            while (it != listeners.end()) {
                ProtectedBusListener pl = *it;
                listenersLock.Unlock(MUTEX_CONTEXT);
                (*pl)->LostAdvertisedName(args[0].v_string.str, args[1].v_uint16, args[2].v_string.str);
                listenersLock.Lock(MUTEX_CONTEXT);
                it = listeners.upper_bound(pl);
            }
            listenersLock.Unlock(MUTEX_CONTEXT);
        } else if (0 == strcmp("SessionLost", msg->GetMemberName())) {
            sessionListenersLock.Lock(MUTEX_CONTEXT);
            SessionId id = static_cast<SessionId>(args[0].v_uint32);
            SessionListenerMap::iterator slit = sessionListeners.find(id);
            if (slit != sessionListeners.end()) {
                ProtectedSessionListener pl = slit->second;
                sessionListenersLock.Unlock(MUTEX_CONTEXT);
                if (*pl) {
                    (*pl)->SessionLost(id);
                }
            } else {
                sessionListenersLock.Unlock(MUTEX_CONTEXT);
            }
        } else if (0 == strcmp("NameOwnerChanged", msg->GetMemberName())) {
            listenersLock.Lock(MUTEX_CONTEXT);
            ListenerSet::iterator it = listeners.begin();
            while (it != listeners.end()) {
                ProtectedBusListener pl = *it;
                listenersLock.Unlock(MUTEX_CONTEXT);
                (*pl)->NameOwnerChanged(args[0].v_string.str,
                                        (0 < args[1].v_string.len) ? args[1].v_string.str : NULL,
                                        (0 < args[2].v_string.len) ? args[2].v_string.str : NULL);
                listenersLock.Lock(MUTEX_CONTEXT);
                it = listeners.upper_bound(pl);
            }
            listenersLock.Unlock(MUTEX_CONTEXT);
        } else if (0 == strcmp("MPSessionChanged", msg->GetMemberName())) {
            SessionId id = static_cast<SessionId>(args[0].v_uint32);
            const char* member = args[1].v_string.str;
            sessionListenersLock.Lock(MUTEX_CONTEXT);
            SessionListenerMap::iterator slit = sessionListeners.find(id);
            if (slit != sessionListeners.end()) {
                ProtectedSessionListener pl = slit->second;
                sessionListenersLock.Unlock(MUTEX_CONTEXT);
                if (*pl) {
                    if (args[2].v_bool) {
                        (*pl)->SessionMemberAdded(id, member);
                    } else {
                        (*pl)->SessionMemberRemoved(id, member);
                    }
                }
            } else {
                sessionListenersLock.Unlock(MUTEX_CONTEXT);
            }
        } else {
            QCC_DbgPrintf(("Unrecognized signal \"%s.%s\" received", msg->GetInterface(), msg->GetMemberName()));
        }
    }
}

uint32_t BusAttachment::GetTimestamp()
{
    return qcc::GetTimestamp();
}

QStatus BusAttachment::SetSessionListener(SessionId id, SessionListener* listener)
{
    return busInternal->SetSessionListener(id, listener);
}

QStatus BusAttachment::CreateInterfacesFromXml(const char* xml)
{
    StringSource source(xml);

    /* Parse the XML to update this ProxyBusObject instance (plus any new children and interfaces) */
    XmlParseContext pc(source);
    QStatus status = XmlElement::Parse(pc);
    if (status == ER_OK) {
        XmlHelper xmlHelper(this, "BusAttachment");
        status = xmlHelper.AddInterfaceDefinitions(pc.GetRoot());
    }
    return status;
}

bool BusAttachment::Internal::CallAcceptListeners(SessionPort sessionPort, const char* joiner, const SessionOpts& opts)
{
    bool isAccepted = false;

    /* Call sessionPortListener */
    sessionListenersLock.Lock(MUTEX_CONTEXT);
    Internal::SessionPortListenerMap::iterator it = sessionPortListeners.find(sessionPort);
    if (it != sessionPortListeners.end()) {
        ProtectedSessionPortListener listener = it->second;
        sessionListenersLock.Unlock(MUTEX_CONTEXT);
        isAccepted = (*listener)->AcceptSessionJoiner(sessionPort, joiner, opts);
    } else {
        sessionListenersLock.Unlock(MUTEX_CONTEXT);
        QCC_LogError(ER_FAIL, ("Unable to find sessionPortListener for port=%d", sessionPort));
    }
    return isAccepted;
}

void BusAttachment::Internal::CallJoinedListeners(SessionPort sessionPort, SessionId sessionId, const char* joiner)
{
    /* Call sessionListener */
    sessionListenersLock.Lock(MUTEX_CONTEXT);
    SessionPortListenerMap::iterator it = sessionPortListeners.find(sessionPort);
    if (it != sessionPortListeners.end()) {
        /* Add entry to sessionListeners */
        if (sessionListeners.find(sessionId) == sessionListeners.end()) {
            SessionListener* np = 0;
            sessionListeners.insert(pair<SessionId, ProtectedSessionListener>(sessionId, ProtectedSessionListener(np)));
        }
        /* Notify user */
        ProtectedSessionPortListener cur = it->second;
        sessionListenersLock.Unlock(MUTEX_CONTEXT);
        (*cur)->SessionJoined(sessionPort, sessionId, joiner);
    } else {
        sessionListenersLock.Unlock(MUTEX_CONTEXT);
        QCC_LogError(ER_FAIL, ("Unable to find sessionPortListener for port=%d", sessionPort));
    }
}

QStatus BusAttachment::Internal::SetSessionListener(SessionId id, SessionListener* listener)
{
    QStatus status = ER_BUS_NO_SESSION;
    sessionListenersLock.Lock(MUTEX_CONTEXT);
    SessionListenerMap::iterator it = sessionListeners.find(id);
    if (it != sessionListeners.end()) {
        it->second = ProtectedSessionListener(listener);
        status = ER_OK;
    }
    sessionListenersLock.Unlock(MUTEX_CONTEXT);
    return status;
}


QStatus BusAttachment::GetPeerGUID(const char* name, qcc::String& guid)
{
    PeerStateTable* peerTable = busInternal->GetPeerStateTable();
    qcc::String peerName;
    if (name && *name) {
        peerName = name;
    } else {
        peerName = GetUniqueName();
    }
    if (peerTable->IsKnownPeer(peerName)) {
        guid = peerTable->GetPeerState(peerName)->GetGuid().ToString();
        return ER_OK;
    } else {
        return ER_BUS_NO_PEER_GUID;
    }
}

QStatus BusAttachment::ReloadKeyStore()
{
    return busInternal->keyStore.Reload();
}

QStatus BusAttachment::ClearKeys(const qcc::String& guid)
{
    if (!qcc::GUID128::IsGUID(guid)) {
        return ER_INVALID_GUID;
    } else {
        qcc::GUID128 g(guid);
        if (busInternal->keyStore.HasKey(g)) {
            return busInternal->keyStore.DelKey(g);
        } else {
            return ER_BUS_KEY_UNAVAILABLE;
        }
    }
}

QStatus BusAttachment::SetKeyExpiration(const qcc::String& guid, uint32_t timeout)
{
    if (timeout == 0) {
        return ClearKeys(guid);
    }
    if (!qcc::GUID128::IsGUID(guid)) {
        return ER_INVALID_GUID;
    } else {
        qcc::GUID128 g(guid);
        uint64_t millis = 1000ull * timeout;
        Timespec expiration(millis, TIME_RELATIVE);
        return busInternal->keyStore.SetKeyExpiration(g, expiration);
    }
}

QStatus BusAttachment::GetKeyExpiration(const qcc::String& guid, uint32_t& timeout)
{
    if (!qcc::GUID128::IsGUID(guid)) {
        return ER_INVALID_GUID;
    } else {
        qcc::GUID128 g(guid);
        Timespec expiration;
        QStatus status = busInternal->keyStore.GetKeyExpiration(g, expiration);
        if (status == ER_OK) {
            int64_t deltaMillis = expiration - Timespec(0, TIME_RELATIVE);
            if (deltaMillis < 0) {
                timeout = 0;
            } else if (deltaMillis > (0xFFFFFFFFll * 1000)) {
                timeout = 0xFFFFFFFF;
            } else {
                timeout = (uint32_t)((deltaMillis + 500ull) / 1000ull);
            }
        }
        return status;
    }
}

}
