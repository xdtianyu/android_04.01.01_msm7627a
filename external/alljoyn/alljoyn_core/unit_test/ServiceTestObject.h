/**
 * @file
 * Implementation of a service object.
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

#include <signal.h>
#include <assert.h>
#include <stdio.h>
#include <vector>

#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/Environ.h>
#include <qcc/Util.h>
#include <qcc/Thread.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/MsgArg.h>
#include <alljoyn/version.h>

#include <Status.h>

#define SUCCESS 1

using namespace std;
using namespace qcc;
using namespace ajn;


class ServiceTestObject : public BusObject {
  public:

    ServiceTestObject(BusAttachment& bus, const char*);
    void NameAcquiredSignalHandler(const InterfaceDescription::Member*member, const char*, Message& msg);
    void ObjectRegistered(void);
    void ByteArrayTest(const InterfaceDescription::Member* member, Message& msg);
    void DoubleArrayTest(const InterfaceDescription::Member* member, Message& msg);
    void Ping(const InterfaceDescription::Member* member, Message& msg);
    void Sing(const InterfaceDescription::Member* member, Message& msg);
    void King(const InterfaceDescription::Member* member, Message& msg);
    QStatus Get(const char*ifcName, const char*propName, MsgArg& val);
    QStatus Set(const char*ifcName, const char*propName, MsgArg& val);
    int getOutput() { return output1; }
    int setOutput(int value) {  output1 = value; return output1; }
    void RegisterForNameAcquiredSignals();
    void PopulateSignalMembers(const char*);
    QStatus InstallMethodHandlers(const char*);
    QStatus AddInterfaceToObject(const InterfaceDescription*);

    const InterfaceDescription::Member* my_signal_member;
    qcc::String prop_str_val;
    qcc::String prop_ro_str;
    int32_t prop_int_val;
    int output1;
};
