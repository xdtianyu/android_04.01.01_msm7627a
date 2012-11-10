/**
 * @file
 *
 * This file tests AllJoyn use of the DBus wire protocol
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

#include <ctype.h>
#include <qcc/platform.h>

#include <qcc/Util.h>
#include <qcc/Debug.h>
#include <qcc/Pipe.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/Message.h>
#include <alljoyn/version.h>

#include <Status.h>

/* Private files included for unit testing */
#include <PeerState.h>
#include <SignatureUtils.h>
#include <RemoteEndpoint.h>

using namespace qcc;
using namespace std;
using namespace ajn;

static BusAttachment* gBus;

class MyMessage : public _Message {
  public:


    MyMessage() : _Message(*gBus) { };

    QStatus MethodCall(const char* destination,
                       const char* objPath,
                       const char* interface,
                       const char* methodName,
                       uint32_t& serial,
                       const MsgArg* argList,
                       size_t numArgs,
                       uint8_t flags = 0)
    {
        qcc::String sig = MsgArg::Signature(argList, numArgs);
        printf("Signature = \"%s\"\n", sig.c_str());
        return CallMsg(sig, destination, 0, objPath, interface, methodName, serial, argList, numArgs, flags);
    }

    QStatus Signal(const char* destination,
                   const char* objPath,
                   const char* interface,
                   const char* signalName,
                   const MsgArg* argList,
                   size_t numArgs)
    {
        qcc::String sig = MsgArg::Signature(argList, numArgs);
        printf("Signature = \"%s\"\n", sig.c_str());
        return SignalMsg(sig, destination, 0, objPath, interface, signalName, argList, numArgs, 0, 0);
    }

    QStatus UnmarshalBody() { return UnmarshalArgs("*"); }

    QStatus Unmarshal(RemoteEndpoint& ep, bool pedantic = true)
    {
        return _Message::Unmarshal(ep, pedantic);
    }

    QStatus ReMarshal(const char* senderName)
    {
        return _Message::ReMarshal(senderName, true);
    }

    QStatus Deliver(RemoteEndpoint& ep)
    {
        return _Message::Deliver(ep);
    }
};


/*
 * Marshal and unmarshal an arg list
 */
static QStatus TestRemarshal(const MsgArg* argList, size_t numArgs, const char* exception = NULL)
{
    QStatus status;
    Pipe stream;
    RemoteEndpoint ep(*gBus, false, "", &stream, "dummy");
    MyMessage msg;
    uint32_t serial;

    if (numArgs == 0) {
        printf("Empty arg.v_struct.Elements, arg.v_struct.numElements\n");
        return ER_FAIL;
    }

    printf("++++++++++++++++++++++++++++++++++++++++++++\n");
    qcc::String inargList = MsgArg::ToString(argList, numArgs);
    qcc::String inSig = MsgArg::Signature(argList, numArgs);
    printf("ArgList:\n%s", inargList.c_str());

    status = msg.MethodCall("desti.nation", "/foo/bar", "foo.bar", "test", serial, argList, numArgs);
    printf("MethodCall status:%s\n", QCC_StatusText(status));
    if (status != ER_OK) {
        return status;
    }
    status = msg.Deliver(ep);
    if (status != ER_OK) {
        return status;
    }

    status = msg.Unmarshal(ep);
    if (status != ER_OK) {
        printf("Message::Unmarshal status:%s\n", QCC_StatusText(status));
        return status;
    }
    status = msg.UnmarshalBody();
    if (status != ER_OK) {
        printf("Message::UnmarshalArgs status:%s\n", QCC_StatusText(status));
        return status;
    }

    msg.ReMarshal("from.sender");
    status = msg.Deliver(ep);
    if (status != ER_OK) {
        printf("Message::ReMarshal status:%s\n", QCC_StatusText(status));
        return status;
    }
    status = msg.Unmarshal(ep);
    if (status != ER_OK) {
        printf("Message::Unmarshal status:%s\n", QCC_StatusText(status));
        return status;
    }
    status = msg.UnmarshalBody();
    if (status != ER_OK) {
        printf("Message::UnmarshalArgs status:%s\n", QCC_StatusText(status));
        return status;
    }

    return status;
}

int main(int argc, char** argv)
{
    QStatus status = ER_OK;

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());

    gBus = new BusAttachment("remarshal");
    gBus->Start();

    struct {
        uint32_t num;
        const char* ord;
        bool even;
    } table[] = { { 1, "first", true }, { 2, "second", false }, { 3, "third", true } };
    MsgArg dict[ArraySize(table)];
    for (size_t i = 0; i < ArraySize(table); i++) {
        dict[i].Set("{s(ib)}", table[i].ord, table[i].num, table[i].even);
    }
    MsgArg arg;
    status = arg.Set("a{s(ib)}", 3, dict);
    if (status == ER_OK) {
        status = TestRemarshal(&arg, 1);
    }

    if (status == ER_OK) {
        QCC_SyncPrintf("\n PASSED ");
    } else {
        QCC_SyncPrintf("\n FAILED ");
    }
    printf("\n");

    return 0;
}
