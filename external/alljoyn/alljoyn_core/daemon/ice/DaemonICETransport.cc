/**
 * @file
 * DaemonICETransport is an implementation of Transport for daemon
 */

/******************************************************************************
 * Copyright 2012 Qualcomm Innovation Center, Inc.
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

#include <qcc/IPAddress.h>
#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/StringSink.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/TransportMask.h>
#include <alljoyn/Session.h>
#include <alljoyn/version.h>

#include "BusInternal.h"
#include "RemoteEndpoint.h"
#include "Router.h"
#include "DaemonConfig.h"
#include "DiscoveryManager.h"
#include "DaemonICETransport.h"
#include "ICEManager.h"
#include "RendezvousServerInterface.h"
#include "PacketEngine.h"
#include "STUNSocketStream.h"

#ifdef QCC_OS_GROUP_POSIX
#include "posix/ICEPacketStream.h"
#endif


#define QCC_MODULE "ICE"

using namespace std;
using namespace qcc;

const uint32_t ICE_LINK_TIMEOUT_PROBE_ATTEMPTS       = 1;
const uint32_t ICE_LINK_TIMEOUT_PROBE_RESPONSE_DELAY = 10;
const uint32_t ICE_LINK_TIMEOUT_MIN_LINK_TIMEOUT     = 40;
const uint32_t PACKET_ENGINE_ACCEPT_TIMEOUT_MS       = 5000;

namespace ajn {

/**
 * Name of transport used in transport specs.
 */
const char* DaemonICETransport::TransportName = "ice";

/*
 * An endpoint class to handle the details of authenticating a connection in a
 * way that avoids denial of service attacks.
 */
class DaemonICEEndpoint : public RemoteEndpoint {
  public:

    /**
     * There are three threads that can be running around in this data
     * structure.  An auth thread is run before the endpoint is started in order
     * to handle the security stuff that must be taken care of before messages
     * can start passing.  This enum reflects the states of the authentication
     * process and the state can be found in m_authState.  Once authentication
     * is complete, the auth thread must go away, but it must also be joined,
     * which is indicated by the AUTH_DONE state.  The other threads are the
     * endpoint RX and TX threads, which are dealt with by the EndpointState.
     */
    enum AuthState {
        AUTH_ILLEGAL = 0,
        AUTH_INITIALIZED,    /**< This endpoint structure has been allocated but no auth thread has been run */
        AUTH_AUTHENTICATING, /**< We have spun up an authentication thread and it has begun running our user function */
        AUTH_FAILED,         /**< The authentication has failed and the authentication thread is exiting immidiately */
        AUTH_SUCCEEDED,      /**< The auth process (Establish) has succeeded and the connection is ready to be started */
        AUTH_DONE,           /**< The auth thread has been successfully shut down and joined */
    };

    /**
     * There are three threads that can be running around in this data
     * structure.  Two threads, and RX thread and a TX thread are used to pump
     * messages through an endpoint.  These threads cannot be run until the
     * authentication process has completed.  This enum reflects the states of
     * the endpoint RX and TX threads and can be found in m_epState.  The auth
     * thread is dealt with by the AuthState enum above.  These threads must be
     * joined when they exit, which is indicated by the EP_DONE state.
     */
    enum EndpointState {
        EP_ILLEGAL = 0,
        EP_INITIALIZED,      /**< This endpoint structure has been allocated but not used */
        EP_FAILED,           /**< Starting the RX and TX threads has failed and this endpoint is not usable */
        EP_STARTED,          /**< The RX and TX threads have been started (they work as a unit) */
        EP_STOPPING,         /**< The RX and TX threads are stopping (have run ThreadExit) but have not been joined */
        EP_DONE              /**< The RX and TX threads have been shut down and joined */
    };

    /**
     * Connections can either be created as a result of a Connect() or an Accept().
     * If a connection happens as a result of a connect it is the active side of
     * a connection.  If a connection happens because of an Accept() it is the
     * passive side of a connection.  This is important because of reference
     * counting of bus-to-bus endpoints.
     */
    enum SideState {
        SIDE_ILLEGAL = 0,
        SIDE_INITIALIZED,    /**< This endpoint structure has been allocated but don't know if active or passive yet */
        SIDE_ACTIVE,         /**< This endpoint is the active side of a connection */
        SIDE_PASSIVE         /**< This endpoint is the passive side of a connection */
    };

    DaemonICEEndpoint(DaemonICETransport* transport,
                      BusAttachment& bus,
                      bool incoming,
                      const String connectSpec,
                      ICEPacketStream& icePktStream) :
        RemoteEndpoint(bus, incoming, connectSpec, &m_stream, "ice"),
        m_transport(transport),
        m_sideState(SIDE_INITIALIZED),
        m_authState(AUTH_INITIALIZED),
        m_epState(EP_INITIALIZED),
        m_tStart(Timespec(0)),
        m_authThread(this, transport),
        m_icePktStream(icePktStream),
        m_stream(),
        m_wasSuddenDisconnect(!incoming),
        m_isConnected(false)
    {
    }

    ~DaemonICEEndpoint()
    {
        if (m_isConnected) {
            /* Attempt graceful disconnect with other side if still connected */
            m_transport->m_packetEngine.Disconnect(m_stream);
        }
    }

    void SetStartTime(Timespec tStart) { m_tStart = tStart; }
    Timespec GetStartTime(void) { return m_tStart; }
    QStatus Authenticate(void);
    void AuthStop(void);
    void AuthJoin(void);

    SideState GetSideState(void) { return m_sideState; }

    void SetActive(void)
    {
        m_sideState = SIDE_ACTIVE;
    }

    void SetPassive(void)
    {
        m_sideState = SIDE_PASSIVE;
    }

    AuthState GetAuthState(void) { return m_authState; }

    void SetAuthDone(void)
    {
        m_authState = AUTH_DONE;
    }

    void SetAuthenticating(void)
    {
        m_authState = AUTH_AUTHENTICATING;
    }

    EndpointState GetEpState(void) { return m_epState; }

    void SetEpFailed(void)
    {
        m_epState = EP_FAILED;
    }

    void SetEpStarted(void)
    {
        m_epState = EP_STARTED;
    }

    void SetEpStopping(void)
    {
        assert(m_epState == EP_STARTED);
        m_epState = EP_STOPPING;
    }

    void SetEpDone(void)
    {
        assert(m_epState == EP_FAILED || m_epState == EP_STOPPING);
        m_epState = EP_DONE;
    }

    void SetStream(const PacketEngineStream& stream) { m_stream = stream; RemoteEndpoint::SetStream(&m_stream); }

    bool IsSuddenDisconnect() { return m_wasSuddenDisconnect; }
    void SetSuddenDisconnect(bool val) { m_wasSuddenDisconnect = val; }

    QStatus SetLinkTimeout(uint32_t& linkTimeout)
    {
        QStatus status = ER_OK;
        if (linkTimeout > 0) {
            uint32_t to = max(linkTimeout, ICE_LINK_TIMEOUT_MIN_LINK_TIMEOUT);
            to -= ICE_LINK_TIMEOUT_PROBE_RESPONSE_DELAY * ICE_LINK_TIMEOUT_PROBE_ATTEMPTS;
            status = RemoteEndpoint::SetLinkTimeout(to, ICE_LINK_TIMEOUT_PROBE_RESPONSE_DELAY, ICE_LINK_TIMEOUT_PROBE_ATTEMPTS);
            if ((status == ER_OK) && (to > 0)) {
                linkTimeout = to + ICE_LINK_TIMEOUT_PROBE_RESPONSE_DELAY * ICE_LINK_TIMEOUT_PROBE_ATTEMPTS;
            }

        } else {
            RemoteEndpoint::SetLinkTimeout(0, 0, 0);
        }
        return status;
    }

    /*
     * Return true if the auth thread is STARTED, RUNNING or STOPPING.  A true
     * response means the authentication thread is in a state that indicates
     * a possibility it might touch the endpoint data structure.  This means
     * don't delete the endpoint if this method returns true.  This method
     * indicates nothing about endpoint rx and tx thread state.
     */
    bool IsAuthThreadRunning(void)
    {
        return m_authThread.IsRunning();
    }

  private:

    friend class DaemonICETransport;

    QStatus PacketEngineConnect(const IPAddress& addr, uint16_t port);

    class AuthThread : public qcc::Thread {
      public:
        AuthThread(DaemonICEEndpoint* conn, DaemonICETransport* trans) : Thread("auth"), m_transport(trans) { }
      private:
        virtual qcc::ThreadReturn STDCALL Run(void* arg);

        DaemonICETransport* m_transport;
    };

    DaemonICETransport* m_transport;    /**< The server holding the connection */
    volatile SideState m_sideState;     /**< Is this an active or passive connection */
    volatile AuthState m_authState;     /**< The state of the endpoint authentication process */
    volatile EndpointState m_epState;   /**< The state of the endpoint authentication process */
    Timespec m_tStart;                  /**< Timestamp indicating when the authentication process started */
    AuthThread m_authThread;            /**< Thread used to do blocking calls during startup */
    ICEPacketStream& m_icePktStream;    /**< ICE packet stream associated with the packet engine stream m_stream */
    PacketEngineStream m_stream;        /*< Stream used by authentication code */
    bool m_wasSuddenDisconnect;         /**< If true, assumption is that any disconnect is unexpected due to lower level error */
    bool m_isConnected;                 /**< true iff endpoint is connected to a remote side */
    qcc::Event* m_connectWaitEvent;     /**< Event used to wait for connects to complete */
    QStatus m_packetEngineReturnStatus; /**< Status returned from PacketEngine */
};

QStatus DaemonICEEndpoint::Authenticate(void)
{
    QCC_DbgTrace(("DaemonICEEndpoint::Authenticate()"));
    /*
     * Start the authentication thread.
     */
    QStatus status = m_authThread.Start(this);
    if (status != ER_OK) {
        m_authState = AUTH_FAILED;
    }
    return status;
}

void DaemonICEEndpoint::AuthStop(void)
{
    QCC_DbgTrace(("DaemonICEEndpoint::AuthStop()"));

    /*
     * Ask the auth thread to stop executing.  The only ways out of the thread
     * run function will set the state to either AUTH_SUCCEEDED or AUTH_FAILED.
     * There is a very small chance that we will send a stop to the thread after
     * it has successfully authenticated, but we expect that this will result in
     * an AUTH_FAILED state for the vast majority of cases.  In this case, we
     * notice that the thread failed the next time through the main server run
     * loop, join the thread via AuthJoin below and delete the endpoint.  Note
     * that this is a lazy cleanup of the endpoint.
     */
    m_authThread.Stop();
}

void DaemonICEEndpoint::AuthJoin(void)
{
    QCC_DbgTrace(("DaemonICEEndpoint::AuthJoin()"));

    /*
     * Join the auth thread to stop executing.  All threads must be joined in
     * order to communicate their return status.  The auth thread is no exception.
     * This is done in a lazy fashion from the main server accept loop, where we
     * cleanup every time through the loop.
     */
    m_authThread.Join();
}

