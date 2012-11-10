/**
 * @file Bus is the top-level object responsible for implementing the message
 * bus.
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
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
#ifndef _ALLJOYN_BUS_H
#define _ALLJOYN_BUS_H

#ifndef __cplusplus
#error Only include Bus.h in C++ code.
#endif

#include <qcc/platform.h>

#include <list>

#include <qcc/String.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusListener.h>

#include <Status.h>

#include "TransportList.h"
#include "DaemonRouter.h"
#include "BusInternal.h"

namespace ajn {

class Bus : public BusAttachment, public NameListener {
  public:
    /**
     * Construct a Bus.
     *
     * @param applicationName   Name of the application.
     *
     * @param factories         A container of transport factories that knows
     *                          how to create the individual transports.  Some
     *                          transports may be defined as default, in which
     *                          case they are automatically instantiated.  If
     *                          not, they are instantiated based on the listen
     *                          spec if present.
     *
     * @param listenSpecs       A semicolon separated list of bus addresses that this daemon is capable of listening on.
     */
    Bus(const char* applicationName, TransportFactoryContainer& factories, const char* listenSpecs = NULL);

    /**
     * Listen for incoming AllJoyn connections on a given transport address.
     *
     * This method will fail if the BusAttachment instance is a strictly for client-side use.
     *
     * @param listenSpecs     A list of transport connection spec strings each of the form:
     *                        @c \<transport\>:\<param1\>=\<value1\>,\<param2\>=\<value2\>...
     *
     * @return
     *      - ER_OK if successful
     *      - ER_BUS_NO_TRANSPORTS if transport can not be found to listen for.
     */
    QStatus StartListen(const char* listenSpecs);

    /**
     * Stop listening for incomming AllJoyn connections on a given transport address.
     *
     * @param listenSpecs  A transport connection spec string of the form:
     *                     @c \<transport\>:\<param1\>=\<value1\>,\<param2\>=\<value2\>...[;]
     *
     * @return ER_OK
     */
    QStatus StopListen(const char* listenSpecs);

    /**
     * Get addresses that can be used by applications running on the same
     * machine (i.e., unix: and tcp:).
     *
     * @return  Local bus addresses in standard DBus address notation
     */
    const qcc::String& GetLocalAddresses() const { return localAddrs; }


    /**
     * Get addresses that can be used by applications running on other
     * machines (i.e., tcp: and bluetooth:).
     *
     * @return  External bus addresses in standard DBus address notation
     */
    const qcc::String& GetExternalAddresses() const { return externalAddrs; }


    /**
     * Get all unique names and their exportable alias (well-known) names.
     *
     * @param  nameVec   Vector of (uniqueName, aliases) pairs where aliases is a vector of alias names.
     */
    void GetUniqueNamesAndAliases(std::vector<std::pair<qcc::String, std::vector<qcc::String> > >& nameVec) const
    {
        reinterpret_cast<const DaemonRouter&>(GetInternal().GetRouter()).GetUniqueNamesAndAliases(nameVec);
    }

    /**
     * Register an object that will receive bus event notifications.
     *
     * @param listener  Object instance that will receive bus event notifications.
     */
    void RegisterBusListener(BusListener& listener);

    /**
     * Unregister an object that was previously registered as a BusListener
     *
     * @param listener  BusListener to be un registered.
     */
    void UnregisterBusListener(BusListener& listener);

  private:

    /**
     * Forwards name owner changed events from the name table to a registered bus listener.
     */
    void NameOwnerChanged(const qcc::String& alias, const qcc::String* oldOwner, const qcc::String* newOwner);

    /**
     * Listen for incomming AllJoyn connections on a single transport address.
     *
     * @param listenSpec      A transport connection spec string of the form:
     *                        @c \<transport\>:\<param1\>=\<value1\>,\<param2\>=\<value2\>...[;]
     *
     * @param listening       [OUT] true if started listening on specified transport.
     *
     * @return
     *      - ER_OK if successful
     *      - ER_BUS_NO_TRANSPORTS if transport can not be found to listen for.
     */
    QStatus StartListen(const qcc::String& listenSpec, bool& listening);

    qcc::String localAddrs;      ///< Bus Addresses locally accessable
    qcc::String externalAddrs;   ///< Bus Addresses externall accessable

    BusListener* busListener;
};

}
#endif
