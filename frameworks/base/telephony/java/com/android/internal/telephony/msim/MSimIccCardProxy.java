/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
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
 *
 */

package com.android.internal.telephony.msim;

import static android.Manifest.permission.READ_PHONE_STATE;
import android.app.ActivityManagerNative;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncResult;
import android.os.Message;
import android.util.Log;
import android.telephony.MSimTelephonyManager;

import com.android.internal.telephony.CommandsInterface;
import com.android.internal.telephony.MccTable;
import com.android.internal.telephony.MSimConstants;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.TelephonyIntents;
import com.android.internal.telephony.uicc.IccCardProxy;
import com.android.internal.telephony.uicc.IccRecords;
import com.android.internal.telephony.uicc.IccCardStatus.CardState;
import com.android.internal.telephony.uicc.SIMRecords;
import com.android.internal.telephony.uicc.UiccCard;
import com.android.internal.telephony.uicc.UiccController;
import com.android.internal.telephony.uicc.UiccCardApplication;

import static com.android.internal.telephony.TelephonyProperties.PROPERTY_ICC_OPERATOR_ALPHA;
import static com.android.internal.telephony.TelephonyProperties.PROPERTY_ICC_OPERATOR_NUMERIC;
import static com.android.internal.telephony.TelephonyProperties.PROPERTY_APN_SIM_OPERATOR_NUMERIC;
import static com.android.internal.telephony.TelephonyProperties.PROPERTY_ICC_OPERATOR_ISO_COUNTRY;
import static com.android.internal.telephony.TelephonyProperties.PROPERTY_SIM_STATE;

public class MSimIccCardProxy extends IccCardProxy {
    private static final String LOG_TAG = "RIL_MSimIccCardProxy";
    private static final boolean DBG = true;

    private static final int EVENT_ICC_RECORD_EVENTS = 500;
    private static final int EVENT_SUBSCRIPTION_ACTIVATED = 501;
    private static final int EVENT_SUBSCRIPTION_DEACTIVATED = 502;

    private Integer mCardIndex = null;
    private Subscription mSubscriptionData = null;

    public MSimIccCardProxy(Context context, CommandsInterface ci, int cardIndex) {
        super(context, ci);

        mCardIndex = cardIndex;

        //TODO: Card index and subscription are same???
        SubscriptionManager subMgr = SubscriptionManager.getInstance();
        subMgr.registerForSubscriptionActivated(mCardIndex,
                this, EVENT_SUBSCRIPTION_ACTIVATED, null);
        subMgr.registerForSubscriptionDeactivated(mCardIndex,
                this, EVENT_SUBSCRIPTION_DEACTIVATED, null);

        resetProperties();
        setExternalState(State.NOT_READY, false);
    }

    @Override
    public void dispose() {
        super.dispose();
        resetProperties();
    }

    @Override
    public void handleMessage(Message msg) {
        switch (msg.what) {
            case EVENT_SUBSCRIPTION_ACTIVATED:
                log("EVENT_SUBSCRIPTION_ACTIVATED");
                onSubscriptionActivated();
                break;

            case EVENT_SUBSCRIPTION_DEACTIVATED:
                log("EVENT_SUBSCRIPTION_DEACTIVATED");
                onSubscriptionDeactivated();
                break;

            case EVENT_RECORDS_LOADED:
                if (mIccRecords != null) {
                    String operator = mIccRecords.getOperatorNumeric();
                    int sub = (mSubscriptionData != null) ? mSubscriptionData.subId : 0;

                    log("operator = " + operator + " SUB = " + sub);

                    if (operator != null) {
                        MSimTelephonyManager.setTelephonyProperty(
                                PROPERTY_ICC_OPERATOR_NUMERIC, sub, operator);
                        if (mCurrentAppType == UiccController.APP_FAM_3GPP) {
                            MSimTelephonyManager.setTelephonyProperty(
                                    PROPERTY_APN_SIM_OPERATOR_NUMERIC, sub, operator);
                        }
                        String countryCode = operator.substring(0,3);
                        if (countryCode != null) {
                            MSimTelephonyManager.setTelephonyProperty(
                                    PROPERTY_ICC_OPERATOR_ISO_COUNTRY, sub,
                                    MccTable.countryCodeForMcc(Integer.parseInt(countryCode)));
                        } else {
                            loge("EVENT_RECORDS_LOADED Country code is null");
                        }
                    } else {
                        loge("EVENT_RECORDS_LOADED Operator name is null");
                    }
                }
                broadcastIccStateChangedIntent(INTENT_VALUE_ICC_LOADED, null);
                break;

            case EVENT_ICC_RECORD_EVENTS:
                if ((mCurrentAppType == UiccController.APP_FAM_3GPP) && (mIccRecords != null)) {
                    int sub = (mSubscriptionData != null) ? mSubscriptionData.subId : 0;
                    AsyncResult ar = (AsyncResult)msg.obj;
                    int eventCode = (Integer) ar.result;
                    if (eventCode == SIMRecords.EVENT_SPN) {
                        MSimTelephonyManager.setTelephonyProperty(
                                PROPERTY_ICC_OPERATOR_ALPHA, sub,
                                mIccRecords.getServiceProviderName());
                    }
                }
                break;

            default:
                super.handleMessage(msg);
        }
    }

