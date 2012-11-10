/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

package com.android.bluetooth.proximity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.UUID;
import java.util.Timer;
import java.util.TimerTask;

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.IBluetoothGattProfile;
import android.bluetooth.IBluetoothLEProximityServices;
import android.bluetooth.IBluetoothThermometerCallBack;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.util.Log;

public class LEProximityServices extends Service {

    private static final short CLIENT_CONF_INDICATE_VALUE = 2;

    private static final short CLIENT_CONF_NOTIFY_VALUE = 1;

    private static final int DATE_START_INDEX = 2;

    private static final int DATE_SIZE = 7;

    private static final int OCTET_SIZE = 8;

    private static final String ISO_DATE_FORMAT = "yyyy-MM-dd HH:mm:ss";

    private static final int HEX_RADIX = 16;

    private static final int SIZE_FOUR = 4;

    private static final int SIZE_TWO = 2;

    private static final byte PROHIBIT_REMOTE_CHG = 0;

    private static final byte FILTER_POLICY = 0;

    private static final int AGRESSIVE_SCAN_INTERVAL = 96;

    private static final int SCAN_INTERVAL = 4096;

    private static final int AGRESSIVE_SCAN_WINDOW = 48;

    private static final int SCAN_WINDOW = 18;

    private static final int CONNECTION_INTERVAL_MIN = 8;

    private static final int CONNECTION_INTERVAL_MAX = 256;

    private static final int SUPERVISION_TIMEOUT = 192;

    private static final int MIN_CE_LEN = 1;

    private static final int MAX_CE_LEN = 1;

    private static final int CONNECTION_ATTEMPT_TIMEOUT = 30;

    private static final int CONNECTION_ATTEMPT_INFINITE_TIMEOUT = 0;

    private static final int RECONNECTION_TASK_TIMEOUT = 31000;

    private static final int LATENCY = 0;

    private static final String TAG = "LEProximityServices";

    private int mStartId = -1;

    private BluetoothAdapter mAdapter;

    private IBluetoothThermometerCallBack srvCallBack = null;

    private boolean mHasStarted = false;

    public static ParcelUuid GATTServiceUUID = null;

    private Timer timer = null;

    public static final String USER_DEFINED = "UserDefined";

    public static LEProximityDevice mDevice;

    public static final int ERROR = Integer.MIN_VALUE;

    public static String[] characteristicsPath = null;

    public static ParcelUuid[] uuidArray = null;

    protected static final int GATT_SERVICE_STARTED = 0;

    public static final String ACTION_GATT_SERVICE_EXTRA = "ACTION_GATT_SERVICE_EXTRA";

    public static final int GATT_SERVICE_STARTED_UUID = 0;

    public static final int GATT_SERVICE_STARTED_OBJ = 1;

    public static final int GATT_SERVICE_CHANGED = 2;

    public static final int GATT_SERVICE_CONNECTED = 3;

    public static final int GATT_SERVICE_DISCONNECTED = 4;

    public static final int REMOTE_DEVICE_RSSI_UPDATE = 5;

    public static final String ACTION_GATT_SERVICE_EXTRA_DEVICE = "ACTION_GATT_SERVICE_EXTRA_DEVICE";

    public static final String ACTION_GATT_SERVICE_EXTRA_OBJ = "ACTION_GATT_SERVICE_EXTRA_OBJ";

    public static final String ACTION_RSSI_UPDATE_EXTRA_OBJ = "ACTION_RSSI_UPDATE_EXTRA_OBJ";

    public static final String ACTION_GATT_SERVICE_CHANGED_DEVICE = "ACTION_GATT_SERVICE_CHANGED_DEVICE";

    public static final int TEMP_MSR_INTR_MIN = 1;

    public static final int TEMP_MSR_INTR_MAX = 65535;

    public static final String LINK_LOSS_SERVICE_UUID = "0000180300001000800000805f9b34fb";

    public static final String IMMEDIATE_ALERT_SERVICE_UUID = "0000180200001000800000805f9b34fb";

    public static final String TX_POWER_SERVICE_UUID = "0000180400001000800000805f9b34fb";

    public static final String ALERT_LEVEL_UUID = "00002a0600001000800000805f9b34fb";

    public static final String TX_POWER_LEVEL_UUID = "00002a0700001000800000805f9b34fb";

    public static final String TEMPERATURE_MEASUREMENT_UUID = "00002a1c00001000800000805f9b34fb";

    public static final String TEMPERATURE_TYPE_UUID = "00002a1d00001000800000805f9b34fb";

    public static final String INTERMEDIATE_TEMPERATURE_UUID = "00002a1e00001000800000805f9b34fb";

    public static final String MEASUREMENT_INTERVAL_UUID = "00002a2100001000800000805f9b34fb";

    public static final String DATE_TIME_UUID = "00002a0800001000800000805f9b34fb";

    public static final String MANUFACTURER_NAME_STRING_UUID = "00002a2900001000800000805f9b34fb";

    public static final String MODEL_NUMBER_STRING_UUID = "00002a2400001000800000805f9b34fb";

    public static final String SERIAL_NUMBER_STRING_UUID = "00002a2500001000800000805f9b34fb";

    public static final String HARDWARE_REVISION_STRING_UUID = "00002a2700001000800000805f9b34fb";

    public static final String FIRMWARE_REVISION_STRING_UUID = "00002a2600001000800000805f9b34fb";

    public static final String SOFTWARE_REVISION_STRING_UUID = "00002a2800001000800000805f9b34fb";

    public static final String SYSTEM_ID_UUID = "00002a2300001000800000805f9b34fb";

