/*
 * Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileInputStream;
import java.io.BufferedInputStream;
import java.util.Arrays;

import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.os.Process;
import android.util.Log;
import android.webkit.MimeTypeMap;

import android.os.SystemProperties;
import android.database.Cursor;
import android.provider.ContactsContract.Contacts;
import android.provider.ContactsContract.Profile;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.lang.Thread;


import javax.btobex.HeaderSet;
import javax.btobex.ObexHelper;
import javax.btobex.ObexTransport;
import javax.btobex.Operation;
import javax.btobex.ResponseCodes;
import javax.btobex.ServerRequestHandler;
import javax.btobex.ServerSession;
import javax.btobex.ServerOperation;

/**
 * This class runs as an OBEX server
 */
public class BluetoothOppObexServerSession extends ServerRequestHandler implements
        BluetoothOppObexSession {

    private static final String TAG = "BtOppObexServer";
    private static final boolean D = Constants.DEBUG;
    private static final boolean V = Constants.VERBOSE;

    private static final String OPP_SERVER_DIR_PATH = "/data/data/com.android.bluetooth/oppserver";
    private static final String OPP_MIME_TYPE = "text/x-vcard";
    private static final String OPP_DEFAULT_vCARD_NAME = "default.vcf";
    private static final String OPP_myProfile_NAME = "myprofile.vcf";
    private static final int OPP_LOCAL_BUF_SIZE  = 0x4000;

    private ObexTransport mTransport;

    private Context mContext;

    private Handler mCallback = null;

    /* status when server is blocking for user/auto confirmation */
    private boolean mServerBlocking = true;

    /* the current transfer info */
    private BluetoothOppShareInfo mInfo;

    /* info id when we insert the record */
    private int mLocalShareInfoId;

    private int mAccepted = BluetoothShare.USER_CONFIRMATION_PENDING;

    private boolean mInterrupted = false;

    private ServerSession mSession;

    private long mTimestamp;

    private BluetoothOppReceiveFileInfo mFileInfo;

    private WakeLock mWakeLock;

    private WakeLock mPartialWakeLock;

    /* To control multiple incoming connection request */
    static boolean mServerSessionIsUp = false;

    boolean mTimeoutMsgSent = false;

    boolean mTransferInProgress = false;

    public BluetoothOppObexServerSession(Context context, ObexTransport transport) {
        mContext = context;
        mTransport = transport;
        PowerManager pm = (PowerManager)mContext.getSystemService(Context.POWER_SERVICE);
        mWakeLock = pm.newWakeLock(PowerManager.FULL_WAKE_LOCK | PowerManager.ACQUIRE_CAUSES_WAKEUP
                | PowerManager.ON_AFTER_RELEASE, TAG);
        mPartialWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, TAG);
    }

    public void unblock() {
        mServerBlocking = false;
    }

    /**
     * Called when connection is accepted from remote, to retrieve the first
     * Header then wait for user confirmation
     */
    public void preStart() {
        if (D) Log.d(TAG, "acquire full WakeLock");
        mWakeLock.acquire();
        try {
            if (D) Log.d(TAG, "Create ServerSession with transport " + mTransport.toString());
            mSession = new ServerSession(mTransport, this, null);
            int mps = ((BluetoothOppTransport)mTransport).getMaxPacketSize();
            mSession.setMaxPacketSize(mps);
            if (D) Log.d(TAG, "Setting ServerSession mps " + mps);

            // Turn on/off SRM based on transport capability (whether this is OBEX-over-L2CAP, or not)
            mSession.mSrmServer.setLocalSrmCapability(((BluetoothOppTransport)mTransport).isSrmCapable());

            if (!mSession.mSrmServer.getLocalSrmCapability()) {
                mSession.mSrmServer.setLocalSrmParamStatus(ObexHelper.SRMP_DISABLED);
            }
            /* Server session is created */
            mServerSessionIsUp = true;
        } catch (IOException e) {
            Log.e(TAG, "Create server session error" + e);
        }
    }

    /**
     * Called from BluetoothOppTransfer to start the "Transfer"
     */
    public void start(Handler handler) {
        if (D) Log.d(TAG, "Start!");
        mCallback = handler;

    }

    /**
     * Called from BluetoothOppTransfer to cancel the "Transfer" Otherwise,
     * server should end by itself.
     */
    public void stop() {
        /*
         * TODO now we implement in a tough way, just close the socket.
         * maybe need nice way
         */
        if (D) Log.d(TAG, "Stop!");
        mInterrupted = true;
        if (mSession != null) {
            try {
                mSession.close();
                mTransport.close();
            } catch (IOException e) {
                Log.e(TAG, "close mTransport error" + e);
            }
        }
        mCallback = null;
        mSession = null;
    }

    private class ContentResolverUpdateThread extends Thread {

        private static final int sSleepTime = 500;
        private Uri contentUri;
        private Context mContext1;
        private long position;
        private volatile boolean interrupted = false;

        public ContentResolverUpdateThread(Context context, Uri cntUri, long pos) {
            super("BtOpp Server ContentResolverUpdateThread");
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
                    if (V) Log.v(TAG, "Server ContentResolverUpdateThread was interrupted (1), exiting");
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

    /*
    * Called when a ABORT request is received.
    */
    @Override
    public int onAbort(HeaderSet request, HeaderSet reply) {
        if (D) Log.d(TAG, "onAbort()");
        if (mTransferInProgress) {
           return ResponseCodes.OBEX_HTTP_OK;
        } else {
            /* Transfer is completed already or the command is received
             * when no transfer in progress. Send -ve response.
             */
            return ResponseCodes.OBEX_HTTP_NOT_ACCEPTABLE;
        }
    }

    public void addShare(BluetoothOppShareInfo info) {
        if (D) Log.d(TAG, "addShare for id " + info.mId);
        mInfo = info;
        mFileInfo = processShareInfo();
    }

    @Override
    public int onPut(Operation op) {
        if (D) Log.d(TAG, "onPut " + op.toString());
        HeaderSet request;
        String name, mimeType;
        Long length;
        Byte srm;

        int obexResponse = ResponseCodes.OBEX_HTTP_OK;

        /**
         * For multiple objects, reject further objects after user deny the
         * first one
         */
        if (mAccepted == BluetoothShare.USER_CONFIRMATION_DENIED ||
            mAccepted == BluetoothShare.USER_CONFIRMATION_TIMEOUT) {
            return ResponseCodes.OBEX_HTTP_FORBIDDEN;
        }

        String destination;
        if (mTransport instanceof BluetoothOppTransport) {
            destination = ((BluetoothOppTransport)mTransport).getRemoteAddress();
        } else {
            destination = "FF:FF:FF:00:00:00";
        }
        boolean isWhitelisted = BluetoothOppManager.getInstance(mContext).
                isWhitelisted(destination);

        try {
            boolean pre_reject = false;

            request = op.getReceivedHeader();
            if (V) Constants.logHeader(request);
            name = (String)request.getHeader(HeaderSet.NAME);
            length = (Long)request.getHeader(HeaderSet.LENGTH);
            mimeType = (String)request.getHeader(HeaderSet.TYPE);

            if (((ServerOperation)op).mSrmServerSession.getLocalSrmCapability() == ObexHelper.SRM_CAPABLE) {
                if (V) Log.v(TAG, "Local Device SRM: Capable");

                srm = (Byte)request.getHeader(HeaderSet.SINGLE_RESPONSE_MODE);
                if (srm == ObexHelper.OBEX_SRM_ENABLED) {
                    if (V) Log.v(TAG, "SRM status: Enabled");
                    ((ServerOperation)op).mSrmServerSession.setLocalSrmStatus(ObexHelper.LOCAL_SRM_ENABLED);
                } else {
                    if (V) Log.v(TAG, "SRM status: Disabled");
                    ((ServerOperation)op).mSrmServerSession.setLocalSrmStatus(ObexHelper.LOCAL_SRM_DISABLED);
                }
            } else {
                if (V) Log.v(TAG, "Local Device SRM: Incapable");
                ((ServerOperation)op).mSrmServerSession.setLocalSrmStatus(ObexHelper.LOCAL_SRM_DISABLED);
            }

            if (length == 0) {
                if (D) Log.w(TAG, "length is 0, reject the transfer");
                pre_reject = true;
                obexResponse = ResponseCodes.OBEX_HTTP_LENGTH_REQUIRED;
            }

            if (name == null || name.equals("")) {
                if (D) Log.w(TAG, "name is null or empty, reject the transfer");
                pre_reject = true;
                obexResponse = ResponseCodes.OBEX_HTTP_BAD_REQUEST;
            }

            if (!pre_reject) {
                /* first we look for Mimetype in Android map */
                String extension, type;
                int dotIndex = name.lastIndexOf(".");
                if (dotIndex < 0 && mimeType == null) {
                    if (D) Log.w(TAG, "There is no file extension or mime type," +
                            "reject the transfer");
                    pre_reject = true;
                    obexResponse = ResponseCodes.OBEX_HTTP_BAD_REQUEST;
                } else {
                    extension = name.substring(dotIndex + 1).toLowerCase();
                    MimeTypeMap map = MimeTypeMap.getSingleton();
                    type = map.getMimeTypeFromExtension(extension);
                    if (V) Log.v(TAG, "Mimetype guessed from extension " + extension + " is " + type);
                    if (type != null) {
                        mimeType = type;

                    } else {
                        if (mimeType == null) {
                            if (D) Log.w(TAG, "Can't get mimetype, reject the transfer");
                            pre_reject = true;
                            obexResponse = ResponseCodes.OBEX_HTTP_UNSUPPORTED_TYPE;
                        }
                    }
                    if (mimeType != null) {
                        mimeType = mimeType.toLowerCase();
                    }
                }
            }

            // Reject policy: anything outside the "white list" plus unspecified
            // MIME Types. Also reject everything in the "black list".
            if (!pre_reject
                    && (mimeType == null
                            || (!isWhitelisted && !Constants.mimeTypeMatches(mimeType,
                                    Constants.ACCEPTABLE_SHARE_INBOUND_TYPES))
                            || Constants.mimeTypeMatches(mimeType,
                                    Constants.UNACCEPTABLE_SHARE_INBOUND_TYPES))) {
                if (D) Log.w(TAG, "mimeType is null or in unacceptable list, reject the transfer");
                pre_reject = true;
                obexResponse = ResponseCodes.OBEX_HTTP_UNSUPPORTED_TYPE;
            }

            if (pre_reject && obexResponse != ResponseCodes.OBEX_HTTP_OK) {
                // some bad implemented client won't send disconnect
                return obexResponse;
            }

        } catch (IOException e) {
            Log.e(TAG, "get getReceivedHeaders error " + e);
            return ResponseCodes.OBEX_HTTP_BAD_REQUEST;
        }

        ContentValues values = new ContentValues();

        values.put(BluetoothShare.FILENAME_HINT, name);
        values.put(BluetoothShare.TOTAL_BYTES, length);
        values.put(BluetoothShare.MIMETYPE, mimeType);

        if(mTransport instanceof BluetoothOppTransport){
            String a = ((BluetoothOppTransport)mTransport).getRemoteAddress();
            values.put(BluetoothShare.DESTINATION, a);
        } else {
            values.put(BluetoothShare.DESTINATION, "FF:FF:FF:00:00:00");
        }

        values.put(BluetoothShare.DIRECTION, BluetoothShare.DIRECTION_INBOUND);
        values.put(BluetoothShare.TIMESTAMP, mTimestamp);

        boolean needConfirm = true;
        /** It's not first put if !serverBlocking, so we auto accept it */
        if (!mServerBlocking) {
            values.put(BluetoothShare.USER_CONFIRMATION,
                    BluetoothShare.USER_CONFIRMATION_AUTO_CONFIRMED);
            needConfirm = false;
        }

        if (isWhitelisted) {
            values.put(BluetoothShare.USER_CONFIRMATION,
                    BluetoothShare.USER_CONFIRMATION_HANDOVER_CONFIRMED);
            needConfirm = false;
        }

        Uri contentUri = mContext.getContentResolver().insert(BluetoothShare.CONTENT_URI, values);
        mLocalShareInfoId = Integer.parseInt(contentUri.getPathSegments().get(1));

        if (needConfirm) {
            Intent in = new Intent(BluetoothShare.INCOMING_FILE_CONFIRMATION_REQUEST_ACTION);
            in.setClassName(Constants.THIS_PACKAGE_NAME, BluetoothOppReceiver.class.getName());
            mContext.sendBroadcast(in);
        }

        if (V) Log.v(TAG, "insert contentUri: " + contentUri);
        if (V) Log.v(TAG, "mLocalShareInfoId = " + mLocalShareInfoId);

        if (V) Log.v(TAG, "acquire partial WakeLock");


        synchronized (this) {
            if (mWakeLock.isHeld()) {
                mPartialWakeLock.acquire();
                mWakeLock.release();
            }
            mServerBlocking = true;
            try {

                while (mServerBlocking) {
                    wait(1000);
                    if (mCallback != null && !mTimeoutMsgSent) {
                        mCallback.sendMessageDelayed(mCallback
                                .obtainMessage(BluetoothOppObexSession.MSG_CONNECT_TIMEOUT),
                                BluetoothOppObexSession.SESSION_TIMEOUT);
                        mTimeoutMsgSent = true;
                        if (V) Log.v(TAG, "MSG_CONNECT_TIMEOUT sent");
                    }
                }
            } catch (InterruptedException e) {
                if (V) Log.v(TAG, "Interrupted in onPut blocking");
            }
        }
        if (D) Log.d(TAG, "Server unblocked ");
        synchronized (this) {
            if (mCallback != null && mTimeoutMsgSent) {
                mCallback.removeMessages(BluetoothOppObexSession.MSG_CONNECT_TIMEOUT);
            }
        }

        /* we should have mInfo now */

        /*
         * TODO check if this mInfo match the one that we insert before server
         * blocking? just to make sure no error happens
         */
        if (mInfo.mId != mLocalShareInfoId) {
            Log.e(TAG, "Unexpected error!");
        }
        mAccepted = mInfo.mConfirm;

        if (V) Log.v(TAG, "after confirm: userAccepted=" + mAccepted);
        int status = BluetoothShare.STATUS_SUCCESS;

        if (mAccepted == BluetoothShare.USER_CONFIRMATION_CONFIRMED
                || mAccepted == BluetoothShare.USER_CONFIRMATION_AUTO_CONFIRMED
                || mAccepted == BluetoothShare.USER_CONFIRMATION_HANDOVER_CONFIRMED) {
            /* Confirm or auto-confirm */

            if (mFileInfo.mFileName == null) {
                status = mFileInfo.mStatus;
                /* TODO need to check if this line is correct */
                mInfo.mStatus = mFileInfo.mStatus;
                Constants.updateShareStatus(mContext, mInfo.mId, status);
                obexResponse = ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;

            }

            if (mFileInfo.mFileName != null) {

                ContentValues updateValues = new ContentValues();
                contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + mInfo.mId);
                updateValues.put(BluetoothShare._DATA, mFileInfo.mFileName);
                updateValues.put(BluetoothShare.STATUS, BluetoothShare.STATUS_RUNNING);
                mContext.getContentResolver().update(contentUri, updateValues, null, null);

                status = receiveFile(mFileInfo, op);
                /*
                 * TODO map status to obex response code
                 */
                if (status != BluetoothShare.STATUS_SUCCESS) {
                    obexResponse = ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
                }
                Constants.updateShareStatus(mContext, mInfo.mId, status);
            }

        } else if (mAccepted == BluetoothShare.USER_CONFIRMATION_DENIED
                || mAccepted == BluetoothShare.USER_CONFIRMATION_TIMEOUT) {
            /* user actively deny the inbound transfer */
            /*
             * Note There is a question: what's next if user deny the first obj?
             * Option 1 :continue prompt for next objects
             * Option 2 :reject next objects and finish the session
             * Now we take option 2:
             */

            Log.i(TAG, "Rejected incoming request");
            if (mFileInfo.mFileName != null) {
                try {
                    mFileInfo.mOutputStream.close();
                } catch (IOException e) {
                    Log.e(TAG, "error close file stream");
                }
                new File(mFileInfo.mFileName).delete();
            }
            // set status as local cancel
            status = BluetoothShare.STATUS_CANCELED;
            Constants.updateShareStatus(mContext, mInfo.mId, status);
            obexResponse = ResponseCodes.OBEX_HTTP_FORBIDDEN;

            if (mCallback != null) {
                Message msg = Message.obtain(mCallback);
                msg.what = BluetoothOppObexSession.MSG_SHARE_INTERRUPTED;
                mInfo.mStatus = status;
                if (msg != null) {
                    msg.obj = mInfo;
                    msg.sendToTarget();
                } else {
                    Log.e(TAG, "Could not get message!");
                }
            } else {
                Log.e(TAG, "Error! mCallback is null");
            }
        }
        return obexResponse;
    }

    private int receiveFile(BluetoothOppReceiveFileInfo fileInfo, Operation op) {
        /*
         * implement receive file
         */
        long beginTime = 0;
        int status = -1;
        BufferedOutputStream bos = null;
        ContentResolverUpdateThread uiUpdateThread = null;

        InputStream is = null;
        boolean error = false;
        try {
            is = op.openInputStream();
        } catch (IOException e1) {
            Log.e(TAG, "Error when openInputStream");
            status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
            error = true;
        }

        Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + mInfo.mId);

        if (!error) {
            ContentValues updateValues = new ContentValues();
            updateValues.put(BluetoothShare._DATA, fileInfo.mFileName);
            mContext.getContentResolver().update(contentUri, updateValues, null, null);
        }

        long position = 0;
        if (!error) {
            bos = new BufferedOutputStream(fileInfo.mOutputStream, 0x10000);
        }

        if (!error) {
            int outputBufferSize = op.getMaxPacketSize();
            byte[] b = new byte[outputBufferSize];
            int readLength = 0;
            long timestamp = 0;
            try {
                beginTime = System.currentTimeMillis();
                while ((!mInterrupted) && (position != fileInfo.mLength)) {

                    if (V) timestamp = System.currentTimeMillis();

                    readLength = is.read(b);

                    if (readLength == -1) {
                        if (D) Log.d(TAG, "Receive file reached stream end at position" + position);
                        break;
                    }

                    bos.write(b, 0, readLength);
                    position += readLength;

                    if (V) {
                        Log.v(TAG, "Receive file position = " + position + " readLength "
                                + readLength + " bytes took "
                                + (System.currentTimeMillis() - timestamp) + " ms");
                    }

                    if (uiUpdateThread == null) {
                        uiUpdateThread = new ContentResolverUpdateThread (mContext, contentUri, position);
                        if (V) {
                            Log.v(TAG, "Worker for Updation : Created");
                        }
                        uiUpdateThread.start();
                    } else {
                        uiUpdateThread.updateProgress (position);
                    }
                }

                if (uiUpdateThread != null) {
                    try {
                        if (V) {
                            Log.v(TAG, "Worker for Updation : Destroying");
                        }
                        uiUpdateThread.interrupt ();
                        uiUpdateThread.join ();
                        uiUpdateThread = null;

                        ContentValues updateValues = new ContentValues();
                        updateValues.put(BluetoothShare.CURRENT_BYTES, position);
                        mContext.getContentResolver().update(contentUri, updateValues,
                                        null, null);
                    } catch (InterruptedException ie) {
                            if (V) Log.v(TAG, "Interrupted waiting for uiUpdateThread to join");
                    }
                }
            } catch (IOException e1) {
                Log.e(TAG, "Error when receiving file");
                /* OBEX Abort packet received from remote device */
                if ("Abort Received".equals(e1.getMessage())) {
                    status = BluetoothShare.STATUS_CANCELED;
                } else {
                    status = BluetoothShare.STATUS_OBEX_DATA_ERROR;
                }
                error = true;
            } finally {
                if (uiUpdateThread != null) {
                    if (V) {
                        Log.v(TAG, "Worker for Updation : Finally Destroying");
                    }
                    uiUpdateThread.interrupt ();
                    uiUpdateThread = null;
                }
            }
        }

        if (mInterrupted) {
            if (D) Log.d(TAG, "receiving file interrupted by user.");
            status = BluetoothShare.STATUS_CANCELED;
        } else {
            if (position == fileInfo.mLength) {
                long endTime = System.currentTimeMillis();
                Log.i(TAG, "Receiving file completed for " + fileInfo.mFileName
                        + " length " + fileInfo.mLength
                        + " Bytes in " + (endTime - beginTime) + "ms"  );
                status = BluetoothShare.STATUS_SUCCESS;
            } else {
                Log.i(TAG, "Reading file failed at " + position + " of " + fileInfo.mLength);
                if (status == -1) {
                    status = BluetoothShare.STATUS_UNKNOWN_ERROR;
                }
            }
        }

        if (bos != null) {
            try {
                bos.close();
            } catch (IOException e) {
                Log.e(TAG, "Error when closing stream after send");
            }
        }
        return status;
    }

    private BluetoothOppReceiveFileInfo processShareInfo() {
        if (D) Log.d(TAG, "processShareInfo() " + mInfo.mId);
        BluetoothOppReceiveFileInfo fileInfo = BluetoothOppReceiveFileInfo.generateFileInfo(
                mContext, mInfo.mId);
        if (V) {
            Log.v(TAG, "Generate BluetoothOppReceiveFileInfo:");
            Log.v(TAG, "filename  :" + fileInfo.mFileName);
            Log.v(TAG, "length    :" + fileInfo.mLength);
            Log.v(TAG, "status    :" + fileInfo.mStatus);
        }
        return fileInfo;
    }

    public File getMyBusinessCard ( ) {
        Log.v(TAG, " getMyBusinessCard()");
        Uri myProfileUri = Profile.CONTENT_URI;
        Uri lookupUri = null;
        Uri shareUri = null;
        Log.v(TAG," My Profile Uri  " + myProfileUri);
        Cursor c = mContext.getContentResolver().query(myProfileUri,
          new String[] {Contacts._ID, Contacts.LOOKUP_KEY},null, null, null);
        try{
            if (c.moveToFirst()) {
                final long contactId = c.getLong(0);
                final String lookupKey = c.getString(1);
                lookupUri = Contacts.getLookupUri(contactId, lookupKey);
                if(V) Log.v(TAG," My LookupUri  " + lookupUri + " LookUpKey "+ lookupKey);
                shareUri = Uri.withAppendedPath(Contacts.CONTENT_VCARD_URI, lookupKey);
                if(V) Log.v(TAG," My ShareUri  " + shareUri);
            }
        } finally {
            c.close();
        }

        File dir   =  new File(OPP_SERVER_DIR_PATH);
        File myCard = new File(OPP_SERVER_DIR_PATH, OPP_myProfile_NAME);
        if((dir.exists())&&(myCard.exists())) {
            Log.v(TAG," Directory and File is alredy There, Delete it");
            myCard.delete();
            dir.delete();
        }
        if(shareUri != null){
            InputStream i = null;
            int length;
            byte[] buffer = new byte[1024];
            try {
                if(!dir.exists()){
                    dir.mkdirs();
                    }else {
                    Log.d(TAG," ** Not able to Create DIR ***");
                    }
                if(!myCard.createNewFile()){
                    Log.d(TAG," my Profile vcard not created");
                    return myCard;
                }
                try {
                    i=mContext.getContentResolver().openInputStream(Uri.parse(shareUri.toString()));
                } catch (FileNotFoundException e) {
                    Log.d(TAG," Not able to open URI" + e.toString());
                }
                FileOutputStream fos = new FileOutputStream(myCard);
                do {
                    if(i == null)
                        break;
                    length = i.read(buffer);
                    if(length == -1)
                        break;
                    fos.write(buffer,0,length);
                }while(true);
             }catch(IOException e) {
                Log.e(TAG,"Not able to read myVcard"+ e.toString());
            }
            return myCard;
        }else {
            File mDefaultvCard = new File(OPP_SERVER_DIR_PATH, OPP_DEFAULT_vCARD_NAME);
            return mDefaultvCard;
        }
    }

    @Override
    public int onGet(Operation op) {
        if (D) Log.d(TAG, "onGet() +");

        HeaderSet request = null;
        String type = "";
        String name = "";
        OutputStream outputStream = null;
        FileInputStream  fis = null;
        BufferedInputStream bis = null;
        long fileLength = 0;
        long fileReadPos = 0;
        int readLength;
        Byte srm;
        File myCard = null;
        if(SystemProperties.getBoolean("ro.qualcomm.bluetooth.sndmyinfo", true)){
            myCard = getMyBusinessCard ( );
        } else {
            return ResponseCodes.OBEX_HTTP_NOT_IMPLEMENTED;
        }
        int outputBufferSize = op.getMaxPacketSize();
        /* Extract the name and type header from operation object */
        try {
            request = op.getReceivedHeader();
            type = (String)request.getHeader(HeaderSet.TYPE);

            name = (String)request.getHeader(HeaderSet.NAME);

        } catch (IOException e) {
            Log.e(TAG,"onGet request headers "+ e.toString());
            if (D) Log.d(TAG, "request headers error");
        }
        if (Constants.mimeTypeMatches(type, OPP_MIME_TYPE)  == false) {
            return ResponseCodes.OBEX_HTTP_UNSUPPORTED_TYPE;
        }

        if (D) Log.d(TAG,"type = " + type);
        if (D && (name != null)) Log.d(TAG, " name = " + name);
        if ((myCard.exists() != true) || (myCard.length() == 0)) {
            Log.e(TAG, "Default Business Card Not Found ! ");
            return ResponseCodes.OBEX_HTTP_NOT_FOUND;
        }

        try {
            fis = new FileInputStream(myCard);
            outputStream = op.openOutputStream();
        } catch(IOException e) {
            Log.e(TAG,"OPP Pull Business Card : open stream Exception"+ e.toString());
            return ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
        } finally {
            if (fis != null && outputStream == null) {
                try {
                    fis.close();
                } catch(IOException e) {}
                return ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
            }
        }

        try {
            if (((ServerOperation)op).mSrmServerSession.getLocalSrmCapability() == ObexHelper.SRM_CAPABLE) {
                if (V) Log.v(TAG, "Local Device SRM: Capable");

                srm = (Byte)request.getHeader(HeaderSet.SINGLE_RESPONSE_MODE);
                if (srm == ObexHelper.OBEX_SRM_ENABLED) {
                    if (V) Log.v(TAG, "SRM status: Enabled");
                    ((ServerOperation)op).mSrmServerSession.setLocalSrmStatus(ObexHelper.LOCAL_SRM_ENABLED);
                    Byte srmp = (Byte)request.getHeader(HeaderSet.SINGLE_RESPONSE_MODE_PARAMETER);
                    Log.v(TAG, "SRMP header (CONTINUE or OK): " + srmp);
                    if (srmp == ObexHelper.OBEX_SRM_PARAM_WAIT) {
                        Log.v(TAG, "SRMP status: WAIT");
                        ((ServerOperation)op).mSrmServerSession.setLocalSrmpWait(true);
                    } else {
                        Log.v(TAG, "SRMP status: NONE");
                        ((ServerOperation)op).mSrmServerSession.setLocalSrmpWait(false);
                    }

                } else {
                    if (V) Log.v(TAG, "SRM status: Disabled");
                    ((ServerOperation)op).mSrmServerSession.setLocalSrmStatus(ObexHelper.LOCAL_SRM_DISABLED);
                    ((ServerOperation)op).mSrmServerSession.setLocalSrmpWait(false);
                }
            } else {
                if (V) Log.v(TAG, "Local Device SRM: Incapable");
                ((ServerOperation)op).mSrmServerSession.setLocalSrmStatus(ObexHelper.LOCAL_SRM_DISABLED);
                ((ServerOperation)op).mSrmServerSession.setLocalSrmpWait(false);
            }
        } catch (IOException e) {
                Log.e(TAG, "get getReceivedHeaders for SRM error " + e);
                return ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
        }

        byte[] buffer = new byte[outputBufferSize];

        /* Read the file and send */
        fileLength = myCard.length ( );
        HeaderSet reply = new HeaderSet();
        reply.setHeader(HeaderSet.NAME, OPP_DEFAULT_vCARD_NAME);
        reply.setHeader(HeaderSet.TYPE, type);
        reply.setHeader(HeaderSet.LENGTH, fileLength);

        bis = new BufferedInputStream(fis, OPP_LOCAL_BUF_SIZE);
        try {
            op.sendHeaders (reply);
            while (fileReadPos != fileLength) {
                readLength = bis.read(buffer, 0, outputBufferSize);
                fileReadPos += readLength;

                if(((ServerOperation)op).isAborted() != true) {
                    outputStream.write(buffer, 0, readLength);
                    /* To handle abort request depends on the current state */
                    mTransferInProgress = true;
                    if (((ServerOperation)op).mSrmServerSession.getLocalSrmpWait( ) == true) {
                        try {
                            request = op.getReceivedHeader();
                            Byte srmp = (Byte)request.getHeader(HeaderSet.SINGLE_RESPONSE_MODE_PARAMETER);
                            Log.v(TAG, "SRMP header (CONTINUE or OK): " + srmp);
                            if (srmp == ObexHelper.OBEX_SRM_PARAM_WAIT) {
                                Log.v(TAG, "SRMP status: WAIT");
                                ((ServerOperation)op).mSrmServerSession.setLocalSrmpWait(true);
                            } else {
                                Log.v(TAG, "SRMP status: NONE");
                                ((ServerOperation)op).mSrmServerSession.setLocalSrmpWait(false);
                            }
                        } catch (IOException e) {
                            Log.e(TAG, "Failed to read SRMP Header " + e);
                        }
                    }
                } else {
                    /* Abort command is received. Bring down the GET operation */
                    Log.v(TAG, "Abort is received");
                    break;
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "OPP Pull Business Card : Write outputstrem failed" + e.toString());
            return ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
        }

        /* Close the streams */
        if (bis != null) {
           try {
              bis.close();
            } catch (IOException e) {
                Log.e(TAG,"input stream close" + e.toString());
                if (D) Log.d(TAG, "Error when closing stream after send");
                return ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
            }
        }

        if (!closeStream(outputStream, op)) {
          return ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
        }

        /* Reset the state */
        mTransferInProgress = false;
        if (D) Log.d(TAG, "onGet() -");
        return ResponseCodes.OBEX_HTTP_OK;
    }

    @Override
    public int onConnect(HeaderSet request, HeaderSet reply) {

        if (D) Log.d(TAG, "onConnect");
        if (V) Constants.logHeader(request);
        try {
            byte[] uuid = (byte[])request.getHeader(HeaderSet.TARGET);
            if (V) Log.v(TAG, "onConnect(): uuid =" + Arrays.toString(uuid));
            if(uuid != null) {
                 return ResponseCodes.OBEX_HTTP_NOT_ACCEPTABLE;
            }
        } catch (IOException e) {
            Log.e(TAG, e.toString());
            return ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
        }
        mTimestamp = System.currentTimeMillis();
        return ResponseCodes.OBEX_HTTP_OK;
    }

    @Override
    public void onDisconnect(HeaderSet req, HeaderSet resp) {
        if (D) Log.d(TAG, "onDisconnect");
        mServerSessionIsUp = false;
        resp.responseCode = ResponseCodes.OBEX_HTTP_OK;
    }

    private synchronized void releaseWakeLocks() {
        if (mWakeLock.isHeld()) {
            mWakeLock.release();
        }
        if (mPartialWakeLock.isHeld()) {
            mPartialWakeLock.release();
        }
    }

    @Override
    public void onClose() {
        if (V) Log.v(TAG, "release WakeLock");
        releaseWakeLocks();
        /* Remote device abruptly terminated. Server session is down */
        mServerSessionIsUp = false;

        /* onClose could happen even before start() where mCallback is set */
        if (mCallback != null) {
            Message msg = Message.obtain(mCallback);
            msg.what = BluetoothOppObexSession.MSG_SESSION_COMPLETE;
            msg.obj = mInfo;
            msg.sendToTarget();
        }
    }

    public static boolean closeStream(final OutputStream out, final Operation op) {
        boolean returnvalue = true;
        if (D) Log.d(TAG, "closeoutStream +");
        try {
            if (out != null) {
                out.close();
            }
        } catch (IOException e) {
            Log.e(TAG, "outputStream close failed" + e.toString());
            returnvalue = false;
        }
        try {
            if (op != null) {
                op.close();
            }
        } catch (IOException e) {
            Log.e(TAG, "operation close failed" + e.toString());
            returnvalue = false;
        }

        if (D) Log.d(TAG, "closeoutStream -");
        return returnvalue;
    }
}
