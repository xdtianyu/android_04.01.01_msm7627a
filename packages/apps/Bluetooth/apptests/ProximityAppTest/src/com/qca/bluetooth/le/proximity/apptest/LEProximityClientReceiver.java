/*
 * Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *          notice, this list of conditions and the following disclaimer in the
 *          documentation and/or other materials provided with the distribution.
 *        * Neither the name of Code Aurora nor
 *          the names of its contributors may be used to endorse or promote
 *          products derived from this software without specific prior written
 *          permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.qca.bluetooth.le.proximity.apptest;


import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothDevicePicker;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.widget.Toast;

public class LEProximityClientReceiver extends BroadcastReceiver {

    private final static String TAG = "LEProximityClientReceiver";

    private static Handler handler = null;

    @Override
    public void onReceive(Context context, Intent intent) {

        String action = intent.getAction();

        if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
            if (BluetoothAdapter.STATE_ON == intent.getIntExtra(
                                                               BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)) {
                Log.d(TAG, "Received BLUETOOTH_STATE_CHANGED_ACTION, BLUETOOTH_STATE_ON");
            }
        } else if (action.equals(BluetoothDevicePicker.ACTION_DEVICE_SELECTED)) {

            BluetoothDevice remoteDevice = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);

            Log.d(TAG, "Received BT device selected intent, bt device: " + remoteDevice.getAddress());

            String deviceName = remoteDevice.getName();
            String toastMsg;
            toastMsg = " The user selected the device named " + deviceName;
            Toast.makeText(context, toastMsg, Toast.LENGTH_SHORT).show();
            Message msg = new Message();
            msg.what = LEProximityClient.DEVICE_SELECTED;
            Bundle b = new Bundle();
            b.putParcelable(LEProximityClient.REMOTE_DEVICE, remoteDevice);
            msg.setData(b);
            handler.sendMessage(msg);
        }
    }

    public static void registerHandler(Handler handle)
    {
        Log.e(TAG, "Registered Handler");
        handler = handle;
    }
}
