/**
 * @file
 * LocalTransport is a special type of Transport that is responsible
 * for all communication of all endpoints that terminate at registered
 * BusObjects residing within this Bus instance.
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
#ifndef _ALLJOYN_LOCALTRANSPORT_H
#define _ALLJOYN_LOCALTRANSPORT_H

#include <qcc/platform.h>

#include <map>

#include <qcc/String.h>
#include <qcc/GUID.h>
#include <qcc/Event.h>
#include <qcc/Mutex.h>
#include <qcc/StringMapKey.h>
#include <qcc/Timer.h>
#include <qcc/Util.h>

#include <alljoyn/BusObject.h>
#include <alljoyn/Message.h>
#include <alljoyn/MessageReceiver.h>
#include <alljoyn/ProxyBusObject.h>

#include <Status.h>

#include "BusEndpoint.h"
#include "CompressionRules.h"
#include "MethodTable.h"
#include "SignalTable.h"
#include "Transport.h"

#include <qcc/STLContainer.h>
//#if defined(__GNUCC__) || defined (QCC_OS_DARWIN)
//#include <ext/hash_map>
//namespace std {
//using namespace __gnu_cxx;
//}
//#else
//#include <hash_map>
//#endif

namespace ajn {

class BusAttachment;
class AllJoynPeerObj;


/**
 * %LocalEndpoint represents an endpoint connection to DBus/AllJoyn server
 */
class LocalEndpoint : public BusEndpoint, public qcc::AlarmListener, public MessageReceiver {

    friend class TCPTransport;
    friend class LocalTransport;
    friend class UnixTransport;

  public:

    /**
     * Constructor
     *
     * @param bus          Bus associated with endpoint.
     */
    LocalEndpoint(BusAttachment& bus);

    /**
     * Destructor.
     */
    ~LocalEndpoint();

    /**
     * Get the bus attachment for this endpoint
     */
    BusAttachment& GetBus() { return bus; }

    /**
     * Start endpoint.
     *
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise
     */
    QStatus Start();

    /**
     * Stop endpoint.
     *
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise
     */
    QStatus Stop();

    /**
     * Although LocalEndpoint is not a thread it contains threads that need to be joined.
     *
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise
     */
    QStatus Join();

    /**
     * Register a handler for method call reply
     *
     * @param receiver     The object that will receive the response
     * @param replyHandler The reply callback function
     * @param method       Interface/member of method call awaiting this reply.
     * @param serial       The serial number expected in the reply
     * @param secure       The value is true if the reply must be secure. Replies must be
     *                     secure if the method call was secure.
     * @param context      Opaque context pointer passed from method call to it's reply handler.
     * @param timeout      Timeout specified in milliseconds to wait for a reply to a method call.
     *                     The value 0 means use the implementation dependent default timeout.
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus RegisterReplyHandler(MessageReceiver* receiver,
                                 MessageReceiver::ReplyHandler replyHandler,
                                 const InterfaceDescription::Member& method,
                                 uint32_t serial,
                                 bool secure,
                                 void* context = NULL,
                                 uint32_t timeout = 0);

    /**
     * Un-Register a handler for method call reply
     *
     * @param serial       The serial number expected in the reply
     *
     * @return true if a handler matching the serial number is found and unregistered
     */
    bool UnregisterReplyHandler(uint32_t serial);

    /**
     * Extend the timeout on the handler for method call reply
     *
     * @param serial       The serial number expected in the reply
     * @param extension    The number of milliseconds to add to the timeout.
     *
     * @return
     *      - ER_OK if the timeout was succesfully extended.
     *      - An error status otherwise
     */
    QStatus ExtendReplyHandlerTimeout(uint32_t serial, uint32_t extension);