void* DaemonICEEndpoint::AuthThread::Run(void* arg)
{
    QCC_DbgTrace(("DaemonICEEndpoint::AuthThread::Run()"));

    DaemonICEEndpoint* conn = reinterpret_cast<DaemonICEEndpoint*>(arg);

    conn->m_authState = AUTH_AUTHENTICATING;

    /*
     * We're running an authentication process here and we are cooperating with
     * the main server thread.  This thread is running in an object that is
     * allocated on the heap, and the server is managing these objects so we
     * need to coordinate getting all of this cleaned up.
     *
     * There is a state variable that only we write.  The server thread only
     * reads this variable, so there are no data sharing issues.  If there is an
     * authentication failure, this thread sets that state variable to
     * AUTH_FAILED and then exits.  The server holds a list of currently
     * authenticating connections and will look for AUTH_FAILED connections when
     * it runs its Accept loop.  If it finds one, it will AuthJoin() this
     * thread.  Since we set AUTH_FAILED immediately before exiting, there will
     * be no problem having the server block waiting for the Join() to complete.
     * We fail authentication here and let the server clean up after us, lazily.
     *
     * If we succeed in the authentication process, we set the state variable
     * to AUTH_SUCEEDED and then call back into the server telling it that we are
     * up and running.  It needs to take us off of the list of authenticating
     * connections and put us on the list of running connections.  This thread
     * will quickly go away and will be replaced by the RX and TX threads of
     * the running RemoteEndpoint.
     *
     * If we are running an authentication process, we are probably ultimately
     * blocked on a socket.  We expect that if the server is asked to shut
     * down, it will run through its list of authenticating connections and
     * AuthStop() each one.  That will cause a thread Stop() which should unblock
     * all of the reads and return an error which will eventually pop out here
     * with an authentication failure.
     *
     * Finally, if the server decides we've spent too much time here and we are
     * actually a denial of service attack, it can close us down by doing an
     * AuthStop() on the authenticating endpoint.  This will do a thread Stop()
     * on the auth thread of the endpoint which will pop out of here as an
     * authentication failure as well.  The only ways out of this method must be
     * with state = AUTH_FAILED or state = AUTH_SUCCEEDED.
     */
    uint8_t byte = 'x';
    size_t nbytes = 0;

    /*
     * Eat the first byte of the stream.  This is required to be zero by the
     * DBus protocol.  It is used in the Unix socket implementation to carry
     * out-of-band capabilities, but is discarded here.  We do this here since
     * it involves a read that can block.
     */
    QStatus status = conn->m_stream.PullBytes(&byte, 1, nbytes);
    if ((status != ER_OK) || (nbytes != 1) || (byte != 0)) {
        QCC_LogError(status, ("Failed to read first byte from stream (byte=%x, nbytes=%d)", (int) byte, (int) nbytes));

        /*
         * Management of the resources used by the authentication thread is done
         * in one place, by the server Accept loop.  The authentication thread
         * writes its state into the connection and the server Accept loop reads
         * this state.  As soon as we set this state to AUTH_FAILED, we are
         * telling the Accept loop that we are done with the conn data
         * structure.  That thread is then free to do anything it wants with the
         * connection, including deleting it, so we are not allowed to touch
         * conn after setting this state.
         *
         * In addition to releasing responsibility for the conn data structure,
         * when we set the state to AUTH_SUCCEEDED we are telling the server
         * accept loop that we are exiting now and so it can Join() on us (the
         * authentication thread) without being worried about blocking since the
         * next thing we do is exit.
         */
        conn->m_authState = AUTH_FAILED;
        conn->m_transport->wakeDaemonICETransportRun.SetEvent();
        return (void*)ER_FAIL;
    }

    /* Initialized the features for this endpoint */
    conn->GetFeatures().isBusToBus = false;
    conn->GetFeatures().handlePassing = false;

    /* Run the actual connection authentication code. */
    qcc::String authName;
    qcc::String redirection;
    status = conn->Establish("ANONYMOUS", authName, redirection);
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to establish Daemon ICE endpoint"));

        /*
         * Management of the resources used by the authentication thread is done
         * in one place, by the server Accept loop.  The authentication thread
         * writes its state into the connection and the server Accept loop reads
         * this state.  As soon as we set this state to AUTH_FAILED, we are
         * telling the Accept loop that we are done with the conn data
         * structure.  That thread is then free to do anything it wants with the
         * connection, including deleting it, so we are not allowed to touch
         * conn after setting this state.
         *
         * In addition to releasing responsibility for the conn data structure,
         * when we set the state to AUTH_SUCCEEDED we are telling the server
         * accept loop that we are exiting now and so it can Join() on us (the
         * authentication thread) without being worried about blocking since the
         * next thing we do is exit.
         */
        conn->m_authState = AUTH_FAILED;
        conn->m_transport->wakeDaemonICETransportRun.SetEvent();
        return (void*)status;
    }

    /*
     * Tell the transport that the authentication has succeeded and that it can
     * now bring the connection up.
     */
    conn->m_transport->Authenticated(conn);

    QCC_DbgTrace(("DaemonICEEndpoint::AuthThread::Run(): Returning"));

    /*
     * We are now done with the authentication process.  We have succeeded doing
     * the authentication and we may or may not have succeeded in starting the
     * endpoint TX and RX threads depending on what happened down in
     * Authenticated().  What concerns us here is that we are done with this
     * thread (the authentication thread) and we are about to exit.  Before
     * exiting, we must tell server accept loop that we are done with this data
     * structure.  As soon as we set this state to AUTH_SUCCEEDED that thread is
     * then free to do anything it wants with the connection, including deleting
     * it, so we are not allowed to touch conn after setting this state.
     *
     * In addition to releasing responsibility for the conn data structure, when
     * we set the state to AUTH_SUCCEEDED we are telling the server accept loop
     * that we are exiting now and so it can Join() the authentication thread
     * without being worried about blocking since the next thing we do is exit.
     */
    conn->m_authState = AUTH_SUCCEEDED;

    return (void*)status;
}

QStatus DaemonICEEndpoint::PacketEngineConnect(const IPAddress& addr, uint16_t port)
{
    QCC_DbgTrace(("DaemonICEEndpoint::PacketEngineConnect()"));

    QStatus status;

    PacketDest packDest = ICEPacketStream::GetPacketDest(addr, port);

    /* Connect to dest */
    Event waitEvt;
    m_connectWaitEvent = &waitEvt;
    status = m_transport->m_packetEngine.Connect(packDest, m_icePktStream, *m_transport, this);
    if (status != ER_OK) {
        m_authState = AUTH_FAILED;
        QCC_LogError(status, ("DaemonICEEndpoint::AuthThread::Run(): Failed PacketEngine::Connect()"));
        return status;
    }

    status = Event::Wait(waitEvt);
    if (status != ER_OK) {
        m_authState = AUTH_FAILED;
        QCC_LogError(status, ("DaemonICEEndpoint::AuthThread::Run(): Timed-out or failed wait on m_pktEngineConnectEvent"));
        return status;
    }

    if (m_packetEngineReturnStatus != ER_OK) {
        status = m_packetEngineReturnStatus;
        m_authState = AUTH_FAILED;
        QCC_LogError(status, ("DaemonICEEndpoint::AuthThread::Run(): PacketEngineConnectCB returned a failure"));
        return status;
    }

    /*
     * We now have a UDP connection established, but DBus (the wire
     * protocol which we are using) requires that every connection,
     * irrespective of transport, start with a single zero byte.  This
     * is so that the Unix-domain socket transport used by DBus can pass
     * SCM_RIGHTS out-of-band when that byte is sent.
     */
    char sendData = '\0';
    size_t sent;
    status = m_stream.PushBytes((void*)&sendData, 1, sent);
    if ((ER_OK != status) || (sent != 1)) {
        status = ER_FAIL;
        QCC_LogError(status, ("DaemonICEEndpoint::PacketEngineConnect(): Sending of nul byte failed"));
    }

    return status;
}


DaemonICETransport::DaemonICETransport(BusAttachment& bus) :
    Thread("DaemonICETransport"),
    m_bus(bus),
    m_dm(0),
    m_iceManager(),
    m_stopping(false),
    m_listener(0),
    m_packetEngine("ice_packet_engine"),
    m_iceCallback(m_listener, this)
{
    /*
     * We know we are daemon code, so we'd better be running with a daemon
     * router.  This is assumed elsewhere.
     */
    assert(m_bus.GetInternal().GetRouter().IsDaemon());

    /* Start the daemonICETransportTimer which is used to handle all the alarms */
    daemonICETransportTimer.Start();
}

DaemonICETransport::~DaemonICETransport()
{
    QCC_DbgTrace(("DaemonICETransport::~DaemonICETransport()"));
    /* Wait for any outstanding AllocateICESessionThreads */
    allocateICESessionThreadsLock.Lock(MUTEX_CONTEXT);
    vector<AllocateICESessionThread*>::iterator it = allocateICESessionThreads.begin();
    while (it != allocateICESessionThreads.end()) {
        (*it)->Stop();
        ++it;
    }
    while (!allocateICESessionThreads.empty()) {
        allocateICESessionThreadsLock.Unlock(MUTEX_CONTEXT);
        qcc::Sleep(50);
        allocateICESessionThreadsLock.Lock(MUTEX_CONTEXT);
    }
    allocateICESessionThreadsLock.Unlock(MUTEX_CONTEXT);

    /* Make sure all threads are safely gone */
    Stop();
    Join();

    /* Deregister  packetStreams from packetEngine before packetStreams are destroyed */
    pktStreamMapLock.Lock();
    map<String, pair<ICEPacketStream, int32_t> >::iterator pit = pktStreamMap.begin();
    while (pit != pktStreamMap.end()) {
        m_packetEngine.RemovePacketStream(pit->second.first);
        ++pit;
    }
    pktStreamMapLock.Unlock();

    delete m_dm;
    m_dm = NULL;
}

void DaemonICETransport::Authenticated(DaemonICEEndpoint* conn)
{
    QCC_DbgTrace(("DaemonICETransport::Authenticated()"));

    /*
     * If Authenticated() is being called, it is as a result of the
     * authentication thread telling us that it has succeeded.  What we need to
     * do here is to try and Start() the endpoint which will spin up its TX and
     * RX threads and register the endpoint with the daemon router.  As soon as
     * we call Start(), we are transferring responsibility for error reporting
     * through endpoint ThreadExit() function.  This will percolate out our
     * EndpointExit function.  It will expect to find <conn> on the endpoint
     * list so we move it from the authList to the endpointList before calling
     * Start.
     */
    m_endpointListLock.Lock(MUTEX_CONTEXT);

    set<DaemonICEEndpoint*>::iterator i = m_authList.find(conn);
    assert(i != m_authList.end() && "DaemonICETransport::Authenticated(): Conn not on m_authList");

    /*
     * Note here that we have not yet marked the authState as AUTH_SUCCEEDED so
     * this is a point in time where the authState can be AUTH_AUTHENTICATING
     * and the endpoint can be on the endpointList and not the authList.
     */
    m_authList.erase(i);
    m_endpointList.insert(conn);

    m_endpointListLock.Unlock(MUTEX_CONTEXT);

    conn->SetListener(this);
    QStatus status = conn->Start();
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::Authenticated(): Failed to start Daemon ICE endpoint"));
        /*
         * We were unable to start up the endpoint for some reason.  As soon as
         * we set this state to EP_FAILED, we are telling the server accept loop
         * that we tried to start the connection but it failed.  This connection
         * is now useless and is a candidate for cleanup.  This will be
         * prevented until authState changes from AUTH_AUTHENTICATING to
         * AUTH_SUCCEEDED.  This may be a little confusing, but the
         * authentication process has really succeeded but the endpoint start
         * has failed.  The combination of status in this case will be
         * AUTH_SUCCEEDED and EP_FAILED.  Once this state is detected by the
         * server accept loop it is then free to do anything it wants with the
         * connection, including deleting it.
         */
        conn->SetEpFailed();
    } else {
        /*
         * We were able to successfully start up the endpoint.  As soon as we
         * set this state to EP_STARTED, we are telling the server accept loop
         * that there are TX and RX threads wandering around in this endpoint.
         */
        conn->SetEpStarted();
    }
}

QStatus DaemonICETransport::Start()
{
    m_stopping = false;

    QStatus status;

    /*
     * A true response from IsRunning tells us that the DaemonICETransport Run thread is
     * STARTED, RUNNING or STOPPING.
     *
     * When a thread is created it is in state INITIAL.  When an actual tread is
     * spun up as a result of Start(), it becomes STARTED.  Just before the
     * user's Run method is called, the thread becomes RUNNING.  If the Run
     * method exits, the thread becomes STOPPING.  When the thread is Join()ed
     * it becomes DEAD.
     *
     * IsRunning means that someone has called Thread::Start() and the process
     * has progressed enough that the thread has begun to execute.  If we get
     * multiple Start() calls calls on multiple threads, this test may fail to
     * detect multiple starts in a failsafe way and we may end up with multiple
     * server accept threads running.  We assume that since Start() requests
     * come in from our containing transport list it will not allow concurrent
     * start requests.
     */
    if (IsRunning()) {
        QCC_LogError(ER_BUS_BUS_ALREADY_STARTED, ("DaemonICETransport::Start(): Already started"));
        return ER_BUS_BUS_ALREADY_STARTED;
    }

    /*
     * In order to pass the IsRunning() gate above, there must be no DaemonICETransport Run thread running.
     * Running includes a thread that has been asked to
     * stop but has not been Join()ed yet.  So we know that there is no thread
     * and that either a Start() has never happened, or a Start() followed by a
     * Stop() and a Join() has happened.  Since Join() does a Thread::Join and
     * then deletes the name service, it is possible that a Join() done on one
     * thread is done enough to pass the gate above, but has not yet finished
     * deleting the name service instance when a Start() comes in on another
     * thread.  Because of this (rare and unusual) possibility we also check the
     * name service instance and return an error if we find it non-zero.  If the
     * name service is NULL, the Stop() and Join() is totally complete and we
     * can safely proceed.
     */
    if (m_dm != NULL) {
        QCC_LogError(ER_BUS_BUS_ALREADY_STARTED, ("DaemonICETransport::Start(): Discovery Manager has already started"));
        return ER_BUS_BUS_ALREADY_STARTED;
    }

    /* Start the PacketEngine */
    status = m_packetEngine.Start(MAX_ICE_MTU);
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::Start(): PacketEngine::Start failed"));
        return status;
    }

    /*
     * Start up an instance of the lightweight Discovery Manager and tell it what
     * GUID we think we are.
     */

    m_dm = new DiscoveryManager(m_bus);

    assert(m_dm);
    m_stopping = false;

    /*
     * Get the guid from the bus attachment which will act as the globally unique
     * ID of the daemon.
     */
    String guidStr = m_bus.GetInternal().GetGlobalGUID().ToString();

    status = m_dm->Init(guidStr);
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::Start(): Error starting Discovery Manager"));
        return status;
    }

    /*
     * Tell the Discovery Manager to call us back on our ICECallback method when
     * we hear about a new well-known bus name.
     */
    m_dm->SetCallback(
        new CallbackImpl<ICECallback, void, ajn::DiscoveryManager::CallbackType, const String&, const vector<String>*, uint8_t>
            (&m_iceCallback, &ICECallback::ICE));

    /*
     * Start the DaemonICETransport Run loop through the thread base class
     */
    return Thread::Start();
}

