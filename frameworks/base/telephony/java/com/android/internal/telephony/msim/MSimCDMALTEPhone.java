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

import android.app.ActivityManagerNative;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.SQLException;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.provider.Telephony;
import android.os.AsyncResult;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.os.SystemProperties;
import android.text.TextUtils;
import android.util.Log;
import android.net.Uri;

import android.telephony.MSimTelephonyManager;

import com.android.internal.telephony.CommandsInterface;
import com.android.internal.telephony.CommandException;
import com.android.internal.telephony.cdma.CDMALTEPhone;
import com.android.internal.telephony.cdma.CdmaCallTracker;
import com.android.internal.telephony.cdma.CdmaSubscriptionSourceManager;
import com.android.internal.telephony.cdma.CdmaSMSDispatcher;
import com.android.internal.telephony.cdma.EriManager;
import com.android.internal.telephony.cdma.RuimPhoneBookInterfaceManager;
import com.android.internal.telephony.cdma.RuimSmsInterfaceManager;
import com.android.internal.telephony.MccTable;
import com.android.internal.telephony.OperatorInfo;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneNotifier;
import com.android.internal.telephony.PhoneSubInfo;
import com.android.internal.telephony.ServiceStateTracker;
import com.android.internal.telephony.TelephonyProperties;
import com.android.internal.telephony.TelephonyIntents;

import com.android.internal.telephony.msim.Subscription;
import com.android.internal.telephony.msim.SubscriptionManager;
import com.android.internal.telephony.uicc.UiccController;
import com.android.internal.telephony.uicc.UiccCardApplication;
import com.android.internal.telephony.uicc.IccRecords;
import com.android.internal.telephony.uicc.IsimRecords;

import static com.android.internal.telephony.TelephonyProperties.PROPERTY_ICC_OPERATOR_ALPHA;
import static com.android.internal.telephony.TelephonyProperties.PROPERTY_ICC_OPERATOR_ISO_COUNTRY;
import static com.android.internal.telephony.TelephonyProperties.PROPERTY_ICC_OPERATOR_NUMERIC;

import static com.android.internal.telephony.MSimConstants.SUBSCRIPTION_KEY;
import static com.android.internal.telephony.MSimConstants.EVENT_SUBSCRIPTION_ACTIVATED;
import static com.android.internal.telephony.MSimConstants.EVENT_SUBSCRIPTION_DEACTIVATED;

public class MSimCDMALTEPhone extends CDMALTEPhone {
    static final String LOG_TAG = "CDMA";

    private Subscription mSubscriptionData; // to store subscription information
    private int mSubscription = 0;

    // Constructors
    public MSimCDMALTEPhone(Context context, CommandsInterface ci, PhoneNotifier notifier,
            int subscription) {
        this(context, ci, notifier, false, subscription);
    }

    public MSimCDMALTEPhone(Context context, CommandsInterface ci, PhoneNotifier notifier,
            boolean unitTestMode, int subscription) {
        super(context, ci, notifier);

        mSubscription = subscription;

        Log.d(LOG_TAG, "MSimCDMALTEPhone: constructor: sub = " + mSubscription);

        mDataConnectionTracker = new MSimCdmaDataConnectionTracker(this);

        mVmNumCdmaKey = mVmNumCdmaKey + mSubscription;
        mVmCountKey = mVmCountKey + mSubscription;

        SubscriptionManager subMgr = SubscriptionManager.getInstance();
        subMgr.registerForSubscriptionActivated(mSubscription,
                this, EVENT_SUBSCRIPTION_ACTIVATED, null);
        subMgr.registerForSubscriptionDeactivated(mSubscription,
                this, EVENT_SUBSCRIPTION_DEACTIVATED, null);
    }

    @Override
    protected void initSstIcc() {
        mSST = new MSimCdmaLteServiceStateTracker(this);
    }

