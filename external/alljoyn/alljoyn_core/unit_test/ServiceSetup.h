/*******************************************************************************
 * Copyright 2011, Qualcomm Innovation Center, Inc.
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
#ifndef _TEST_ServiceSetup_H
#define _TEST_ServiceSetup_H

#include <signal.h>
#include <assert.h>
#include <stdio.h>
#include <vector>

#include <alljoyn/DBusStd.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/version.h>
#include <alljoyn/MsgArg.h>
#include <alljoyn/BusObject.h>

#include <qcc/platform.h>
#include <qcc/Environ.h>
#include <qcc/Util.h>
#include <qcc/Debug.h>
#include <qcc/String.h>
#include <Status.h>

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;
using namespace ajn;

/* Service Bus Listener */
class MyBusListener : public BusListener, public SessionPortListener {

  public:
    MyBusListener() : BusListener() { }

    bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts);

};

/* Auth Listener */
class MyAuthListener : public AuthListener {
    QStatus RequestPwd(const qcc::String& authMechanism, uint8_t minLen, qcc::String& pwd);
};

/* Old LocalTestObject class */
class ServiceObject : public BusObject {
  public:
    ServiceObject(BusAttachment& bus, const char*path);
    ~ServiceObject();

    // Service Object related API
    void ObjectRegistered();
    QStatus AddInterfaceToObject(const InterfaceDescription* intf);
    void PopulateSignalMembers();
    void NameAcquiredCB(Message& msg, void* context);
    void RequestName(const char* name);
    QStatus InstallMethodHandlers();
    void Ping(const InterfaceDescription::Member* member, Message& msg);
    void Sing(const InterfaceDescription::Member* member, Message& msg);
    void ParamTest(const InterfaceDescription::Member* member, Message& msg);
    QStatus EmitTestSignal(String newName);
    QStatus Get(const char*ifcName, const char*propName, MsgArg& val);
    QStatus Set(const char*ifcName, const char*propName, MsgArg& val);
    bool getobjectRegistered();
    void setobjectRegistered(bool value);

    const char* getAlljoynInterfaceName() const;
    const char* getServiceInterfaceName() const;
    const char* getAlljoynWellKnownName() const;
    const char* getServiceWellKnownName() const;
    const char* getAlljoynObjectPath() const;
    const char* getServiceObjectPath() const;
    const char* getAlljoynValuesInterfaceName() const;
    const char* getServiceValuesInterfaceName() const;

    const char* getAlljoynDummyInterfaceName1() const;
    const char* getServiceDummyInterfaceName1() const;
    const char* getAlljoynDummyInterfaceName2() const;
    const char* getServiceDummyInterfaceName2() const;
    const char* getAlljoynDummyInterfaceName3() const;
    const char* getServiceDummyInterfaceName3() const;

    const char* getAlljoynValuesDummyInterfaceName1() const;
    const char* getServiceValuesDummyInterfaceName1() const;
    const char* getAlljoynValuesDummyInterfaceName2() const;
    const char* getServiceValuesDummyInterfaceName2() const;
    const char* getAlljoynValuesDummyInterfaceName3() const;
    const char* getServiceValuesDummyInterfaceName3() const;

  private:

    qcc::String prop_str_val;
    qcc::String prop_ro_str;
    int32_t prop_int_val;

    qcc::String prop_signal;
    const InterfaceDescription::Member* my_signal_member;
    const InterfaceDescription::Member* my_signal_member_2;
    bool objectRegistered;

};

#endif
