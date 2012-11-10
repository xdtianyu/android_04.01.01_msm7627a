/*
 * Copyright (C) 2006 The Android Open Source Project
 * Copyright (c) 2011-12 Code Aurora Forum. All rights reserved.
 * Not a Contribution, Apache license notifications and license are retained
 * for attribution purposes only.
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

package com.android.internal.telephony.msim;

import android.content.Intent;
import android.content.res.Resources;
import android.os.Message;
import android.provider.Telephony.Intents;
import android.telephony.MSimTelephonyManager;
import android.telephony.ServiceState;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.telephony.DataConnectionTracker;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.gsm.GSMPhone;
import com.android.internal.telephony.gsm.GsmServiceStateTracker;
import com.android.internal.telephony.MSimConstants;
import com.android.internal.telephony.msim.Subscription;
import com.android.internal.telephony.uicc.SIMRecords;
import com.android.internal.telephony.uicc.UiccCardApplication;
import com.android.internal.telephony.uicc.UiccController;

/**
 * {@hide}
 */
public final class MSimGsmServiceStateTracker extends GsmServiceStateTracker {

    static final String LOG_TAG = "GSM";
    static final boolean DBG = true;
    protected static final int EVENT_ALL_DATA_DISCONNECTED = 1001;

    public MSimGsmServiceStateTracker(GSMPhone phone) {
        super(phone);
    }

    @Override
    protected UiccCardApplication getUiccCardApplication() {
        Subscription subscriptionData = ((MSimGSMPhone)phone).getSubscriptionInfo();
        if(subscriptionData != null) {
            return  mUiccController.getUiccCardApplication(subscriptionData.slotId,
                    UiccController.APP_FAM_3GPP);
        }
        return null;
    }

    @Override
    protected void updateSpnDisplay() {
        if (mIccRecords == null) {
            return;
        }
        int rule = mIccRecords.getDisplayRule(ss.getOperatorNumeric());
        String spn = mIccRecords.getServiceProviderName();
        String plmn = ss.getOperatorAlphaLong();

        // For emergency calls only, pass the EmergencyCallsOnly string via EXTRA_PLMN
        if (mEmergencyOnly && cm.getRadioState().isOn()) {
            plmn = Resources.getSystem().
                getText(com.android.internal.R.string.emergency_calls_only).toString();
            if (DBG) log("updateSpnDisplay: emergency only and radio is on plmn='" + plmn + "'");
        }

        if (rule != curSpnRule
                || !TextUtils.equals(spn, curSpn)
                || !TextUtils.equals(plmn, curPlmn)) {
            boolean showSpn = !mEmergencyOnly && !TextUtils.isEmpty(spn)
                && (rule & SIMRecords.SPN_RULE_SHOW_SPN) == SIMRecords.SPN_RULE_SHOW_SPN
                && (ss.getState() == ServiceState.STATE_IN_SERVICE);
            boolean showPlmn = !TextUtils.isEmpty(plmn) &&
                (rule & SIMRecords.SPN_RULE_SHOW_PLMN) == SIMRecords.SPN_RULE_SHOW_PLMN;

            if (DBG) {
                log(String.format("updateSpnDisplay: changed sending intent" + " rule=" + rule +
                            " showPlmn='%b' plmn='%s' showSpn='%b' spn='%s'",
                            showPlmn, plmn, showSpn, spn));
            }
            Intent intent = new Intent(Intents.SPN_STRINGS_UPDATED_ACTION);
            intent.addFlags(Intent.FLAG_RECEIVER_REPLACE_PENDING);
            intent.putExtra(Intents.EXTRA_SHOW_SPN, showSpn);
            intent.putExtra(Intents.EXTRA_SPN, spn);
            intent.putExtra(Intents.EXTRA_SHOW_PLMN, showPlmn);
            intent.putExtra(Intents.EXTRA_PLMN, plmn);
            intent.putExtra(MSimConstants.SUBSCRIPTION_KEY, phone.getSubscription());
            phone.getContext().sendStickyBroadcast(intent);
        }

        curSpnRule = rule;
        curSpn = spn;
        curPlmn = plmn;
    }

    @Override
    public String getSystemProperty(String property, String defValue) {
        return MSimTelephonyManager.getTelephonyProperty(
                property, phone.getSubscription(), defValue);
    }

    /**
     * Clean up existing voice and data connection then turn off radio power.
     *
     * Hang up the existing voice calls to decrease call drop rate.
     */
    @Override
    public void powerOffRadioSafely(DataConnectionTracker dcTracker) {
        synchronized (this) {
            if (!mPendingRadioPowerOffAfterDataOff) {
                int dds = MSimPhoneFactory.getDataSubscription();
                // To minimize race conditions we call cleanUpAllConnections on
                // both if else paths instead of before this isDisconnected test.
                if (dcTracker.isDisconnected()
                        && (dds == phone.getSubscription()
                            || (dds != phone.getSubscription()
                                && MSimProxyManager.getInstance().isDataDisconnected(dds)))) {
                    // To minimize race conditions we do this after isDisconnected
                    dcTracker.cleanUpAllConnections(Phone.REASON_RADIO_TURNED_OFF);
                    if (DBG) log("Data disconnected, turn off radio right away.");
                    hangupAndPowerOff();
                } else {
                    dcTracker.cleanUpAllConnections(Phone.REASON_RADIO_TURNED_OFF);
                    if (dds != phone.getSubscription()
                            && !MSimProxyManager.getInstance().isDataDisconnected(dds)) {
                        if (DBG) log("Data is active on DDS.  Wait for all data disconnect");
                        // Data is not disconnected on DDS. Wait for the data disconnect complete
                        // before sending the RADIO_POWER off.
                        MSimProxyManager.getInstance().registerForAllDataDisconnected(dds, this,
                                EVENT_ALL_DATA_DISCONNECTED, null);
                        mPendingRadioPowerOffAfterDataOff = true;
                    }
                    Message msg = Message.obtain(this);
                    msg.what = EVENT_SET_RADIO_POWER_OFF;
                    msg.arg1 = ++mPendingRadioPowerOffAfterDataOffTag;
                    if (sendMessageDelayed(msg, 30000)) {
                        if (DBG) log("Wait upto 30s for data to disconnect, then turn off radio.");
                        mPendingRadioPowerOffAfterDataOff = true;
                    } else {
                        log("Cannot send delayed Msg, turn off radio right away.");
                        hangupAndPowerOff();
                        mPendingRadioPowerOffAfterDataOff = false;
                    }
                }
            }
        }
    }

    @Override
    public void handleMessage(Message msg) {
        switch (msg.what) {
            case EVENT_ALL_DATA_DISCONNECTED:
                int dds = MSimPhoneFactory.getDataSubscription();
                MSimProxyManager.getInstance().unregisterForAllDataDisconnected(dds, this);
                synchronized(this) {
                    if (mPendingRadioPowerOffAfterDataOff) {
                        if (DBG) log("EVENT_ALL_DATA_DISCONNECTED, turn radio off now.");
                        hangupAndPowerOff();
                        mPendingRadioPowerOffAfterDataOff = false;
                    } else {
                        log("EVENT_ALL_DATA_DISCONNECTED is stale");
                    }
                }
                break;

            default:
                super.handleMessage(msg);
                break;
        }
    }

    @Override
    protected void log(String s) {
        Log.d(LOG_TAG, "[MSimGsmSST] [SUB : " + phone.getSubscription() + "] " + s);
    }

    @Override
    protected void loge(String s) {
        Log.e(LOG_TAG, "[MSimGsmSST] [SUB : " + phone.getSubscription() + "] " + s);
    }
}