    public static final String CERTIFICATION_DATA_UUID = "00002a2a00001000800000805f9b34fb";

    public static final String PROXIMITY_SERVICE_OPERATION = "PROXIMITY_SERVICE_OPERATION";

    public static final String PROXIMITY_SERVICE_OP_SERVICE_READY = "PROXIMITY_SERVICE_OP_SERVICE_READY";

    public static final String PROXIMITY_SERVICE_OP_READ_VALUE = "PROXIMITY_SERVICE_OP_READ";

    public static final String PROXIMITY_SERVICE_OP_DEV_DISCONNECTED = "PROXIMITY_SERVICE_OP_DEV_DISCONNECTED";

    public static final String PROXIMITY_SERVICE_OP_DEV_CONNECTED = "PROXIMITY_SERVICE_OP_DEV_CONNECTED";

    public static final String PROXIMITY_SERVICE_OP_PATH_LOSS_EXCEEDED = "PROXIMITY_SERVICE_OP_PATH_LOSS_EXCEEDED";

    public static final String PROXIMITY_SERVICE_OP_STATUS = "PROXIMITY_SERVICE_OP_STATUS";

    public static final String PROXIMITY_SERVICE_OP_VALUE = "PROXIMITY_SERVICE_OP_VALUE";

    public static final String PROXIMITY_SERVICE_OP_WRITE_VALUE = "PROXIMITY_SERVICE_OP_WRITE_VALUE";

    public static final String PROXIMITY_SERVICE_CHANGE = "PROXIMITY_SERVICE_CHANGE";

    public static final String PROXIMITY_SERVICE_OP_REGISTER_NOTIFY_INDICATE = "PROXIMITY_SERVICE_OP_REGISTER_NOTIFY_INDICATE";

    public static final String PROXIMITY_SERVICE_CHAR_UUID = "PROXIMITY_SERVICE_CHAR_UUID";

    public static final String PROXIMITY_SERVICE_NOTIFICATION_INDICATION_VALUE = "PROXIMITY_SERVICE_NOTIFICATION_INDICATION_VALUE";

    public static enum TempType {
        ARMPIT, BODY, EAR, FINGER, GASTRO, MOUTH, RECT, TOE, TYMPHANUM
    }

    public static HashMap<Integer, String> tempTypeMap = null;

    private IntentFilter inFilter = null;

    private LEProximityReceiver receiver = null;

    public final Handler msgHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
            case GATT_SERVICE_STARTED_UUID:
                Log.d(TAG, "Received GATT_SERVICE_STARTED_UUID message");
                break;
            case GATT_SERVICE_STARTED_OBJ:
                Log.d(TAG, "Received GATT_SERVICE_STARTED_OBJ message");
                ArrayList<String> gattDataList = msg.getData()
                                                 .getStringArrayList(ACTION_GATT_SERVICE_EXTRA_OBJ);
                int size = gattDataList.size();
                Log.d(TAG, "GATT Service data list len : " + size);
                String selectedServiceObjPath = gattDataList.get(0);
                Log.d(TAG, "GATT Service path array obj : "
                      + selectedServiceObjPath);
                String uuidStr = gattDataList.get(size - 1);
                Log.d(TAG, "GATT Service uuidStr : " + uuidStr);
                ParcelUuid selectedUUID = ParcelUuid.fromString(uuidStr);
                Log.d(TAG, "ParcelUUID rep of selectedUUID : " + selectedUUID);

                if(isProximityProfileService(selectedUUID)) {
                    Log.d(TAG, "Proceed to creating proximity profile gatt service");
                    mDevice.uuidObjPathMap.put(uuidStr + ":" + uuidStr,
                                               selectedServiceObjPath);
                    mDevice.objPathUuidMap.put(selectedServiceObjPath, uuidStr
                                               + ":" + uuidStr);
                    Log.d(TAG, "getBluetoothGattService");
                    getBluetoothGattService(selectedServiceObjPath, selectedUUID);
                }

                break;
            case GATT_SERVICE_DISCONNECTED:
                Log.d(TAG, "Received GATT_SERVICE_DISCONNECTED message");
                String bdAddr = msg.getData().getString(
                                                       ACTION_GATT_SERVICE_EXTRA_DEVICE);

                if (mDevice.BDevice.getAddress().equals(bdAddr)) {
                    Log.d(TAG,
                          " received  GATT_SERVICE_DISCONNECTED for device : "
                          + bdAddr);
                    ParcelUuid uuid = convertStrToParcelUUID(LINK_LOSS_SERVICE_UUID);
                    ArrayList<String> values = new ArrayList<String>();
                    values.add(bdAddr);
                    values.add(String.valueOf(mDevice.linkLossAlertLevel));
                    bundleAndSendResult(uuid,
                                        PROXIMITY_SERVICE_OP_DEV_DISCONNECTED, true, values);

                    /* Attempt to connect back to the service */
                    Log.d(TAG, "Attempting to reconnect back");
                    gattReconnect(convertStrToParcelUUID(LINK_LOSS_SERVICE_UUID));
                }
                break;
            case GATT_SERVICE_CONNECTED:
                Log.d(TAG, "Received GATT_SERVICE_CONNECTED message");
                cleanUpTimer();
                String connAddr = msg.getData().getString(
                                                         ACTION_GATT_SERVICE_EXTRA_DEVICE);

