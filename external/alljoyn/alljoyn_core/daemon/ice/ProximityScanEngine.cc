/**
 * @file
 * ProximityScanner gets the scan results used by the Discovery framework and Rendezvous server
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
#include <qcc/Timer.h>
#include <stdio.h>
#include <qcc/Thread.h>

#include "DiscoveryManager.h"
#include "ProximityScanEngine.h"
#include <ProximityScanner.h>

using namespace std;
using namespace qcc;

#define QCC_MODULE "PROXIMITY_SCAN_ENGINE"

class DiscoveryManager;

namespace ajn {


ProximityMessage ProximityScanEngine::GetScanResults(list<String>& bssids, list<String>& macIds) {

    QCC_DbgTrace(("ProximityScanEngine::GetScanResults() called"));
    ProximityMessage proximityMessage;
    list<WiFiProximity> wifi_bssid_list;
    list<BTProximity> bt_mac_list;
    std::map<std::pair<qcc::String, qcc::String>, bool>::iterator final_map_it;

    wifi_bssid_list.clear();
    bt_mac_list.clear();
    bssids.clear();
    macIds.clear();

    bssid_lock.Lock(MUTEX_CONTEXT);
    for (final_map_it = finalMap.begin(); final_map_it != finalMap.end(); final_map_it++) {
        WiFiProximity bssid_info;
        bssid_info.BSSID = final_map_it->first.first;
        bssid_info.SSID = final_map_it->first.second;
        bssid_info.attached = final_map_it->second;
        wifi_bssid_list.push_front(bssid_info);
        bssids.push_back(bssid_info.BSSID);
    }
    bssid_lock.Unlock(MUTEX_CONTEXT);
    proximityMessage.wifiaps = wifi_bssid_list;
    proximityMessage.BTs = bt_mac_list;

    /* Sort the lists */
    if (!bssids.empty()) {
        bssids.sort();
    }

    if (!macIds.empty()) {
        macIds.sort();
    }

    return proximityMessage;
}



void ProximityScanEngine::PrintFinalMap() {

    QCC_DbgTrace(("ProximityScanEngine::PrintFinalMap() called"));
    QCC_DbgPrintf(("-------------------Final Map ----------------------"));
    std::map<std::pair<qcc::String, qcc::String>, bool>::iterator it;
    for (it = finalMap.begin(); it != finalMap.end(); it++) {
        QCC_DbgPrintf(("BSSID: %s  SSID: %s attached: %s", it->first.first.c_str(), it->first.second.c_str(), it->second ? "true" : "false"));
    }
    QCC_DbgPrintf((" ---------------------------------------------------"));



}

void ProximityScanEngine::PrintHysteresis() {
    QCC_DbgTrace(("ProximityScanEngine::PrintHysteresis() called"));
    QCC_DbgPrintf(("-------------Hysteresis Map -----------------"));
    if (hysteresisMap.empty()) {
        QCC_DbgPrintf(("MAP is CLEAR"));
    }
    std::map<std::pair<qcc::String, qcc::String>, int>::iterator it;
    for (it = hysteresisMap.begin(); it != hysteresisMap.end(); it++) {
        QCC_DbgPrintf(("BSSID: %s   SSID: %s   COUNT: %d", it->first.first.c_str(), it->first.second.c_str(), it->second));
    }
    QCC_DbgPrintf(("----------------------------------------------"));
}

ProximityScanEngine::ProximityScanEngine(DiscoveryManager*dm) : bus(dm->bus) {

    QCC_DbgTrace(("ProximityScanEngine::ProximityScanEngine() called"));
    tadd_count = 1;
    wifiapDroppped = false;
    //wifiapAdded = false;
    wifiON = false;
    //request_scan = false;
    request_scan = true;
    no_scan_results_count = 0;
    discoveryManager = dm;
    proximityScanner = new ProximityScanner(bus);
    finalMap.clear();
    hysteresisMap.clear();
}


ProximityScanEngine::~ProximityScanEngine() {
    QCC_DbgTrace(("ProximityScanEngine::~ProximityScanEngine() called"));

    StopScan();

    delete proximityScanner;
    proximityScanner = NULL;
}

