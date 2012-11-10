/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.cellbroadcastreceiver;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.preference.PreferenceManager;
import android.provider.Telephony;
import android.telephony.PhoneStateListener;
import android.telephony.ServiceState;
import android.telephony.TelephonyManager;
import android.telephony.cdma.CdmaSmsCbProgramData;
import android.telephony.cdma.CdmaSmsCbProgramResults;
import android.util.Log;

import com.android.internal.telephony.ITelephony;
import com.android.internal.telephony.cdma.sms.SmsEnvelope;

import java.util.ArrayList;

public class CellBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = "CellBroadcastReceiver";
    static final boolean DBG = true;    // STOPSHIP: change to false before ship
    private ServiceStateListener mSsl = new ServiceStateListener();
    private Context mC;
    private int mSs = -1;

    @Override
    public void onReceive(Context context, Intent intent) {
        onReceiveWithPrivilege(context, intent, false);
    }

    protected void onReceiveWithPrivilege(Context context, Intent intent, boolean privileged) {
        if (DBG) log("onReceive " + intent);

        String action = intent.getAction();

        if (Intent.ACTION_BOOT_COMPLETED.equals(action)) {
            mC = context;
            if (DBG) log("Registering for ServiceState updates");
            TelephonyManager tm = (TelephonyManager)context.getSystemService(
                    Context.TELEPHONY_SERVICE);
            tm.listen(mSsl, PhoneStateListener.LISTEN_SERVICE_STATE);
        } else if (Intent.ACTION_AIRPLANE_MODE_CHANGED.equals(action)) {
            boolean airplaneModeOn = intent.getBooleanExtra("state", false);
            if (DBG) log("airplaneModeOn: " + airplaneModeOn);
            if (!airplaneModeOn) {
                startConfigService(context);
            }
        } else if (Telephony.Sms.Intents.SMS_EMERGENCY_CB_RECEIVED_ACTION.equals(action) ||
                Telephony.Sms.Intents.SMS_CB_RECEIVED_ACTION.equals(action)) {
            // If 'privileged' is false, it means that the intent was delivered to the base
            // no-permissions receiver class.  If we get an SMS_CB_RECEIVED message that way, it
            // means someone has tried to spoof the message by delivering it outside the normal
            // permission-checked route, so we just ignore it.
            if (privileged) {
                intent.setClass(context, CellBroadcastAlertService.class);
                context.startService(intent);
            } else {
                Log.e(TAG, "ignoring unprivileged action received " + action);
            }
        } else if (Telephony.Sms.Intents.SMS_SERVICE_CATEGORY_PROGRAM_DATA_RECEIVED_ACTION
                .equals(action)) {
            if (privileged) {
                String sender = intent.getStringExtra("sender");
                if (sender == null) {
                    Log.e(TAG, "SCPD intent received with no originating address");
                    return;
                }

                ArrayList<CdmaSmsCbProgramData> programData =
                        intent.getParcelableArrayListExtra("program_data");
                if (programData == null) {
                    Log.e(TAG, "SCPD intent received with no program_data");
                    return;
                }

                ArrayList<CdmaSmsCbProgramResults> results = handleCdmaSmsCbProgramData(context,
                        programData);
                Bundle extras = new Bundle();
                extras.putString("sender", sender);
                extras.putParcelableArrayList("results", results);
                setResult(Activity.RESULT_OK, null, extras);
            } else {
                Log.e(TAG, "ignoring unprivileged action received " + action);
            }
        } else {
            Log.w(TAG, "onReceive() unexpected action " + action);
        }
    }

    /**
     * Handle Service Category Program Data message and return responses.
     *
     * @param context the context to use
     * @param programDataList an array of SCPD operations
     * @return the SCP results ArrayList to send to the message center
     */
    private static ArrayList<CdmaSmsCbProgramResults> handleCdmaSmsCbProgramData(Context context,
            ArrayList<CdmaSmsCbProgramData> programDataList) {
        ArrayList<CdmaSmsCbProgramResults> results
                = new ArrayList<CdmaSmsCbProgramResults>(programDataList.size());

        for (CdmaSmsCbProgramData programData : programDataList) {
            int result;
            switch (programData.getOperation()) {
                case CdmaSmsCbProgramData.OPERATION_ADD_CATEGORY:
                    result = tryCdmaSetCategory(context, programData.getCategory(), true);
                    break;

                case CdmaSmsCbProgramData.OPERATION_DELETE_CATEGORY:
                    result = tryCdmaSetCategory(context, programData.getCategory(), false);
                    break;

                case CdmaSmsCbProgramData.OPERATION_CLEAR_CATEGORIES:
                    tryCdmaSetCategory(context,
                            SmsEnvelope.SERVICE_CATEGORY_CMAS_EXTREME_THREAT, false);
                    tryCdmaSetCategory(context,
                            SmsEnvelope.SERVICE_CATEGORY_CMAS_SEVERE_THREAT, false);
                    tryCdmaSetCategory(context,
                            SmsEnvelope.SERVICE_CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY, false);
                    tryCdmaSetCategory(context,
                            SmsEnvelope.SERVICE_CATEGORY_CMAS_TEST_MESSAGE, false);
                    result = CdmaSmsCbProgramResults.RESULT_SUCCESS;
                    break;

                default:
                    Log.e(TAG, "Ignoring unknown SCPD operation " + programData.getOperation());
                    result = CdmaSmsCbProgramResults.RESULT_UNSPECIFIED_FAILURE;
            }
            results.add(new CdmaSmsCbProgramResults(programData.getCategory(),
                    programData.getLanguage(), result));
        }

        return results;
    }

    /**
     * Enables or disables a CMAS category.
     * @param context the context to use
     * @param category the CDMA service category
     * @param enable true to enable; false to disable
     * @return the service category program result code for this request
     */
    private static int tryCdmaSetCategory(Context context, int category, boolean enable) {
        String key;
        switch (category) {
            case SmsEnvelope.SERVICE_CATEGORY_CMAS_EXTREME_THREAT:
                key = CellBroadcastSettings.KEY_ENABLE_CMAS_EXTREME_THREAT_ALERTS;
                break;

            case SmsEnvelope.SERVICE_CATEGORY_CMAS_SEVERE_THREAT:
                key = CellBroadcastSettings.KEY_ENABLE_CMAS_SEVERE_THREAT_ALERTS;
                break;

            case SmsEnvelope.SERVICE_CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY:
                key = CellBroadcastSettings.KEY_ENABLE_CMAS_AMBER_ALERTS;
                break;

            case SmsEnvelope.SERVICE_CATEGORY_CMAS_TEST_MESSAGE:
                key = CellBroadcastSettings.KEY_ENABLE_CMAS_TEST_ALERTS;
                break;

            default:
                Log.w(TAG, "SCPD category " + category + " is unknown, not setting to " + enable);
                return CdmaSmsCbProgramResults.RESULT_UNSPECIFIED_FAILURE;
        }

        SharedPreferences sharedPrefs = PreferenceManager.getDefaultSharedPreferences(context);

        // default value is opt-in for all categories except for test messages.
        boolean oldValue = sharedPrefs.getBoolean(key,
                (category != SmsEnvelope.SERVICE_CATEGORY_CMAS_TEST_MESSAGE));

        if (enable && oldValue) {
            Log.d(TAG, "SCPD category " + category + " is already enabled.");
            return CdmaSmsCbProgramResults.RESULT_CATEGORY_ALREADY_ADDED;
        } else if (!enable && !oldValue) {
            Log.d(TAG, "SCPD category " + category + " is already disabled.");
            return CdmaSmsCbProgramResults.RESULT_CATEGORY_ALREADY_DELETED;
        } else {
            Log.d(TAG, "SCPD category " + category + " is now " + enable);
            sharedPrefs.edit().putBoolean(key, enable).apply();
            return CdmaSmsCbProgramResults.RESULT_SUCCESS;
        }
    }

    /**
     * Tell {@link CellBroadcastConfigService} to enable the CB channels.
     * @param context the broadcast receiver context
     */
    static void startConfigService(Context context) {
        String action = CellBroadcastConfigService.ACTION_ENABLE_CHANNELS_GSM;
        if (phoneIsCdma()) {
            action = CellBroadcastConfigService.ACTION_ENABLE_CHANNELS_CDMA;
        }
        Intent serviceIntent = new Intent(action, null,
                context, CellBroadcastConfigService.class);
        context.startService(serviceIntent);
    }

    /**
     * @return true if the phone is a CDMA phone type
     */
    static boolean phoneIsCdma() {
        boolean isCdma = false;
        try {
            ITelephony phone = ITelephony.Stub.asInterface(ServiceManager.checkService("phone"));
            if (phone != null) {
                isCdma = (phone.getActivePhoneType() == TelephonyManager.PHONE_TYPE_CDMA);
            }
        } catch (RemoteException e) {
            Log.w(TAG, "phone.getActivePhoneType() failed", e);
        }
        return isCdma;
    }

    private class ServiceStateListener extends PhoneStateListener {
        @Override
        public void onServiceStateChanged(ServiceState ss) {
            if (ss.getState() != mSs) {
                Log.d(TAG, "Service state changed! " + ss.getState() + " Full: " + ss);
                if (ss.getState() == ServiceState.STATE_IN_SERVICE ||
                    ss.getState() == ServiceState.STATE_EMERGENCY_ONLY    ) {
                    mSs = ss.getState();
                    startConfigService(mC);
                }
            }
        }
    }

    private static void log(String msg) {
        Log.d(TAG, msg);
    }
}