                if (mDevice.BDevice.getAddress().equals(connAddr)) {
                    Log.d(TAG,
                          " received  GATT_SERVICE_CONNECTED for device : "
                          + connAddr);
                    ParcelUuid linkUuid = convertStrToParcelUUID(LINK_LOSS_SERVICE_UUID);
                    readUpdatedCharValue(
                                        convertStrToParcelUUID(ALERT_LEVEL_UUID),
                                        convertStrToParcelUUID(LINK_LOSS_SERVICE_UUID));
                    ArrayList<String> vals = new ArrayList<String>();
                    vals.add(connAddr);
                    vals.add(String.valueOf(mDevice.linkLossAlertLevel));
                    bundleAndSendResult(linkUuid,
                                        PROXIMITY_SERVICE_OP_DEV_CONNECTED, true, vals);
                }
                break;
            case REMOTE_DEVICE_RSSI_UPDATE:
                Log.d(TAG, "Received REMOTE_DEVICE_RSSI_UPDATE message");
                                    ArrayList<String> rssiDataList = msg.getData()
                                                 .getStringArrayList(ACTION_RSSI_UPDATE_EXTRA_OBJ);
                int len = rssiDataList.size();
                Log.d(TAG, "RSSI data list len : " + len);
                String devAddr = rssiDataList.get(0);
                Log.d(TAG, "RSSI update for device : " + devAddr);
                String rssiVal = rssiDataList.get(1);
                Log.d(TAG, "RSSI Value: " + rssiVal);

                mDevice.rssi = (byte) Integer.parseInt(rssiVal);
                Log.d(TAG, "mDevice.rssi: " + mDevice.rssi);

                Log.d(TAG, "Tx power level :" + mDevice.txPower);
                int pathLoss = mDevice.txPower - mDevice.rssi;
                Log.d(TAG, "Path Loss : " + pathLoss);
                ParcelUuid immAlertSrvUuid = convertStrToParcelUUID(IMMEDIATE_ALERT_SERVICE_UUID);
                ParcelUuid alertLevelUuid = convertStrToParcelUUID(ALERT_LEVEL_UUID);
                boolean srvAlive = mDevice.uuidGattSrvMap
                                   .containsKey(immAlertSrvUuid);

