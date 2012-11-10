/*
 * Copyright (C) 2006 The Android Open Source Project
 * Copyright (c) 2011-2012 Code Aurora Forum. All rights reserved.
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

import android.app.AlarmManager;
import android.content.Context;
import android.os.AsyncResult;
import android.os.Handler;
import android.os.Message;
import android.os.RegistrantList;
import android.provider.Settings;
import android.util.Log;

import com.android.internal.telephony.ApnContext;
import com.android.internal.telephony.DataProfile;
import com.android.internal.telephony.DataConnection;
import com.android.internal.telephony.DataConnectionAc;
import com.android.internal.telephony.uicc.IccRecords;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneBase;
import com.android.internal.telephony.msim.Subscription;
import com.android.internal.telephony.msim.MSimPhoneFactory;

import com.android.internal.telephony.gsm.GsmDataConnectionTracker;
import com.android.internal.telephony.uicc.UiccController;

import java.util.ArrayList;
import java.util.Collection;

/**
 * This file is used to handle Multi sim case
 * Functions are overriden to register and notify data disconnect
 */
public final class MSimGsmDataConnectionTracker extends GsmDataConnectionTracker {

    /** Subscription id */
    protected Integer mSubscription;

    /**
     * List of messages that are waiting to be posted, when data call disconnect
     * is complete
     */
    private ArrayList <Message> mDisconnectAllCompleteMsgList = new ArrayList<Message>();

    private RegistrantList mAllDataDisconnectedRegistrants = new RegistrantList();

    protected int mDisconnectPendingCount = 0;

    MSimGsmDataConnectionTracker(PhoneBase p) {
        super(p);
        mSubscription = mPhone.getSubscription();
        mInternalDataEnabled = isActiveDataSubscription();
        log("mInternalDataEnabled (is data sub?) = " + mInternalDataEnabled);
        broadcastMessenger();
    }

    protected void registerForAllEvents() {
        mPhone.mCM.registerForAvailable (this, EVENT_RADIO_AVAILABLE, null);
        mPhone.mCM.registerForOffOrNotAvailable(this, EVENT_RADIO_OFF_OR_NOT_AVAILABLE, null);
        mPhone.mCM.registerForDataNetworkStateChanged(this, EVENT_DATA_STATE_CHANGED, null);
        mPhone.getCallTracker().registerForVoiceCallEnded (this, EVENT_VOICE_CALL_ENDED, null);
        mPhone.getCallTracker().registerForVoiceCallStarted (this, EVENT_VOICE_CALL_STARTED, null);
        mPhone.getServiceStateTracker().registerForDataConnectionAttached(this,
               EVENT_DATA_CONNECTION_ATTACHED, null);
        mPhone.getServiceStateTracker().registerForDataConnectionDetached(this,
               EVENT_DATA_CONNECTION_DETACHED, null);
        mPhone.getServiceStateTracker().registerForRoamingOn(this, EVENT_ROAMING_ON, null);
        mPhone.getServiceStateTracker().registerForRoamingOff(this, EVENT_ROAMING_OFF, null);
        mPhone.getServiceStateTracker().registerForPsRestrictedEnabled(this,
                EVENT_PS_RESTRICT_ENABLED, null);
        mPhone.getServiceStateTracker().registerForPsRestrictedDisabled(this,
                EVENT_PS_RESTRICT_DISABLED, null);
    }

