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
#include <gtest/gtest.h>
#include "ajTestCommon.h"
#include <alljoyn/Message.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/DBusStd.h>
#include <qcc/Thread.h>

using namespace ajn;
using namespace qcc;

/*constants*/
static const char* INTERFACE_NAME = "org.alljoyn.test.ProxyBusObjectTest";
static const char* OBJECT_NAME =    "org.alljoyn.test.ProxyBusObjectTest";
static const char* OBJECT_PATH =   "/org/alljoyn/test/ProxyObjectTest";

class ProxyBusObjectTestMethodHandlers {
  public:
    static void Ping(const InterfaceDescription::Member* member, Message& msg)
    {

    }

    static void Chirp(const InterfaceDescription::Member* member, Message& msg)
    {

    }
};

class ProxyBusObjectTest : public testing::Test {
  public:
    ProxyBusObjectTest() :
        bus("ProxyBusObjectTest", false),
        servicebus("ProxyBusObjectTestservice", false)
    { };

    virtual void SetUp() {
        status = bus.Start();
        EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
        status = bus.Connect(ajn::getConnectArg().c_str());
        EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    }

//    virtual void TearDown() {
//    }

    void SetUpProxyBusObjectTestService()
    {
        buslistener.name_owner_changed_flag = false;
        /* create/start/connect alljoyn_busattachment */
        status = servicebus.Start();
        EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
        status = servicebus.Connect(ajn::getConnectArg().c_str());
        EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

        /* create/activate alljoyn_interface */
        InterfaceDescription* testIntf = NULL;
        status = servicebus.CreateInterface(INTERFACE_NAME, testIntf, false);
        EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
        status = testIntf->AddMember(MESSAGE_METHOD_CALL, "ping", "s", "s", "in,out", 0);
        EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
        status = testIntf->AddMember(MESSAGE_METHOD_CALL, "chirp", "s", "", "chirp", 0);
        EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
        testIntf->Activate();

        servicebus.RegisterBusListener(buslistener);

        ProxyBusObjectTestBusObject testObj(servicebus, OBJECT_PATH);

        const InterfaceDescription* exampleIntf = servicebus.GetInterface(INTERFACE_NAME);
        ASSERT_TRUE(exampleIntf);

        testObj.SetUp(*exampleIntf);

        status = servicebus.RegisterBusObject(testObj);
        EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
        buslistener.name_owner_changed_flag = false; //make sure the flag is false

        /* request name */
        uint32_t flags = DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE;
        status = servicebus.RequestName(OBJECT_NAME, flags);
        EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
        for (size_t i = 0; i < 200; ++i) {
            if (buslistener.name_owner_changed_flag) {
                break;
            }
            qcc::Sleep(5);
        }
        EXPECT_TRUE(buslistener.name_owner_changed_flag);
    }

    void TearDownProxyBusObjectTestService()
    {
//        alljoyn_busattachment_unregisterbuslistener(servicebus, buslistener);
    }

    class ProxyBusObjectTestBusListener : public BusListener {
      public:
        ProxyBusObjectTestBusListener() : name_owner_changed_flag(false) { };
        void    NameOwnerChanged(const char*busName, const char*previousOwner, const char*newOwner) {
            if (strcmp(busName, OBJECT_NAME) == 0) {
                name_owner_changed_flag = true;
            }
        }

        bool name_owner_changed_flag;
    };

    class ProxyBusObjectTestBusObject : public BusObject {
      public:
        ProxyBusObjectTestBusObject(BusAttachment& bus, const char* path) :
            BusObject(bus, path)
        {

        }

        void SetUp(const InterfaceDescription& intf)
        {
            QStatus status = ER_OK;
            status = AddInterface(intf);
            EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

            /* register method handlers */
            const InterfaceDescription::Member* ping_member = intf.GetMember("ping");
            ASSERT_TRUE(ping_member);

            const InterfaceDescription::Member* chirp_member = intf.GetMember("chirp");
            ASSERT_TRUE(chirp_member);

            /** Register the method handlers with the object */
            const MethodEntry methodEntries[] = {
                { ping_member, static_cast<MessageReceiver::MethodHandler>(&ProxyBusObjectTest::ProxyBusObjectTestBusObject::Ping) },
                { chirp_member, static_cast<MessageReceiver::MethodHandler>(&ProxyBusObjectTest::ProxyBusObjectTestBusObject::Chirp) }
            };
            status = AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
            EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
        }

        void Ping(const InterfaceDescription::Member* member, Message& msg)
        {

        }

        void Chirp(const InterfaceDescription::Member* member, Message& msg)
        {

        }
    };

    QStatus status;
    BusAttachment bus;

    BusAttachment servicebus;

    ProxyBusObjectTestBusListener buslistener;
};


TEST_F(ProxyBusObjectTest, ParseXml) {
    const char* busObjectXML =
        "<node name=\"/org/alljoyn/test/ProxyObjectTest\">"
        "  <interface name=\"org.alljoyn.test.ProxyBusObjectTest\">\n"
        "    <signal name=\"chirp\">\n"
        "      <arg name=\"chirp\" type=\"s\"/>\n"
        "    </signal>\n"
        "    <signal name=\"chirp2\">\n"
        "      <arg name=\"chirp\" type=\"s\" direction=\"out\"/>\n"
        "    </signal>\n"
        "    <method name=\"ping\">\n"
        "      <arg name=\"in\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"out\" type=\"s\" direction=\"out\"/>\n"
        "    </method>\n"
        "  </interface>\n"
        "</node>\n";
    QStatus status;

    ProxyBusObject proxyObj(bus, NULL, NULL, 0);
    status = proxyObj.ParseXml(busObjectXML, NULL);
    EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    EXPECT_TRUE(proxyObj.ImplementsInterface("org.alljoyn.test.ProxyBusObjectTest"));

    const InterfaceDescription* testIntf = proxyObj.GetInterface("org.alljoyn.test.ProxyBusObjectTest");
    qcc::String introspect = testIntf->Introspect(0);

    const char* expectedIntrospect =
        "<interface name=\"org.alljoyn.test.ProxyBusObjectTest\">\n"
        "  <signal name=\"chirp\">\n"
        "    <arg name=\"chirp\" type=\"s\" direction=\"out\"/>\n"
        "  </signal>\n"
        "  <signal name=\"chirp2\">\n"
        "    <arg name=\"chirp\" type=\"s\" direction=\"out\"/>\n"
        "  </signal>\n"
        "  <method name=\"ping\">\n"
        "    <arg name=\"in\" type=\"s\" direction=\"in\"/>\n"
        "    <arg name=\"out\" type=\"s\" direction=\"out\"/>\n"
        "  </method>\n"
        "</interface>\n";
    EXPECT_STREQ(expectedIntrospect, introspect.c_str());
}