                if (pathLoss > mDevice.pathLossThresh) {

                    Log.d(TAG, "Set the IMMEDIATE_ALERT_SERVICE_UUID to high");
                    writeProximityCharValue(alertLevelUuid, immAlertSrvUuid,
                                            String.valueOf(2));
                    if ((mDevice.BDevice != null) && srvAlive) {
                        mDevice.BDevice.registerRssiUpdateWatcher(mDevice.rssiThresh,
                                                                  mDevice.rssiInterval, true);
                    }
                    bundleAndSendResult(
                                       convertStrToParcelUUID(IMMEDIATE_ALERT_SERVICE_UUID),
                                       PROXIMITY_SERVICE_OP_PATH_LOSS_EXCEEDED, true,
                                       new ArrayList<String>());

                } else {
                    Log.d(TAG, "Set the IMMEDIATE_ALERT_SERVICE_UUID to low");
                    writeProximityCharValue(alertLevelUuid, immAlertSrvUuid,
                                            String.valueOf(0));
                    if ((mDevice.BDevice != null) && srvAlive) {
                        mDevice.BDevice
                        .registerRssiUpdateWatcher(mDevice.rssiThresh,
                                                   mDevice.rssiInterval, false);
                    }
                    bundleAndSendResult(
                                       convertStrToParcelUUID(IMMEDIATE_ALERT_SERVICE_UUID),
                                       PROXIMITY_SERVICE_OP_PATH_LOSS_EXCEEDED, false,
                                       new ArrayList<String>());
                }
                break;
            default:
                break;
            }
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG, "onCreate Proximity service");
        mAdapter = BluetoothAdapter.getDefaultAdapter();

        if (!mHasStarted) {
            mHasStarted = true;
            Log.e(TAG, "Creating proximity service");
            int state = mAdapter.getState();
            if (state == BluetoothAdapter.STATE_ON) {
                Log.d(TAG, "Bluetooth is on");
                mDevice = new LEProximityDevice();
                mDevice.uuidObjPathMap = new HashMap<String, String>();
                mDevice.objPathUuidMap = new HashMap<String, String>();
                mDevice.uuidGattSrvMap = new HashMap<ParcelUuid, BluetoothGattService>();

                Log.d(TAG, "registering receiver handler");
                LEProximityReceiver.registerHandler(msgHandler);

                inFilter = new IntentFilter();
                inFilter.addAction("android.bluetooth.device.action.GATT");
                inFilter.addAction("android.bluetooth.device.action.ACL_DISCONNECTED");
                inFilter.addAction("android.bluetooth.device.action.ACL_CONNECTED");
                inFilter.addAction("android.bluetooth.device.action.RSSI_UPDATE");

                this.receiver = new LEProximityReceiver();
                Log.d(TAG, "Registering the receiver");
                this.registerReceiver(this.receiver, inFilter);
            } else {
                Log.d(TAG, "Bluetooth is not on");
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind proximity service");
        return mBinder;
    }

    @Override
    public void onRebind(Intent intent) {
        Log.d(TAG, "onRebind proximity service");
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.d(TAG, "onUnbind proximity service");
        return true;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy proximity service");

        cleanUpTimer();
        //cancel gatt reconnect request if pending.
        boolean res = gattReconnectCancel();
        Log.d(TAG, "Gatt reconnect cancel : " + res);

        closeAllProximityServices();
        Log.d(TAG, "Unregistering the receiver");
        if (this.receiver != null) {
            try {
                this.unregisterReceiver(this.receiver);
            } catch (Exception e) {
                Log.e(TAG, "Error while unregistering the receiver");
            }
        }
        mDevice = null;
    }

    private boolean closeAllProximityServices() {
        boolean closeLinkLoss = closeService(convertStrToParcelUUID(LINK_LOSS_SERVICE_UUID));
        Log.d(TAG, "Closing the proximity Link Loss service : " + closeLinkLoss);
        boolean closeImmAlert = closeService(convertStrToParcelUUID(IMMEDIATE_ALERT_SERVICE_UUID));
        Log.d(TAG, "Closing the Imm Alert  service : " + closeImmAlert);
        boolean closeTxPower = closeService(convertStrToParcelUUID(TX_POWER_SERVICE_UUID));
        Log.d(TAG, "Closing the Tx Power service : " + closeTxPower);
        if (closeLinkLoss && closeImmAlert && closeTxPower) {
            return true;
        }
        return false;
    }

    private boolean closeService(ParcelUuid srvUuid) {
        if (mDevice != null) {
            BluetoothGattService gattService = mDevice.uuidGattSrvMap
                                               .get(srvUuid);
            if (gattService != null) {
                try {
                    Log.d(TAG, "Calling gattService.close()");
                    gattService.close();
                    Log.d(TAG, "removing Gatt service for UUID : " + srvUuid);
                    mDevice.uuidGattSrvMap.remove(srvUuid);
                    LEProximityReceiver.setSendDisconnect(false);
                    return true;
                } catch (Exception e) {
                    Log.e(TAG,
                          "************Error while closing the Gatt Service");
                    e.printStackTrace();
                    return false;
                }
            }
        }
        return false;
    }

    private final IBluetoothGattProfile.Stub btGattCallback = new IBluetoothGattProfile.Stub() {
        public void onDiscoverCharacteristicsResult(String path, boolean result) {
            Log.d(TAG, "onDiscoverCharacteristicsResult : " + "path : " + path
                  + "result : " + result);
            String srvCharUuid = mDevice.objPathUuidMap.get(path);
            String[] uuids = srvCharUuid.split(":");
            ParcelUuid srvUUID = ParcelUuid.fromString(uuids[0]);
            BluetoothGattService gattService = mDevice.uuidGattSrvMap
                                               .get(srvUUID);
            if (result) {
                if (gattService != null) {
                    Log.d(TAG, "gattService.getServiceUuid() ======= "
                          + srvUUID.toString());
                    try {
                        discoverCharacteristics(srvUUID);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                } else {
                    Log.e(TAG, " gattService is null");
                }
            } else {
                Log.e(TAG, "Discover characterisitcs failed ");
            }

        }

        public void onSetCharacteristicValueResult(String path, boolean result) {
            String srvCharUuid = mDevice.objPathUuidMap.get(path);

            Log.d(TAG, "callback onSetCharacteristicValueResult: ");
            Log.d(TAG, "result : " + result);

            String[] uuids = srvCharUuid.split(":");
            ParcelUuid uuid = ParcelUuid.fromString(uuids[1]);
            Log.d(TAG, "uuid : " + uuid);
            ParcelUuid srvUuid = ParcelUuid.fromString(uuids[0]);

            bundleAndSendResult(uuid, PROXIMITY_SERVICE_OP_WRITE_VALUE, result,
                                new ArrayList<String>());
        }

        public void onSetCharacteristicCliConfResult(String path, boolean result) {
        }

        public void onValueChanged(String path, String value) {

        }

        public void onUpdateCharacteristicValueResult(String arg0, boolean arg1) {
            Log.d(TAG, "callback onUpdateCharacteristicValueResult: ");
            Log.d(TAG, "arg0 : " + arg0);
            Log.d(TAG, "arg1 : " + arg1);
            String srvCharUuid = mDevice.objPathUuidMap.get(arg0);
            String[] uuids = srvCharUuid.split(":");
            ParcelUuid uuid = ParcelUuid.fromString(uuids[1]);
            Log.d(TAG, "uuid : " + uuid);
            ParcelUuid srvUuid = ParcelUuid.fromString(uuids[0]);
            if (arg1) {
                if (convertStrToParcelUUID(ALERT_LEVEL_UUID).toString().equals(
                                                                              uuid.toString())) {
                    Log.d(TAG, "Reading the PROXIMITY Charactersitics");
                    String value = readProximityChar(uuid, srvUuid);
                    bundleAndSendResult(uuid, PROXIMITY_SERVICE_OP_READ_VALUE,
                                        true, new ArrayList<String>(Arrays.asList(value)));
                } else if (convertStrToParcelUUID(TX_POWER_LEVEL_UUID)
                           .toString().equals(uuid.toString())) {
                    Log.d(TAG, "Reading the TX power Charactersitics");
                    String value = readTXPower(uuid, srvUuid);
                    bundleAndSendResult(uuid, PROXIMITY_SERVICE_OP_READ_VALUE,
                                        true, new ArrayList<String>(Arrays.asList(value)));
                } else {
                    Log.d(TAG, "Proximity char UUID does not match");

                }

            } else {
                Log.e(TAG, "onUpdateCharacteristicValueResult : " + arg1);
                bundleAndSendResult(uuid, PROXIMITY_SERVICE_OP_READ_VALUE,
                                    false, new ArrayList<String>());
            }
        }
    };

    private final IBluetoothLEProximityServices.Stub mBinder =
    new IBluetoothLEProximityServices.Stub() {
        public synchronized boolean startProximityService(BluetoothDevice btDevice, ParcelUuid uuid,
                                                          IBluetoothThermometerCallBack callBack)
        throws RemoteException {

            Log.d(TAG, "Inside startGattService: ");
            if (mDevice == null) {
                Log.e(TAG, "mDevice is null");
                return false;
            }
            if (!(mDevice.uuidGattSrvMap.containsKey(uuid))) {
                Log.d(TAG, "Creating new GATT service for UUID : " + uuid);
                srvCallBack = callBack;
                if ((mDevice.BDevice != null)
                    && (mDevice.BDevice.getAddress().equals(btDevice
                                                            .getAddress()))) {
                    Log.d(TAG,
                          "services have already been discovered. Create Gatt service");
                    String objPath = mDevice.uuidObjPathMap.get(uuid + ":"
                                                                + uuid);
                    if (objPath != null) {
                        Log.d(TAG, "GET GATT SERVICE for : " + uuid);
                        getBluetoothGattService(objPath, uuid);
                    } else {
                        Log.d(TAG,
                              "action GATT has not been received for uuid : "
                              + uuid);
                        return getGattServices(uuid, btDevice);
                    }
                } else {
                    Log.d(TAG, "Primary services need to be discovered");
                    return getGattServices(uuid, btDevice);
                }

            } else {
                Log.d(TAG, "Gatt service already exists for UUID : " + uuid);
                bundleAndSendResult(uuid,
                                    LEProximityServices.PROXIMITY_SERVICE_OP_SERVICE_READY,
                                    true, new ArrayList<String>());
            }
            return true;
        }

        public synchronized void readCharacteristicsValue(ParcelUuid charUuid,
                                                          ParcelUuid srvUuid)
        throws RemoteException {
            Log.d(TAG, "Inside Proximity readCharacteristics");
            readUpdatedCharValue(charUuid, srvUuid);

        }

        public synchronized boolean writeCharacteristicsValue(ParcelUuid uuid,
                                                              ParcelUuid srvUuid, String value)
        throws RemoteException {
            Log.d(TAG, "Inside Proximity writeCharacteristics");
            Log.d(TAG, "Compare " + srvUuid + ":" + uuid);
            for (String key : mDevice.uuidObjPathMap.keySet()) {
                Log.d(TAG, "key : " + key);
            }
            return writeProximityCharValue(uuid, srvUuid, value);
        }

        public synchronized void registerRssiUpdates(BluetoothDevice btDevice,
                                                     int pathLossThresh, int interval)
        throws RemoteException {
            if (btDevice != null) {
                String bdAddr = btDevice.getAddress();
                if (mDevice.BDevice.getAddress().equals(bdAddr)) {
                    Log.d(TAG, "Setting path loss threshold for the device : "
                          + bdAddr);
                    mDevice.pathLossThresh = pathLossThresh;
                    mDevice.rssiInterval = interval;
                    mDevice.rssiThresh = mDevice.txPower - pathLossThresh;

                    Log.d(TAG, "Rssi thresh : " + mDevice.rssiThresh
                          + "Path loss thresh : " + mDevice.pathLossThresh);
                    Log.d(TAG, "Register RSSI watcher");
                    mDevice.BDevice.registerRssiUpdateWatcher(mDevice.rssiThresh,
                                                              mDevice.rssiInterval, false);
                }
            } else {
                Log.e(TAG, "The device is null");
            }
        }

        public synchronized void unregisterRssiUpdates(BluetoothDevice btDevice)
        throws RemoteException {
            if (btDevice != null) {
                String bdAddr = btDevice.getAddress();
                if (mDevice.BDevice.getAddress().equals(bdAddr)) {
                    Log.d(TAG, "unregister rssi updates for the device : "
                          + bdAddr);
                    mDevice.BDevice.unregisterRssiUpdateWatcher();
                }
            } else {
                Log.e(TAG, "The device is null");
            }
        }

        public synchronized boolean closeProximityService(BluetoothDevice btDevice,
                                                          ParcelUuid srvUuid)
        throws RemoteException {
            return closeService(srvUuid);
        }
    };

    private void readUpdatedCharValue(ParcelUuid charUuid, ParcelUuid srvUuid) {

        Bundle bundle = null;
        String readCharUUID = charUuid.toString();
        if (mDevice.uuidObjPathMap.containsKey(srvUuid + ":" + charUuid)) {
            Log.d(TAG, "uuidObjPathMap containsKey " + srvUuid + ":" + charUuid);
            if ((convertStrToParcelUUID(ALERT_LEVEL_UUID).toString()
                 .equals(readCharUUID))
                || (convertStrToParcelUUID(TX_POWER_LEVEL_UUID).toString()
                    .equals(readCharUUID))) {
                Log.d(TAG, "Reading the proximity Charactersitics");
                if (!updateCharacteristic(charUuid, srvUuid)) {
                    bundleAndSendResult(charUuid,
                                        PROXIMITY_SERVICE_OP_READ_VALUE, false,
                                        new ArrayList<String>());
                }
            } else {
                Log.e(TAG, "This characteristics : " + charUuid
                      + " cannot be read");
                bundleAndSendResult(charUuid, PROXIMITY_SERVICE_OP_READ_VALUE,
                                    false, new ArrayList<String>());
            }
        } else {
            Log.e(TAG, "The character parcel uuid is invalid");
            bundleAndSendResult(charUuid, PROXIMITY_SERVICE_OP_READ_VALUE,
                                false, new ArrayList<String>());
        }
    }

    private boolean writeProximityCharValue(ParcelUuid uuid,
                                            ParcelUuid srvUuid, String value) {
        boolean result = false;
        String writeCharUUID = uuid.toString();
        if (!mDevice.uuidObjPathMap.containsKey(srvUuid + ":" + uuid)) {
            Log.e(TAG, "The character parcel uuid is invalid");
            return false;
        }
        if (convertStrToParcelUUID(ALERT_LEVEL_UUID).toString().equals(
                                                                      writeCharUUID)) {
            Log.d(TAG, "writing the alert level Charactersitics for service : "
                  + srvUuid);
            ParcelUuid linkLossUuid = convertStrToParcelUUID(LINK_LOSS_SERVICE_UUID);
            if(linkLossUuid.toString().equals(srvUuid.toString()))
               result = writeAlertLevel(uuid, srvUuid, value, true);
            else
                result = writeAlertLevel(uuid, srvUuid, value, false);

            if (result
                && (srvUuid.toString().
                    equals(convertStrToParcelUUID(LINK_LOSS_SERVICE_UUID).toString()))) {

                mDevice.linkLossAlertLevel = Integer.parseInt(value);
                Log.d(TAG, "populate link loss alert level : "
                      + mDevice.linkLossAlertLevel);
            }
            Log.d(TAG, "write Alert level : " + result);
        } else {
            Log.e(TAG, "This characteristics : " + uuid + " cannot be written");
            result = false;
        }
        return result;

    }

    private boolean isProximityProfileService(ParcelUuid uuid) {
        if(convertStrToParcelUUID(LINK_LOSS_SERVICE_UUID).toString().
           equals(uuid.toString()) ||
           convertStrToParcelUUID(IMMEDIATE_ALERT_SERVICE_UUID).toString().
           equals(uuid.toString()) ||
           convertStrToParcelUUID(TX_POWER_SERVICE_UUID).toString().
           equals(uuid.toString())) {
            return true;
        }
        return false;
    }

    private boolean getGattServices(ParcelUuid uuid, BluetoothDevice btDevice) {
        mDevice.BDevice = btDevice;
        Log.d(TAG, "Setting Preferred Connection Parameters");
        setPreferredConnParameters();
        Log.d(TAG, "GATT Extra Bt Device : " + mDevice.BDevice);
        Log.d(TAG, "GATT UUID : " + uuid);
        Log.d(TAG, "Calling  btDevice.getGattServices");
        return btDevice.getGattServices(uuid.getUuid());
    }

    private void bundleAndSendResult(ParcelUuid uuid, String operation,
                                     boolean result, ArrayList<String> values) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(PROXIMITY_SERVICE_CHAR_UUID, uuid);
        bundle.putString(PROXIMITY_SERVICE_OPERATION, operation);
        bundle.putBoolean(PROXIMITY_SERVICE_OP_STATUS, result);
        bundle.putStringArrayList(PROXIMITY_SERVICE_OP_VALUE, values);
        try {
            Log.d(TAG, "proximitySrvCallBack.sendResult");
            srvCallBack.sendResult(bundle);
        } catch (RemoteException e) {
            Log.e(TAG, "proximitySrvCallBack.sendResult failed");
            e.printStackTrace();
        }
    }

    private ParcelUuid convertStrToParcelUUID(String uuidStr) {
        return new ParcelUuid(convertUUIDStringToUUID(uuidStr));
    }

    private void getBluetoothGattService(String objPath, ParcelUuid uuid) {
        if ((mDevice != null) && (mDevice.BDevice != null)) {
            Log.d(TAG, " ++++++ Creating BluetoothGattService with device = "
                  + mDevice.BDevice.getAddress() + " uuid " + uuid.toString()
                  + " objPath = " + objPath);

            BluetoothGattService gattService =
            new BluetoothGattService(mDevice.BDevice, uuid, objPath, btGattCallback);

            if (gattService != null) {
                mDevice.uuidGattSrvMap.put(uuid, gattService);
                Log.d(TAG, "Adding gatt service to map for : " + uuid
                      + "size :" + mDevice.uuidGattSrvMap.size());
                boolean isDiscovered = gattService.isDiscoveryDone();
                Log.d(TAG, "isDiscovered returned : " + isDiscovered);
                if (isDiscovered) {
                    discoverCharacteristics(uuid);
                }
            } else {
                Log.e(TAG, "Gatt service is null for UUID : " + uuid.toString());
            }
        } else {
            Log.e(TAG, " mDevice is null");
        }

    }

    private void discoverCharacteristics(ParcelUuid srvUUID) {
        Log.d(TAG, "Calling gattService.getCharacteristics()");

        BluetoothGattService gattService = mDevice.uuidGattSrvMap.get(srvUUID);
        if (gattService != null) {
            String[] charObjPathArray = gattService.getCharacteristics();
            if (charObjPathArray != null) {
                Log.d(TAG, " charObjPath length " + charObjPathArray.length);
                for (String objPath : Arrays.asList(charObjPathArray)) {
                    ParcelUuid parcelUUID = gattService
                                            .getCharacteristicUuid(objPath);
                    mDevice.uuidObjPathMap.put(srvUUID + ":" + parcelUUID,
                                               objPath);
                    mDevice.objPathUuidMap.put(objPath, srvUUID + ":"
                                               + parcelUUID);
                    Log.d(TAG, " Map with key UUID : " + parcelUUID
                          + " value : " + objPath);
                }
                Log.d(TAG,
                      "Created map with size : "
                      + mDevice.uuidObjPathMap.size());

                Log.d(TAG, "update LE Connection Parameters");
                updateConnectionParameters();

                getTXPowerLevelVal(srvUUID);

                bundleAndSendResult(srvUUID,
                                    LEProximityServices.PROXIMITY_SERVICE_OP_SERVICE_READY,
                                    true, new ArrayList<String>());
            } else {
                Log.e(TAG, " gattService.getCharacteristics() returned null");
            }
        } else {
            Log.e(TAG, "Gatt service is null for UUID :" + srvUUID);
        }

    }

    private void updateConnectionParameters() {
        if(!mDevice.isConnectionParamUpdated) {
            mDevice.isConnectionParamUpdated = mDevice.BDevice.
                        updateLEConnectionParams((byte)0, 8, 256, 0, 192);
            Log.d(TAG, "update LE connection parameters result : " +
                          mDevice.isConnectionParamUpdated);
        }
    }

    private void setPreferredConnParameters() {
        if(!mDevice.isConnectionParamSet) {
            mDevice.isConnectionParamSet = mDevice.BDevice.
                    setLEConnectionParams((byte)0, (byte)0, 4, 4, 8, 256, 0, 192, 1, 1);
            Log.d(TAG, "Set preferred LE connection parameters result : " +
                      mDevice.isConnectionParamSet);
        }
    }
    private boolean gattReconnect(ParcelUuid srvUuid) {
        boolean result;
        //fast connection
        result = gattConnect(srvUuid, PROHIBIT_REMOTE_CHG, FILTER_POLICY, AGRESSIVE_SCAN_INTERVAL,
                             AGRESSIVE_SCAN_WINDOW, CONNECTION_INTERVAL_MIN,
                             CONNECTION_INTERVAL_MAX, LATENCY, SUPERVISION_TIMEOUT, MIN_CE_LEN,
                             MAX_CE_LEN, CONNECTION_ATTEMPT_TIMEOUT);
        if(result) {
            Log.d(TAG, "Scheduling timer for 30 seconds");
            cleanUpTimer();
            timer = new Timer();
            timer.schedule(new reconnectTask(), RECONNECTION_TASK_TIMEOUT);
            return true;
        }
        return false;
    }

    private boolean gattConnect(ParcelUuid srvUuid, byte prohibitRemoteChg, byte filterPolicy,
                                int scanInterval, int scanWindow, int intervalMin, int intervalMax,
                                int latency, int superVisionTimeout, int minCeLen, int maxCeLen,
                                int connTimeout) {
        BluetoothGattService gattService = mDevice.uuidGattSrvMap.get(srvUuid);
        if (gattService != null) {
            //Fast Connection parameters for first 30 seconds.
            return gattService.gattConnect(prohibitRemoteChg, filterPolicy, scanInterval,
                                           scanWindow, intervalMin, intervalMax,
                                           latency, superVisionTimeout, minCeLen, maxCeLen,
                                           connTimeout);
        }
        Log.d(TAG, "Gatt service is null.. cannot connect to gatt service");
        return false;
    }

    private boolean gattReconnectCancel() {
        ParcelUuid srvUuid = convertStrToParcelUUID(LINK_LOSS_SERVICE_UUID);
        BluetoothGattService gattService = mDevice.uuidGattSrvMap.get(srvUuid);
        if(gattService != null) {
            return gattService.gattConnectCancel();
        }
        Log.d(TAG, "Gatt service does not exist");
        return false;
    }

    private void cleanUpTimer() {
        if(timer != null) {
            Log.d(TAG,"Cancel the timer");
            timer.cancel();
            timer.purge();
            timer = null;
        }
    }

    private void getTXPowerLevelVal(ParcelUuid srvUUID) {
        ParcelUuid txCharUuid = convertStrToParcelUUID(TX_POWER_LEVEL_UUID);
        ParcelUuid txSrvUuid = convertStrToParcelUUID(TX_POWER_SERVICE_UUID);
        if (srvUUID.toString().equals(txSrvUuid.toString())) {
            Log.d(TAG,
                  "Currently discovered TX service..Reading the TX power level");
            readUpdatedCharValue(txCharUuid, txSrvUuid);
        }
    }

    private boolean writeCharacteristic(ParcelUuid charUUID,
                                        ParcelUuid srvUUID, byte[] data,
                                        boolean writeWithResponse) {
        String objPath = mDevice.uuidObjPathMap.get(srvUUID + ":" + charUUID);
        BluetoothGattService gattService = mDevice.uuidGattSrvMap.get(srvUUID);
        Boolean result;

        if ((objPath == null) || (gattService == null)) {
            Log.e(TAG, "Object is null objPath : " + objPath + " gattService: "
                  + gattService);
            return false;
        }

        Log.d(TAG, "Writing characterisitcs with uuid : " + charUUID
              + " and objPath : " + objPath + "write response : " + writeWithResponse);
        for (int i = 0; i < data.length; i++) {
            Log.d(TAG, "data : " + Integer.toHexString(0xFF & data[i]));
        }
        try {
            result = gattService.writeCharacteristicRaw(objPath, data, writeWithResponse);
            Log.d(TAG, "gattService.writeCharacteristicRaw : " + result);
        } catch (Exception e) {
            result = false;
            e.printStackTrace();
        }
        return result;
    }

    private String readCharacteristic(ParcelUuid charUUID, ParcelUuid srvUUID) {
        String objPath = mDevice.uuidObjPathMap.get(srvUUID + ":" + charUUID);
        BluetoothGattService gattService = mDevice.uuidGattSrvMap.get(srvUUID);
        if ((objPath == null) || (gattService == null)) {
            Log.e(TAG, "Object is null objPath : " + objPath + " gattService: "
                  + gattService);
            return null;
        }
        Log.d(TAG, "Reading characterisitcs with uuid : " + charUUID
              + " and objPath : " + objPath);
        byte[] rawValue = gattService.readCharacteristicRaw(objPath);
        Log.d(TAG, "Raw characteristic byte arr length : " + rawValue.length);
        Log.d(TAG, "Raw characteristic value : " + rawValue);
        for (int i = 0; i < rawValue.length; i++) {
            Log.d(TAG, "Byte array at i = " + i + " is : " + rawValue[i]);
        }
        String value = new String(rawValue);
        Log.d(TAG, "String characteristic value : " + value);
        return value;
    }

    private boolean writeAlertLevel(ParcelUuid uuid, ParcelUuid srvUuid,
                                    String value, boolean writeWithResp) {
        boolean result;
        int intVal;
        try {
            intVal = Integer.parseInt(value);
        } catch (Exception ex) {
            Log.e(TAG, "Invalid value for lert level");
            return false;
        }
        if ((intVal < 0) || (intVal > 2)) {
            Log.d(TAG, "Invalid Alert Value");
            return false;
        }
        byte[] valBytes = new byte[1];
        valBytes[0] = (byte) intVal;
        result = writeCharacteristic(uuid, srvUuid, valBytes, writeWithResp);
        return result;
    }

    private int convertAtHexToDec(Integer start, int size, String input) {
        int end = start + size;
        int result = Integer.parseInt(input.substring(start, end), HEX_RADIX);
        return result;
    }

    private String convertHexStrToVal(String value) {
        int indx = 0;
        if (value.equals("0") || (value.length() < SIZE_TWO)) {
            return "0";
        }
        int proxChar = convertAtHexToDec(indx, SIZE_TWO, value);
        Log.d(TAG, "Proximity Char after cnversion to int : " + proxChar);
        String charStr = String.valueOf(proxChar);
        Log.d(TAG, "Proximity Charin str : " + charStr);
        return charStr;
    }

    private String readProximityChar(ParcelUuid charUUID, ParcelUuid srvUuid) {
        String result = null;
        try {
            String value = readCharacteristic(charUUID, srvUuid);
            Log.d(TAG, "Proximity char value before conversion : " + value);
            result = convertHexStrToVal(value);
        } catch (Exception e) {
            Log.e(TAG, "Exception while readProximityChar : " + e.getMessage());
        }

        if (srvUuid.toString().equals(
                                     convertStrToParcelUUID(LINK_LOSS_SERVICE_UUID).toString())) {

            mDevice.linkLossAlertLevel = Integer.parseInt(result);
            Log.d(TAG, "populate link loss alert level : "
                  + mDevice.linkLossAlertLevel);
        }
        return result;
    }

    private String readTXPower(ParcelUuid charUUID, ParcelUuid srvUUID) {
        String result = readProximityChar(charUUID, srvUUID);
        Log.d(TAG, "after readProximityChar in readTXPower: " + result);
        byte txPower = (byte) Integer.parseInt(result);
        mDevice.txPower = txPower;
        Log.d(TAG, "TX power after converting to byte : " + mDevice.txPower);
        return String.valueOf(txPower);
    }

    private boolean updateCharacteristic(ParcelUuid charUUID, ParcelUuid srvUUID) {
        boolean result = false;
        String objPath = mDevice.uuidObjPathMap.get(srvUUID + ":" + charUUID);
        Log.d(TAG, "updateCharacteristic : " + objPath);
        BluetoothGattService gattService = mDevice.uuidGattSrvMap.get(srvUUID);
        Log.d(TAG, "gattService : " + gattService);

        if ((objPath == null) || (gattService == null)) {
            Log.e(TAG, "Object is null objPath : " + objPath + " gattService: "
                  + gattService);
            return false;
        }

        Log.d(TAG, "Updating characterisitcs with uuid : " + charUUID
              + " and objPath : " + objPath);
        try {
            if (gattService != null) {
                result = gattService.updateCharacteristicValue(objPath);
            }
        } catch (Exception e) {
            result = false;
            Log.e(TAG, "Error while updateCharacteristicValue : " + result);
            e.printStackTrace();
        }
        return result;
    }

    private UUID convertUUIDStringToUUID(String UUIDStr) {
        if (UUIDStr.length() != 32) {
            return null;
        }
        String uuidMsB = UUIDStr.substring(0, 16);
        String uuidLsB = UUIDStr.substring(16, 32);

        if (uuidLsB.equals("800000805f9b34fb")) {
            UUID uuid = new UUID(Long.valueOf(uuidMsB, 16), 0x800000805f9b34fbL);
            return uuid;
        } else {
            UUID uuid = new UUID(Long.valueOf(uuidMsB, 16),
                                 Long.valueOf(uuidLsB));
            return uuid;
        }
    }

    class reconnectTask extends TimerTask {
        @Override
        public void run() {
            Log.d(TAG, "Inside reconnect task");
            ParcelUuid srvUuid = convertStrToParcelUUID(LINK_LOSS_SERVICE_UUID);
            //try to reconnect after 30 sec with less agressive value and infinite timeout.
            gattConnect(srvUuid, PROHIBIT_REMOTE_CHG, FILTER_POLICY, SCAN_INTERVAL, SCAN_WINDOW,
                        CONNECTION_INTERVAL_MIN, CONNECTION_INTERVAL_MAX, LATENCY,
                        SUPERVISION_TIMEOUT, MIN_CE_LEN, MAX_CE_LEN,
                        CONNECTION_ATTEMPT_INFINITE_TIMEOUT);
        }
    };
}