    protected void unregisterForAllEvents() {
         //Unregister for all events
        mPhone.mCM.unregisterForAvailable(this);
        mPhone.mCM.unregisterForOffOrNotAvailable(this);
        IccRecords r = mIccRecords.get();
        if (r != null) { r.unregisterForRecordsLoaded(this);}
        mPhone.mCM.unregisterForDataNetworkStateChanged(this);
        mPhone.getCallTracker().unregisterForVoiceCallEnded(this);
        mPhone.getCallTracker().unregisterForVoiceCallStarted(this);
        mPhone.getServiceStateTracker().unregisterForDataConnectionAttached(this);
        mPhone.getServiceStateTracker().unregisterForDataConnectionDetached(this);
        mPhone.getServiceStateTracker().unregisterForRoamingOn(this);
        mPhone.getServiceStateTracker().unregisterForRoamingOff(this);
        mPhone.getServiceStateTracker().unregisterForPsRestrictedEnabled(this);
        mPhone.getServiceStateTracker().unregisterForPsRestrictedDisabled(this);
    }

    @Override
    public void handleMessage (Message msg) {
        if (!isActiveDataSubscription()) {
            loge("Ignore GSM msgs since GSM phone is not the current DDS");
            return;
        }
        switch (msg.what) {
            case EVENT_SET_INTERNAL_DATA_ENABLE:
                boolean enabled = (msg.arg1 == ENABLED) ? true : false;
                onSetInternalDataEnabled(enabled, (Message) msg.obj);
                break;

            default:
                super.handleMessage(msg);
        }
    }

    /**
     * If tearDown is true, this only tears down a CONNECTED session. Presently,
     * there is no mechanism for abandoning an INITING/CONNECTING session,
     * but would likely involve cancelling pending async requests or
     * setting a flag or new state to ignore them when they came in
     *
     * Notify Data connection after disonnect complete
     *
     * @param tearDown true if the underlying GsmDataConnection should be
     * disconnected.
     * @param reason reason for the clean up.
     *
     */
    @Override
    protected void cleanUpAllConnections(boolean tearDown, String reason) {
        super.cleanUpAllConnections(tearDown, reason);

        log("cleanUpConnection: mDisconnectPendingCount = " + mDisconnectPendingCount);
        if (tearDown && mDisconnectPendingCount == 0) {
            notifyDataDisconnectComplete();
            notifyAllDataDisconnected();
        }
    }

    @Override
    protected void cleanUpConnection(boolean tearDown, ApnContext apnContext, boolean doAll) {

        if (apnContext == null) {
            if (DBG) log("cleanUpConnection: apn context is null");
            return;
        }

        DataConnectionAc dcac = apnContext.getDataConnectionAc();
        if (DBG) {
            log("cleanUpConnection: E tearDown=" + tearDown + " reason=" + apnContext.getReason() +
                    " apnContext=" + apnContext);
        }
        if (tearDown) {
            if (apnContext.isDisconnected()) {
                // The request is tearDown and but ApnContext is not connected.
                // If apnContext is not enabled anymore, break the linkage to the DCAC/DC.
                apnContext.setState(State.IDLE);
                if (!apnContext.isReady()) {
                    apnContext.setDataConnection(null);
                    apnContext.setDataConnectionAc(null);
                }
            } else {
                // Connection is still there. Try to clean up.
                if (dcac != null) {
                    if (apnContext.getState() != State.DISCONNECTING) {
                        boolean disconnectAll = doAll;
                        if (Phone.APN_TYPE_DUN.equals(apnContext.getApnType())) {
                            DataProfile dunSetting = fetchDunApn();
                            if (dunSetting != null &&
                                    dunSetting.equals(apnContext.getApnSetting())) {
                                if (DBG) log("tearing down dedicated DUN connection");
                                // we need to tear it down - we brought it up just for dun and
                                // other people are camped on it and now dun is done.  We need
                                // to stop using it and let the normal apn list get used to find
                                // connections for the remaining desired connections
                                disconnectAll = true;
                            }
                        }
                        if (DBG) {
                            log("cleanUpConnection: tearing down" + (disconnectAll ? " all" :""));
                        }
                        Message msg = obtainMessage(EVENT_DISCONNECT_DONE, apnContext);
                        if (disconnectAll) {
                            apnContext.getDataConnection().tearDownAll(apnContext.getReason(), msg);
                        } else {
                            apnContext.getDataConnection().tearDown(apnContext.getReason(), msg);
                        }
                        apnContext.setState(State.DISCONNECTING);
                        mDisconnectPendingCount++;
                    }
                } else {
                    // apn is connected but no reference to dcac.
                    // Should not be happen, but reset the state in case.
                    apnContext.setState(State.IDLE);
                    mPhone.notifyDataConnection(apnContext.getReason(),
                                                apnContext.getApnType());
                }
            }
        } else {
            // force clean up the data connection.
            if (dcac != null) dcac.resetSync();
            apnContext.setState(State.IDLE);
            mPhone.notifyDataConnection(apnContext.getReason(), apnContext.getApnType());
            apnContext.setDataConnection(null);
            apnContext.setDataConnectionAc(null);
        }

        // make sure reconnection alarm is cleaned up if there is no ApnContext
        // associated to the connection.
        if (dcac != null) {
            Collection<ApnContext> apnList = dcac.getApnListSync();
            if (apnList.isEmpty()) {
                cancelReconnectAlarm(dcac);
            }
        }
        if (DBG) {
            log("cleanUpConnection: X tearDown=" + tearDown + " reason=" + apnContext.getReason() +
                    " apnContext=" + apnContext + " dc=" + apnContext.getDataConnection());
        }
    }

