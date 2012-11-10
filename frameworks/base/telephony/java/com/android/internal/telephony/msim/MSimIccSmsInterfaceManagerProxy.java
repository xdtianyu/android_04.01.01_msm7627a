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

import android.app.PendingIntent;
import android.util.Log;
import android.os.ServiceManager;

import com.android.internal.telephony.Phone;
import com.android.internal.telephony.SmsRawData;

import java.util.ArrayList;
import java.util.List;

/**
 * MSimIccSmsInterfaceManagerProxy to provide an inter-process communication to
 * access Sms in Icc.
 */
public class MSimIccSmsInterfaceManagerProxy extends ISmsMSim.Stub {
    static final String LOG_TAG = "RIL_MSimIccSms";

    protected Phone[] mPhone;

    protected MSimIccSmsInterfaceManagerProxy(Phone[] phone){
        mPhone = phone;

        if (ServiceManager.getService("isms_msim") == null) {
            ServiceManager.addService("isms_msim", this);
        }
    }

    public boolean
    updateMessageOnIccEf(int index, int status, byte[] pdu, int subscription)
                throws android.os.RemoteException {
        MSimIccSmsInterfaceManager iccSmsIntMgr = getIccSmsInterfaceManager(subscription);
        if (iccSmsIntMgr != null) {
            return iccSmsIntMgr.updateMessageOnIccEf(index, status, pdu);
        } else {
            Log.e(LOG_TAG,"updateMessageOnIccEf iccSmsIntMgr is null" +
                          " for Subscription:"+subscription);
            return false;
        }
    }

    public boolean copyMessageToIccEf(int status, byte[] pdu, byte[] smsc, int subscription)
                throws android.os.RemoteException {
        MSimIccSmsInterfaceManager iccSmsIntMgr = getIccSmsInterfaceManager(subscription);
        if (iccSmsIntMgr != null) {
            return iccSmsIntMgr.copyMessageToIccEf(status, pdu, smsc);
        } else {
            Log.e(LOG_TAG,"copyMessageToIccEf iccSmsIntMgr is null" +
                          " for Subscription:"+subscription);
            return false;
        }
    }

    public List<SmsRawData> getAllMessagesFromIccEf(int subscription)
                throws android.os.RemoteException {
        MSimIccSmsInterfaceManager iccSmsIntMgr = getIccSmsInterfaceManager(subscription);
        if (iccSmsIntMgr != null) {
            return iccSmsIntMgr.getAllMessagesFromIccEf();
        } else {
            Log.e(LOG_TAG,"getAllMessagesFromIccEf iccSmsIntMgr is" +
                          " null for Subscription:"+subscription);
            return null;
        }
    }

    /**
     * Send a data based SMS to a specific application port.
     *
     * @param destAddr the address to send the message to
     * @param scAddr is the service center address or null to use
     *  the current default SMSC
     * @param destPort the port to deliver the message to
     * @param data the body of the message to send
     * @param sentIntent if not NULL this <code>PendingIntent</code> is
     *  broadcast when the message is successfully sent, or failed.
     *  The result code will be <code>Activity.RESULT_OK<code> for success,
     *  or one of these errors:<br>
     *  <code>RESULT_ERROR_GENERIC_FAILURE</code><br>
     *  <code>RESULT_ERROR_RADIO_OFF</code><br>
     *  <code>RESULT_ERROR_NULL_PDU</code><br>
     *  For <code>RESULT_ERROR_GENERIC_FAILURE</code> the sentIntent may include
     *  the extra "errorCode" containing a radio technology specific value,
     *  generally only useful for troubleshooting.<br>
     *  The per-application based SMS control checks sentIntent. If sentIntent
     *  is NULL the caller will be checked against all unknown applications,
     *  which cause smaller number of SMS to be sent in checking period.
     * @param deliveryIntent if not NULL this <code>PendingIntent</code> is
     *  broadcast when the message is delivered to the recipient.  The
     *  raw pdu of the status report is in the extended data ("pdu").
     */
    public void sendData(String destAddr, String scAddr, int destPort,
            byte[] data, PendingIntent sentIntent, PendingIntent deliveryIntent, int subscription) {
        MSimIccSmsInterfaceManager iccSmsIntMgr = getIccSmsInterfaceManager(subscription);
        if (iccSmsIntMgr != null) {
            iccSmsIntMgr.sendData(destAddr, scAddr, destPort, data, sentIntent, deliveryIntent);
        } else {
            Log.e(LOG_TAG,"sendText iccSmsIntMgr is null for" +
                          " Subscription:"+subscription);
        }
    }

