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

import android.app.AlarmManager;
import android.content.Context;
import android.os.AsyncResult;
import android.os.Handler;
import android.os.Message;
import android.os.RegistrantList;
import android.provider.Settings;
import android.util.Log;

import com.android.internal.telephony.cdma.CdmaDataConnectionTracker;
import com.android.internal.telephony.DataConnection;
import com.android.internal.telephony.DataConnectionAc;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.msim.Subscription;
import com.android.internal.telephony.msim.MSimPhoneFactory;
import com.android.internal.telephony.uicc.IccRecords;
import com.android.internal.telephony.uicc.UiccCardApplication;
import com.android.internal.telephony.uicc.UiccController;

import java.util.ArrayList;

/**
 * This file is used to handle Multi sim case
 * Functions are overriden to register and notify data disconnect
 */
public final class MSimCdmaDataConnectionTracker extends CdmaDataConnectionTracker {

    /** Subscription id */
    protected Integer mSubscription;

    protected MSimCDMALTEPhone mPhone;

    /**
     * List of messages that are waiting to be posted, when data call disconnect
     * is complete
     */
    private ArrayList <Message> mDisconnectAllCompleteMsgList = new ArrayList<Message>();

    private RegistrantList mAllDataDisconnectedRegistrants = new RegistrantList();

    protected int mDisconnectPendingCount = 0;

    MSimCdmaDataConnectionTracker(MSimCDMALTEPhone p) {
        super(p);
        mPhone = p;
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
        mPhone.getServiceStateTracker()
            .registerForDataConnectionAttached(this, EVENT_TRY_SETUP_DATA, null);
        mPhone.getServiceStateTracker()
            .registerForDataConnectionDetached(this, EVENT_CDMA_DATA_DETACHED, null);
        mPhone.getServiceStateTracker().registerForRoamingOn(this, EVENT_ROAMING_ON, null);
        mPhone.getServiceStateTracker().registerForRoamingOff(this, EVENT_ROAMING_OFF, null);
        mPhone.mCM.registerForCdmaOtaProvision(this, EVENT_CDMA_OTA_PROVISION, null);
        mDpt.registerForModemProfileReady(this, EVENT_MODEM_DATA_PROFILE_READY, null);
    }


    protected void unregisterForAllEvents() {
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
        mPhone.mCM.unregisterForCdmaOtaProvision(this);
        mDpt.unregisterForModemProfileReady(this);
    }

    @Override
    public void handleMessage (Message msg) {
        if (!isActiveDataSubscription()) {
            loge("Ignore CDMA msgs since CDMA phone is not the current DDS");
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
     * Cleanup the CDMA data connection (only one is supported)
     *
     * @param tearDown true if the underlying DataConnection should be disconnected.
     * @param reason for the clean up.
     */
    @Override
    protected void cleanUpConnection(boolean tearDown, String reason, boolean doAll) {
        if (DBG) log("cleanUpConnection: reason: " + reason);

        // Clear the reconnect alarm, if set.
        if (mReconnectIntent != null) {
            AlarmManager am =
                (AlarmManager) mPhone.getContext().getSystemService(Context.ALARM_SERVICE);
            am.cancel(mReconnectIntent);
            mReconnectIntent = null;
        }

        setState(State.DISCONNECTING);
        notifyOffApnsOfAvailability(reason);

        boolean notificationDeferred = false;
        for (DataConnection conn : mDataConnections.values()) {
            if(conn != null) {
                DataConnectionAc dcac =
                    mDataConnectionAsyncChannels.get(conn.getDataConnectionId());
                if (tearDown) {
                    if (doAll) {
                        if (DBG) log("cleanUpConnection: teardown, conn.tearDownAll");
                        conn.tearDownAll(reason, obtainMessage(EVENT_DISCONNECT_DONE,
                                conn.getDataConnectionId(), 0, reason));
                    } else {
                        if (DBG) log("cleanUpConnection: teardown, conn.tearDown");
                        conn.tearDown(reason, obtainMessage(EVENT_DISCONNECT_DONE,
                                conn.getDataConnectionId(), 0, reason));
                    }
                    notificationDeferred = true;
                    mDisconnectPendingCount++;
                } else {
                    if (DBG) log("cleanUpConnection: !tearDown, call conn.resetSynchronously");
                    if (dcac != null) {
                        dcac.resetSync();
                    }
                    notificationDeferred = false;
                }
            }
        }

        stopNetStatPoll();

        if (!notificationDeferred) {
            if (DBG) log("cleanupConnection: !notificationDeferred");
            gotoIdleAndNotifyDataConnection(reason);
        }

        if (tearDown && mDisconnectPendingCount == 0) {
            notifyDataDisconnectComplete();
            notifyAllDataDisconnected();
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
    protected void onDataStateChanged(AsyncResult ar) {
        super.onDataStateChanged(ar);
        if (mState == State.IDLE) {
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
        Subscription subscriptionData = mPhone.getSubscriptionInfo();
        if(subscriptionData != null) {
            return mUiccController.getIccRecords(subscriptionData.slotId,
                    UiccController.APP_FAM_3GPP2);
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
            log("setInternalDataEnabled(" + enable + ")");

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

        /* When flag is set to flase after SETUP_DATA_CALL is invoked, we need to
         * clean the data connections.
         */
        if (!mInternalDataEnabled) {
            cleanUpAllConnections(null);
        }
    }

    @Override
    public void cleanUpAllConnections(String cause) {
        cleanUpAllConnections(cause, null);
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

    public void updateRecords() {
        onUpdateIcc();
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
            mPhone.updateCurrentCarrierInProvider();
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

    public void notifyAllDataDisconnected() {
        log("notifyAllDataDisconnected");
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
        Log.d(LOG_TAG, "[MSimCdmaDCT:" + mSubscription + "] " + s);
    }

    @Override
    protected void loge(String s) {
        Log.e(LOG_TAG, "[MSimCdmaDCT:" + mSubscription + "] " + s);
    }
}
