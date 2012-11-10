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

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.IBluetoothGattProfile;
import android.bluetooth.IBluetoothThermometerCallBack;
import android.bluetooth.IBluetoothThermometerServices;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Parcel;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.util.Log;

import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Date;
import java.util.HashMap;
import java.util.UUID;
import java.io.UnsupportedEncodingException;

public class BluetoothThermometerServices extends Service {

    private static final short CLIENT_CONF_INDICATE_VALUE = 2;

    private static final short CLIENT_CONF_NOTIFY_VALUE = 1;

    private static final int DATE_START_INDEX = 2;

    private static final int DATE_SIZE = 7;

    private static final int OCTET_SIZE = 8;

    private static final String ISO_DATE_FORMAT = "yyyy-MM-dd HH:mm:ss";

    private static final int HEX_RADIX = 16;

    private static final int SIZE_FOUR = 4;

    private static final int SIZE_TWO = 2;

    private static final String TAG = "BluetoothThermometerServices";

    private int mStartId = -1;

    private BluetoothAdapter mAdapter;

    private IBluetoothThermometerCallBack thermometerSrvCallBack = null;

    private boolean mHasStarted = false;

    public static ParcelUuid GATTServiceUUID = null;

    public static final String USER_DEFINED = "UserDefined";

    public static BluetoothThermometerDevice mDevice;

    public static int THERMOMETER_HEALTH_SERVICE = 0;

    public static int THERMOMETER_DEVICE_SERVICE = 1;

    public static int THERMOMETER_BATTERY_SERVICE = 2;

    public static final int ERROR = Integer.MIN_VALUE;

    public static String[] characteristicsPath = null;

    public static ParcelUuid[] uuidArray = null;

    static final String THERMOMETER_SERVICE_WAKEUP = "android.bluetooth.le.action.THERMOMETER_SERVICE_WAKEUP";

    static final String THERMOMETER_DEVICE = "android.bluetooth.le.action.THERMOMETER_DEVICE";

    static final String THERMOMETER_DEVICE_SERVICE_ON = "android.bluetooth.le.action.THERMOMETER_DEVICE_SERVICE_ON";

    static final String THERMOMETER_BATTERY_SERVICE_ON = "android.bluetooth.le.action.THERMOMETER_BATTERY_SERVICE_ON";

    protected static final int GATT_SERVICE_STARTED = 0;

    public static final String ACTION_GATT_SERVICE_EXTRA = "ACTION_GATT_SERVICE_EXTRA";

    public static final int GATT_SERVICE_STARTED_UUID = 0;

    public static final int GATT_SERVICE_STARTED_OBJ = 1;

    public static final String ACTION_GATT_SERVICE_EXTRA_DEVICE = "ACTION_GATT_SERVICE_EXTRA_DEVICE";

    public static final String ACTION_GATT_SERVICE_EXTRA_OBJ = "ACTION_GATT_SERVICE_EXTRA_OBJ";

    public static final int TEMP_MSR_INTR_MIN = 1;

    public static final int TEMP_MSR_INTR_MAX = 65535;

    public static final String HEALTH_THERMOMETER_SERVICE_UUID = "0000180900001000800000805f9b34fb";

    public static final String DEVICE_INFORMATION_SERVICE_UUID = "0000180a00001000800000805f9b34fb";

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

    public static final String THERMOMETER_SERVICE_OPERATION = "THERMOMETER_SERVICE_OPERATION";

    public static final String THERMOMETER_SERVICE_OP_SERVICE_READY = "THERMOMETER_SERVICE_OP_SERVICE_READY";

    public static final String THERMOMETER_SERVICE_OP_READ_VALUE = "THERMOMETER_SERVICE_OP_READ";

    public static final String THERMOMETER_SERVICE_OP_STATUS = "THERMOMETER_SERVICE_OP_STATUS";

    public static final String THERMOMETER_SERVICE_OP_VALUE = "THERMOMETER_SERVICE_OP_VALUE";

    public static final String THERMOMETER_SERVICE_OP_WRITE_VALUE = "THERMOMETER_SERVICE_OP_WRITE_VALUE";

    public static final String THERMOMETER_SERVICE_OP_REGISTER_NOTIFY_INDICATE = "THERMOMETER_SERVICE_OP_REGISTER_NOTIFY_INDICATE";

    public static final String THERMOMETER_SERVICE_CHAR_UUID = "THERMOMETER_SERVICE_CHAR_UUID";

    public static final String THERMOMETER_SERVICE_NOTIFICATION_INDICATION_VALUE = "THERMOMETER_SERVICE_NOTIFICATION_INDICATION_VALUE";

    public static enum TempType {
        ARMPIT, BODY, EAR, FINGER, GASTRO, MOUTH, RECT, TOE, TYMPHANUM
    }

    public static HashMap<Integer, String> tempTypeMap = null;

    private IntentFilter inFilter = null;

    private BluetoothThermometerReceiver receiver = null;

    public final Handler msgHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
            case GATT_SERVICE_STARTED_UUID:
                Log.d(TAG, "Received GATT_SERVICE_STARTED_UUID message");
                break;
            case GATT_SERVICE_STARTED_OBJ:
                Log.d(TAG, "Received GATT_SERVICE_STARTED_OBJ message");
                ArrayList<String> gattDataList = msg.getData().getStringArrayList(
                    ACTION_GATT_SERVICE_EXTRA_OBJ);
                int size = gattDataList.size();
                Log.d(TAG, "GATT Service data list len : "
                      + size);
                String selectedServiceObjPath = gattDataList.get(0);
                Log.d(TAG, "GATT Service path array obj : "
                      + selectedServiceObjPath);
                String uuidStr = gattDataList.get(size-1);
                Log.d(TAG, "GATT Service uuidStr : "
                      + uuidStr);
                ParcelUuid selectedUUID = ParcelUuid.fromString(uuidStr);
                Log.d(TAG, "ParcelUUID rep of selectedUUID : " + selectedUUID);

