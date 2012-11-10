/**
 * @file
 * NullTransport is the transport mechanism for bundled daemons.
 */

/******************************************************************************
 * Copyright 2012, Qualcomm Innovation Center, Inc.
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

#ifndef _ALLJOYN_NULLTRANSPORT_H
#define _ALLJOYN_NULLTRANSPORT_H

#ifndef __cplusplus
#error Only include NullTransport.h in C++ code.
#endif

#include <Status.h>

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/Mutex.h>
#include <qcc/Socket.h>
#include <qcc/Thread.h>
#include <qcc/SocketStream.h>
#include <qcc/time.h>

#include <alljoyn/BusAttachment.h>

#include "Transport.h"
#include "BusEndpoint.h"

namespace ajn {

class NullTransport;

/**
 * Class for launching a bundled daemon.
 */
struct DaemonLauncher {

    virtual QStatus Start(NullTransport* nullTransport) = 0;

    virtual QStatus Stop() = 0;

    virtual void Join() = 0;

    virtual ~DaemonLauncher() { }
};

/**
 * @brief A class for communicating from a client to a bundled daemon
 *
 * The NullTransport class has different incarnations depending on the platform.
 */
class NullTransport : public Transport {

  public:
    /**
     * Create a NullTransport
     *
     * @param bus The BusAttachment associated with this endpoint
     */
    NullTransport(BusAttachment& bus);

    /**
     * Destructor
     */
    virtual ~NullTransport();

    /**
     * Start the transport and associate it with a router.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Start();

    /**
     * Stop the transport.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Stop();

    /**
     * Pend the caller until the transport stops.
     *
     * @return
     *      - ER_OK if successful
     *      - an error status otherwise.
     */
    QStatus Join();

    /**
     * Determine if this transport is running. Running means Start() has been called.
     *
     * @return  Returns true if the transport is running.
     */
    bool IsRunning() { return running; }

    /**
     * Normalize a transport specification.
     * Given a transport specification, convert it into a form which is guaranteed to have a one-to-one
     * relationship with a transport.
     *
     * @param inSpec    Input transport connect spec.
     * @param outSpec   Output transport connect spec.
     * @param argMap    Parsed parameter map.
     *
     * @return ER_OK if successful.
     */
    QStatus NormalizeTransportSpec(const char* inSpec, qcc::String& outSpec, std::map<qcc::String, qcc::String>& argMap) const;

    /**
     * Connect to the bundled daemon
     *
     * @param connectSpec    Transport specific key/value args used to configure the client-side endpoint.
     *                       The form of this string is @c "<transport>:<key1>=<val1>,<key2>=<val2>..."
     * @param opts           Requested sessions opts.
     * @param newep          [OUT] Endpoint created as a result of successful connect.
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Connect(const char* connectSpec, const SessionOpts& opts, BusEndpoint** newep);

    /**
     * Disconnect from the bundled daemon.
     *
     * @param connectSpec    The connectSpec used in Connect.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Disconnect(const char* connectSpec);

    /**
     * The null transport emits so no events so this is a no-op.
     *
     * @param listener  Listener for transport related events.
     */
    void SetListener(TransportListener* listener) { }

    /**
     * Returns the name of this transport
     */
    const char* GetTransportName() const { return TransportName; }

    /**
     * Get the transport mask for this transport
     *
     * @return the TransportMask for this transport.
     */
    TransportMask GetTransportMask() const { return 0; }

    /**
     * Indicates whether this transport is used for client-to-bus or bus-to-bus connections.
     *
     * @return  Always returns false, NullTransports are only used to connect to a local daemon.
     */
    bool IsBusToBus() const { return false; }

    /**
     * Link the daemon bus to the client bus
     */
    QStatus LinkBus(BusAttachment* otherBus);

    /**
     * If there is a bundled daemon it will call in to register a launcher with the
     * null transport. The bundled daemon is launched the first time a null transport
     * is connected.
     */
    static void RegisterDaemonLauncher(DaemonLauncher* launcher);

    /**
     * The null transport is only available if the application has been linked with bundled daemon
     * support. Check if the null transport is available.
     *
     * @return  Returns true if the null transport is available.
     */
    static bool IsAvailable() { return daemonLauncher != NULL; }

    /**
     * Name of transport used in transport specs.
     */
    static const char* TransportName;

  private:
    BusAttachment& bus;           /**< The message bus for this transport */
    bool running;                 /**< True after Start() has been called, before Stop() */
    BusEndpoint* endpoint;        /**< The active endpoint */
    BusAttachment* daemonBus;     /**< The daemon bus attachment if the a bundled daemon was launched */

    static DaemonLauncher* daemonLauncher; /**< The daemon launcher if there is bundled daemon present */
};

} // namespace ajn

#endif // _ALLJOYN_NULLTRANSPORT_H