QStatus DaemonICETransport::Stop(void)
{
    QCC_DbgTrace(("DaemonICETransport::Stop()"));

    /*
     * It is legal to call Stop() more than once, so it must be possible to
     * call Stop() on a stopped transport.
     */
    m_stopping = true;

    /*
     * Tell the Discovery Manager to stop calling us back if it's there (we may get
     * called more than once in the chain of destruction) so the pointer is not
     * required to be non-NULL.
     */
    if (m_dm) {
        m_dm->SetCallback(NULL);
    }

    /*
     * Tell the DaemonICETransport Run thread to shut down through the thread
     * base class.
     */
    QStatus status = Thread::Stop();
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::Stop(): Failed to Stop() DaemonICETransport Run thread"));
    }

    m_endpointListLock.Lock(MUTEX_CONTEXT);

    /*
     * Ask any authenticating endpoints to shut down and exit their threads.  By its
     * presence on the m_authList, we know that the endpoint is authenticating and
     * the authentication thread has responsibility for dealing with the endpoint
     * data structure.  We call Stop() to stop that thread from running.  The
     * endpoint Rx and Tx threads will not be running yet.
     */
    for (set<DaemonICEEndpoint*>::iterator i = m_authList.begin(); i != m_authList.end(); ++i) {
        (*i)->AuthStop();
    }

    /*
     * Ask any running endpoints to shut down and exit their threads.  By its
     * presence on the m_endpointList, we know that authentication is compete and
     * the Rx and Tx threads have responsibility for dealing with the endpoint
     * data structure.  We call Stop() to stop those threads from running.  Since
     * the connnection is on the m_endpointList, we know that the authentication
     * thread has handed off responsibility.
     */
    for (set<DaemonICEEndpoint*>::iterator i = m_endpointList.begin(); i != m_endpointList.end(); ++i) {
        (*i)->Stop();
    }

    m_endpointListLock.Unlock(MUTEX_CONTEXT);

    /*
     * The use model for DaemonICETransport is that it works like a thread.
     * There is a call to Start() that spins up the DaemonICETransport Run loop in order
     * to get it running.  When someone wants to tear down the transport, they
     * call Stop() which requests the transport to stop.  This is followed by
     * Join() which waits for all of the threads to actually stop.
     *
     * The DiscoveryManager should play by those rules as well.  We allocate and
     * initialize it in Start(), which will spin up the main thread there.
     * We need to Stop() the DiscoveryManager here and Join its thread in
     * DaemonICETransport::Join().  If someone just deletes the transport
     * there is an implied Stop() and Join() so it behaves correctly.
     */
    if (m_dm) {
        m_dm->Stop();
    }

    /* Stop the timer */
    daemonICETransportTimer.Stop();

    return ER_OK;
}

QStatus DaemonICETransport::Join(void)
{
    QCC_DbgTrace(("DaemonICETransport::Join()"));

    /*
     * It is legal to call Join() more than once, so it must be possible to
     * call Join() on a joined transport.
     *
     * First, wait for the DaemonICETransport loop thread to exit.
     */
    QStatus status = Thread::Join();
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::Join(): Failed to Join() DaemonICETransport thread"));
        return status;
    }

    /*
     * A required call to Stop() that needs to happen before this Join will ask
     * all of the endpoints to stop; and will also cause any authenticating
     * endpoints to stop.  We still need to wait here until all of the threads
     * running in those endpoints actually stop running.
     *
     * Since Stop() is a request to stop, and this is what has ultimately been
     * done to both authentication threads and Rx and Tx threads, it is possible
     * that a thread is actually running after the call to Stop().  If that
     * thead happens to be an authenticating endpoint, it is possible that an
     * authentication actually completes after Stop() is called.  This will move
     * a connection from the m_authList to the m_endpointList, so we need to
     * make sure we wait for all of the connections on the m_authList to go away
     * before we look for the connections on the m_endpointlist.
     */
    m_endpointListLock.Lock(MUTEX_CONTEXT);

    /*
     * Any authenticating endpoints have been asked to shut down and exit their
     * authentication threads in a previously required Stop().  We need to
     * Join() all of these auth threads here.
     */
    set<DaemonICEEndpoint*>::iterator it = m_authList.begin();
    while (it != m_authList.end()) {
        DaemonICEEndpoint* ep = *it;
        m_authList.erase(it);
        m_endpointListLock.Unlock(MUTEX_CONTEXT);
        ep->AuthJoin();
        delete ep;
        m_endpointListLock.Lock(MUTEX_CONTEXT);
        it = m_authList.upper_bound(ep);
    }

    /*
     * Any running endpoints have been asked it their threads in a previously
     * required Stop().  We need to Join() all of these threads here.  This
     * Join() will wait on the endpoint rx and tx threads to exit as opposed to
     * the joining of the auth thread we did above.
     */
    it = m_endpointList.begin();
    while (it != m_endpointList.end()) {
        DaemonICEEndpoint* ep = *it;
        m_endpointList.erase(it);
        m_endpointListLock.Unlock(MUTEX_CONTEXT);
        ep->Join();
        delete ep;
        m_endpointListLock.Lock(MUTEX_CONTEXT);
        it = m_endpointList.upper_bound(ep);
    }

    m_endpointListLock.Unlock(MUTEX_CONTEXT);

    /* Join the timer */
    daemonICETransportTimer.Join();

    /* Join the discovery manager */
    if (m_dm) {
        m_dm->Join();
    }

    m_stopping = false;

    return ER_OK;
}

QStatus DaemonICETransport::GetListenAddresses(const SessionOpts& opts, std::vector<String>& busAddrs) const
{
    QCC_DbgTrace(("DaemonICETransport::GetListenAddresses()"));

    if (opts.transports & GetTransportMask()) {

        /* For the ICE transport, peerAddr is the alias of GUID */
        qcc::String peerAddr = m_dm->GetPeerAddr();

        if (peerAddr.empty()) {
            QCC_LogError(ER_FAIL, ("DaemonICETransport::GetListenAddresses(): PeerAddr is empty"));
            return ER_FAIL;
        } else {
            qcc::String listenAddr = qcc::String("ice:guid=") + peerAddr;
            if (!listenAddr.empty()) {
                busAddrs.push_back(listenAddr);
            }
        }
    }
    return ER_OK;
}

void DaemonICETransport::PacketEngineConnectCB(PacketEngine& engine,
                                               QStatus status,
                                               const PacketEngineStream* stream,
                                               const PacketDest& dest,
                                               void* context)
{
    QCC_DbgTrace(("DaemonICETransport::PacketEngineConnectCB(status=%s, context=%p)", QCC_StatusText(status), context));

    DaemonICEEndpoint* ep = static_cast<DaemonICEEndpoint*>(context);
    assert(ep->m_connectWaitEvent);

    if (status == ER_OK) {
        ep->SetStream(*stream);
        ep->m_isConnected = true;
    } else {
        QCC_LogError(status, ("%s(ep=%p) Connect to %s failed\n", __FUNCTION__, ep, m_packetEngine.ToString(ep->m_icePktStream, dest).c_str()));
    }

    ep->m_packetEngineReturnStatus = status;
    ep->m_connectWaitEvent->SetEvent();
}

bool DaemonICETransport::PacketEngineAcceptCB(PacketEngine& engine, const PacketEngineStream& stream, const PacketDest& dest)
{
    QCC_DbgTrace(("%s(stream=%p)", __FUNCTION__, &stream));

    QStatus status = ER_FAIL;
    ICEPacketStream* icePktStream = static_cast<ICEPacketStream*>(engine.GetPacketStream(stream));

    /* Increment the ref count on this pktStream */
    if (icePktStream) {
        /* Make sure icePacketStream is still valid */
        status = AcquireICEPacketStreamByPointer(icePktStream);
        if (status == ER_OK) {
            /*
             * If there is an accept timeout alarm pending, then we don't want to increment the ref count
             * because the first ref of a new packet stream comes from AllocateICESessionThread
             */
            if (daemonICETransportTimer.RemoveAlarm(icePktStream->GetTimeoutAlarm())) {
                ReleaseICEPacketStream(*icePktStream);
            }
        } else {
            /* icePacketStream is no longer valid */
            icePktStream = NULL;
        }
    }
    if (icePktStream) {
        /* Create endpoint */
        DaemonICEEndpoint* conn = new DaemonICEEndpoint(this, m_bus, true, "", *icePktStream);
        conn->SetStream(stream);
        conn->SetPassive();
        Timespec tNow;
        GetTimeNow(&tNow);
        conn->SetStartTime(tNow);

        /*
         * By putting the connection on the m_authList, we are
         * transferring responsibility for the connection to the
         * Authentication thread.  Therefore, we must check that the
         * thread actually started running to ensure the handoff
         * worked.  If it didn't we need to deal with the connection
         * here.  Since there are no threads running we can just
         * pitch the connection.
         */
        m_endpointListLock.Lock(MUTEX_CONTEXT);
        m_authList.insert(conn);
        m_endpointListLock.Unlock(MUTEX_CONTEXT);

        status = conn->Authenticate();
        if (status != ER_OK) {
            m_endpointListLock.Lock(MUTEX_CONTEXT);
            set<DaemonICEEndpoint*>::iterator eit = m_authList.find(conn);
            if (eit != m_authList.end()) {
                m_authList.erase(eit);
                m_endpointListLock.Unlock(MUTEX_CONTEXT);
                delete conn;
            } else {
                m_endpointListLock.Unlock(MUTEX_CONTEXT);
            }
        }
    }

    bool ret = (status == ER_OK);
    if ((status != ER_OK) && icePktStream) {
        ReleaseICEPacketStream(*icePktStream);
    }
    QCC_DbgPrintf(("%s connect attempt from %s", ret ? "Accepting" : "Rejecting", icePktStream ? engine.ToString(*icePktStream, dest).c_str() : "<unknown>"));
    return ret;
}

void DaemonICETransport::PacketEngineDisconnectCB(PacketEngine& engine, const PacketEngineStream& stream, const PacketDest& dest)
{
    ICEPacketStream* icePktStream = static_cast<ICEPacketStream*>(engine.GetPacketStream(stream));

    QCC_DbgTrace(("%s(this=%p, stream=%p, dest=%s)", __FUNCTION__, this, &stream, icePktStream ? engine.ToString(*icePktStream, dest).c_str() : "<unknown>"));

    /* Find endpoint that uses stream and stop it */
    m_endpointListLock.Lock(MUTEX_CONTEXT);
    bool foundEp = false;
    set<DaemonICEEndpoint*>::iterator it = m_endpointList.begin();
    while (it != m_endpointList.end()) {
        if ((*it)->m_stream == stream) {
            (*it)->Stop();
            foundEp = true;
            break;
        }
        ++it;
    }

    /* Endpoint might also be on auth list */
    if (!foundEp) {
        it = m_authList.begin();
        while (it != m_authList.end()) {
            if ((*it)->m_stream == stream) {
                (*it)->Stop();
            }
            ++it;
        }
    }
    m_endpointListLock.Unlock();
}

void DaemonICETransport::SendSTUNKeepAliveAndTURNRefreshRequest(ICEPacketStream& icePktStream)
{
    QCC_DbgTrace(("DaemonICETransport::SendSTUNKeepAliveAndTURNRefreshRequest(icePktStream=%p)", &icePktStream));

    QStatus status = ER_OK;

    if (status == ER_OK) {
        status = icePktStream.SendNATKeepAlive();
    }
    if (status != ER_OK) {
        QCC_LogError(ER_FAIL, ("Failed to send NAT keep alive for icePktStream=%p", &icePktStream));
    }

    /* Send TURN refresh (if needed) at slower interval */
    if (icePktStream.IsLocalTurn()) {
        uint64_t now = GetTimestamp64();

        if ((now - icePktStream.GetTurnRefreshTimestamp()) >= icePktStream.GetTurnRefreshPeriod()) {

            /* Send TURN refresh */
            if (status == ER_OK) {
                status = icePktStream.SendTURNRefresh(now);
            }

            if (status != ER_OK) {
                QCC_LogError(status, ("Failed to send TURN refresh for icePktStream=%p", &icePktStream));
            }
        }
    }

    /* Reload the alarm */
    AlarmContext* ctx = new AlarmContext(&icePktStream);
    Alarm keepAliveAlarm(icePktStream.GetStunKeepAlivePeriod(), this, 0, ctx);
    status = daemonICETransportTimer.AddAlarm(keepAliveAlarm);
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICEEndpoint::SendSTUNKeepAliveAndTURNRefreshRequest(): Unable to add KeepAliveAlarm to daemonICETransportTimer"));
    }
}