    /**
     * Register a signal handler.
     * Signals are forwarded to the signalHandler if sender, interface, member and path
     * qualifiers are ALL met.
     *
     * @param receiver       The object receiving the signal.
     * @param signalHandler  The signal handler method.
     * @param member         Interface/member of signal.
     * @param srcPath        The object path of the emitter of the signal or NULL for all paths
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus RegisterSignalHandler(MessageReceiver* receiver,
                                  MessageReceiver::SignalHandler signalHandler,
                                  const InterfaceDescription::Member* member,
                                  const char* srcPath);

    /**
     * Un-Register a signal handler.
     * Remove the signal handler that was registered with the given parameters.
     *
     * @param receiver       The object receiving the signal.
     * @param signalHandler  The signal handler method.
     * @param member         Interface/member of signal.
     * @param srcPath        The object path of the emitter of the signal or NULL for all paths
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus UnregisterSignalHandler(MessageReceiver* receiver,
                                    MessageReceiver::SignalHandler signalHandler,
                                    const InterfaceDescription::Member* member,
                                    const char* srcPath);

    /**
     * Un-Register all signal and reply handlers registered to the specified MessageReceiver.
     *
     * @param receiver   The object receiving the signal or waiting for the reply.
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus UnregisterAllHandlers(MessageReceiver* receiver);

    /**
     * Get the endpoint's unique name.
     *
     * @return   Unique name for endpoint.
     */
    const qcc::String& GetUniqueName() const { return uniqueName; }

    /**
     * Set the endpoint's unique name.
     *
     * @param uniqueName   Unique name for endpoint.
     */
    void SetUniqueName(const qcc::String& uniqueName) { this->uniqueName = uniqueName; }

    /**
     * Register a BusObject.
     *
     * @param obj       BusObject to be registered.
     * @return
     *      - ER_OK if successful.
     *      - ER_BUS_BAD_OBJ_PATH for a bad object path
     */
    QStatus RegisterBusObject(BusObject& obj);

    /**
     * Unregisters an object and its method and signal handlers.
     *
     * @param obj  Object to be unregistered.
     */
    void UnregisterBusObject(BusObject& obj);

    /**
     * Find a local object.
     *
     * @param objectPath   Object path.
     * @return
     *      - Pointer to matching object
     *      - NULL if none is found.
     */
    BusObject* FindLocalObject(const char* objectPath);

    /**
     * Notify local endpoint that a bus connection has been made.
     */
    void BusIsConnected();

    /**
     * Get the org.freedesktop.DBus remote object.
     *
     * @return org.freedesktop.DBus remote object
     */
    const ProxyBusObject& GetDBusProxyObj() const { return *dbusObj; }

    /**
     * Get the org.alljoyn.Bus remote object.
     *
     * @return org.alljoyn.Bus remote object
     */
    const ProxyBusObject& GetAllJoynProxyObj() const { return *alljoynObj; }

    /**
     * Get the org.alljoyn.Debug remote object.
     *
     * @return org.alljoyn.Debug remote object
     */
    const ProxyBusObject& GetAllJoynDebugObj();

    /**
     * Get the org.alljoyn.Bus.Peer local object.
     */
    AllJoynPeerObj* GetPeerObj() const { return peerObj; }

    /**
     * Get the guid for the local endpoint
     *
     * @return  The guid for the local endpoint
     */
    const qcc::GUID128& GetGuid() { return guid; }

    /**
     * Return the user id of the endpoint.
     *
     * @return  User ID number.
     */
    uint32_t GetUserId() const { return qcc::GetUid(); }

    /**
     * Return the group id of the endpoint.
     *
     * @return  Group ID number.
     */
    uint32_t GetGroupId() const { return qcc::GetGid(); }

    /**
     * Return the process id of the endpoint.
     *
     * @return  Process ID number.
     */
    uint32_t GetProcessId() const { return qcc::GetPid(); }

    /**
     * Indicates if the endpoint supports reporting UNIX style user, group, and process IDs.
     *
     * @return  'true' if UNIX IDs supported, 'false' if not supported.
     */
    bool SupportsUnixIDs() const { return true; }

    /**
     * Send message to this endpoint.
     *
     * @param msg        Message to deliver to this endpoint.
     *
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus PushMessage(Message& msg);

    /**
     * Indicate whether this endpoint is allowed to receive messages from remote devices.
     * LocalEndpoints always allow remote messages.
     *
     * @return true
     */
    bool AllowRemoteMessages() { return true; }

    /**
     * Get the method dispatcher
     */
    qcc::Timer& GetDispatcher() { return dispatcher; }

    /** Internal utility method needed (only) by PermissionMsg */
    void SendErrMessage(Message& message, qcc::String errStr, qcc::String description);