    private void onSubscriptionActivated() {
        SubscriptionManager subMgr = SubscriptionManager.getInstance();
        mSubscriptionData = subMgr.getCurrentSubscription(mCardIndex);

        resetProperties();
        updateIccAvailability();
        updateStateProperty();
    }

    private void onSubscriptionDeactivated() {
        resetProperties();
        mSubscriptionData = null;
        updateIccAvailability();
        updateStateProperty();
    }


    @Override
    protected void updateIccAvailability() {
        UiccCard newCard = mUiccController.getUiccCard(mCardIndex);
        CardState state = CardState.CARDSTATE_ABSENT;
        UiccCardApplication newApp = null;
        IccRecords newRecords = null;
        if (newCard != null) {
            state = newCard.getCardState();
            log("Card State = " + state);
            newApp = newCard.getApplication(mCurrentAppType);
            if (newApp != null) {
                newRecords = newApp.getIccRecords();
            }
        } else {
            log("No card available");
        }

        if (mIccRecords != newRecords || mUiccApplication != newApp || mUiccCard != newCard) {
            if (DBG) log("Icc changed. Reregestering.");
            unregisterUiccCardEvents();
            mUiccCard = newCard;
            mUiccApplication = newApp;
            mIccRecords = newRecords;
            registerUiccCardEvents();
            updateActiveRecord();
        }

        updateExternalState();
    }

    void resetProperties() {
        if (mCurrentAppType == UiccController.APP_FAM_3GPP) {
            MSimTelephonyManager.setTelephonyProperty(
                    PROPERTY_APN_SIM_OPERATOR_NUMERIC, mCardIndex, "");
            MSimTelephonyManager.setTelephonyProperty(
                    PROPERTY_ICC_OPERATOR_NUMERIC, mCardIndex, "");
            MSimTelephonyManager.setTelephonyProperty(
                    PROPERTY_ICC_OPERATOR_ISO_COUNTRY, mCardIndex, "");
            MSimTelephonyManager.setTelephonyProperty(
                    PROPERTY_ICC_OPERATOR_ALPHA, mCardIndex, "");
         }
    }

    private void updateStateProperty() {
        if (mSubscriptionData != null) {
            MSimTelephonyManager.setTelephonyProperty
                (PROPERTY_SIM_STATE, mSubscriptionData.subId, getState().toString());
        }
    }

    @Override
    protected void registerUiccCardEvents() {
        super.registerUiccCardEvents();
        if (mIccRecords != null) {
            mIccRecords.registerForRecordsEvents(this, EVENT_ICC_RECORD_EVENTS, null);
        }
    }

    @Override
    protected void unregisterUiccCardEvents() {
        super.unregisterUiccCardEvents();
        if (mIccRecords != null) mIccRecords.unregisterForRecordsEvents(this);
    }

    @Override
    public void broadcastIccStateChangedIntent(String value, String reason) {
        if (mCardIndex == null) {
            loge("broadcastIccStateChangedIntent: Card Index is not set; Return!!");
            return;
        }

        int subId = mCardIndex;
        if (mQuietMode) {
            log("QuietMode: NOT Broadcasting intent ACTION_SIM_STATE_CHANGED " +  value
                    + " reason " + reason);
            return;
        }

        Intent intent = new Intent(TelephonyIntents.ACTION_SIM_STATE_CHANGED);
        intent.addFlags(Intent.FLAG_RECEIVER_REPLACE_PENDING);
        intent.putExtra(Phone.PHONE_NAME_KEY, "Phone");
        intent.putExtra(INTENT_KEY_ICC_STATE, value);
        intent.putExtra(INTENT_KEY_LOCKED_REASON, reason);

        intent.putExtra(MSimConstants.SUBSCRIPTION_KEY, subId);
        log("Broadcasting intent ACTION_SIM_STATE_CHANGED " +  value
            + " reason " + reason + " for subscription : " + subId);
        ActivityManagerNative.broadcastStickyIntent(intent, READ_PHONE_STATE);
    }

    @Override
    protected void setExternalState(State newState, boolean override) {
        if (mCardIndex == null) {
            loge("setExternalState: Card Index is not set; Return!!");
            return;
        }

        if (!override && newState == mExternalState) {
            return;
        }
        mExternalState = newState;
        MSimTelephonyManager.setTelephonyProperty(PROPERTY_SIM_STATE,
                mCardIndex, getState().toString());
        broadcastIccStateChangedIntent(mExternalState.getIntentString(),
                mExternalState.getReason());
        // TODO: Need to notify registrants for other states as well.
        if ( State.ABSENT == mExternalState) {
            mAbsentRegistrants.notifyRegistrants();
        }
    }

    @Override
    protected void log(String msg) {
        if (DBG) Log.d(LOG_TAG, "[CardIndex:" + mCardIndex + "]" + msg);
    }

    @Override
    protected void loge(String msg) {
        Log.e(LOG_TAG, "[CardIndex:" + mCardIndex + "]" + msg);
    }
}
