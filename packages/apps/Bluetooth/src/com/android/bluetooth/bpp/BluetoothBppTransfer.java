/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *        * Redistributions of source code must retain the above copyright
 *           notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above
 *          copyright notice, this list of conditions and the following
 *           disclaimer in the documentation and/or other materials provided
 *           with the distribution.
 *        * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *           contributors may be used to endorse or promote products derived
 *           from this software without specific prior written permission.
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
package com.android.bluetooth.bpp;

import javax.btobex.ObexTransport;

import com.android.bluetooth.opp.BluetoothOppTransfer;
import com.android.bluetooth.opp.BluetoothOppUtility;
import com.android.bluetooth.opp.BluetoothShare;
import com.android.bluetooth.opp.Constants;
import com.android.bluetooth.opp.BluetoothOppShareInfo;
import com.android.bluetooth.opp.BluetoothOppBatch;
import com.android.bluetooth.opp.BluetoothOppSendFileInfo;
import com.android.bluetooth.opp.BluetoothOppService;
import com.android.bluetooth.opp.BluetoothOppManager;

import android.app.NotificationManager;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.BluetoothUuid;
import android.os.ParcelUuid;
import android.content.BroadcastReceiver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.Parcelable;
import android.os.PowerManager;
import android.util.Log;
import android.widget.Toast;
import android.app.Activity;
import android.database.Cursor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.IOException;

/**
 * This class run an actual BPP transfer session (from connect target device to
 * disconnect)
 * It has a Thread and event handler to handle event from BluetoothBppObexClientSession
 * or BluetoothBppEvent class.
 * It also has Broadcast receiver for BT shutdown or ACL disconnect to handle unexpected
 * event.
 * This class also control entire BPP operation process.
 */
public class BluetoothBppTransfer implements BluetoothOppBatch.BluetoothOppBatchListener {
    private static final String TAG = "BluetoothBppTransfer";

    private static final boolean D = BluetoothBppConstant.DEBUG;

    private static final boolean V = BluetoothBppConstant.VERBOSE;

    public static final int RFCOMM_ERROR = 10;

    public static final int RFCOMM_CONNECTED = 11;

    public static final int RFCOMM_CONNECTED_EVT = 12;

    public static final int SDP_RESULT = 13;

    public static final int RFCOMM_CONNECT = 14;

    public static final int GET_PRINTER_ATTR = 15;

    public static final int CREATE_JOB = 16;

    public static final int GET_EVENT = 17;

    public static final int SEND_DOCUMENT = 18;

    public static final int START_EVENT_THREAD = 19;

    public static final int CANCEL = 20;

    public static final int MSG_OBEX_AUTH_CHALL = 21;

    public static final int USER_TIMEOUT = 22;

    public static final int STATUS_CON_CHANGE = 23;

    public static final int FILE_SENDING = 24;

    public static final int STATUS_UPDATE = 25;

    /* GetEvent Messages */
    public static final int EVENT_GETEVENT_RESPONSE = 30;

    private static final int USER_CONFIRM_TIMEOUT_VALUE = 30000;

    public static final String USER_CONFIRM_TIMEOUT_ACTION =
            "com.android.bluetooth.bpp.userconfirmtimeout";

    private static Context mContext;

    static private BluetoothBppAuthenticator mAuth = null;

    private BluetoothAdapter mAdapter;

    public BluetoothOppBatch mBatch;

    public BluetoothBppObexClientSession mSession;

    public BluetoothBppEvent mSessionEvent;

    private BluetoothOppShareInfo mCurrentShare;

    private ObexTransport mTransport;

    private ObexTransport mTransportEvent;

    private HandlerThread mHandlerThread;

    EventHandler mSessionHandler = null;

    public int JobChannel = -1;

    public int StatusChannel = -1;

    boolean bonding_process;

    private PowerManager mPowerManager;

    private long mTimestamp;

    public boolean mForceClose = false;

    private volatile boolean mBPPregisterReceiver = false;

    public boolean mTransferCancelled = false;

    String PrintResultMsg = null;

    public boolean mAuthChallProcess = false;

    public int mStatusFinal = 0;

