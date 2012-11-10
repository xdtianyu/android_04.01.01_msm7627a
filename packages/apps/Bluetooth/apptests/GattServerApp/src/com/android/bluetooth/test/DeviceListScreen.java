/*
* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following
*      disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of Code Aurora Forum, Inc. nor the names of its
*      contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.

* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package com.android.bluetooth.test;

import android.app.ListActivity;
import android.bluetooth.BluetoothDevice;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ListView;
/**
 * User interface for the Gatt Server Test application. This activity passes messages to and from
 * the service.
 */
public class DeviceListScreen extends ListActivity {
    private static final String TAG = "DeviceListScreen";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if(GattServerAppService.connectedDevicesList != null &&
                GattServerAppService.connectedDevicesList.size() > 0) {
            Log.d(TAG, "list of connected devices ::");
            for(int i=0; i < GattServerAppService.connectedDevicesList.size(); i++) {
                Log.d(TAG, GattServerAppService.connectedDevicesList.get(i).getAddress());
            }

            int arrLength = GattServerAppService.connectedDevicesList.size();
            String[] connectedDevicesAddr = new String[arrLength];
            for(int index =0; index < arrLength; index++) {
                connectedDevicesAddr[index] = GattServerAppService.connectedDevicesList.get(index).getAddress();
            }
            setListAdapter(new ArrayAdapter<String>(this, R.layout.device_list, connectedDevicesAddr));
            ListView lv = getListView();
            lv.setTextFilterEnabled(true);

            lv.setOnItemClickListener(new OnItemClickListener() {
              @Override
            public void onItemClick(AdapterView<?> parent, View view,
                  int position, long id) {
                BluetoothDevice remoteDevice = null;
                Log.d(TAG, "position of address selected::"+position);
                if(GattServerAppService.connectedDevicesList != null &&
                        GattServerAppService.connectedDevicesList.size() > 0) {
                    remoteDevice = GattServerAppService.connectedDevicesList.get(position);
                }
                // When clicked, call disconnect API
                GattServerAppService gattService = new GattServerAppService();
                gattService.disconnectLEDevice(remoteDevice);
                //close the activity
                finish();
              }
            });
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

}