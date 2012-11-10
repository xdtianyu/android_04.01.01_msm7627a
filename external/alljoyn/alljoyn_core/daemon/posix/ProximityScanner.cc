/**
 * @file
 * ProximityScanner provides the scan results used by the Discovery framework and Rendezvous server
 */

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

#include <qcc/platform.h>
#include <map>
#include <stdio.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <Status.h>
#include <alljoyn/Message.h>
#include <alljoyn/MsgArg.h>
#include <utility>
#include "ProximityScanner.h"


#define QCC_MODULE "PROXIMITY_SCANNER"

using namespace qcc;

namespace ajn {

ProximityScanner::ProximityScanner(BusAttachment& bus) : bus(bus) {
    QCC_DbgTrace(("ProximityScanner::ProximityScanner()"));
}

void ProximityScanner::PrintBSSIDMap(std::map<qcc::String, qcc::String> mymap) {

    std::map<qcc::String, qcc::String>::iterator it;
    for (it = mymap.begin(); it != mymap.end(); it++) {
        //QCC_DbgPrintf(("\n BSSID : %s", it->first.c_str()));
    }

}


class MyBusListener : public BusListener, public SessionListener {
  public:
    MyBusListener() : BusListener(), sessionId(0) { }

    void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
    {
        //QCC_DbgPrintf(("\n Found the SERVICE ..... Wooooohoooo !!"));
    }

    SessionId GetSessionId() const { return sessionId; }

  private:
    SessionId sessionId;
};

//void ProximityScanner::StopScan() {
//
//
//	MyBusListener* g_busListener;
//	g_busListener = new MyBusListener();
//	bus.RegisterBusListener(*g_busListener);
//
//
//
//
//
//
//}


void ProximityScanner::Scan(bool request_scan) {

    QCC_DbgTrace(("ProximityScanner::Scan()"));
    QStatus status;
    MyBusListener* g_busListener;

    g_busListener = new MyBusListener();
    bus.RegisterBusListener(*g_busListener);

    bool hasOwer;

    uint32_t starttime = GetTimestamp();



    while (true) {
        status = bus.NameHasOwner("org.alljoyn.proximity.proximityservice", hasOwer);
        if (ER_OK != status) {
            QCC_LogError(status, ("Error while calling NameHasOwner"));
        }
        if (hasOwer) {
            QCC_DbgPrintf((" =-=-=-=-=-=-=- NameHasOwnwer: Android Helper Service running  =-=-=-=-=-="));
            break;
        } else {
            QCC_DbgPrintf(("No Android service owner found yet"));
            // If service is not present there is not point in sleeping. We can return empty data
            scanResults.clear();
            return;
            //qcc::Sleep(5000);
        }

    }

    ProxyBusObject*remoteObj = new ProxyBusObject(bus, "org.alljoyn.proximity.proximityservice", "/ProximityService", 0);

    status = remoteObj->IntrospectRemoteObject();
    if (ER_OK != status) {
        QCC_LogError(status, ("Problem while introspecting the remote object /ProximityService"));
    } else {
        QCC_DbgPrintf(("Introspection on the remote object /ProximityService successful"));
    }



    // Call the remote method SCAN on the service

    // Why do change request_scan here ? This handles the situation where the service was killed by the OS and
    // we are not able to get the scan results

    QCC_DbgPrintf(("===============Time before Scan ================== %d", starttime));
    Message reply(bus);
    MsgArg arg;
    arg.Set("b", request_scan);
    status = remoteObj->MethodCall("org.alljoyn.proximity.proximityservice", "Scan", &arg, 1, reply, 35000);
    //status = remoteObj->MethodCall("org.alljoyn.proximity.proximityservice", "Scan", NULL, 0, reply, 35000);
    if (ER_OK != status) {
        QCC_LogError(status, ("Problem while calling method Scan on the remote object"));
        qcc::String errorMsg;
        reply->GetErrorName(&errorMsg);
        QCC_DbgPrintf(("Call to Scan returned error message : %s", errorMsg.c_str()));
        return;

    } else {
        QCC_DbgPrintf(("Method call Scan was successful \n"));
    }

    //
    // Clear the map before storing any results in in
    //
    scanResults.clear();
    //
    // Copy the results from the reply to the scanResults map
    //
    MsgArg*scanArray;
    size_t scanArraySize;
    const MsgArg*args = reply->GetArg(0);
    if (args == NULL) {
        return;
    }
    status = args->Get("a(ssb)", &scanArraySize, &scanArray);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while unmarshalling the array of structs recevied from the service"));
    }
    //
    // We populate the scanResultsMap only when we have results
    //
    //printf("\n -------------------- From Message --------------------------------- \n");
    if (ER_OK == status && scanArraySize > 0) {
        QCC_DbgPrintf(("Array size of scan results > 0"));
        for (size_t i = 0; i < scanArraySize; i++) {
            char* bssid;
            char* ssid;
            bool attached;

            status = scanArray[i].Get("(ssb)", &bssid, &ssid, &attached);
            if (ER_OK != status) {
                QCC_LogError(status, ("Error while getting the struct members Expected signature = %s", scanArray[i].Signature().c_str()));
            } else {
                //printf("\n BSSID = %s , SSID = %s , attached = %s",bssid, ssid, attached ? "true" : "false");
                qcc::String bssid_str(bssid);
                qcc::String ssid_str(ssid);
                scanResults.insert(std::map<std::pair<qcc::String, qcc::String>, bool>::value_type(std::make_pair(bssid_str, ssid_str), attached));
            }

        }

        QCC_DbgPrintf(("-------------------- From Scan function -----------------------------------\n"));
        std::map<std::pair<qcc::String, qcc::String>, bool>::iterator it;
        for (it = scanResults.begin(); it != scanResults.end(); it++) {
            QCC_DbgPrintf(("BSSID = %s , SSID = %s, attached = %s", it->first.first.c_str(), it->first.second.c_str(), (it->second ? "true" : "false")));
        }


    } else {
        // We do not have any scan results returned by the Android service ??
        QCC_DbgPrintf(("No Scan results were returned by the service. Either Wifi is turned off or there are no APs around"));
    }


    QCC_DbgPrintf(("================ Time after Scan processing ============  %d", GetTimestamp() - starttime));
}

}
