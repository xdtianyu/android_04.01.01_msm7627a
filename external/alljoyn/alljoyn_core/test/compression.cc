/**
 * @file
 *
 * This file tests AllJoyn header compression.
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

#include <qcc/platform.h>

#include <qcc/Debug.h>
#include <qcc/Pipe.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>
#include <qcc/Thread.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/Message.h>
#include <alljoyn/version.h>

#include <Status.h>

/* Private files included for unit testing */
#include <RemoteEndpoint.h>


using namespace qcc;
using namespace std;
using namespace ajn;


class MyMessage : public _Message {
  public:

    MyMessage(BusAttachment& bus) : _Message(bus) { }


    QStatus MethodCall(const char* destination,
                       const char* objPath,
                       const char* interface,
                       const char* methodName,
                       uint32_t& serial,
                       uint8_t flags = 0)
    {
        flags |= ALLJOYN_FLAG_COMPRESSED;
        return CallMsg("", destination, 0, objPath, interface, methodName, serial, NULL, 0, flags);
    }

    QStatus Signal(const char* destination,
                   const char* objPath,
                   const char* interface,
                   const char* signalName,
                   uint16_t ttl,
                   uint32_t sessionId = 0)
    {
        return SignalMsg("", destination, sessionId, objPath, interface, signalName, NULL, 0, ALLJOYN_FLAG_COMPRESSED, ttl);
    }

    QStatus Unmarshal(RemoteEndpoint& ep, const qcc::String& endpointName, bool pedantic = true)
    {
        return _Message::Unmarshal(ep, pedantic);
    }

    QStatus Deliver(RemoteEndpoint& ep)
    {
        return _Message::Deliver(ep);
    }

};


int main(int argc, char** argv)
{
    QStatus status;
    uint32_t tok1;
    uint32_t tok2;
    BusAttachment bus("compression");
    uint32_t serial;
    MyMessage msg(bus);
    Pipe stream;
    RemoteEndpoint ep(bus, false, "", &stream, "dummy", false);

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());

    bus.Start();

    status = msg.MethodCall(":1.99", "/foo/bar", "foo.bar", "test", serial);
    if (status != ER_OK) {
        printf("Error %s\n", QCC_StatusText(status));
    }
    tok1 = msg.GetCompressionToken();

    status = msg.MethodCall(":1.99", "/foo/bar", "foo.bar", "test", serial);
    if (status != ER_OK) {
        printf("Error %s\n", QCC_StatusText(status));
    }
    tok2 = msg.GetCompressionToken();

    /* Expect same message to have same token */
    if (tok1 != tok2) {
        printf("\nFAILED 1\n");
        return -1;
    }

    status = msg.MethodCall(":1.98", "/foo/bar", "foo.bar", "test", serial);
    if (status != ER_OK) {
        printf("Error %s\n", QCC_StatusText(status));
    }
    tok2 = msg.GetCompressionToken();

    /* Expect different messages to have different token */
    if (tok1 == tok2) {
        printf("\nFAILED 2\n");
        return -1;
    }

    status = msg.Signal(":1.99", "/foo/bar/gorn", "foo.bar", "test", 0);
    if (status != ER_OK) {
        printf("Error %s\n", QCC_StatusText(status));
    }
    tok1 = msg.GetCompressionToken();

    status = msg.Signal(":1.99", "/foo/bar/gorn", "foo.bar", "test", 1000);
    if (status != ER_OK) {
        printf("Error %s\n", QCC_StatusText(status));
        return -1;
    }
    tok2 = msg.GetCompressionToken();

    /* Expect messages with and without TTL to have different token */
    if (tok1 == tok2) {
        printf("\nFAILED 3\n");
        return -1;
    }

    /* Expect messages with different TTLs to have different token */
    status = msg.Signal(":1.99", "/foo/bar/gorn", "foo.bar", "test", 9999);
    if (status != ER_OK) {
        printf("Error %s\n", QCC_StatusText(status));
        return -1;
    }
    tok1 = msg.GetCompressionToken();

    if (tok1 == tok2) {
        printf("\nFAILED 4\n");
        return -1;
    }

    /* Expect messages with same TTL but different timestamps to have same token */
    status = msg.Signal(":1.1234", "/foo/bar/again", "boo.far", "test", 1700);
    if (status != ER_OK) {
        printf("Error %s\n", QCC_StatusText(status));
        return -1;
    }
    tok1 = msg.GetCompressionToken();

    qcc::Sleep(5);

    status = msg.Signal(":1.1234", "/foo/bar/again", "boo.far", "test", 1700);
    if (status != ER_OK) {
        printf("Error %s\n", QCC_StatusText(status));
        return -1;
    }
    tok2 = msg.GetCompressionToken();

    if (tok1 != tok2) {
        printf("\nFAILED 5\n");
        return -1;
    }

    status = msg.Signal(":1.99", "/foo/bar/gorn", "foo.bar", "test", 0, 1234);
    if (status != ER_OK) {
        printf("Error %s\n", QCC_StatusText(status));
    }
    tok1 = msg.GetCompressionToken();

    status = msg.Signal(":1.99", "/foo/bar/gorn", "foo.bar", "test", 0, 5678);
    if (status != ER_OK) {
        printf("Error %s\n", QCC_StatusText(status));
        return -1;
    }
    tok2 = msg.GetCompressionToken();

    /* Expect messages with different session id's to have different token */
    if (tok1 == tok2) {
        printf("\nFAILED 3\n");
        return -1;
    }

    /* No do a real marshal unmarshal round trip */
    for (int i = 0; i < 20; ++i) {
        SessionId sess = 1000 + i % 3;
        qcc::String sig = "test" + qcc::U32ToString(i);
        status = msg.Signal(":1.1234", "/fun/games", "boo.far", sig.c_str(), 1900, sess);
        if (status != ER_OK) {
            printf("Error %s\n", QCC_StatusText(status));
            return -1;
        }
        status = msg.Deliver(ep);
        if (status != ER_OK) {
            printf("Error %s\n", QCC_StatusText(status));
            return -1;
        }
    }

    for (int i = 0; i < 20; ++i) {
        SessionId sess = 1000 + i % 3;
        qcc::String sig = "test" + qcc::U32ToString(i);
        MyMessage msg2(bus);
        status = msg2.Unmarshal(ep, ":88.88");
        if (status != ER_OK) {
            printf("Error %s\n", QCC_StatusText(status));
            return -1;
        }
        if (msg2.GetSessionId() != sess) {
            printf("\nFAILED 6.%d\n", i);
            return -1;
        }
        if (sig != msg2.GetMemberName()) {
            printf("\nFAILED 6.%d\n", i);
            return -1;
        }
    }

    printf("PASSED\n");

    return 0;
}