    public BluetoothBppTransfer(Context context, PowerManager powerManager,
            BluetoothOppBatch batch, BluetoothBppObexClientSession session) {

        mContext = context;
        mPowerManager = powerManager;
        mBatch = batch;
        mForceClose = false;
        mTransferCancelled = false;
        PrintResultMsg = null;
        mAuthChallProcess = false;
        mStatusFinal = 0;

        mBatch.registerListern(this);
        mAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    private final BroadcastReceiver mBluetoothReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();

            if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
                switch (intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)) {
                    case BluetoothAdapter.STATE_TURNING_OFF:
                        if (V) Log.v(TAG, "Received DISABLED_ACTION");
                        ForceCloseBpp();
                        break;
                }
            }
            else if (action.equals(BluetoothDevice.ACTION_ACL_DISCONNECTED)) {
                BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                if (device == null) {
                    Log.e(TAG, "Receive ACTION_ACL_DISCONNECTED, device null");
                    return;
                }
                    if (V) Log.v(TAG, "ACTION_ACL_DISCONNECTED for device " + device
                        + "- Printer device: " + mBatch.mDestination);
                    if (device.equals(mBatch.mDestination) && mBPPregisterReceiver) {
                        if (V) Log.v(TAG, "Received ACTION_ACL_DISCONNECTED - batch: " + mBatch.mId);
                        mStatusFinal = BluetoothShare.STATUS_BPP_DISCONNECTED;
                        ForceCloseBpp();
                        printResultMsg();
                }
            }
        }
    };

    private void ForceCloseBpp(){
        if (V) Log.v(TAG, "BPP Operation got Emergency Stop!!");
        mForceClose = true;

        if (BluetoothOppService.mBppTransfer.size() > 0) {
           mStatusFinal = BluetoothShare.STATUS_BPP_DISCONNECTED;
           markBatchResult(mStatusFinal);
        }

        mSessionHandler.obtainMessage(BluetoothBppTransfer.CANCEL, -1).sendToTarget();

        if(BluetoothBppStatusActivity.mContext != null){
            ((Activity) BluetoothBppStatusActivity.mContext).finish();
        }
        if(BluetoothBppPrintPrefActivity.mContext != null){
            ((Activity) BluetoothBppPrintPrefActivity.mContext).finish();
        }
        if(BluetoothBppSetting.mContext != null){
            ((Activity) BluetoothBppSetting.mContext).finish();
        }
        if(BluetoothBppActivity.mContext != null){
            ((Activity) BluetoothBppActivity.mContext).finish();
        }
    }

    public BluetoothBppTransfer(Context context, PowerManager powerManager, BluetoothOppBatch batch) {
        this(context, powerManager, batch, null);
    }

    public int getBatchId() {
        return mBatch.mId;
    }

    /*
     * Receives events from mConnectThread & mSession back in the main thread.
     */
    public class EventHandler extends Handler {
        public EventHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case SDP_RESULT:
                    if (V) Log.v(TAG, "SDP request returned " + msg.arg1 + " (" +
                            (System.currentTimeMillis() - mTimestamp + " ms)"));
                    if (!((BluetoothDevice)msg.obj).equals(mBatch.mDestination)) {
                        return;
                    }
                    try {
                        mContext.unregisterReceiver(mReceiver);
                    } catch (IllegalArgumentException e) {
                        // ignore
                    }
                    if (msg.arg1 > 0) {
                        Intent in = new Intent(mContext, BluetoothBppActivity.class);
                        in.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        in.putExtra("jobCh", msg.arg1);
                        in.putExtra("statCh", msg.arg2);
                        mContext.startActivity(in);
                    } else {
                        /* SDP query fail case */
                        Log.e(TAG, "SDP query failed!");
                        mStatusFinal = BluetoothShare.STATUS_CONNECTION_ERROR;
                        printResultMsg();
                    }
                    break;
                case RFCOMM_CONNECT:
                    if (V) Log.v(TAG, "Get RFCOMM_CONNECT Message");
                    if (msg.arg1 > 0) {
                        mConnectThread = new SocketConnectThread(mBatch.mDestination, JobChannel);
                        mConnectThread.start();

                    } else {
                        /* SDP query fail case */
                        Log.e(TAG, "SDP query failed!");
                        mStatusFinal = BluetoothShare.STATUS_CONNECTION_ERROR;
                        printResultMsg();
                    }
                    break;
                /*
                 * RFCOMM connect fail is for outbound share only! Mark batch
                 * failed, and all shares in batch failed
                 */
                case RFCOMM_ERROR:
                    if (V) Log.v(TAG, "receive RFCOMM_ERROR msg");
                    if(mConnectThread.isAlive()){
                        try {
                            mConnectThread.interrupt();
                            mConnectThread.join();
                        }catch (InterruptedException e) {
                            if (V) Log.v(TAG, "EXP: RFCOMM_ERROR waiting to join");
                        }
                    }
                    BluetoothBppActivity.mOPPstop = false;
                    if(BluetoothBppActivity.mContext != null){
                        ((Activity) BluetoothBppActivity.mContext).finish();
                    }
                    while( true ) {
                        if(BluetoothBppActivity.mContext != null){
                            try {
                                if(V) Log.v(TAG," RFCOMM_ERROR waiting for 10 sec");
                                Thread.sleep(10);
                            }catch (InterruptedException e) {
                                if(V) Log.v(TAG,"  Thread Interrupted");
                            }
                        }else {
                            break;
                        }
                    }
                    mStatusFinal = BluetoothShare.STATUS_CONNECTION_ERROR;
                    printResultMsg();
                    break;
                /*
                 * RFCOMM connected is for outbound share only! Create
                 * BluetoothOppObexClientSession and start it
                 */
                case RFCOMM_CONNECTED:
                    if (V) Log.v(TAG, "Transfer receive RFCOMM_CONNECTED msg checking IsAlive()");
                    mTransport = (ObexTransport)msg.obj;
                    startObexSession();
                    break;

                case RFCOMM_CONNECTED_EVT:
                    if (V) Log.v(TAG, "Transfer receive RFCOMM_CONNECTED_EVT msg");
                    mTransportEvent = (ObexTransport)msg.obj;
                    startEventThread();
                    break;

                case GET_PRINTER_ATTR:
                    if (V) Log.v(TAG, "Transfer receive GET_PRINTER_ATTR msg");
                    mSession.mSoapProcess = BluetoothBppObexClientSession.GETATTRIBUTE;
                    break;

                case CREATE_JOB:
                    if (V) Log.v(TAG, "Transfer receive CREATE_JOB msg");
                    mSession.mSoapProcess = BluetoothBppObexClientSession.CREATJOB;
                    break;

                case GET_EVENT:
                    if (V) Log.v(TAG, "Transfer receive GET_EVENT msg");
                    mSession.mSoapProcess = BluetoothBppObexClientSession.GETEVENT;
                    break;

                case SEND_DOCUMENT:
                    if (V) Log.v(TAG, "Transfer receive SEND_DOCUMENT msg");
                    mSession.mSoapProcess = BluetoothBppObexClientSession.SENDDOCUMENT;
                    break;

                case CANCEL:
                    if (V) Log.v(TAG, "Transfer receive CANCEL msg: " + msg.arg1);
                    if (mSession != null) {
                        mSession.mSoapProcess = BluetoothBppObexClientSession.CANCEL;
                    }
                    if (mSessionEvent != null ) {
                        mSessionEvent.bs.mJobStatus = "cancelled";
                    }
                    if ((msg.arg1 > 0) && (mStatusFinal != BluetoothShare.STATUS_CANCELED) ) {
                        mStatusFinal = msg.arg1;
                    }
                    break;

                case START_EVENT_THREAD:
                    if (V) Log.v(TAG, "Transfer receive START_EVENT_THREAD msg");
                    if(StatusChannel == -1){
                        if (V) Log.v(TAG, "Status RFCOMM channel has not been set correctly");
                        return;
                    }
                    mConnectThreadEvent = new SocketConnectThread(mBatch.mDestination,
                                                                    StatusChannel);
                    mConnectThreadEvent.start();
                    break;

                case MSG_OBEX_AUTH_CHALL:
                    if (V) Log.v(TAG, "Transfer receive MSG_OBEX_AUTH_CHALL msg");
                    mAuthChallProcess = true;
                    Intent in2 = new Intent(mContext, BluetoothBppAuthActivity.class);
                    in2.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    mContext.startActivity(in2);
                    mSessionHandler.sendMessageDelayed(mSessionHandler
                        .obtainMessage(USER_TIMEOUT), USER_CONFIRM_TIMEOUT_VALUE);
                    break;

                case USER_TIMEOUT:
                    if (V) Log.v(TAG, "Transfer receive USER_TIMEOUT msg");
                    Intent intent = new Intent(USER_CONFIRM_TIMEOUT_ACTION);
                    mContext.sendBroadcast(intent);
                    break;
                case STATUS_CON_CHANGE:
                    if (V) Log.v(TAG, "Transfer receive STATUS_CON_CHANGE msg - " + msg.arg1);
                    mSession.mEventConn =  msg.arg1;
                    break;
                case FILE_SENDING:
                    if (V) Log.v(TAG, "Transfer receive FILE_SENDING msg - " + msg.arg1);
                    if(mSessionEvent != null) {
                        mSessionEvent.bs.mFileSending = (msg.arg1==0)? false:true;
                    }
                    break;
                case STATUS_UPDATE:
                    if (V) Log.v(TAG, "Transfer receive STATUS_UPDATE msg - " + msg.arg1);
                    if (mStatusFinal == 0) {
                        mStatusFinal = msg.arg1;
                    }
                    break;
                /*
                 * Put next share if available,or finish the transfer.
                 * For outbound session, call session.addShare() to send next file,
                 * or call session.stop().
                 * For inbounds session, do nothing. If there is next file to receive,it
                 * will be notified through onShareAdded()
                 */
                case BluetoothBppObexClientSession.MSG_SHARE_COMPLETE:
                    BluetoothOppShareInfo info = (BluetoothOppShareInfo)msg.obj;
                    if (V) Log.v(TAG, "receive MSG_SHARE_COMPLETE for info " + info.mId);
                    if (mStatusFinal == 0) {
                        mStatusFinal = msg.arg1;
                    }
                    if (mBatch.mDirection == BluetoothShare.DIRECTION_OUTBOUND) {
                        BluetoothOppShareInfo newShare = mBatch.getPendingShare();

                        if (newShare != null) {
                            /* we have additional share to process */
                            if (V) Log.v(TAG, "continue session for info " + mCurrentShare.mId +
                                    " from batch " + mBatch.mId);
                            mCurrentShare = newShare;
                            processCurrentShare();
                        } else {
                            /* for outbound transfer, all shares are processed */
                            if (V) Log.v(TAG, "Batch " + mBatch.mId + " is done");

                            /* In case of JobStatus got completed before actual printing is done,
                               it need to stop event Thread here !!
                             */
                            if ((mSessionEvent.bs.mJobStatus.compareTo("completed")== 0)
                                && mSessionEvent.mConnected){
                                mSessionEvent.stop();
                            } else if ((mSessionEvent.mConnected == false)&&(mSession != null)) {
                                mSession.stop();
                            }
                        }
                    }
                    break;

                case BluetoothBppObexClientSession.MSG_SESSION_EVENT_COMPLETE:
                    if (V) Log.v(TAG, "receive MSG_SESSION_EVENT_COMPLETE");
                    /* Disconnect Status Channel */
                    if (msg.arg1 > 0) {
                        mSessionEvent.mEnforceClose = true;
                    }
                    if (mForceClose) {
                        mBatch.mStatus = Constants.BATCH_STATUS_FAILED;
                    }
                    mSessionEvent.stop();
                    break;

                case BluetoothBppObexClientSession.MSG_SESSION_STOP:
                    if (V) Log.v(TAG, "receive MSG_SESSION_STOP");
                    /* Disconnect Job Channel */
                    if ((!mSessionEvent.bs.mFileSending || mSessionEvent.mEnforceClose
                         || mForceClose) && (mSession != null)) {
                        mSession.stop();
                    }
                    break;
              /*
               * Handle session completed status Set batch status to
               * finished
               */
                case BluetoothBppObexClientSession.MSG_SESSION_COMPLETE:
                    BluetoothOppShareInfo info1 = (BluetoothOppShareInfo)msg.obj;
                    if (V) Log.v(TAG, "receive MSG_SESSION_COMPLETE for batch " + mBatch.mId);

                    synchronized (this) {
                        try {
                            if(mBPPregisterReceiver){
                                mBPPregisterReceiver = false;
                                mContext.unregisterReceiver(mBluetoothReceiver);
                                if (V) Log.v(TAG, "mBluetoothReceiver - " + mBluetoothReceiver );
                            }
                        } catch (IllegalArgumentException e) {
                            //Ignore
                        }
                    }

                    BluetoothBppActivity.mOPPstop = false;
                    BluetoothBppPrintPrefActivity.mOPPstop = false;

                    if (mStatusFinal == 0 ) {
                        mStatusFinal = info1.mStatus;
                    }

                    if(BluetoothBppStatusActivity.mContext != null) {
                        ((Activity) BluetoothBppStatusActivity.mContext).finish();
                    }
                    if(BluetoothBppPrintPrefActivity.mContext != null){
                        ((Activity) BluetoothBppPrintPrefActivity.mContext).finish();
                    }
                    if(BluetoothBppSetting.mContext != null){
                        ((Activity) BluetoothBppSetting.mContext).finish();
                    }
                    if(BluetoothBppActivity.mContext != null){
                        ((Activity) BluetoothBppActivity.mContext).finish();
                    }
                    while( true ) {
                        if((BluetoothBppActivity.mContext != null)&&
                          (BluetoothBppSetting.mContext != null)&&
                          (BluetoothBppPrintPrefActivity.mContext != null)&&
                          (BluetoothBppStatusActivity.mContext != null)){
                            try {
                                if(V) Log.v(TAG," MSG_SESSION_COMPLETE Thread Waiting for 10 ms");
                                Thread.sleep(10);
                            }catch (InterruptedException e) {
                                if(V) Log.v(TAG,"  Thread Interrupted");
                            }
                        }else {
                            break;
                        }
                    }
                    printResultMsg();
                    break;

                /* Handle the error state of an Obex session */
                case BluetoothBppObexClientSession.MSG_SESSION_ERROR:
                    if (V) Log.v(TAG, "receive MSG_SESSION_ERROR for batch " + mBatch.mId);

                    /* In case that error happened between Start Obex session
                     * and GetPrinterAttribute response, it should close BluetoothActivity.class
                     */
                    if(BluetoothBppActivity.mContext != null){
                        ((Activity) BluetoothBppActivity.mContext).finish();
                    }
                    while( true ) {
                        if(BluetoothBppActivity.mContext != null) {
                            try {
                                if(V) Log.v(TAG," MSG_SESSION_ERROR Thread Waiting for 10 ms");
                                Thread.sleep(10);
                            }catch (InterruptedException e) {
                                if(V) Log.v(TAG,"  Thread Interrupted");
                            }
                       } else {
                            break;
                        }
                    }
                    BluetoothOppShareInfo info2 = (BluetoothOppShareInfo)msg.obj;
                    if (mStatusFinal == 0) {
                        mStatusFinal = info2.mStatus;
                    }
                    if ( mSessionEvent != null) {
                        if (mSessionEvent.mConnected == true) {
                            mSessionEvent.mEnforceClose = true;
                            mSessionEvent.stop();
                        } else if(mSession != null){
                            mSession.stop();
                        }
                    }else{
                        if (V) Log.v(TAG, "Event Thread has not been started,"+
                            " so just close obexsession");
                        if(mSession != null)
                            mSession.stop();
                    }
                    break;

                case BluetoothBppObexClientSession.MSG_SHARE_INTERRUPTED:
                    if (V) Log.v(TAG, "receive MSG_SHARE_INTERRUPTED for batch " + mBatch.mId);
                    BluetoothOppShareInfo info3 = (BluetoothOppShareInfo)msg.obj;
                    if (mStatusFinal == 0) {
                        mStatusFinal = info3.mStatus;
                    }
                    if (mBatch.mDirection == BluetoothShare.DIRECTION_OUTBOUND) {
                        try {
                            if (mTransport == null) {
                                Log.v(TAG, "receive MSG_SHARE_INTERRUPTED but mTransport = null");
                            } else {
                                mTransport.close();
                            }
                        } catch (IOException e) {
                            Log.e(TAG, "failed to close mTransport");
                        }
                        if (V) Log.v(TAG, "mTransport closed ");
                        printResultMsg();
                    }
                    break;

                case BluetoothBppObexClientSession.MSG_CONNECT_TIMEOUT:
                    if (V) Log.v(TAG, "receive MSG_CONNECT_TIMEOUT for batch " + mBatch.mId);
                    /* for outbound transfer, the block point is BluetoothSocket.write()
                     * The only way to unblock is to tear down lower transport
                     * */
                    if (mBatch.mDirection == BluetoothShare.DIRECTION_OUTBOUND) {
                        try {
                            if (mTransport == null) {
                                Log.v(TAG, "receive MSG_SHARE_INTERRUPTED but mTransport = null");
                            } else {
                                mTransport.close();
                            }
                        } catch (IOException e) {
                            Log.e(TAG, "failed to close mTransport");
                        }
                        if (V) Log.v(TAG, "mTransport closed ");
                        if (mStatusFinal == 0) {
                            mStatusFinal = BluetoothShare.STATUS_CONNECTION_ERROR;
                        }
                        printResultMsg();
                    }
                    break;
            }
        }
    }

    public void printResultMsg(){
        if (mForceClose) return;
        if (V) Log.v(TAG, "printResultMsg: " + mStatusFinal);
        markBatchResult(mStatusFinal);
        /* Show AlertDiaglog Message */
        mStatusFinal = (mStatusFinal==200)?(BluetoothShare.STATUS_BPP_SUCCESS): mStatusFinal;
        String msg = BluetoothOppUtility.getStatusDescription(mContext, mStatusFinal, null);
        Toast.makeText(mContext, msg, Toast.LENGTH_SHORT).show();
    }

    private void markShareTimeout(BluetoothOppShareInfo share) {
        Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + share.mId);
        ContentValues updateValues = new ContentValues();
        updateValues
                .put(BluetoothShare.USER_CONFIRMATION, BluetoothShare.USER_CONFIRMATION_TIMEOUT);
        mContext.getContentResolver().update(contentUri, updateValues, null, null);
    }

    public void markBatchResult(int statusCode) {
        BluetoothOppShareInfo info = mBatch.getPendingShare();
        if (info == null) {
            info = mCurrentShare;
        }
        if (V) Log.v(TAG, "info: " + info+ ","+ mCurrentShare);
        if (info != null) {
            if (statusCode == BluetoothShare.STATUS_SUCCESS) {
                mBatch.mStatus = Constants.BATCH_STATUS_FINISHED;
            } else {
                mBatch.mStatus = Constants.BATCH_STATUS_FAILED;
            }
            info.mStatus = statusCode;
            Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + info.mId);
            ContentValues updateValues = new ContentValues();
            updateValues.put(BluetoothShare.STATUS, info.mStatus);

            BluetoothOppSendFileInfo fileInfo = BluetoothOppSendFileInfo.generateFileInfo(mContext,
                    info.mUri, info.mMimetype, info.mDestination);
            if (fileInfo.mFileName != null) {
                updateValues.put(BluetoothShare.FILENAME_HINT, fileInfo.mFileName);
                updateValues.put(BluetoothShare.TOTAL_BYTES, fileInfo.mLength);
                updateValues.put(BluetoothShare.MIMETYPE, fileInfo.mMimetype);
            }
            mContext.getContentResolver().update(contentUri, updateValues, null, null);
        }
    }


    private void markInfoStatus(BluetoothOppShareInfo info, int status) {
        if (info == null) {
            Log.e(TAG, "info is null!!");
            return;
        }
        if (D) Log.d(TAG, "Mark ShareInfo in the batch as " + status );
        Constants.updateShareStatus(mContext, info.mId, status);
    }

    /*
     * NOTE
     * For outbound transfer
     * 1) Check Bluetooth status
     * 2) Start handler thread
     * 3) new a thread to connect to target device
     * 3.1) Try a few times to do SDP query for target device OPUSH channel
     * 3.2) Try a few seconds to connect to target socket
     * 4) After BluetoothSocket is connected,create an instance of RfcommTransport
     * 5) Create an instance of BluetoothOppClientSession
     * 6) Start the session and process the first share in batch
     * For inbound transfer
     * The transfer already has session and transport setup, just start it
     * 1) Check Bluetooth status
     * 2) Start handler thread
     * 3) Start the session and process the first share in batch
     */
    /**
     * Start the transfer
     */
    public void start() {
        /* Set all pending Share file to Canel Status in case of Device shut down without intention
         */
        markInfoStatus(mBatch.getPendingShare(), BluetoothShare.STATUS_QUEUE);

        /* check Bluetooth enable status */
        /*
         * normally it's impossible to reach here if BT is disabled. Just check
         * for safety
         */
        if (!mAdapter.isEnabled()) {
            Log.e(TAG, "Can't start transfer when Bluetooth is disabled for " + mBatch.mId);
            mStatusFinal = BluetoothShare.STATUS_PRECONDITION_FAILED;
            printResultMsg();
            return;
        }

        if (mHandlerThread == null) {
            if (V) Log.v(TAG, "Create handler thread for batch " + mBatch.mId);
            mHandlerThread = new HandlerThread("BtBpp Transfer Handler");
            mHandlerThread.start();
            mSessionHandler = new EventHandler(mHandlerThread.getLooper());

            if (mBatch.mDirection == BluetoothShare.DIRECTION_OUTBOUND) {
                /* for outbound transfer, we do connect first */
                startSDPSession();
            } else if (mBatch.mDirection == BluetoothShare.DIRECTION_INBOUND) {
                if (V) Log.v(TAG, "BPP doesn't support Server !!");
            }
        } else {
            if(V) Log.v(TAG,"  Start: Thread Already there");
        }
    }

    /**
     * Stop the transfer
     */
    public void stop() {
        if (V) Log.v(TAG, "stop");
        if((mConnectThread!= null)&&(mConnectThread.isAlive())) {
            try {
                mConnectThread.interrupt();
                if (V) Log.v(TAG, "waiting for connect thread to terminate");
                mConnectThread.join();
            } catch (InterruptedException e) {
                if (V) Log.v(TAG, "EXP: stop() waiting to join");
            }
        }
        if (mSession != null) {
            if (V) Log.v(TAG, "Stop mSession");
            mSession.stop();
            mSession = null;
        }
        if (mHandlerThread != null) {
            if (V) Log.v(TAG, " Stopping mHandlerThread !!");
            /* TO-DO --- need to think about how to handle this..*/
            //mHandlerThread.getLooper().quit();
            mHandlerThread.interrupt();
            mHandlerThread = null;
            mSessionHandler = null;
        }

        synchronized (this) {
            try {
                if(mBPPregisterReceiver){
                    mBPPregisterReceiver = false;
                    mContext.unregisterReceiver(mBluetoothReceiver);
                    if (V) Log.v(TAG, "mBluetoothReceiver - " + mBluetoothReceiver );
                }
            } catch (IllegalArgumentException e) {
                //Ignore
            }
        }
    }

    private void startObexSession() {

        mBatch.mStatus = Constants.BATCH_STATUS_RUNNING;

        mCurrentShare = mBatch.getPendingShare();
        if (mCurrentShare == null) {
            Log.e(TAG, "Unexpected error happened !");
            // It should disconnect RFCOMM channel right away.
            if(mConnectThread.isAlive()) {
            try {
                mConnectThread.interrupt();
                mConnectThread.join();
            }catch (InterruptedException e) {
                if (V) Log.v(TAG, "EXP: StartObexSession()waiting to join");
            }
            }
            return;
        }
        if (V) Log.v(TAG, "Start session for info " + mCurrentShare.mId + " for batch " +
                mBatch.mId);

        if (mBatch.mDirection == BluetoothShare.DIRECTION_OUTBOUND) {
            if (V) Log.v(TAG, "Create Client session with transport " + mTransport.toString());
            mSession = new BluetoothBppObexClientSession(mContext, mTransport);
        } else if (mBatch.mDirection == BluetoothShare.DIRECTION_INBOUND) {
            if (V) Log.v(TAG, "BPP doesn't support server");
            return;
        }

        synchronized (this) {
            mAuth = new BluetoothBppAuthenticator(mSessionHandler);
            mAuth.setChallenged(false);
            mAuth.setCancelled(false);
            mConnectThreadEvent = null;
        }

        mSession.start(mSessionHandler, mAuth);
        processCurrentShare();

        /* OBEX channel need to be monitored for unexpected ACL disconnection
         * such as Printer power off
         */
        synchronized (this) {
            try {
                IntentFilter filter = new IntentFilter();
                filter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
                filter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED);
                mContext.registerReceiver(mBluetoothReceiver, filter);
                mBPPregisterReceiver = true;
                if (V) Log.v(TAG, "mBluetoothReceiver is registered !!");
            } catch (IllegalArgumentException e) {
                //Ignore
            }
        }
    }

    static public void notifyAuthKeyInput(final String key) {
        synchronized (mAuth) {
            if (key != null) {
                mAuth.setSessionKey(key);
            }
            mAuth.setChallenged(true);
            mAuth.notify();
        }
        Intent intent = new Intent(USER_CONFIRM_TIMEOUT_ACTION);
        mContext.sendBroadcast(intent);
    }

    static public void notifyAuthCancelled() {
        synchronized (mAuth) {
            mAuth.setCancelled(true);
            mAuth.notify();
        }
        Intent intent = new Intent(USER_CONFIRM_TIMEOUT_ACTION);
        mContext.sendBroadcast(intent);
    }


    public String getRemoteDeviceName(){
        if(mBatch.mDestination != null){
            return mBatch.mDestination .getName();
        }
        return null;
    }

    private void startEventThread() {

        if (V) Log.v(TAG, "Create Client session with transport " + mTransportEvent.toString());
        mSessionEvent = new BluetoothBppEvent(mContext, mTransportEvent);
        mSessionEvent.start(mSessionHandler, mSession.bs.mPrinter_JobId);
    }


    private void processCurrentShare() {
        /* This transfer need user confirm */
        if (V) Log.v(TAG, "processCurrentShare" + mCurrentShare.mId);
        mSession.addShare(mCurrentShare);
    }

    /**
     * Set transfer confirmed status. It should only be called for inbound
     * transfer
     */
    public void setConfirmed() {
        /* unblock server session */
        final Thread notifyThread = new Thread("Server Unblock thread") {
            public void run() {
                synchronized (mSession) {
                    mSession.unblock();
                    mSession.notify();
                }
            }
        };
        if (V) Log.v(TAG, "setConfirmed to unblock mSession" + mSession.toString());
        notifyThread.start();
    }

    private void startSDPSession() {
        if (V) Log.v(TAG, "startSDPSession");
        BluetoothBppPreference INSTANCE = null;
        INSTANCE = BluetoothBppPreference.getInstance(mContext);
        if (INSTANCE == null) {
            Log.e(TAG, "Unexpected error! Instance is null");
            if (mSessionHandler != null) {
                mSessionHandler.obtainMessage(SDP_RESULT, -1, -1,
                                        mBatch.mDestination).sendToTarget();
            } else {
                Log.e(TAG, "Error! mSessionHandler is null");
            }
            return;
        }
        JobChannel = INSTANCE.getChannel(
                    mBatch.mDestination, BluetoothBppConstant.DPS_UUID16);

        StatusChannel = INSTANCE.getChannel(
                    mBatch.mDestination, BluetoothBppConstant.STS_UUID16);

        String docFormats = INSTANCE.getFeatures(
                    mBatch.mDestination);

        if(IsNotSupportedDocFormats(docFormats)) return;

        if ((JobChannel != -1) && (StatusChannel !=-1) && (docFormats != null)) {
            if (D) Log.d(TAG, "Get BPP Job channel: " + JobChannel +
                ", Status Channel: " + StatusChannel + " from cache for " +
                mBatch.mDestination);
            mTimestamp = System.currentTimeMillis();
            if (mSessionHandler != null) {
                mSessionHandler.obtainMessage(SDP_RESULT, JobChannel, StatusChannel,
                                        mBatch.mDestination).sendToTarget();
            } else {
                Log.e(TAG, "Error! mSessionHandler is null");
            }
        } else {
            doBppSdp();
        }
    }

    private void doBppSdp() {
        if (V) Log.v(TAG, "Do BPP SDP request for address " + mBatch.mDestination);

        mTimestamp = System.currentTimeMillis();
        String docFormats = mBatch.mDestination.getFeature("SupportedFormats");
        if(IsNotSupportedDocFormats(docFormats)) return;

        JobChannel = mBatch.mDestination.getServiceChannel(BluetoothUuid.DirectPrinting);
        StatusChannel = mBatch.mDestination.getServiceChannel(BluetoothUuid.PrintingStatus);

        if ((JobChannel != -1) && (StatusChannel !=-1) && (docFormats != null)) {
            if (D) Log.d(TAG, "Get BPP Job channel: " + JobChannel +
                ", Status Channel: " + StatusChannel + " from SDP for " +
                mBatch.mDestination);

            if (mSessionHandler != null) {
                mSessionHandler.obtainMessage(SDP_RESULT, JobChannel, StatusChannel,
                                        mBatch.mDestination).sendToTarget();
            } else {
                Log.e(TAG, "Error! mSessionHandler is null");
            }

            return;

        } else {
            if (V) Log.v(TAG, "Remote Service channel not in cache");

            if (!mBatch.mDestination.fetchUuidsWithSdp()) {
                Log.e(TAG, "Start SDP query failed");
            } else {
                // we expect framework send us Intent ACTION_UUID. otherwise we will fail
                if (V) Log.v(TAG, "Start new SDP, wait for result");
                bonding_process = false;
                IntentFilter filter = new IntentFilter();
                filter.addAction(BluetoothDevice.ACTION_UUID);
                filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
                mContext.registerReceiver(mReceiver, filter);
                return;
            }
        }
        if (mSessionHandler != null) {
            Message msg = mSessionHandler.obtainMessage(SDP_RESULT, JobChannel,
                                                        StatusChannel, mBatch.mDestination);
            mSessionHandler.sendMessageDelayed(msg, 2000);
        } else {
            Log.e(TAG, "Error! mSessionHandler is null");
        }
    }

    /**
     * This function check the MIME type of the current file and compare with Printer supported
     * formats. If the MIME type doesn't match with the printer supported formats, it will invoke
     * OPP operation instead of BPP.
     * @param docFormats Printer supported document formats.
     * @return If true, BPP operation will be finished.
     */
    private boolean IsNotSupportedDocFormats(String docFormats){
        // Overwriting previous value with cache from BluetoothDevice
        String extractMime;
        BluetoothBppConstant.mSupportedDocs = docFormats;

        if(docFormats != null){
            if (V) Log.v(TAG, "Printer supports doc format: " + docFormats);
            String currFileType = mBatch.getPendingShare().mMimetype;
            String currUriName =  mBatch.getPendingShare().mUri;
            if (V) Log.v(TAG, "File Type: " + currFileType + "\r\nFile Name: "+ currUriName);
            String fileName = null;
            Uri u = Uri.parse(currUriName);
            String scheme = u.getScheme();
            if(V) Log.v(TAG,"Scheme = " + scheme);
            if (scheme.equals("content")) {
                    Cursor metadataCursor = mContext.getContentResolver().query(u, new String[] {
                             OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE
                            }, null, null, null);
                   if (metadataCursor != null) {
                     try {
                         if (metadataCursor.moveToFirst()) {
                             fileName = metadataCursor.getString(0);
                        }
                     } finally {
                           metadataCursor.close();
                       }
                 }
            } else if (scheme.equals("file")) {
                fileName = u.getLastPathSegment();
            }
            if ((currFileType == null) || ( docFormats.indexOf(currFileType, 0) == -1 )){
                if(V) Log.v(TAG," filename after change =   "+ fileName);
                if ((extractMime = checkUnknownMimetype(currFileType, fileName)) != null) {
                    if (V) Log.v(TAG, "Set the file type to " + extractMime);
                    if(docFormats.indexOf(extractMime, 0) != -1 ){
                        return false;
                    }
                }
                if (V) Log.v(TAG, "No match doc format, let OPP handle it !! - batch "
                    + mBatch.mId );
                this.stop();
                BluetoothOppService.mBppTransfer.remove(this);
                BluetoothOppService.mBppTransId--;
                mBatch.mOwner = BluetoothShare.OWNER_OPP;
                mBatch.mStatus = BluetoothShare.STATUS_PENDING;
                BluetoothOppShareInfo info = mBatch.getPendingShare();
                if (info == null) {
                    Log.e(TAG, "Error! info is null");
                    return false;
                }
                BluetoothOppService.markBatchOwnership(mContext, info.mId,
                    BluetoothShare.OWNER_OPP);

                if (BluetoothOppService.mTransfer == null) {
                    if (V) Log.v(TAG, "OPP Transfer Start!! - " + mBatch.mId );
                    BluetoothOppService.mTransfer =
                        new BluetoothOppTransfer(mContext, mPowerManager, mBatch);
                    if (BluetoothOppService.mTransfer != null) {
                        BluetoothOppService.mTransfer.start();
                    } else {
                        Log.e(TAG, "Error! mTransfer is null");
                    }
                }
                return true;
            }
        }
        return false;

    }

    static public String checkUnknownMimetype(String fileType, String fileName){
        String mimeType = null;
        if(fileType == null || fileName == null)
            return null;
        int dotIndex = fileName.lastIndexOf(".");
        String extension = fileName.substring(dotIndex + 1).toLowerCase();
        if(V) Log.v(TAG,"extension = " + extension);
        if (extension.equals("vcf")) {
            mimeType = "text/x-vcard";
        }else if (extension.equals("vcs")) {
            mimeType = "text/x-vcalendar";
        }else if (extension.equals("vmg")) {
            mimeType = "text/x-vmessage";
        }else if (extension.equals("ical")) {
            mimeType = "text/calendar";
        }else if (extension.equals("msg")) {
            mimeType = "text/x-vmessage";
        }else if (extension.equals("htm")) {
            mimeType = "application/vnd.pwg-xhtml-print+xml";
        }else if ((extension.equals("jpg"))||(extension.equals("jpeg"))
                ||(extension.equals("jpe"))||(extension.equals("jfif")) ) {
            mimeType = "image/jpeg";
        }else if (extension.equals("gif")) {
            mimeType = "image/gif";
        }else if (extension.equals("png")) {
            mimeType = "image/png";
        }else if ((extension.equals("tif"))||(extension.equals("tiff"))) {
            mimeType = "image/tiff";
        }
        return mimeType;
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(BluetoothDevice.ACTION_UUID)) {
                BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                if (device == null) {
                    Log.e(TAG, "Unexpected error! device is null");
                    return;
                }
                if (V) Log.v(TAG, "ACTION_UUID for device " + device + "requested device: "
                                                                        + mBatch.mDestination);
                if (device.equals(mBatch.mDestination)) {
                    Parcelable[] uuid = intent.getParcelableArrayExtra(BluetoothDevice.EXTRA_UUID);
                    if (uuid != null) {
                        ParcelUuid[] uuids = new ParcelUuid[uuid.length];
                        for (int i = 0; i < uuid.length; i++) {
                            uuids[i] = (ParcelUuid)uuid[i];
                        }
                        // Get Printer Supported Documents list
                        String docFormats = mBatch.mDestination.getFeature("SupportedFormats");
                        if(IsNotSupportedDocFormats(docFormats)){
                            try {
                                mContext.unregisterReceiver(mReceiver);
                            } catch (IllegalArgumentException e) {
                                // ignore
                            }
                            return;
                        }
                        // Get BPP DirectPrinting RFCOMM Channel number
                        if (BluetoothUuid.isUuidPresent(uuids, BluetoothUuid.DirectPrinting)) {
                            JobChannel = mBatch.mDestination
                                    .getServiceChannel(BluetoothUuid.DirectPrinting);

                            if (V) Log.v(TAG, "SDP get BPP DTS Channel: " + JobChannel);
                        }
                        // Get BPP Status RFCOMM Channel number
                        if (BluetoothUuid.isUuidPresent(uuids, BluetoothUuid.PrintingStatus)) {
                            StatusChannel = mBatch.mDestination
                                .getServiceChannel(BluetoothUuid.PrintingStatus);
                            if (V) Log.v(TAG, "SDP get BPP STS Channel: " + StatusChannel);
                        }
                        // Get BPP Status RFCOMM Channel number
                        if ( (StatusChannel == -1) && (JobChannel == -1) &&
                            (BluetoothUuid.isUuidPresent(uuids, BluetoothUuid.ObexObjectPush))) {
                            if (V) Log.v(TAG, "This printer doesn't support BPP, but OPP"
                                + " so, let's start OPP");

                            if (V) Log.v(TAG, "mBppTransfer.size() - "
                                + BluetoothOppService.mBppTransfer.size()
                                + "BluetoothOppService.mBppTransId - "
                                + BluetoothOppService.mBppTransId);
                            BluetoothBppTransfer bp =
                                BluetoothOppService.mBppTransfer.get
                                    ((BluetoothOppService.mBppTransId -1));
                            bp.stop();
                            BluetoothOppService.mBppTransfer.remove(bp);
                            BluetoothOppService.mBppTransId--;
                            try {
                                mContext.unregisterReceiver(mReceiver);
                            } catch (IllegalArgumentException e) {
                                // ignore
                            }
                            mBatch.mOwner = BluetoothShare.OWNER_OPP;
                            mBatch.mStatus = BluetoothShare.STATUS_PENDING;
                            BluetoothOppShareInfo info = mBatch.getPendingShare();
                            if (info != null) {
                                BluetoothOppService.markBatchOwnership(mContext, info.mId,
                                    BluetoothShare.OWNER_OPP);
                            } else {
                                Log.e(TAG, "BroadcastReceiver, Action_UUID, info is null");
                            }

                            if (BluetoothOppService.mTransfer == null) {
                                if (V) Log.v(TAG, "OPP Transfer Start!! - " + mBatch.mId );
                                BluetoothOppService.mTransfer =
                                    new BluetoothOppTransfer(mContext, mPowerManager, mBatch);
                                if (BluetoothOppService.mTransfer != null) {
                                    BluetoothOppService.mTransfer.start();
                                } else {
                                    Log.e(TAG, "Error! mTransfer is null");
                                }
                            }
                            return;
                        }
                    }
                    if(bonding_process == false){
                        mSessionHandler.obtainMessage(SDP_RESULT, JobChannel, StatusChannel,
                                                    mBatch.mDestination).sendToTarget();
                    }
                }
            }
            else if (intent.getAction().equals(BluetoothDevice.ACTION_BOND_STATE_CHANGED)) {
                bonding_process = true;
                int bondState = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                                                   BluetoothDevice.ERROR);

                if (V) Log.v(TAG, "ACTION_BOND_STATE_CHANGED - " + bondState );
                if (bondState == BluetoothDevice.BOND_BONDED ){
                    bonding_process = false;
                    if (V) Log.v(TAG, "Start SDP now ");
                    if (!mBatch.mDestination.fetchUuidsWithSdp()) {
                        Log.e(TAG, "Start SDP query failed");
                        if (mSessionHandler != null) {
                            Message msg = mSessionHandler.obtainMessage(SDP_RESULT, -1, -1,
                                                                    mBatch.mDestination);
                            mSessionHandler.sendMessageDelayed(msg, 2000);
                        } else {
                            Log.e(TAG, "Error! mSessionHandler is null");
                        }
                    }
                }
            }
        }
    };

    private SocketConnectThread mConnectThread = null;
    private SocketConnectThread mConnectThreadEvent;

    private class SocketConnectThread extends Thread {

        private final BluetoothDevice device;

        private final int channel;

        private long timestamp;

        private BluetoothSocket btSocket = null;

        /* create a Rfcomm Socket */
        public SocketConnectThread(BluetoothDevice device, int channel) {
            super("Socket Connect Thread");
            this.device = device;
            this.channel = channel;
        }

        public void interrupt() {
            if (!Constants.USE_TCP_DEBUG) {
                if (btSocket != null) {
                    try {
                        btSocket.close();
                    } catch (IOException e) {
                        Log.v(TAG, "Error when close socket");
                    }
                }
            }
            super.interrupt();
        }

        @Override
        public void run() {

            BluetoothBppPreference INSTANCE = null;
            timestamp = System.currentTimeMillis();
            if (device ==null) {
                Log.e(TAG, "Error! device is null");
                return;
            }
            /* Use BluetoothSocket to connect */
            try {
                if (V) Log.v(TAG, "Rfcomm socket Connection init: " + device.getAddress());

                btSocket = device.createInsecureRfcommSocket(channel);
            } catch (IOException e1) {
                Log.e(TAG, "Rfcomm socket create error");
                markConnectionFailed(btSocket);
                return;
            }
            try {
                btSocket.connect();

                if (V) Log.v(TAG, "Rfcomm socket connection attempt took " +
                        (System.currentTimeMillis() - timestamp) + " ms");
                BluetoothBppRfcommTransport transport;
                transport = new BluetoothBppRfcommTransport(btSocket);
                INSTANCE = BluetoothBppPreference.getInstance(mContext);
                INSTANCE.setChannel(device,
                          BluetoothBppConstant.DPS_UUID16, JobChannel);
                INSTANCE.setChannel(device,
                          BluetoothBppConstant.STS_UUID16, StatusChannel);
                INSTANCE.setName(device, device.getName());
                INSTANCE.setFeatures(device,
                          BluetoothBppConstant.mSupportedDocs);
                if (V) Log.v(TAG, "Send transport message " + transport.toString());
                if(channel == JobChannel){
                    mSessionHandler.obtainMessage(RFCOMM_CONNECTED, transport).sendToTarget();
                } else {
                    mSessionHandler.obtainMessage(RFCOMM_CONNECTED_EVT, transport).sendToTarget();
                }
            } catch (IOException e) {
                Log.e(TAG, "Rfcomm socket connect exception " + e);
                if (INSTANCE != null) {
                    INSTANCE.removeChannel(device, BluetoothBppConstant.DPS_UUID16);
                    INSTANCE.removeChannel(device, BluetoothBppConstant.STS_UUID16);
                }
                markConnectionFailed(btSocket);
                return;
            }
        }

        private void markConnectionFailed(BluetoothSocket s) {
            try {
                s.close();
            } catch (IOException e) {
                if (V) Log.e(TAG, "Error when close socket");
            }
            mSessionHandler.obtainMessage(RFCOMM_ERROR).sendToTarget();
            return;
        }
    };

    /* update a trivial field of a share to notify Provider the batch status change */
    private void tickShareStatus(BluetoothOppShareInfo share) {
        if(share == null){
            if (V) Log.v(TAG, "tickShareStatus() - share is null");
            return;
        }
        Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + share.mId);
        ContentValues updateValues = new ContentValues();
        updateValues.put(BluetoothShare.DIRECTION, share.mDirection);
        mContext.getContentResolver().update(contentUri, updateValues, null, null);
    }

    /*
     * Note: For outbound transfer We don't implement this method now. If later
     * we want to support merging a later added share into an existing session,
     * we could implement here For inbounds transfer add share means it's
     * multiple receive in the same session, we should handle it to fill it into
     * mSession
     */
    /**
     * Process when a share is added to current transfer
     */
    public void onShareAdded(int id) {
        BluetoothOppShareInfo info = mBatch.getPendingShare();
        if (info == null) {
            Log.e(TAG, "Error! info is null");
            return;
        }
        if (info.mDirection == BluetoothShare.DIRECTION_INBOUND) {
            mCurrentShare = mBatch.getPendingShare();
            if (mCurrentShare != null
                    && mCurrentShare.mConfirm == BluetoothShare.USER_CONFIRMATION_AUTO_CONFIRMED) {
                /* have additional auto confirmed share to process */
                if (V) Log.v(TAG, "Transfer continue session for info " + mCurrentShare.mId +
                        " from batch " + mBatch.mId);
                processCurrentShare();
                setConfirmed();
            }
        }
    }

    /*
     * NOTE We don't implement this method now. Now delete a single share from
     * the batch means the whole batch should be canceled. If later we want to
     * support single cancel, we could implement here For outbound transfer, if
     * the share is currently in transfer, cancel it For inbounds transfer,
     * delete share means the current receiving file should be canceled.
     */
    /**
     * Process when a share is deleted from current transfer
     */
    public void onShareDeleted(int id) {

    }

    /**
     * Process when current transfer is canceled
     */
    public void onBatchCanceled() {
        if (V) Log.v(TAG, "Transfer on Batch canceled");
        this.stop();
        mBatch.mStatus = Constants.BATCH_STATUS_FINISHED;
    }
}
