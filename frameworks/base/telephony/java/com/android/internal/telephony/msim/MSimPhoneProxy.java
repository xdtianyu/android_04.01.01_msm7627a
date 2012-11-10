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

import android.app.ActivityManagerNative;
import android.content.Intent;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import android.telephony.ServiceState;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.IccPhoneBookInterfaceManagerProxy;
import com.android.internal.telephony.IccSmsInterfaceManager;
import com.android.internal.telephony.MSimConstants;
import com.android.internal.telephony.PhoneBase;
import com.android.internal.telephony.PhoneProxy;
import com.android.internal.telephony.PhoneSubInfoProxy;
import com.android.internal.telephony.TelephonyIntents;
import com.android.internal.telephony.uicc.IccFileHandler;
import com.android.internal.telephony.uicc.IccCardProxy;

public class MSimPhoneProxy extends PhoneProxy {

    private int mSubscription = 0;

    //***** Constructors
    public MSimPhoneProxy(PhoneBase phone) {
        super(phone);

        mSubscription = phone.getSubscription();
    }

    protected void init() {
        mIccSmsInterfaceManager =
                new MSimIccSmsInterfaceManager((PhoneBase)this.mActivePhone);
        mIccCardProxy = new MSimIccCardProxy(mActivePhone.getContext(),
                mCommandsInterface, mActivePhone.getSubscription());
    }

    @Override
    protected void createNewPhone(int newVoiceRadioTech) {
        if (ServiceState.isCdma(newVoiceRadioTech)) {
            logd("MSimPhoneProxy: deleteAndCreatePhone: Creating MSimCDMALTEPhone");
            mActivePhone = MSimPhoneFactory.getMSimCdmaPhone(mSubscription);
        } else if (ServiceState.isGsm(newVoiceRadioTech)) {
            logd("MSimPhoneProxy: deleteAndCreatePhone: Creating MSimGsmPhone");
            mActivePhone = MSimPhoneFactory.getMSimGsmPhone(mSubscription);
        }
    }

    @Override
    protected void sendBroadcastStickyIntent() {
        // Send an Intent to the PhoneApp that we had a radio technology change
        Intent intent = new Intent(TelephonyIntents.ACTION_RADIO_TECHNOLOGY_CHANGED);
        intent.addFlags(Intent.FLAG_RECEIVER_REPLACE_PENDING);
        intent.putExtra(Phone.PHONE_NAME_KEY, mActivePhone.getPhoneName());
        intent.putExtra(MSimConstants.SUBSCRIPTION_KEY, mSubscription);
        ActivityManagerNative.broadcastStickyIntent(intent, null);
    }

    public PhoneSubInfoProxy getPhoneSubInfoProxy(){
        return mPhoneSubInfoProxy;
    }

    public IccPhoneBookInterfaceManagerProxy getIccPhoneBookInterfaceManagerProxy() {
        return mIccPhoneBookInterfaceManagerProxy;
    }

    public IccSmsInterfaceManager getIccSmsInterfaceManager() {
        return mIccSmsInterfaceManager;
    }

    public IccFileHandler getIccFileHandler() {
        return ((MSimGSMPhone)mActivePhone).getIccFileHandler();
    }

    public boolean updateCurrentCarrierInProvider() {
        if (mActivePhone instanceof MSimCDMALTEPhone) {
            return ((MSimCDMALTEPhone)mActivePhone).updateCurrentCarrierInProvider();
        } else if (mActivePhone instanceof MSimGSMPhone) {
            return ((MSimGSMPhone)mActivePhone).updateCurrentCarrierInProvider();
        } else {
           logd("Phone object is not MultiSim. This should not hit!!!!");
           return false;
        }
    }

    public void updateDataConnectionTracker() {
        logd("Updating Data Connection Tracker");
        if (mActivePhone instanceof MSimCDMALTEPhone) {
            ((MSimCDMALTEPhone)mActivePhone).updateDataConnectionTracker();
        } else if (mActivePhone instanceof MSimGSMPhone) {
            ((MSimGSMPhone)mActivePhone).updateDataConnectionTracker();
        } else {
           logd("Phone object is not MultiSim. This should not hit!!!!");
        }
    }

    public void setInternalDataEnabled(boolean enable) {
        setInternalDataEnabled(enable, null);
    }

    public boolean setInternalDataEnabledFlag(boolean enable) {
        boolean flag = false;
        if (mActivePhone instanceof MSimCDMALTEPhone) {
            flag = ((MSimCDMALTEPhone)mActivePhone).setInternalDataEnabledFlag(enable);
        } else if (mActivePhone instanceof MSimGSMPhone) {
            flag = ((MSimGSMPhone)mActivePhone).setInternalDataEnabledFlag(enable);
        } else {
           logd("Phone object is not MultiSim. This should not hit!!!!");
        }
        return flag;
    }

    public void setInternalDataEnabled(boolean enable, Message onCompleteMsg) {
        if (mActivePhone instanceof MSimCDMALTEPhone) {
            ((MSimCDMALTEPhone)mActivePhone).setInternalDataEnabled(enable, onCompleteMsg);
        } else if (mActivePhone instanceof MSimGSMPhone) {
            ((MSimGSMPhone)mActivePhone).setInternalDataEnabled(enable, onCompleteMsg);
        } else {
           logd("Phone object is not MultiSim. This should not hit!!!!");
        }
    }

    public void registerForAllDataDisconnected(Handler h, int what, Object obj) {
        if (mActivePhone instanceof MSimCDMALTEPhone) {
            ((MSimCDMALTEPhone)mActivePhone).registerForAllDataDisconnected(h, what, obj);
        } else if (mActivePhone instanceof MSimGSMPhone) {
            ((MSimGSMPhone)mActivePhone).registerForAllDataDisconnected(h, what, obj);
        } else {
           logd("Phone object is not MultiSim. This should not hit!!!!");
        }
    }

    public void unregisterForAllDataDisconnected(Handler h) {
        if (mActivePhone instanceof MSimCDMALTEPhone) {
            ((MSimCDMALTEPhone)mActivePhone).unregisterForAllDataDisconnected(h);
        } else if (mActivePhone instanceof MSimGSMPhone) {
            ((MSimGSMPhone)mActivePhone).unregisterForAllDataDisconnected(h);
        } else {
           logd("Phone object is not MultiSim. This should not hit!!!!");
        }
    }

    public int getSubscription() {
        return mSubscription;
    }

    private void logv(String msg) {
        Log.v(LOG_TAG, "[MSimPhoneProxy(" + mSubscription + ")] " + msg);
    }

    private void logd(String msg) {
        Log.d(LOG_TAG, "[MSimPhoneProxy(" + mSubscription + ")] " + msg);
    }

    private void logw(String msg) {
        Log.w(LOG_TAG, "[MSimPhoneProxy(" + mSubscription + ")] " + msg);
    }

    private void loge(String msg) {
        Log.e(LOG_TAG, "[MSimPhoneProxy(" + mSubscription + ")] " + msg);
    }

    private void logi(String msg) {
        Log.i(LOG_TAG, "[MSimPhoneProxy(" + mSubscription + ")] " + msg);
    }
}
