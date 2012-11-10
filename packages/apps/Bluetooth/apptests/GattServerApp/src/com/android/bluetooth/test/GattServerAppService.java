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

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattAppConfiguration;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothGattCallback;


import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.ParcelFileDescriptor;
import android.os.ParcelUuid;
import android.os.PowerManager;
import android.os.RemoteException;
import android.util.Log;
import android.widget.Toast;

import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.UUID;

import java.util.Date;
import java.util.Calendar;
import java.util.Timer;
import java.util.TimerTask;
import android.bluetooth.BluetoothDevicePicker;

/**
* This class is the service which runs in the background when the
* Gatt Server app is started. This service maintains the attribute
* data in the data structures and processes the different API requests
* from the client device and generates responses to be sent to client device.
*/
public class GattServerAppService extends Service {
    private static final String TAG = "GattServerAppService";

    // Message codes received from the UI client.
    // Register GATT server configuration.
    public static final int MSG_REG_GATT_SERVER_CONFIG = 300;
    // Unregister GATT server configuration.
    public static final int MSG_UNREG_GATT_SERVER_CONFIG = 301;

    public static final int MSG_REG_GATT_SERVER_SUCCESS = 400;
    public static final int MSG_REG_GATT_SERVER_FAILURE = 401;
    public static final int MSG_UNREG_GATT_SERVER_SUCCESS = 500;
    public static final int MSG_UNREG_GATT_SERVER_FAILURE = 501;

    protected static final int DEVICE_SELECTED = 0;
    // Message codes received from the UI client.
    // Register client with this service.
    public static final int MSG_REG_CLIENT = 200;

    // Status codes sent back to the UI client.
    // Application registration complete.
    public static final int STATUS_GATT_SERVER_REG = 100;
    // Application unregistration complete.
    public static final int STATUS_GATT_SERVER_UNREG = 101;

    public static final boolean SERVICE_DEBUG = false;
    public static final int MSG_CONNECT_GATT_SERVER = 600;
    public static final int MSG_DISCONNECT_GATT_SERVER = 601;

    private BluetoothAdapter mBluetoothAdapter;
    private Messenger mClient;
    private Alarm alarm;

    InputStream raw = null;

    public static ArrayList<Attribute> gattHandleToAttributes;

    public static HashMap<String, Attribute> includedServiceMap = new HashMap<String, Attribute>();

    public static HashMap<String, List<Integer>> gattAttribTypeToHandle =
                new HashMap<String, List<Integer>>();

    public static int serverMinHandle = 0;

    public static int serverMaxHandle = -1;

    private GattServiceParser gattServiceParser = null;

    private final BluetoothAdapter bluetoothAdapter = null;

    public Context mContext = null;

    public static BluetoothGattAppConfiguration serverConfiguration = null;

    public static BluetoothGatt gattProfile = null;

    public static final String BLUETOOTH_BASE_UUID = "0000xxxx00001000800000805f9b34fb";

    public boolean is_registered = false;

    public boolean isAlarmStarted = false;

    static final String REMOTE_DEVICE = "RemoteDevice";

    public static BluetoothDevice remoteDevice = null;

    public static ArrayList<BluetoothDevice> connectedDevicesList;

