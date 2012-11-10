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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.UUID;

import android.app.Activity;
import android.bluetooth.IBluetoothLEProximityServices;
import android.bluetooth.IBluetoothThermometerCallBack;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.text.Editable;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.Toast;

public class LEProximityServicesScreen extends Activity {

    public final static String TAG = "LEProximityServicesScreen";

    public static HashMap<String, ParcelUuid> charUUIDMap;

    public static HashMap<ParcelUuid, String> UUIDNameMap;

    private IBluetoothLEProximityServices proximityService = null;

    private String currentServiceName = null;

    private static Button buttonClearNotify;

    private static Button buttonRead;

    private static Button buttonRssi;

    private static Button buttonPathLoss;

    private static Button buttonWrite;

    private static EditText readText;

    private static EditText writeValueText;

    private static EditText statusText;

    private static EditText rssiText;

    private static Spinner spinner = null;

    public static boolean linkLossServiceReady = false;

    public static boolean immAlertServiceReady = false;

    public static boolean txPowerServiceReady = false;

    private static ParcelUuid selectedCharUUID;

    private static ParcelUuid selectedSrvUUID;

    public static Context myContext = null;

    private final static int UPDATE_STATUS = 0;

    private final static int UPDATE_READ = 1;

    private final static int SERVICE_READY = 2;

    private final static int SERVICE_CHANGE = 3;

    private final static int UPDATE_CONNECTED = 4;

    private final static int ABOVE_PATH_LOSS_THRESH = 6;

    private final static int BELOW_PATH_LOSS_THRESH = 7;

    private final static int UPDATE_DISCONNECTED = 5;

    private static long count = 0;

    private final static String ARG = "arg";

    public static final String INTERMEDIATE_TEMPERATURE_UUID = "00002a1e00001000800000805f9b34fb";

    public static final String PROXIMITY_SERVICE_OPERATION = "PROXIMITY_SERVICE_OPERATION";

    public static final String PROXIMITY_SERVICE_OP_SERVICE_READY = "PROXIMITY_SERVICE_OP_SERVICE_READY";

    public static final String PROXIMITY_SERVICE_DISCOVER_PRIMARY = "PROXIMITY_SERVICE_DISCOVER_PRIMARY";

    public static final String PROXIMITY_SERVICE_OP_READ_VALUE = "PROXIMITY_SERVICE_OP_READ";

    public static final String PROXIMITY_SERVICE_OP_STATUS = "PROXIMITY_SERVICE_OP_STATUS";

    public static final String PROXIMITY_SERVICE_OP_VALUE = "PROXIMITY_SERVICE_OP_VALUE";

    public static final String PROXIMITY_SERVICE_OP_PATH_LOSS_EXCEEDED = "PROXIMITY_SERVICE_OP_PATH_LOSS_EXCEEDED";

    public static final String PROXIMITY_SERVICE_CHANGE = "PROXIMITY_SERVICE_CHANGE";

    public static final String PROXIMITY_SERVICE_OP_WRITE_VALUE = "PROXIMITY_SERVICE_OP_WRITE_VALUE";

    public static final String PROXIMITY_SERVICE_OP_REGISTER_NOTIFY_INDICATE = "PROXIMITY_SERVICE_OP_REGISTER_NOTIFY_INDICATE";

    public static final String PROXIMITY_SERVICE_CHAR_UUID = "PROXIMITY_SERVICE_CHAR_UUID";

    public static final String PROXIMITY_SERVICE_NOTIFICATION_INDICATION_VALUE = "PROXIMITY_SERVICE_NOTIFICATION_INDICATION_VALUE";

    public static final String PROXIMITY_SERVICE_OP_DEV_DISCONNECTED = "PROXIMITY_SERVICE_OP_DEV_DISCONNECTED";

