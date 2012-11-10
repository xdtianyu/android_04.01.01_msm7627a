/*
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
 */

#include <alljoyn/BusAttachment.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <qcc/Log.h>
#include <qcc/String.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>

using namespace ajn;

/* constants */
static const char* CHAT_SERVICE_INTERFACE_NAME = "org.alljoyn.bus.samples.chat";
static const char* NAME_PREFIX = "org.alljoyn.bus.samples.chat.";
static const char* CHAT_SERVICE_OBJECT_PATH = "/chatService";
static const SessionPort CHAT_PORT = 27;

/* forward declaration */
class ChatObject;
class MyBusListener;

/* static data */
static ajn::BusAttachment* s_bus = NULL;
static ChatObject* s_chatObj = NULL;
static MyBusListener* s_busListener = NULL;
static qcc::String s_advertisedName;
static qcc::String s_joinName;
static SessionId s_sessionId = 0;
static bool s_joinComplete = false;

/*
 * get a line of input from the the file pointer (most likely stdin).
 * This will capture the the num-1 characters or till a newline character is
 * entered.
 *
 * @param[out] str a pointer to a character array that will hold the user input
 * @param[in]  num the size of the character array 'str'
 * @param[in]  fp  the file pointer the sting will be read from. (most likely stdin)
 *
 * @return returns the same string as 'str' if there has been a read error a null
 *                 pointer will be returned and 'str' will remain unchanged.
 */
char*get_line(char*str, size_t num, FILE*fp)
{
    char*p = fgets(str, num, fp);

    // fgets will capture the '\n' character if the string entered is shorter than
    // num. Remove the '\n' from the end of the line and replace it with nul '\0'.
    if (p != NULL) {
        size_t last = strlen(str) - 1;
        if (str[last] == '\n') {
            str[last] = '\0';
        }
    }
    return p;
}

/* Bus object */
class ChatObject : public BusObject {
  public:

    ChatObject(BusAttachment& bus, const char* path) : BusObject(bus, path), chatSignalMember(NULL)
    {
        QStatus status;

        /* Add the chat interface to this object */
        const InterfaceDescription* chatIntf = bus.GetInterface(CHAT_SERVICE_INTERFACE_NAME);
        assert(chatIntf);
        AddInterface(*chatIntf);

        /* Store the Chat signal member away so it can be quickly looked up when signals are sent */
        chatSignalMember = chatIntf->GetMember("Chat");
        assert(chatSignalMember);

        /* Register signal handler */
        status =  bus.RegisterSignalHandler(this,
                                            static_cast<MessageReceiver::SignalHandler>(&ChatObject::ChatSignalHandler),
                                            chatSignalMember,
                                            NULL);

        if (ER_OK != status) {
            printf("Failed to register signal handler for ChatObject::Chat (%s)\n", QCC_StatusText(status));
        }
    }

    /** Send a Chat signal */
    QStatus SendChatSignal(const char* msg) {

        MsgArg chatArg("s", msg);
        uint8_t flags = 0;
        if (0 == s_sessionId) {
            printf("Sending Chat signal without a session id\n");
        }
        return Signal(NULL, s_sessionId, *chatSignalMember, &chatArg, 1, 0, flags);
    }

    /** Receive a signal from another Chat client */
    void ChatSignalHandler(const InterfaceDescription::Member* member, const char* srcPath, Message& msg)
    {
        printf("%s: %s\n", msg->GetSender(), msg->GetArg(0)->v_string.str);
    }

  private:
    const InterfaceDescription::Member* chatSignalMember;
};

class MyBusListener : public BusListener, public SessionPortListener, public SessionListener {
    void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
    {
        const char* convName = name + strlen(NAME_PREFIX);
        printf("Discovered chat conversation: \"%s\"\n", convName);

        /* Join the conversation */
        SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
        QStatus status = s_bus->JoinSession(name, CHAT_PORT, this, s_sessionId, opts);
        if (ER_OK == status) {
            printf("Joined conversation \"%s\"\n", convName);
        } else {
            printf("JoinSession failed (status=%s)\n", QCC_StatusText(status));
        }
        s_joinComplete = true;
    }
    void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
        printf("NameOwnerChanged: name=%s, oldOwner=%s, newOwner=%s\n", busName, previousOwner ? previousOwner : "<none>",
               newOwner ? newOwner : "<none>");
    }
    bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts)
    {
        if (sessionPort != CHAT_PORT) {
            printf("Rejecting join attempt on non-chat session port %d\n", sessionPort);
            return false;
        }

        printf("Accepting join session request from %s (opts.proximity=%x, opts.traffic=%x, opts.transports=%x)\n",
               joiner, opts.proximity, opts.traffic, opts.transports);
        return true;
    }

    void SessionJoined(SessionPort sessionPort, SessionId id, const char* joiner)
    {
        s_sessionId = id;
        printf("SessionJoined with %s (id=%d)\n", joiner, id);
    }
};


