#ifndef _ALLJOYN_DBUSSTD_H
#define _ALLJOYN_DBUSSTD_H

/**
 * @file
 * This file provides definitions for standard DBus interfaces
 *
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

#include <qcc/platform.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/InterfaceDescription.h>

#include <Status.h>

namespace ajn {
namespace org {
namespace freedesktop {

/** Interface Definitions for org.freedesktop.DBus */
namespace DBus {
extern const char* ObjectPath;                         /**< Object path */
extern const char* InterfaceName;                      /**< Name of the interface */
extern const char* WellKnownName;                      /**< The well known name */

extern const char* AnnotateNoReply;                    /**< Annotation for reply to a method call */
extern const char* AnnotateDeprecated;                 /**< Annotation for marking entry as depreciated  */

/** Definitions for org.freedesktop.DBus.Properties */
namespace Properties {
extern const char* InterfaceName;                          /**< Name of the interface   */
}

/** Definitions for org.freedesktop.DBus.Peer */
namespace Peer {
extern const char* InterfaceName;                          /**< Name of the interface   */
}

/** Definitions for org.freedesktop.DBus.Introspectable */
namespace Introspectable {
extern const char* InterfaceName;                         /**< Name of the interface   */
extern const char* IntrospectDocType;                     /**< Type of introspection document */
}

/** Create the org.freedesktop.DBus interfaces and sub-interfaces */
QStatus CreateInterfaces(BusAttachment& bus);

}
}
}
/**
 * @name DBus RequestName input params
 * org.freedesktop.DBus.RequestName input params (see DBus spec)
 */
// @{
#define DBUS_NAME_FLAG_ALLOW_REPLACEMENT 0x01     /**< RequestName input flag: Allow others to take ownership of this name */
#define DBUS_NAME_FLAG_REPLACE_EXISTING  0x02     /**< RequestName input flag: Attempt to take ownership of name if already taken */
#define DBUS_NAME_FLAG_DO_NOT_QUEUE      0x04     /**< RequestName input flag: Fail if name cannot be immediately obtained */
// @}
/**
 * @name DBus RequestName return values
 * org.freedesktop.DBUs.RequestName return values (see DBus spec)
 */
// @{
#define DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER   1   /**< RequestName reply: Name was successfully obtained */
#define DBUS_REQUEST_NAME_REPLY_IN_QUEUE        2   /**< RequestName reply: Name is already owned, request for name has been queued */
#define DBUS_REQUEST_NAME_REPLY_EXISTS          3   /**< RequestName reply: Name is already owned and DO_NOT_QUEUE was specified in request */
#define DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER   4   /**< RequestName reply: Name is already owned by this endpoint */
// @}

/**
 * @name DBus ReleaaseName return values
 * org.freedesktop.DBus.ReleaseName return values (see DBus spec)
 */
// @{
#define DBUS_RELEASE_NAME_REPLY_RELEASED      1     /**< ReleaseName reply: Name was released */
#define DBUS_RELEASE_NAME_REPLY_NON_EXISTENT  2     /**< ReleaseName reply: Name does not exist */
#define DBUS_RELEASE_NAME_REPLY_NOT_OWNER     3     /**< ReleaseName reply: Request to release name that is not owned by this endpoint */
// @}
/**
 * @name DBus StartServiceByName return values
 * org.freedesktop.DBus.StartService return values (see DBus spec)
 */
// @{
#define DBUS_START_REPLY_SUCCESS          1         /**< StartServiceByName reply: Service is started */
#define DBUS_START_REPLY_ALREADY_RUNNING  2         /**< StartServiceByName reply: Service is already running */
// @}
}

#endif
