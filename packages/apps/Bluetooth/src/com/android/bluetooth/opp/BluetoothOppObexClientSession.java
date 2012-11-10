/*
 * Copyright (c) 2010-2012 Code Aurora Forum. All rights reserved.
 * Copyright (c) 2008-2009, Motorola, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * - Neither the name of the Motorola, Inc. nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.bluetooth.opp;

import javax.btobex.ClientOperation;
import javax.btobex.ClientSession;
import javax.btobex.HeaderSet;
import javax.btobex.ObexHelper;
import javax.btobex.ObexTransport;
import javax.btobex.ResponseCodes;

import android.bluetooth.BluetoothSocket;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.os.Process;
import android.os.SystemProperties;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.Thread;
import android.widget.RemoteViews;
import android.app.PendingIntent;
import android.app.Notification;
import android.app.NotificationManager;
import android.content.Intent;

/**
 * This class runs as an OBEX client
 */
public class BluetoothOppObexClientSession implements BluetoothOppObexSession {

    private static final String TAG = "BtOpp ObexClient";
    private static final boolean D = Constants.DEBUG;
    private static final boolean V = Constants.VERBOSE;

    /* AMP move file size threshold property */
    private static final String AMP_MOVE_THRESHOLD = "bt.opp.amp_move_threshold";
    /* Approx threshold for 2 sec AMP channel move delay, 600kB/sec AMP, 140 kB/sec BR/EDR */
    private static final int AMP_MOVE_THRESHOLD_DEFAULT = 400000;

    /* Debugging hooks to control AMP-related operations */
    private static final String DEBUG_PRE_AMP_MOVE_DELAY = "debug.bt.opp.ms_pre_amp_move";
    private static final String DEBUG_POST_AMP_MOVE_DELAY = "debug.bt.opp.ms_post_amp_move";

    private ClientThread mThread;

    private ObexTransport mTransport;

    private Context mContext;

    private volatile boolean mInterrupted;

    private volatile boolean mWaitingForRemote;

    private Handler mCallback;

    public BluetoothOppObexClientSession(Context context, ObexTransport transport) {
        if (transport == null) {
            throw new NullPointerException("transport is null");
        }
        mContext = context;
        mTransport = transport;
    }

    public void start(Handler handler) {
        if (D) Log.d(TAG, "Start!");
        mCallback = handler;
        mThread = new ClientThread(mContext, mTransport);
        mThread.start();
    }