#ifdef __cplusplus
extern "C" {
#endif

static void usage()
{
    printf("Usage: chat [-h] [-s <name>] | [-j <name>]\n");
    exit(1);
}

int main(int argc, char** argv)
{
    QStatus status = ER_OK;

    /* Parse command line args */
    for (int i = 1; i < argc; ++i) {
        if (0 == ::strcmp("-s", argv[i])) {
            if ((++i < argc) && (argv[i][0] != '-')) {
                s_advertisedName = NAME_PREFIX;
                s_advertisedName += argv[i];
            } else {
                printf("Missing parameter for \"-s\" option\n");
                usage();
            }
        } else if (0 == ::strcmp("-j", argv[i])) {
            if ((++i < argc) && (argv[i][0] != '-')) {
                s_joinName = NAME_PREFIX;
                s_joinName += argv[i];
            } else {
                printf("Missing parameter for \"-j\" option\n");
                usage();
            }
        } else if (0 == ::strcmp("-h", argv[i])) {
            usage();
        } else {
            printf("Unknown argument \"%s\"\n", argv[i]);
            usage();
        }
    }

    /* Validate command line */
    if (s_advertisedName.empty() && s_joinName.empty()) {
        printf("Must specify either -s or -j\n");
        usage();
    } else if (!s_advertisedName.empty() && !s_joinName.empty()) {
        printf("Cannot specify both -s  and -j\n");
        usage();
    }

    /* Create message bus */
    BusAttachment* bus = new BusAttachment("chat", true);
    s_bus = bus;

    /* Create org.alljoyn.bus.samples.chat interface */
    InterfaceDescription* chatIntf = NULL;
    status = bus->CreateInterface(CHAT_SERVICE_INTERFACE_NAME, chatIntf);
    if (ER_OK == status) {
        chatIntf->AddSignal("Chat", "s",  "str", 0);
        chatIntf->Activate();
    } else {
        printf("Failed to create interface \"%s\" (%s)\n", CHAT_SERVICE_INTERFACE_NAME, QCC_StatusText(status));
    }

    /* Create and register the bus object that will be used to send and receive signals */
    ChatObject chatObj(*bus, CHAT_SERVICE_OBJECT_PATH);
    bus->RegisterBusObject(chatObj);
    s_chatObj = &chatObj;

    /* Start the msg bus */
    if (ER_OK == status) {
        status = bus->Start();
        if (ER_OK != status) {
            printf("BusAttachment::Start failed (%s)\n", QCC_StatusText(status));
        }
    }

    /* Register a bus listener */
    if (ER_OK == status) {
        s_busListener = new MyBusListener();
        s_bus->RegisterBusListener(*s_busListener);
    }

    /* Get env vars */
    const char* connectSpec = getenv("BUS_ADDRESS");
    if (connectSpec == NULL) {
#ifdef _WIN32
        connectSpec = "tcp:addr=127.0.0.1,port=9956";
#else
        connectSpec = "unix:abstract=alljoyn";
#endif
    }

    /* Connect to the local daemon */
    if (ER_OK == status) {
        status = s_bus->Connect(connectSpec);
        if (ER_OK != status) {
            printf("BusAttachment::Connect(%s) failed (%s)\n", connectSpec, QCC_StatusText(status));
        }
    }

    /* Advertise or discover based on command line options */
    if (!s_advertisedName.empty()) {
        /* Request name */
        QStatus status = s_bus->RequestName(s_advertisedName.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE);
        if (ER_OK != status) {
            printf("RequestName(%s) failed (status=%s)\n", s_advertisedName.c_str(), QCC_StatusText(status));
            status = (status == ER_OK) ? ER_FAIL : status;
        }

        /* Bind the session port*/
        SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
        if (ER_OK == status) {
            SessionPort sp = CHAT_PORT;
            status = s_bus->BindSessionPort(sp, opts, *s_busListener);
            if (ER_OK != status) {
                printf("BindSessionPort failed (%s)\n", QCC_StatusText(status));
            }
        }

        /* Advertise name */
        if (ER_OK == status) {
            status = s_bus->AdvertiseName(s_advertisedName.c_str(), opts.transports);
            if (status != ER_OK) {
                printf("Failed to advertise name %s (%s)\n", s_advertisedName.c_str(), QCC_StatusText(status));
            }
        }
    } else {
        /* Discover name */
        status = s_bus->FindAdvertisedName(s_joinName.c_str());
        if (status != ER_OK) {
            printf("org.alljoyn.Bus.FindAdvertisedName failed (%s)\n", QCC_StatusText(status));
        }

        /* Wait for join session to complete */
        while (!s_joinComplete) {
#ifdef _WIN32
            Sleep(10);
#else
            usleep(10 * 1000);
#endif
        }
    }

    /* Take input from stdin and send it as a chat messages */
    const int bufSize = 1024;
    char buf[bufSize];
    while ((ER_OK == status) && (get_line(buf, bufSize, stdin))) {
        status = chatObj.SendChatSignal(buf);
    }

    /* Cleanup */
    delete bus;
    bus = NULL;
    s_bus = NULL;

    if (NULL != s_busListener) {
        delete s_busListener;
        s_busListener = NULL;
    }

    return (int) status;
}

#ifdef __cplusplus
}
#endif
