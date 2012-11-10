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

import com.android.bluetooth.opp.BluetoothOppSendFileInfo;
import com.android.bluetooth.opp.Constants;
import com.android.bluetooth.opp.BluetoothShare;
import com.android.bluetooth.opp.BluetoothOppShareInfo;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.ByteArrayInputStream;

import javax.btobex.ClientOperation;
import javax.btobex.ClientSession;
import javax.btobex.HeaderSet;
import javax.btobex.ObexTransport;
import javax.btobex.ResponseCodes;
import javax.btobex.Authenticator;

import android.content.Context;
import android.content.ContentValues;
import android.net.Uri;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.util.Log;
import android.os.Process;
import android.content.Intent;
import android.app.Activity;

/**
 * This class handles Job Channel connection, disconnection, SOAP request/response,
 * and sending document.
 * It has a Thread to handle a file to send from content provider as well.
 * From BPP specification, Job Channel should be disconnected after Status Channel is disconnected.
 * So, after file transfer is done, it will wait for Status Channel disconnected notification.
 */
public class BluetoothBppObexClientSession{
    private static final String TAG = "BluetoothBppObexClientSession";

    private static final boolean D = BluetoothBppConstant.DEBUG;

    private static final boolean V = BluetoothBppConstant.VERBOSE;

    static final int MSG_SESSION_COMPLETE           = 1;

    static final int MSG_SESSION_EVENT_COMPLETE     = 2;

    static final int MSG_SHARE_COMPLETE             = 3;

    static final int MSG_SESSION_STOP               = 4;

    static final int MSG_SESSION_ERROR              = 5;

    static final int MSG_SHARE_INTERRUPTED          = 6;

    static final int MSG_CONNECT_TIMEOUT            = 7;

    static final int MSG_GETEVENT_RESPONSE          = 8;

    static final int STANDBY          = 1;

    static final int PROCESSING       = 2;

    static final int GETATTRIBUTE     = 3;

    static final int CREATJOB         = 4;

    static final int GETEVENT         = 5;

    static final int SENDDOCUMENT     = 6;

    static final int CANCEL           = 7;

    static final int CANCELLING       = 8;

    static final int CANCELLED        = 9;

    static final int DONE             = 10;

    static final int MAX_OBEX_PACKET_LENGTH = 65000;

    public static byte[] mScreenshot;

    private ClientThread mThread;

    private ObexTransport mTransport;

    private Context mContext;

    private volatile boolean mInterrupted;

    private volatile boolean mWaitingForRemote;

    private BluetoothBppAuthenticator mAuth = null;

    Handler mCallback;

    BluetoothBppSoap bs;

    public int mEventConn;

    public volatile int mSoapProcess = GETATTRIBUTE;

    private String mFileName;

    private long mFileSize;

    private String mMimeType;

    private int mTimeout;

    public BluetoothBppObexClientSession(Context context, ObexTransport transport) {
        if (transport == null) {
            throw new NullPointerException("transport is null");
        }

        if (D) Log.d(TAG, "context: "+ context + ", transport: " + transport);
        mContext = context;
        mTransport = transport;
        mEventConn = 0;
        mTimeout = 0;
    }

    public void start(Handler handler, Authenticator auth) {
        if (D) Log.d(TAG, "Start!");
        bs = new BluetoothBppSoap(handler, null);
        bs.mFileSending = false;
        mCallback = handler;
        mAuth = (BluetoothBppAuthenticator) auth;
        mThread = new ClientThread(mContext, mTransport);
        mThread.start();
    }

    public void stop() {
        if (D) Log.d(TAG, "Stop!");
        if ((mThread != null) && (mThread.isAlive())) {
            try {
                mThread.interrupt();
                if (V) Log.v(TAG, "waiting for thread to terminate");
                mThread.join();
                mThread = null;
            } catch (InterruptedException e) {
                  if (V) Log.v(TAG, "Interrupted waiting for thread to join");
            }
        }
        mCallback = null;
    }


    public void addShare(BluetoothOppShareInfo share) {
        mThread.addShare(share);
    }

    private class ClientThread extends Thread {

        private static final int sSleepTime = 500;

        private Context mContext1;

        private BluetoothOppShareInfo mInfo;

        private volatile boolean waitingForShare;

        private ObexTransport mTransport1;

        private ClientSession mCs;