void ProximityScanEngine::ProcessScanResults() {

    QCC_DbgTrace(("ProximityScanEngine::ProcessScanResults() called"));
    QCC_DbgPrintf(("Size of scan results = %d", proximityScanner->scanResults.size()));
    QCC_DbgPrintf(("Size of scan Hysteresis = %d", hysteresisMap.size()));
    QCC_DbgPrintf(("Size of scan Final Map = %d", finalMap.size()));

    std::map<std::pair<qcc::String, qcc::String>, bool>::iterator it;
    std::map<std::pair<qcc::String, qcc::String>, int>::iterator hit;

    // First get the scan results and update the hysteresis map : increase the count of the ones seen and
    // decrease the count of the ones not seen
    //
    // Increment count if present else add to Hysteresis AND final map
    //

    QCC_DbgPrintf(("Incrementing counts in the Hysteresis Map..."));
    QCC_DbgPrintf(("BEFORE Incrementing the maps are"));
    PrintHysteresis();
    PrintFinalMap();

    if (proximityScanner == NULL) {
        QCC_LogError(ER_FAIL, ("proximityScanner == NULL "));
    }

    bssid_lock.Lock(MUTEX_CONTEXT);
    for (it = proximityScanner->scanResults.begin(); it != proximityScanner->scanResults.end(); it++) {
        //hit = hysteresisMap.find(it->first);
        hit = hysteresisMap.find(std::pair<qcc::String, qcc::String>(it->first.first, it->first.second));
        if (hit != hysteresisMap.end()) {
            QCC_DbgPrintf(("Found the entry in hysteresisMap"));
            hit->second = START_COUNT;
            QCC_DbgPrintf(("Value of scan entry =%s,%s updated to %d", hit->first.first.c_str(), hit->first.second.c_str(), hit->second));
        } else {

            QCC_DbgPrintf(("Inserting new entry in the hysteresis and final map <%s,%s> , %s", it->first.first.c_str(), it->first.second.c_str(), it->second ? "true" : "false"));

            // Make pair outside
            hysteresisMap.insert(std::map<std::pair<qcc::String, qcc::String>, int>::value_type(std::make_pair(it->first.first, it->first.second), START_COUNT));
            finalMap.insert(std::map<std::pair<qcc::String, qcc::String>, bool>::value_type(std::make_pair(it->first.first, it->first.second), it->second));

        }
    }
    bssid_lock.Unlock(MUTEX_CONTEXT);

    if (proximityScanner->scanResults.size() > 0) {
        QCC_DbgPrintf(("Scan returned results so APs were added to the final Map"));
        //wifiapAdded = true;
        wifiON = true;
    }
    QCC_DbgPrintf(("Printing Maps after incrementing counts in Hysteresis Map"));
    PrintHysteresis();
    PrintFinalMap();

    //
    // Decrement count of those not present in scan results
    //
    //
    // look at the final hysteresis map
    // if count has reached zero remove it from the final AND hysteresis map in that order since you need the key from hysteresis
    //          Update final map .. Indicate with a boolean that there has been a change in the final map
    QCC_DbgPrintf(("Decrementing counts in Hysteresis Map "));
    bssid_lock.Lock(MUTEX_CONTEXT);
    hit = hysteresisMap.begin();
    while (hit != hysteresisMap.end()) {
        it = proximityScanner->scanResults.find(hit->first);
        if (it == proximityScanner->scanResults.end()) {
            hit->second = hit->second - 1;
            QCC_DbgPrintf(("Value of <%s,%s> = %d after decrementing", hit->first.first.c_str(), hit->first.second.c_str(), hit->second));
            if (hit->second == 0) {
                wifiapDroppped = true;
                QCC_DbgPrintf(("Entry <%s,%s> reached count 0 .... Deleting from hysteresis and final map", hit->first.first.c_str(), hit->first.second.c_str()));
                finalMap.erase(hit->first);
                hysteresisMap.erase(hit++);
            } else {
                ++hit;
            }
        } else {
            ++hit;
        }
    }
    bssid_lock.Unlock(MUTEX_CONTEXT);
//  We send an update in two conditions:
//	1. We reached Tadd count == 4 and the scan results are being returned with some results (non-empty)
//	2. Something was dropped from the final map

    PrintHysteresis();
    PrintFinalMap();

    // If TADD_COUNT has been reached
    //		all entries in the hysteresis with count > 0 make it to final
    //          Update final map .. Indicate with a boolean that there has been a change in the final map

    if ((tadd_count == TADD_COUNT && wifiON) || wifiapDroppped || request_scan) {
        // Form the proximity message if needed by checking for the boolean set in the above two cases
        // and Queue it if there is a change

        list<String> bssids;
        list<String> macIds;

        bssids.clear();
        macIds.clear();

        ProximityMessage proximityMsg = GetScanResults(bssids, macIds);
        QCC_DbgPrintf(("=-=-=-=-=-=-=-=-=-=-=-= Queuing Proximity Message =-=-=-=-=-=-=-=-=-=-=-="));
        PrintFinalMap();

        discoveryManager->QueueProximityMessage(proximityMsg, bssids, macIds);

        wifiapDroppped = false;
        //wifiapAdded = false;
        wifiON = true;
        tadd_count = 0;
    } else {
        tadd_count++;
    }

    //
    // This needs to checked for the following conditions:
    // 1. We did not get any opportunistic scan results since the last 4 scans = 120 secs.
    //    This could mean that we are either connected to a network in which case we are not returned any results
    //	  This could also mean that Wifi is turned off or the phone is acting as a portable hotspot
    // 2. Wifi is turned ON but we do not have any networks in the vicinity. In that case it is not harmful to
    //    request a scan once in 120 secs apart from what the device is already doing
    //

    if (proximityScanner->scanResults.size() <= 1) {
        no_scan_results_count++;
    } else {
        no_scan_results_count = 0;
    }


    if (no_scan_results_count == 3) {
        request_scan = true;
    } else {
        request_scan = false;
    }

/*
    // Extract the SSID corresponding to the BSSID since the tokenizing code did not look nice we do it in two iterations
    //printf("\n Printing the initial BSSID set");
    //PrintBSSIDMap(initial_bssid);

    tadd_count++;

    //
    // We have kept the time to determine whether a BSSID is worth adding to our final set to four scans
    // If the BSSID shows up in, at least one of the four scans then we add it to the final_bssid map
    // If it does not show up in either one of the four scans remove it from the final_bssid map
    //

    if (tadd_count == TADD_COUNT) {
        printf("\n Tadd triggered ..... do something");
        tadd_count = 0;

        //
        // Adding to final_bssid comes here
        // We look at the final set set here. If we do not find the bssid in the hysteresis map then we add this
        // bssid to the final set
        //

        map<qcc::String, int>::iterator it;
        bssid_lock.Lock(MUTEX_CONTEXT);
        final_bssid.clear();
        for (it = hystereXsis_for_adding.begin(); it != hysteresis_for_adding.end(); it++) {
            // If the BSSid showed up even once then we can add it to the final set
            if (final_bssid.find(it->first) == final_bssid.end()) {
                final_bssid.insert(pair<qcc::String, qcc::String>(it->first, ""));
            }

        }
        bssid_lock.Unlock(MUTEX_CONTEXT);
    }
    //
    // If we have not reached count of four scans we update the Hysteresis for adding
    //
    else {
        map<qcc::String, qcc::String>::iterator bssid_it;
        map<qcc::String, int>::iterator hys_it;
        for (bssid_it = initial_bssid.begin(); bssid_it != initial_bssid.end(); bssid_it++) {
            hys_it = hysteresis_for_adding.find(bssid_it->first);
            if (hys_it == hysteresis_for_adding.end()) {
                hysteresis_for_adding.insert(pair<qcc::String, int>(bssid_it->first, 1));
            }
        }

    }



    PrintHysteresis(hysteresis_for_adding);


    //
    // Dropping an existing BSSID from the final_bssid comes here
    // We look at the final bssid set here. If we do not find the bssid present in the final set in any of the
    // scans which is necessarily the hystereis map then we remove it from the final set
    //

    map<qcc::String, qcc::String>::iterator final_bssid_it;
    map<qcc::String, int>::iterator hys_it;

    bssid_lock.Lock(MUTEX_CONTEXT);
    for (final_bssid_it = final_bssid.begin(); final_bssid_it != final_bssid.end(); final_bssid_it++) {
        hys_it = hysteresis_for_adding.find(final_bssid_it->first);
        if (hys_it == hysteresis_for_adding.end()) {
            final_bssid.erase(final_bssid_it);
        }
    }
    bssid_lock.Unlock(MUTEX_CONTEXT);

    list<String> bssids;
    list<String> macIds;

    bssids.clear();
    macIds.clear();

    //
    // At this point we have the final_bssid formed so we update the Proximity message
    // First we access the current Proximity list. See if the elements inside have changed. If yes then we call QueueProximityMessage
    // else we do not send the Proximity message
    //
    // TO DO : right now we are sending whatever is the new set instead of sending when something changes
    //
    ProximityMessage proximityMessage;
    list<WiFiProximity> wifi_bssid_list;

    for (final_bssid_it = final_bssid.begin(); final_bssid_it != final_bssid.end(); final_bssid_it++) {
        WiFiProximity bssid_info;
        bssid_info.BSSID = final_bssid_it->first;
        bssid_info.attached = false;
        wifi_bssid_list.push_front(bssid_info);

        bssids.push_back(bssid_info.BSSID);
    }
    proximityMessage.wifiaps = wifi_bssid_list;

    printf("\n Printing contents of proximity message");
    for (int i = 0; i < (int)wifi_bssid_list.size(); i++) {
        printf("\n BSSID : %s", wifi_bssid_list.front().BSSID.c_str());
    }

    PrintHysteresis(hysteresis_for_adding);
    PrintBSSIDMap(final_bssid);

   Sort the lists
    if (!bssids.empty()) {
        bssids.sort();
    }

    if (!macIds.empty()) {
        macIds.sort();
    }

   //   discoveryManager.QueueProximityMessage(proximityMessage, bssids, macIds);

    // Clear the hysteresis map
    hysteresis_for_adding.clear();

 */

}