    public void stop() {
        if (D) Log.d(TAG, "Stop!");
        if (mThread != null) {
            mInterrupted = true;
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

    private class ContentResolverUpdateThread extends Thread {

        private static final int sSleepTime = 500;
        private Uri contentUri;
        private Context mContext1;
        private long position;
        private volatile boolean interrupted = false;

        public ContentResolverUpdateThread(Context context, Uri cntUri, long pos) {
            super("BtOpp ContentResolverUpdateThread");
            mContext1 = context;
            contentUri = cntUri;
            position = pos;
        }

        public void updateProgress (long pos) {
            position = pos;
        }

        @Override
        public void run() {
            ContentValues updateValues;

            Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);

            while (true) {
                updateValues = new ContentValues();
                updateValues.put(BluetoothShare.CURRENT_BYTES, position);
                mContext1.getContentResolver().update(contentUri, updateValues,
                        null, null);

                /*
                    Check if the Operation is interrupted before entering sleep
                */
                if (interrupted == true) {
                    if (V) Log.v(TAG, "ContentResolverUpdateThread was interrupted before sleep !, exiting");
                    return;
                }

                try {
                    Thread.sleep(sSleepTime);
                } catch (InterruptedException e1) {
                    if (V) Log.v(TAG, "ContentResolverUpdateThread was interrupted (1), exiting");
                    return;
                }
            }
        }

        @Override
        public void interrupt() {
            interrupted = true;
            super.interrupt();
        }
    }

    private static int readFully(InputStream is, byte[] buffer, int size) throws IOException {
        int done = 0;
        while (done < size) {
            int got = is.read(buffer, done, size - done);
            if (got <= 0) break;
            done += got;
        }
        return done;
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
            super("BtOpp ClientThread");
            mContext1 = context;
            mTransport1 = transport;
            waitingForShare = true;
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
                if (!waitingForShare) {
                    doSend();
                } else {
                    try {
                        if (D) Log.d(TAG, "Client thread waiting for next share, sleep for "
                                    + sSleepTime);
                        Thread.sleep(sSleepTime);
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
            msg.what = BluetoothOppObexSession.MSG_SESSION_COMPLETE;
            msg.obj = mInfo;
            msg.sendToTarget();

        }

        private void disconnect() {
            try {
                if (mCs != null) {
                    mCs.disconnect(null);
                }
                mCs = null;
                if (D) Log.d(TAG, "OBEX session disconnected");
            } catch (IOException e) {
                Log.w(TAG, "OBEX session disconnect error" + e);
            }
            try {
                if (mCs != null) {
                    if (D) Log.d(TAG, "OBEX session close mCs");
                    mCs.close();
                    if (D) Log.d(TAG, "OBEX session closed");
                    }
            } catch (IOException e) {
                Log.w(TAG, "OBEX session close error" + e);
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
                mConnected = true;
                int mps = ((BluetoothOppTransport)mTransport1).getMaxPacketSize();
                mCs.setMaxPacketSize(mps);
                if (D) Log.d(TAG, "Setting ClientSession mps " + mps);
            } catch (IOException e1) {
                Log.e(TAG, "OBEX session create error");
            }
            if (mConnected) {
                mConnected = false;
                HeaderSet hs = new HeaderSet();
                synchronized (this) {
                    mWaitingForRemote = true;
                }
                try {
                    mCs.connect(hs);
                    if (D) Log.d(TAG, "OBEX session created");
                    mConnected = true;
                } catch (IOException e) {
                    Log.e(TAG, "OBEX session connect error");
                }
            }
            synchronized (this) {
                mWaitingForRemote = false;
            }
        }

        private void doSend() {

            int status = BluetoothShare.STATUS_SUCCESS;

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
            }
            if (status == BluetoothShare.STATUS_SUCCESS) {
                /* do real send */
                if (mFileInfo.mFileName != null) {
                    status = sendFile(mFileInfo);
                } else {
                    /* this is invalid request */
                    status = mFileInfo.mStatus;
                }
                waitingForShare = true;
            } else {
                Constants.updateShareStatus(mContext1, mInfo.mId, status);
            }
        }

        /*
         * Validate this ShareInfo
         */
        private BluetoothOppSendFileInfo processShareInfo() {
            if (V) Log.v(TAG, "Client thread processShareInfo() " + mInfo.mId);
            int icon = android.R.drawable.stat_sys_upload;
            CharSequence tickerText = "Preparing Bluetooth Share";
            long when = System.currentTimeMillis();
            Context context = mContext1;
            CharSequence contentTitle = "Bluetooth Share";
            CharSequence contentText = "Preparing...";

            Notification n = new Notification(icon, tickerText, when);

            Intent intent = new Intent(Constants.ACTION_LIST);
            PendingIntent contentIntent = PendingIntent.getActivity(context, 0, intent, 0);
            n.setLatestEventInfo(context, contentTitle, contentText, contentIntent);
            ((NotificationManager)(context.getSystemService(context.NOTIFICATION_SERVICE))).notify(mInfo.mId, n);

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

                ContentValues updateValues = new ContentValues();
                Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + mInfo.mId);

                updateValues.put(BluetoothShare.FILENAME_HINT, fileInfo.mFileName);
                updateValues.put(BluetoothShare.TOTAL_BYTES, fileInfo.mLength);
                updateValues.put(BluetoothShare.MIMETYPE, fileInfo.mMimetype);

                mContext1.getContentResolver().update(contentUri, updateValues, null, null);

            }
            ((NotificationManager)(context.getSystemService(context.NOTIFICATION_SERVICE))).cancel(mInfo.mId);
            return fileInfo;
        }

        private int sendFile(BluetoothOppSendFileInfo fileInfo) {
            boolean error = false;
            int responseCode = -1;
            int status = BluetoothShare.STATUS_SUCCESS;
            Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + mInfo.mId);
            ContentValues updateValues;
            ContentResolverUpdateThread uiUpdateThread = null;
            HeaderSet reply;
            long position = 0;
            reply = new HeaderSet();
            HeaderSet request;
            request = new HeaderSet();
            request.setHeader(HeaderSet.NAME, fileInfo.mFileName);
            request.setHeader(HeaderSet.TYPE, fileInfo.mMimetype);

            applyRemoteDeviceQuirks(request, fileInfo);

            Constants.updateShareStatus(mContext1, mInfo.mId, BluetoothShare.STATUS_RUNNING);