        private WakeLock wakeLock;

        private BluetoothOppSendFileInfo mFileInfo = null;

        private boolean mConnected = false;

        public ClientThread(Context context, ObexTransport transport) {

            super("BtBpp ClientThread");
            mContext1 = context;
            mTransport1 = transport;
            waitingForShare = false;
            mWaitingForRemote = false;

            PowerManager pm = (PowerManager)mContext1.getSystemService(Context.POWER_SERVICE);
            wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, TAG);
        }

        public void addShare(BluetoothOppShareInfo info) {
            mInfo = info;
            mFileInfo = processShareInfo();
            waitingForShare = false;
        }

        @Override
        public void run() {
            Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);

            if (V) Log.v(TAG, "acquire partial WakeLock");
            wakeLock.acquire();
            mSoapProcess = GETATTRIBUTE;

            try {
                Thread.sleep(100);
            } catch (InterruptedException e1) {
                if (V) Log.v(TAG, "Client thread was interrupted (1), exiting");
                mInterrupted = true;
            }
            if (!mInterrupted) {
                connect();
            }

            while (!mInterrupted) {
                if(mSoapProcess == GETATTRIBUTE){
                    mSoapProcess = PROCESSING;
                    // GetPrinterAttributes SOAP Request
                    if (D) Log.d(TAG, "SOAP Request: GetPrinterAttributes");
                    int status = sendSoapRequest(BluetoothBppSoap.SOAP_REQ_GET_PR_ATTR);

                    if(status != BluetoothShare.STATUS_SUCCESS){
                        if (D) Log.d(TAG, "SOAP Request fail : " + status );
                        sendErrorReport(BluetoothShare.STATUS_OBEX_DATA_ERROR);
                    }else {
                        // Check if this operation is mInterrupted, otherwise,
                        // it will proceed next procedure
                        if (!mInterrupted) {
                            Intent in = new Intent(mContext1, BluetoothBppPrintPrefActivity.class);
                            in.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                            mContext1.startActivity(in);
                        }

                        BluetoothBppActivity.mOPPstop = false;
                        if (BluetoothBppActivity.mContext != null) {
                            ((Activity) BluetoothBppActivity.mContext).finish();
                        }
                    }
                } else if (mSoapProcess == CREATJOB) {
                    mSoapProcess = PROCESSING;
                    // CreateJob SOAP Request
                    if (D) Log.d(TAG, "SOAP Request: CreateJob");
                    int status = sendSoapRequest(BluetoothBppSoap.SOAP_REQ_CREATE_JOB);

                    if(status != BluetoothShare.STATUS_SUCCESS){
                        if (D) Log.d(TAG, "SOAP Request fail : " + status );
                        sendErrorReport(BluetoothShare.STATUS_OBEX_DATA_ERROR);
                    }
                } else if((mSoapProcess == SENDDOCUMENT) && !waitingForShare ){
                    doSend();
                } else if(mSoapProcess == CANCEL){
                    mSoapProcess = CANCELLING;
                    if (mEventConn == 1) {
                        if (D) Log.d(TAG, "SOAP Request: CancelJob");
                        int status = sendSoapRequest(BluetoothBppSoap.SOAP_REQ_CANCEL_JOB);
                        if(status != BluetoothShare.STATUS_SUCCESS){
                            if (D) Log.d(TAG, "SOAP Request fail : " + status );
                            sendErrorReport(BluetoothShare.STATUS_OBEX_DATA_ERROR);
                        }
                    } else {
                        mSoapProcess = CANCELLED;
                        sendErrorReport(BluetoothShare.STATUS_CANCELED);
                    }

                } else {
                    try {
                        if (D) Log.d(TAG, "Client thread waiting for next share, sleep for "
                            + sSleepTime);
                        Thread.sleep(sSleepTime);
                        // Wait for 1 min and if there is no action, then,
                        // it will send error message
                        if (mTimeout++ > 2*600 ) {
                            sendErrorReport(BluetoothShare.STATUS_OBEX_DATA_ERROR);
                        }
                    } catch (InterruptedException e) {

                    }
                }
            }

            disconnect();