    @Override
    protected void init(Context context, PhoneNotifier notifier) {
        mCM.setPhoneType(Phone.PHONE_TYPE_CDMA);
        mCT = new CdmaCallTracker(this);
        mCdmaSSM = CdmaSubscriptionSourceManager.getInstance(context, mCM, this,
                EVENT_CDMA_SUBSCRIPTION_SOURCE_CHANGED, null);
        mRuimPhoneBookInterfaceManager = new RuimPhoneBookInterfaceManager(this);
        mSubInfo = new PhoneSubInfo(this);
        mEriManager = new EriManager(this, context, EriManager.ERI_FROM_XML);

        mCM.registerForAvailable(this, EVENT_RADIO_AVAILABLE, null);
        mCM.registerForOffOrNotAvailable(this, EVENT_RADIO_OFF_OR_NOT_AVAILABLE, null);
        mCM.registerForOn(this, EVENT_RADIO_ON, null);
        mCM.setOnSuppServiceNotification(this, EVENT_SSN, null);
        mSST.registerForNetworkAttached(this, EVENT_REGISTERED_TO_NETWORK, null);
        mCM.setEmergencyCallbackMode(this, EVENT_EMERGENCY_CALLBACK_MODE_ENTER, null);
        mCM.registerForExitEmergencyCallbackMode(this, EVENT_EXIT_EMERGENCY_CALLBACK_RESPONSE,
                null);

        PowerManager pm
            = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,LOG_TAG);

        // This is needed to handle phone process crashes
        String inEcm = SystemProperties.get(TelephonyProperties.PROPERTY_INECM_MODE, "false");
        mIsPhoneInEcmState = inEcm.equals("true");
        if (mIsPhoneInEcmState) {
            // Send a message which will invoke handleExitEmergencyCallbackMode
            mCM.exitEmergencyCallbackMode(obtainMessage(EVENT_EXIT_EMERGENCY_CALLBACK_RESPONSE));
        }

        // get the string that specifies the carrier OTA Sp number
        mCarrierOtaSpNumSchema = SystemProperties.get(
                TelephonyProperties.PROPERTY_OTASP_NUM_SCHEMA,"");