void DaemonICETransport::EndpointExit(RemoteEndpoint* ep)
{
    /*
     * This is a callback driven from the remote endpoint thread exit function.
     * Our DaemonICEEndpoint inherits from class RemoteEndpoint and so when
     * either of the threads (transmit or receive) of one of our endpoints exits
     * for some reason, we get called back here.  We only get called if either
     * the tx or rx thread exits, which implies that they have been run.  It
     * turns out that in the case of an endpoint receiving a connection, it
     * means that authentication has succeeded.  In the case of an endpoint
     * doing the connect, the EndpointExit may have resulted from an
     * authentication error since authentication is done in the context of the
     * Connect()ing thread and may be reported through EndpointExit.
     */
    QCC_DbgTrace(("DaemonICETransport::EndpointExit()"));

    DaemonICEEndpoint* tep = static_cast<DaemonICEEndpoint*>(ep);
    assert(tep);

    /*
     * The endpoint can exit if it was asked to by us in response to a Disconnect()
     * from higher level code, or if it got an error from the underlying transport.
     * We need to notify upper level code if the disconnect is due to an event from
     * the transport.
     */
    if (m_listener && tep->IsSuddenDisconnect()) {
        m_listener->BusConnectionLost(tep->GetConnectSpec());
    }

    /*
     * If this is an active connection, what has happened is that the reference
     * count on the underlying RemoteEndpoint has been decremented to zero and
     * the Stop() function of the endpoint has been called.  This means that
     * we are done with the endpoint and it should be cleaned up.  Marking
     * the connection as active prevented the passive side cleanup, so we need
     * to deal with cleanup now.
     */
    tep->SetPassive();

    /*
     * Mark the endpoint as no longer running.  Since we are called from
     * the RemoteEndpoint ThreadExit routine, we know it has stopped both
     * the RX and TX threads and we can Join them in a timely manner.
     */
    tep->SetEpStopping();

    /* Remove enpoint's packetStream ref from icePktStreamMap */
    ReleaseICEPacketStream(tep->m_icePktStream);

    /*
     * Wake up the DaemonICETransport loop so that it deals with our passing immediately.
     */
    wakeDaemonICETransportRun.SetEvent();
}

ThreadReturn STDCALL DaemonICETransport::AllocateICESessionThread::Run(void* arg)
{
    QCC_DbgHLPrintf(("DaemonICETransport::AllocateICESessionThread::Run(): clientGUID(%s)", clientGUID.c_str()));

    QStatus status = ER_FAIL;

    /*Figure out the ICE Address Candidates*/
    ICESessionListenerImpl iceListener;
    ICESession* iceSession = NULL;

    assert(transportObj->m_dm);

    STUNServerInfo stunInfo;

    DiscoveryManager::SessionEntry entry;

    /*
     * Retrieve the STUN server information corresponding to the particular service name
     * on the remote daemon that we are intending to connect to. The STUN server
     * information is required to allocate the ICE candidates.
     */
    if (transportObj->m_dm->GetSTUNInfo(false, clientGUID, stunInfo) == ER_OK) {
        QCC_DbgPrintf(("DaemonICETransport::AllocateICESessionThread::Run(): Retrieved the STUN server information from the Discovery Manager"));
    } else {
        status = ER_FAIL;
        QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): Unable to retrieve the STUN server information from the Discovery Manager"));
        return 0;
    }

    /* Ensure that the TURN user and pwd tokens have not expired. If they have, then get new tokens from the
     * Rendezvous Server */
    if (!transportObj->CheckTURNTokenExpiry(stunInfo)) {
        status = transportObj->GetNewTokensFromServer(false, stunInfo, clientGUID);

        if (status != ER_OK) {
            QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): Unable to retrieve new "
                                  "tokens from the Rendezvous Server"));
            return 0;
        }
    }

    IPAddress onDemandAddress, persistentAddress;
    transportObj->m_dm->GetRendezvousConnIPAddresses(onDemandAddress, persistentAddress);

    /* Gather ICE candidates */
    status = transportObj->m_iceManager.AllocateSession(true, true, transportObj->m_dm->GetEnableIPv6(), &iceListener, iceSession, stunInfo,
                                                        onDemandAddress, persistentAddress);

    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): AllocateSession failed"));
    } else {
        status = iceListener.Wait();

        if (ER_OK != status) {
            if (status == ER_TIMEOUT) {
                QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): Timed out waiting for ICE Listener change notification"));
            } else {
                QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): Error waiting for ICE Listener change notification"));
            }
        } else if (iceListener.GetState() != ICESession::ICECandidatesGathered) {
            status = ER_FAIL;
            QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): Unexpected ICE listener state %d. Expected %d",
                                  iceListener.GetState(),
                                  ICESession::ICECandidatesGathered));
        } else {

            list<ICECandidates> candidates;
            String ufrag, pwd;

            /* Get the local ICE candidates */
            status = iceSession->GetLocalICECandidates(candidates, ufrag, pwd);
            QCC_DbgPrintf(("GetLocalICECandidates returned ufrag=%s, pwd=%s", ufrag.c_str(), pwd.c_str()));

            if (ER_OK == status) {
                /* Send candidates to the server */
                QCC_DbgPrintf(("DaemonICETransport::AllocateICESessionThread::Run(): Service sending candidates to Peer"));

                PeerCandidateListenerImpl peerCandidateListener;
                entry.SetServiceInfo(candidates, ufrag, pwd, &peerCandidateListener);

                /* Send the ICE Address Candidates to the client */
                status = (transportObj->m_dm)->QueueICEAddressCandidatesMessage(false, std::pair<String, DiscoveryManager::SessionEntry>(clientGUID, entry));

                if (status == ER_OK) {
                    /*
                     * We already have the Client's candidates in the DiscoveryManager. But
                     * Wait for the service candidates to be delivered to the client before triggering the ICE Checks
                     */
                    status = peerCandidateListener.Wait();
                    if (status != ER_OK && status != ER_TIMEOUT) {

                        QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): peerCandidateListener.Wait(): Failed"));

                    } else {

                        if (status == ER_OK) {
                            list<ICECandidates> peerCandidates;
                            String ice_frag;
                            String ice_pwd;

                            peerCandidateListener.GetPeerCandiates(peerCandidates, ice_frag, ice_pwd);

                            /* Start the ICE checks only if both the local and the remote candidate list are not empty */
                            if ((!candidates.empty()) && (!peerCandidates.empty())) {
                                QCC_DbgPrintf(("DaemonICETransport::AllocateICESessionThread::Run(): StartChecks(peer_frag=%s, peer_pwd=%s)", ice_frag.c_str(), ice_pwd.c_str()));

                                /*Start the ICE Checks*/
                                status = iceSession->StartChecks(peerCandidates, ice_frag, ice_pwd);

                                QCC_DbgPrintf(("DaemonICETransport::AllocateICESessionThread::Run(): StartChecks status(0x%x)\n", status));

                                if (status == ER_OK) {
                                    /* Wait for ICE to change to final state */

                                    QCC_DbgPrintf(("DaemonICETransport::AllocateICESessionThread::Run(): Wait for ICE Checks to complete\n"));
                                    /* Wait for the ICE Checks to complete */
                                    status = iceListener.Wait();

                                    if (ER_OK == status) {
                                        QCC_DbgPrintf(("DaemonICETransport::AllocateICESessionThread::Run(): ICE Checks complete\n"));

                                        ICESession::ICESessionState state = iceListener.GetState();

                                        QCC_DbgPrintf(("DaemonICETransport::AllocateICESessionThread::Run(): iceListener.GetState(0x%x)\n", state));

                                        if (ICESession::ICEChecksSucceeded == state) {
                                            QCC_DbgPrintf(("DaemonICETransport::AllocateICESessionThread::Run(): ICE Checks Succeeded\n"));

                                            /*Make note of the selected candidate pair*/
                                            vector<ICECandidatePair*> selectedCandidatePairList;
                                            iceSession->GetSelectedCandidatePairList(selectedCandidatePairList);

                                            if (selectedCandidatePairList.size() > 0) {

                                                StunActivity* stunActivityPtr = selectedCandidatePairList[0]->local->GetStunActivity();
                                                String remoteAddr = (stunActivityPtr->stun->GetRemoteAddr()).ToString();
                                                String remotePort = U32ToString((uint32_t)(stunActivityPtr->stun->GetRemotePort()));
                                                String connectSpec = "ice:guid=" + clientGUID;

                                                /* Wait for a while to let ICE settle down */
                                                // @@ JP THIS NEEDS WORK
                                                qcc::Sleep(2000);

                                                /* Disable listener threads */
                                                for (size_t i = 0; i < selectedCandidatePairList.size(); ++i) {
                                                    stunActivityPtr->candidate->StopCheckListener();
                                                }

                                                /* Make sure we still need this new ICE connection */
                                                transportObj->pktStreamMapLock.Lock(MUTEX_CONTEXT);
                                                pktStream = transportObj->AcquireICEPacketStream(connectSpec);
                                                if (pktStream) {
                                                    /* Reuse existing pkstream rather than having two for the same destination */
                                                    QCC_DbgPrintf(("DaemonICETransport::AllocateICESessionThread: Reusing existing pktStream for %s", connectSpec.c_str()));
                                                    transportObj->ReleaseICEPacketStream(*pktStream);
                                                } else {
                                                    /* Wrap ICE session FD in a new ICEPacketStream */
                                                    ICEPacketStream pks(*iceSession, *stunActivityPtr->stun, *selectedCandidatePairList[0]);
                                                    map<String, pair<ICEPacketStream, int32_t> >::iterator sit =
                                                        transportObj->pktStreamMap.insert(pair<String, pair<ICEPacketStream, int32_t> >(connectSpec, pair<ICEPacketStream, int32_t>(pks, 1))).first;
                                                    pktStream = &(sit->second.first);

                                                    /* Start ICEPacketStream */
                                                    status = pktStream->Start();

                                                    /* Stop the STUN RxThread and claim its file descriptor as our own */
                                                    stunActivityPtr->stun->ReleaseFD();

                                                    /* Make the packetEngine listen on icePktStream */
                                                    if (status == ER_OK) {
                                                        status = transportObj->m_packetEngine.AddPacketStream(*pktStream, *transportObj);
                                                    }

                                                    /*
                                                     * Set an alarm to guard against the client-side successfully creating an ICE session
                                                     * and then not following through with a PacketEngine connect.
                                                     */
                                                    if (status == ER_OK) {
                                                        AlarmContext* ctx = new AlarmContext(pktStream);
                                                        pktStream->SetTimeoutAlarm(Alarm(PACKET_ENGINE_ACCEPT_TIMEOUT_MS, transportObj, 0, ctx));
                                                        status = transportObj->daemonICETransportTimer.AddAlarm(pktStream->GetTimeoutAlarm());
                                                    }

                                                    if (status == ER_OK) {
                                                        /* If we are using the local and remote host candidate, we need not send
                                                         * NAT keepalives or TURN refreshes */
                                                        if ((!pktStream->IsLocalHost()) || (!pktStream->IsRemoteHost())) {
                                                            /* Arm the keep-alive/TURN refresh timer (immediate fire) */
                                                            AlarmContext* ctx = new AlarmContext(pktStream);
                                                            transportObj->daemonICETransportTimer.AddAlarm(Alarm(0, transportObj, 0, ctx));
                                                        }
                                                    } else {
                                                        transportObj->ReleaseICEPacketStream(*pktStream);
                                                        QCC_LogError(status, ("ICEPacketStream.Start or AddPacketStream failed"));
                                                    }
                                                }
                                                transportObj->pktStreamMapLock.Unlock(MUTEX_CONTEXT);
                                            } else {
                                                status = ER_FAIL;
                                                QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run():No successful candidates gathered"));
                                            }
                                        } else if (ICESession::ICEChecksRunning != state) {
                                            status = ER_FAIL;
                                            QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run():ICE Listener reported non-successful completion (%d)", state));
                                        }
                                    } else {
                                        if (status == ER_TIMEOUT) {
                                            QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): Timed out waiting for StartChecks to complete"));
                                        } else {
                                            QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): Wait for StartChecks failed"));
                                        }
                                    }
                                } else {
                                    QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): Unable to start the ICE Checks"));
                                }
                            } else {
                                status = ER_FAIL;
                                QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): Not starting the ICE checks: candidates.empty()=%d peerCandidates.empty()=%d", candidates.empty(), peerCandidates.empty()));
                            }
                        } else {
                            QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): Timed out waiting for the delivery of the Address Candidates to the peer"));
                        }
                    }
                } else {
                    QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): QueueICEAddressCandidatesMessage failed"));
                }
            } else {
                QCC_LogError(status, ("DaemonICETransport::AllocateICESessionThread::Run(): GetLocalICECandidates failed"));
            }
        }
    }

    /* Succeed or fail, this iceSession is done */
    if (iceSession) {
        transportObj->m_iceManager.DeallocateSession(iceSession);
        iceSession = NULL;
        (transportObj->m_dm)->RemoveSessionDetailFromMap(false, std::pair<String, DiscoveryManager::SessionEntry>(clientGUID, entry));
    }

    return 0;
}

