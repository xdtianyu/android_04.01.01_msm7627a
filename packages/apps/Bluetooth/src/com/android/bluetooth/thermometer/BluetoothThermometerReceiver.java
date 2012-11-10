/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

package com.android.bluetooth.thermometer;

import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.ParcelUuid;
import android.util.Log;

import java.util.ArrayList;
import java.util.Arrays;

public class BluetoothThermometerReceiver extends BroadcastReceiver {

    private static final int ACTION_GATT_PARAMS_NO = 3;

    private final static String TAG = "BluetoothThermometerReceiver";

    private static Handler handler = null;

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = (intent == null) ? null : intent.getAction();
        if (action == null) {
            Log.e(TAG, "action is null");
            return;
        }
        Intent in = new Intent();
        in.putExtras(intent);
        in.setClass(context, BluetoothThermometerServices.class);
        in.putExtra("action", action);

        if (action.equals(BluetoothDevice.ACTION_GATT)) {
            try {
                Log.d(TAG,
                      " ACTION GATT INTENT RECEIVED as a result of gatGattService");
                BluetoothDevice remoteDevice = intent
                                               .getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                Log.d(TAG, "Remote Device: " + remoteDevice.getAddress());

                ParcelUuid uuid = (ParcelUuid) intent
                                  .getExtra(BluetoothDevice.EXTRA_UUID);
                Log.d(TAG, " UUID: " + uuid);

                String[] ObjectPathArray = (String[]) intent
                                           .getExtra(BluetoothDevice.EXTRA_GATT);
                if (ObjectPathArray != null) {
                    Log.d(TAG, " objPathList length : " + ObjectPathArray.length);
                    Message objMsg = new Message();
                    objMsg.what = BluetoothThermometerServices.GATT_SERVICE_STARTED_OBJ;
                    Bundle objBundle = new Bundle();
                    ArrayList<String> gattDataList = new ArrayList<String>(
                        Arrays.asList(ObjectPathArray));
                    gattDataList.add(uuid.toString());
                    objBundle.putStringArrayList(
                        BluetoothThermometerServices.ACTION_GATT_SERVICE_EXTRA_OBJ,
                        gattDataList);
                    Log.d(TAG, " gattDataList  : " + gattDataList.get(0));
                    objMsg.setData(objBundle);
                    Log.d(TAG, " before sendmessage : " + objMsg.what);
                    handler.sendMessage(objMsg);
                } else {
                    Log.e(TAG, "No object paths in the ACTION GATT intent");
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    public static void registerHandler(Handler handle)
    {
        Log.d(TAG, " Registered Thermometer Service Handler ::");
        handler = handle;
        Log.d(TAG,
              " after Registered Thermometer Service Handler : "
              + handler.toString());
    }
}