                if(isThermometerProfileService(selectedUUID)) {
                    Log.d(TAG, "Proceed to creating thermometer profile gatt service");
                    mDevice.uuidObjPathMap.put(selectedUUID, selectedServiceObjPath);
                    mDevice.objPathUuidMap.put(selectedServiceObjPath, selectedUUID);
                    getBluetoothGattService(selectedServiceObjPath, selectedUUID);
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
        Log.d(TAG, "onCreate thermometer service");
        mAdapter = BluetoothAdapter.getDefaultAdapter();

        if (!mHasStarted) {
            mHasStarted = true;
            Log.e(TAG, "Creating thermometer service");
            int state = mAdapter.getState();
            if (state == BluetoothAdapter.STATE_ON) {
                Log.d(TAG, "Bluetooth is on");
                mDevice = new BluetoothThermometerDevice();
                mDevice.uuidObjPathMap = new HashMap<ParcelUuid, String>();
                mDevice.objPathUuidMap = new HashMap<String, ParcelUuid>();
                mDevice.uuidGattSrvMap = new HashMap<ParcelUuid, BluetoothGattService>();
                tempTypeMap = new HashMap<Integer, String>();
                populateTempType();

                Log.d(TAG, "registering receiver handler");
                BluetoothThermometerReceiver.registerHandler(msgHandler);

                inFilter = new IntentFilter();
                inFilter.addAction("android.bluetooth.device.action.GATT");
                this.receiver = new BluetoothThermometerReceiver();
                Log.d(TAG, "Registering the receiver");
                this.registerReceiver(this.receiver, inFilter);
            } else {
                Log.d(TAG, "Bluetooth is not on");
            }
        }
    }

    private void populateTempType() {
        tempTypeMap.put(1, "ARMPIT");
        tempTypeMap.put(2, "BODY");
        tempTypeMap.put(3, "EAR");
        tempTypeMap.put(4, "FINGER");
        tempTypeMap.put(5, "GASTRO");
        tempTypeMap.put(6, "MOUTH");
        tempTypeMap.put(7, "RECT");
        tempTypeMap.put(8, "TOE");
        tempTypeMap.put(9, "TYMPHANUM");
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind thermometer service");
        return mBinder;
    }