void DaemonICETransport::AllocateICESessionThread::ThreadExit(Thread* thread)
{
    transportObj->allocateICESessionThreadsLock.Lock(MUTEX_CONTEXT);
    vector<AllocateICESessionThread*>::iterator it = transportObj->allocateICESessionThreads.begin();
    AllocateICESessionThread* deleteMe = NULL;
    while (it != transportObj->allocateICESessionThreads.end()) {
        if (*it == thread) {
            deleteMe = *it;
            transportObj->allocateICESessionThreads.erase(it++);
            break;
        } else {
            ++it;
        }
    }
    transportObj->allocateICESessionThreadsLock.Unlock(MUTEX_CONTEXT);
    if (deleteMe) {
        delete deleteMe;
    } else {
        QCC_LogError(ER_FAIL, ("Internal error: AllocateICESessionThread not found on list"));
    }
}

void DaemonICETransport::ManageEndpoints(Timespec tTimeout)
{
    QCC_DbgPrintf(("DaemonICETransport::ManageEndpoints"));
    m_endpointListLock.Lock(MUTEX_CONTEXT);

    /*
     * Run through the list of connections on the authList and cleanup
     * any that are no longer running or are taking too long to authenticate
     * (we assume a denial of service attack in this case).
     */
    set<DaemonICEEndpoint*>::iterator i = m_authList.begin();
    while (i != m_authList.end()) {
        DaemonICEEndpoint* ep = *i;
        DaemonICEEndpoint::AuthState authState = ep->GetAuthState();

        if (authState == DaemonICEEndpoint::AUTH_FAILED) {
            /*
             * The endpoint has failed authentication and the auth thread is
             * gone or is going away.  Since it has failed there is no way this
             * endpoint is going to be started so we can get rid of it as soon
             * as we Join() the (failed) authentication thread.
             */
            QCC_DbgHLPrintf(("DaemonICETransport::ManageEndpoints(): Scavenging failed authenticator"));
            m_authList.erase(i);
            m_endpointListLock.Unlock(MUTEX_CONTEXT);
            ep->AuthJoin();
            delete ep;
            m_endpointListLock.Lock(MUTEX_CONTEXT);
            i = m_authList.upper_bound(ep);
            continue;
        }

        Timespec tNow;
        GetTimeNow(&tNow);

        if (ep->GetStartTime() + tTimeout < tNow) {
            /*
             * This endpoint is taking too long to authenticate.  Stop the
             * authentication process.  The auth thread is still running, so we
             * can't just delete the connection, we need to let it stop in its
             * own time.  What that thread will do is to set AUTH_FAILED and
             * exit.  we will then clean it up the next time through this loop.
             * In the hope that the thread can exit and we can catch its exit
             * here and now, we take our thread off the OS ready list (Sleep)
             * and let the other thread run before looping back.
             */
            QCC_DbgHLPrintf(("DaemonICETransport::ManageEndpoints(): Scavenging slow authenticator"));
            ep->AuthStop();
            qcc::Sleep(1);
        }
        ++i;
    }

    /*
     * We've handled the authList, so now run through the list of connections on
     * the endpointList and cleanup any that are no longer running or Join()
     * authentication threads that have successfully completed.
     */
    i = m_endpointList.begin();
    while (i != m_endpointList.end()) {
        DaemonICEEndpoint* ep = *i;

        /*
         * We are only managing passive connections here, or active connections
         * that are done and are explicitly ready to be cleaned up.
         */
        DaemonICEEndpoint::SideState sideState = ep->GetSideState();
        if (sideState == DaemonICEEndpoint::SIDE_ACTIVE) {
            ++i;
            continue;
        }

        DaemonICEEndpoint::AuthState authState = ep->GetAuthState();
        DaemonICEEndpoint::EndpointState endpointState = ep->GetEpState();

        if (authState == DaemonICEEndpoint::AUTH_SUCCEEDED) {
            /*
             * The endpoint has succeeded authentication and the auth thread is
             * gone or is going away.  Take this opportunity to join the auth
             * thread.  Since the auth thread promised not to touch the state
             * after setting AUTH_SUCCEEEDED, we can safely change the state
             * here since we now own the conn.  We do this through a method call
             * to enable this single special case where we are allowed to set
             * the state.
             */
            QCC_DbgHLPrintf(("DaemonICETransport::ManageEndpoints(): Scavenging failed authenticator"));
            m_endpointListLock.Unlock(MUTEX_CONTEXT);
            ep->AuthJoin();
            ep->SetAuthDone();
            m_endpointListLock.Lock(MUTEX_CONTEXT);
            i = m_endpointList.upper_bound(ep);
            continue;
        }

        /*
         * There are two possibilities for the disposition of the RX and
         * TX threads.  First, they were never successfully started.  In
         * this case, the epState will be EP_FAILED.  If we find this, we
         * can just remove the useless endpoint from the list and delete
         * it.  Since the threads were never started, they must not be
         * joined.
         */
        if (endpointState == DaemonICEEndpoint::EP_FAILED) {
            m_endpointList.erase(i);
            m_endpointListLock.Unlock(MUTEX_CONTEXT);
            delete ep;
            m_endpointListLock.Lock(MUTEX_CONTEXT);
            i = m_endpointList.upper_bound(ep);
            continue;
        }

        /*
         * The second possibility for the disposition of the RX and
         * TX threads is that they were successfully started but
         * have been stopped for some reason, either because of a
         * Disconnect() or a network error.  In this case, the
         * epState will be EP_STOPPING, which was set in the
         * EndpointExit function.  If we find this, we need to Join
         * the endpoint threads, remove the endpoint from the
         * endpoint list and delete it.  Note that we are calling
         * the endpoint Join() to join the TX and RX threads and not
         * the endpoint AuthJoin() to join the auth thread.
         */
        if (endpointState == DaemonICEEndpoint::EP_STOPPING) {
            m_endpointList.erase(i);
            m_endpointListLock.Unlock(MUTEX_CONTEXT);
            ep->Join();
            delete ep;
            m_endpointListLock.Lock(MUTEX_CONTEXT);
            i = m_endpointList.upper_bound(ep);
            continue;
        }
        ++i;
    }
    m_endpointListLock.Unlock(MUTEX_CONTEXT);
}


void* DaemonICETransport::Run(void* arg)
{
    QCC_DbgTrace(("DaemonICETransport::Run()"));

    /*
     * This is the Thread Run function for our server accept loop.  We require
     * that the discovery manager be started before the Thread that will call us
     * here.
     */
    assert(m_dm);

    /*
     * We need to find the defaults for our connection limits.  These limits
     * can be specified in the configuration database with corresponding limits
     * used for DBus.  If any of those are present, we use them, otherwise we
     * provide some hopefully reasonable defaults.
     */
    DaemonConfig* config = DaemonConfig::Access();

    /*
     * tTimeout is the maximum amount of time we allow incoming connections to
     * mess about while they should be authenticating.  If they take longer
     * than this time, we feel free to disconnect them as deniers of service.
     */
    Timespec tTimeout = config->Get("limit@auth_timeout", ALLJOYN_AUTH_TIMEOUT_DEFAULT);

    /*
     * maxAuth is the maximum number of incoming connections that can be in
     * the process of authenticating.  If starting to authenticate a new
     * connection would mean exceeding this number, we drop the new connection.
     */
    uint32_t maxAuth = config->Get("ice/limit@max_incomplete_connections", ALLJOYN_MAX_INCOMPLETE_CONNECTIONS_ICE_DEFAULT);

    /*
     * maxConn is the maximum number of active connections possible over the
     * ICE transport.  If starting to process a new connection would mean
     * exceeding this number, we drop the new connection.
     */
    uint32_t maxConn = config->Get("ice/limit@max_completed_connections", ALLJOYN_MAX_COMPLETED_CONNECTIONS_ICE_DEFAULT);

    QStatus status = ER_OK;

    vector<Event*> checkEvents, signaledEvents;

    /*
     * Each time through the loop, we need to wait on the stop and the wakeDaemonICETransportRun event.
     */
    checkEvents.push_back(&stopEvent);
    checkEvents.push_back(&wakeDaemonICETransportRun);

    /* Add the DaemonICETransport::Run schedule alarm to the daemonICETransportTimer */
    AlarmContext* ctx = new AlarmContext();
    Alarm runAlarm(DAEMON_ICE_TRANSPORT_RUN_SCHEDULING_INTERVAL, this, 0, ctx);
    status = daemonICETransportTimer.AddAlarm(runAlarm);

    while (!IsStopping()) {
        /*
         * We require that the discovery manager be created and started before the
         * Thread that called us here; and we require that the discovery manager stay
         * around until after we leave.
         */
        assert(m_dm);

        status = Event::Wait(checkEvents, signaledEvents);

        if (ER_OK != status) {
            QCC_LogError(status, ("DaemonICETransport::Run(): Event::Wait failed"));
            break;
        }

        QCC_DbgPrintf(("DaemonICETransport::Run()"));

        /*
         * We're back from our Wait() so we have received either the stop or
         * wakeDaemonICETransportRun the event
         */
        for (vector<Event*>::iterator i = signaledEvents.begin(); i != signaledEvents.end(); ++i) {

            /*
             * In order to rationalize management of resources, we manage the
             * various lists in one place on one thread.  This thread is a
             * convenient victim, so we do it here.
             */
            ManageEndpoints(tTimeout);

            /*
             * Reset the Stop() event if it has been received.
             */
            if (*i == &stopEvent) {
                stopEvent.ResetEvent();
                continue;
            }

            /* If the current event received is not the stop event, then it is the wakeDaemonICETransportRun
             * event which indicates that a new AllocateICESession request has been received*/

            // Now look into IncomingICESessions to see if there are any incoming ICE Session Allocation
            // requests. If there are, then allocate the ICE Sessions and clear the corresponding entry from
            // the IncomingICESessions. Spin off a separate thread to handle this allocation of ICE Sessions.
            m_IncomingICESessionsLock.Lock(MUTEX_CONTEXT);

            while (!IncomingICESessions.empty()) {
                QCC_DbgPrintf(("DaemonICETransport::Run(): maxAuth == %d", maxAuth));
                QCC_DbgPrintf(("DaemonICETransport::Run(): maxConn == %d", maxConn));
                QCC_DbgPrintf(("DaemonICETransport::Run(): mAuthList.size() == %d", m_authList.size()));
                QCC_DbgPrintf(("DaemonICETransport::Run(): mEndpointList.size() == %d", m_endpointList.size()));
                assert(m_authList.size() + m_endpointList.size() <= maxConn);

                /*
                 * Do we have a slot available for a new connection?  If so, use
                 * it.
                 */
                m_endpointListLock.Lock(MUTEX_CONTEXT);
                if ((m_authList.size() < maxAuth) && (m_authList.size() + m_endpointList.size() < maxConn)) {
                    /* Handle AllocateICESession on another thread */
                    allocateICESessionThreadsLock.Lock(MUTEX_CONTEXT);

                    if (!m_stopping) {
                        AllocateICESessionThread* ast = new AllocateICESessionThread(this, IncomingICESessions.front());
                        status = ast->Start(NULL, ast);
                        if (status == ER_OK) {
                            allocateICESessionThreads.push_back(ast);

                        } else {
                            QCC_LogError(status, ("DaemonICETransport::Run(): Failed to start AllocateICESessionThread"));
                        }
                    }
                    IncomingICESessions.pop_front();
                    allocateICESessionThreadsLock.Unlock(MUTEX_CONTEXT);
                    m_endpointListLock.Unlock(MUTEX_CONTEXT);
                } else {
                    m_endpointListLock.Unlock(MUTEX_CONTEXT);
                    IncomingICESessions.clear();
                    status = ER_AUTH_FAIL;
                    QCC_LogError(status, ("DaemonICETransport::Run(): No slot for new connection"));
                }
            }

            m_IncomingICESessionsLock.Unlock(MUTEX_CONTEXT);

            // Reset the wakeDaemonICETransportRun
            if (*i == &wakeDaemonICETransportRun) {
                wakeDaemonICETransportRun.ResetEvent();
            }

            if (status != ER_OK) {
                QCC_LogError(status, ("DaemonICETransport::Run(): Error accepting new connection. Ignoring..."));
            }
        }

        signaledEvents.clear();
    }

    QCC_DbgPrintf(("DaemonICETransport::Run is exiting status=%s", QCC_StatusText(status)));
    return (void*) status;
}