    /** Internal utility function needed (only) by PermissionMgr */
    void DoCallMethodHandler(const MethodTable::Entry* entry, Message& message)
    {
        entry->object->CallMethodHandler(entry->handler, entry->member, message, entry->context);
    }

  private:

    /** Signal/Method dispatcher */
    class Dispatcher : public qcc::Timer, public qcc::AlarmListener {
      public:
        Dispatcher(LocalEndpoint* ep);

        QStatus DispatchMessage(Message& msg);

        void AlarmTriggered(const qcc::Alarm& alarm, QStatus reason);

      private:
        LocalEndpoint* endpoint;
    };

    Dispatcher dispatcher;

    /**
     * PushMessage worker.
     */
    QStatus DoPushMessage(Message& msg);

    /**
     * Assignment operator is private - LocalEndpoints cannot be assigned.
     */
    LocalEndpoint& operator=(const LocalEndpoint& other);

    /**
     * Copy constructor is private - LocalEndpoints cannot be copied.
     */
    LocalEndpoint(const LocalEndpoint& other);

    /**
     * Type definition for a method call reply context
     */
    typedef struct {
        MessageReceiver* object;                     /**< The object to receive the reply */
        MessageReceiver::ReplyHandler handler;       /**< The receiving object's handler function */
        const InterfaceDescription::Member* method;  /**< The method that was called */
        bool secure;                                 /**< This wil be true if the method call was secure */
        void* context;                               /**< The calling object's context */
        qcc::Alarm alarm;                            /**< Alarm object for handling method call timeouts */
    } ReplyContext;

    /**
     * Equality function for matching object paths
     */
    struct PathEq { bool operator()(const char* p1, const char* p2) const { return (p1 == p2) || (strcmp(p1, p2) == 0); } };

    /**
     * Hash functor
     */
    struct Hash {
        inline size_t operator()(const char* s) const {
            return qcc::hash_string(s);
        }
    };

    /**
     * Registered LocalObjects
     */
    unordered_map<const char*, BusObject*, Hash, PathEq> localObjects;

    /**
     * Map from serial numbers for outstanding method calls to response handelers.
     */
    std::map<uint32_t, ReplyContext> replyMap;

#if defined(QCC_OS_ANDROID)
    /**
     * Type definition for a message pending for permission check.
     */
    typedef struct ChkPendingMsg {
        Message msg;                                /**< The message pending for permission check */
        const MethodTable::Entry* methodEntry;      /**< Method handler */
        std::list<SignalTable::Entry> signalCallList;    /**< List of signal handlers */
        qcc::String perms;                          /**< The required permissions */
        ChkPendingMsg(Message& msg, const MethodTable::Entry* methodEntry, const qcc::String& perms) : msg(msg), methodEntry(methodEntry), perms(perms) { }
        ChkPendingMsg(Message& msg, std::list<SignalTable::Entry>& signalCallList, const qcc::String& perms) : msg(msg), signalCallList(signalCallList), perms(perms) { }
    } ChkPendingMsg;

    /**
     * Type definition for a permission-checked method or signal call.
     */
    typedef struct PermCheckedEntry {
      public:
        const qcc::String sender;                        /**< The endpoint name that issues the call */
        const qcc::String sourcePath;                    /**< The object path of the call */
        const qcc::String iface;                         /**< The interface name of the call */
        const qcc::String signalName;                    /**< The method or signal name of the call */
        PermCheckedEntry(const qcc::String& sender, const qcc::String& sourcePath, const qcc::String& iface, const qcc::String& signalName) : sender(sender), sourcePath(sourcePath), iface(iface), signalName(signalName) { }
        bool operator<(const PermCheckedEntry& other) const {
            return (sender < other.sender) || ((sender == other.sender) && (sourcePath < other.sourcePath))
                   || ((sourcePath == other.sourcePath) && (iface < other.iface))
                   || ((iface == other.iface) && (signalName < other.signalName));
        }
    } PermCheckedEntry;

    /**
     * Type definition for a thread that does the permission verification on the message calls
     */
    class PermVerifyThread : public qcc::Thread {
        qcc::ThreadReturn STDCALL Run(void* arg);

