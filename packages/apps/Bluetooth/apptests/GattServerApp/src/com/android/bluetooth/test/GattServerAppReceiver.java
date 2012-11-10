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

import java.util.ArrayList;
import java.util.Arrays;

import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;
import android.app.Activity;
import android.bluetooth.BluetoothDevicePicker;
import android.bluetooth.BluetoothDevice;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.ParcelUuid;

/**
* This class is the broadcast receiver class for the Gatt Server application.
* This receiver handles the intents and starts the Gatt Server service when the
* phone boots up and when Bluetooth is turned on
*/
public class GattServerAppReceiver extends BroadcastReceiver{
    private final static String TAG = "GattServerAppReceiver";
    private static final int REQUEST_ENABLE_BT = 1;
    private static Handler handler = null;
    GattServerAppService gattService = new GattServerAppService();

    public void onReceive(Context context, Intent intent) {
        final String action = intent.getAction();
        BluetoothAdapter mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        if(action != null && action.equalsIgnoreCase(BluetoothAdapter.ACTION_STATE_CHANGED)) {
            if (intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR) ==
                BluetoothAdapter.STATE_ON) {
                Intent serviceIntent = new Intent();
                serviceIntent.setAction("com.android.bluetooth.test.GattServerAppService");
                Log.d(TAG, "Going to start service from BT Server app Broadcast Receiver::");
                context.startService(serviceIntent);
            }
            else if(intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1) ==
                    BluetoothAdapter.STATE_OFF) {
                Log.d(TAG, "Bluetooth is off");

                if(GattServerAppService.gattProfile != null &&
                        GattServerAppService.serverConfiguration != null) {
                    if(GattServerAppService.connectedDevicesList != null &&
                            GattServerAppService.connectedDevicesList.size() > 0) {
                        if(GattServerAppService.connectedDevicesList != null &&
                                GattServerAppService.connectedDevicesList.size() > 0) {
                            for(int i=0; i < GattServerAppService.connectedDevicesList.size(); i++) {
                                BluetoothDevice remoteDevice = GattServerAppService.connectedDevicesList.get(i);
                                gattService.disconnectLEDevice(remoteDevice);
                            }
                        }
                    }
                    GattServerAppService.gattProfile.
                            unregisterServerConfiguration(GattServerAppService.serverConfiguration);
                    Log.d(TAG, "Unregistered server app configuration");
                    Intent serviceIntent = new Intent();
                    serviceIntent.setAction("com.android.bluetooth.test.GattServerAppService");
                    Log.d(TAG, "Going to stop service from BT Server app Broadcast Receiver::");
                    context.stopService(serviceIntent);
                }
            }
        }
        else if (action.equals(BluetoothDevicePicker.ACTION_DEVICE_SELECTED)) {
            BluetoothDevice remoteDevice = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            Log.d(TAG, "Received BT device selected intent, BT device: " + remoteDevice.getAddress());
            String deviceName = remoteDevice.getName();
            Message msg = new Message();
            msg.what = GattServerAppService.DEVICE_SELECTED;
            Bundle b = new Bundle();
            b.putParcelable(GattServerAppService.REMOTE_DEVICE, remoteDevice);
            msg.setData(b);
            handler.sendMessage(msg);
        }
    }
    public static void registerHandler(Handler handle) {
        Log.d(TAG, "Registered Handler");
        handler = handle;
    }
}