    public void sendText(String destAddr, String scAddr,
            String text, PendingIntent sentIntent, PendingIntent deliveryIntent, int subscription) {
        MSimIccSmsInterfaceManager iccSmsIntMgr = getIccSmsInterfaceManager(subscription);
        if (iccSmsIntMgr != null) {
            iccSmsIntMgr.sendText(destAddr, scAddr, text, sentIntent, deliveryIntent);
        } else {
            Log.e(LOG_TAG,"sendText iccSmsIntMgr is null for" +
                          " Subscription:"+subscription);
        }
    }

    public void sendMultipartText(String destAddr, String scAddr, List<String> parts,
            List<PendingIntent> sentIntents, List<PendingIntent> deliveryIntents, int subscription)
                    throws android.os.RemoteException {
        MSimIccSmsInterfaceManager iccSmsIntMgr = getIccSmsInterfaceManager(subscription);
        if (iccSmsIntMgr != null ) {
            iccSmsIntMgr.sendMultipartText(destAddr, scAddr, parts, sentIntents, deliveryIntents);
        } else {
            Log.e(LOG_TAG,"sendMultipartText iccSmsIntMgr is null for" +
                          " Subscription:"+subscription);
        }
    }

    public boolean enableCellBroadcast(int messageIdentifier, int subscription)
                throws android.os.RemoteException {
        return enableCellBroadcastRange(messageIdentifier, messageIdentifier, subscription);
    }

    public boolean enableCellBroadcastRange(int startMessageId, int endMessageId, int subscription)
                throws android.os.RemoteException {
        MSimIccSmsInterfaceManager iccSmsIntMgr = getIccSmsInterfaceManager(subscription);
        if (iccSmsIntMgr != null ) {
            return iccSmsIntMgr.enableCellBroadcastRange(startMessageId, endMessageId);
        } else {
            Log.e(LOG_TAG,"enableCellBroadcast iccSmsIntMgr is null for" +
                          " Subscription:"+subscription);
        }
        return false;
    }

    public boolean disableCellBroadcast(int messageIdentifier, int subscription)
                throws android.os.RemoteException {
        return disableCellBroadcastRange(messageIdentifier, messageIdentifier, subscription);
    }

    public boolean disableCellBroadcastRange(int startMessageId, int endMessageId, int subscription)
                throws android.os.RemoteException {
        MSimIccSmsInterfaceManager iccSmsIntMgr = getIccSmsInterfaceManager(subscription);
        if (iccSmsIntMgr != null ) {
            return iccSmsIntMgr.disableCellBroadcastRange(startMessageId, endMessageId);
        } else {
            Log.e(LOG_TAG,"disableCellBroadcast iccSmsIntMgr is null for" +
                          " Subscription:"+subscription);
        }
       return false;
    }

    /**
     * get sms interface manager object based on subscription.
     **/
    private MSimIccSmsInterfaceManager getIccSmsInterfaceManager(int subscription) {
        try {
            return (MSimIccSmsInterfaceManager)
                ((MSimPhoneProxy)mPhone[subscription]).getIccSmsInterfaceManager();
        } catch (NullPointerException e) {
            Log.e(LOG_TAG, "Exception is :"+e.toString()+" For subscription :"+subscription );
            e.printStackTrace(); //This will print stact trace
            return null;
        } catch (ArrayIndexOutOfBoundsException e) {
            Log.e(LOG_TAG, "Exception is :"+e.toString()+" For subscription :"+subscription );
            e.printStackTrace(); //This will print stack trace
            return null;
        }
    }

    /**
       Gets User preferred SMS subscription */
    public int getPreferredSmsSubscription() {
        return MSimPhoneFactory.getSMSSubscription();
    }
}