QStatus DaemonICETransport::NormalizeListenSpec(const char* inSpec, String& outSpec, std::map<String, String>& argMap) const
{
    /*
     * Take the string in inSpec, which must start with "ice:" and parse it,
     * looking for comma-separated "key=value" pairs and initialize the
     * argMap with those pairs.
     */
    QStatus status = ParseArguments("ice", inSpec, argMap);
    if (status != ER_OK) {
        return status;
    }

    return ER_OK;
}

QStatus DaemonICETransport::NormalizeTransportSpec(const char* inSpec, String& outSpec, std::map<String, String>& argMap) const
{
    /*
     * Take the string in inSpec, which must start with "ice:" and parse it,
     * looking for comma-separated "key=value" pairs and initialize the
     * argMap with those pairs.
     */
    QStatus status = ParseArguments("ice", inSpec, argMap);
    if (status != ER_OK) {
        return status;
    }

    map<String, String>::iterator i = argMap.find("guid");
    if (i == argMap.end()) {
        status = ER_BUS_BAD_TRANSPORT_ARGS;
        QCC_LogError(status, ("DaemonICETransport::NormalizeTransportSpec: The GUID of the remote daemon "
                              "has not been specified in the ICE Transport Address"));
    } else {
        /*
         * We have a value associated with the "guid" key.  Run it through
         * a conversion function to make sure it's a valid value.
         */
        outSpec = "ice:guid=" + i->second;
    }

    return ER_OK;
}

QStatus DaemonICETransport::Connect(const char* connectSpec, const SessionOpts& opts, BusEndpoint** newep)
{
    QCC_DbgHLPrintf(("DaemonICETransport::Connect(): %s", connectSpec));

    QStatus status = ER_FAIL;
    ICESession* iceSession = NULL;

    /*
     * We only want to allow this call to proceed if we have a running
     * DaemonICETransport thread that isn't in the process of shutting down.
     * We use the thread response from IsRunning to give us an idea of what our
     * Run thread is doing.  See the comment in Start() for details
     * about what IsRunning actually means, which might be subtly different from
     * your intuition.
     *
     * If we see IsRunning(), the thread might actually have gotten a Stop(),
     * but has not yet exited its Run routine and become STOPPING.  To plug this
     * hole, we need to check IsRunning() and also m_stopping, which is set in
     * our Stop() method.
     */
    if (IsRunning() == false || m_stopping == true) {
        QCC_LogError(ER_BUS_TRANSPORT_NOT_STARTED, ("DaemonICETransport::Connect(): Not running or stopping; exiting"));
        return ER_BUS_TRANSPORT_NOT_STARTED;
    }

    /*
     * If we pass the IsRunning() gate above, we must have a Run
     * thread spinning up or shutting down but not yet joined.  Since the
     * DsicoveryManager is created before the server accept thread is spun up, and
     * deleted after it is joined, we must have a valid name service or someone
     * isn't playing by the rules; so an assert is appropriate here.
     */
    assert(m_dm);

    DiscoveryManager::SessionEntry entry;

    /*
     * Parse and normalize the connectArgs.
     */
    String normSpec;
    map<String, String> argMap;
    status = NormalizeTransportSpec(connectSpec, normSpec, argMap);
    if (ER_OK != status) {
        QCC_LogError(status, ("DaemonICETransport::Connect(): Invalid ICE connect spec \"%s\"", connectSpec));
        return status;
    }

    pktStreamMapLock.Lock();
    ICEPacketStream* pktStream = AcquireICEPacketStream(normSpec);
    if (!pktStream) {
        /*
         * No pktStream exists. Put a dummy one on the pktStreamMap so other join attempts for the
         * same destination (normSpec) will wait for this join's ICE dance to complete.
         */
        ICEPacketStream pks;
        std::pair<std::map<qcc::String, std::pair<ICEPacketStream, int32_t> >::iterator, bool> ins =
            pktStreamMap.insert(std::make_pair(normSpec, pair<ICEPacketStream, int32_t>(pks, 1)));
        pktStream = &(ins.first->second.first);
        pktStreamMapLock.Unlock();

        /*
         * Figure out the ICE Address Candidates
         */
        ICESessionListenerImpl iceListener;

        STUNServerInfo stunInfo;

        /*
         * Retrieve the STUN server information corresponding to the particular service name
         * on the remote daemon that we are intending to connect to. The STUN server
         * information is required to allocate the ICE candidates.
         */
        if (m_dm->GetSTUNInfo(true, argMap["guid"], stunInfo) == ER_OK) {
            QCC_DbgPrintf(("DaemonICETransport::Connect(): Retrieved the STUN server information from the Discovery Manager"));
        } else {
            status = ER_FAIL;
            QCC_LogError(status, ("DaemonICETransport::Connect(): Unable to retrieve the STUN server information from the Discovery Manager"));
            return status;
        }

        /* Ensure that the TURN user and pwd tokens have not expired. If they have, then get new tokens from the
         * Rendezvous Server */
        if (!CheckTURNTokenExpiry(stunInfo)) {
            status = GetNewTokensFromServer(true, stunInfo, argMap["guid"]);
            if (status != ER_OK) {
                QCC_LogError(status, ("DaemonICETransport::Connect(): Unable to retrieve new tokens from the Rendezvous Server"));
                return status;
            }
        }

        IPAddress onDemandAddress, persistentAddress;
        m_dm->GetRendezvousConnIPAddresses(onDemandAddress, persistentAddress);

        /* Gather ICE candidates */
        status = m_iceManager.AllocateSession(true, false, m_dm->GetEnableIPv6(), &iceListener, iceSession, stunInfo,
                                              onDemandAddress, persistentAddress);
        if (status == ER_OK) {
            status = iceListener.Wait();

            if (ER_OK != status) {
                if (status == ER_TIMEOUT) {
                    QCC_LogError(status, ("DaemonICETransport::Connect(): Timed out waiting for ICE Listener change notification"));
                } else {
                    QCC_LogError(status, ("DaemonICETransport::Connect(): Error waiting for ICE Listener change notification"));
                }
            } else if (iceListener.GetState() == ICESession::ICECandidatesGathered) {

                list<ICECandidates> candidates;
                String ufrag, pwd;

                /* Get the local ICE candidates */
                status = iceSession->GetLocalICECandidates(candidates, ufrag, pwd);

                if (ER_OK == status) {
                    /* Send ICE candidates to server */
                    QCC_DbgPrintf(("DaemonICETransport::Connect(): Client sending its candidates to Peer"));

                    PeerCandidateListenerImpl peerCandidateListener;
                    entry.SetClientInfo(candidates, ufrag, pwd, &peerCandidateListener);

                    status = m_dm->QueueICEAddressCandidatesMessage(true, std::pair<String, DiscoveryManager::SessionEntry>(argMap["guid"], entry));

                    if (status == ER_OK) {
                        /*
                         * Wait for something to happen.  if we get an error, there's not
                         * much we can do about it but bail.
                         */
                        status = peerCandidateListener.Wait();

                        if (status == ER_OK) {
                            QCC_DbgPrintf(("DaemonICETransport::Connect(): Wake event fired\n"));

                            list<ICECandidates> peerCandidates;
                            String ice_frag;
                            String ice_pwd;

                            /* Retrieve the Service's candidates */
                            peerCandidateListener.GetPeerCandiates(peerCandidates, ice_frag, ice_pwd);

                            if ((!candidates.empty()) && (!peerCandidates.empty())) {
                                QCC_DbgPrintf(("DaemonICETransport::Connect(): Starting ICE Checks"));

                                /* Start the ICE Checks*/
                                status = iceSession->StartChecks(peerCandidates, false, ice_frag, ice_pwd);

                                QCC_DbgPrintf(("DaemonICETransport::Connect(): StartChecks status = 0x%x", status));

                                if (status == ER_OK) {
                                    /* Wait for ICE to change to final state */
                                    QCC_DbgPrintf(("DaemonICETransport::Connect(): Waiting for StartChecks to complete"));
                                    status = iceListener.Wait();
                                    QCC_DbgPrintf(("DaemonICETransport::Connect(): StartChecks done status=0x%x", status));

                                    if (ER_OK == status) {
                                        ICESession::ICESessionState state = iceListener.GetState();

                                        QCC_DbgPrintf(("DaemonICETransport::Connect(): state=0x%x", state));

                                        if (ICESession::ICEChecksSucceeded == state) {

                                            QCC_DbgPrintf(("DaemonICETransport::Connect(): ICE Checks Succeeded"));

                                            /*Make note of the selected candidate pair*/
                                            vector<ICECandidatePair*> selectedCandidatePairList;
                                            iceSession->GetSelectedCandidatePairList(selectedCandidatePairList);

                                            if (selectedCandidatePairList.size() > 0) {

                                                /* Wait for a while to let ICE settle down */
                                                // @@ JP THIS NEEDS WORK
                                                qcc::Sleep(2000);

                                                /* Disable listener threads */
                                                for (size_t i = 0; i < selectedCandidatePairList.size(); ++i) {
                                                    selectedCandidatePairList[i]->local->GetStunActivity()->candidate->StopCheckListener();
                                                }

                                                /* Hold pktStreamMapLock while updating this pktStream */
                                                pktStreamMapLock.Lock(MUTEX_CONTEXT);

                                                /* Stop the STUN RxThread and claim its file descriptor as our own */
                                                Stun* stun = selectedCandidatePairList[0]->local->GetStunActivity()->stun;

                                                /* Wrap ICE session FD in a new ICEPacketStream (and reset ref count) */
                                                *pktStream = ICEPacketStream(*iceSession, *stun, *selectedCandidatePairList[0]);

                                                /* Start ICEPacketStream */
                                                status = pktStream->Start();

                                                /* Make Stun give up ownership of its fd */
                                                stun->ReleaseFD();

                                                /* Deallocate the iceSession. This must be done BEFORE the packetEngine starts using stun's fd */
                                                m_iceManager.DeallocateSession(iceSession);
                                                iceSession = NULL;
                                                m_dm->RemoveSessionDetailFromMap(true, std::pair<String, DiscoveryManager::SessionEntry>(argMap["guid"], entry));

                                                /* Make the packetEngine listen on icePktStream */
                                                if (status == ER_OK) {
                                                    status = m_packetEngine.AddPacketStream(*pktStream, *this);
                                                }

                                                if (status == ER_OK) {
                                                    /* If we are using the local and remote host candidate, we need not send NAT keepalives or TURN refreshes */
                                                    if ((!pktStream->IsLocalHost()) || (!pktStream->IsRemoteHost())) {
                                                        /* Arm the keep-alive (immediate fire) */
                                                        AlarmContext* ctx = new AlarmContext(pktStream);
                                                        daemonICETransportTimer.AddAlarm(Alarm(0, this, 0, ctx));
                                                    }
                                                } else {
                                                    QCC_LogError(status, ("ICEPacketStream.Start or AddPacketStream failed"));
                                                }
                                                pktStreamMapLock.Unlock();
                                            } else {
                                                status = ER_FAIL;
                                                QCC_LogError(status, ("DaemonICETransport::Connect():No successful candidates gathered"));
                                            }
                                        } else if (ICESession::ICEChecksRunning != state) {
                                            status = ER_FAIL;
                                            QCC_LogError(status, ("DaemonICETransport::Connect():ICE Listener reported non-successful completion (%d)", state));
                                        } else {
                                            status = ER_FAIL;
                                            QCC_LogError(status, ("DaemonICETransport::Connect(): Unexpected ICE state (%d)", state));
                                        }
                                    } else {
                                        if (status == ER_TIMEOUT) {
                                            QCC_LogError(status, ("DaemonICETransport::Connect(): Timed out waiting for StartChecks to complete"));
                                        } else {
                                            QCC_LogError(status, ("DaemonICETransport::Connect(): Error waiting for StartChecks to complete"));
                                        }
                                    }
                                } else {
                                    QCC_LogError(status, ("DaemonICETransport::Connect(): Unable to start the ICE Checks"));
                                }
                            } else {
                                status = ER_FAIL;
                                QCC_LogError(status, ("DaemonICETransport::Connect(): Not starting the ICE checks: candidates.empty()=%d peerCandidates.empty()=%d", candidates.empty(), peerCandidates.empty()));
                            }
                        } else if (status == ER_TIMEOUT) {
                            QCC_DbgPrintf(("DaemonICETransport::Connect(): Wait timed out\n"));
                        } else {
                            QCC_LogError(status, ("DaemonICETransport::Connect(): peerCandidateListener.Wait() Failed"));
                        }
                    } else {
                        QCC_LogError(status, ("DaemonICETransport::Connect(): QueueICEAddressCandidatesMessage failed"));
                    }
                } else {
                    QCC_LogError(status, ("DaemonICETransport::Connect(): GetLocalICECandidates failed"));
                }
            } else {
                status = ER_FAIL;
                QCC_LogError(status, ("DaemonICETransport::Connect(): Unexpected ICE listener state %d. Expected %d",
                                      iceListener.GetState(),
                                      ICESession::ICECandidatesGathered));
            }
        } else {
            QCC_LogError(status, ("DaemonICETransport::Connect(): AllocateSession failed"));
        }
    } else {
        /*
         * Attempt to reuse existing pktStream.
         * pktStream may still be initializing from a different session's ICE dance.
         * Wait for a fully functional pktStream or until it disappears.
         */
        pktStreamMapLock.Unlock();
        while (pktStream && !pktStream->HasSocket()) {
            ReleaseICEPacketStream(*pktStream);
            qcc::Sleep(5);
            pktStream = AcquireICEPacketStream(normSpec);
        }
    }

    /* Make sure we have a pktStream */
    if (!pktStream || !pktStream->HasSocket()) {
        status = ER_BUS_CONNECT_FAILED;
    }

    /* If we created or reused an ICEPacketStream, the wrap it in a DamonICEEndpoint */
    DaemonICEEndpoint* conn = NULL;
    if (status == ER_OK) {
        conn = new DaemonICEEndpoint(this, m_bus, false, normSpec, *pktStream);
        /* Setup the PacketEngine connection */
        status = conn->PacketEngineConnect(pktStream->GetICERemoteAddr(), pktStream->GetICERemotePort());
        if (status == ER_OK) {
            /*
             * On the active side of a connection, we don't need an authentication
             * thread to run since we have the caller thread.  We do have to put the
             * endpoint on the endpoint list to be assured that errors get logged.
             * By marking the connection as active, we prevent the server accept thread
             * from cleaning up this endpoint.  For consistency, we mark the endpoint
             * as authenticating to avoid ugly surprises.
             */
            conn->SetActive();
            conn->SetAuthenticating();
            m_endpointListLock.Lock(MUTEX_CONTEXT);
            m_endpointList.insert(conn);
            m_endpointListLock.Unlock(MUTEX_CONTEXT);

            /* Initialized the features for this endpoint */
            conn->GetFeatures().isBusToBus = true;
            conn->GetFeatures().allowRemote = m_bus.GetInternal().AllowRemoteMessages();
            conn->GetFeatures().handlePassing = false;

            String authName;
            String redirection;
            /*
             * Go ahead and to the authentication in the context of this thread.  Even
             * though we have prevented the server accept loop from cleaning up our
             * endpoint by marking it as active, we keep the states consistent.
             */
            status = conn->Establish("ANONYMOUS", authName, redirection);
            if (status == ER_OK) {
                conn->SetListener(this);
                status = conn->Start();
                if (status == ER_OK) {
                    conn->SetEpStarted();
                    conn->SetAuthDone();
                } else {
                    conn->SetEpFailed();
                    conn->SetAuthDone();
                }
            }
        }

        /*
         * If we did get an error, we need to remove the endpoint if it
         * is still there and the endpoint exit callback didn't kill it.
         */
        if ((status != ER_OK) && conn) {
            QCC_LogError(status, ("DaemonICETransport::Connect(): Start DaemonICEEndpoint failed"));

            m_endpointListLock.Lock(MUTEX_CONTEXT);
            set<DaemonICEEndpoint*>::iterator i = m_endpointList.find(conn);
            if (i != m_endpointList.end()) {
                m_endpointList.erase(i);
            }
            m_endpointListLock.Unlock(MUTEX_CONTEXT);
            delete conn;
        }
    }

    /* Clean up iceSession if it hasnt been already */
    if (iceSession) {
        m_iceManager.DeallocateSession(iceSession);
        iceSession = NULL;
        m_dm->RemoveSessionDetailFromMap(true, std::pair<String, DiscoveryManager::SessionEntry>(argMap["guid"], entry));
    }

    /* Remove new packet stream if we failed */
    if ((status != ER_OK) && pktStream) {
        ReleaseICEPacketStream(*pktStream);
    }

    /* Set caller's ep ref */
    if (status != ER_OK) {
        if (newep) {
            *newep = NULL;
        }
    } else {
        if (newep && (conn != NULL)) {
            *newep = conn;
        }
    }

    return status;
}