    /**
     * Called when EVENT_DISCONNECT_DONE is received.
     */
    @Override
    protected void onDisconnectDone(int connId, AsyncResult ar) {
        super.onDisconnectDone(connId, ar);
        if (mDisconnectPendingCount > 0)
            mDisconnectPendingCount--;

        if (mDisconnectPendingCount == 0) {
            notifyDataDisconnectComplete();
            notifyAllDataDisconnected();
        }
    }

    @Override
    protected void broadcastMessenger() {
        // Broadcast the data connection tracker messenger only if
        // this is corresponds to the current DDS.
        if (!isActiveDataSubscription()) {
            return;
        }
        super.broadcastMessenger();
    }

    @Override
    protected IccRecords getUiccCardApplication() {
        Subscription subscriptionData = null;
        int appType = UiccController.APP_FAM_3GPP;

        if (mPhone instanceof MSimCDMALTEPhone) {
            subscriptionData = ((MSimCDMALTEPhone)mPhone).getSubscriptionInfo();
            appType = UiccController.APP_FAM_3GPP2;
        } else if (mPhone instanceof MSimGSMPhone) {
            subscriptionData = ((MSimGSMPhone)mPhone).getSubscriptionInfo();
            appType = UiccController.APP_FAM_3GPP;
        }

        if(subscriptionData != null) {
            return  mUiccController.getIccRecords(subscriptionData.slotId, appType);
        }

        return null;
    }

    @Override
    public boolean setInternalDataEnabled(boolean enable) {
        return setInternalDataEnabled(enable, null);
    }

    public boolean setInternalDataEnabled(boolean enable, Message onCompleteMsg) {
        if (DBG)
            log("setInternalDataEnabled(" + enable + ")");

        Message msg = obtainMessage(EVENT_SET_INTERNAL_DATA_ENABLE, onCompleteMsg);
        msg.arg1 = (enable ? ENABLED : DISABLED);
        sendMessage(msg);
        return true;
    }

    public boolean setInternalDataEnabledFlag(boolean enable) {
        if (DBG)
            log("setInternalDataEnabledFlag(" + enable + ")");

        if (mInternalDataEnabled != enable) {
            mInternalDataEnabled = enable;
        }
        return true;
    }

    @Override
    protected void onSetInternalDataEnabled(boolean enable) {
        onSetInternalDataEnabled(enable, null);
    }

