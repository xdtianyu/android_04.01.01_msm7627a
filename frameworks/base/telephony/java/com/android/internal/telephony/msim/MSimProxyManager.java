/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
 */

package com.android.internal.telephony.msim;

import android.content.Context;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.telephony.ServiceState;

import com.android.internal.telephony.CommandsInterface;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneBase;
import com.android.internal.telephony.PhoneProxy;
import com.android.internal.telephony.uicc.UiccController;

public class MSimProxyManager {
    static final String LOG_TAG = "PROXY";

    //***** Class Variables
    private static MSimProxyManager sMSimProxyManager;

    private Phone[] mProxyPhones;

    private UiccController mUiccController;

    private CommandsInterface[] mCi;

    private Context mContext;

    //MSimIccPhoneBookInterfaceManager; Proxy to use proper IccPhoneBookInterfaceManagerProxy object
    private MSimIccPhoneBookInterfaceManagerProxy mMSimIccPhoneBookInterfaceManagerProxy;

    //MSimPhoneSubInfoProxy to use proper PhoneSubInfoProxy object
    private MSimPhoneSubInfoProxy mMSimPhoneSubInfoProxy;

    //MSimIccSmsInterfaceManager to use proper IccSmsInterfaceManager object
    private MSimIccSmsInterfaceManagerProxy mMSimIccSmsInterfaceManagerProxy;

    private CardSubscriptionManager mCardSubscriptionManager;

    private SubscriptionManager mSubscriptionManager;

    //***** Class Methods
    public static MSimProxyManager getInstance(Context context, Phone[] phoneProxy,
            UiccController uiccController, CommandsInterface[] ci) {
        if (sMSimProxyManager == null) {
            sMSimProxyManager = new MSimProxyManager(context, phoneProxy, uiccController, ci);
        }
        return sMSimProxyManager;
    }

    static public MSimProxyManager getInstance() {
        return sMSimProxyManager;
    }

    private MSimProxyManager(Context context, Phone[] phoneProxy, UiccController uiccController,
            CommandsInterface[] ci) {
        logd("Constructor - Enter");

        mContext = context;
        mProxyPhones = phoneProxy;
        mUiccController = uiccController;
        mCi = ci;

        mMSimIccPhoneBookInterfaceManagerProxy
                = new MSimIccPhoneBookInterfaceManagerProxy(mProxyPhones);
        mMSimPhoneSubInfoProxy = new MSimPhoneSubInfoProxy(mProxyPhones);
        mMSimIccSmsInterfaceManagerProxy = new MSimIccSmsInterfaceManagerProxy(mProxyPhones);
        mCardSubscriptionManager = CardSubscriptionManager.getInstance(context, uiccController, ci);
        mSubscriptionManager = SubscriptionManager.getInstance(context, uiccController, ci);

        logd("Constructor - Exit");
    }

    public void updateDataConnectionTracker(int sub) {
        ((MSimPhoneProxy) mProxyPhones[sub]).updateDataConnectionTracker();
    }

    public void enableDataConnectivity(int sub) {
        ((MSimPhoneProxy) mProxyPhones[sub]).setInternalDataEnabled(true);
    }

    public void disableDataConnectivity(int sub,
            Message dataCleanedUpMsg) {
        ((MSimPhoneProxy) mProxyPhones[sub]).setInternalDataEnabled(false, dataCleanedUpMsg);
    }

    public boolean enableDataConnectivityFlag(int sub) {
        return ((MSimPhoneProxy) mProxyPhones[sub]).setInternalDataEnabledFlag(true);
    }

    public boolean disableDataConnectivityFlag(int sub) {
        return ((MSimPhoneProxy) mProxyPhones[sub]).setInternalDataEnabledFlag(false);
    }

    public void updateCurrentCarrierInProvider(int sub) {
        ((MSimPhoneProxy) mProxyPhones[sub]).updateCurrentCarrierInProvider();
    }

    public void checkAndUpdatePhoneObject(Subscription userSub) {
        int subId = userSub.subId;
        if ((userSub.appType.equals("SIM")
                || userSub.appType.equals("USIM"))
                && (!mProxyPhones[subId].getPhoneName().equals("GSM"))) {
            logd("gets New GSM phone" );
            ((PhoneProxy) mProxyPhones[subId])
                .updatePhoneObject(ServiceState.RIL_RADIO_TECHNOLOGY_GSM);
        } else if ((userSub.appType.equals("RUIM")
                || userSub.appType.equals("CSIM"))
                && (!mProxyPhones[subId].getPhoneName().equals("CDMA"))) {
            logd("gets New CDMA phone" );
            ((PhoneProxy) mProxyPhones[subId])
                .updatePhoneObject(ServiceState.RIL_RADIO_TECHNOLOGY_1xRTT);
        }
    }

    public void registerForAllDataDisconnected(int sub, Handler h, int what, Object obj) {
        ((MSimPhoneProxy) mProxyPhones[sub]).registerForAllDataDisconnected(h, what, obj);
    }

    public void unregisterForAllDataDisconnected(int sub, Handler h) {
        ((MSimPhoneProxy) mProxyPhones[sub]).unregisterForAllDataDisconnected(h);
    }

    public boolean isDataDisconnected(int sub) {
        Phone activePhone = ((MSimPhoneProxy) mProxyPhones[sub]).getActivePhone();
        return ((PhoneBase) activePhone).mDataConnectionTracker.isDisconnected();
    }

    private void logd(String string) {
        Log.d(LOG_TAG, string);
    }
}
