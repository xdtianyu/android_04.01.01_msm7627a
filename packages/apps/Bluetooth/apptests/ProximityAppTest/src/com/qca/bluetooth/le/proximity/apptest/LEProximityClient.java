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

package com.qca.bluetooth.le.proximity.apptest;

import java.util.UUID;

import android.app.Activity;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothDevicePicker;
import android.bluetooth.IBluetoothLEProximityServices;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ListView;
import android.widget.Toast;

public class LEProximityClient extends Activity {
    private static final String TAG = "LEProximityClient";

    public static IBluetoothLEProximityServices proximityService = null;

    public static UUID GATTServiceUUID = null;

    public static ParcelUuid GATTServiceParcelUUID = null;

    public static BluetoothDevice RemoteDevice = null;

    public static ListView mListView = null;

    static final String REMOTE_DEVICE = "RemoteDevice";

    public static final String[] StringServicesUUID = {
        "0000180300001000800000805f9b34fb", // Link Loss
        "0000180200001000800000805f9b34fb", // Immediate Alert
        "0000180400001000800000805f9b34fb"}; // TX Power level

    protected static final int DEVICE_SELECTED = 0;

    public static final String GATT_SERVICE_NAME = "GATT_SERVICE_NAME";

    public static final String GATT_SERVICE_LINK_LOSS_SERVICE = "GATT_SERVICE_LINK_LOSS_SERVICE";

    public static final String GATT_SERVICE_IMMEDIATE_ALERT_SERVICE = "GATT_SERVICE_IMMEDIATE_ALERT_SERVICE";

    public static final String GATT_SERVICE_TX_POWER_SERVICE = "GATT_SERVICE_TX_POWER_SERVICE";

    public static Context mainContext = null;

    public static Button buttonLinkLoss = null;

    public static Button buttonImmAlert = null;

    public static Button buttonTxPower = null;

    private ServiceConnection mConnection = new ServiceConnection() {

        public void onServiceConnected(ComponentName className, IBinder service) {
            Log.d(TAG, "**********onServiceConnected***************");
            proximityService = IBluetoothLEProximityServices.Stub.asInterface(service);
        }

        public void onServiceDisconnected(ComponentName name) {
            Log.e(TAG, "*************onServiceDisconnected***********");
            onServiceDisconn();
        }
    };