QStatus DaemonICETransport::Disconnect(const char* connectSpec)
{
    QCC_DbgHLPrintf(("DaemonICETransport::Disconnect(): %s", connectSpec));

    /*
     * We only want to allow this call to proceed if we have a Run
     * thread that isn't in the process of shutting down.  We use the
     * thread response from IsRunning to give us an idea of what our
     * (Run) thread is doing, and by extension the endpoint threads which
     * must be running to properly clean up.  See the comment in Start() for
     * details about what IsRunning actually means, which might be subtly
     * different from your intuitition.
     *
     * If we see IsRunning(), the thread might actually have gotten a Stop(),
     * but has not yet exited its Run routine and become STOPPING.  To plug this
     * hole, we need to check IsRunning() and also m_stopping, which is set in
     * our Stop() method.
     */
    if (IsRunning() == false || m_stopping == true) {
        QCC_LogError(ER_BUS_TRANSPORT_NOT_STARTED, ("DaemonICETransport::Disconnect(): Not running or stopping; exiting"));
        return ER_BUS_TRANSPORT_NOT_STARTED;
    }

    /*
     * If we pass the IsRunning() gate above, we must have a Run
     * thread spinning up or shutting down but not yet joined.  Since the discovery
     * manager is created before the Run thread is spun up, and
     * deleted after it is joined, we must have a valid discovery manager or someone
     * isn't playing by the rules; so an assert is appropriate here.
     */
    assert(m_dm);

    /*
     * Higher level code tells us which connection is refers to by giving us the
     * same connect spec it used in the Connect() call.
     */
    qcc::String normSpec;
    map<qcc::String, qcc::String> argMap;
    QStatus status = NormalizeTransportSpec(connectSpec, normSpec, argMap);
    if (ER_OK != status) {
        QCC_LogError(status, ("DaemonICETransport::Disconnect(): Invalid ICE connect spec \"%s\"", connectSpec));
        return status;
    }

    /*
     * Stop the remote endpoint.  Be careful here since calling Stop() on the
     * DaemonICEEndpoint is going to cause the transmit and receive threads of the
     * underlying RemoteEndpoint to exit, which will cause our EndpointExit()
     * to be called, which will walk the list of endpoints and delete the one
     * we are stopping.  Once we poke ep->Stop(), the pointer to ep must be
     * considered dead.
     */
    status = ER_BUS_BAD_TRANSPORT_ARGS;
    m_endpointListLock.Lock(MUTEX_CONTEXT);
    for (set<DaemonICEEndpoint*>::iterator i = m_endpointList.begin(); i != m_endpointList.end(); ++i) {
        if ((*i)->GetConnectSpec() == connectSpec) {
            DaemonICEEndpoint* ep = *i;
            ep->SetSuddenDisconnect(false);
            m_endpointListLock.Unlock(MUTEX_CONTEXT);
            return ep->Stop();
        }
    }
    m_endpointListLock.Unlock(MUTEX_CONTEXT);
    return status;
}

/*
 * The default interface for the Discovery Manager to use.  The wildcard character
 * means to listen and transmit over all interfaces that are up with any IP address
 * they happen to have.  This default also applies to the search for listen address interfaces.
 */
static const char* INTERFACES_DEFAULT = "*";

QStatus DaemonICETransport::StartListen(const char* listenSpec)
{
    QCC_DbgPrintf(("DaemonICETransport::StartListen()"));

    /*
     * We only want to allow this call to proceed if we have a running Run
     * thread that isn't in the process of shutting down.  We use the
     * thread response from IsRunning to give us an idea of what our server
     * accept (Run) thread is doing.  See the comment in Start() for details
     * about what IsRunning actually means, which might be subtly different from
     * your intuition.
     *
     * If we see IsRunning(), the thread might actually have gotten a Stop(),
     * but has not yet exited its Run routine and become STOPPING.  To plug this
     * hole, we need to check IsRunning() and also m_stopping, which is set in
     * our Stop() method.
     */
    if (IsRunning() == false || m_stopping == true) {
        QCC_LogError(ER_BUS_TRANSPORT_NOT_STARTED, ("DaemonICETransport::StartListen(): Not running or stopping; exiting"));
        return ER_BUS_TRANSPORT_NOT_STARTED;
    }

    /*
     * Normalize the listen spec.  Although this looks like a connectSpec it is
     * different in that reasonable defaults are possible.
     */
    String normSpec;
    map<String, String> argMap;
    QStatus status = NormalizeListenSpec(listenSpec, normSpec, argMap);
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::StartListen(): Invalid listen spec \"%s\"", listenSpec));
        return status;
    }

    assert(m_dm);

    QCC_DbgPrintf(("DaemonICETransport::StartListen():"));

    /*
     * Get the configuration item telling us which network interfaces we
     * should run the Discovery Manager over. The item specifies an interface name.
     * The Discovery Manager waits until it finds the interface IFF_UP
     * with an assigned IP address and then starts using the interface.  If the
     * configuration item contains "*" (the wildcard) it is interpreted as
     * meaning all suitable interfaces.  If the configuration item is
     * empty (not assigned in the configuration database) it defaults to "*".
     */
    qcc::String interfaces = DaemonConfig::Access()->Get("ice_discovery_manager/property@interfaces", INTERFACES_DEFAULT);

    while (interfaces.size()) {
        String currentInterface;
        size_t i = interfaces.find(",");
        if (i != String::npos) {
            currentInterface = interfaces.substr(0, i);
            interfaces = interfaces.substr(i + 1, interfaces.size() - i - 1);
        } else {
            currentInterface = interfaces;
            interfaces.clear();
        }

        status = m_dm->OpenInterface(currentInterface);

        if (status != ER_OK) {
            QCC_LogError(status, ("DaemonICETransport::StartListen(): OpenInterface() failed for %s", currentInterface.c_str()));
        }
    }

    return status;
}

QStatus DaemonICETransport::StopListen(const char* listenSpec)
{
    QCC_DbgPrintf(("DaemonICETransport::StopListen()"));
    QStatus status = ER_OK;

    /*
     * We only want to allow this call to proceed if we have a running Run
     * thread that isn't in the process of shutting down.  We use the
     * thread response from IsRunning to give us an idea of what our server
     * accept (Run) thread is doing.  See the comment in Start() for details
     * about what IsRunning actually means, which might be subtly different from
     * your intuitition.
     *
     * If we see IsRunning(), the thread might actually have gotten a Stop(),
     * but has not yet exited its Run routine and become STOPPING.  To plug this
     * hole, we need to check IsRunning() and also m_stopping, which is set in
     * our Stop() method.
     */
    if (IsRunning() == false || m_stopping == true) {
        QCC_LogError(ER_BUS_TRANSPORT_NOT_STARTED, ("DaemonICETransport::StopListen(): Not running or stopping; exiting"));
        return ER_BUS_TRANSPORT_NOT_STARTED;
    }

    assert(m_dm);
    //
    // Tell the Discovery Manager to tear down the existing TCP connection with the
    // Rendezvous Server - if it exists
    //
    m_dm->SetDisconnectEvent();

    return status;
}

void DaemonICETransport::EnableDiscovery(const char* namePrefix)
{
    /*
     * We only want to allow this call to proceed if we have a running Run
     * thread that isn't in the process of shutting down.  We use the
     * thread response from IsRunning to give us an idea of what our server
     * accept (Run) thread is doing.  See the comment in Start() for details
     * about what IsRunning actually means, which might be subtly different from
     * your intuitition.
     *
     * If we see IsRunning(), the thread might actually have gotten a Stop(),
     * but has not yet exited its Run routine and become STOPPING.  To plug this
     * hole, we need to check IsRunning() and also m_stopping, which is set in
     * our Stop() method.
     */
    if (IsRunning() == false || m_stopping == true) {
        QCC_LogError(ER_BUS_TRANSPORT_NOT_STARTED, ("DaemonICETransport::EnableDiscovery(): Not running or stopping; exiting"));
        return;
    }

    QStatus status = m_dm->SearchName(namePrefix);

    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::EnableDiscovery(): Failure enabling discovery for \"%s\" on ICE", namePrefix));
    }
}

void DaemonICETransport::DisableDiscovery(const char* namePrefix)
{
    /*
     * We only want to allow this call to proceed if we have a running Run
     * thread that isn't in the process of shutting down.  We use the
     * thread response from IsRunning to give us an idea of what our server
     * accept (Run) thread is doing.  See the comment in Start() for details
     * about what IsRunning actually means, which might be subtly different from
     * your intuitition.
     *
     * If we see IsRunning(), the thread might actually have gotten a Stop(),
     * but has not yet exited its Run routine and become STOPPING.  To plug this
     * hole, we need to check IsRunning() and also m_stopping, which is set in
     * our Stop() method.
     */
    if (IsRunning() == false || m_stopping == true) {
        QCC_LogError(ER_BUS_TRANSPORT_NOT_STARTED, ("DaemonICETransport::DisableDiscovery(): Not running or stopping; exiting"));
        return;
    }

    assert(m_dm);
    QStatus status = m_dm->CancelSearchName(namePrefix);

    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::DisableDiscovery(): Failure disabling discovery for \"%s\" on ICE", namePrefix));
    }
}