    @Override
    public void onRebind(Intent intent) {
        Log.d(TAG, "onRebind thermometer service");
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.d(TAG, "onUnbind thermometer service");
        return true;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy thermometer service");
        Log.d(TAG, "Closing the thermometer health service : " +
              closeService(convertStrToParcelUUID(HEALTH_THERMOMETER_SERVICE_UUID)));
        Log.d(TAG, "Closing the thermometer health service : " +
              closeService(convertStrToParcelUUID(DEVICE_INFORMATION_SERVICE_UUID)));
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

    private boolean closeService(ParcelUuid srvUuid) {
        if (mDevice != null) {
            BluetoothGattService gattService = mDevice.uuidGattSrvMap.get(srvUuid);
            if (gattService != null) {
                try {
                    Log.d(TAG, "Calling gattService.close()");
                    gattService.close();
                    Log.d(TAG, "removing Gatt service for UUID : " + srvUuid);
                    mDevice.uuidGattSrvMap.remove(srvUuid);
                } catch (Exception e) {
                    Log.e(TAG, "************Error while closing the Gatt Service");
                    e.printStackTrace();
                    return false;
                }
            }
        }
        return true;
    }

    private final IBluetoothGattProfile.Stub btGattCallback = new IBluetoothGattProfile.Stub() {
        public void onDiscoverCharacteristicsResult(String path, boolean result) {
            Log.d(TAG, "onDiscoverCharacteristicsResult : " +
                  "path : " + path + "result : " + result);
            ParcelUuid srvUUID = mDevice.objPathUuidMap.get(path);
            BluetoothGattService gattService = mDevice.uuidGattSrvMap.get(srvUUID);
            if (result) {
                if (gattService != null) {
                    Log.d(TAG, "gattService.getServiceUuid() ======= "
                          + srvUUID.toString());
                    try {
                        registerForWatcher(srvUUID);
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
            ParcelUuid uuid = mDevice.objPathUuidMap.get(path);

            Log.d(TAG, "callback onSetCharacteristicValueResult: ");
            Log.d(TAG, "result : " + result);

            bundleAndSendResult(
                               uuid,
                               BluetoothThermometerServices.THERMOMETER_SERVICE_OP_WRITE_VALUE,
                               result, new ArrayList<String>());
        }

        public void onSetCharacteristicCliConfResult(String path, boolean result) {
            ParcelUuid uuid = mDevice.objPathUuidMap.get(path);

            Log.d(TAG, "callback onSetCharacteristicCliConfResult: ");
            Log.d(TAG, "result : " + result);
            bundleAndSendResult(
                               uuid,
                               BluetoothThermometerServices.THERMOMETER_SERVICE_OP_REGISTER_NOTIFY_INDICATE,
                               result, new ArrayList<String>());
        }

        public void onValueChanged(String path, String value) {
            ParcelUuid uuid = mDevice.objPathUuidMap.get(path);
            Log.d(TAG, "callback onValueChanged: " + uuid.toString());
            Log.d(TAG, "value : " + value);

            if (convertStrToParcelUUID(MEASUREMENT_INTERVAL_UUID).toString()
                .equals(uuid.toString())) {
                Log.d(TAG, "onValueChanged FOR MEASUREMENT_INTERVAL_UUID");
                String msrIntStr = convertLittleEnHexStrToVal(value);
                bundleAndSendResult(
                                   uuid,
                                   BluetoothThermometerServices.THERMOMETER_SERVICE_NOTIFICATION_INDICATION_VALUE,
                                   true, new ArrayList<String>(Arrays.asList(msrIntStr)));

            } else if (convertStrToParcelUUID(TEMPERATURE_MEASUREMENT_UUID)
                       .toString().equals(uuid.toString())
                       || convertStrToParcelUUID(INTERMEDIATE_TEMPERATURE_UUID)
                       .toString().equals(uuid.toString())) {
                Log.d(TAG, "onValueChanged FOR TEMPERATURE");
                ArrayList<String> values = parseTempMeasurement(value);
                Log.d(TAG, "value list size : " + values.size());
                bundleAndSendResult(
                                   uuid,
                                   BluetoothThermometerServices.THERMOMETER_SERVICE_NOTIFICATION_INDICATION_VALUE,
                                   true, values);
            }
        }

        public void onUpdateCharacteristicValueResult(String arg0, boolean arg1) {
            Log.d(TAG, "callback onUpdateCharacteristicValueResult: ");
            Log.d(TAG, "arg0 : " + arg0);
            Log.d(TAG, "arg1 : " + arg1);
            ParcelUuid uuid = mDevice.objPathUuidMap.get(arg0);
            Log.d(TAG, "uuid : " + uuid);
            if (arg1) {
                if (convertStrToParcelUUID(DATE_TIME_UUID).toString().equals(
                                                                            uuid.toString())) {
                    Log.d(TAG,
                          "calling Date Time Read Char to get updated value");
                    String value = readDateTime(uuid);
                    bundleAndSendResult(uuid,
                                        THERMOMETER_SERVICE_OP_READ_VALUE, true,
                                        new ArrayList<String>(Arrays.asList(value)));
                } else if (convertStrToParcelUUID(MEASUREMENT_INTERVAL_UUID).toString()
                           .equals(uuid.toString())) {
                    Log.d(TAG,
                          "calling Msr Interval Read Char to get updated value");
                    String value = readMeasurementInterval(uuid);
                    bundleAndSendResult(uuid,
                                        THERMOMETER_SERVICE_OP_READ_VALUE,
                                        true, new ArrayList<String>(Arrays.asList(value)));

                } else if (convertStrToParcelUUID(TEMPERATURE_TYPE_UUID).toString()
                           .equals(uuid.toString())) {
                    Log.d(TAG, "Reading the temperature type Charactersitics");
                    String value = readTemperatureType(uuid);
                    bundleAndSendResult(uuid,
                                        THERMOMETER_SERVICE_OP_READ_VALUE,
                                        true, new ArrayList<String>(Arrays.asList(value)));

                } else if ((convertStrToParcelUUID(MANUFACTURER_NAME_STRING_UUID)
                            .toString().equals(uuid.toString())) ||
                           (convertStrToParcelUUID(MODEL_NUMBER_STRING_UUID)
                            .toString().equals(uuid.toString())) ||
                           (convertStrToParcelUUID(SERIAL_NUMBER_STRING_UUID)
                            .toString().equals(uuid.toString())) ||
                           (convertStrToParcelUUID(HARDWARE_REVISION_STRING_UUID)
                            .toString().equals(uuid.toString())) ||
                           (convertStrToParcelUUID(FIRMWARE_REVISION_STRING_UUID)
                            .toString().equals(uuid.toString())) ||
                           (convertStrToParcelUUID(SOFTWARE_REVISION_STRING_UUID)
                            .toString().equals(uuid.toString())) ||
                           (convertStrToParcelUUID(SYSTEM_ID_UUID)
                            .toString().equals(uuid.toString())) ||
                           (convertStrToParcelUUID(CERTIFICATION_DATA_UUID)
                            .toString().equals(uuid.toString()))) {
                    Log.d(TAG, "Reading the Device Information UTF-8 Charactersitic");
                    String value = readDeviceInformationUtf8String(uuid);
                    bundleAndSendResult(uuid,
                                        THERMOMETER_SERVICE_OP_READ_VALUE,
                                        true, new ArrayList<String>(Arrays.asList(value)));


                } else if ((convertStrToParcelUUID(SYSTEM_ID_UUID)
                            .toString().equals(uuid.toString())) ||
                           (convertStrToParcelUUID(CERTIFICATION_DATA_UUID)
                            .toString().equals(uuid.toString()))) {
                    Log.d(TAG, "Reading the Device Information Charactersitic");
                    String value = readDeviceInformationString(uuid);
                    bundleAndSendResult(
                        uuid,
                        BluetoothThermometerServices.THERMOMETER_SERVICE_OP_READ_VALUE,
                        true, new ArrayList<String>(Arrays.asList(value)));

                    } else {
                    Log.d(TAG, "Character cannot be updated");
                }
            } else {
                Log.e(TAG, "onUpdateCharacteristicValueResult : " + arg1);
                bundleAndSendResult(uuid, THERMOMETER_SERVICE_OP_READ_VALUE,
                                    false, new ArrayList<String>());
            }
        }
    };

    private final IBluetoothThermometerServices.Stub mBinder = new IBluetoothThermometerServices.Stub() {
        public synchronized boolean startThermometerService(BluetoothDevice btDevice,
                                                            ParcelUuid uuid,
                                                            IBluetoothThermometerCallBack callBack)
        throws RemoteException {
            Log.d(TAG, "Inside startGattService: ");
            if (mDevice == null) {
                Log.e(TAG, "mDevice is null");
                return false;
            }
            if (!(mDevice.uuidGattSrvMap.containsKey(uuid))) {
                Log.d(TAG, "Creating new GATT service for UUID : " + uuid);
                thermometerSrvCallBack = callBack;
                if ((mDevice.BDevice != null) &&
                    (mDevice.BDevice.getAddress().equals(btDevice.getAddress()))) {
                    Log.d(TAG, "services have already been discovered. Create Gatt service");
                    String objPath = mDevice.uuidObjPathMap.get(uuid);
                    if (objPath != null) {
                        Log.d(TAG, "GET GATT SERVICE for : " + uuid);
                        getBluetoothGattService(objPath, uuid);
                    } else {
                        Log.d(TAG, "action GATT has not been received for uuid : " + uuid);
                        return getGattServices(uuid, btDevice);
                    }
                } else {
                    Log.d(TAG, "Primary services need to be discovered");
                    return getGattServices(uuid, btDevice);
                }

            } else {
                Log.d(TAG, "Gatt service already exists for UUID : " + uuid);
                bundleAndSendResult(
                                   uuid,
                                   BluetoothThermometerServices.THERMOMETER_SERVICE_OP_SERVICE_READY,
                                   true, new ArrayList<String>());
            }
            return true;
        }

        public synchronized String getServiceName(ParcelUuid serviceUuid) throws RemoteException {
            String srvName = "";
            /*if (mDevice.gattService != null) {
                try {
                    srvName = mDevice.gattService.getServiceName();
                } catch (Exception e) {
                    Log.e(TAG, "Error while getting the service name");
                }
            }*/
            return srvName;
        }

        public synchronized void readCharacteristicsValue(ParcelUuid uuid)
        throws RemoteException {
            Log.d(TAG,
                  "Inside BluetoothThermometerServices readCharacteristics");
            Bundle bundle = null;

            String readCharUUID = uuid.toString();

            if (mDevice.uuidObjPathMap.containsKey(uuid)) {
                if (convertStrToParcelUUID(DATE_TIME_UUID).toString().equals(
                                                                            readCharUUID)) {
                    Log.d(TAG, "Reading the Date Time Charactersitics");
                    if (!updateDateTime(uuid)) {
                        bundleAndSendResult(
                                           uuid,
                                           BluetoothThermometerServices.THERMOMETER_SERVICE_OP_READ_VALUE,
                                           false, new ArrayList<String>());
                    }
                } else if (convertStrToParcelUUID(MEASUREMENT_INTERVAL_UUID)
                           .toString().equals(readCharUUID)) {
                    Log.d(TAG, "Reading the Measurement Interval Charactersitics");
                    updateCharValue(uuid);
                } else if (convertStrToParcelUUID(TEMPERATURE_TYPE_UUID).toString()
                           .equals(readCharUUID)) {
                    Log.d(TAG, "Reading the temperature type Charactersitics");
                    String value = readTemperatureType(uuid);
                    if(value == null) {
                        Log.d(TAG, "Read Char returned null..so calling update char");
                        updateCharValue(uuid);
                    } else {
                        bundleAndSendResult(
                                           uuid,
                                           BluetoothThermometerServices.THERMOMETER_SERVICE_OP_READ_VALUE,
                                           true, new ArrayList<String>(Arrays.asList(value)));
                    }
                } else if ((convertStrToParcelUUID(MANUFACTURER_NAME_STRING_UUID)
                            .toString().equals(readCharUUID)) ||
                           (convertStrToParcelUUID(MODEL_NUMBER_STRING_UUID)
                            .toString().equals(readCharUUID)) ||
                           (convertStrToParcelUUID(SERIAL_NUMBER_STRING_UUID)
                            .toString().equals(readCharUUID)) ||
                           (convertStrToParcelUUID(HARDWARE_REVISION_STRING_UUID)
                            .toString().equals(readCharUUID)) ||
                           (convertStrToParcelUUID(FIRMWARE_REVISION_STRING_UUID)
                            .toString().equals(readCharUUID)) ||
                           (convertStrToParcelUUID(SOFTWARE_REVISION_STRING_UUID)
                            .toString().equals(readCharUUID)) ||
                           (convertStrToParcelUUID(SYSTEM_ID_UUID)
                            .toString().equals(readCharUUID)) ||
                           (convertStrToParcelUUID(CERTIFICATION_DATA_UUID)
                            .toString().equals(readCharUUID))) {
                    Log.d(TAG, "Reading the Device Information UTF-8 Charactersitic");
                    String value = readDeviceInformationUtf8String(uuid);
                    if(value == null) {
                        Log.d(TAG, "Read Char returned null..so calling update char");
                        updateCharValue(uuid);
                    } else {
                        bundleAndSendResult(
                                           uuid,
                                           BluetoothThermometerServices.THERMOMETER_SERVICE_OP_READ_VALUE,
                                           true, new ArrayList<String>(Arrays.asList(value)));
                    }

                } else if ((convertStrToParcelUUID(SYSTEM_ID_UUID)
                            .toString().equals(readCharUUID)) ||
                           (convertStrToParcelUUID(CERTIFICATION_DATA_UUID)
                            .toString().equals(readCharUUID))) {
                    Log.d(TAG, "Reading the Device Information Charactersitic");
                    String value = readDeviceInformationString(uuid);
                    if(value == null) {
                        Log.d(TAG, "Read Char returned null..so calling update char");
                        updateCharValue(uuid);
                    } else {
                        bundleAndSendResult(
                                           uuid,
                                           BluetoothThermometerServices.THERMOMETER_SERVICE_OP_READ_VALUE,
                                           true, new ArrayList<String>(Arrays.asList(value)));
                    }

                } else {
                    Log.e(TAG, "This characteristics : " + uuid + " cannot be read");
                    bundleAndSendResult(
                                       uuid,
                                       BluetoothThermometerServices.THERMOMETER_SERVICE_OP_READ_VALUE,
                                       false, new ArrayList<String>());
                }
            } else {
                Log.e(TAG, "The character parcel uuid is invalid");
                bundleAndSendResult(
                                   uuid,
                                   BluetoothThermometerServices.THERMOMETER_SERVICE_OP_READ_VALUE,
                                   false, new ArrayList<String>());
            }
        }

        public synchronized boolean writeCharacteristicsValue(ParcelUuid uuid, String value)
        throws RemoteException {
            Log.d(TAG,
                  "Inside BluetoothThermometerServices writeCharacteristics");
            boolean result = false;
            String writeCharUUID = uuid.toString();
            if (!mDevice.uuidObjPathMap.containsKey(uuid)) {
                Log.e(TAG, "The character parcel uuid is invalid");
                return false;
            }
            if (convertStrToParcelUUID(DATE_TIME_UUID).toString().equals(
                                                                        writeCharUUID)) {
                Log.d(TAG, "writing the Date Time Charactersitics");
                result = writeDateTime(uuid, value);
                Log.d(TAG, "writeDateTime : " + result);
            } else if (convertStrToParcelUUID(MEASUREMENT_INTERVAL_UUID)
                       .toString().equals(writeCharUUID)) {
                Log.d(TAG, "writing the Measurement Interval Charactersitics");
                result = writeMeasurementInterval(uuid, value);
            } else {
                Log.e(TAG, "This characteristics : " + uuid
                      + " cannot be written");
                result = false;
            }
            return result;
        }

        public synchronized int readClientCharConf(ParcelUuid uuid)
        throws RemoteException {
            Log.d(TAG, "Inside BluetoothThermometerServices readClientCharConf");
            int result = -1;
            String readCharUUID = uuid.toString();
            if (!mDevice.uuidObjPathMap.containsKey(uuid)) {
                Log.e(TAG, "The character parcel uuid is invalid");
                return result;
            }
            if (convertStrToParcelUUID(TEMPERATURE_MEASUREMENT_UUID).toString()
                .equals(readCharUUID)
                || convertStrToParcelUUID(INTERMEDIATE_TEMPERATURE_UUID)
                .toString().equals(readCharUUID)
                || convertStrToParcelUUID(MEASUREMENT_INTERVAL_UUID)
                .toString().equals(readCharUUID)) {
                Log.d(TAG, "Reading Char client conf for : " + uuid);
                result = Integer.parseInt(getClientConfDesc(uuid));
            } else {
                Log.e(TAG, "The client conf for this char : " + uuid
                      + " cannot be read");
                result = -1;
            }
            return result;
        }

        public synchronized boolean notifyIndicateValue(ParcelUuid uuid)
        throws RemoteException {
            Log.d(TAG, "Inside notifyIndicateValue");
            boolean result;
            String readCharUUID = uuid.toString();
            if (!mDevice.uuidObjPathMap.containsKey(uuid)) {
                Log.e(TAG, "The character parcel uuid is invalid");
                return false;
            }
            Log.d(TAG, "Setting Char client conf for : " + uuid);
            if (convertStrToParcelUUID(TEMPERATURE_MEASUREMENT_UUID).toString()
                .equals(readCharUUID)
                || convertStrToParcelUUID(MEASUREMENT_INTERVAL_UUID)
                .toString().equals(readCharUUID)) {
                result = setClientConfDesc(uuid, CLIENT_CONF_INDICATE_VALUE);
            } else if (convertStrToParcelUUID(INTERMEDIATE_TEMPERATURE_UUID)
                       .toString().equals(readCharUUID)) {
                result = setClientConfDesc(uuid, CLIENT_CONF_NOTIFY_VALUE);
            } else {
                Log.e(TAG,
                      " Cannot Notify or Indicate this characteristics : "
                      + uuid);
                result = false;

            }
            return result;
        }

        public synchronized boolean clearNotifyIndicate(ParcelUuid uuid)
        throws RemoteException {
            Log.d(TAG, "Inside clearNotifyIndicate");
            boolean result;
            String readCharUUID = uuid.toString();
            if (!mDevice.uuidObjPathMap.containsKey(uuid)) {
                Log.e(TAG, "The character parcel uuid is invalid");
                return false;
            }
            Log.d(TAG, "Setting Char client conf for : " + uuid);
            if (convertStrToParcelUUID(TEMPERATURE_MEASUREMENT_UUID).toString()
                .equals(readCharUUID)
                || convertStrToParcelUUID(MEASUREMENT_INTERVAL_UUID)
                .toString().equals(readCharUUID)
                || convertStrToParcelUUID(INTERMEDIATE_TEMPERATURE_UUID)
                .toString().equals(readCharUUID)) {
                result = setClientConfDesc(uuid, (short)0);
            } else {
                Log.e(TAG, " Cannot Notify/Indicate "+
                      "this char : " + uuid);
                result = false;

            }
            return result;
        }

        public synchronized boolean closeThermometerService(BluetoothDevice btDevice,
                                                            ParcelUuid srvUuid)
        throws RemoteException {
            return closeService(srvUuid);
        }
    };

    private boolean isThermometerProfileService(ParcelUuid uuid) {
        if(convertStrToParcelUUID(HEALTH_THERMOMETER_SERVICE_UUID).toString().
           equals(uuid.toString()) ||
           convertStrToParcelUUID(DEVICE_INFORMATION_SERVICE_UUID).toString().
           equals(uuid.toString())) {
            return true;
        }
        return false;
    }

    private boolean getGattServices(ParcelUuid uuid, BluetoothDevice btDevice) {
        mDevice.BDevice = btDevice;
        Log.d(TAG, "GATT Extra Bt Device : " + mDevice.BDevice);
        Log.d(TAG, "GATT UUID : " + uuid);
        Log.d(TAG, "Calling  btDevice.getGattServices");
        return btDevice.getGattServices(uuid.getUuid());
    }

    private void bundleAndSendResult(ParcelUuid uuid, String operation,
                                     boolean result, ArrayList<String> values) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(THERMOMETER_SERVICE_CHAR_UUID, uuid);
        bundle.putString(THERMOMETER_SERVICE_OPERATION, operation);
        bundle.putBoolean(THERMOMETER_SERVICE_OP_STATUS, result);
        bundle.putStringArrayList(THERMOMETER_SERVICE_OP_VALUE, values);
        try {
            thermometerSrvCallBack.sendResult(bundle);
        } catch (RemoteException e) {
            Log.e(TAG, "thermometerSrvCallBack.sendResult failed");
            e.printStackTrace();
        }
    }

    private ParcelUuid convertStrToParcelUUID(String uuidStr) {
        return new ParcelUuid(convertUUIDStringToUUID(uuidStr));
    }

    private void getBluetoothGattService(String objPath, ParcelUuid uuid) {
        if ((mDevice != null) && (mDevice.BDevice != null)) {
            Log.d(TAG, " ++++++ Creating BluetoothGattService with device = "
                  + mDevice.BDevice.getAddress() + " uuid "
                  + uuid.toString() + " objPath = "
                  + objPath);

            BluetoothGattService gattService = new BluetoothGattService(mDevice.BDevice,
                                                                        uuid,
                                                                        objPath,
                                                                        btGattCallback);

            if (gattService != null) {
                mDevice.uuidGattSrvMap.put(uuid, gattService);
                Log.d(TAG, "Adding gatt service to map for : " + uuid + "size :" +
                      mDevice.uuidGattSrvMap.size());
                                boolean isDiscovered =  gattService.isDiscoveryDone();
                Log.d(TAG, "isDiscovered returned : " + isDiscovered);
                if(isDiscovered) {
                    registerForWatcher(uuid);
                    discoverCharacteristics(uuid);
                }
            } else {
                Log.e(TAG, "Gatt service is null for UUID : " + uuid.toString());
            }
        } else {
            Log.e(TAG, " mDevice is null");
        }

    }

    private void registerForWatcher(ParcelUuid srvUUID) {
        boolean regWatchRes;
        BluetoothGattService gattService = mDevice.uuidGattSrvMap.get(srvUUID);
        try {
            regWatchRes = gattService.registerWatcher();
        } catch (Exception e) {
            regWatchRes = false;
            e.printStackTrace();
        }
        Log.d(TAG, "register watcher result : " + regWatchRes);
    }

    private void discoverCharacteristics(ParcelUuid srvUUID) {
        Log.d(TAG, "Calling gattService.getCharacteristics()");

        BluetoothGattService gattService = mDevice.uuidGattSrvMap.get(srvUUID);
        if (gattService != null) {
            String[] charObjPathArray = gattService
                                        .getCharacteristics();
            if (charObjPathArray != null) {
                Log.d(TAG, " charObjPath length " + charObjPathArray.length);
                for (String objPath : Arrays.asList(charObjPathArray)) {
                    ParcelUuid parcelUUID = gattService
                                            .getCharacteristicUuid(objPath);
                    mDevice.uuidObjPathMap.put(parcelUUID, objPath);
                    mDevice.objPathUuidMap.put(objPath, parcelUUID);
                    Log.d(TAG, " Map with key UUID : " + parcelUUID + " value : "
                          + objPath);
                }
                Log.d(TAG, "Created map with size : " + mDevice.uuidObjPathMap.size());
                bundleAndSendResult(srvUUID,
                                    BluetoothThermometerServices.THERMOMETER_SERVICE_OP_SERVICE_READY,
                                    true, new ArrayList<String>());
            } else {
                Log.e(TAG, " gattService.getCharacteristics() returned null");
            }
        } else {
            Log.e(TAG, "Gatt service is null for UUID :" + srvUUID);
        }

    }

    private String getServiceName() {
        String name = null;
        try {
            if (mDevice.gattService != null) {
                name = mDevice.gattService.getServiceName();
            } else {
                Log.e(TAG, "gattservice is null");
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return name;
    }

    private String readDateTime(ParcelUuid charUUID) {
        String result = null;
        try {
            String value = readCharacteristic(charUUID);
            Log.d(TAG, "Date Time value before conversion : " + value);
            result = convertValToDateTime(value);

        } catch (Exception e) {
            Log.e(TAG, "Exception while readDateTime : " + e.getMessage());
        }
        return result;
    }

    private String readMeasurementInterval(ParcelUuid charUUID) {
        String result = null;
        try {
            String value = readCharacteristic(charUUID);
            Log.d(TAG, "Temperature Measurement value before conversion : " + value);
            result = convertLittleEnHexStrToVal(value);

        } catch (Exception e) {
            Log.e(TAG, "Exception while readMeasurementInterval : " + e.getMessage());
        }
        return result;
    }

    private String readTemperatureType(ParcelUuid charUUID) {
        String result = null;
        try {
            String value = readCharacteristic(charUUID);
            Log.d(TAG, "Temperature Type value before conversion : " + value);
            result = convertHexStrToVal(value);
        } catch (Exception e) {
            Log.e(TAG, "Exception while readTemperatureType : " + e.getMessage());
        }
        return result;
    }

    private String readDeviceInformationUtf8String(ParcelUuid charUUID) {
        String result = null;
        String value = null;
        try {
            value = readCharacteristic(charUUID);
            Log.d(TAG, "DeviceInformation String value before conversion : " + value);
            result = convertUTF8Hex(value);
        } catch (Exception e) {
            Log.e(TAG, "Exception while readDeviceInformationUtf8String : " + e.getMessage());
        }
        return result;
    }

    private String readDeviceInformationString(ParcelUuid charUUID) {
        String value = null;
        try {
            value = readCharacteristic(charUUID);
            Log.d(TAG, "DeviceInformation String value before conversion : " + value);
        } catch (Exception e) {
            Log.e(TAG, "Exception while readDeviceInformation : " + e.getMessage());
        }
        return value;
    }

    private String readCharacteristic(ParcelUuid charUUID) {
        String objPath = mDevice.uuidObjPathMap.get(charUUID);
        BluetoothGattService gattService = getGattService(charUUID);
        if ((objPath == null) || (gattService == null)) {
            Log.e(TAG, "Object is null objPath : " + objPath +
                  " gattService: " +  gattService);
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

    private boolean updateDateTime(ParcelUuid charUUID) {
        return updateCharacteristic(charUUID);
    }

    private boolean updateCharacteristic(ParcelUuid charUUID) {
        boolean result = false;
        String objPath = mDevice.uuidObjPathMap.get(charUUID);
        BluetoothGattService gattService = getGattService(charUUID);

        if ((objPath == null) || (gattService == null)) {
            Log.e(TAG, "Object is null objPath : " + objPath +
                  " gattService: " +  gattService);
            return false;
        }

        Log.d(TAG, "Updating characterisitcs with uuid : " + charUUID
              + " and objPath : " + objPath);
        try {
            if (gattService != null) {
                result = gattService.updateCharacteristicValue(objPath);
            } else {
                Log.e(TAG, "GATTservice is null");
                result = false;
            }
        } catch (Exception e) {
            result = false;
            Log.e(TAG, "Error while updateCharacteristicValue : " + result);
            e.printStackTrace();
        }
        return result;
    }

    private String getClientConfDesc(ParcelUuid uuid) {
        String objPath = mDevice.uuidObjPathMap.get(uuid);
        BluetoothGattService gattService = getGattService(uuid);

        if ((objPath == null) || (gattService == null)) {
            Log.e(TAG, "Object is null objPath : " + objPath +
                  " gattService: " +  gattService);
            return null;
        }

        String confStr = gattService
                         .getCharacteristicClientConf(objPath);
        Log.d(TAG, "Client char conf for : " + uuid + " is : " + confStr);
        return convertLittleEnHexStrToVal(confStr);
    }

    private boolean writeMeasurementInterval(ParcelUuid uuid, String value) {
        int msrInterval = 0;
        byte[] msrIntByte = new byte[SIZE_TWO];
        try {
            msrInterval = Integer.valueOf(value);
            if ((msrInterval < TEMP_MSR_INTR_MIN)
                || (msrInterval > TEMP_MSR_INTR_MAX)) {
                Log.e(TAG, "Measurement Interval Value out of range");
                return false;
            }
            byte[] valByte = reverseByte(msrInterval, SIZE_FOUR);
            System.arraycopy(valByte, 0, msrIntByte, 0, SIZE_TWO);

        } catch (Exception e) {
            Log.e(TAG, "Exception while parsing msr interval " + e.getMessage());
            return false;
        }

        return writeCharacteristic(uuid, msrIntByte);
    }

    private boolean writeDateTime(ParcelUuid uuid, String value) {
        byte[] dateByte;
        Log.d(TAG, "Date written : " + value);
        try {
            dateByte = convertStrToDate(value);
        } catch (Exception e) {
            Log.e(TAG, "Exception while parsing date time " + e.getMessage());
            return false;
        }
        return writeCharacteristic(uuid, dateByte);
    }

    private boolean writeCharacteristic(ParcelUuid charUUID, byte[] data) {
        String objPath = mDevice.uuidObjPathMap.get(charUUID);
        BluetoothGattService gattService = getGattService(charUUID);
        Boolean result;

        if ((objPath == null) || (gattService == null)) {
            Log.e(TAG, "Object is null objPath : " + objPath +
                  " gattService: " +  gattService);
            return false;
        }

        Log.d(TAG, "Writing characterisitcs with uuid : " + charUUID
              + " and objPath : " + objPath);
        for (int i = 0; i < data.length; i++) {
            Log.d(TAG, "data : " + Integer.toHexString(0xFF & data[i]));
        }
        try {
            result = gattService.writeCharacteristicRaw(objPath, data, true);
            Log.d(TAG, "gattService.writeCharacteristicRaw : " + result);
        } catch (Exception e) {
            result = false;
            e.printStackTrace();
        }
        return result;
    }

    private Boolean setClientConfDesc(ParcelUuid uuid, short value) {
        String objPath = mDevice.uuidObjPathMap.get(uuid);
        BluetoothGattService gattService = getGattService(uuid);

        if ((objPath == null) || (gattService == null)) {
            Log.e(TAG, "Object is null objPath : " + objPath +
                  " gattService: " +  gattService);
            return false;
        }

        int confVal = ((value << 8) & 0xff00);
        Log.d(TAG, "Set Client char conf result for : " + uuid
              + " little en val : " + confVal);
        Boolean result;
        try {
            result = gattService.setCharacteristicClientConf(objPath,
                                                             confVal);
        } catch (Exception e) {
            result = false;
            e.printStackTrace();
        }
        Log.d(TAG, "Set Client char conf result for : " + uuid + " is : "
              + result);
        return result;
    }

    private BluetoothGattService getGattService(ParcelUuid charUUID) {
        BluetoothGattService gattService = null;
        Log.d(TAG, "get GATT srevice for uuid : " + charUUID);
        if ( (convertStrToParcelUUID(TEMPERATURE_MEASUREMENT_UUID)
              .toString().equals(charUUID.toString())) ||
             (convertStrToParcelUUID(TEMPERATURE_TYPE_UUID)
              .toString().equals(charUUID.toString())) ||
             (convertStrToParcelUUID(INTERMEDIATE_TEMPERATURE_UUID)
              .toString().equals(charUUID.toString())) ||
             (convertStrToParcelUUID(MEASUREMENT_INTERVAL_UUID)
              .toString().equals(charUUID.toString())) ||
             (convertStrToParcelUUID(DATE_TIME_UUID)
              .toString().equals(charUUID.toString()))) {
            Log.d(TAG, "get health thermometer service GATT service");
            ParcelUuid srvUUID = convertStrToParcelUUID(HEALTH_THERMOMETER_SERVICE_UUID);
            gattService = mDevice.uuidGattSrvMap.get(srvUUID);
        } else if ((convertStrToParcelUUID(MANUFACTURER_NAME_STRING_UUID)
                    .toString().equals(charUUID.toString())) ||
                   (convertStrToParcelUUID(MODEL_NUMBER_STRING_UUID)
                    .toString().equals(charUUID.toString())) ||
                   (convertStrToParcelUUID(SERIAL_NUMBER_STRING_UUID)
                    .toString().equals(charUUID.toString())) ||
                   (convertStrToParcelUUID(HARDWARE_REVISION_STRING_UUID)
                    .toString().equals(charUUID.toString())) ||
                   (convertStrToParcelUUID(FIRMWARE_REVISION_STRING_UUID)
                    .toString().equals(charUUID.toString())) ||
                   (convertStrToParcelUUID(SOFTWARE_REVISION_STRING_UUID)
                    .toString().equals(charUUID.toString())) ||
                   (convertStrToParcelUUID(SYSTEM_ID_UUID)
                    .toString().equals(charUUID.toString())) ||
                   (convertStrToParcelUUID(CERTIFICATION_DATA_UUID)
                    .toString().equals(charUUID.toString()))) {
            Log.d(TAG, "get device information service GATT service");
            ParcelUuid srvUUID = convertStrToParcelUUID(DEVICE_INFORMATION_SERVICE_UUID);
            gattService = mDevice.uuidGattSrvMap.get(srvUUID);
        } else {
            Log.e(TAG, "Gatt service cannot be found for the UUID :" + charUUID);
            gattService = null;
        }
        return gattService;
    }

    private static String convertUTF8Hex(String utf8Hex) {
        byte[] bytes = new BigInteger(utf8Hex, 16).toByteArray();
        String out = null;
        try {
            out = new String(bytes, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            Log.e(TAG, "Error while converting UTF-8 HEX string ");
            e.printStackTrace();
        }
        return out;
    }
    private static byte[] convertStrToDate(String dateStr) {
        SimpleDateFormat dateFormatter = new SimpleDateFormat(ISO_DATE_FORMAT);
        Calendar cal = null;
        byte[] dateByte = new byte[7];
        try {
            Date date = dateFormatter.parse(dateStr);
            cal = Calendar.getInstance();
            cal.setTime(date);
            int index = 2;
            int year = cal.get(Calendar.YEAR);
            byte[] valByte = reverseByte(year, SIZE_FOUR);
            System.arraycopy(valByte, 0, dateByte, 0, SIZE_TWO);

            dateByte[index++] = new Integer(cal.get(Calendar.MONTH) + 1)
                                .byteValue();
            dateByte[index++] = new Integer(cal.get(Calendar.DAY_OF_MONTH))
                                .byteValue();
            dateByte[index++] = new Integer(cal.get(Calendar.HOUR_OF_DAY))
                                .byteValue();
            dateByte[index++] = new Integer(cal.get(Calendar.MINUTE))
                                .byteValue();
            dateByte[index] = new Integer(cal.get(Calendar.SECOND)).byteValue();
            for (int i = 0; i < dateByte.length; i++) {
                Log.d(TAG, Integer.toHexString(0xFF & dateByte[i]) + ":");
            }
        } catch (ParseException e) {
            Log.e(TAG, "Error while parsing the date : " + "Parse Exception");
        } catch (Exception e) {
            Log.e(TAG, "Error while parsing the date : " + e.getMessage());
        }
        return dateByte;
    }

    private static byte[] reverseByte(int x, int size) {
        try {
            ByteBuffer bbuf = ByteBuffer.allocate(size);
            bbuf.order(ByteOrder.BIG_ENDIAN);
            bbuf.putInt(x);
            int bigEnVal = bbuf.getInt(0);
            Log.d(TAG, "before conversion : " + bigEnVal);
            byte[] bigByte = intToByteArray(bigEnVal);
            for (int i = 0; i < bigByte.length; i++) {
                Log.d(TAG,
                      "bigByte in hex at i : " + i + " : "
                      + Integer.toHexString(0xFF & bigByte[i]));
            }
            bbuf.order(ByteOrder.LITTLE_ENDIAN);
            int littleEnVal = bbuf.getInt(0);
            byte[] littleByte = intToByteArray(littleEnVal);
            for (int i = 0; i < littleByte.length; i++) {
                Log.d(TAG,
                      "littleByte in hex at i : " + i + " : "
                      + Integer.toHexString(0xFF & littleByte[i]));
            }
            Log.d(TAG, "after conversion : " + littleEnVal);
            return littleByte;
        } catch (Exception e) {
            Log.e(TAG, "Error while parsing the date : " + e.getMessage());
        }
        return null;
    }

    private static byte[] intToByteArray(int value) {
        byte[] b = new byte[SIZE_FOUR];
        for (int i = 0; i < SIZE_FOUR; i++) {
            int offset = (b.length - 1 - i) * OCTET_SIZE;
            b[i] = (byte) ((value >>> offset) & 0xFF);
        }
        return b;
    }

    private String convertValToDateTime(String value) {
        Calendar dateTime = Calendar.getInstance();
        String[] tokens = value.split("(?<=\\G.{2})");
        Log.d(TAG, "Date Time byte array size : " + tokens.length);
        int indx = 0;
        String year = strSwap(value.substring(indx, SIZE_FOUR));
        int yr = convertAtHexToDec(indx, SIZE_FOUR, year);
        indx += SIZE_FOUR;
        int month = convertAtHexToDec(indx, SIZE_TWO, value) - 1;
        indx += SIZE_TWO;
        int day = convertAtHexToDec(indx, SIZE_TWO, value);
        indx += SIZE_TWO;
        int hours = convertAtHexToDec(indx, SIZE_TWO, value);
        indx += SIZE_TWO;
        int minutes = convertAtHexToDec(indx, SIZE_TWO, value);
        indx += SIZE_TWO;
        int seconds = convertAtHexToDec(indx, SIZE_TWO, value);
        Log.d(TAG, " Year : " + yr + " month : " + month + " day : "
              + day
              + " hours : " + hours + " min : " + minutes + " sec : "
              + seconds);
        dateTime.set(yr, month, day, hours, minutes, seconds);
        SimpleDateFormat dateFormatter = new SimpleDateFormat(ISO_DATE_FORMAT);
        String dateTimeStr = dateFormatter.format(dateTime.getTime());
        Log.d(TAG, "ISO Formatted Date Time : " + dateTimeStr);
        return dateTimeStr;
    }

    private String convertLittleEnHexStrToVal(String value) {
        int indx = 0;
        if (value.equals("0") || (value.length() < SIZE_FOUR)) {
            return "0";
        }
        String valStr = strSwap(value.substring(indx, SIZE_FOUR));
        int res = convertAtHexToDec(indx, SIZE_FOUR, valStr);
        Log.d(TAG, "Measurement Interval after cnversion to int : "
              + res);
        String resStr = String.valueOf(res);
        Log.d(TAG, "Measurement Interval after conversion to str : "
              + res);
        return resStr;
    }

    private String convertHexStrToVal(String value) {
        int indx = 0;
        if (value.equals("0") || (value.length() < SIZE_TWO)) {
            return "0";
        }
        int tempType = convertAtHexToDec(indx, SIZE_TWO, value);
        Log.d(TAG, "Temperature Type after cnversion to int : " + tempType);
        String tempTypeStr = tempTypeMap.get(tempType);
        Log.d(TAG, "Temperature Type in str : " + tempTypeStr);
        return tempTypeStr;
    }

    private int convertAtHexToDec(Integer start, int size, String input) {
        int end = start + size;
        int result = Integer.parseInt(input.substring(start, end), HEX_RADIX);
        return result;
    }

    private String strSwap(String inputStr) {
        int start = 0;
        int end = 0;
        String result = null;
        if (inputStr.length() == SIZE_FOUR) {
            String lsb = inputStr.substring(start, SIZE_TWO);
            start = (start + SIZE_TWO);
            end = start + SIZE_TWO;
            String msb = inputStr.substring(start, end);
            result = msb + lsb;
        }
        return result;
    }

    private String reverseHexStr(String hexStr) {
        int length = hexStr.length();
        String revHexStr = "";
        for (int i = (length - 2); i >= 0; i -= SIZE_TWO) {
            revHexStr = revHexStr + hexStr.substring(i, i + 2);
        }
        Log.d(TAG, "After reversing Hex String : " + revHexStr);
        return revHexStr;
    }

    private Float getIEEE754FloatFromHex(int val) {
        Float res = Float.intBitsToFloat(val);
        Log.d(TAG, "getIEEE754FloatFromHex : " + res);
        return res;
    }

   private Float getIEEE11073FloatFromHex(String hexStr) {
        int mantissa;
        Log.d(TAG, "getIEEE11073FloatFromHex : " + hexStr);

        // get signed 8 bit exponent value
        String hexExp = hexStr.substring(0, 2);
        byte exp = (byte) Integer.parseInt(hexExp, 16);
        Log.d(TAG, "exp : " + exp);

        String hexMantissa = hexStr.substring(2, 8);
        String signStr = Character.toString(hexStr.charAt(2));
        int sign = Integer.parseInt(signStr, 16) & 8;
        int tempMantissa = Integer.parseInt(hexMantissa, 16);
        Log.d(TAG, "temp Mantissa : " + tempMantissa);

        if (sign != 0) {
            Log.d(TAG, "negative matissa");
            // get signed 24 bits mantissa value
            int value = (int) Math.pow(2, 24);
            int signed24BitVal = ~(value - tempMantissa);
            mantissa = signed24BitVal;
            Log.d(TAG, "neg mantissa : " + mantissa);
        } else {
            mantissa = tempMantissa;
            Log.d(TAG, "positive mantissa : " + mantissa);
        }

        // value = 10 pow (exponent) * mantissa
        float floatValue = (float) Math.pow(10, exp) * mantissa;
        return floatValue;
    }

   private ArrayList<String> parseTempMeasurement(String value) {
       Log.d(TAG, "parseTempMeasurement " );
       ArrayList<String> list = new ArrayList<String>();
       String tempValStr = "";

       String flags = value.substring(0, 2);
       int flagVal = Integer.parseInt(flags, 16);

       String temStr = value.substring(2, 10);
       Log.d(TAG, "Temp msr str : " + temStr);

       String revHexStr = reverseHexStr(temStr);
       if(revHexStr.equals("007FFFFE") || revHexStr.equals("00800002")) {
           tempValStr = "infinity";
           list.add(tempValStr);
       } else if (revHexStr.equals("007FFFFF") ||
                  revHexStr.equals("00800000") ||
                  revHexStr.equals("00800001")) {
           tempValStr = "invalid number";
           list.add(tempValStr);
       } else {
           tempValStr = String.valueOf(getIEEE11073FloatFromHex(revHexStr));

           if ((flagVal & 0x01) == 1) {
               tempValStr = tempValStr + " F";
           }
           if ((flagVal & 0x01) == 0) {
               tempValStr = tempValStr + " C";
           }
           list.add(tempValStr);

           if ((flagVal & 0x02) == 2) {
               String date = value.substring(10, 24);
               Log.d(TAG, "Date  str : " + date);
               String dateTime = convertValToDateTime(date);
               Log.d(TAG, "Date time : " + dateTime);
               list.add(dateTime);
           }

           if ((flagVal & 0x04) == 4) {
               String tempType = value.substring(24, 26);
               Log.d(TAG, "TempType  str : " + tempType);
               Log.d(TAG, "Temp Type : " + Integer.parseInt(tempType, 16));
               list.add(tempTypeMap.get(Integer.valueOf(tempType)));
           }
           Log.d(TAG, "parseTempMeasurement : " + list.size());
       }
       return list;
   }

    private void updateCharValue(ParcelUuid uuid) {
        if (!updateCharacteristic(uuid)) {
            bundleAndSendResult(uuid,
                                BluetoothThermometerServices.THERMOMETER_SERVICE_OP_READ_VALUE,
                                false, new ArrayList<String>());
            }
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
}
