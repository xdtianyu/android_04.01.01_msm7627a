/**
 * @file
 *
 * This file provides definitions for standard AllJoyn interfaces
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
#include <qcc/Debug.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/InterfaceDescription.h>

#include "SessionInternal.h"

#define QCC_MODULE  "ALLJOYN"

namespace ajn {

/** org.alljoyn.Bus interface definitions */
const char* org::alljoyn::Bus::ErrorName = "org.alljoyn.Bus.ErStatus";
const char* org::alljoyn::Bus::ObjectPath = "/org/alljoyn/Bus";
const char* org::alljoyn::Bus::InterfaceName = "org.alljoyn.Bus";
const char* org::alljoyn::Bus::WellKnownName = "org.alljoyn.Bus";
const char* org::alljoyn::Bus::Secure = "org.alljoyn.Bus.Secure";
const char* org::alljoyn::Bus::Peer::ObjectPath = "/org/alljoyn/Bus/Peer";

/** org.alljoyn.Daemon interface definitions */
const char* org::alljoyn::Daemon::ErrorName = "org.alljoyn.Daemon.ErStatus";
const char* org::alljoyn::Daemon::ObjectPath = "/org/alljoyn/Bus";
const char* org::alljoyn::Daemon::InterfaceName = "org.alljoyn.Daemon";
const char* org::alljoyn::Daemon::WellKnownName = "org.alljoyn.Daemon";

/** org.alljoyn.Daemon.Debug interface definitions */
const char* org::alljoyn::Daemon::Debug::ObjectPath = "/org/alljoyn/Debug";
const char* org::alljoyn::Daemon::Debug::InterfaceName = "org.alljoyn.Debug";

/** org.alljoyn.Bus.Peer.* interface definitions */
const char* org::alljoyn::Bus::Peer::HeaderCompression::InterfaceName = "org.alljoyn.Bus.Peer.HeaderCompression";
const char* org::alljoyn::Bus::Peer::Authentication::InterfaceName = "org.alljoyn.Bus.Peer.Authentication";
const char* org::alljoyn::Bus::Peer::Session::InterfaceName = "org.alljoyn.Bus.Peer.Session";