        // Notify voicemails.
        notifier.notifyMessageWaitingChanged(this);
        setProperties();
    }

    @Override
    public void dispose() {
        super.dispose();

        SubscriptionManager subMgr = SubscriptionManager.getInstance();
        subMgr.unregisterForSubscriptionActivated(mSubscription, this);
        subMgr.unregisterForSubscriptionDeactivated(mSubscription, this);
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

            default:
                super.handleMessage(msg);
        }
    }

    private void onSubscriptionActivated() {
        SubscriptionManager subMgr = SubscriptionManager.getInstance();
        mSubscriptionData = subMgr.getCurrentSubscription(mSubscription);

        log("SUBSCRIPTION ACTIVATED : slotId : " + mSubscriptionData.slotId
                + " appid : " + mSubscriptionData.m3gpp2Index
                + " subId : " + mSubscriptionData.subId
                + " subStatus : " + mSubscriptionData.subStatus);

        // Make sure properties are set for proper subscription.
        setProperties();

        onUpdateIccAvailability();
        mSST.sendMessage(mSST.obtainMessage(ServiceStateTracker.EVENT_ICC_CHANGED));
        ((MSimCdmaLteServiceStateTracker)mSST).updateCdmaSubscription();
        ((MSimCdmaDataConnectionTracker)mDataConnectionTracker).updateRecords();

        // read the subscription specifics now
        mCM.getDeviceIdentity(obtainMessage(EVENT_GET_DEVICE_IDENTITY_DONE));
        mCM.getBasebandVersion(obtainMessage(EVENT_GET_BASEBAND_VERSION_DONE));
    }

    private void onSubscriptionDeactivated() {
        log("SUBSCRIPTION DEACTIVATED");
        // resetSubSpecifics
        mImei = null;
        mImeiSv = null;
        mMeid = null;
        mEsn = null;
        setVoiceMessageCount(0);
        mSubscriptionData = null;
    }

    //Gets Subscription information in the Phone Object
    public Subscription getSubscriptionInfo() {
        return mSubscriptionData;
    }

    // Gets the Subscription ID
    public int getSubscription() {
        return mSubscription;
    }

    @Override
    protected void sendEmergencyCallbackModeChange(){
        //Send an Intent
        Intent intent = new Intent(TelephonyIntents.ACTION_EMERGENCY_CALLBACK_MODE_CHANGED);
        intent.putExtra(PHONE_IN_ECM_STATE, mIsPhoneInEcmState);
        intent.putExtra(SUBSCRIPTION_KEY, mSubscription);
        ActivityManagerNative.broadcastStickyIntent(intent,null);
        log("sendEmergencyCallbackModeChange");
    }

    // Set the properties per subscription
    private void setProperties() {
        //Change the system property
        MSimTelephonyManager.setTelephonyProperty(TelephonyProperties.CURRENT_ACTIVE_PHONE,
                mSubscription,
                new Integer(Phone.PHONE_TYPE_CDMA).toString());
        // Sets operator alpha property by retrieving from build-time system property
        String operatorAlpha = SystemProperties.get("ro.cdma.home.operator.alpha");
        setSystemProperty(PROPERTY_ICC_OPERATOR_ALPHA, operatorAlpha);

        // Sets operator numeric property by retrieving from build-time system property
        String operatorNumeric = SystemProperties.get(PROPERTY_CDMA_HOME_OPERATOR_NUMERIC);
        setSystemProperty(PROPERTY_ICC_OPERATOR_NUMERIC, operatorNumeric);
        // Sets iso country property by retrieving from build-time system property
        setIsoCountryProperty(operatorNumeric);
        // Updates MCC MNC device configuration information
        MccTable.updateMccMncConfiguration(mContext, operatorNumeric);
        // Sets current entry in the telephony carrier table
        updateCurrentCarrierInProvider();
    }

    @Override
    protected UiccCardApplication getUiccCardApplication() {
        if(mSubscriptionData != null) {
            return  mUiccController.getUiccCardApplication(mSubscriptionData.slotId,
                    UiccController.APP_FAM_3GPP2);
        }
        return null;
    }

    @Override
    public void setSystemProperty(String property, String value) {
        if(getUnitTestMode()) {
            return;
        }
        MSimTelephonyManager.setTelephonyProperty(property, mSubscription, value);
    }

    public String getSystemProperty(String property, String defValue) {
        if(getUnitTestMode()) {
            return null;
        }
        return MSimTelephonyManager.getTelephonyProperty(property, mSubscription, defValue);
    }

    public void updateDataConnectionTracker() {
        ((MSimCdmaDataConnectionTracker)mDataConnectionTracker).update();
    }

    public void setInternalDataEnabled(boolean enable, Message onCompleteMsg) {
        ((MSimCdmaDataConnectionTracker)mDataConnectionTracker)
                .setInternalDataEnabled(enable, onCompleteMsg);
    }

    public boolean setInternalDataEnabledFlag(boolean enable) {
       return ((MSimCdmaDataConnectionTracker)mDataConnectionTracker)
                .setInternalDataEnabledFlag(enable);
    }

    /**
     * @return operator numeric.
     */
    public String getOperatorNumeric() {
        String operatorNumeric = null;

        if (mCdmaSubscriptionSource == CDMA_SUBSCRIPTION_NV) {
            operatorNumeric = SystemProperties.get("ro.cdma.home.operator.numeric");
        } else if (mCdmaSubscriptionSource == CDMA_SUBSCRIPTION_RUIM_SIM
                && mIccRecords != null && mIccRecords.get() != null) {
            operatorNumeric = mIccRecords.get().getOperatorNumeric();
        } else {
            Log.e(LOG_TAG, "getOperatorNumeric: Cannot retrieve operatorNumeric:"
                    + " mCdmaSubscriptionSource = " + mCdmaSubscriptionSource + " mIccRecords = "
                    + ((mIccRecords != null) && (mIccRecords.get() != null)
                        ? mIccRecords.get().getRecordsLoaded()
                        : null));
        }

        Log.d(LOG_TAG, "getOperatorNumeric: mCdmaSubscriptionSource = " + mCdmaSubscriptionSource
                + " operatorNumeric = " + operatorNumeric);

        return operatorNumeric;
    }

    /**
     * Sets the "current" field in the telephony provider according to the operator numeric.
     *
     * @return true for success; false otherwise.
     */
    public boolean updateCurrentCarrierInProvider() {
        int currentDds = MSimPhoneFactory.getDataSubscription();
        String operatorNumeric = getOperatorNumeric();

        Log.d(LOG_TAG, "updateCurrentCarrierInProvider: mSubscription = " + getSubscription()
                + " currentDds = " + currentDds + " operatorNumeric = " + operatorNumeric);

        if (!TextUtils.isEmpty(operatorNumeric) && (getSubscription() == currentDds)) {
            try {
                Uri uri = Uri.withAppendedPath(Telephony.Carriers.CONTENT_URI, "current");
                ContentValues map = new ContentValues();
                map.put(Telephony.Carriers.NUMERIC, operatorNumeric);
                mContext.getContentResolver().insert(uri, map);
                return true;
            } catch (SQLException e) {
                Log.e(LOG_TAG, "Can't store current operator", e);
            }
        }
        return false;
    }

    public void registerForAllDataDisconnected(Handler h, int what, Object obj) {
        ((MSimCdmaDataConnectionTracker)mDataConnectionTracker)
               .registerForAllDataDisconnected(h, what, obj);
    }

    public void unregisterForAllDataDisconnected(Handler h) {
        ((MSimCdmaDataConnectionTracker)mDataConnectionTracker)
                .unregisterForAllDataDisconnected(h);
    }

    /* Override the LTE specific methods from CDMALTEPhone - START */
    @Override
    public void selectNetworkManually(OperatorInfo network, Message response) {
        Log.e(LOG_TAG, "selectNetworkManually: not possible in CDMA");
        if (response != null) {
            CommandException ce = new CommandException(CommandException.Error.REQUEST_NOT_SUPPORTED);
            AsyncResult.forMessage(response).exception = ce;
            response.sendToTarget();
        }
    }

    @Override
    public String getSubscriberId() {
        return mSST.getImsi();
    }

    @Override
    public String getImei() {
        Log.e(LOG_TAG, "IMEI is not available in CDMA");
        return null;
    }

    @Override
    public String getDeviceSvn() {
        Log.d(LOG_TAG, "getDeviceSvn(): return 0");
        return "0";
    }

    @Override
    public IsimRecords getIsimRecords() {
        Log.e(LOG_TAG, "getIsimRecords() is only supported on LTE devices");
        return null;
    }

    @Override
    public String getMsisdn() {
        Log.e(LOG_TAG, "Error! getMsisdn() in MSimCDMALTEPhone should not be " +
                "called, GSMPhone inactive.");
        return null;
    }

    @Override
    public void getAvailableNetworks(Message response) {
        Log.e(LOG_TAG, "getAvailableNetworks: not possible in CDMA");
        if (response != null) {
            CommandException ce = new CommandException(CommandException.Error.REQUEST_NOT_SUPPORTED);
            AsyncResult.forMessage(response).exception = ce;
            response.sendToTarget();
        }
    }

    @Override
    public void requestIsimAuthentication(String nonce, Message result) {
        Log.e(LOG_TAG, "requestIsimAuthentication() is only supported on LTE devices");
    }

    @Override
    protected void onUpdateIccAvailability() {
        if (mUiccController == null ) {
            return;
        }

        UiccCardApplication newUiccApplication = getUiccCardApplication();

        UiccCardApplication app = mUiccApplication.get();
        if (app != newUiccApplication) {
            if (app != null) {
                log("Removing stale icc objects.");
                if (mIccRecords.get() != null) {
                    unregisterForRuimRecordEvents();
                    mRuimPhoneBookInterfaceManager.updateIccRecords(null);
                }
                mIccRecords.set(null);
                mUiccApplication.set(null);
                mRuimCard = null;
            }
            if (newUiccApplication != null) {
                log("New Uicc application found");
                mUiccApplication.set(newUiccApplication);
                mRuimCard = mUiccApplication.get().getCard();
                mIccRecords.set(newUiccApplication.getIccRecords());
                registerForRuimRecordEvents();
                mRuimPhoneBookInterfaceManager.updateIccRecords(mIccRecords.get());
            }
        }
    }

    @Override
    protected void log(String s) {
        Log.d(LOG_TAG, "[MSimCDMALTEPhone] " + s);
    }

    /* Override the LTE specific methods from CDMALTEPhone - END */

}