    // Handles events sent by GattServerAppActivity.
    private class IncomingHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            Context context = getApplicationContext();
            CharSequence text = null;
            int duration = 0;
            Toast toast = null;
            switch (msg.what) {
                // Register Gatt Server configuration.
                case MSG_REG_GATT_SERVER_CONFIG:
                    registerApp();
                    break;
                // Unregister Gatt Server configuration.
                case MSG_UNREG_GATT_SERVER_CONFIG:
                    unregisterApp();
                    break;
                // Connect
                case MSG_CONNECT_GATT_SERVER:
                    connect();
                    break;
                // Disconnect
                case MSG_DISCONNECT_GATT_SERVER:
                    disconnect();
                    break;
                case MSG_REG_GATT_SERVER_SUCCESS:
                    text = "GATT Server registration was successful!";
                    duration = Toast.LENGTH_LONG;
                    toast = Toast.makeText(context, text, duration);
                    toast.show();
                    break;
                case MSG_REG_GATT_SERVER_FAILURE:
                    text = "GATT Server registration was not successful!";
                    duration = Toast.LENGTH_LONG;
                    toast = Toast.makeText(context, text, duration);
                    toast.show();
                    break;
                case MSG_UNREG_GATT_SERVER_SUCCESS:
                    text = "GATT Server Unregistration was successful!";
                    duration = Toast.LENGTH_LONG;
                    toast = Toast.makeText(context, text, duration);
                    toast.show();
                    break;
                case MSG_UNREG_GATT_SERVER_FAILURE:
                    text = "GATT Server Unregistration was not successful!";
                    duration = Toast.LENGTH_LONG;
                    toast = Toast.makeText(context, text, duration);
                    toast.show();
                    break;
                case DEVICE_SELECTED:
                    remoteDevice = (BluetoothDevice) msg.getData().getParcelable(REMOTE_DEVICE);
                    connectLEDevice();
                    break;
                default:
                    super.handleMessage(msg);
            }
        }
    }

    final Messenger mMessenger = new Messenger(new IncomingHandler());

    /**
     * Make sure Bluetooth and Gatt profile are available on the Android device.  Stop service
     * if they are not available.
     */
    @Override
    public void onCreate() {
        String FILENAME = "genericservice.xml";
        super.onCreate();
        alarm = new Alarm();
        mContext = this;
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        if (mBluetoothAdapter == null || !mBluetoothAdapter.isEnabled()) {
            // Bluetooth adapter isn't available.  The client of the service is supposed to
            // verify that it is available and activate before invoking this service.
            stopSelf();
            return;
        }
        if (!mBluetoothAdapter.getProfileProxy(this, mBluetoothServiceListener,
                BluetoothProfile.GATT)) {
            stopSelf();
            return;
        }

        populateGattAttribTypeMap();

        gattServiceParser = new GattServiceParser();
        try {
            raw = new BufferedInputStream(new FileInputStream("/system/bin/"+FILENAME));
        }
        catch (FileNotFoundException e) {
            e.printStackTrace();
        }
        catch (IOException e) {
            e.printStackTrace();
        }
        if (raw != null) {
            Log.d(TAG, "Inside the Service.. XML is read");
            gattServiceParser.parse(raw);

            //update data structures from characteristic_values file
            updateDataStructuresFromFile();

            if(SERVICE_DEBUG) {
                Log.d(TAG, "Attribute data list");
                Log.d(TAG, "Messages length : " + gattHandleToAttributes.size());
                for (int i = 0; i < gattHandleToAttributes.size(); i++) {
                    Attribute attr = gattHandleToAttributes.get(i);
                    Log.d(TAG, "Attirbute handle " + i);
                    Log.d(TAG, "Attirbute name : " + attr.name);
                    Log.d(TAG, " handle : " + attr.handle);
                    Log.d(TAG, " type : " + attr.type);
                    Log.d(TAG, " uuid : "+ attr.uuid);
                    Log.d(TAG, " permission : " + attr.permission);
                    Log.d(TAG, " Permission Bits: " + attr.permBits);
                    Log.d(TAG, " properties : " + attr.properties);
                    Log.d(TAG, " start handle : " + attr.startHandle);
                    Log.d(TAG, " end handle : " + attr.endHandle);
                    Log.d(TAG, " ref handle : "     + attr.referenceHandle);
                    if (attr.value != null) {
                        Log.d(TAG, "The attribute value is ::");
                        for(int z=0; z < attr.value.length; z++) {
                            Log.d(TAG, ""+attr.value[z]);
                        }
                    }
                    Log.d(TAG, " min range : " + attr.min_range);
                    Log.d(TAG, " max range : " + attr.max_range);
                }
                for(Map.Entry<String, List<Integer>> entry : gattAttribTypeToHandle.entrySet()) {
                    Log.d(TAG,"gattAttribTypeToHandle KEY : " + entry.getKey());
                    Log.d(TAG,"gattAttribTypeToHandle VALUE : " + entry.getValue());
                }

                Log.d(TAG, "Server MIN RANGE : " + serverMinHandle);
                Log.d(TAG, "Server MAX RANGE : " + serverMaxHandle);
            }
        }
        //Register the Server app with frameworks
        registerApp();
        //Register receiver handler
        GattServerAppReceiver.registerHandler(new IncomingHandler());
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "onStart Command of GattServerAppService called");
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mMessenger.getBinder();
    };

    // Register Gatt server application through Bluetooth Gatt API.
    private void registerApp() {
        Log.d(TAG, "Register Server config called::");
        gattProfile.registerServerConfiguration(
                "GATTServerTest",
                serverMaxHandle+1,
                bluetoothGattCallBack);

    }

    // Unregister Gatt server application through Bluetooth Gatt API.
    private void unregisterApp() {
        Log.d(TAG, "Unregister Server config called::");
        gattProfile.unregisterServerConfiguration(serverConfiguration);
    }

    // Connect to client.
    private void connect() {
        Log.d(TAG, "Connect called::");
        //DevicePicker call
        selectDevice();
    }

    // Disconnect
    private void disconnect() {
        Log.d(TAG, "Disconnect called::");
        //DevicePicker call
        selectConnectedDevice();
    }

    // Callbacks to handle connection set up and disconnection clean up.
    private final BluetoothProfile.ServiceListener mBluetoothServiceListener =
            new BluetoothProfile.ServiceListener() {
        @Override
        public void onServiceConnected(int profile, BluetoothProfile proxy) {
            if (profile == BluetoothProfile.GATT) {
                gattProfile = (BluetoothGatt) proxy;
                if (Log.isLoggable(TAG, Log.DEBUG))
                    Log.d(TAG, "onServiceConnected to profile: " + profile);
            }
        }

        @Override
        public void onServiceDisconnected(int profile) {
            if (profile == BluetoothProfile.GATT) {
                Log.d(TAG, "onServiceDisconnected to profile: " + profile);
                gattProfile = null;
            }
        }
    };

    /**
     * Callback to handle application registration, unregistration events and other
     * API requests coming from the client device.
    */
    private final BluetoothGattCallback bluetoothGattCallBack = new BluetoothGattCallback() {
        @Override
        public void onGattAppConfigurationStatusChange(BluetoothGattAppConfiguration config,
                int status) {
            Log.d(TAG, "onGattAppConfigurationStatusChange: " + config + "Status: " + status);
            serverConfiguration = config;

            switch(status) {
                case BluetoothGatt.GATT_CONFIG_REGISTRATION_SUCCESS:
                    sendMessage(GattServerAppService.MSG_REG_GATT_SERVER_SUCCESS, 0);
                    is_registered = true;
                    break;
                case BluetoothGatt.GATT_CONFIG_REGISTRATION_FAILURE:
                    sendMessage(GattServerAppService.MSG_REG_GATT_SERVER_FAILURE, 0);
                    is_registered = false;
                    break;
                case BluetoothGatt.GATT_CONFIG_UNREGISTRATION_SUCCESS:
                    sendMessage(GattServerAppService.MSG_UNREG_GATT_SERVER_SUCCESS, 0);
                    is_registered = false;
                    break;
                case BluetoothGatt.GATT_CONFIG_UNREGISTRATION_FAILURE:
                    sendMessage(GattServerAppService.MSG_UNREG_GATT_SERVER_FAILURE, 0);
                    is_registered = true;
                    break;
            }
        }

        public void onGattActionComplete(String action, int status) {
            Log.d(TAG, "onGattActionComplete: " + action + "Status: " + status);
        }

        /**
         * Processes the Discover Primary Services Request from client and sends the response
         * to the client.
        */
        @Override
        public void onGattDiscoverPrimaryServiceRequest(BluetoothGattAppConfiguration config,
                int startHandle, int endHandle, int requestHandle) {
            int j, k, hdlFoundStatus =0;
            int startAttrHdl = 0, endAttrHdl = 0;
            int status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
            ParcelUuid uuid = null;
            String uuid1=null;
            boolean retVal;
            List<Integer> hndlList = null;
            if(gattAttribTypeToHandle != null) {
                for(Map.Entry<String, List<Integer>> entry : gattAttribTypeToHandle.entrySet()) {
                    if("00002800-0000-1000-8000-00805F9B34FB".
                            equalsIgnoreCase(entry.getKey().toString())) {
                        //List of primary service handles
                        hndlList = entry.getValue();
                    }
                }
            }
            if(hndlList != null) {
                for(j=0; j< hndlList.size(); j++) {
                    int handle = hndlList.get(j);
                    if(handle >= 0) {
                        if((handle >= startHandle) && (handle <= endHandle)){
                            hdlFoundStatus = 1;
                            if(gattHandleToAttributes != null) {
                                //To get the attribute values for the particular handle
                                for(k=0; k<gattHandleToAttributes.size(); k++) {
                                    if(handle == gattHandleToAttributes.get(k).handle) {
                                        Attribute attr = gattHandleToAttributes.get(k);
                                        startAttrHdl = attr.startHandle;
                                        endAttrHdl = attr.endHandle;
                                        uuid1 = attr.uuid;
                                        if(attr.uuid!=null) {
                                            uuid = ParcelUuid.fromString(attr.uuid);
                                        }
                                        status = BluetoothGatt.GATT_SUCCESS;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if(hdlFoundStatus == 1) {
                        status = BluetoothGatt.GATT_SUCCESS;
                        break;
                    }
                    if(j == (hndlList.size()-1)) {
                        status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
                        break;
                    }
                }
            }

            Log.d(TAG, "Results of onGattDiscoverPrimaryServiceRequest ::"+" status ::"+status +
                    " startAttrHdl ::"+startAttrHdl+" endAttrHdl ::"+endAttrHdl +
                    " Service uuid Parcel UUId::"+uuid + " Service uuid String ::"+uuid1);

            retVal = gattProfile.discoverPrimaryServiceResponse(config, requestHandle, status,
                    startAttrHdl, endAttrHdl, uuid);
            if(SERVICE_DEBUG) {
                Log.d(TAG, "onGattDiscoverPrimaryServiceRequest response: " + retVal);
            }
        }

        /**
         * Processes the Discover Primary Services by UUID Request from client and sends the
         * response to the client.
        */
        @Override
        public void onGattDiscoverPrimaryServiceByUuidRequest(BluetoothGattAppConfiguration config,
                int startHandle, int endHandle, ParcelUuid uuid, int requestHandle) {
            int j, k, hdlFoundStatus =0;
            int startAttrHdl = 0, endAttrHdl = 0;
            int status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
            boolean retVal;
            List<Integer> hndlList = null;
            if(gattAttribTypeToHandle != null) {
                for(Map.Entry<String, List<Integer>> entry : gattAttribTypeToHandle.entrySet()) {
                    if("00002800-0000-1000-8000-00805F9B34FB".
                            equalsIgnoreCase(entry.getKey().toString())) {
                        //List of primary service handles
                        hndlList = entry.getValue();
                    }
                }
            }
            if(hndlList != null) {
                for(j=0; j< hndlList.size(); j++) {
                    int handle = hndlList.get(j);
                    if(handle >= 0) {
                        if((handle >= startHandle) && (handle <= endHandle)){
                            if(gattHandleToAttributes != null) {
                                //To get the attribute values for the particular handle
                                for(k=0; k<gattHandleToAttributes.size(); k++) {
                                    if(handle == gattHandleToAttributes.get(k).handle) {
                                        Attribute attr = gattHandleToAttributes.get(k);
                                        startAttrHdl = attr.startHandle;
                                        endAttrHdl = attr.endHandle;
                                        if(attr.uuid != null &&
                                                attr.uuid.equalsIgnoreCase(uuid.toString())) {
                                            hdlFoundStatus = 1;
                                            status = BluetoothGatt.GATT_SUCCESS;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if(hdlFoundStatus == 1) {
                        status = BluetoothGatt.GATT_SUCCESS;
                        break;
                    }
                    if(j == (hndlList.size()-1)) {
                        status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
                        break;
                    }
                }
            }
            Log.d(TAG, "Results of onGattDiscoverPrimaryServiceByUuidRequest ::"+" status ::"+status+
                    " startAttrHdl ::"+startAttrHdl+" endAttrHdl ::"+endAttrHdl+" Service uuid Parcel Uuid::"+uuid);

            retVal = gattProfile.discoverPrimaryServiceByUuidResponse(config, requestHandle, status,
                    startAttrHdl, endAttrHdl, uuid);
            if(SERVICE_DEBUG) {
                Log.d(TAG, "onGattDiscoverPrimaryServiceByUuidRequest response: " + retVal);
            }
        }

        /**
         * Processes the Find Included Services Request from client and sends the response
         * to the client.
        */
        @Override
        public void onGattFindIncludedServiceRequest(BluetoothGattAppConfiguration config,
                int startHandle, int endHandle, int requestHandle) {
            int j, k, hdlFoundStatus =0;
            int inclSvcHdl = 0, startInclSvcHdl = 0, endInclSvcHdl = 0;
            int status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
            boolean retVal;
            String svcUuid, inclSvcUuid = null;
            ParcelUuid pInclSvcUuid = null;
            List<Integer> hndlList = null;
            if(gattAttribTypeToHandle != null) {
                for(Map.Entry<String, List<Integer>> entry : gattAttribTypeToHandle.entrySet()) {
                    if("00002802-0000-1000-8000-00805F9B34FB".
                            equalsIgnoreCase(entry.getKey().toString())) {
                        //List of included service handles
                        hndlList = entry.getValue();
                    }
                }
            }
            if(hndlList != null) {
                for(j=0; j< hndlList.size(); j++) {
                    int handle = hndlList.get(j);
                    if(handle >= 0) {
                        if((handle >= startHandle) && (handle <= endHandle)){
                            hdlFoundStatus = 1;
                            if(gattHandleToAttributes != null) {
                                //To get the attribute values for the particular handle
                                for(k=0; k<gattHandleToAttributes.size(); k++) {
                                    if(handle == gattHandleToAttributes.get(k).handle) {
                                        Attribute attr = gattHandleToAttributes.get(k);
                                        svcUuid = attr.uuid;
                                        inclSvcHdl = attr.handle;
                                        startInclSvcHdl = attr.startHandle;
                                        endInclSvcHdl = attr.endHandle;
                                        inclSvcUuid = attr.uuid;
                                        pInclSvcUuid = ParcelUuid.fromString(inclSvcUuid);
                                        status = BluetoothGatt.GATT_SUCCESS;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if(hdlFoundStatus == 1) {
                        status = BluetoothGatt.GATT_SUCCESS;
                        break;
                    }
                    if(j == (hndlList.size()-1)) {
                        status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
                        break;
                    }
                }
            }

            Log.d(TAG, "Results of onGattFindIncludedServiceRequest ::"+" status ::"+status+
                    " inclSvcHdl ::"+inclSvcHdl+" startInclSvcHdl ::"+startInclSvcHdl+
                    " endInclSvcHdl ::"+endInclSvcHdl+" Service uuid str::"+inclSvcUuid+
                    " Service uuid Parcel Uuid::"+pInclSvcUuid);

            retVal = gattProfile.findIncludedServiceResponse(config, requestHandle, status,
                    inclSvcHdl, startInclSvcHdl, endInclSvcHdl, pInclSvcUuid);
            if(SERVICE_DEBUG) {
                Log.d(TAG, "onGattFindIncludedServiceRequest response: " + retVal);
            }
        }

        /**
         * Processes the Find Info request from client and sends the
         * response to the client.
        */
        @Override
        public void onGattFindInfoRequest(BluetoothGattAppConfiguration
                config, int startHandle, int endHandle, int requestHandle) {
            int index;
            int status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
            boolean retVal;
            ParcelUuid respUuid = null;
            int respHandle= -1;
            if(gattHandleToAttributes != null) {
                //To get the attribute values
                for(index=0; index<gattHandleToAttributes.size(); index++) {
                    Attribute attrCurr = gattHandleToAttributes.get(index);
                    if(attrCurr.handle >= startHandle && attrCurr.handle <= endHandle) {
                        respHandle = attrCurr.handle;
                        respUuid = ParcelUuid.fromString(attrCurr.type);
                        status = BluetoothGatt.GATT_SUCCESS;
                        break;
                    }
                    if(index == (gattHandleToAttributes.size()-1)) {
                        status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
                        break;
                    }
                }
            }

            Log.d(TAG, "Results of onGattFindInfoRequest ::"+" status ::"+status+
                    " Attribute Handle ::"+respHandle+" Attribute UUID::"+respUuid);

            retVal = gattProfile.findInfoResponse(config, requestHandle,
                    status, respHandle, respUuid);
            if(SERVICE_DEBUG) {
                Log.d(TAG, "findInfoResponse: " + retVal);
            }
        }

        /**
         * Processes the Discover Characteristics Request from client and sends the response
         * to the client.
        */
        @Override
        public void onGattDiscoverCharacteristicRequest(BluetoothGattAppConfiguration config,
                int startHandle, int endHandle, int requestHandle) {
            int j, k, hdlFoundStatus =0;
            int charHdl = 0, charValueHdl = 0;
            int status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
            ParcelUuid charUuid = null;
            byte charProperty = 0;
            boolean retVal;
            List<Integer> hndlList = null;
            if(gattAttribTypeToHandle != null) {
                for(Map.Entry<String, List<Integer>> entry : gattAttribTypeToHandle.entrySet()) {
                    if("00002803-0000-1000-8000-00805F9B34FB".
                            equalsIgnoreCase(entry.getKey().toString())) {
                        //List of characteristic handles
                        hndlList = entry.getValue();
                    }
                }
            }
            if(hndlList != null) {
                for(j=0; j< hndlList.size(); j++) {
                    int handle = hndlList.get(j);
                    if(handle >= 0) {
                        if((handle >= startHandle) && (handle <= endHandle)){
                            hdlFoundStatus = 1;
                            if(gattHandleToAttributes != null) {
                                //To get the attribute values for the particular handle
                                for(k=0; k<gattHandleToAttributes.size(); k++) {
                                    if(handle == gattHandleToAttributes.get(k).handle) {
                                        Attribute attr = gattHandleToAttributes.get(k);
                                        charHdl = attr.handle;
                                        charProperty = (byte)attr.properties;
                                        if(attr.uuid!=null) {
                                            charUuid = ParcelUuid.fromString(attr.uuid);
                                        }
                                        if((k+1) < gattHandleToAttributes.size()) {
                                            charValueHdl = gattHandleToAttributes.get(k+1).handle;
                                        }
                                        status = BluetoothGatt.GATT_SUCCESS;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if(hdlFoundStatus == 1) {
                        status = BluetoothGatt.GATT_SUCCESS;
                        break;
                    }
                    if(j == (hndlList.size()-1)) {
                        status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
                        break;
                    }
                }
            }

            Log.d(TAG, "Results of onGattDiscoverCharacteristicRequest ::"+" status ::"+status+
                    " Characteristic Handle ::"+charHdl+" Characteristic Property ::"+charProperty+
                    " Characteristic Vlaue Handle::"+charValueHdl+" Characteristic UUID ::"+charUuid);

            retVal = gattProfile.discoverCharacteristicResponse(config, status, requestHandle,
                    charHdl, charProperty, charValueHdl, charUuid);
            if(SERVICE_DEBUG) {
                Log.d(TAG, "onGattDiscoverCharacteristicRequest response: " + retVal);
            }
        }

        /**
         * Processes the Read By Attribute Type Request from client and sends the response
         * to the client.
        */
        @Override
        public void onGattReadByTypeRequest(BluetoothGattAppConfiguration config, ParcelUuid uuid,
                int startHandle, int endHandle, String authentication, int requestHandle) {
            int i, j, k, hdlFoundStatus=0, status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
            boolean retVal;
            String attrUuidStr;
            int attrHandle=-1, startAttrHdl =-1, endAttrHdl=-1, attrHandleNext =-1;
            String uuidStr=null;
            byte attrPermission=0;
            byte[] payload = null;
            byte attrProperties = 0;
            String attrUuidPrev = null;
            String attrTypePrev = null;
            byte[] attrValue = null;
            String attrTypeStr = null;

            List<Integer> hndlList = null;
            String attributeType = uuid.toString();
            int charValueAttrType = 0;
            boolean is_permission_available = false;
            int security_status_code = 0;//1 means not authorized, 2 means not authenticated
            int handle = -1;

            //update data structures from characteristic_values file
            updateDataStructuresFromFile();

            if(gattAttribTypeToHandle != null) {
                for(Map.Entry<String, List<Integer>> entry : gattAttribTypeToHandle.entrySet()) {
                    if(attributeType.equalsIgnoreCase(entry.getKey().toString())) {
                        //List of attribute type handles
                        hndlList = entry.getValue();
                    }
                }
            }
            if(hndlList != null) {
                for(j=0; j< hndlList.size(); j++) {
                    handle = hndlList.get(j);
                    if(handle >= 0) {
                        if((handle >= startHandle) && (handle <= endHandle)){
                            hdlFoundStatus = 1;
                            if(gattHandleToAttributes != null) {
                                //To get the attribute values for the particular handle
                                for(k=0; k<gattHandleToAttributes.size(); k++) {
                                    if(handle == gattHandleToAttributes.get(k).handle) {
                                        Attribute attr = gattHandleToAttributes.get(k);
                                        attrTypeStr = attr.type;
                                        attrPermission = attr.permBits;

                                        //if the attribute value is authorized/authenticated
                                        if((attrPermission > 0) &&
                                                ((attrPermission & 0x01) == 0x01)) {
                                            if(authentication.equalsIgnoreCase("Authenticated") ||
                                                    authentication.equalsIgnoreCase("Authorized")) {
                                                is_permission_available = true;
                                            }
                                            else {
                                                //not authorized
                                                security_status_code = 1;
                                            }
                                        }
                                        else if((attrPermission > 0) &&
                                                ((attrPermission & 0x02) == 0x02)) {
                                            if(authentication.equalsIgnoreCase("Authenticated")) {
                                                is_permission_available = true;
                                            }
                                            else {
                                                //not authenticated
                                                security_status_code = 2;
                                            }
                                        }
                                        else if(attrPermission == 0 ||
                                                (((attrPermission & 0x01) != 0x01)
                                                && ((attrPermission & 0x02) != 0x02))) {
                                            is_permission_available = true;
                                        }
                                        if((k+1) < gattHandleToAttributes.size()) {
                                            attrHandleNext= gattHandleToAttributes.
                                                    get(k+1).handle;
                                        }
                                        if((k-1) >= 0) {
                                            attrUuidPrev = gattHandleToAttributes.get(k-1).uuid;
                                            attrTypePrev = gattHandleToAttributes.get(k-1).type;
                                            if(attrTypeStr.equalsIgnoreCase(attrUuidPrev) &&
                                                    attrTypePrev.
                                                    equalsIgnoreCase("00002803-0000-1000-8000-00805F9B34FB")) {
                                                charValueAttrType = 1;
                                                attrProperties = (byte)gattHandleToAttributes.get(k-1).properties;
                                            }
                                            else {
                                                attrProperties = (byte)attr.properties;
                                            }
                                        }
                                        if(is_permission_available) {
                                            if(attrTypeStr !=null &&
                                                    (attrTypeStr.equalsIgnoreCase("00002800-0000-1000-8000-00805F9B34FB") ||
                                                    attrTypeStr.equalsIgnoreCase("00002801-0000-1000-8000-00805F9B34FB") ||
                                                    attrTypeStr.equalsIgnoreCase("00002802-0000-1000-8000-00805F9B34FB"))) {
                                                attrHandle = attr.handle;
                                                startAttrHdl = attr.startHandle;
                                                endAttrHdl = attr.endHandle;
                                                attrValue = attr.value;
                                                uuidStr = attr.uuid;
                                                status = BluetoothGatt.GATT_SUCCESS;
                                            }
                                            else if(attrTypeStr !=null &&
                                                    !(attrTypeStr.equalsIgnoreCase("00002800-0000-1000-8000-00805F9B34FB") ||
                                                    attrTypeStr.equalsIgnoreCase("00002801-0000-1000-8000-00805F9B34FB") ||
                                                    attrTypeStr.equalsIgnoreCase("00002802-0000-1000-8000-00805F9B34FB"))){
                                                    //need to check whether attribute is readable before reading
                                                attrHandle = attr.handle;
                                                if((attrProperties > 0) && ((attrProperties & 0x02) == 0x02)) {
                                                    startAttrHdl = attr.startHandle;
                                                    endAttrHdl = attr.endHandle;
                                                    attrValue = attr.value;
                                                    uuidStr = attr.uuid;
                                                    status = BluetoothGatt.GATT_SUCCESS;
                                                }
                                                else {
                                                    uuid = null;
                                                    status = BluetoothGatt.ATT_READ_NOT_PERM;
                                                }
                                            }
                                        }
                                        else {
                                            if(security_status_code == 1) {
                                                status = BluetoothGatt.ATT_AUTHORIZATION;
                                            }
                                            else if(security_status_code == 2) {
                                                status = BluetoothGatt.ATT_AUTHENTICATION;
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if(hdlFoundStatus == 1) {
                        //status should be success only when authentication levels are satisfied
                        if(is_permission_available) {
                            if(attrTypeStr !=null &&
                                    (attrTypeStr.equalsIgnoreCase("00002800-0000-1000-8000-00805F9B34FB") ||
                                    attrTypeStr.equalsIgnoreCase("00002801-0000-1000-8000-00805F9B34FB") ||
                                    attrTypeStr.equalsIgnoreCase("00002802-0000-1000-8000-00805F9B34FB"))) {
                                status = BluetoothGatt.GATT_SUCCESS;
                            }
                            else if(attrTypeStr !=null &&
                                    !(attrTypeStr.equalsIgnoreCase("00002800-0000-1000-8000-00805F9B34FB") ||
                                    attrTypeStr.equalsIgnoreCase("00002801-0000-1000-8000-00805F9B34FB") ||
                                    attrTypeStr.equalsIgnoreCase("00002802-0000-1000-8000-00805F9B34FB"))){
                                    //need to check whether attribute is readable before reading
                                if((attrProperties > 0) && ((attrProperties & 0x02) == 0x02)) {
                                    status = BluetoothGatt.GATT_SUCCESS;
                                }
                            }
                        }
                        break;
                    }
                    else if(j == (hndlList.size()-1)) {
                        status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
                        break;
                    }
                }
            }

            if(is_permission_available) {
                if(attributeType != null && attributeType.length()>0) {
                    //Primary Service definition
                    if(attributeType.equalsIgnoreCase("00002800-0000-1000-8000-00805F9B34FB") ||
                            attributeType.equalsIgnoreCase("00002801-0000-1000-8000-00805F9B34FB")){
                        int cnt =0;
                        payload = new byte[16];
                        //Primary service UUID
                        uuidStr = removeChar(uuidStr, '-');
                        byte[] bytes = new byte[16];
                        bytes = hexStringToByteArray(uuidStr);

                        if(bytes!=null) {
                            for(i=((bytes.length)-1);i >= 0; i--) {
                                payload[cnt++] = bytes[i];
                            }
                        }
                        if(SERVICE_DEBUG) {
                            Log.d(TAG,"The payload data::");
                            if(payload != null) {
                                for(i=0; i < payload.length; i++) {
                                    Log.d(TAG,"\n"+payload[i]);
                                }
                            }
                        }
                    }
                    //Included Service definition
                    else if(attributeType.equalsIgnoreCase("00002802-0000-1000-8000-00805F9B34FB")){
                        int cnt =0;
                        payload = new byte[20];
                        //Included service attr handle
                        byte startHdlLsb = (byte)(startAttrHdl & 0x00FF);
                        byte startHdlMsb = (byte)((startAttrHdl & 0xFF00) >> 8);
                        payload[cnt++] = startHdlLsb;
                        payload[cnt++] = startHdlMsb;

                        //End group handle
                        byte endHdlLsb = (byte)(endAttrHdl & 0x00FF);
                        byte endHdlMsb = (byte)((endAttrHdl & 0xFF00) >> 8);
                        payload[cnt++] = endHdlLsb;
                        payload[cnt++] = endHdlMsb;

                        //service uuid
                        uuidStr = removeChar(uuidStr, '-');
                        byte[] bytes = new byte[16];
                        bytes = hexStringToByteArray(uuidStr);

                        if(bytes!=null) {
                            for(i=((bytes.length)-1);i >= 0; i--) {
                                payload[cnt++] = bytes[i];
                            }
                        }

                        if(SERVICE_DEBUG) {
                            Log.d(TAG,"The payload data::");
                            if(payload != null) {
                                for(i=0; i < payload.length; i++) {
                                    Log.d(TAG,"\n"+payload[i]);
                                }
                            }
                        }
                    }
                    //Characteristic declaration
                    else if(attributeType.equalsIgnoreCase("00002803-0000-1000-8000-00805F9B34FB")){
                        if(((attrProperties > 0) && ((attrProperties & 0x02) == 0x02))) {
                            int cnt = 0;
                            payload = new byte[19];

                            //Characteristic properties
                            payload[cnt++] = attrProperties;

                            //Characteristic value attribute handle
                            byte hdlLsb = (byte)(attrHandleNext & 0x00FF);
                            byte hdlMsb = (byte)((attrHandleNext & 0xFF00) >> 8);
                            payload[cnt++] = hdlLsb;
                            payload[cnt++] = hdlMsb;

                            //Characteristic uuid
                            uuidStr = removeChar(uuidStr, '-');
                            byte[] bytes = new byte[16];
                            bytes = hexStringToByteArray(uuidStr);

                            if(bytes!=null) {
                                for(i=((bytes.length)-1);i >= 0; i--) {
                                    payload[cnt++] = bytes[i];
                                }
                            }

                            if(SERVICE_DEBUG) {
                                Log.d(TAG,"The payload data::");
                                if(payload != null) {
                                    for(i=0; i < payload.length; i++) {
                                        Log.d(TAG,"\n"+payload[i]);
                                    }
                                }
                            }
                        }
                    }
                    //Characteristic Value declaration/ Descriptors
                    else if(charValueAttrType == 1 ||
                            attributeType.equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")
                            || attributeType.equalsIgnoreCase("00002900-0000-1000-8000-00805F9B34FB")
                            || attributeType.equalsIgnoreCase("00002901-0000-1000-8000-00805F9B34FB")
                            || attributeType.equalsIgnoreCase("00002904-0000-1000-8000-00805F9B34FB")) {
                        if(((attrProperties > 0) && ((attrProperties & 0x02) == 0x02))) {
                            List<Byte> byteArrList= new ArrayList<Byte>();
                            int cnt = 0;
                            //Characteristic Value
                            if(attrValue!=null && attrValue.length > 0) {
                                for(i=0; i< attrValue.length; i++) {
                                    byteArrList.add(attrValue[i]);
                                }
                            }
                            if(attrTypeStr.equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")) {
                                payload = new byte[2];
                                payload[0] = 0x00;
                                payload[1] = 0x00;
                                if(SERVICE_DEBUG) {
                                    Log.d(TAG,"The payload data::");
                                    if(payload != null) {
                                        for(i=0; i < payload.length; i++) {
                                            Log.d(TAG,""+payload[i]);
                                        }
                                    }
                                }
                            }
                            else {
                                payload = new byte[byteArrList.size()];
                                //Transfer Arraylist contents to byte array
                                for(i=(byteArrList.size()-1); i >= 0; i--) {
                                    payload[cnt++] = byteArrList.get(i).byteValue();
                                }
                                if(SERVICE_DEBUG) {
                                    Log.d(TAG,"The payload data::");
                                    if(payload != null) {
                                        for(i=0; i < payload.length; i++) {
                                            Log.d(TAG,"\n"+payload[i]);
                                        }
                                    }
                                }
                            }
                            if(attrTypeStr.equalsIgnoreCase("00002901-0000-1000-8000-00805F9B34FB")) {
                                String attrValStr = new String(payload);
                                Log.d(TAG, attrValStr);
                            }
                        }
                    }
                    //Characteristic Aggregate Format descriptor
                    else if(attributeType.equalsIgnoreCase("00002905-0000-1000-8000-00805F9B34FB")) {
                        int charHandle = -1;
                        int charStartHdl = -1;
                        int charEndHdl = -1;
                        if(((attrProperties > 0) && ((attrProperties & 0x02) == 0x02))) {
                            List<Integer> descHdlList = null;
                            List<Byte> byteArrList= new ArrayList<Byte>();
                            int cnt = 0;
                            if(gattAttribTypeToHandle != null) {
                                for(Map.Entry<String, List<Integer>> entry : gattAttribTypeToHandle.entrySet()) {
                                    if("00002904-0000-1000-8000-00805F9B34FB".
                                            equalsIgnoreCase(entry.getKey().toString())) {
                                        //List of characteristic presentation format handles
                                        descHdlList = entry.getValue();
                                    }
                                }
                            }
                            if(gattHandleToAttributes != null) {
                                Attribute attrDesc = gattHandleToAttributes.get(handle);
                                Attribute attrCharValue = gattHandleToAttributes.get(attrDesc.referenceHandle);
                                Attribute attrChar = gattHandleToAttributes.get(attrCharValue.referenceHandle);
                                charHandle = attrChar.handle;
                                charStartHdl = attrChar.startHandle;
                                charEndHdl = attrChar.endHandle;
                            }
                            if(descHdlList != null) {
                                byteArrList = new ArrayList<Byte>();
                                //convert Int ArryList to byte ArrayList
                                for(int index=0; index<descHdlList.size(); index++) {
                                    int descHdl = descHdlList.get(index);
                                    if((descHdl >= charStartHdl) && (descHdl <= charEndHdl)) {
                                        byte msbDescHdl = (byte)(descHdl & 0xFF00);
                                        byte lsbDescHdl = (byte)(descHdl & 0x00FF);
                                        byteArrList.add(lsbDescHdl);
                                        byteArrList.add(msbDescHdl);
                                    }
                                }

                                payload = new byte[byteArrList.size()];
                                //Transfer Arraylist contents to byte array
                                for(i=0; i < byteArrList.size();i++) {
                                    payload[cnt++] = byteArrList.get(i).byteValue();
                                }
                            }
                            if(SERVICE_DEBUG) {
                                Log.d(TAG,"The payload data::");
                                if(payload != null) {
                                    for(i=0; i < payload.length; i++) {
                                        Log.d(TAG,"\n"+payload[i]);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            retVal = gattProfile.readByTypeResponse(config, requestHandle, status, uuid,
                    attrHandle, payload);
            Log.d(TAG, "onGattReadByTypeRequest Response: " + retVal);
        }

        /**
         * Processes the Read Request from client and sends the response
         * to the client.
        */
        @Override
        public void onGattReadRequest(BluetoothGattAppConfiguration config, int handle,
                String authentication, int requestHandle) {
            int i, k, hdlFoundStatus=0, status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
            boolean retVal;
            byte[] payload = null;
            byte[] attrValue = null;
            String attrTypeStr = null;
            ParcelUuid uuid = null;
            byte attrPermission = 0;
            boolean is_permission_available = false;
            String attrUuidPrev, attrTypePrev;
            byte attrProperties = 0;
            int charValueAttrType = 0;
            int startAttrHdl = -1;
            int endAttrHdl = -1;
            String uuidStr = null;
            int attrHandleNext = -1;
            int security_status_code = 0;//1 means not authorized, 2 means not authenticated

            //update data structures from characteristic_values file
            updateDataStructuresFromFile();

            if(handle >= 0) {
                if(gattHandleToAttributes != null) {
                    //To get the attribute values for the particular handle
                    for(k=0; k<gattHandleToAttributes.size(); k++) {
                        if(handle == gattHandleToAttributes.get(k).handle) {
                            hdlFoundStatus = 1;
                            Attribute attr = gattHandleToAttributes.get(k);
                            attrPermission = attr.permBits;
                            attrTypeStr = attr.type;
                            //if the attribute value is authorized/authenticated
                            if((attrPermission > 0) && ((attrPermission & 0x01) == 0x01)) {
                                if(authentication.equalsIgnoreCase("Authenticated") ||
                                        authentication.equalsIgnoreCase("Authorized")) {
                                    is_permission_available = true;
                                }
                                else {
                                    //not authorized
                                    security_status_code = 1;
                                }
                            }
                            else if((attrPermission > 0) && ((attrPermission & 0x02) == 0x02)) {
                                if(authentication.equalsIgnoreCase("Authenticated")) {
                                    is_permission_available = true;
                                }
                                else {
                                    //not authenticated
                                    security_status_code = 2;
                                }
                            }
                            else if(attrPermission == 0 || (((attrPermission & 0x01) != 0x01)
                                    && ((attrPermission & 0x02) != 0x02))) {
                                is_permission_available = true;
                            }
                            if((k+1) < gattHandleToAttributes.size()) {
                                attrHandleNext= gattHandleToAttributes.
                                        get(k+1).handle;
                            }
                            if((k-1) >= 0) {
                                attrTypeStr = attr.type;
                                attrUuidPrev = gattHandleToAttributes.get(k-1).uuid;
                                attrTypePrev = gattHandleToAttributes.get(k-1).type;
                                if(attrTypeStr.equalsIgnoreCase(attrUuidPrev) &&
                                        attrTypePrev.
                                        equalsIgnoreCase("00002803-0000-1000-8000-00805F9B34FB")) {
                                    attrProperties = (byte)gattHandleToAttributes.get(k-1).properties;
                                    charValueAttrType = 1;
                                }
                                else {
                                    attrProperties = (byte)attr.properties;
                                }
                            }
                            if(is_permission_available) {
                                if(attrTypeStr !=null &&
                                        (attrTypeStr.equalsIgnoreCase("00002800-0000-1000-8000-00805F9B34FB") ||
                                        attrTypeStr.equalsIgnoreCase("00002801-0000-1000-8000-00805F9B34FB") ||
                                        attrTypeStr.equalsIgnoreCase("00002802-0000-1000-8000-00805F9B34FB"))) {
                                    uuid = ParcelUuid.fromString(attrTypeStr);
                                    attrValue = attr.value;
                                    startAttrHdl = attr.startHandle;
                                    endAttrHdl = attr.endHandle;
                                    uuidStr = attr.uuid;
                                    status = BluetoothGatt.GATT_SUCCESS;
                                }
                                else if(attrTypeStr !=null &&
                                        !(attrTypeStr.equalsIgnoreCase("00002800-0000-1000-8000-00805F9B34FB") ||
                                        attrTypeStr.equalsIgnoreCase("00002801-0000-1000-8000-00805F9B34FB") ||
                                        attrTypeStr.equalsIgnoreCase("00002802-0000-1000-8000-00805F9B34FB"))){
                                    //need to check whether attribute is readable before reading
                                    if((attrProperties > 0) && ((attrProperties & 0x02) == 0x02)) {
                                        uuid = ParcelUuid.fromString(attrTypeStr);
                                        attrValue = attr.value;
                                        startAttrHdl = attr.startHandle;
                                        endAttrHdl = attr.endHandle;
                                        uuidStr = attr.uuid;
                                        status = BluetoothGatt.GATT_SUCCESS;
                                    }
                                    else {
                                        status = BluetoothGatt.ATT_READ_NOT_PERM;
                                    }
                                }
                            }
                            else {
                                if(security_status_code == 1) {
                                    status = BluetoothGatt.ATT_AUTHORIZATION;
                                }
                                else if(security_status_code == 2) {
                                    status = BluetoothGatt.ATT_AUTHENTICATION;
                                }
                            }
                            break;
                        }
                    }
                    if(hdlFoundStatus == 1) {
                        if(is_permission_available) {
                            if(attrTypeStr !=null &&
                                    (attrTypeStr.equalsIgnoreCase("00002800-0000-1000-8000-00805F9B34FB") ||
                                    attrTypeStr.equalsIgnoreCase("00002801-0000-1000-8000-00805F9B34FB") ||
                                    attrTypeStr.equalsIgnoreCase("00002802-0000-1000-8000-00805F9B34FB"))) {
                                status = BluetoothGatt.GATT_SUCCESS;
                            }
                            else if(attrTypeStr !=null &&
                                    !(attrTypeStr.equalsIgnoreCase("00002800-0000-1000-8000-00805F9B34FB") ||
                                    attrTypeStr.equalsIgnoreCase("00002801-0000-1000-8000-00805F9B34FB") ||
                                    attrTypeStr.equalsIgnoreCase("00002802-0000-1000-8000-00805F9B34FB"))){
                                //need to check whether attribute is readable before reading
                                if((attrProperties > 0) && ((attrProperties & 0x02) == 0x02)) {
                                    status = BluetoothGatt.GATT_SUCCESS;
                                }
                            }
                        }
                    }
                    else if(k == (gattHandleToAttributes.size()-1)) {
                        status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
                    }
                }
            }

            if(is_permission_available) {
                if(attrTypeStr != null && attrTypeStr.length()>0) {
                    //Primary Service definition
                    if(attrTypeStr.equalsIgnoreCase("00002800-0000-1000-8000-00805F9B34FB") ||
                            attrTypeStr.equalsIgnoreCase("00002801-0000-1000-8000-00805F9B34FB")){
                        int cnt =0;
                        payload = new byte[16];
                        //Primary service UUID
                        uuidStr = removeChar(uuidStr, '-');
                        byte[] bytes = new byte[16];
                        bytes = hexStringToByteArray(uuidStr);

                        if(bytes!=null) {
                            for(i=((bytes.length)-1);i >= 0; i--) {
                                payload[cnt++] = bytes[i];
                            }
                        }
                        if(SERVICE_DEBUG) {
                            Log.d(TAG,"The payload data::");
                            if(payload != null) {
                                for(i=0; i < payload.length; i++) {
                                    Log.d(TAG,"\n"+payload[i]);
                                }
                            }
                        }
                    }
                    //Included Service definition
                    else if(attrTypeStr.equalsIgnoreCase("00002802-0000-1000-8000-00805F9B34FB")){
                        int cnt =0;
                        payload = new byte[20];
                        //Included service attr handle
                        byte startHdlLsb = (byte)(startAttrHdl & 0x00FF);
                        byte startHdlMsb = (byte)((startAttrHdl & 0xFF00) >> 8);
                        payload[cnt++] = startHdlLsb;
                        payload[cnt++] = startHdlMsb;

                        //End group handle
                        byte endHdlLsb = (byte)(endAttrHdl & 0x00FF);
                        byte endHdlMsb = (byte)((endAttrHdl & 0xFF00) >> 8);
                        payload[cnt++] = endHdlLsb;
                        payload[cnt++] = endHdlMsb;

                        //service uuid
                        uuidStr = removeChar(uuidStr, '-');
                        byte[] bytes = new byte[16];
                        bytes = hexStringToByteArray(uuidStr);

                        if(bytes!=null) {
                            for(i=((bytes.length)-1);i >= 0; i--) {
                                payload[cnt++] = bytes[i];
                            }
                        }
                        if(SERVICE_DEBUG) {
                            Log.d(TAG,"The payload data::");
                            if(payload != null) {
                                for(i=0; i < payload.length; i++) {
                                    Log.d(TAG,"\n"+payload[i]);
                                }
                            }
                        }
                    }
                    //Characteristic declaration
                    else if(attrTypeStr.equalsIgnoreCase("00002803-0000-1000-8000-00805F9B34FB")){
                        if(((attrProperties > 0) && ((attrProperties & 0x02) == 0x02))) {
                            int cnt = 0;
                            payload = new byte[19];

                            //Characteristic properties
                            payload[cnt++] = attrProperties;

                            //Characteristic value attribute handle
                            byte hdlLsb = (byte)(attrHandleNext & 0x00FF);
                            byte hdlMsb = (byte)((attrHandleNext & 0xFF00) >> 8);
                            payload[cnt++] = hdlLsb;
                            payload[cnt++] = hdlMsb;

                            //Characteristic uuid
                            uuidStr = removeChar(uuidStr, '-');
                            byte[] bytes = new byte[16];
                            bytes = hexStringToByteArray(uuidStr);

                            if(bytes!=null) {
                                for(i=((bytes.length)-1);i >= 0; i--) {
                                    payload[cnt++] = bytes[i];
                                }
                            }
                            if(SERVICE_DEBUG) {
                                Log.d(TAG,"The payload data::");
                                if(payload != null) {
                                    for(i=0; i < payload.length; i++) {
                                        Log.d(TAG,"\n"+payload[i]);
                                    }
                                }
                            }
                        }
                    }
                    //Client characteristic configuration descriptor or Characteristic Value
                    else if(charValueAttrType == 1 ||
                            attrTypeStr.equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB") ||
                            attrTypeStr.equalsIgnoreCase("00002900-0000-1000-8000-00805F9B34FB")
                            || attrTypeStr.equalsIgnoreCase("00002901-0000-1000-8000-00805F9B34FB")
                            || attrTypeStr.equalsIgnoreCase("00002904-0000-1000-8000-00805F9B34FB")) {
                        if(((attrProperties > 0) && ((attrProperties & 0x02) == 0x02))) {
                            if(attrValue!=null && attrValue.length > 0) {
                                List<Byte> byteArrList= new ArrayList<Byte>();

                                //Descriptor Value
                                if(attrValue!=null && attrValue.length > 0) {
                                    for(i=0; i< attrValue.length; i++) {
                                        byteArrList.add(attrValue[i]);
                                    }
                                }
                                if(attrTypeStr.equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")) {
                                    //Client config bits value
                                    payload = new byte[2];
                                    payload[0] = 0x00;
                                    payload[1] = 0x00;
                                    if(SERVICE_DEBUG) {
                                        Log.d(TAG,"The payload data::");
                                        if(payload != null) {
                                            for(i=0; i < payload.length; i++) {
                                                Log.d(TAG,""+payload[i]);
                                            }
                                        }
                                    }
                                }
                                else {
                                    payload = new byte[byteArrList.size()];
                                    //Transfer Arraylist contents to byte array
                                    for(i=(byteArrList.size()-1); i >= 0; i--) {
                                        payload[i] = byteArrList.get(i).byteValue();
                                    }
                                    if(SERVICE_DEBUG) {
                                        Log.d(TAG,"The payload data::");
                                        if(payload != null) {
                                            for(i=0; i < payload.length; i++) {
                                                Log.d(TAG,""+payload[i]);
                                            }
                                        }
                                    }
                                }
                                if(attrTypeStr.equalsIgnoreCase("00002901-0000-1000-8000-00805F9B34FB")) {
                                    String attrValStr = new String(payload);
                                    Log.d(TAG, attrValStr);
                                }
                            }
                        }
                    }
                    //Characteristic Aggregate Format descriptor
                    else if(attrTypeStr.equalsIgnoreCase("00002905-0000-1000-8000-00805F9B34FB")) {
                        int charHandle = -1;
                        int charStartHdl = -1;
                        int charEndHdl = -1;
                        if(((attrProperties > 0) && ((attrProperties & 0x02) == 0x02))) {
                                List<Integer> descHdlList = null;
                                List<Byte> byteArrList= new ArrayList<Byte>();
                                int cnt = 0;
                            if(gattAttribTypeToHandle != null) {
                                for(Map.Entry<String, List<Integer>> entry : gattAttribTypeToHandle.entrySet()) {
                                    if("00002904-0000-1000-8000-00805F9B34FB".
                                            equalsIgnoreCase(entry.getKey().toString())) {
                                        //List of characteristic presentation format handles
                                        descHdlList = entry.getValue();
                                    }
                                }
                            }
                            if(gattHandleToAttributes != null) {
                                Attribute attrDesc = gattHandleToAttributes.get(handle);
                                Attribute attrCharValue = gattHandleToAttributes.get(attrDesc.referenceHandle);
                                Attribute attrChar = gattHandleToAttributes.get(attrCharValue.referenceHandle);
                                charHandle = attrChar.handle;
                                charStartHdl = attrChar.startHandle;
                                charEndHdl = attrChar.endHandle;
                            }
                            if(descHdlList != null) {
                                byteArrList = new ArrayList<Byte>();
                                //convert Int ArryList to byte ArrayList
                                for(int index=0; index<descHdlList.size(); index++) {
                                    int descHdl = descHdlList.get(index);
                                    if((descHdl >= charStartHdl) && (descHdl <= charEndHdl)) {
                                        byte msbDescHdl = (byte)(descHdl & 0xFF00);
                                        byte lsbDescHdl = (byte)(descHdl & 0x00FF);
                                        byteArrList.add(lsbDescHdl);
                                        byteArrList.add(msbDescHdl);
                                    }
                                }

                                payload = new byte[byteArrList.size()];
                                //Transfer Arraylist contents to byte array
                                for(i=0; i < byteArrList.size();i++) {
                                    payload[cnt++] = byteArrList.get(i).byteValue();
                                }
                            }
                            if(SERVICE_DEBUG) {
                                Log.d(TAG,"The payload data::");
                                if(payload != null) {
                                    for(i=0; i < payload.length; i++) {
                                        Log.d(TAG,"\n"+payload[i]);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Log.d(TAG, "Results of readRequest::"+" The parcel uuid value::"+uuid +
                    " status::"+status);

            retVal = gattProfile.readResponse(config, requestHandle, status, uuid, payload);
            if(SERVICE_DEBUG) {
                Log.d(TAG, "onGattReadRequest response: " + retVal);
            }
        }

        /**
         * Processes the Write Request from client and sends the response
         * to the client.
        */
        @Override
        public void onGattWriteRequest(BluetoothGattAppConfiguration config, int handle,
                byte value[], String authentication, int sessionHandle,
                int requestHandle) {
            int i, k, hdlFoundStatus=0, status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
            boolean retVal = false;
            int attrHandleNext = 0;
            String attrUuidPrev, attrTypePrev, attrTypeStr = null;
            int charValueAttrType = 0;
            byte attrProperties =0;
            byte attrCharProperties = 0;
            ParcelUuid uuid = null;
            boolean is_permission_available = false;
            boolean is_attr_writable = false;
            int security_status_code = 0;//1 means not authorized, 2 means not authenticated
            if(handle >= 0) {
                if(gattHandleToAttributes != null) {
                    //To get the attribute values for the particular handle
                    for(k=0; k<gattHandleToAttributes.size(); k++) {
                        if(handle == gattHandleToAttributes.get(k).handle) {
                            hdlFoundStatus = 1;
                            Attribute attr = gattHandleToAttributes.get(k);
                            attrTypeStr = attr.type;
                            //need to check whether attribute is writable before writing
                            if((k-1) >= 0) {
                                attrUuidPrev = gattHandleToAttributes.get(k-1).uuid;
                                attrTypePrev = gattHandleToAttributes.get(k-1).type;
                                if(attrTypeStr.equalsIgnoreCase(attrUuidPrev) &&
                                        attrTypePrev.
                                        equalsIgnoreCase("00002803-0000-1000-8000-00805F9B34FB")) {
                                    charValueAttrType = 1;
                                    attrProperties = (byte)gattHandleToAttributes.get(k-1).properties;
                                }
                                else if(attrTypeStr.
                                        equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")) {
                                    Attribute charValueAttr = gattHandleToAttributes.get(attr.referenceHandle);
                                    Attribute charAttr = gattHandleToAttributes.get(charValueAttr.referenceHandle);
                                    attrCharProperties = (byte) charAttr.properties;
                                    attrProperties = (byte)attr.properties;
                                }
                                else {
                                    attrProperties = (byte)attr.properties;
                                }
                            }
                            if((attrProperties > 0) && ((attrProperties & 0x08) == 0x08)) {
                                //If the Attribute type is Characteristic value
                                if(charValueAttrType == 1) {
                                    String attrPermission = attr.permission;
                                    byte attrPermBits = attr.permBits;
                                    is_permission_available = false;
                                    //if the char value is authorized/authenticated
                                    if((attrPermBits > 0) && ((attrPermBits & 0x08) == 0x08)) {
                                        if(authentication.equalsIgnoreCase("Authenticated") ||
                                                authentication.equalsIgnoreCase("Authorized")) {
                                            is_permission_available = true;
                                        }
                                        else {
                                            //not authorized
                                            security_status_code = 1;
                                        }
                                    }
                                    else if((attrPermBits > 0) && ((attrPermBits & 0x10) == 0x10)) {
                                        if(authentication.equalsIgnoreCase("Authenticated")) {
                                            is_permission_available = true;
                                        }
                                        else {
                                            //not authenticated
                                            security_status_code = 2;
                                        }
                                    }
                                    else if(attrPermBits == 0 || (((attrPermBits & 0x08) != 0x08) &&
                                            ((attrPermBits & 0x10) != 0x10))) {
                                        is_permission_available = true;
                                    }
                                    if(is_permission_available && (value != null)) {
                                        attr.value = value;
                                        if(attr.sessionHandle != null &&
                                                !attr.sessionHandle.contains(sessionHandle)) {
                                            attr.sessionHandle.add(sessionHandle);
                                        }
                                        else if(attr.sessionHandle == null) {
                                            attr.sessionHandle = new ArrayList<Integer>();
                                            attr.sessionHandle.add(sessionHandle);
                                        }
                                        uuid = ParcelUuid.fromString(attrTypeStr);
                                        status = BluetoothGatt.GATT_SUCCESS;
                                        //reading and checking whether the char value file exists
                                        //already
                                        byte[] buffer = new byte[2000];
                                        List<Byte> byteArrList= new ArrayList<Byte>();
                                        List<Byte> hdlsArrList= new ArrayList<Byte>();
                                        HashMap<Integer, Integer> hdlIndexMap =
                                                new HashMap<Integer, Integer>();
                                        int isHdlInFile = 0;
                                        String FILENAME = "characteristic_values";
                                        Context context = getApplicationContext();
                                        try {
                                            Log.d(TAG,"File path ::"+
                                                    context.getFilesDir().getAbsolutePath());
                                            FileInputStream fis =
                                                    new FileInputStream(context.getFilesDir().getAbsolutePath()
                                                    + "/" + FILENAME);
                                            int bytesRead = fis.read(buffer);
                                            if(bytesRead > 0) {
                                                for(i=0; i< bytesRead;) {
                                                    hdlsArrList.add(buffer[i]);//handle msb
                                                    hdlsArrList.add(buffer[i+1]);//handle lsb
                                                    int hdlValue = (buffer[i] << 8) + (buffer[i+1]);
                                                    hdlIndexMap.put(hdlValue, (i+3));
                                                    i= i+4+buffer[i+2];
                                                }
                                                if(SERVICE_DEBUG) {
                                                    if(hdlIndexMap != null) {
                                                        for(Map.Entry<Integer, Integer> entry : hdlIndexMap.entrySet()) {
                                                            Log.d(TAG, "Key::"+entry.getKey());
                                                            Log.d(TAG, "Value::"+entry.getValue());
                                                        }
                                                    }
                                                }
                                                byte handleLSB = (byte)(handle & 0x00FF);
                                                byte handleMSB = (byte)((handle & 0xFF00) >> 8);

                                                //check if the char value handle is already
                                                //present in the File.
                                                //If present, get the handle and the index
                                                //and update the char value at the correct index
                                                if(hdlsArrList != null && hdlsArrList.size() >0) {
                                                    for(i=0;i<hdlsArrList.size();i++) {
                                                        if((hdlsArrList.get(i) == handleMSB) &&
                                                                (hdlsArrList.get(i+1) == handleLSB)){
                                                            isHdlInFile = 1;
                                                            byte hdlValTmpMSB = (byte)((handle & 0xFF00) >> 8);
                                                            byte hdlValTmpLSB = (byte)(handle & 0x00FF);
                                                            int hdlValTmp = (hdlValTmpMSB << 8) + (hdlValTmpLSB);
                                                            int index = hdlIndexMap.get(hdlValTmp);

                                                            byte[] bufferTemp = new byte[2000];
                                                            int tmpIndex=0;
                                                            if(buffer != null) {
                                                                int z = 0;
                                                                //Get the index for '\n' after the handle
                                                                for(z=index; z < bytesRead; z++) {
                                                                    if(buffer[z] == ((byte)'\n')) {
                                                                        break;
                                                                    }
                                                                }
                                                                //Store the remaining byte array values in
                                                                //a temp byte array

                                                                for(tmpIndex=0; tmpIndex < (bytesRead-(z+1)); tmpIndex++) {
                                                                    bufferTemp[tmpIndex] = buffer[tmpIndex+z+1];
                                                                }
                                                            }
                                                            //Write the char value and update the length
                                                            buffer[index-1] = (byte)value.length;
                                                            for(int j=0; j < value.length; j++) {
                                                                buffer[index++] = value[j];
                                                            }
                                                            //append '\n' after every char value
                                                            buffer[index++] = (byte)'\n';
                                                            //append the remaining byte array elements again
                                                            for(int y=0; y < tmpIndex; y++) {
                                                                buffer[index++] = bufferTemp[y];
                                                            }
                                                            //For testing
                                                            if(SERVICE_DEBUG) {
                                                                Log.d(TAG, "buffer printed");
                                                                for(int r=0; r< index; r++) {
                                                                    Log.d(TAG, ""+buffer[r]);
                                                                }
                                                            }
                                                            for(i=0; i< index; i++) {
                                                                byteArrList.add(buffer[i]);
                                                            }
                                                        }
                                                    }
                                                }
                                                if(SERVICE_DEBUG) {
                                                    Log.d(TAG, "buffer printed outside");
                                                    for(int r=0; r< bytesRead; r++) {
                                                        Log.d(TAG, ""+buffer[r]);
                                                    }
                                                }
                                                //If char value handle is not already present in the
                                                //file, just store the values read from file into
                                                //the byte array list
                                                if(isHdlInFile == 0) {
                                                    for(i=0; i< bytesRead; i++) {
                                                        byteArrList.add(buffer[i]);
                                                    }
                                                }
                                            }
                                            fis.close();
                                        }
                                        catch (FileNotFoundException e) {
                                            e.printStackTrace();
                                        }
                                        catch (IOException e) {
                                            e.printStackTrace();
                                        }
                                        //write to a file
                                        if(SERVICE_DEBUG) {
                                            Log.d(TAG, "Writing to Char values file::");
                                        }

                                        if(isHdlInFile == 0) {
                                            //Creating the byte array for a char value
                                            //writing the new char value data not already in file
                                            //into arraylist
                                            //big endian
                                            byte handleLSB = (byte)(handle & 0x00FF);
                                            byte handleMSB = (byte)((handle & 0xFF00) >> 8);

                                            byteArrList.add(handleMSB);
                                            byteArrList.add(handleLSB);
                                            byteArrList.add((byte)value.length);

                                            if(value!=null && value.length > 0) {
                                                for(i=0; i< value.length; i++) {
                                                    byteArrList.add(value[i]);
                                                }
                                            }
                                            byteArrList.add((byte)'\n');
                                        }

                                        byte[] charValueBytes = new byte[byteArrList.size()];
                                        //Transfer Arraylist contents to byte array
                                        for(i=0; i < byteArrList.size(); i++) {
                                            charValueBytes[i] = byteArrList.get(i).byteValue();
                                        }
                                        if(SERVICE_DEBUG) {
                                            Log.d(TAG, "The data written to file onWriteRequest");
                                            for(i=0; i< charValueBytes.length; i++) {
                                                Log.d(TAG,""+charValueBytes[i]);
                                            }
                                        }
                                        try {
                                            FileOutputStream fos =
                                                    openFileOutput(FILENAME, Context.MODE_PRIVATE);
                                            fos.write(charValueBytes);
                                            fos.close();
                                        }
                                        catch (FileNotFoundException e) {
                                            e.printStackTrace();
                                        }
                                        catch (IOException e) {
                                             e.printStackTrace();
                                        }
                                    }
                                    else if(value == null) {
                                        status = 0x80; //Application error
                                    }
                                    if(security_status_code == 1) {
                                        status = BluetoothGatt.ATT_AUTHORIZATION;
                                    }
                                    else if(security_status_code == 2) {
                                        status = BluetoothGatt.ATT_AUTHENTICATION;
                                    }
                                }
                                //If the attribute type is client characteristic descriptor
                                else if(attrTypeStr.
                                        equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")
                                        || attrTypeStr.
                                        equalsIgnoreCase("00002901-0000-1000-8000-00805F9B34FB")) {
                                    String attrPermission = attr.permission;
                                    byte attrPermBits = attr.permBits;
                                    is_permission_available = false;
                                    //if the client char descriptor is authorized/authenticated
                                    if((attrPermBits > 0) && ((attrPermBits & 0x08) == 0x08)) {
                                        if(authentication.equalsIgnoreCase("Authenticated") ||
                                                authentication.equalsIgnoreCase("Authorized")) {
                                            is_permission_available = true;
                                        }
                                        else {
                                            //not authorized
                                            security_status_code = 1;
                                        }
                                    }
                                    //if the client char config descriptor is authenticated
                                    else if((attrPermBits > 0) && ((attrPermBits & 0x10) == 0x10)) {
                                        if(authentication.equalsIgnoreCase("Authenticated")) {
                                            is_permission_available = true;
                                        }
                                        else {
                                            //not authenticated
                                            security_status_code = 2;
                                        }
                                    }
                                    else if(attrPermBits == 0 || (((attrPermBits & 0x08) != 0x08) &&
                                            ((attrPermBits & 0x10) != 0x10))) {
                                        is_permission_available = true;
                                    }
                                    if(is_permission_available) {
                                        if(value != null) {
                                            if(attrTypeStr.
                                                    equalsIgnoreCase("00002901-0000-1000-8000-00805F9B34FB")) {
                                                is_attr_writable = checkWritePermission(attr.handle);
                                                if(is_attr_writable) {
                                                    attr.value = value;
                                                    if((attr.sessionHandle != null &&
                                                            !attr.sessionHandle.contains(sessionHandle))) {
                                                        attr.sessionHandle.add(sessionHandle);
                                                    }
                                                    else if(attr.sessionHandle == null) {
                                                        attr.sessionHandle = new ArrayList<Integer>();
                                                        attr.sessionHandle.add(sessionHandle);
                                                    }
                                                    uuid = ParcelUuid.fromString(attrTypeStr);
                                                    status = BluetoothGatt.GATT_SUCCESS;
                                                }
                                            }
                                            else if(attrTypeStr.
                                                    equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")) {
                                                //Check whether the Char properties is notifiable
                                                if(value != null && value[0] == 0x01) {
                                                    if((attrCharProperties > 0) && ((attrCharProperties & 0x10) == 0x10)) {
                                                        if(attr.attrValue != null) {
                                                            attr.attrValue.put(sessionHandle, value[0]);
                                                        }
                                                        else if(attr.attrValue == null) {
                                                            attr.attrValue = new HashMap<Integer, Byte>();
                                                            attr.attrValue.put(sessionHandle, value[0]);
                                                        }
                                                        uuid = ParcelUuid.fromString(attrTypeStr);
                                                        status = BluetoothGatt.GATT_SUCCESS;
                                                        if((attr.sessionHandle != null &&
                                                            !attr.sessionHandle.contains(sessionHandle))) {
                                                            attr.sessionHandle.add(sessionHandle);
                                                        }
                                                        else if(attr.sessionHandle == null) {
                                                            attr.sessionHandle = new ArrayList<Integer>();
                                                            attr.sessionHandle.add(sessionHandle);
                                                        }
                                                    }
                                                    else {
                                                        status = 0x80;
                                                    }
                                                }
                                                //Check whether the Char properties is indicatable
                                                else if(value != null && value[0] == 0x02) {
                                                    if((attrCharProperties > 0) && ((attrCharProperties & 0x20) == 0x20)) {
                                                        if(attr.attrValue != null) {
                                                            attr.attrValue.put(sessionHandle, value[0]);
                                                        }
                                                        else if(attr.attrValue == null) {
                                                            attr.attrValue = new HashMap<Integer, Byte>();
                                                            attr.attrValue.put(sessionHandle, value[0]);
                                                        }
                                                        uuid = ParcelUuid.fromString(attrTypeStr);
                                                        status = BluetoothGatt.GATT_SUCCESS;
                                                        if((attr.sessionHandle != null &&
                                                                !attr.sessionHandle.contains(sessionHandle))) {
                                                            attr.sessionHandle.add(sessionHandle);
                                                        }
                                                        else if(attr.sessionHandle == null) {
                                                            attr.sessionHandle = new ArrayList<Integer>();
                                                            attr.sessionHandle.add(sessionHandle);
                                                        }
                                                    }
                                                    else {
                                                        status = 0x80;
                                                    }
                                                }
                                                else if(value != null && value[0] == 0x00) {
                                                    if(attr.attrValue != null) {
                                                        attr.attrValue.put(sessionHandle, value[0]);
                                                    }
                                                    else if(attr.attrValue == null) {
                                                        attr.attrValue = new HashMap<Integer, Byte>();
                                                        attr.attrValue.put(sessionHandle, value[0]);
                                                    }
                                                    uuid = ParcelUuid.fromString(attrTypeStr);
                                                    status = BluetoothGatt.GATT_SUCCESS;
                                                    if((attr.sessionHandle != null &&
                                                            !attr.sessionHandle.contains(sessionHandle))) {
                                                        attr.sessionHandle.add(sessionHandle);
                                                    }
                                                    else if(attr.sessionHandle == null) {
                                                        attr.sessionHandle = new ArrayList<Integer>();
                                                        attr.sessionHandle.add(sessionHandle);
                                                    }
                                                }
                                                else {
                                                    status = 0x80;
                                                }
                                            }
                                            else {
                                                attr.value = value;
                                                if(attr.sessionHandle != null &&
                                                        !attr.sessionHandle.contains(sessionHandle)) {
                                                    attr.sessionHandle.add(sessionHandle);
                                                }
                                                else if(attr.sessionHandle == null) {
                                                    attr.sessionHandle = new ArrayList<Integer>();
                                                    attr.sessionHandle.add(sessionHandle);
                                                }
                                                uuid = ParcelUuid.fromString(attrTypeStr);
                                                status = BluetoothGatt.GATT_SUCCESS;
                                            }
                                        }
                                        else if(value == null) {
                                            status = 0x80; //Application error
                                        }
                                    }
                                    else {
                                        if(security_status_code == 1) {
                                            status = BluetoothGatt.ATT_AUTHORIZATION;
                                        }
                                        else if(security_status_code == 2) {
                                            status = BluetoothGatt.ATT_AUTHENTICATION;
                                        }
                                    }
                                }
                            }
                            else {
                                //need to change the error to NOT_AUTHORIZED
                                status = BluetoothGatt.ATT_WRITE_NOT_PERM;
                            }
                            break;
                        }
                    }
                    if(hdlFoundStatus == 1) {
                        if(is_permission_available && ((attrProperties > 0) && ((attrProperties & 0x08) == 0x08))) {
                            if(attrTypeStr.
                                    equalsIgnoreCase("00002901-0000-1000-8000-00805F9B34FB")) {
                                if(is_attr_writable && value != null) {
                                    status = BluetoothGatt.GATT_SUCCESS;
                                }
                            }
                            else if(attrTypeStr.
                                    equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")) {
                                if((value != null) && (value[0] >= 0x00) && (value[0] <= 0x02)
                                        && (status != BluetoothGatt.ATT_ATTR_NOT_FOUND) && !(status > 0x00)) {
                                    status = BluetoothGatt.GATT_SUCCESS;
                                }
                            }
                            else if(value != null){
                                status = BluetoothGatt.GATT_SUCCESS;
                            }
                        }
                    }
                    else if(k == (gattHandleToAttributes.size()-1)) {
                        status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
                    }
                }
            }

            Log.d(TAG,"Results of onGattWriteRequest::"+" status"+status+
                    " uuid"+ uuid);

            retVal = gattProfile.writeResponse(config, requestHandle, status, uuid);
            if(SERVICE_DEBUG) {
                Log.d(TAG, "onGattWriteRequest response: " + retVal);
            }
            boolean isClientConfigSet = false;
            if(is_permission_available && hdlFoundStatus == 1) {
                //send notification/indication for the particular
                //client char config handle
                if(attrTypeStr.
                        equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")) {
                    if((value != null) && (value.length > 0)) {
                        if(value[0] > 0x00) {
                            sendNotificationIndicationHandle(handle, sessionHandle);
                            isClientConfigSet = isAnyClientConfigSet();
                            if(isClientConfigSet && !isAlarmStarted) {
                                alarm.SetAlarm(mContext);
                                isAlarmStarted = true;
                            }
                            else if(!isClientConfigSet){
                                alarm.CancelAlarm(mContext);
                                isAlarmStarted = false;
                            }
                        }
                        else {
                            isClientConfigSet = isAnyClientConfigSet();
                            if(!isClientConfigSet) {
                                alarm.CancelAlarm(mContext);
                                isAlarmStarted = false;
                            }
                            else if(isClientConfigSet && !isAlarmStarted) {
                                alarm.SetAlarm(mContext);
                                isAlarmStarted = true;
                            }
                        }
                    }
                }
            }
        }

        /**
         * Processes the Write Command from client and sends the response
         * to the client.
        */
        @Override
        public void onGattWriteCommand(BluetoothGattAppConfiguration config, int handle,
                byte value[], String authentication) {
            int i, k, hdlFoundStatus=0, status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
            boolean retVal = false;
            int attrHandleNext = 0;
            String attrUuidPrev, attrTypePrev, attrTypeStr = null;
            int charValueAttrType = 0;
            byte attrProperties =0;
            byte attrCharProperties = 0;
            ParcelUuid uuid = null;
            boolean is_permission_available = false;
            boolean is_attr_writable = false;
            int security_status_code = 0;//1 means not authorized, 2 means not authenticated
            if(handle >= 0) {
                if(gattHandleToAttributes != null) {
                    //To get the attribute values for the particular handle
                    for(k=0; k<gattHandleToAttributes.size(); k++) {
                        if(handle == gattHandleToAttributes.get(k).handle) {
                            hdlFoundStatus = 1;
                            Attribute attr = gattHandleToAttributes.get(k);
                            attrTypeStr = attr.type;
                            if((k-1) >= 0) {
                                attrUuidPrev = gattHandleToAttributes.get(k-1).uuid;
                                attrTypePrev = gattHandleToAttributes.get(k-1).type;
                                if(attrTypeStr.equalsIgnoreCase(attrUuidPrev) &&
                                        attrTypePrev.
                                        equalsIgnoreCase("00002803-0000-1000-8000-00805F9B34FB")) {
                                    charValueAttrType = 1;
                                    attrProperties = (byte)gattHandleToAttributes.get(k-1).properties;
                                }
                                else if(attrTypeStr.
                                        equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")) {
                                    Attribute charValueAttr = gattHandleToAttributes.get(attr.referenceHandle);
                                    Attribute charAttr = gattHandleToAttributes.get(charValueAttr.referenceHandle);
                                    attrCharProperties = (byte) charAttr.properties;
                                    attrProperties = (byte)attr.properties;
                                }
                                else {
                                    attrProperties = (byte)attr.properties;
                                }
                            }
                            //need to check whether attribute is writable before writing
                            if((attrProperties > 0) && ((attrProperties & 0x04) == 0x04)) {
                                //If the Attribute type is Characteristic value
                                if(charValueAttrType == 1) {
                                    String attrPermission = attr.permission;
                                    byte attrPermBits = attr.permBits;
                                    is_permission_available = false;
                                    //if the char value is authorized/authenticated
                                    if((attrPermBits > 0) && ((attrPermBits & 0x08) == 0x08)) {
                                        if(authentication.equalsIgnoreCase("Authenticated") ||
                                                authentication.equalsIgnoreCase("Authorized")) {
                                            is_permission_available = true;
                                        }
                                        else {
                                            //not authorized
                                            security_status_code = 1;
                                        }
                                    }
                                    else if((attrPermBits > 0) && ((attrPermBits & 0x10) == 0x10)) {
                                        if(authentication.equalsIgnoreCase("Authenticated")) {
                                            is_permission_available = true;
                                        }
                                        else {
                                            //not authenticated
                                            security_status_code = 2;
                                        }
                                    }
                                    else if(attrPermBits == 0 || (((attrPermBits & 0x08) != 0x08) &&
                                            ((attrPermBits & 0x10) != 0x10))) {
                                        is_permission_available = true;
                                    }
                                    if(is_permission_available) {
                                        if(value != null) {
                                            attr.value = value;
                                            uuid = ParcelUuid.fromString(attrTypeStr);
                                            status = BluetoothGatt.GATT_SUCCESS;
                                            //reading and checking whether the char value file exists
                                            //already
                                            byte[] buffer = new byte[2000];
                                            List<Byte> byteArrList= new ArrayList<Byte>();
                                            List<Byte> hdlsArrList= new ArrayList<Byte>();
                                            HashMap<Integer, Integer> hdlIndexMap =
                                                            new HashMap<Integer, Integer>();
                                            int isHdlInFile = 0;
                                            String FILENAME = "characteristic_values";
                                            Context context = getApplicationContext();
                                            try {
                                                Log.d(TAG,"File path ::"+
                                                        context.getFilesDir().getAbsolutePath());
                                                FileInputStream fis =
                                                        new FileInputStream(context.getFilesDir().getAbsolutePath()
                                                        + "/" + FILENAME);
                                                int bytesRead = fis.read(buffer);
                                                if(bytesRead > 0) {
                                                    for(i=0; i< bytesRead;) {
                                                        hdlsArrList.add(buffer[i]);//handle msb
                                                        hdlsArrList.add(buffer[i+1]);//handle lsb
                                                        int hdlValue = (buffer[i] << 8) + (buffer[i+1]);
                                                        hdlIndexMap.put(hdlValue, (i+3));
                                                        //4 below represents handle-2 bytes,
                                                        //length-1 byte,"\n"-1 byte
                                                        i= i+4+buffer[i+2];
                                                    }
                                                    byte handleLSB = (byte)(handle & 0x00FF);
                                                    byte handleMSB = (byte)((handle & 0xFF00) >> 8);

                                                    //check if the char value handle is already
                                                    //present in the File.
                                                    //If present, get the handle and the index
                                                    //and update the char value at the correct index
                                                    if(hdlsArrList != null && hdlsArrList.size() >0) {
                                                        for(i=0;i<hdlsArrList.size();i++) {
                                                            if((hdlsArrList.get(i) == handleMSB) &&
                                                                    (hdlsArrList.get(i+1) == handleLSB)){
                                                              isHdlInFile = 1;

                                                              byte hdlValTmpMSB = (byte)((handle & 0xFF00) >> 8);
                                                              byte hdlValTmpLSB = (byte)(handle & 0x00FF);
                                                              int hdlValTmp = (hdlValTmpMSB << 8) + (hdlValTmpLSB);
                                                              int index = hdlIndexMap.get(hdlValTmp);

                                                              byte[] bufferTemp = new byte[2000];
                                                              int tmpIndex=0;
                                                              if(buffer != null) {
                                                                  int z = 0;
                                                                  //Get the index for '\n' after the handle
                                                                  for(z=index; z < bytesRead; z++) {
                                                                      if(buffer[z] == ((byte)'\n')) {
                                                                          break;
                                                                      }
                                                                  }
                                                                  //Store the remaining byte array values in
                                                                  //a temp byte array
                                                                  for(tmpIndex=0; tmpIndex < (bytesRead-(z+1)); tmpIndex++) {
                                                                      bufferTemp[tmpIndex] = buffer[tmpIndex+z+1];
                                                                  }
                                                              }
                                                              //Write the char value
                                                              buffer[index-1] = (byte)value.length;
                                                              for(int j=0; j < value.length; j++) {
                                                                  buffer[index++] = value[j];
                                                              }
                                                              //append '\n' after every char value
                                                              buffer[index++] = (byte)'\n';
                                                              //append the remaining byte array elements again
                                                              for(int y=0; y < tmpIndex; y++) {
                                                                  buffer[index++] = bufferTemp[y];
                                                              }
                                                              for(i=0; i< index; i++) {
                                                                  byteArrList.add(buffer[i]);
                                                              }
                                                            }
                                                        }
                                                    }
                                                    //If char value handle is not already present in the
                                                    //file, just store the values read from file into
                                                    //the byte array list
                                                    if(isHdlInFile == 0) {
                                                        for(i=0; i< bytesRead; i++) {
                                                            byteArrList.add(buffer[i]);
                                                        }
                                                    }
                                                }
                                                fis.close();
                                            }
                                            catch (FileNotFoundException e) {
                                                e.printStackTrace();
                                            }
                                            catch (IOException e) {
                                                e.printStackTrace();
                                            }
                                            //write to a file
                                            if(SERVICE_DEBUG) {
                                                Log.d(TAG, "Writing to Char values file::");
                                            }

                                            if(isHdlInFile == 0) {
                                                //Creating the byte array for a char value
                                                //writing the new char value data not already in file
                                                //into arraylist
                                                //big endian
                                                byte handleLSB = (byte)(handle & 0x00FF);
                                                byte handleMSB = (byte)((handle & 0xFF00) >> 8);

                                                byteArrList.add(handleMSB);
                                                byteArrList.add(handleLSB);
                                                byteArrList.add((byte)value.length);

                                                if(value!=null && value.length > 0) {
                                                    for(i=0; i< value.length; i++) {
                                                        byteArrList.add(value[i]);
                                                    }
                                                }
                                                byteArrList.add((byte)'\n');
                                            }

                                            byte[] charValueBytes = new byte[byteArrList.size()];
                                            //Transfer Arraylist contents to byte array
                                            for(i=0; i < byteArrList.size(); i++) {
                                                charValueBytes[i] = byteArrList.get(i).byteValue();
                                            }
                                            if(SERVICE_DEBUG) {
                                                Log.d(TAG, "The data written to file onWriteCommand");
                                                for(i=0; i< charValueBytes.length; i++) {
                                                    Log.d(TAG,""+charValueBytes[i]);
                                                }
                                            }
                                            try {
                                                FileOutputStream fos =
                                                        openFileOutput(FILENAME, Context.MODE_PRIVATE);
                                                fos.write(charValueBytes);
                                                fos.close();
                                            }
                                            catch (FileNotFoundException e) {
                                                e.printStackTrace();
                                            }
                                            catch (IOException e) {
                                                e.printStackTrace();
                                            }
                                        }
                                        else if(value == null){
                                            status = 0x80;//Application error
                                        }
                                    }
                                    else {
                                        if(security_status_code == 1) {
                                            status = BluetoothGatt.ATT_AUTHORIZATION;
                                        }
                                        else if(security_status_code == 2) {
                                            status = BluetoothGatt.ATT_AUTHENTICATION;
                                        }
                                    }
                                }
                                //If the attribute type is client characteristic descriptor
                                else if(attrTypeStr.
                                        equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")
                                        || attrTypeStr.
                                        equalsIgnoreCase("00002901-0000-1000-8000-00805F9B34FB")) {
                                    String attrPermission = attr.permission;
                                    byte attrPermBits = attr.permBits;
                                    is_permission_available = false;
                                    //if the client char descriptor is authorized/authenticated
                                    if((attrPermBits > 0) && ((attrPermBits & 0x08) == 0x08)) {
                                        if(authentication.equalsIgnoreCase("Authenticated") ||
                                                authentication.equalsIgnoreCase("Authorized")) {
                                            is_permission_available = true;
                                        }
                                        else {
                                            //not authorized
                                            security_status_code = 1;
                                        }
                                    }
                                    //if the client char config descriptor is authenticated
                                    else if((attrPermBits > 0) && ((attrPermBits & 0x10) == 0x10)) {
                                        if(authentication.equalsIgnoreCase("Authenticated")) {
                                            is_permission_available = true;
                                        }
                                        else {
                                            //not authenticated
                                            security_status_code = 2;
                                        }
                                    }
                                    else if(attrPermBits == 0 || (((attrPermBits & 0x08) != 0x08) &&
                                            ((attrPermBits & 0x10) != 0x10))) {
                                        is_permission_available = true;
                                    }
                                    if(is_permission_available) {
                                        if(value != null) {
                                            if(attrTypeStr.
                                                    equalsIgnoreCase("00002901-0000-1000-8000-00805F9B34FB")) {
                                                is_attr_writable = checkWritePermission(attr.handle);
                                                if(is_attr_writable) {
                                                    attr.value = value;
                                                    uuid = ParcelUuid.fromString(attrTypeStr);
                                                    status = BluetoothGatt.GATT_SUCCESS;
                                                }
                                            }
                                            else if(attrTypeStr.
                                                equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")) {
                                                    //Check whether the Char properties is notifiable
                                                if(value != null && value[0] == 0x01) {
                                                    if((attrCharProperties > 0) && ((attrCharProperties & 0x10) == 0x10)) {
                                                        attr.value = value;
                                                        uuid = ParcelUuid.fromString(attrTypeStr);
                                                        status = BluetoothGatt.GATT_SUCCESS;
                                                    }
                                                    else {
                                                        status = 0x80;
                                                    }
                                                }
                                                //Check whether the Char properties is indicatable
                                                else if(value != null && value[0] == 0x02) {
                                                    if((attrCharProperties > 0) && ((attrCharProperties & 0x20) == 0x20)) {
                                                        attr.value = value;
                                                        uuid = ParcelUuid.fromString(attrTypeStr);
                                                        status = BluetoothGatt.GATT_SUCCESS;
                                                    }
                                                    else {
                                                        status = 0x80;
                                                    }
                                                }
                                                else if(value != null && value[0] == 0x00) {
                                                    attr.value = value;
                                                    uuid = ParcelUuid.fromString(attrTypeStr);
                                                    status = BluetoothGatt.GATT_SUCCESS;
                                                }
                                                else {
                                                    status = 0x80;
                                                }
                                            }
                                            else {
                                                attr.value = value;
                                                uuid = ParcelUuid.fromString(attrTypeStr);
                                                status = BluetoothGatt.GATT_SUCCESS;
                                            }
                                        }
                                        else if(value == null) {
                                            status = 0x80;//Application error
                                        }
                                    }
                                    else {
                                        if(security_status_code == 1) {
                                            status = BluetoothGatt.ATT_AUTHORIZATION;
                                        }
                                        else if(security_status_code == 2) {
                                            status = BluetoothGatt.ATT_AUTHENTICATION;
                                        }
                                    }
                                }
                            }
                            else {
                                status = BluetoothGatt.ATT_WRITE_NOT_PERM;
                            }
                            break;
                        }
                    }
                    if(hdlFoundStatus == 1) {
                        if(is_permission_available && ((attrProperties > 0) && ((attrProperties & 0x04) == 0x04))) {
                            if(attrTypeStr.
                                    equalsIgnoreCase("00002901-0000-1000-8000-00805F9B34FB")) {
                                if(is_attr_writable) {
                                    status = BluetoothGatt.GATT_SUCCESS;
                                }
                            }
                            else if(attrTypeStr.
                                    equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")) {
                                if((value != null) && (value[0] >= 0x00) && (value[0] <= 0x02)
                                        && (status != BluetoothGatt.ATT_ATTR_NOT_FOUND) && !(status > 0x00)) {
                                    status = BluetoothGatt.GATT_SUCCESS;
                                }
                            }
                            else if(value != null){
                                status = BluetoothGatt.GATT_SUCCESS;
                            }
                        }
                    }
                    else if(k == (gattHandleToAttributes.size()-1)) {
                        status = BluetoothGatt.ATT_ATTR_NOT_FOUND;
                    }
                    if(is_permission_available && hdlFoundStatus == 1) {
                        //send notification/indication for the particular
                        //client char config handle
                        if(attrTypeStr.
                                equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")) {
                            if((value != null) && (value.length > 0)) {
                                if(value[0] > 0x00) {
                                    //sendNotificationIndicationHandle(handle);
                                }
                            }
                        }
                    }
                }
            }
        }

        @Override
        public void onGattSetClientConfigDescriptor(BluetoothGattAppConfiguration config,
                int handle, byte[] value, int sessionHandle) {
            byte attrProperties = 0;
            boolean isClientConfigSet = false;
            if(gattHandleToAttributes != null) {
                Attribute attr = gattHandleToAttributes.get(handle);
                Attribute charValueAttr = gattHandleToAttributes.get(attr.referenceHandle);
                Attribute charAttr = gattHandleToAttributes.get(charValueAttr.referenceHandle);
                attrProperties = (byte)charAttr.properties;
                if(value != null && (value.length > 0)) {
                    if(attr.attrValue != null && attr.attrValue.containsKey(sessionHandle)) {
                        byte descValue = attr.attrValue.get(sessionHandle);
                        //If value is 0, remove the session handle
                        if(value[0] == 0) {
                            attr.attrValue.put(sessionHandle, value[0]);
                            int index = attr.sessionHandle.indexOf(sessionHandle);
                            attr.sessionHandle.remove(index);
                        }
                    }
                    //If value is 1 or 2, add the session handle to notify/indicate
                    else if(attr.attrValue != null && value[0] == 0x01) {
                        if((attrProperties > 0) && ((attrProperties & 0x10) == 0x10)) {
                            Log.d(TAG,"notifications should be set");
                                attr.attrValue.put(sessionHandle, value[0]);
                            if(attr.sessionHandle != null &&
                                    !attr.sessionHandle.contains(sessionHandle)) {
                                attr.sessionHandle.add(sessionHandle);
                            }
                            else if(attr.sessionHandle == null) {
                                attr.sessionHandle = new ArrayList<Integer>();
                                attr.sessionHandle.add(sessionHandle);
                            }
                        }
                    }
                    else if(attr.attrValue != null && value[0] == 0x02) {
                        if((attrProperties > 0) && ((attrProperties & 0x20) == 0x20)) {
                            Log.d(TAG,"indications should be set");
                                attr.attrValue.put(sessionHandle, value[0]);
                            if(attr.sessionHandle != null &&
                                    !attr.sessionHandle.contains(sessionHandle)) {
                                attr.sessionHandle.add(sessionHandle);
                            }
                            else if(attr.sessionHandle == null) {
                                attr.sessionHandle = new ArrayList<Integer>();
                                attr.sessionHandle.add(sessionHandle);
                            }
                        }
                    }
                }
            }
            //send notification/indication for the particular client char config handle
            if((value != null) && (value.length > 0)) {
                if(value[0] > 0x00 && ((attrProperties > 0) && (((attrProperties & 0x20) == 0x20) ||
                        ((attrProperties & 0x10) == 0x10)))) {
                    sendNotificationIndicationHandle(handle, sessionHandle);
                    isClientConfigSet = isAnyClientConfigSet();
                    if(isClientConfigSet && !isAlarmStarted) {
                        alarm.SetAlarm(mContext);
                        isAlarmStarted = true;
                    }
                    else if(!isClientConfigSet){
                        alarm.CancelAlarm(mContext);
                        isAlarmStarted = false;
                    }
                }
                else if(value[0] == 0x00){
                    isClientConfigSet = isAnyClientConfigSet();
                    if(!isClientConfigSet) {
                        alarm.CancelAlarm(mContext);
                        isAlarmStarted = false;
                    }
                    else if(isClientConfigSet && !isAlarmStarted) {
                        alarm.SetAlarm(mContext);
                        isAlarmStarted = true;
                    }
                }
            }
        }
    };

    // Sends an update message to registered UI client.
    private void sendMessage(int what, int value) {
        //mClient
        if (mMessenger == null) {
            Log.d(TAG, "No clients registered.");
            return;
        }

        try {
            //mClient.
            mMessenger.send(Message.obtain(null, what, value, 0));
        } catch (RemoteException e) {
            // Unable to reach client.
            e.printStackTrace();
        }
    }
    /**
     * Populates the hash map with Attribute type and their correspondng attribute handles.
    */
    private void populateGattAttribTypeMap() {
        gattAttribTypeToHandle.put("00002800-0000-1000-8000-00805F9B34FB", new ArrayList<Integer>());
        gattAttribTypeToHandle.put("00002801-0000-1000-8000-00805F9B34FB", new ArrayList<Integer>());
        gattAttribTypeToHandle.put("00002802-0000-1000-8000-00805F9B34FB", new ArrayList<Integer>());
        gattAttribTypeToHandle.put("00002803-0000-1000-8000-00805F9B34FB", new ArrayList<Integer>());
    }

    /**
     * Removes a particular character from a String
     * @return the String with the character removed
    */
    public static String removeChar(String s, char c) {
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < s.length(); i ++) {
           char cur = s.charAt(i);
           if (cur != c) {
               sb.append(cur);
           }
        }
        return sb.toString();
    }

    /**
     * Converts a hexdecimal String into byte array
     * @return the byte array formed from hexadecimal String
    */
    public static byte[] hexStringToByteArray(String s) {
        int len = s.length();
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            data[i / 2] = (byte) ((Character.digit(s.charAt(i), 16) << 4)
                    + Character.digit(s.charAt(i+1), 16));
        }
        return data;
    }

    /**
     * Converts a String into byte array
     * @return the byte array formed from the input String
    */
    public static byte[] stringToByteArray(String s) {
        int len = s.length();
        int byteArrLen = 0, cnt =0;
        if(len%2 == 0) {
            byteArrLen = len/2;
        }
        else {
            byteArrLen = (len/2)+1;
        }
        byte[] data = new byte[byteArrLen];
        for (int i = 0; i < len; i += 2) {
            if(((i+1) < len)) {
                data[cnt++] = (byte) ((Character.digit(s.charAt(i), 16) << 4)
                        + Character.digit(s.charAt(i+1), 16));
            }
            else if(((i+1) >= len)) {
                data[cnt++] = (byte) (Character.digit(s.charAt(i), 16));
            }
        }
        if(SERVICE_DEBUG) {
            Log.d(TAG, "The String to byte array value::");
            for(int j=0; j< data.length;j++) {
                Log.d(TAG,""+data[j]);
            }
        }
        return data;
    }
    /**
     * Sends notifications and indications to the client
     *
    */
    void sendNotificationIndication() {
        Calendar cal = Calendar.getInstance();
        int day = cal.get(Calendar.DAY_OF_MONTH);
        int month = cal.get(Calendar.MONTH) + 1;
        int year = cal.get(Calendar.YEAR);
        String yearStr = Integer.toString(year);
        yearStr = yearStr.substring(2, 4);
        byte year1 = Byte.valueOf(yearStr);
        int hour = cal.get(Calendar.HOUR_OF_DAY);
        int minute = cal.get(Calendar.MINUTE);
        int second = cal.get(Calendar.SECOND);
        Date today = new Date();
        String todaystring = today.toString();
        int i=0;
        List<Integer> hndlList = null;
        int j=0;
        boolean is_permitted = false;
        if(gattAttribTypeToHandle != null) {
            for(Map.Entry<String, List<Integer>> entry : gattAttribTypeToHandle.entrySet()) {
                if("00002902-0000-1000-8000-00805F9B34FB".
                        equalsIgnoreCase(entry.getKey().toString())) {
                    //List of client characteristic configuration descriptor handles
                    hndlList = entry.getValue();
                }
            }
        }
        if(hndlList!=null) {
            for(j=0; j< hndlList.size(); j++) {
                int handle = hndlList.get(j);
                if(handle >= 0) {
                    if(gattHandleToAttributes != null) {
                        //To get the attribute values for the particular handle
                        for(int k=0; k<gattHandleToAttributes.size(); k++) {
                            if(handle == gattHandleToAttributes.get(k).handle) {
                                Attribute attr = gattHandleToAttributes.get(k);
                                ArrayList<Integer> sessionHdlList = attr.sessionHandle;
                                if(sessionHdlList != null) {
                                    for(int index=0; index < sessionHdlList.size(); index++) {
                                        int sessionHdl = sessionHdlList.get(index);
                                        if(attr.attrValue != null && (attr.attrValue.containsKey(sessionHdl))) {
                                            byte descValue = attr.attrValue.get(sessionHdl);
                                            if((descValue > 0x00) && (descValue <= 0x02)) {
                                                int charValueHdl = attr.referenceHandle;
                                                Attribute attrCharValue =
                                                        gattHandleToAttributes.get(charValueHdl);
                                                byte[] charValue = new byte[attrCharValue.value.length + 6];
                                                for(i=0; i < attrCharValue.value.length; i++) {
                                                    charValue[i] = attrCharValue.value[i];
                                                }
                                                charValue[i++]= (byte)day;
                                                charValue[i++]= (byte)month;
                                                charValue[i++]= year1;
                                                charValue[i++]= (byte)hour;
                                                charValue[i++]= (byte)minute;
                                                charValue[i++]= (byte)second;

                                                int charHandle = attrCharValue.referenceHandle;
                                                Attribute attrChar = gattHandleToAttributes.get(charHandle);
                                                int charProperties = attrChar.properties;
                                                boolean notify = false;
                                                if(descValue == 0x01) {
                                                    notify = true;
                                                    //check for permission from Characteristic properties
                                                    if(charProperties > 0 && ((charProperties & 0x10) == 0x10)) {
                                                        is_permitted = true;
                                                    }
                                                }
                                                else if(descValue == 0x02) {
                                                    notify = false;
                                                    //check for permission from Characteristic properties
                                                    if(charProperties > 0 && ((charProperties & 0x20) == 0x20)) {
                                                        is_permitted = true;
                                                    }
                                                }

                                                if(descValue <= 0x02 && (is_permitted)) {
                                                    if(SERVICE_DEBUG) {
                                                        Log.d(TAG, "The characteristic values notified/indicated " +
                                                                "are as follows:");
                                                        for(int z=0; z<i; z++) {
                                                            Log.d(TAG, ""+charValue[z]);
                                                        }
                                                    }
                                                    boolean result = false;
                                                    result = gattProfile.sendIndication(
                                                            serverConfiguration, charValueHdl,
                                                            charValue, notify, sessionHdl);
                                                    if(SERVICE_DEBUG) {
                                                        Log.d(TAG, "SendIndication result::"+result);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    /**
     * Sends notifications and indications for a specific attribute handle
     * to the client
     *
    */
    void sendNotificationIndicationHandle(int handle, int sessionHandle) {
        Calendar cal = Calendar.getInstance();
        int day = cal.get(Calendar.DAY_OF_MONTH);
        int month = cal.get(Calendar.MONTH) + 1;
        int year = cal.get(Calendar.YEAR);
        String yearStr = Integer.toString(year);
        yearStr = yearStr.substring(2, 4);
        byte year1 = Byte.valueOf(yearStr);
        int hour = cal.get(Calendar.HOUR_OF_DAY);
        int minute = cal.get(Calendar.MINUTE);
        int second = cal.get(Calendar.SECOND);
        int i =0;
        Date today = new Date();
        String todaystring = today.toString();
        boolean is_permitted = false;

        if(gattHandleToAttributes != null) {
            //To get the attribute values for the particular handle
            for(int k=0; k<gattHandleToAttributes.size(); k++) {
                if(handle == gattHandleToAttributes.get(k).handle) {
                    Attribute attr = gattHandleToAttributes.get(k);
                    int sessionHdl = sessionHandle;
                    String attrTypeStr = attr.type;
                    if(attrTypeStr!=null && attrTypeStr.
                            equalsIgnoreCase("00002902-0000-1000-8000-00805F9B34FB")) {
                        if(attr.attrValue != null && (attr.attrValue.containsKey(sessionHandle))) {
                                byte descValue = attr.attrValue.get(sessionHandle);
                            if((descValue > 0x00) && (descValue <= 0x02)) {
                                int charValueHdl = attr.referenceHandle;
                                Attribute attrCharValue = gattHandleToAttributes.get(charValueHdl);
                                byte[] charValue = new byte[attrCharValue.value.length + 6];
                                for(i=0; i < attrCharValue.value.length; i++) {
                                    charValue[i] = attrCharValue.value[i];
                                }
                                charValue[i++]= (byte)day;
                                charValue[i++]= (byte)month;
                                charValue[i++]= year1;
                                charValue[i++]= (byte)hour;
                                charValue[i++]= (byte)minute;
                                charValue[i++]= (byte)second;
                                int charHandle = attrCharValue.referenceHandle;
                                Attribute attrChar =
                                        gattHandleToAttributes.get(charHandle);
                                int charProperties = attrChar.properties;

                                boolean notify = false;
                                if(descValue == 0x01) {
                                    notify = true;
                                    //check for permission from Characteristic properties
                                    if(charProperties > 0 && ((charProperties & 0x10) == 0x10)) {
                                        is_permitted = true;
                                    }
                                }
                                else if(descValue == 0x02) {
                                    notify = false;
                                    //check for permission from Characteristic properties
                                    if(charProperties > 0 && ((charProperties & 0x20) == 0x20)) {
                                        is_permitted = true;
                                    }
                                }

                                if(descValue <= 0x02 && (is_permitted)) {
                                    if(SERVICE_DEBUG) {
                                        Log.d(TAG, "The characteristic values notified/indicated " +
                                                "are as follows:");
                                        for(int z=0; z<i; z++) {
                                            Log.d(TAG, ""+charValue[z]);
                                        }
                                    }
                                    boolean result = gattProfile.sendIndication(serverConfiguration,
                                            charValueHdl, charValue, notify, sessionHdl);
                                    if(SERVICE_DEBUG) {
                                        Log.d(TAG, "SendIndication result::"+result);
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    /**
     * Updates the data structures in this service with the
     * Charactersitic values from the file "characteristic_values"
     *
    */
    void updateDataStructuresFromFile() {
        //logic for updating the Hashmap and  Arraylist from characteristic_values file
        String FILENAME = "characteristic_values";
        byte[] buffer = new byte[2000];
        Context context = getApplicationContext();
        try {
            Log.d(TAG,"File path::"+context.getFilesDir().getAbsolutePath());
            FileInputStream fis = new FileInputStream(context.getFilesDir().getAbsolutePath()
                    + "/" + FILENAME);

            int bytesRead = fis.read(buffer);
            int j=0;//index for the buffer byte array read from file
            while((bytesRead > 0) && (j < bytesRead)) {
                int charValueHandle = (buffer[j] << 8) + buffer[j+1];
                byte len = buffer[j+2];
                byte[] charValue = new byte[len];
                int index = j+3;
                for(int k=0; k < len; k++) {
                    charValue[k] = buffer[index++];
                }
                if(gattHandleToAttributes != null) {
                    for(int i=0; i < gattHandleToAttributes.size(); i++) {
                        Attribute attrPres = gattHandleToAttributes.get(i);
                        if(attrPres.handle == charValueHandle) {
                            attrPres.value = charValue;
                        }
                    }
                }
                //4 represents Handle-2 bytes, len-1 byte, "\n"-1 byte
                j=j+4+buffer[j+2];
            }
            fis.close();
        }
        catch (FileNotFoundException e) {
            e.printStackTrace();
        }
        catch (IOException e) {
            e.printStackTrace();
        }
    }
    /**
     * Checks the write permission of "Characteristic User
     * Description" descriptor by checking the value of
     * "Characteristic Extended Properties" descriptor value
     *
    */
    boolean checkWritePermission(int descHandle) {
        boolean is_writable = false;
        int charValueHandle = -1;
        int charHandle = -1;
        int startHdl = -1;
        int endHdl = -1;
        byte[] attrValue = null;

        if(gattHandleToAttributes != null) {
            charValueHandle = gattHandleToAttributes.get(descHandle).referenceHandle;
            charHandle = gattHandleToAttributes.get(charValueHandle).referenceHandle;
            startHdl = gattHandleToAttributes.get(charHandle).startHandle;
            endHdl = gattHandleToAttributes.get(charHandle).endHandle;
            for(int k=0; k<gattHandleToAttributes.size(); k++) {
                int handle = gattHandleToAttributes.get(k).handle;
                if(handle >= startHdl && handle <= endHdl) {
                    if(gattHandleToAttributes.get(k).
                            type.equalsIgnoreCase("00002900-0000-1000-8000-00805F9B34FB")) {
                        Attribute attr = gattHandleToAttributes.get(k);
                        attrValue = attr.value;
                        if(attrValue != null && (attrValue.length > 0)) {
                            if((attrValue[0] & 0x02) == 0x02) {
                                is_writable = true;
                            }
                        }
                    }
                }
            }
        }
        return is_writable;
    }
    /**
     * Checks whether any of the Client characteristic
     * configuration descriptors are set for all
     * the client devices connected to the GATT server
     * If set, returns true
     * else returns false
     */
    boolean isAnyClientConfigSet() {
        boolean is_permitted = false;
        boolean isClientConfigSet = false;
        List<Integer> hndlList = null;
        if(gattAttribTypeToHandle != null) {
            for(Map.Entry<String, List<Integer>> entry : gattAttribTypeToHandle.entrySet()) {
                if("00002902-0000-1000-8000-00805F9B34FB".
                                equalsIgnoreCase(entry.getKey().toString())) {
                    //List of client characteristic configuration descriptor handles
                    hndlList = entry.getValue();
                }
            }
        }

        if(hndlList!=null) {
            for(int j=0; j< hndlList.size(); j++) {
                int handle = hndlList.get(j);
                if(handle >= 0) {
                    if(gattHandleToAttributes != null) {
                        //To get the attribute values for the particular handle
                        if(handle == gattHandleToAttributes.get(handle).handle) {
                            Attribute attr = gattHandleToAttributes.get(handle);
                            ArrayList<Integer> sessionHdlList = attr.sessionHandle;
                            if(sessionHdlList != null) {
                                for(int index=0; index < sessionHdlList.size(); index++) {
                                    int sessionHdl = sessionHdlList.get(index);
                                    if(attr.attrValue != null && (attr.attrValue.containsKey(sessionHdl))) {
                                        byte descValue = attr.attrValue.get(sessionHdl);
                                        if((descValue > 0x00) && (descValue <= 0x02)) {
                                            int charValueHdl = attr.referenceHandle;
                                            Attribute attrCharValue =
                                                gattHandleToAttributes.get(charValueHdl);

                                            int charHandle = attrCharValue.referenceHandle;
                                            Attribute attrChar = gattHandleToAttributes.get(charHandle);
                                            int charProperties = attrChar.properties;
                                            boolean notify = false;
                                            if(descValue == 0x01) {
                                                notify = true;
                                                //check for permission from Characteristic properties
                                                if(charProperties > 0 && ((charProperties & 0x10) == 0x10)) {
                                                    is_permitted = true;
                                                }
                                            }
                                            else if(descValue == 0x02) {
                                                notify = false;
                                                //check for permission from Characteristic properties
                                                if(charProperties > 0 && ((charProperties & 0x20) == 0x20)) {
                                                    is_permitted = true;
                                                }
                                            }
                                            if(SERVICE_DEBUG) {
                                                Log.d(TAG, "The client config handle is :"+handle);
                                                Log.d(TAG, "The client config descriptor value is :"+descValue);
                                                Log.d(TAG, "The char value handle is :"+charValueHdl);
                                                Log.d(TAG, "The sessionhandle  is :"+sessionHdl);
                                            }

                                            if(descValue <= 0x02 && (is_permitted)) {
                                                isClientConfigSet = true;
                                                return isClientConfigSet;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        return isClientConfigSet;
    }
    public void selectDevice() {
        Intent in1 = new Intent(BluetoothDevicePicker.ACTION_LAUNCH);
        in1.putExtra(BluetoothDevicePicker.EXTRA_NEED_AUTH, false);
        in1.putExtra(BluetoothDevicePicker.EXTRA_FILTER_TYPE,
                     BluetoothDevicePicker.FILTER_TYPE_ALL);
        in1.putExtra(BluetoothDevicePicker.EXTRA_LAUNCH_CLASS,
                GattServerAppReceiver.class.getName());
        in1.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        this.startActivity(in1);
    }
    public void selectConnectedDevice() {
        Context context = getApplicationContext();
        Intent in1 = new Intent(context, DeviceListScreen.class);
        in1.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        this.startActivity(in1);
    }
    private void connectLEDevice() {
        int status = -1;
        if (gattProfile != null && remoteDevice != null) {
            status = gattProfile.
                    gattConnectLe(remoteDevice.getAddress(),(byte)0,(byte)0,
                    4, 4, 8, 256, 0, 192, 1, 1, 0);
            Log.d(TAG, "status of connect request::"+status);
            while(status == BluetoothDevice.GATT_RESULT_BUSY) {
                try {
                    Thread.sleep(3000L);// 3 seconds
                    status = gattProfile.
                            gattConnectLe(remoteDevice.getAddress(),(byte)0,
                            (byte)0, 4, 4, 8, 256, 0, 192, 1, 1, 0);
                }
                catch (Exception e) {}
            }
            if(status == BluetoothDevice.GATT_RESULT_SUCCESS) {
                if(connectedDevicesList != null && connectedDevicesList.size() > 0) {
                    if(!connectedDevicesList.contains(remoteDevice.getAddress())) {
                        connectedDevicesList.add(remoteDevice);
                    }
                }
                else {
                    connectedDevicesList = new ArrayList<BluetoothDevice>();
                    connectedDevicesList.add(remoteDevice);
                }
            }
        }
        else {
            Log.d(TAG, " remoteDevice is null");
        }
    }
    public void disconnectLEDevice(BluetoothDevice clientDevice) {
        boolean status = false;
        if (clientDevice != null) {
            status = gattProfile.closeGattLeConnection(serverConfiguration, clientDevice.getAddress());
            if(status) {
                if(connectedDevicesList != null && connectedDevicesList.size() > 0) {
                    for(int i=0; i < connectedDevicesList.size(); i++) {
                        BluetoothDevice deviceObj = connectedDevicesList.get(i);
                        if(deviceObj.getAddress().equalsIgnoreCase(clientDevice.getAddress())) {
                            connectedDevicesList.remove(i);
                        }
                    }
                }
            }
            Log.d(TAG, "status of disconnect request::"+status);
        }
        else {
            Log.d(TAG, " clientDevice is null");
        }
    }
 }