            if (wakeLock.isHeld()) {
                if (V) Log.v(TAG, "release partial WakeLock");
                wakeLock.release();
            }
            Message msg = Message.obtain(mCallback);
            msg.what = BluetoothBppObexClientSession.MSG_SESSION_COMPLETE;
            msg.obj = mInfo;
            msg.sendToTarget();
        }

        private void disconnect() {
            try {
                if (mConnected != false) {
                    mCs.disconnect(null);
                    mCs = null;
                    mConnected = false;
                }
                if (D) Log.d(TAG, "OBEX session disconnected");
            } catch (IOException e) {
                Log.e(TAG, "OBEX session disconnect error" + e);
            }
            try {
                if (mCs != null) {
                    if (D) Log.d(TAG, "OBEX session close mCs");
                    mCs.close();
                    if (D) Log.d(TAG, "OBEX session closed");
                }
            } catch (IOException e) {
                Log.e(TAG, "OBEX session close error" + e);
            }
            if (mTransport1 != null) {
                try {
                    mTransport1.close();
                } catch (IOException e) {
                    Log.e(TAG, "mTransport.close error");
                }
            }
        }

        private void connect() {
            if (D) Log.d(TAG, "Create ClientSession with transport " + mTransport1.toString());
            try {
                mCs = new ClientSession(mTransport1);
                mCs.setAuthenticator(mAuth);
                mCs.setMaxPacketSize(MAX_OBEX_PACKET_LENGTH);
                mConnected = true;
            } catch (IOException e1) {
                Log.e(TAG, "OBEX session create error");
            }
            if (mConnected) {
                mConnected = false;
                HeaderSet hs = new HeaderSet();

                // OBEX Authentication Setup
                if(BluetoothBppSetting.bpp_auth == true){
                    if (D) Log.d(TAG, "OBEX Authentication is initiated");
                    try {
                        hs.createAuthenticationChallenge(null, false, true);
                    } catch (IOException e1) {
                        e1.printStackTrace();
                    }
                }

                hs.setHeader(HeaderSet.TARGET, BluetoothBppConstant.DPS_Target_UUID);

                synchronized (this) {
                    mWaitingForRemote = true;
                }
                try {
                    mCs.connect(hs);
                    if (D) Log.d(TAG, "OBEX session created");
                    mConnected = true;
                } catch (IOException e) {
                    Log.e(TAG, "OBEX session connect error");
                    sendErrorReport(BluetoothShare.STATUS_CONNECTION_ERROR);
                }
            }
            synchronized (this) {
                mWaitingForRemote = false;
            }
        }

        private void sendErrorReport(int statusCode) {
            Message msg = Message.obtain(mCallback);
            msg.what = BluetoothBppObexClientSession.MSG_SESSION_ERROR;

            mInfo.mStatus = statusCode;
            msg.obj = mInfo;
            msg.sendToTarget();
        }

        private void doSend() {
            int status = BluetoothShare.STATUS_SUCCESS;

            if (D) Log.d(TAG, "OBEX: Sending data ...");
            /* connection is established too fast to get first mInfo */
            while (mFileInfo == null) {
                try {
                    Thread.sleep(50);
                } catch (InterruptedException e) {
                    status = BluetoothShare.STATUS_CANCELED;
                }
            }

            if (!mConnected) {
                // Obex connection error
                status = BluetoothShare.STATUS_CONNECTION_ERROR;
                if (D) Log.d(TAG, "no OBEX Connection");
            }
            if (status == BluetoothShare.STATUS_SUCCESS) {
                // do real send
                if (mFileInfo.mFileName != null) {
                    // Send Document
                    status = sendFile(mFileInfo);
                } else {
                    // this is invalid request
                    status = mFileInfo.mStatus;
                }
                waitingForShare = true;
            } else {
                Constants.updateShareStatus(mContext1, mInfo.mId, status);
            }
            bs.mFileSending = false;
            mCallback.obtainMessage(
                    BluetoothBppTransfer.FILE_SENDING, 0, -1).sendToTarget();

            if (status == BluetoothShare.STATUS_SUCCESS) {
                Message msg = Message.obtain(mCallback);
                msg.what = BluetoothBppObexClientSession.MSG_SHARE_COMPLETE;
                msg.arg1 = status;
                msg.obj = mInfo;
                msg.sendToTarget();
            } else {
                sendErrorReport(status);
            }
        }

        /*
         * Validate this ShareInfo
         */
        private BluetoothOppSendFileInfo processShareInfo() {
            if (V) Log.v(TAG, "Client thread processShareInfo() " + mInfo.mId);

            BluetoothOppSendFileInfo fileInfo = BluetoothOppSendFileInfo.generateFileInfo(
                        mContext1, mInfo.mUri, mInfo.mMimetype, mInfo.mDestination);
            if (fileInfo.mFileName == null || fileInfo.mLength == 0) {
                if (V) Log.v(TAG, "BluetoothOppSendFileInfo get invalid file");
                Constants.updateShareStatus(mContext1, mInfo.mId, fileInfo.mStatus);

            } else {
                if (V) {
                    Log.v(TAG, "Generate BluetoothOppSendFileInfo:");
                    Log.v(TAG, "filename  :" + fileInfo.mFileName);
                    Log.v(TAG, "length    :" + fileInfo.mLength);
                    Log.v(TAG, "mimetype  :" + fileInfo.mMimetype);
                }

                if (mScreenshot != null) {
                    mFileName = "bt_shared_screenshot.jpg";
                    mFileSize = mScreenshot.length;
                    mMimeType = "image/jpeg";

                    if (V) {
                        Log.v(TAG, "Screenshot image file");
                        Log.v(TAG, "filename  :" + mFileName);
                        Log.v(TAG, "length    :" + mFileSize);
                        Log.v(TAG, "mimetype  :" + mMimeType);
                    }
                } else {
                    mFileName = fileInfo.mFileName;
                    mFileSize = fileInfo.mLength;
                    mMimeType = fileInfo.mMimetype;
                }

                ContentValues updateValues = new ContentValues();
                Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + mInfo.mId);

                updateValues.put(BluetoothShare.FILENAME_HINT, mFileName);
                updateValues.put(BluetoothShare.TOTAL_BYTES, mFileSize);
                updateValues.put(BluetoothShare.MIMETYPE, mMimeType);
                mContext1.getContentResolver().update(contentUri, updateValues, null, null);
            }
            return fileInfo;
        }

        private synchronized int sendSoapRequest(String soapReq){
            boolean error = false;
            int responseCode = -1;
            HeaderSet request;
            int status = BluetoothShare.STATUS_SUCCESS;
            int responseLength = 0;

            ClientOperation getOperation = null;
            OutputStream outputStream = null;
            InputStream inputStream = null;

            request = new HeaderSet();
            request.setHeader(HeaderSet.TYPE, BluetoothBppConstant.MIME_TYPE_SOAP);

            try {
                // it sends GET command with current existing Header now..
                getOperation = (ClientOperation)mCs.get(request);
            } catch (IOException e) {
                Log.e(TAG, "Error when get HeaderSet ");
                error = true;
            }
            synchronized (this) {
                mWaitingForRemote = false;
            }

            if (!error) {
                try {
                    outputStream = getOperation.openOutputStream();
                } catch (IOException e) {
                    Log.e(TAG, "Error when openOutputStream");
                    error = true;
                }
            }

            try {
                synchronized (this) {
                    mWaitingForRemote = true;
                }

                if (!error) {
                    int position = 0;
                    int offset = 0;
                    int readLength = 0;
                    int remainedData = 0;
                    long timestamp = 0;
                    int maxBuffSize = getOperation.getMaxPacketSize();
                    byte[] readBuff = null;
                    String soapMsg = null;
                    int soapRequestSize = 0;

                    // Building SOAP Message based on SOAP Request
                    soapMsg = bs.Builder(soapReq);
                    soapRequestSize = soapMsg.length();

                    if (!mInterrupted && (position != soapRequestSize)) {
                        synchronized (this) {
                            mWaitingForRemote = true;
                        }

                        if (V) timestamp = System.currentTimeMillis();

                        // Sending Soap Request Data
                        while(position != soapRequestSize){
                            // Check if the data size is bigger than max packet size.
                            remainedData = soapRequestSize - position;
                            readLength = (maxBuffSize > remainedData)? remainedData : maxBuffSize;

                            if (V) Log.v(TAG, "Soap Request is now being sent...");
                            outputStream.write(soapMsg.getBytes(), position, readLength);

                            //Wait for response code
                            responseCode = getOperation.getResponseCode();

                            if (V) Log.v(TAG, "Rx - OBEX Response: " + ((responseCode
                                == ResponseCodes.OBEX_HTTP_CONTINUE)? "Continue"
                                    :(responseCode == ResponseCodes.OBEX_HTTP_OK)?
                                    "SUCCESS": responseCode ));
                            if (responseCode != ResponseCodes.OBEX_HTTP_CONTINUE
                                && responseCode != ResponseCodes.OBEX_HTTP_OK) {

                                outputStream.close();
                                getOperation.close();

                                status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
                                return status;
                            } else {
                                /*In case that a remote send a length for SOAP response right away
                                  after SOAP request.. */
                                if((getOperation.getLength()!= -1) && (readBuff == null)){
                                    if (V) Log.v(TAG, "Get OBEX Length - "
                                        + getOperation.getLength());
                                    responseLength = (int)getOperation.getLength();
                                    readBuff = new byte[responseLength];
                                }

                                position += readLength;
                                if (V) {
                                    Log.v(TAG, "Soap Request is sent...\r\n" +
                                            " - Total Size : " + soapRequestSize + "bytes" +
                                            "\r\n - Sent Size  : " + position + "bytes" +
                                            "\r\n - Taken Time : " + (System.currentTimeMillis()
                                                        - timestamp) + " ms");
                                }
                            }
                        }
                        // Sending data completely !!
                        outputStream.close();

                        synchronized (this) {
                            mWaitingForRemote = false;
                        }

                        // Prepare to get input data
                        inputStream = getOperation.openInputStream();

                        // Check response code from the remote
                        while( (responseCode = getOperation.getResponseCode())
                            == ResponseCodes.OBEX_HTTP_CONTINUE) {
                            if (V) Log.v(TAG, "Rx - OBEX Response: Continue");

                            if((getOperation.getLength()!= -1) && (readBuff == null)){
                                if (V) Log.v(TAG, "Get OBEX Length - " + getOperation.getLength());
                                responseLength = (int)getOperation.getLength();
                                readBuff = new byte[responseLength];
                            }

                            if(readBuff != null){
                                offset += inputStream.read(readBuff, offset,
                                    (responseLength - offset));
                                if (V) Log.v(TAG, "OBEX Data Read: " + offset + "/"
                                    + responseLength + "bytes");
                            } else {
                                getOperation.continueOperation(true, true);
                                Thread.sleep(10); // Wait 100ms
                            }
                        }

                        if(responseCode == ResponseCodes.OBEX_HTTP_OK){
                            if (V) Log.v(TAG, "OBEX GET: SUCCESS");
                            if (V) Log.v(TAG, "Get OBEX Length - " + getOperation.getLength());

                            if(responseLength == 0){
                                if((getOperation.getLength()!= -1) && (readBuff == null)){
                                    if (V) Log.v(TAG, "Get OBEX Length - "
                                                            + getOperation.getLength());
                                    responseLength = (int)getOperation.getLength();
                                    readBuff = new byte[responseLength];
                                }
                            }

                            if(readBuff != null){
                                if(offset != responseLength){
                                    offset += inputStream.read(readBuff, offset,
                                        (responseLength - offset));
                                    if (V) Log.v(TAG, "OBEX Data Read: " + offset
                                        + "/" + responseLength + "bytes");
                                }

                                if(offset != responseLength){
                                    if (V) Log.v(TAG, "OBEX Data Read: Fail!! - missing "
                                        + (responseLength - offset) + "bytes");
                                    status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
                                    return status;
                                }

                                String soapRsp = new String(readBuff);
                                if (V) Log.v(TAG, "Soap Response Message:\r\n" + soapRsp);
                                bs.Parser(soapReq, soapRsp);
                            }
                        }
                        else
                        {
                            if (V) Log.v(TAG, "OBEX GET: FAIL - " + responseCode );
                            switch(responseCode)
                            {
                                case ResponseCodes.OBEX_HTTP_FORBIDDEN:
                                case ResponseCodes.OBEX_HTTP_NOT_ACCEPTABLE:
                                    status = BluetoothShare.STATUS_FORBIDDEN;
                                    break;
                                case ResponseCodes.OBEX_HTTP_UNSUPPORTED_TYPE:
                                    status = BluetoothShare.STATUS_NOT_ACCEPTABLE;
                                    break;
                                default:
                                    status = BluetoothShare.STATUS_UNKNOWN_ERROR;
                                    break;
                            }
                        }
                    }
                }
            } catch (IOException e) {
                if (V) Log.v(TAG, " IOException e) : " + e);
                status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
            } catch (NullPointerException e) {
                if (V) Log.v(TAG, " NullPointerException : " + e);
                status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
            } catch (IndexOutOfBoundsException e) {
                if (V) Log.v(TAG, " IndexOutOfBoundsException : " + e);
                status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
            } catch (InterruptedException e) {
                status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
            } finally {
                if (inputStream != null) {
                    try {
                        inputStream.close();
                    } catch (IOException e) {
                        if (V) Log.v(TAG, "inputStream.close IOException : " + e);
                        status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
                    }
                }
                if (getOperation != null) {
                    try {
                        getOperation.close();
                    } catch (IOException e) {
                        if (V) Log.v(TAG, "getOperation.close IOException : " + e);
                        status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
                    }
                }
            }
            return status;
        }

        private int sendFile(BluetoothOppSendFileInfo fileInfo) {
            boolean error = false;
            int responseCode = -1;
            int position = 0;
            int status = BluetoothShare.STATUS_SUCCESS;
            Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + mInfo.mId);
            ContentValues updateValues;
            HeaderSet request;
            int newBufferSize = 0;
            byte[] newBuffer = null;

            if(bs.bppJobId == null){
                status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
                if (V) Log.v(TAG, "BPP Error: Missing JobId");
                return status;
            }

            if (V) Log.v(TAG, "sendFile - File name: " + mFileName +
                                "\r\n\t Mime Type: " + mMimeType +
                                "\r\n\t Size: " + mFileSize );

            bs.mFileSending = true;
            mCallback.obtainMessage(
                    BluetoothBppTransfer.FILE_SENDING, 1, -1).sendToTarget();

            BluetoothBppStatusActivity.mTrans_Progress.setMax((int)mFileSize);
            request = new HeaderSet();
            request.setHeader(HeaderSet.NAME, mFileName );
            String newMimeType;
            if ((newMimeType = BluetoothBppTransfer.checkUnknownMimetype(mMimeType,
                    mFileName)) != null){
                request.setHeader(HeaderSet.TYPE, newMimeType );
                if(V) Log.v(TAG, "set new MIME Type: " + newMimeType);

            } else {
                request.setHeader(HeaderSet.TYPE, mMimeType );
            }

            byte appParam[] = new byte[6];
            appParam[0] = BluetoothBppConstant.OBEX_HDR_APP_PARAM_JOBID; // Tag
            appParam[1] = 4; // Length
            appParam[2] = bs.bppJobId[3];
            appParam[3] = bs.bppJobId[2];
            appParam[4] = bs.bppJobId[1];
            appParam[5] = bs.bppJobId[0];
            request.setHeader(HeaderSet.APPLICATION_PARAMETER, appParam);

            applyRemoteDeviceQuirks(request, fileInfo);

            Constants.updateShareStatus(mContext1, mInfo.mId, BluetoothShare.STATUS_RUNNING);

            request.setHeader(HeaderSet.LENGTH, mFileSize);
            ClientOperation putOperation = null;
            OutputStream outputStream = null;
            InputStream inputStream = null;
            try {
                synchronized (this) {
                    mWaitingForRemote = true;
                }
                try {
                    if (V) Log.v(TAG, "put headerset for " + mFileName);
                    putOperation = (ClientOperation)mCs.put(request);
                } catch (IOException e) {
                    status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
                    Constants.updateShareStatus(mContext1, mInfo.mId, status);

                    Log.e(TAG, "Error when put HeaderSet ");
                    error = true;
                }
                synchronized (this) {
                    mWaitingForRemote = false;
                }

                if (!error) {
                    try {
                        if (V) Log.v(TAG, "openOutputStream " + mFileName);
                        outputStream = putOperation.openOutputStream();
                        inputStream = putOperation.openInputStream();
                    } catch (IOException e) {
                        status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
                        Constants.updateShareStatus(mContext1, mInfo.mId, status);
                        Log.e(TAG, "Error when openOutputStream");
                        error = true;
                    }
                }

                if (!error) {
                    updateValues = new ContentValues();
                    updateValues.put(BluetoothShare.CURRENT_BYTES, 0);
                    updateValues.put(BluetoothShare.STATUS, BluetoothShare.STATUS_RUNNING);
                    mContext1.getContentResolver().update(contentUri, updateValues, null, null);
                }

                if (!error) {
                    int readLength = 0;
                    boolean okToProceed = false;
                    long timestamp = 0;
                    int outputBufferSize = putOperation.getMaxPacketSize();
                    byte[] buffer = new byte[outputBufferSize];
                    BufferedInputStream a = null;

                    if (mScreenshot != null ) {
                        a = new BufferedInputStream(new ByteArrayInputStream(mScreenshot), 0x4000);
                    } else {
                        a = new BufferedInputStream(fileInfo.mInputStream, 0x4000);
                    }
                    if (!mInterrupted && (position != mFileSize ) && (mSoapProcess != CANCEL)) {
                        readLength = a.read(buffer, 0, outputBufferSize);

                        synchronized (this) {
                            mWaitingForRemote = true;
                        }
                        // first packet will block here
                        outputStream.write(buffer, 0, readLength);
                        position += readLength;
                        Log.d(TAG, "Obex Packet size - " +  readLength);
                        BluetoothBppStatusActivity.updateProgress(position, 0);

                        if (position != mFileSize) {
                            synchronized (this) {
                                mWaitingForRemote = false;
                            }
                        } else {
                            // if file length is smaller than buffer size, only one packet
                            // so block point is here
                            outputStream.close();
                            synchronized (this) {
                                mWaitingForRemote = false;
                            }
                        }
                        // check remote accept or reject
                        responseCode = putOperation.getResponseCode();

                        if (responseCode == ResponseCodes.OBEX_HTTP_CONTINUE
                            || responseCode == ResponseCodes.OBEX_HTTP_OK) {
                            if (V) Log.v(TAG, "Remote accept");
                            okToProceed = true;
                            updateValues = new ContentValues();
                            updateValues.put(BluetoothShare.CURRENT_BYTES, position);
                            mContext1.getContentResolver().update(contentUri, updateValues, null,
                                null);
                        } else {
                            Log.i(TAG, "Remote reject, Response code is " + responseCode);
                            bs.mPrinterStateReason = "\"Printer reject operation\"";
                        }
                    }
                    // After first packet, add unused obex header size to current obex packet buffer
                    // to increase throughput
                    newBufferSize = outputBufferSize /* Current buffer size */
                                    + (fileInfo.mFileName.length()+ 3) /* File name field length */
                                    + (fileInfo.mMimetype.length() + 3) /* Mime type field length */
                                    + (appParam.length + 3) /* Application parameter field length */
                                    + 5;  /* File size field length */
                    newBuffer = new byte[newBufferSize];

                    while (!mInterrupted && okToProceed && (position != mFileSize)
                            && (mSoapProcess != CANCEL)) {
                        {
                            if (V) timestamp = System.currentTimeMillis();
                            readLength = a.read(newBuffer, 0, newBufferSize);
                            outputStream.write(newBuffer, 0, readLength);
                            Log.d(TAG, "New Obex Packet size - " +  readLength);

                            // check remote abort
                            responseCode = putOperation.getResponseCode();
                            if (V) Log.v(TAG, "Response code is " + responseCode);
                            if (responseCode != ResponseCodes.OBEX_HTTP_CONTINUE
                                    && responseCode != ResponseCodes.OBEX_HTTP_OK) {
                                // abort happens
                                okToProceed = false;
                            } else {
                                position += readLength;
                                BluetoothBppStatusActivity.updateProgress(position, 0);
                                if (V) {
                                    Log.v(TAG, "Sending file position = " + position
                                        + " readLength " + readLength + " bytes took "
                                        + (System.currentTimeMillis() - timestamp) + " ms");
                                }
                                updateValues = new ContentValues();
                                updateValues.put(BluetoothShare.CURRENT_BYTES, position);
                                mContext1.getContentResolver().update(contentUri, updateValues,
                                        null, null);
                            }
                        }
                    }
                    if((mSoapProcess == CANCEL) && (position != mFileSize)
                       && (!mInterrupted)){
                        mSoapProcess = CANCELLING;
                        error = true;
                        status = BluetoothShare.STATUS_CANCELED;

                        putOperation.abort();
                        // Send CancelJob Request.
                        sendSoapRequest(BluetoothBppSoap.SOAP_REQ_CANCEL_JOB);

                        Log.i(TAG, "SendFile interrupted when send out file " + mFileName
                                + " at " + position + " of " + mFileSize);
                    } else if (responseCode == ResponseCodes.OBEX_HTTP_FORBIDDEN
                            || responseCode == ResponseCodes.OBEX_HTTP_NOT_ACCEPTABLE) {
                        Log.i(TAG, "Remote reject file " + mFileName + " length " + mFileSize);
                    } else if (responseCode == ResponseCodes.OBEX_HTTP_UNSUPPORTED_TYPE) {
                        Log.i(TAG, "Remote reject file type " + mMimeType);
                    } else if (!mInterrupted && position == mFileSize) {
                        Log.i(TAG, "SendFile finished send out file " + mFileName
                                + " length " + mFileSize);
                        outputStream.close();
                    } else {
                        error = true;
                        status = BluetoothShare.STATUS_CANCELED;
                        putOperation.abort();
                        /* interrupted */
                        Log.i(TAG, "SendFile interrupted when send out file " + mFileName
                                + " at " + position + " of " + mFileSize);
                    }
                }
            } catch (IOException e) {
                status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
            } catch (NullPointerException e) {
                status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
            } catch (IndexOutOfBoundsException e) {
                status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
            } finally {
                try {
                    fileInfo.mInputStream.close();
                    if (!error) {
                        responseCode = putOperation.getResponseCode();
                        if (responseCode != -1) {
                            if (V) Log.v(TAG, "Get response code " + responseCode);
                            if (responseCode != ResponseCodes.OBEX_HTTP_OK) {
                                Log.i(TAG, "Response error code is " + responseCode);
                                status = BluetoothShare.STATUS_UNHANDLED_OBEX_CODE;
                                if (responseCode == ResponseCodes.OBEX_HTTP_UNSUPPORTED_TYPE) {
                                    status = BluetoothShare.STATUS_NOT_ACCEPTABLE;
                                }
                                if (responseCode == ResponseCodes.OBEX_HTTP_FORBIDDEN
                                        || responseCode == ResponseCodes.OBEX_HTTP_NOT_ACCEPTABLE) {
                                    status = BluetoothShare.STATUS_FORBIDDEN;
                                }
                            }
                        } else {
                            // responseCode is -1, which means connection error
                            status = BluetoothShare.STATUS_CONNECTION_ERROR;
                        }
                    }
                    Constants.updateShareStatus(mContext1, mInfo.mId, status);

                    if (inputStream != null) {
                        inputStream.close();
                    }
                    if (putOperation != null) {
                        putOperation.close();
                    }
                } catch (IOException e) {
                    Log.e(TAG, "Error when closing stream after send");
                    /* Socket is been closed due to the response timeout in the framework
                     * Hence, mark the transfer as failure
                     */
                    if (position != mFileSize)
                      status = BluetoothShare.STATUS_FORBIDDEN;
                }
            }
            return status;
        }

        @Override
        public void interrupt() {
            if(V) Log.v(TAG,"interrupt + ");
            if(mInterrupted){
                if (V) Log.v(TAG, "Interupt already in progress");
                return;
            }
            super.interrupt();
            mInterrupted = true;
        }
    }

    public static void applyRemoteDeviceQuirks(HeaderSet request,
                                                     BluetoothOppSendFileInfo info) {
        String address = info.mDestAddr;
        if (address == null) {
            return;
        }
        if (address.startsWith("00:04:48")) {
            // Poloroid Pogo
            // Rejects filenames with more than one '.'. Rename to '_'.
            // for example: 'a.b.jpg' -> 'a_b.jpg'
            //      'abc.jpg' NOT CHANGED
            String filename = info.mFileName;

            char[] c = filename.toCharArray();
            boolean firstDot = true;
            boolean modified = false;
            for (int i = c.length - 1; i >= 0; i--) {
                if (c[i] == '.') {
                    if (!firstDot) {
                        modified = true;
                        c[i] = '_';
                    }
                    firstDot = false;
                }
            }
            if (modified) {
                String newFilename = new String(c);
                request.setHeader(HeaderSet.NAME, newFilename);
                Log.i(TAG, "Sending file \"" + filename + "\" as \"" + newFilename +
                        "\" to workaround Poloroid filename quirk");
            }
        }
    }

    public void unblock() {
        // Not used for client case
    }

    public static void putScreenshot(byte[] data) {
        mScreenshot = new byte[data.length];
        System.arraycopy(data, 0, mScreenshot, 0, data.length);
    }
}