    public final Handler msgHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
            case DEVICE_SELECTED:
                Log.d(TAG, "device selected");
                RemoteDevice = (BluetoothDevice) msg.getData().getParcelable(
                                                                            REMOTE_DEVICE);
                buttonLinkLoss.setEnabled(true);
                buttonLinkLoss.setClickable(true);
                buttonImmAlert.setEnabled(true);
                buttonImmAlert.setClickable(true);
                buttonTxPower.setEnabled(true);
                buttonTxPower.setClickable(true);
                startGattService(StringServicesUUID[2]);
                break;
            default:
                break;
            }
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.proximitymain);
        Log.d(TAG, "****Set main content view*****");

        mainContext = this.getApplicationContext();
        LEProximityClientReceiver.registerHandler(msgHandler);
        final Button buttonConnect = (Button) findViewById(R.id.buttonConnProximity);
        buttonConnect.setOnClickListener(new View.OnClickListener() {
                                             public void onClick(View v) {
                                                 Log.d(TAG, "Button connect to bt devices clicked");
                                                 bindToProximityService();

                                             }
                                         });
        buttonLinkLoss = (Button) findViewById(R.id.buttonLinkLoss);
        buttonLinkLoss.setEnabled(false);
        buttonLinkLoss.setClickable(false);
        buttonLinkLoss.setOnClickListener(new View.OnClickListener() {
                                              public void onClick(View v) {
                                                  startGattService(StringServicesUUID[0]);
                                              }
                                          });

        buttonImmAlert = (Button) findViewById(R.id.buttonImmAlert);
        buttonImmAlert.setEnabled(false);
        buttonImmAlert.setClickable(false);
        buttonImmAlert.setOnClickListener(new View.OnClickListener() {
                                              public void onClick(View v) {
                                                  startGattService(StringServicesUUID[1]);
                                              }
                                          });

        buttonTxPower = (Button) findViewById(R.id.buttonTxPower);
        buttonTxPower.setEnabled(false);
        buttonTxPower.setClickable(false);
        buttonTxPower.setOnClickListener(new View.OnClickListener() {
                                             public void onClick(View v) {
                                                 Log.d(TAG, "Clicked TX button");
                                                 startGattService(StringServicesUUID[2]);
                                             }
                                         });
    }

    @Override
    public void onPause() {
        super.onPause();
        Log.e(TAG, "****the activity is paused*****");
    }

    @Override
    public void onStop() {
        super.onStop();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.e(TAG, "****the activity is destroyed*****");
        close();
    }

    @Override
    public void onRestart() {
        super.onRestart();
        Log.e(TAG, "****the activity is restart*****");
    }

    public void onServiceConn() {
        Intent in1 = new Intent(BluetoothDevicePicker.ACTION_LAUNCH);
        in1.putExtra(BluetoothDevicePicker.EXTRA_NEED_AUTH, false);
        in1.putExtra(BluetoothDevicePicker.EXTRA_FILTER_TYPE,
                     BluetoothDevicePicker.FILTER_TYPE_ALL);
        /*in1.putExtra(BluetoothDevicePicker.EXTRA_LAUNCH_PACKAGE,
                "com.android.bluetooth.proximity");*/
        in1.putExtra(BluetoothDevicePicker.EXTRA_LAUNCH_PACKAGE,
                     "com.android.proximity");
        in1.putExtra(BluetoothDevicePicker.EXTRA_LAUNCH_CLASS,
                     LEProximityClientReceiver.class.getName());
        in1.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        this.startActivity(in1);
    }

    public void startGattService(String uuidString) {
        Log.d(TAG, "Inside startGattService for : " + uuidString);
        try {
            GATTServiceUUID = convertUUIDStringToUUID(uuidString);
            Log.d(TAG, " GATTServiceUUID = " + GATTServiceUUID);
            GATTServiceParcelUUID = new ParcelUuid(GATTServiceUUID);
            Log.d(TAG, " GATTServiceParcelUUID = " + GATTServiceParcelUUID);
            if (proximityService != null) {
                boolean isGattService = proximityService.startProximityService(RemoteDevice,
                                                                               GATTServiceParcelUUID,
                                                                               LEProximityServicesScreen.mCallback);
                if (!isGattService) {
                    Log.e(TAG, "Proximity service could not get GATT service");
                    Toast.makeText(getApplicationContext(),
                                   "could not start Proximity service",
                                   Toast.LENGTH_SHORT).show();
                } else {
                    Log.d(TAG, "Proximity service got GATT service : "
                          + GATTServiceParcelUUID);
                    Intent in = new Intent();
                    in.setClass(
                               mainContext,
                               LEProximityServicesScreen.class);
                    in.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    if (uuidString.equals(StringServicesUUID[0])) {
                        Log.d(TAG,
                              "Service name .. GATT_SERVICE_LINK_LOSS_SERVICE");
                        in.putExtra(GATT_SERVICE_NAME,
                                    GATT_SERVICE_LINK_LOSS_SERVICE);
                    } else if (uuidString.equals(StringServicesUUID[1])) {
                        Log.d(TAG,
                              "Service name .. GATT_SERVICE_IMMEDIATE_ALERT_SERVICE");
                        in.putExtra(GATT_SERVICE_NAME,
                                    GATT_SERVICE_IMMEDIATE_ALERT_SERVICE);
                    } else if (uuidString.equals(StringServicesUUID[2])) {
                        Log.d(TAG, "Service name .. TX_POWER_SERVICE");
                        in.putExtra(GATT_SERVICE_NAME,
                                    GATT_SERVICE_TX_POWER_SERVICE);
                    } else {
                        Log.e(TAG,
                              "Error uuidString not found in the services list");
                    }

                    mainContext.startActivity(in);
                }
            } else {
                Toast.makeText(getApplicationContext(),
                               "Not connected to service", Toast.LENGTH_SHORT).show();
            }
        } catch (RemoteException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
    }

    public void onServiceDisconn() {
        close();
    }

    public synchronized void close() {
        if (mConnection != null) {
            Log.e(TAG, "unbinding from Proximity service");
            mainContext.unbindService(mConnection);
        }
        mConnection = null;
        RemoteDevice = null;
        GATTServiceParcelUUID = null;
        GATTServiceUUID = null;
        mainContext = null;
        LEProximityServicesScreen.linkLossServiceReady = false;
        LEProximityServicesScreen.immAlertServiceReady = false;
        LEProximityServicesScreen.txPowerServiceReady = false;
    }

    public void bindToProximityService() {
        String className = IBluetoothLEProximityServices.class.getName();
        Log.d(TAG, "class name : " + className);
        Intent in = new Intent(className);
        if (!mainContext.bindService(in, mConnection, Context.BIND_AUTO_CREATE)) {
            Log.e(TAG, "Could not bind to Remote Service");
        } else {
            Log.e(TAG, "Succ bound to Remote Service");
            onServiceConn();
        }
    }

    private UUID convertUUIDStringToUUID(String UUIDStr) {
        if (UUIDStr.length() != 32) {
            return null;
        }
        String uuidMsB = UUIDStr.substring(0, 16);
        String uuidLsB = UUIDStr.substring(16, 32);

        if (uuidLsB.equals("800000805f9b34fb")) {
            // TODO Long is represented as two complement. Fix this later.
            UUID uuid = new UUID(Long.valueOf(uuidMsB, 16), 0x800000805f9b34fbL);
            return uuid;
        } else {
            UUID uuid = new UUID(Long.valueOf(uuidMsB, 16),
                                 Long.valueOf(uuidLsB));
            return uuid;
        }
    }

}