    protected void onSetInternalDataEnabled(boolean enabled, Message onCompleteMsg) {
        boolean sendOnComplete = true;

        synchronized (mDataEnabledLock) {
            mInternalDataEnabled = enabled;
            if (enabled) {
                log("onSetInternalDataEnabled: changed to enabled, try to setup data call");
                resetAllRetryCounts();
                onTrySetupData(Phone.REASON_DATA_ENABLED);
            } else {
                sendOnComplete = false;
                log("onSetInternalDataEnabled: changed to disabled, cleanUpAllConnections");
                cleanUpAllConnections(null, onCompleteMsg);
            }
        }

        if (sendOnComplete) {
            if (onCompleteMsg != null) {
                onCompleteMsg.sendToTarget();
            }
        }
    }

    @Override
    protected void onDataSetupComplete(AsyncResult ar) {
        super.onDataSetupComplete(ar);

        /* If flag is set to false after SETUP_DATA_CALL is invoked, we need
         * to clean data connections.
         */
        if (!mInternalDataEnabled) {
            cleanUpAllConnections(null);
        }
    }


    @Override
    public void cleanUpAllConnections(String cause) {
        cleanUpAllConnections(cause, null);
    }

    public void updateRecords() {
        if (isActiveDataSubscription()) {
            onUpdateIcc();
        }
    }

    public void cleanUpAllConnections(String cause, Message disconnectAllCompleteMsg) {
        log("cleanUpAllConnections");
        if (disconnectAllCompleteMsg != null) {
            mDisconnectAllCompleteMsgList.add(disconnectAllCompleteMsg);
        }

        Message msg = obtainMessage(EVENT_CLEAN_UP_ALL_CONNECTIONS);
        msg.obj = cause;
        sendMessage(msg);
    }

    /** Returns true if this is current DDS. */
    protected boolean isActiveDataSubscription() {
        return (mSubscription != null
                ? mSubscription == MSimPhoneFactory.getDataSubscription()
                : false);
    }

    // setAsCurrentDataConnectionTracker
    protected void update() {
        log("update");
        if (isActiveDataSubscription()) {
            log("update(): Active DDS, register for all events now!");
            registerForAllEvents();
            onUpdateIcc();

            mUserDataEnabled = Settings.Secure.getInt(mPhone.getContext().getContentResolver(),
                    Settings.Secure.MOBILE_DATA, 1) == 1;

            if (mPhone instanceof MSimCDMALTEPhone) {
                ((MSimCDMALTEPhone)mPhone).updateCurrentCarrierInProvider();
            } else if (mPhone instanceof MSimGSMPhone) {
                ((MSimGSMPhone)mPhone).updateCurrentCarrierInProvider();
            } else {
                log("Phone object is not MultiSim. This should not hit!!!!");
            }

            broadcastMessenger();
        } else {
            unregisterForAllEvents();
            log("update(): NOT the active DDS, unregister for all events!");
        }
    }

    protected void notifyDataDisconnectComplete() {
        log("notifyDataDisconnectComplete");
        for (Message m: mDisconnectAllCompleteMsgList) {
            m.sendToTarget();
        }
        mDisconnectAllCompleteMsgList.clear();
    }

    protected void notifyAllDataDisconnected() {
        mAllDataDisconnectedRegistrants.notifyRegistrants();
    }

    public void registerForAllDataDisconnected(Handler h, int what, Object obj) {
        mAllDataDisconnectedRegistrants.addUnique(h, what, obj);

        if (isDisconnected()) {
            log("notify All Data Disconnected");
            mAllDataDisconnectedRegistrants.notifyRegistrants();
        }
    }

    public void unregisterForAllDataDisconnected(Handler h) {
        mAllDataDisconnectedRegistrants.remove(h);
    }

    @Override
    protected void log(String s) {
        Log.d(LOG_TAG, "[MSimGsmDCT:" + mSubscription + "] " + s);
    }

    @Override
    protected void loge(String s) {
        Log.e(LOG_TAG, "[MSimGsmDCT:" + mSubscription + "] " + s);
    }
}