QStatus DaemonICETransport::EnableAdvertisement(const String& advertiseName)
{
    /*
     * We only want to allow this call to proceed if we have a running Run
     * thread that isn't in the process of shutting down.  We use the
     * thread response from IsRunning to give us an idea of what our server
     * accept (Run) thread is doing.  See the comment in Start() for details
     * about what IsRunning actually means, which might be subtly different from
     * your intuitition.
     *
     * If we see IsRunning(), the thread might actually have gotten a Stop(),
     * but has not yet exited its Run routine and become STOPPING.  To plug this
     * hole, we need to check IsRunning() and also m_stopping, which is set in
     * our Stop() method.
     */
    if (IsRunning() == false || m_stopping == true) {
        QCC_LogError(ER_BUS_TRANSPORT_NOT_STARTED, ("DaemonICETransport::EnableAdvertisement(): Not running or stopping; exiting"));
        return ER_BUS_TRANSPORT_NOT_STARTED;
    }

    assert(m_dm);
    QStatus status = m_dm->AdvertiseName(advertiseName);

    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::EnableAdvertisment(%s) failure", advertiseName.c_str()));
    }
    return status;
}

void DaemonICETransport::DisableAdvertisement(const String& advertiseName, bool nameListEmpty)
{
    /*
     * We only want to allow this call to proceed if we have a running Run
     * thread that isn't in the process of shutting down.  We use the
     * thread response from IsRunning to give us an idea of what our server
     * accept (Run) thread is doing.  See the comment in Start() for details
     * about what IsRunning actually means, which might be subtly different from
     * your intuitition.
     *
     * If we see IsRunning(), the thread might actually have gotten a Stop(),
     * but has not yet exited its Run routine and become STOPPING.  To plug this
     * hole, we need to check IsRunning() and also m_stopping, which is set in
     * our Stop() method.
     */
    if (IsRunning() == false || m_stopping == true) {
        QCC_LogError(ER_BUS_TRANSPORT_NOT_STARTED, ("DaemonICETransport::DisableAdvertisement(): Not running or stopping; exiting"));
        return;
    }

    assert(m_dm);
    QStatus status = m_dm->CancelAdvertiseName(advertiseName);

    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::DisableAdvertisement(): Failure disabling advertising \"%s\" for ICE", advertiseName.c_str()));
    }
}

void DaemonICETransport::RecordIncomingICESessions(String guid)
{
    // We need not check if a similar entry already exists in IncomingICESessions before inserting the
    // details of this request in the same because it is absolutely valid to receive two independent connect
    // requests from the same client on the same daemon to this same service on this daemon.
    m_IncomingICESessionsLock.Lock(MUTEX_CONTEXT);
    IncomingICESessions.push_back(guid);
    m_IncomingICESessionsLock.Unlock(MUTEX_CONTEXT);

    // Wake up the DaemonICETransport::Run thread so that the new connection request can
    // be handled
    wakeDaemonICETransportRun.SetEvent();
}

void DaemonICETransport::PurgeSessionsMap(String peerID, const vector<String>* nameList)
{
    std::list<String>::iterator it;

    //
    // If the nameList is empty delete all the entries corresponding to GUID=peerID
    // else delete service entries corresponding to the service names in nameList and with
    // GUID=peerID
    //
    if (nameList == NULL) {
        QCC_DbgPrintf(("DaemonICETransport::PurgeSessionsMap(): nameList is empty"));

        m_IncomingICESessionsLock.Lock(MUTEX_CONTEXT);
        if (!IncomingICESessions.empty()) {
            for (it = IncomingICESessions.begin(); it != IncomingICESessions.end();) {

                if (*it == peerID) {
                    IncomingICESessions.erase(it++);
                } else {
                    ++it;
                }
            }
        }
        m_IncomingICESessionsLock.Unlock(MUTEX_CONTEXT);
    }
}

void DaemonICETransport::ICECallback::ICE(ajn::DiscoveryManager::CallbackType cbType, const String& guid, const vector<String>* nameList, uint8_t ttl)
{
    /*
     * Whenever the Discovery Manager receives a message indicating that a bus-name
     * is out on the network somewhere, it sends a message back to us via this
     * callback.  In order to avoid duplication of effort, the Discovery Manager does
     * not manage a cache of names, but delegates that to the daemon having this
     * transport.  If timer is zero, it means that the bus names in the nameList
     * are no longer available and should be flushed out of the daemon name cache.
     *
     * The Discovery Manager does not have a cache and therefore cannot time out
     * entries, but also delegates that task to the daemon.
     *
     * Our job here is just to pass the messages on up the stack to the daemon.
     *
     */

    //
    // Use "ice:" in place of BusAddr for ICE transport like "local:"
    // is used for local advertisements
    //
    String busAddr = "ice:";

    assert(m_daemonICETransport->m_dm);

    if (m_listener) {
        if (cbType == m_daemonICETransport->m_dm->FOUND) {
            busAddr = busAddr + "guid=" + guid;
            m_listener->FoundNames(busAddr, guid, TRANSPORT_ICE, nameList, ttl);

            //
            // If ttl has been set to 0, it means that the found callback has been
            // invoked to purge the nameMap. We need to purge OutgoingICESessions and
            // IncomingICESessions
            //
            if (ttl == 0) {
                m_daemonICETransport->PurgeSessionsMap(guid, nameList);
            }
        } else if (cbType == m_daemonICETransport->m_dm->ALLOCATE_ICE_SESSION) {
            m_daemonICETransport->RecordIncomingICESessions(guid);
        }
    }
}

bool DaemonICETransport::CheckTURNTokenExpiry(STUNServerInfo stunInfo)
{
    QCC_DbgPrintf(("DaemonICETransport::CheckTURNTokenExpiry()"));

    uint32_t tNow = GetTimestamp();

    if ((tNow - stunInfo.recvTime) >= stunInfo.expiryTime) {
        QCC_DbgPrintf(("DaemonICETransport::CheckTURNTokenExpiry(): Tokens expired"));
        return false;
    }

    QCC_DbgPrintf(("DaemonICETransport::CheckTURNTokenExpiry(): Tokens have not expired"));
    return true;
}

QStatus DaemonICETransport::GetNewTokensFromServer(bool client, STUNServerInfo& stunInfo, String remotePeerAddress)
{
    QCC_DbgPrintf(("DaemonICETransport::GetNewTokensFromServer()"));

    TokenRefreshListenerImpl tokenRefreshListener;
    TokenRefreshMessage refreshMessage;

    refreshMessage.client = client;
    refreshMessage.remotePeerAddress = remotePeerAddress;
    refreshMessage.tokenRefreshListener = &tokenRefreshListener;

    m_dm->ComposeAndQueueTokenRefreshMessage(refreshMessage);

    QStatus status = tokenRefreshListener.Wait();

    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonICETransport::GetNewTokensFromServer(): tokenRefreshListener wait failed"));
    } else {
        QCC_DbgPrintf(("DaemonICETransport::GetNewTokensFromServer(): Returned from tokenRefreshListener wait"));

        QCC_DbgPrintf(("DaemonICETransport::GetNewTokensFromServer(): Before: acct=%s, pwd=%s, recvTime=%d, expiryTime=%d",
                       stunInfo.acct.c_str(), stunInfo.pwd.c_str(), stunInfo.recvTime, stunInfo.expiryTime));

        tokenRefreshListener.GetTokens(stunInfo.acct, stunInfo.pwd, stunInfo.recvTime, stunInfo.expiryTime);

        QCC_DbgPrintf(("DaemonICETransport::GetNewTokensFromServer(): After: acct=%s, pwd=%s, recvTime=%d, expiryTime=%d",
                       stunInfo.acct.c_str(), stunInfo.pwd.c_str(), stunInfo.recvTime, stunInfo.expiryTime));
    }

    return status;
}

void DaemonICETransport::AlarmTriggered(const qcc::Alarm& alarm, QStatus alarmStatus)
{
    QCC_DbgPrintf(("DaemonICETransport::AlarmTriggered()"));

    AlarmContext* ctx = reinterpret_cast<AlarmContext*>(alarm.GetContext());

    switch (ctx->contextType) {
    case AlarmContext::CONTEXT_NAT_KEEPALIVE:
    {
        ICEPacketStream* ps = ctx->pktStream;

        /* Make sure PacketStream is still alive before calling nat/refresh code */
        QStatus status = AcquireICEPacketStreamByPointer(ps);

        if ((status == ER_OK) && (alarm == ps->GetTimeoutAlarm())) {
            /* PacketEngine Accept timeout */
            QCC_DbgPrintf(("DaemonICETransport::AlarmTriggered: Removing pktStream %p due to PacketEngine accept timeout", ps));
            ReleaseICEPacketStream(*ps);
            ReleaseICEPacketStream(*ps);
        } else if (status == ER_OK) {
            /* Send NAT keep alive and/or turn refresh */
            SendSTUNKeepAliveAndTURNRefreshRequest(*ps);
            ReleaseICEPacketStream(*ps);
        } else {
            /* Cant find pktStream */
            QCC_DbgPrintf(("DaemonICETransport::AlarmTriggered: PktStream=%p was not found. keepalive/refresh timer disabled for this pktStream", ps));
        }
        break;
    }

    case AlarmContext::CONTEXT_SCHEDULE_RUN:
    {
        /* Wake up the DaemonICETransport::Run() thread to purge the endpoints*/
        wakeDaemonICETransportRun.SetEvent();

        /* Reload the alarm */
        AlarmContext* ctx = new AlarmContext();
        Alarm runAlarm(DAEMON_ICE_TRANSPORT_RUN_SCHEDULING_INTERVAL, this, 0, ctx);
        daemonICETransportTimer.AddAlarm(runAlarm);

        break;
    }

    default:
    {
        uint32_t t = static_cast<uint32_t>(ctx->contextType);
        QCC_LogError(ER_FAIL, ("Received AlarmContext with unknown type (%u)", t));
        break;
    }
    }
}

ICEPacketStream* DaemonICETransport::AcquireICEPacketStream(const String& connectSpec)
{
    ICEPacketStream* ret = NULL;
    pktStreamMapLock.Lock(MUTEX_CONTEXT);
    map<String, pair<ICEPacketStream, int32_t> >::iterator it = pktStreamMap.find(connectSpec);
    if (it != pktStreamMap.end()) {
        it->second.second++;
        QCC_DbgPrintf(("%s: Acquired packet stream %s refCount=%d", __FUNCTION__, connectSpec.c_str(), it->second.second));
        ret = &(it->second.first);
    }
    pktStreamMapLock.Unlock(MUTEX_CONTEXT);
    return ret;
}

QStatus DaemonICETransport::AcquireICEPacketStreamByPointer(ICEPacketStream* icePktStream)
{
    QStatus status = ER_FAIL;
    pktStreamMapLock.Lock(MUTEX_CONTEXT);
    map<String, pair<ICEPacketStream, int32_t> >::iterator it = pktStreamMap.begin();
    while (it != pktStreamMap.end()) {
        if (icePktStream == &(it->second.first)) {
            it->second.second++;
            QCC_DbgPrintf(("%s: Acquired packet stream refCount=%d", __FUNCTION__, it->second.second));
            status = ER_OK;
            break;
        }
        ++it;
    }
    pktStreamMapLock.Unlock(MUTEX_CONTEXT);
    return status;
}

void DaemonICETransport::ReleaseICEPacketStream(const ICEPacketStream& icePktStream)
{
    QCC_DbgTrace(("DaemonICETransport::ReleaseICEPacketStream(%p)", &icePktStream));

    pktStreamMapLock.Lock(MUTEX_CONTEXT);
    bool found = false;
    map<String, pair<ICEPacketStream, int32_t> >::iterator it = pktStreamMap.begin();
    while (it != pktStreamMap.end()) {
        if (&icePktStream == &(it->second.first)) {
            --it->second.second;
            QCC_DbgPrintf(("%s: Releasing packet stream %p refCount=%d", __FUNCTION__, &icePktStream, it->second.second));
            if (it->second.second <= 0) {
                QStatus status = m_packetEngine.RemovePacketStream(it->second.first);
                if (status != ER_OK) {
                    QCC_LogError(status, ("RemovePacketStream failed"));
                }
                pktStreamMap.erase(it);
            }
            found = true;
            break;
        }
        ++it;
    }
    pktStreamMapLock.Unlock(MUTEX_CONTEXT);
    if (!found) {
        QCC_LogError(ER_FAIL, ("%s: Cannot find icePacketStream=%p", __FUNCTION__, &icePktStream));
    }
}

} // namespace ajn
