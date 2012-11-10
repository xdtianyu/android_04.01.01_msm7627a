/*
 * Copyright (C) 2008 The Android Open Source Project
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
import android.os.Message;
import android.provider.Telephony.Intents;
import android.telephony.MSimTelephonyManager;
import android.text.TextUtils;

import com.android.internal.telephony.cdma.CDMAPhone;
import com.android.internal.telephony.cdma.CdmaLteServiceStateTracker;
import com.android.internal.telephony.DataConnectionTracker;
import com.android.internal.telephony.msim.Subscription;
import com.android.internal.telephony.MSimConstants;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneBase;
import com.android.internal.telephony.uicc.UiccController;
import com.android.internal.telephony.uicc.UiccCardApplication;

import android.util.Log;

/**
 * {@hide}
 */
final class MSimCdmaLteServiceStateTracker extends CdmaLteServiceStateTracker {
    static final String LOG_TAG = "CDMA";
    protected static final int EVENT_ALL_DATA_DISCONNECTED = 1001;
    public MSimCdmaLteServiceStateTracker(MSimCDMALTEPhone phone) {
        super(phone);
    }

    @Override
    protected UiccCardApplication getUiccCardApplication() {
        Subscription subscriptionData = ((MSimCDMALTEPhone)phone).getSubscriptionInfo();
        if(subscriptionData != null) {
            return  mUiccController.getUiccCardApplication(
                    subscriptionData.slotId, UiccController.APP_FAM_3GPP2);
        }
        return null;
    }

    @Override
    protected void updateSpnDisplay() {
        // mOperatorAlphaLong contains the ERI text
        String plmn = ss.getOperatorAlphaLong();
        if (!TextUtils.equals(plmn, mCurPlmn)) {
            // Allow A blank plmn, "" to set showPlmn to true. Previously, we
            // would set showPlmn to true only if plmn was not empty, i.e. was not
            // null and not blank. But this would cause us to incorrectly display
            // "No Service". Now showPlmn is set to true for any non null string.
            boolean showPlmn = plmn != null;
            if (DBG) {
                log(String.format("updateSpnDisplay: changed sending intent" +
                            " showPlmn='%b' plmn='%s'", showPlmn, plmn));
            }
            Intent intent = new Intent(Intents.SPN_STRINGS_UPDATED_ACTION);
            intent.addFlags(Intent.FLAG_RECEIVER_REPLACE_PENDING);
            intent.putExtra(Intents.EXTRA_SHOW_SPN, false);
            intent.putExtra(Intents.EXTRA_SPN, "");
            intent.putExtra(Intents.EXTRA_SHOW_PLMN, showPlmn);
            intent.putExtra(Intents.EXTRA_PLMN, plmn);
            intent.putExtra(MSimConstants.SUBSCRIPTION_KEY, phone.getSubscription());
            phone.getContext().sendStickyBroadcast(intent);
        }

        mCurPlmn = plmn;
    }

    protected void updateCdmaSubscription() {
        cm.getCDMASubscription(obtainMessage(EVENT_POLL_STATE_CDMA_SUBSCRIPTION));
    }

    @Override
    protected String getSystemProperty(String property, String defValue) {
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
    protected DataConnectionTracker getNewGsmDataConnectionTracker(PhoneBase phone) {
        return new MSimGsmDataConnectionTracker(phone);
    }

    @Override
    protected DataConnectionTracker getNewCdmaDataConnectionTracker(CDMAPhone phone) {
        return new MSimCdmaDataConnectionTracker((MSimCDMALTEPhone)phone);
    }

    @Override
    protected void log(String s) {
        Log.d(LOG_TAG, "[MSimCdmaLteSST] [SUB : " + phone.getSubscription() + "] " + s);
    }

    @Override
    protected void loge(String s) {
        Log.e(LOG_TAG, "[MSimCdmaLteSST] [SUB : " + phone.getSubscription() + "] " + s);
    }
}