QStatus org::alljoyn::CreateInterfaces(BusAttachment& bus)
{
    QStatus status;
    {
        /* Create the org.alljoyn.Bus interface */
        InterfaceDescription* ifc = NULL;
        status = bus.CreateInterface(org::alljoyn::Bus::InterfaceName, ifc);

        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to create interface \"%s\"", org::alljoyn::Bus::InterfaceName));
            return status;
        }
        ifc->AddMethod("BusHello",                 "su",                "ssu",               "GUIDC,protoVerC,GUIDS,uniqueName,protoVerS", 0);
        ifc->AddMethod("BindSessionPort",          "q"SESSIONOPTS_SIG,  "uq",                "portIn,opts,disposition,portOut",            0);
        ifc->AddMethod("UnbindSessionPort",        "q",                 "u",                 "port,disposition",                           0);
        ifc->AddMethod("JoinSession",              "sq"SESSIONOPTS_SIG, "uu"SESSIONOPTS_SIG, "sessionHost,port,opts,disp,sessionId,opts",  0);
        ifc->AddMethod("LeaveSession",             "u",                 "u",                 "sessionId,disposition",                      0);
        ifc->AddMethod("AdvertiseName",            "sq",                "u",                 "name,transports,disposition",                0);
        ifc->AddMethod("CancelAdvertiseName",      "sq",                "u",                 "name,transports,disposition",                0);
        ifc->AddMethod("FindAdvertisedName",       "s",                 "u",                 "name,disposition",                           0);
        ifc->AddMethod("CancelFindAdvertisedName", "s",                 "u",                 "name,disposition",                           0);
        ifc->AddMethod("GetSessionFd",             "u",                 "h",                 "sessionId,handle",                           0);
        ifc->AddMethod("SetLinkTimeout",           "uu",                "uu",                "sessionId,inLinkTO,disposition,outLinkTO",   0);
        ifc->AddMethod("AliasUnixUser",            "u",                 "u",                 "aliasUID, disposition",                      0);

        ifc->AddSignal("FoundAdvertisedName",      "sqs",              "name,transport,prefix",                        0);
        ifc->AddSignal("LostAdvertisedName",       "sqs",              "name,transport,prefix",                        0);
        ifc->AddSignal("SessionLost",              "u",                "sessionId",                                    0);
        ifc->AddSignal("MPSessionChanged",         "usb",              "sessionId,name,isAdded",                       0);

        ifc->Activate();
    }

    {
        /* Create the org.alljoyn.Daemon interface */
        InterfaceDescription* ifc = NULL;
        status = bus.CreateInterface(org::alljoyn::Daemon::InterfaceName, ifc);

        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to create interface \"%s\"", org::alljoyn::Daemon::InterfaceName));
            return status;
        }
        ifc->AddMethod("AttachSession",  "qsssss"SESSIONOPTS_SIG, "uu"SESSIONOPTS_SIG "as", "port,joiner,creator,dest,b2b,busAddr,optsIn,status,id,optsOut,members", 0);
        ifc->AddMethod("GetSessionInfo", "sq"SESSIONOPTS_SIG, "as", "creator,port,opts,busAddrs", 0);
        ifc->AddSignal("DetachSession",  "us",     "sessionId,joiner",       0);
        ifc->AddSignal("ExchangeNames",  "a(sas)", "uniqueName,aliases",     0);
        ifc->AddSignal("NameChanged",    "sss",    "name,oldOwner,newOwner", 0);
        ifc->AddSignal("ProbeReq",       "",       "",                       0);
        ifc->AddSignal("ProbeAck",       "",       "",                       0);
        ifc->Activate();
    }
    {
        /* Create the org.alljoyn.Daemon.Debug interface */
        InterfaceDescription* ifc = NULL;
        status = bus.CreateInterface(org::alljoyn::Daemon::Debug::InterfaceName, ifc);

        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to create interface \"%s\"", org::alljoyn::Daemon::Debug::InterfaceName));
            return status;
        }
        ifc->AddMethod("SetDebugLevel",  "su", NULL, "module,level", 0);
        ifc->Activate();
    }
    {
        /* Create the org.alljoyn.Bus.Peer.HeaderCompression interface */
        InterfaceDescription* ifc = NULL;
        status = bus.CreateInterface(org::alljoyn::Bus::Peer::HeaderCompression::InterfaceName, ifc);
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to create %s interface", org::alljoyn::Bus::Peer::HeaderCompression::InterfaceName));
            return status;
        }
        ifc->AddMethod("GetExpansion", "u", "a(yv)", "token,headerFields");
        ifc->Activate();
    }
    {
        /* Create the org.alljoyn.Bus.Peer.Authentication interface */
        InterfaceDescription* ifc = NULL;
        status = bus.CreateInterface(org::alljoyn::Bus::Peer::Authentication::InterfaceName, ifc);
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to create %s interface", org::alljoyn::Bus::Peer::Authentication::InterfaceName));
            return status;
        }
        ifc->AddMethod("ExchangeGuids",     "su",  "su", "localGuid,localVersion,remoteGuid,remoteVersion");
        ifc->AddMethod("GenSessionKey",     "sss", "ss", "localGuid,remoteGuid,localNonce,remoteNonce,verifier");
        ifc->AddMethod("ExchangeGroupKeys", "ay",  "ay", "localKeyMatter,remoteKeyMatter");
        ifc->AddMethod("AuthChallenge",     "s",   "s",  "challenge,response");
        ifc->AddProperty("Mechanisms",  "s", PROP_ACCESS_READ);
        ifc->AddProperty("Version",     "u", PROP_ACCESS_READ);
        ifc->Activate();
    }
    {
        /* Create the org.alljoyn.Bus.Peer.Session interface */
        InterfaceDescription* ifc = NULL;
        status = bus.CreateInterface(org::alljoyn::Bus::Peer::Session::InterfaceName, ifc);
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to create %s interface", org::alljoyn::Bus::Peer::Session::InterfaceName));
            return status;
        }
        ifc->AddMethod("AcceptSession", "qus"SESSIONOPTS_SIG, "b", "port,id,src,opts,accepted");
        ifc->AddSignal("SessionJoined", "qus", "port,id,src");
        ifc->Activate();
    }
    return status;
}


}