      public:
        PermVerifyThread() : Thread("PermVerifyThread") { }
    };

    PermVerifyThread permVerifyThread;             /**< The permission verification thread */
    std::list<ChkPendingMsg> chkPendingMsgList;    /**< List of messages pending for permission check */
    std::map<PermCheckedEntry, bool> permCheckedCallMap;  /**< Map of a permission-checked method/signal call to the verification result */
    qcc::Mutex chkMsgListLock;                     /**< Mutex protecting the pending message list and the verification result cache map */
    qcc::Event wakeEvent;                          /**< Event to notify the permission verification thread of new pending messages */
#endif

    bool running;                      /**< Is the local endpoint up and running */
    int32_t refCount;                  /**< Reference count for local transport */
    MethodTable methodTable;           /**< Hash table of BusObject methods */
    SignalTable signalTable;           /**< Hash table of BusObject signal handlers */
    BusAttachment& bus;                /**< Message bus */
    qcc::Mutex objectsLock;            /**< Mutex protecting Objects hash table */
    qcc::Mutex replyMapLock;           /**< Mutex protecting replyMap */
    qcc::GUID128 guid;                    /**< GUID to uniquely identify a local endpoint */
    qcc::String uniqueName;            /**< Unique name for endpoint */

    std::vector<BusObject*> defaultObjects;  /**< Auto-generated, heap allocated parent objects */

    /**
     * Remote object for the standard DBus object and its interfaces
     */
    ProxyBusObject* dbusObj;

    /**
     * Remote object for the AllJoyn object and its interfaces
     */
    ProxyBusObject* alljoynObj;

    /**
     * Remote object for the AllJoyn debug object and its interfaces
     */
    ProxyBusObject* alljoynDebugObj;

    /**
     * The local AllJoyn peer object that implements AllJoyn endpoint functionality
     */
    AllJoynPeerObj* peerObj;

    /** Helper to diagnose misses in the methodTable */
    QStatus Diagnose(Message& msg);

    /** Special-cased message handler for the Peer interface */
    QStatus PeerInterface(Message& msg);

    /**
     * Process an incoming SIGNAL message
     */
    QStatus HandleSignal(Message& msg);

    /**
     * Process an incoming METHOD_CALL message
     */
    QStatus HandleMethodCall(Message& msg);

    /**
     * Process an incoming METHOD_REPLY or ERROR message
     */
    QStatus HandleMethodReply(Message& msg);

    /**
     *   Process a timeout on a METHOD_REPLY message
     */
    void AlarmTriggered(const qcc::Alarm& alarm, QStatus reason);

    /**
     * Inner utility method used bo RegisterBusObject.
     * Do not call this method externally.
     */
    QStatus DoRegisterBusObject(BusObject& object, BusObject* parent, bool isPlaceholder);

};

/**
 * %LocalTransport is a special type of Transport that is responsible
 * for all communication of all endpoints that terminate at registered
 * AllJoynObjects residing within this Bus instance.
 */
class LocalTransport : public Transport {
    friend class BusAttachment;

  public:

    /**
     *  Constructor
     *
     * @param bus     The bus
     *
     */
    LocalTransport(BusAttachment& bus) : localEndpoint(bus), isStoppedEvent() { isStoppedEvent.SetEvent(); }

    /**
     * Destructor
     */
    ~LocalTransport();

    /**
     * Normalize a transport specification.
     * Given a transport specification, convert it into a form which is guaranteed to have a one-to-one
     * relationship with a transport.
     *
     * @param inSpec    Input transport connect spec.
     * @param outSpec   Output transport connect spec.
     * @param argMap    Parsed parameter map.
     * @return ER_OK if successful.
     */
    QStatus NormalizeTransportSpec(const char* inSpec,
                                   qcc::String& outSpec,
                                   std::map<qcc::String, qcc::String>& argMap) const { return ER_NOT_IMPLEMENTED; }

    /**
     * Start the transport and associate it with a router.
     *
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus Start();

    /**
     * Stop the transport.
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus Stop(void);

    /**
     * Pend caller until transport is stopped.
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus Join(void);

    /**
     * Determine if this transport is running. Running means Start() has been called.
     *
     * @return  Returns true if the transport is running.
     */
    bool IsRunning();