void ProximityScanEngine::AlarmTriggered(const Alarm& alarm, QStatus reason)
{
    while (true) {
        uint64_t start = GetTimestamp64();
        proximityScanner->Scan(request_scan);
        ProcessScanResults();
        uint32_t delay = ::max((uint64_t)0, SCAN_DELAY - (GetTimestamp64() - start));
        if (delay > 0) {
            //Add alarm with delay time to our main timer
            QCC_DbgPrintf(("Adding Alarm "));
            AddAlarm(delay);
            break;
        }
    }
}

void ProximityScanEngine::StopScan() {

    QCC_DbgTrace(("ProximityScanEngine::StopScan() called"));
    // RemoveTimers
    if (mainTimer.HasAlarm(*tScan)) {
        mainTimer.RemoveAlarm(*myListener, *tScan);
    }
    mainTimer.Stop();
    mainTimer.Join();
    hysteresisMap.clear();
    finalMap.clear();
    //isFirstScanComplete = false;
    wifiapDroppped = false;
    //wifiapAdded = false;
    wifiON = false;
    request_scan = true;
    no_scan_results_count = 0;
    QCC_DbgPrintf(("ProximityScanEngine::StopScan() completed"));

}

void ProximityScanEngine::StartScan() {

    QCC_DbgTrace(("ProximityScanEngine::StartScan() called"));

    //request_scan = true;

    map<qcc::String, qcc::String>::iterator it;

    // Start the timer

    mainTimer.Start();

    // Initialize the Alarm

    uint32_t relativeTime = 5000;
    uint32_t periodMs = 0;
    //AlarmListener*myListener = this;
    myListener = this;
    Alarm tScan(relativeTime, myListener, periodMs);

    // Add the alarm to the timer
    this->tScan = &tScan;
    mainTimer.AddAlarm(*this->tScan);

//    while (true) {
//        if (!isFirstScanComplete) {
//            QCC_DbgPrintf(("Sleeping before getting first scan results"));
//            qcc::Sleep(2000);
//        } else {
//            break;
//        }
//    }

}

// AddAlarm() which will tell the alarm about the listener. That listener should be made a part of the class definition of ProximityScanEngine

void ProximityScanEngine::AddAlarm(uint32_t delay) {

    uint32_t periodMs = 0;
    AlarmListener*myListener = this;
    Alarm tScan(delay, myListener, periodMs);
    mainTimer.AddAlarm(tScan);
}

}