    public static final String PROXIMITY_SERVICE_OP_DEV_CONNECTED = "PROXIMITY_SERVICE_OP_DEV_CONNECTED";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.proximitycharscreen);
        myContext = getApplicationContext();

        Log.d(TAG, "In the Proximity services screen");
        proximityService = LEProximityClient.proximityService;

        readText = (EditText) findViewById(R.id.readText);
        buttonRead = (Button) findViewById(R.id.readButton);
        buttonRead.setOnClickListener(new View.OnClickListener() {
                                          public void onClick(View v) {
                                              Log.d(TAG, "buttonRead clicked");
                                              try {
                                                  proximityService.readCharacteristicsValue(selectedCharUUID,
                                                                                            selectedSrvUUID);
                                              } catch (RemoteException e) {
                                                  Log.e(TAG, " Character could not be read" + e.getMessage());
                                              }
                                          }
                                      });

        writeValueText = (EditText) findViewById(R.id.writeValueText);
        buttonWrite = (Button) findViewById(R.id.writeButton);
        buttonWrite.setOnClickListener(new View.OnClickListener() {
                                           public void onClick(View v) {
                                               Log.d(TAG, "buttonWrite clicked");
                                               try {
                                                   String val = null;
                                                   Editable edText = writeValueText.getText();
                                                   if (edText != null) {
                                                       val = edText.toString();
                                                       Log.d(TAG, "Value to write : " + val);
                                                   }
                                                   if (val != null) {
                                                       Log.d(TAG, "Calling thermo service write for uuid : "
                                                             + selectedCharUUID + " value : " + val
                                                             + " for service : " + selectedSrvUUID);
                                                       boolean result = proximityService
                                                       .writeCharacteristicsValue(selectedCharUUID,
                                                                                  selectedSrvUUID,
                                                                                  val);
                                                       if (!result) {
                                                           Toast.makeText(myContext, "write failed",
                                                                          Toast.LENGTH_SHORT).show();
                                                       }
                                                   }

                                               } catch (RemoteException e) {
                                                   Log.e(TAG, " Character could not be writen");
                                               }

                                           }
                                       });

        rssiText = (EditText) findViewById(R.id.rssiText);
        buttonRssi = (Button) findViewById(R.id.rssiButton);
        buttonRssi.setOnClickListener(new View.OnClickListener() {
                                          public void onClick(View v) {
                                              Log.d(TAG, "buttonRssi clicked");
                                              String val;
                                              Editable edText = rssiText.getText();
                                              if (edText != null) {
                                                  val = edText.toString();
                                                  Log.d(TAG, "Value to write : " + val);
                                                  Log.d(TAG, "Register for RSSI update with path loss threshold : "
                                                        + val);
                                                  try {
                                                      proximityService.registerRssiUpdates(LEProximityClient.RemoteDevice, Integer.valueOf(val), 2000);
                                                  } catch (RemoteException e) {
                                                      e.printStackTrace();
                                                  }
                                              } else {
                                                  Log.e(TAG, "Entered RSSI value is invalid");
                                              }

                                          }
                                      });

        buttonPathLoss = (Button) findViewById(R.id.pathLossButton);
        buttonPathLoss.setOnClickListener(new View.OnClickListener() {
                                              public void onClick(View v) {
                                                  Log.d(TAG, "unregister clicked");
                                                  try {
                                                      Log.d(TAG, "Calling unregisterRssiUpdateWatcher");
                                                      proximityService.unregisterRssiUpdates(LEProximityClient.RemoteDevice);
                                                  } catch (RemoteException e) {
                                                      e.printStackTrace();
                                                  }

                                              }
                                          });

        Intent intent = getIntent();
        currentServiceName = intent
                             .getStringExtra(LEProximityClient.GATT_SERVICE_NAME);
        Log.d(TAG, "Service Name : " + currentServiceName);
        Log.d(TAG, "Link Loss Service ready : " + linkLossServiceReady);
        Log.d(TAG, "Immediate Alert service ready : " + immAlertServiceReady);
        Log.d(TAG, "Tx Power service ready : " + txPowerServiceReady);

        spinner = (Spinner) findViewById(R.id.spinner);
        ArrayAdapter<CharSequence> adapter = null;
        if (currentServiceName
            .equals(LEProximityClient.GATT_SERVICE_LINK_LOSS_SERVICE)) {
            Log.d(TAG, "Add resources for  : "
                  + "GATT_SERVICE_LINK_LOSS_SERVICE");
            selectedSrvUUID = new ParcelUuid(
                                            convertUUIDStringToUUID(LEProximityClient.StringServicesUUID[0]));
            adapter = ArrayAdapter.createFromResource(this,
                                                      R.array.characteristics_array,
                                                      android.R.layout.simple_spinner_item);
            if (!linkLossServiceReady) {
                disableButtons();
            }
            buttonRssi.setEnabled(false);
            buttonRssi.setClickable(false);
            buttonPathLoss.setEnabled(false);
            buttonPathLoss.setClickable(false);
        } else if (currentServiceName
                   .equals(LEProximityClient.GATT_SERVICE_IMMEDIATE_ALERT_SERVICE)) {
            Log.d(TAG, "Add resources for  : "
                  + "GATT_SERVICE_IMMEDIATE_ALERT_SERVICE");
            selectedSrvUUID = new ParcelUuid(
                                            convertUUIDStringToUUID(LEProximityClient.StringServicesUUID[1]));
            adapter = ArrayAdapter.createFromResource(this,
                                                      R.array.imm_alert_characteristics_array,
                                                      android.R.layout.simple_spinner_item);
            if (!immAlertServiceReady) {
                disableButtons();
            }
        } else if (currentServiceName
                   .equals(LEProximityClient.GATT_SERVICE_TX_POWER_SERVICE)) {
            Log.d(TAG, "Add resources for  : "
                  + "GATT_SERVICE_TX_POWER_SERVICE");
            selectedSrvUUID = new ParcelUuid(
                                            convertUUIDStringToUUID(LEProximityClient.StringServicesUUID[2]));
            adapter = ArrayAdapter.createFromResource(this,
                                                      R.array.tx_power_characteristics_array,
                                                      android.R.layout.simple_spinner_item);
            if (!txPowerServiceReady) {
                disableButtons();
            }
            buttonRssi.setEnabled(false);
            buttonRssi.setClickable(false);
            buttonPathLoss.setEnabled(false);
            buttonPathLoss.setClickable(false);
        }

        charUUIDMap = new HashMap<String, ParcelUuid>();
        UUIDNameMap = new HashMap<ParcelUuid, String>();
        populateCharUUIDMap();
        populateUUIDNameMap();
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);
        spinner.setOnItemSelectedListener(new OnItemSelectedListener() {


                                              public void onItemSelected(AdapterView<?> parentView,
                                                                         View selectedItemView, int position, long id) {
                                                  Log.d(TAG, "Selected an item in the list");
                                                  String selectedChar = parentView
                                                  .getItemAtPosition(position).toString();
                                                  Log.d(TAG, "selected character : " + selectedChar);
                                                  selectedCharUUID = charUUIDMap.get(selectedChar);
                                                  Log.d(TAG, "selected ParcelUUID : " + selectedCharUUID);

                                              }

                                              public void onNothingSelected(AdapterView<?> parentView) {

                                              }

                                          });
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.servicescreenmenu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle item selection
        switch (item.getItemId()) {
        case R.id.closeService:
            closeGattService();
            return true;
        default:
            return super.onOptionsItemSelected(item);
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        clearUI();
    }

    @Override
    public void onStop() {
        super.onStop();
        clearUI();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        clearUI();
    }

    private void closeGattService() {
        if (currentServiceName
            .equals(LEProximityClient.GATT_SERVICE_LINK_LOSS_SERVICE)) {
            try {
                proximityService
                .closeProximityService(
                                      LEProximityClient.RemoteDevice,
                                      new ParcelUuid(
                                                    convertUUIDStringToUUID(LEProximityClient.StringServicesUUID[0])));
                linkLossServiceReady = false;
            } catch (RemoteException e) {
                Log.e(TAG, "Error while closing the service : "
                      + LEProximityClient.StringServicesUUID[0]);

                e.printStackTrace();
            }
        } else if (currentServiceName
                   .equals(LEProximityClient.GATT_SERVICE_IMMEDIATE_ALERT_SERVICE)) {
            try {
                proximityService
                .closeProximityService(
                                      LEProximityClient.RemoteDevice,
                                      new ParcelUuid(
                                                    convertUUIDStringToUUID(LEProximityClient.StringServicesUUID[1])));
                immAlertServiceReady = false;
            } catch (RemoteException e) {
                Log.e(TAG, "Error while closing the service : "
                      + LEProximityClient.StringServicesUUID[1]);

                e.printStackTrace();
            }
        } else if (currentServiceName
                   .equals(LEProximityClient.GATT_SERVICE_TX_POWER_SERVICE)) {
            try {
                proximityService
                .closeProximityService(
                                      LEProximityClient.RemoteDevice,
                                      new ParcelUuid(
                                                    convertUUIDStringToUUID(LEProximityClient.StringServicesUUID[2])));
                txPowerServiceReady = false;
            } catch (RemoteException e) {
                Log.e(TAG, "Error while closing the service : "
                      + LEProximityClient.StringServicesUUID[2]);

                e.printStackTrace();
            }
        }
    }


    private void clearUI() {
        // statusText.setText("");
        readText.setText("");
        writeValueText.setText("");
    }

    private static void enableButtons() {
        buttonWrite.setEnabled(true);
        buttonWrite.setClickable(true);
        buttonRead.setEnabled(true);
        buttonRead.setClickable(true);
        /*buttonNotify.setEnabled(true);
        buttonNotify.setClickable(true);
        buttonClearNotify.setEnabled(true);
        buttonClearNotify.setClickable(true);*/
    }

    private static void disableButtons() {
        buttonWrite.setEnabled(false);
        buttonWrite.setClickable(false);
        buttonRead.setEnabled(false);
        buttonRead.setClickable(false);
        /*buttonNotify.setEnabled(false);
        buttonNotify.setClickable(false);
        buttonClearNotify.setEnabled(false);
        buttonClearNotify.setClickable(false);*/
    }

    public static final Handler msgHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            Log.d(TAG, "Inside handleMessage");
            Toast toast;
            switch (msg.what) {
            case UPDATE_STATUS:
                Log.d(TAG, "UPDATE_STATUS_TEXT");
                String status = msg.getData().getString(ARG);
                Log.d(TAG, "!!!!! Status : " + status);
                // statusText.setText(status);
                break;
            case UPDATE_READ:
                Log.d(TAG, "UPDATE_READ_TEXT");
                String read = msg.getData().getString(ARG);
                readText.setText(read);
                break;
            case UPDATE_DISCONNECTED:
                Log.d(TAG, "UPDATE_DISCONNECTED");
                String disconnData = msg.getData().getString(ARG);
                String[] dVals = disconnData.split(" ");
                String dAddr = dVals[0];
                String dAlert = dVals[1];
                String toastStr = "Device : " + dAddr + "disconnected";
                Toast.makeText(myContext, toastStr, 5000).show();
                break;
            case UPDATE_CONNECTED:
                Log.d(TAG, "UPDATE_CONNECTED");
                String[] cVals = msg.getData().getString(ARG).split(" ");
                String cAddr = cVals[0];
                String cAlert = cVals[1];
                String tStr = "Device : " + cAddr + "connected";
                Toast.makeText(myContext, tStr, 5000).show();
                break;
            case SERVICE_READY:
                Log.d(TAG, "SERVICE_READY");
                String uuidStr = msg.getData().getString(ARG);

                if (uuidStr
                    .equals(new ParcelUuid(
                                          convertUUIDStringToUUID(LEProximityClient.StringServicesUUID[0]))
                            .toString())) {
                    linkLossServiceReady = true;
                } else if (uuidStr
                           .equals(new ParcelUuid(
                                                 convertUUIDStringToUUID(LEProximityClient.StringServicesUUID[1]))
                                   .toString())) {
                    immAlertServiceReady = true;
                    buttonRssi.setEnabled(true);
                    buttonRssi.setClickable(true);
                    buttonPathLoss.setEnabled(true);
                    buttonPathLoss.setClickable(true);
                } else if (uuidStr
                           .equals(new ParcelUuid(
                                                 convertUUIDStringToUUID(LEProximityClient.StringServicesUUID[2]))
                                   .toString())) {
                    txPowerServiceReady = true;
                }
                enableButtons();
                break;
            case SERVICE_CHANGE:
                Log.d(TAG, "SERVICE_CHANGE");
                linkLossServiceReady = false;
                immAlertServiceReady = false;
                disableButtons();
                Toast.makeText(myContext, "Service Change Recvd",
                               Toast.LENGTH_SHORT).show();
                break;
            case ABOVE_PATH_LOSS_THRESH:
                Log.d(TAG, "ABOVE_PATH_LOSS_THRESH");
                Toast.makeText(myContext, "Path loss exceeded threshold",
                               7000).show();
                break;
            case BELOW_PATH_LOSS_THRESH:
                Log.d(TAG, "BELOW_PATH_LOSS_THRESH");
                Toast.makeText(myContext, "Path loss is below threshold",
                               7000).show();
                break;
            default:
                break;
            }
        }
    };

    private void populateCharUUIDMap() {
        charUUIDMap.put("Link Loss Alert Level", new ParcelUuid(
                                                               convertUUIDStringToUUID("00002a0600001000800000805f9b34fb")));
        charUUIDMap.put("Imm Alert Level", new ParcelUuid(
                                                         convertUUIDStringToUUID("00002a0600001000800000805f9b34fb")));
        charUUIDMap.put("TX Power Level", new ParcelUuid(
                                                        convertUUIDStringToUUID("00002a0700001000800000805f9b34fb")));

    }

    private void populateUUIDNameMap() {
        UUIDNameMap.put(new ParcelUuid(
                                      convertUUIDStringToUUID("00002a0600001000800000805f9b34fb")),
                        "Link Loss Alert Level");
        UUIDNameMap.put(new ParcelUuid(
                                      convertUUIDStringToUUID("00002a0700001000800000805f9b34fb")),
                        "TX Power Level");
    }

    public static UUID convertUUIDStringToUUID(String UUIDStr) {
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

    public static IBluetoothThermometerCallBack mCallback = new IBluetoothThermometerCallBack.Stub() {

        public void sendResult(Bundle arg0) throws RemoteException {
            Log.d(TAG, "%%%%%%%%%******IRemoteCallback sendResult callback");
            ParcelUuid Uuid = arg0
                              .getParcelable(PROXIMITY_SERVICE_CHAR_UUID);
            Log.d(TAG, "%%%%%%%%% Uuid");
            String charName = UUIDNameMap.get(Uuid);
            Log.d(TAG, "Bundle arg1 : " + Uuid + " Name : " + charName);
            String op = arg0.getString(PROXIMITY_SERVICE_OPERATION);
            Log.d(TAG, "Bundle arg2 : " + op);
            boolean status = arg0.getBoolean(PROXIMITY_SERVICE_OP_STATUS);
            Log.d(TAG, "Bundle arg3 : " + status);
            ArrayList<String> values = arg0
                                       .getStringArrayList(PROXIMITY_SERVICE_OP_VALUE);
            String result = "";
            for (String value : values) {
                result = result + value + " ";
                Log.d(TAG, "Bundle arg4 : " + value);
            }

            Message msg = new Message();
            Bundle b = new Bundle();

            if (op.equals(PROXIMITY_SERVICE_OP_SERVICE_READY)) {
                Log.d(TAG, " PROXIMITY_SERVICE_OP_SERVICE_READY");
                String text = Uuid.toString();
                if (status) {
                    sendMsg(SERVICE_READY, text, msg, b);
                } else {
                    Log.e(TAG, "Error while service ready");
                }

            } else if (op.equals(PROXIMITY_SERVICE_CHANGE)) {
                /*Log.d(TAG, " PROXIMITY_SERVICE_CHANGE");
                String text = "";
                if (status) {
                    Log.d(TAG, "Sending SERVICE_CHANGE msg ");
                    sendMsg(SERVICE_CHANGE, text, msg, b);
                } else {
                    Log.d(TAG, "PROXIMITY_SERVICE_CHANGE status is false ");
                }*/

            } else if (op.equals(PROXIMITY_SERVICE_OP_READ_VALUE)) {
                Log.d(TAG, " PROXIMITY_SERVICE_OP_READ_VALUE");
                String text = charName + " read : " + status;
                if (!status) {
                    sendMsg(UPDATE_STATUS, text, msg, b);
                } else {
                    sendMsg(UPDATE_READ, result, msg, b);
                }

            } else if (op.equals(PROXIMITY_SERVICE_OP_WRITE_VALUE)) {
                Log.d(TAG, " PROXIMITY_SERVICE_OP_WRITE_VALUE");
                String text = charName + " write : " + status;
                sendMsg(UPDATE_STATUS, text, msg, b);
            } else if (op
                       .equals(PROXIMITY_SERVICE_NOTIFICATION_INDICATION_VALUE)) {
                Log.d(TAG, " PROXIMITY_SERVICE_OP_WRITE_VALUE");
                String text = charName + " : " + result;
                sendMsg(UPDATE_STATUS, text, msg, b);
            } else if (op.equals(PROXIMITY_SERVICE_OP_DEV_DISCONNECTED)) {
                Log.d(TAG, " PROXIMITY_SERVICE_OP_DEV_DISCONNECTED");
                String text = result;
                sendMsg(UPDATE_DISCONNECTED, text, msg, b);
            } else if (op.equals(PROXIMITY_SERVICE_OP_DEV_CONNECTED)) {
                Log.d(TAG, " PROXIMITY_SERVICE_OP_DEV_CONNECTED");
                String text = result;
                sendMsg(UPDATE_CONNECTED, text, msg, b);
            } else if (op.equals(PROXIMITY_SERVICE_OP_PATH_LOSS_EXCEEDED)) {
                Log.d(TAG, " PROXIMITY_SERVICE_OP_PATH_LOSS_EXCEEDED");
                String text = "";
                if (status) {
                    sendMsg(ABOVE_PATH_LOSS_THRESH, text, msg, b);
                } else {
                    sendMsg(BELOW_PATH_LOSS_THRESH, text, msg, b);
                }

            }
        }

        private void sendMsg(int msgWhat, String result, Message msg,
                             Bundle b) {
            Log.d(TAG, "Inside sendMsg");
            msg.what = msgWhat;
            b.putString(ARG, result);
            msg.setData(b);
            msgHandler.sendMessage(msg);
        }

    };

}