    /**
     * Connect a local endpoint. (Not used for local transports)
     *
     * @param connectSpec     Unused parameter.
     * @param opts            Requested sessions opts.
     * @param newep           [OUT] Endpoint created as a result of successful connect.
     * @return  ER_NOT_IMPLEMENTED.
     */
    QStatus Connect(const char* connectSpec, const SessionOpts& opts, BusEndpoint** newep) { return ER_NOT_IMPLEMENTED; }

    /**
     * Disconnect a local endpoint. (Not used for local transports)
     *
     * @param args   Connect args used to create connection that is now being disconnected.
     * @return  ER_NOT_IMPLEMENTED.
     */
    QStatus Disconnect(const char* args) { return ER_NOT_IMPLEMENTED; }

    /**
     * Start listening for incomming connections  (Not used for local transports)
     *
     * @param listenSpec      Unused parameter.
     * @return  ER_NOT_IMPLEMENTED.
     */
    QStatus StartListen(const char* listenSpec) { return ER_NOT_IMPLEMENTED; }

    /**
     * Stop listening for incoming connections.  (Not used for local transports)
     *
     * @param listenSpec      Unused parameter.
     * @return  ER_NOT_IMPLEMENTED.
     */
    QStatus StopListen(const char* listenSpec) { return ER_NOT_IMPLEMENTED; }

    /**
     * Register a BusLocalObject.
     *
     * @param obj       BusLocalObject to be registered.
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus RegisterBusObject(BusObject& obj)
    {
        return localEndpoint.RegisterBusObject(obj);
    }

    /**
     * Unregisters an object and its method and signal handlers.
     *
     * @param object   Object to be deregistred.
     */
    void UnregisterBusObject(BusObject& object)
    {
        return localEndpoint.UnregisterBusObject(object);
    }

    /**
     * Return the singleton local endpoint
     *
     * @return  Local endpoint
     */
    LocalEndpoint& GetLocalEndpoint() { return localEndpoint; }

    /**
     * Set a listener for transport related events.
     * There can only be one listener set at a time. Setting a listener
     * implicitly removes any previously set listener.
     *
     * @param listener  Listener for transport related events.
     */
    void SetListener(TransportListener* listener) { };

    /**
     * Start discovering busses.
     */
    void EnableDiscovery(const char* namePrefix) { }

    /**
     * Stop discovering busses to connect to
     */
    void DisableDiscovery(const char* namePrefix) { }

    /**
     * Start advertising a well-known name with a given quality of service.
     *
     * @param advertiseName   Well-known name to add to list of advertised names.
     */
    QStatus EnableAdvertisement(const qcc::String& advertiseName) { return ER_FAIL; }

    /**
     * Stop advertising a well-known name with a given quality of service.
     *
     * @param advertiseName   Well-known name to remove from list of advertised names.
     * @param nameListEmpty   Indicates whether advertise name list is completely empty (safe to disable OTA advertising).
     */
    void DisableAdvertisement(const qcc::String& advertiseName, bool nameListEmpty) { }

    /**
     * Returns the name of this transport
     */
    const char* GetTransportName() const { return "local"; }

    /**
     * Get the transport mask for this transport
     *
     * @return the TransportMask for this transport.
     */
    TransportMask GetTransportMask() const { return TRANSPORT_LOCAL; }

    /**
     * Get a list of the possible listen specs for a given set of session options.
     * @param[IN]    opts      Session options.
     * @param[OUT]   busAddrs  Set of listen addresses. Always empty for this transport.
     * @return ER_OK if successful.
     */
    QStatus GetListenAddresses(const SessionOpts& opts, std::vector<qcc::String>& busAddrs) const { return ER_OK; }

    /**
     * Indicates whether this transport is used for client-to-bus or bus-to-bus connections.
     *
     * @return  Always returns false, the LocalTransport belongs to the local application.
     */
    bool IsBusToBus() const { return false; }

  private:

    /**
     * Singleton endpoint for LocalTransport
     */
    LocalEndpoint localEndpoint;

    /**
     * Set when transport is stopped
     */
    qcc::Event isStoppedEvent;
};

}

#endif