            request.setHeader(HeaderSet.LENGTH, fileInfo.mLength);

            // Turn on/off SRM based on transport capability
            //(whether this is OBEX-over-L2CAP, or not)
            mCs.mSrmClient.setLocalSrmCapability(((BluetoothOppTransport)mTransport1).isSrmCapable());

            // Add the SRM header if both client is SRM capable
            if (mCs.mSrmClient.getLocalSrmCapability() == ObexHelper.SRM_CAPABLE) {
                Log.v(TAG, "SRM status: Enable SRM for first PUT");
                mCs.mSrmClient.setLocalSrmStatus(ObexHelper.LOCAL_SRM_ENABLED);
                request.setHeader(HeaderSet.SINGLE_RESPONSE_MODE, ObexHelper.OBEX_SRM_ENABLED);
            } else {
                Log.v(TAG, "SRM status: Disable SRM for first PUT");
                mCs.mSrmClient.setLocalSrmStatus(ObexHelper.LOCAL_SRM_DISABLED);
            }
            mCs.mSrmClient.setLocalSrmpWait(false);

            ClientOperation putOperation = null;
            OutputStream outputStream = null;
            InputStream inputStream = null;
            try {
                synchronized (this) {
                    mWaitingForRemote = true;
                }
                try {
                    if (V) Log.v(TAG, "put headerset for " + fileInfo.mFileName);
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
                    // Per Bluetooth OPP v1.2 specification (Appendix A), it is
                    // recommended that an AMP move be performed at this point,
                    // if applicable (e.g., if an AMP is present and is deemed
                    // favorable).
                    int ampThresh = SystemProperties.getInt(AMP_MOVE_THRESHOLD,
                            AMP_MOVE_THRESHOLD_DEFAULT);
                    if (((BluetoothOppTransport)mTransport1).isAmpCapable() &&
                        (fileInfo.mLength > ampThresh)) {
                        if (D) {
                            int preWait = SystemProperties.getInt(DEBUG_PRE_AMP_MOVE_DELAY, -1);
                            if (preWait >= 0) {
                                Log.v(TAG, "DEBUG: delaying before AMP move: " + preWait);
                                try {
                                    Thread.sleep(preWait);
                                } catch (InterruptedException e) {
                                    Log.v(TAG, "Interrupted during pre-AMP move delay.");
                                }
                            }
                        }

                        if(!((BluetoothOppTransport)mTransport1).setDesiredAmpPolicy(
                              BluetoothSocket.BT_AMP_POLICY_PREFER_AMP)) {
                            Log.e(TAG, "Unable to set AMP policy, " +
                                "using default (BR/EDR req).");
                        }

                        if (D) {
                            int postWait = SystemProperties.getInt(DEBUG_POST_AMP_MOVE_DELAY, -1);
                            if (postWait >= 0) {
                                Log.v(TAG, "DEBUG: delaying after AMP move: " + postWait);
                                try {
                                    Thread.sleep(postWait);
                                } catch (InterruptedException e) {
                                    Log.v(TAG, "Interrupted during post-AMP move delay.");
                                }
                            }
                        }
                    }

                    try {
                        if (V) Log.v(TAG, "openOutputStream " + fileInfo.mFileName);
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
                    long readbytesleft = 0;
                    boolean okToProceed = false;
                    long timestamp = 0;
                    int outputBufferSize = putOperation.getMaxPacketSize();
                    byte[] buffer = new byte[outputBufferSize];
                    BufferedInputStream a = new BufferedInputStream(fileInfo.mInputStream, 0x4000);

                    if (!mInterrupted && (position != fileInfo.mLength)) {

                        readbytesleft = fileInfo.mLength - position;
                        if(readbytesleft < outputBufferSize) {
                            outputBufferSize = (int) readbytesleft;
                        }

                        readLength = readFully(a, buffer, outputBufferSize);

                        mCallback.sendMessageDelayed(mCallback
                                .obtainMessage(BluetoothOppObexSession.MSG_CONNECT_TIMEOUT),
                                BluetoothOppObexSession.SESSION_TIMEOUT);
                        synchronized (this) {
                            mWaitingForRemote = true;
                        }

                        // first packet will block here
                        outputStream.write(buffer, 0, readLength);

                        position += readLength;

                        if (position != fileInfo.mLength) {
                            mCallback.removeMessages(BluetoothOppObexSession.MSG_CONNECT_TIMEOUT);
                            synchronized (this) {
                                mWaitingForRemote = false;
                            }
                        } else {
                            // if file length is smaller than buffer size, only one packet
                            // so block point is here
                            outputStream.close();
                            mCallback.removeMessages(BluetoothOppObexSession.MSG_CONNECT_TIMEOUT);
                            synchronized (this) {
                                mWaitingForRemote = false;
                            }
                        }
                        /* check remote accept or reject */
                        responseCode = putOperation.getResponseCode();

                        if (responseCode == ResponseCodes.OBEX_HTTP_CONTINUE
                                || responseCode == ResponseCodes.OBEX_HTTP_OK) {
                            if (V) Log.v(TAG, "Remote accept");

                            reply = putOperation.getReceivedHeader();
                            Byte srm = (Byte)reply.getHeader(HeaderSet.SINGLE_RESPONSE_MODE);
                            if (srm == ObexHelper.OBEX_SRM_ENABLED) {
                                Log.v(TAG, "SRM status: Enabled by Server response");
                                mCs.mSrmClient.setLocalSrmStatus(ObexHelper.LOCAL_SRM_ENABLED);
                                Byte srmp = (Byte)reply.getHeader(HeaderSet.SINGLE_RESPONSE_MODE_PARAMETER);
                                Log.v(TAG, "SRMP header (CONTINUE or OK): " + srmp);
                                if (srmp == ObexHelper.OBEX_SRM_PARAM_WAIT) {
                                    Log.v(TAG, "SRMP status: WAIT");
                                    mCs.mSrmClient.setLocalSrmpWait(true);
                                } else {
                                    Log.v(TAG, "SRMP status: NONE");
                                    mCs.mSrmClient.setLocalSrmpWait(false);
                                }
                            } else {
                                Log.v(TAG, "SRM status: Disabled by Server response");
                                mCs.mSrmClient.setLocalSrmStatus(ObexHelper.LOCAL_SRM_DISABLED);
                                mCs.mSrmClient.setLocalSrmpWait(false);
                            }
                            okToProceed = true;
                            updateValues = new ContentValues();
                            updateValues.put(BluetoothShare.CURRENT_BYTES, position);
                            mContext1.getContentResolver().update(contentUri, updateValues, null,
                                    null);
                        } else {
                            Log.i(TAG, "Remote reject, Response code is " + responseCode);
                        }
                    }
                    long beginTime = System.currentTimeMillis();
                    while (!mInterrupted && okToProceed && (position != fileInfo.mLength)) {
                        {
                            if (V) timestamp = System.currentTimeMillis();

                            readbytesleft = fileInfo.mLength - position;
                            if(readbytesleft < outputBufferSize) {
                                outputBufferSize = (int) readbytesleft;
                            }

                            readLength = a.read(buffer, 0, outputBufferSize);
                            outputStream.write(buffer, 0, readLength);

                            /* check remote abort */
                            responseCode = putOperation.getResponseCode();
                            if (V) Log.v(TAG, "Response code is " + responseCode);
                            if (responseCode != ResponseCodes.OBEX_HTTP_CONTINUE
                                    && responseCode != ResponseCodes.OBEX_HTTP_OK) {
                                /* abort happens */
                                okToProceed = false;
                            } else {
                                position += readLength;
                                if (V) {
                                    Log.v(TAG, "Sending file position = " + position
                                            + " readLength " + readLength + " bytes took "
                                            + (System.currentTimeMillis() - timestamp) + " ms");
                                }

                                if (uiUpdateThread == null) {
                                    uiUpdateThread = new ContentResolverUpdateThread (mContext1,
                                                                    contentUri, position);
                                    uiUpdateThread.start ( );
                                } else {
                                    uiUpdateThread.updateProgress (position);
                                }

                            }
                        }
                    }

                    if (uiUpdateThread != null) {
                        try {
                            uiUpdateThread.interrupt ();
                            uiUpdateThread.join ();
                            uiUpdateThread = null;

                            updateValues = new ContentValues();
                            updateValues.put(BluetoothShare.CURRENT_BYTES, position);
                            mContext1.getContentResolver().update(contentUri, updateValues,
                                        null, null);
                        } catch (InterruptedException ie) {
                            if (V) Log.v(TAG, "Interrupted waiting for uiUpdateThread to join");
                        }
                    }

                    if (responseCode == ResponseCodes.OBEX_HTTP_FORBIDDEN
                            || responseCode == ResponseCodes.OBEX_HTTP_NOT_ACCEPTABLE) {
                        Log.i(TAG, "Remote reject file " + fileInfo.mFileName + " length "
                                + fileInfo.mLength);
                        status = BluetoothShare.STATUS_FORBIDDEN;
                    } else if (responseCode == ResponseCodes.OBEX_HTTP_UNSUPPORTED_TYPE) {
                        Log.i(TAG, "Remote reject file type " + fileInfo.mMimetype);
                        status = BluetoothShare.STATUS_NOT_ACCEPTABLE;
                    } else if (!mInterrupted && position == fileInfo.mLength) {
                        long endTime = System.currentTimeMillis();
                        Log.i(TAG, "SendFile finished sending file " + fileInfo.mFileName
                                + " length " + fileInfo.mLength
                                + "Bytes in " + (endTime - beginTime) + "ms"  );
                        outputStream.close();
                    } else {
                        error = true;
                        status = BluetoothShare.STATUS_CANCELED;
                        putOperation.abort();
                        /* interrupted */
                        Log.i(TAG, "SendFile interrupted when send out file " + fileInfo.mFileName
                                + " at " + position + " of " + fileInfo.mLength);
                    }
                }
            } catch (IOException e) {
                Log.e(TAG, "IOException", e);
                handleSendException(e.toString());
            } catch (NullPointerException e) {
                Log.e(TAG, "NullPointerException", e);
                handleSendException(e.toString());
            } catch (IndexOutOfBoundsException e) {
                Log.e(TAG, "IndexOutOfBoundsException", e);
                handleSendException(e.toString());
            } finally {
                try {

                    if (uiUpdateThread != null) {
                        uiUpdateThread.interrupt ();
                        uiUpdateThread = null;
                    }

                    fileInfo.mInputStream.close();
                    if (!error) {
                        responseCode = putOperation.getResponseCode();
                        if (responseCode != -1) {
                            if (V) Log.v(TAG, "Get response code " + responseCode);
                            if (responseCode != ResponseCodes.OBEX_HTTP_OK) {
                               if ((fileInfo.mLength == 0) &&
                                  (responseCode == ResponseCodes.OBEX_HTTP_LENGTH_REQUIRED)) {
                                  /* Set if the file length is zero and it's rejected by remote */
                                  Constants.ZERO_LENGTH_FILE = true;
                                  /* To mark transfer status as failed in the notification */
                                  status = BluetoothShare.STATUS_FORBIDDEN;
                               } else {
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
                    Log.e(TAG, "IOException", e);
                    Log.e(TAG, "Error when closing stream after send");
                    /* Socket is been closed due to the response timeout in the framework
                     * Hence, mark the transfer as failure
                     */
                    if (position != fileInfo.mLength) {
                       status = BluetoothShare.STATUS_FORBIDDEN;
                       Constants.updateShareStatus(mContext1, mInfo.mId, status);
                    }
                }
            }
            return status;
        }

        private void handleSendException(String exception) {
            Log.e(TAG, "Error when sending file: " + exception);
            int status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
            Constants.updateShareStatus(mContext1, mInfo.mId, status);
            mCallback.removeMessages(BluetoothOppObexSession.MSG_CONNECT_TIMEOUT);
        }

        @Override
        public void interrupt() {
            super.interrupt();
            synchronized (this) {
                if (mWaitingForRemote) {
                    if (V) Log.v(TAG, "Interrupted when waitingForRemote");
                    try {
                        mTransport1.close();
                    } catch (IOException e) {
                        Log.e(TAG, "mTransport.close error");
                    }
                    Message msg = Message.obtain(mCallback);
                    msg.what = BluetoothOppObexSession.MSG_SHARE_INTERRUPTED;
                    if (mInfo != null) {
                        msg.obj = mInfo;
                    }
                    msg.sendToTarget();
                }
            }
        }
    }

    public static void applyRemoteDeviceQuirks(HeaderSet request, BluetoothOppSendFileInfo info) {
        String address = info.mDestAddr;
        if (address == null) {
            return;
        }
        if (address.startsWith("00:04:48")) {
            // Poloroid Pogo
            // Rejects filenames with more than one '.'. Rename to '_'.
            // for example: 'a.b.jpg' -> 'a_b.jpg'
            //              'abc.jpg' NOT CHANGED
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

}
