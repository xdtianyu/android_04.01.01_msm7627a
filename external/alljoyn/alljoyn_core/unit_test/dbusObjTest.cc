/******************************************************************************
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
#include <qcc/platform.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <vector>

#include <qcc/String.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/MsgArg.h>
#include <alljoyn/version.h>

#include <Status.h>

/* Header files included for Google Test Framework */
#include <gtest/gtest.h>
#include "ajTestCommon.h"
//#include <qcc/time.h>

using namespace std;
using namespace qcc;
using namespace ajn;
class DBusObjTest : public testing::Test {
  public:
    BusAttachment bus;

    DBusObjTest() : bus("testDBusObj", false) { };

    virtual void SetUp() {
        QStatus status = ER_OK;
        status = bus.Start();
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
        status = bus.Connect(getConnectArg().c_str());
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    }

    //virtual void TearDown() {}

};

TEST_F(DBusObjTest, RequestName_CorrectName_Success)
{
    QStatus status = ER_OK;

    const char* requestedName = "org.alljoyn.myService";

    /* flag indicates that Fail if name cannot be immediatly obtained */
    status = bus.RequestName(requestedName, DBUS_NAME_FLAG_DO_NOT_QUEUE);
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    /* Cleanup */
    bus.ReleaseName(requestedName);
}

TEST_F(DBusObjTest, RequestName_TwoNames_Success)
{
    QStatus status = ER_OK;

    const char* requestedName1 = "org.alljoyn.myService1";
    const char* requestedName2 = "org.alljoyn.myService2";

    /* flag indicates - Fail if name cannot be immediatly obtained */
    status = bus.RequestName(requestedName1, DBUS_NAME_FLAG_DO_NOT_QUEUE);
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    status = bus.RequestName(requestedName2, DBUS_NAME_FLAG_DO_NOT_QUEUE);
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    /* Cleanup */
    bus.ReleaseName(requestedName1);
    bus.ReleaseName(requestedName2);
}

TEST_F(DBusObjTest, RequestName_DuplicateName_Fail)
{
    QStatus status = ER_OK;

    const char* requestedName1 = "org.alljoyn.myService2";
    const char* requestedName2 = "org.alljoyn.myService2";

    /* flag indicates - Fail if name cannot be immediatly obtained */
    status = bus.RequestName(requestedName1, DBUS_NAME_FLAG_DO_NOT_QUEUE);
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    status = bus.RequestName(requestedName2, DBUS_NAME_FLAG_DO_NOT_QUEUE);
    ASSERT_EQ(ER_DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER, status);

    /* Cleanup */
    bus.ReleaseName(requestedName1);
}

TEST_F(DBusObjTest, ListQueuedOwners) {
    QStatus status = ER_OK;
    QStatus testStatus = ER_OK;

    /* Create message bus */
    BusAttachment bus2("testDBusObj2", false);
    BusAttachment bus3("testDBusObj3", false);
    BusAttachment bus4("testDBusObj4", false);

    /* Start the msg bus2 */
    status = bus2.Start();
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    /* Create the client-side endpoint */
    status = bus2.Connect(getConnectArg().c_str());
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    /* Start the msg bus */
    status = bus3.Start();
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    /* Create the client-side endpoint */
    status = bus3.Connect(getConnectArg().c_str());
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    /* Start the msg bus */
    status = bus4.Start();
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    /* Create the client-side endpoint */
    status = bus4.Connect(getConnectArg().c_str());
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);


    ProxyBusObject dbusObj1(bus.GetDBusProxyObj());

    //Initialize variables used to make ListQueuedOwners method call
    MsgArg arg;
    status = arg.Set("s", "com.test.foo");
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    Message reply(bus);

    //Initialize variables that will hold the results of ListQueuedOwners method call
    MsgArg* asArray;
    size_t las;
    std::vector<char*> queuedNames;
    /*-----Calling ListQueuedOwners for name that is not on queue------*/
    /*
     * Check that No names are returned even when the name does not yet exist on
     * the bus
     */
    status = dbusObj1.MethodCall("org.freedesktop.DBus", "ListQueuedOwners", &arg, 1,  reply);
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    reply->GetArg(0)->Get("as", &las, &asArray);
    ASSERT_EQ(las, 0);
    queuedNames.clear();
    queuedNames.reserve(las);
    for (unsigned int i = 0; i < las; ++i) {
        status = asArray[i].Get("s", &queuedNames[i]);
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    }

    /* Request name */
    uint32_t flags = DBUS_NAME_FLAG_ALLOW_REPLACEMENT;
    status = bus.RequestName("com.test.foo", flags);
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);


    /*-----Calling ListQueuedOwners when a single name is on queue------*/
    /*
     * check that no names are returned when only the primary name exist on the bus
     */
    status = dbusObj1.MethodCall("org.freedesktop.DBus", "ListQueuedOwners", &arg, 1,  reply);
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    reply->GetArg(0)->Get("as", &las, &asArray);
    ASSERT_EQ(las, 0);
    queuedNames.clear();
    queuedNames.reserve(las);
    for (unsigned int i = 0; i < las; ++i) {
        status = asArray[i].Get("s", &queuedNames[i]);
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    }

    /*--Calling ListQueuedOwners when multiple names are on the queue---*/
    /* Request name (queue Name) */
    flags = 0;
    status = bus2.RequestName("com.test.foo", flags);
    ASSERT_EQ(ER_DBUS_REQUEST_NAME_REPLY_IN_QUEUE, status);

    /* Request name (queue Name)*/
    status = bus3.RequestName("com.test.foo", flags);
    ASSERT_EQ(ER_DBUS_REQUEST_NAME_REPLY_IN_QUEUE, status);

    /*
     * Check that there are now two unique names in queue
     */
    status = dbusObj1.MethodCall("org.freedesktop.DBus", "ListQueuedOwners", &arg, 1,  reply);
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    status = reply->GetArg(0)->Get("as", &las, &asArray);
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    ASSERT_EQ(2, las);
    queuedNames.clear();
    queuedNames.resize(las);
    for (unsigned int i = 0; i < las; ++i) {
        status = asArray[i].Get("s", &queuedNames[i]);
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    }

    EXPECT_STREQ(bus2.GetUniqueName().c_str(), queuedNames[0]);
    EXPECT_STREQ(bus3.GetUniqueName().c_str(), queuedNames[1]);

    /*----Calling ListQueuedOwners after adding name as primary owner---*/
    /* Request name (make primary owner)*/
    flags = DBUS_NAME_FLAG_REPLACE_EXISTING;
    status = bus4.RequestName("com.test.foo", flags);
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    status = dbusObj1.MethodCall("org.freedesktop.DBus", "ListQueuedOwners", &arg, 1,  reply);
    ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    reply->GetArg(0)->Get("as", &las, &asArray);
    ASSERT_EQ(3, las);
    queuedNames.clear();
    queuedNames.resize(las);
    for (unsigned int i = 0; i < las; ++i) {
        status = asArray[i].Get("s", &queuedNames[i]);
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    }

    EXPECT_STREQ(bus.GetUniqueName().c_str(), queuedNames[0]);
    EXPECT_STREQ(bus2.GetUniqueName().c_str(), queuedNames[1]);
    EXPECT_STREQ(bus3.GetUniqueName().c_str(), queuedNames[2]);

    bus.ReleaseName("com.test.foo");
    bus2.ReleaseName("com.test.foo");
    bus3.ReleaseName("com.test.foo");
    bus4.ReleaseName("com.test.foo");

    bus.Stop();
    bus2.Stop();
    bus3.Stop();
    bus4.Stop();
}
