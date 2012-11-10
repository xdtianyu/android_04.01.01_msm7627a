/**
 * @file
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
package org.alljoyn.jni;

import android.app.AlarmManager;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.Context;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiManager;
import android.util.Log;


import java.util.ArrayList;
import java.util.List;



// This guys needs access to the Map which the original ProximityService is going to send back to the native code


public class ScanResultsReceiver extends BroadcastReceiver{
	
	AllJoynAndroidExt jniProximity;
	AlarmManager alarmMgr;
	//
	// Need a constructor that will take the instance on Proximity Service
	//
	public ScanResultsReceiver(AllJoynAndroidExt jniProximity){
		super();
		this.jniProximity = jniProximity; 
		//alarmMgr = (AlarmManager)getSystemService(Context.ALARM_SERVICE);
	}
	public ScanResultsReceiver(){
		super();
	}
	
	@Override
	public void onReceive(Context c, Intent intent){
		
		if(intent.getAction().equals(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION)){
			
			
			Log.v("ScanResultsReceiver", "SCAN_RESULTS_AVAILABLE_ACTION received");
			
			List<ScanResult> scanResults = jniProximity.wifiMgr.getScanResults();
			//
			// If not access point were returned then we
			//
			if(scanResults.size() == 0){
				Log.v("ScanResultsReceiver", "Result size = 0");
				jniProximity.scanResultsObtained = true;
				return;
			}
			else{
				Log.v("ScanResultsReceiver", "Result size  = " + Integer.toString(scanResults.size()));
			}
			
			
			jniProximity.lastScanTime = System.currentTimeMillis();
			
			// If scan WAS requested we thrash all old results and just create and entirely new scan Results map
			// If scan WAS NOT requested we append the results obtained from this scan to our original result
			
			jniProximity.scanResultMessage = new ScanResultMessage[scanResults.size()];
						
			String currentBSSID = jniProximity.wifiMgr.getConnectionInfo().getBSSID();
			int currentBSSIDIndex = 0;
			
			for (ScanResult result : scanResults){

					jniProximity.scanResultMessage[currentBSSIDIndex] = new ScanResultMessage();
					jniProximity.scanResultMessage[currentBSSIDIndex].bssid = result.BSSID;
					jniProximity.scanResultMessage[currentBSSIDIndex].ssid = result.SSID;
					if(currentBSSID !=null && currentBSSID.equals(result.BSSID)){
						jniProximity.scanResultMessage[currentBSSIDIndex].attached = true;
					}
					currentBSSIDIndex++;
					
			}
			currentBSSIDIndex = 0;
			jniProximity.scanResultsObtained = true;
		}
	
	}

}
