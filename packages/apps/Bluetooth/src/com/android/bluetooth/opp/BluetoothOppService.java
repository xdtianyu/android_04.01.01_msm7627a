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

import com.google.android.collect.Lists;
import javax.btobex.ObexTransport;

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothClass;
import com.android.bluetooth.bpp.BluetoothBppTransfer;

import android.content.BroadcastReceiver;
import android.content.ContentUris;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.CharArrayBuffer;
import android.database.ContentObserver;
import android.database.Cursor;
import android.media.MediaScannerConnection;
import android.media.MediaScannerConnection.MediaScannerConnectionClient;
import android.net.Uri;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.PowerManager;
import android.util.Log;
import android.os.Process;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;

/**
 * Performs the background Bluetooth OPP transfer. It also starts thread to
 * accept incoming OPP connection.
 */

public class BluetoothOppService extends Service {
    private static final boolean D = Constants.DEBUG;
    private static final boolean V = Constants.VERBOSE;

    private boolean userAccepted = false;

    private class BluetoothShareContentObserver extends ContentObserver {

        public BluetoothShareContentObserver() {
            super(new Handler());
        }

        @Override
        public void onChange(boolean selfChange) {
            if (V) Log.v(TAG, "ContentObserver received notification");
            updateFromProvider();
        }
    }

    private static final String TAG = "BtOpp Service";

    /** Observer to get notified when the content observer's data changes */
    private BluetoothShareContentObserver mObserver;

    /** Class to handle Notification Manager updates */
    private BluetoothOppNotification mNotifier;

    private boolean mPendingUpdate;

    private UpdateThread mUpdateThread;

    private ArrayList<BluetoothOppShareInfo> mShares;

    private ArrayList<BluetoothOppBatch> mBatchs;

    public static BluetoothOppTransfer mTransfer;

    private BluetoothOppTransfer mServerTransfer;

    public static ArrayList<BluetoothBppTransfer> mBppTransfer;

    public static int mBppTransId;

    public static boolean mbStopSelf;

    private int mCurrArrayPos;

    private int mBatchId;

    /**
     * Array used when extracting strings from content provider
     */
    private CharArrayBuffer mOldChars;

    /**
     * Array used when extracting strings from content provider
     */
    private CharArrayBuffer mNewChars;

    private BluetoothAdapter mAdapter;

    private PowerManager mPowerManager;

    private BluetoothOppL2capListener mL2capSocketListener;

    private BluetoothOppRfcommListener mRfcommSocketListener;

    private boolean mListenStarted = false;

    private boolean mMediaScanInProgress;

    private int mIncomingRetries = 0;

    private ObexTransport mPendingConnection = null;

    /*
     * TODO No support for queue incoming from multiple devices.
     * Make an array list of server session to support receiving queue from
     * multiple devices
     */
    private BluetoothOppObexServerSession mServerSession;

    @Override
    public IBinder onBind(Intent arg0) {
        throw new UnsupportedOperationException("Cannot bind to Bluetooth OPP Service");
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (V) Log.v(TAG, "Service onCreate");
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        mL2capSocketListener = new BluetoothOppL2capListener(mAdapter);
        mRfcommSocketListener = new BluetoothOppRfcommListener(mAdapter);

        mShares = Lists.newArrayList();
        mBatchs = Lists.newArrayList();
        mBppTransfer = Lists.newArrayList();

        mObserver = new BluetoothShareContentObserver();
        getContentResolver().registerContentObserver(BluetoothShare.CONTENT_URI, true, mObserver);
        mBatchId = 1;
        mBppTransId = 0;
        mNotifier = new BluetoothOppNotification(this);
        mNotifier.mNotificationMgr.cancelAll();
        mNotifier.updateNotification();
        mbStopSelf = false;

        final ContentResolver contentResolver = getContentResolver();
        new Thread("trimDatabase") {
            public void run() {
                trimDatabase(contentResolver);
            }
        }.start();

        IntentFilter filter = new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
        registerReceiver(mBluetoothReceiver, filter);

        synchronized (BluetoothOppService.this) {
            if (mAdapter == null) {
                Log.w(TAG, "Local BT device is not enabled");
            } else {
                startListener();
            }
        }
        if (V) BluetoothOppPreference.getInstance(this).dump();
        updateFromProvider();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (V) Log.v(TAG, "Service onStartCommand");
        int retCode = super.onStartCommand(intent, flags, startId);
        if (retCode == START_STICKY) {
            if (mAdapter == null) {
                Log.w(TAG, "Local BT device is not enabled");
            } else {
                startListener();
            }
            updateFromProvider();
        }
        return retCode;
    }

    private void startListener() {
        if (!mListenStarted) {
            if (mAdapter.isEnabled()) {
                if (V) Log.v(TAG, "Starting RfcommListener");
                mHandler.sendMessage(mHandler.obtainMessage(START_LISTENER));
                mListenStarted = true;
            }
        }
    }

    private static final int START_LISTENER = 1;

    private static final int MEDIA_SCANNED = 2;

    private static final int MEDIA_SCANNED_FAILED = 3;

    private static final int MSG_INCOMING_CONNECTION_RETRY = 4;

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case START_LISTENER:
                    if (mAdapter.isEnabled()) {
                        startSocketListener();
                    }
                    break;
                case MEDIA_SCANNED:
                    if (V) Log.v(TAG, "Update mInfo.id " + msg.arg1 + " for data uri= "
                                + msg.obj.toString());
                    ContentValues updateValues = new ContentValues();
                    Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + msg.arg1);
                    updateValues.put(Constants.MEDIA_SCANNED, Constants.MEDIA_SCANNED_SCANNED_OK);
                    updateValues.put(BluetoothShare.URI, msg.obj.toString()); // update
                    updateValues.put(BluetoothShare.MIMETYPE, getContentResolver().getType(
                            Uri.parse(msg.obj.toString())));
                    getContentResolver().update(contentUri, updateValues, null, null);
                    synchronized (BluetoothOppService.this) {
                        mMediaScanInProgress = false;
                    }
                    break;
                case MEDIA_SCANNED_FAILED:
                    Log.v(TAG, "Update mInfo.id " + msg.arg1 + " for MEDIA_SCANNED_FAILED");
                    ContentValues updateValues1 = new ContentValues();
                    Uri contentUri1 = Uri.parse(BluetoothShare.CONTENT_URI + "/" + msg.arg1);
                    updateValues1.put(Constants.MEDIA_SCANNED,
                            Constants.MEDIA_SCANNED_SCANNED_FAILED);
                    getContentResolver().update(contentUri1, updateValues1, null, null);
                    synchronized (BluetoothOppService.this) {
                        mMediaScanInProgress = false;
                    }
                    break;
                case BluetoothOppRfcommListener.MSG_INCOMING_BTOPP_CONNECTION:
                    if (D) Log.d(TAG, "Get incoming connection");
                    ObexTransport transport = (ObexTransport)msg.obj;
                    /*
                     * Strategy for incoming connections:
                     * 1. If there is no ongoing transfer, no on-hold connection, start it
                     * 2. If there is ongoing transfer, hold it for 20 seconds(1 seconds * 20 times)
                     * 3. If there is on-hold connection, reject directly
                     * 4. If there is BPP transfer and no OPP transfer, then it will start.
                     */
                    if (D) Log.d(TAG, "mBatchs.size(): " + mBatchs.size()
                        + "\r\nmTransfer : " + mTransfer
                        + "\r\nmPendingConnection : " + mPendingConnection
                        + "\r\nmServerSessionIsUp : " + BluetoothOppObexServerSession.mServerSessionIsUp);
                    if ((mBatchs.size() >= 0) &&
                         !BluetoothOppObexServerSession.mServerSessionIsUp) {
                        Log.i(TAG, "### Start Obex Server");
                        createServerSession(transport);
                    } else {
                        if (mPendingConnection != null) {
                            Log.w(TAG, "OPP busy! Reject connection");
                            try {
                                transport.close();
                            } catch (IOException e) {
                                Log.e(TAG, "close tranport error");
                            }
                        } else if (Constants.USE_TCP_DEBUG && !Constants.USE_TCP_SIMPLE_SERVER) {
                            Log.i(TAG, "Start Obex Server in TCP DEBUG mode");
                            createServerSession(transport);
                        } else {
                            Log.i(TAG, "OPP busy! Retry after 1 second");
                            mIncomingRetries = mIncomingRetries + 1;
                            mPendingConnection = transport;
                            Message msg1 = Message.obtain(mHandler);
                            msg1.what = MSG_INCOMING_CONNECTION_RETRY;
                            mHandler.sendMessageDelayed(msg1, 1000);
                        }
                    }
                    break;
                case MSG_INCOMING_CONNECTION_RETRY:
                    if (D) Log.d(TAG, "#2 mBatchs.size(): " + mBatchs.size()
                        + "\r\nmTransfer : " + mTransfer
                        + "\r\nmServerSessionIsUp : " + BluetoothOppObexServerSession.mServerSessionIsUp);
                    if ((mBatchs.size() >= 0) &&
                         !BluetoothOppObexServerSession.mServerSessionIsUp) {
                        Log.i(TAG, "Start Obex Server");
                        createServerSession(mPendingConnection);
                        mIncomingRetries = 0;
                        mPendingConnection = null;
                    } else {
                        if (mIncomingRetries == 20) {
                            Log.w(TAG, "Retried 20 seconds, reject connection");
                            try {
                                mPendingConnection.close();
                            } catch (IOException e) {
                                Log.e(TAG, "close tranport error");
                            }
                            mIncomingRetries = 0;
                            mPendingConnection = null;
                        } else {
                            Log.i(TAG, "OPP busy! Retry after 1 second");
                            mIncomingRetries = mIncomingRetries + 1;
                            Message msg2 = Message.obtain(mHandler);
                            msg2.what = MSG_INCOMING_CONNECTION_RETRY;
                            mHandler.sendMessageDelayed(msg2, 1000);
                        }
                    }
                    break;
            }
        }
    };

    private void startSocketListener() {

        if (V) Log.v(TAG, "start RFCOMM and L2CAP listeners");
        mRfcommSocketListener.start(mHandler);
        mL2capSocketListener.start(mHandler);
        if (V) Log.d(TAG, "RFCOMM and L2CAP listeners started");
    }

    @Override
    public void onDestroy() {
        if (V) Log.v(TAG, "Service onDestroy");
        super.onDestroy();
        getContentResolver().unregisterContentObserver(mObserver);
        unregisterReceiver(mBluetoothReceiver);
        mRfcommSocketListener.stop();
        mL2capSocketListener.stop();
    }

    /* suppose we auto accept an incoming OPUSH connection */
    private void createServerSession(ObexTransport transport) {
        mServerSession = new BluetoothOppObexServerSession(this, transport);
        mServerSession.preStart();
        if (D) Log.d(TAG, "Get ServerSession " + mServerSession.toString()
                    + " for incoming connection" + transport.toString());
    }

    private final BroadcastReceiver mBluetoothReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();

            if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
                switch (intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)) {
                    case BluetoothAdapter.STATE_ON:
                        if (V) Log.v(TAG,
                                    "Receiver BLUETOOTH_STATE_CHANGED_ACTION, BLUETOOTH_STATE_ON");
                        startSocketListener();
                        break;
                    case BluetoothAdapter.STATE_TURNING_OFF:
                        if (V) Log.v(TAG, "Receiver DISABLED_ACTION ");
                        mRfcommSocketListener.stop();
                        mL2capSocketListener.stop();
                        mListenStarted = false;
                        synchronized (BluetoothOppService.this) {
                           mbStopSelf = true;
                           if (mUpdateThread == null) {
                               if(V) Log.v(TAG, "Thread is not running, OPP size:"+mBatchs.size()+"Bpp size"+mBppTransfer.size());
                               if ((mBatchs.size() == 0) && (mBppTransfer.size() == 0)) {
                                    /* Batch is empty and BT is turning off, stop service  */
                                    stopSelf();
                                    mbStopSelf = false;
                               }
                            }
                        }
                        break;
                }
            }
        }
    };

    private void updateFromProvider() {
        synchronized (BluetoothOppService.this) {
            mPendingUpdate = true;
            if (mUpdateThread == null) {
                if (V) Log.v(TAG, "Starting a new thread");
                mUpdateThread = new UpdateThread();
                mUpdateThread.start();
            }
        }
    }

    private class UpdateThread extends Thread {
        public UpdateThread() {
            super("Bluetooth Share Service");
        }

        @Override
        public void run() {
            Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);

            boolean keepService = false;
            for (;;) {
                synchronized (BluetoothOppService.this) {
                    if (mUpdateThread != this) {
                        throw new IllegalStateException(
                                "multiple UpdateThreads in BluetoothOppService");
                    }
                    if (V) Log.v(TAG, "pendingUpdate is " + mPendingUpdate + " keepUpdateThread is "
                                + keepService + " sListenStarted is " + mListenStarted);
                    if (!mPendingUpdate) {
                        mUpdateThread = null;
                        if (!keepService && !mListenStarted) {
                            if (V) Log.v(TAG, "Need to stop self");
                            stopSelf();
                            mbStopSelf = false;
                            break;
                        }
                        if (V) Log.v(TAG, "***returning from updatethread***");
                        return;
                    }
                    mPendingUpdate = false;
                }
                Cursor cursor = getContentResolver().query(BluetoothShare.CONTENT_URI, null, null,
                        null, BluetoothShare._ID);

                if (cursor == null) {
                    return;
                }

                cursor.moveToFirst();

                int arrayPos = 0;

                keepService = false;
                boolean isAfterLast = cursor.isAfterLast();

                int idColumn = cursor.getColumnIndexOrThrow(BluetoothShare._ID);
                /*
                 * Walk the cursor and the local array to keep them in sync. The
                 * key to the algorithm is that the ids are unique and sorted
                 * both in the cursor and in the array, so that they can be
                 * processed in order in both sources at the same time: at each
                 * step, both sources point to the lowest id that hasn't been
                 * processed from that source, and the algorithm processes the
                 * lowest id from those two possibilities. At each step: -If the
                 * array contains an entry that's not in the cursor, remove the
                 * entry, move to next entry in the array. -If the array
                 * contains an entry that's in the cursor, nothing to do, move
                 * to next cursor row and next array entry. -If the cursor
                 * contains an entry that's not in the array, insert a new entry
                 * in the array, move to next cursor row and next array entry.
                 */
                while (!isAfterLast || arrayPos < mShares.size()) {
                    if (isAfterLast) {
                        // We're beyond the end of the cursor but there's still
                        // some
                        // stuff in the local array, which can only be junk
                        if (V) Log.v(TAG, "Array update: trimming " +
                                mShares.get(arrayPos).mId + " @ " + arrayPos);

                        if (shouldScanFile(arrayPos)) {
                            scanFile(null, arrayPos);
                        }
                        deleteShare(arrayPos); // this advances in the array
                    } else {
                        int id = cursor.getInt(idColumn);

                        if (arrayPos == mShares.size()) {
                            if (V) Log.v(TAG, "Array update: inserting " + id + " @ " + arrayPos);
                            insertShare(cursor, arrayPos);
                            if (shouldScanFile(arrayPos) && (!scanFile(cursor, arrayPos))) {
                                keepService = true;
                            }
                            if (visibleNotification(arrayPos)) {
                                keepService = true;
                            }
                            if (needAction(arrayPos)) {
                                keepService = true;
                            }

                            ++arrayPos;
                            cursor.moveToNext();
                            isAfterLast = cursor.isAfterLast();
                        } else {
                            int arrayId = mShares.get(arrayPos).mId;

                            if (arrayId < id) {
                                if (V) Log.v(TAG, "Array update: removing " + arrayId + " @ "
                                            + arrayPos);
                                if (shouldScanFile(arrayPos)) {
                                    scanFile(null, arrayPos);
                                }
                                deleteShare(arrayPos);
                            } else if (arrayId == id) {
                                // This cursor row already exists in the stored
                                // array
                                if(V) Log.v(TAG," Calling Updateshare arraypos " + arrayPos);
                                updateShare(cursor, arrayPos, userAccepted);
                                if (shouldScanFile(arrayPos) && (!scanFile(cursor, arrayPos))) {
                                    keepService = true;
                                }
                                if (visibleNotification(arrayPos)) {
                                    keepService = true;
                                }
                                if (needAction(arrayPos)) {
                                    keepService = true;
                                }

                                ++arrayPos;
                                cursor.moveToNext();
                                isAfterLast = cursor.isAfterLast();
                            } else {
                                // This cursor entry didn't exist in the stored
                                // array
                                if (V) Log.v(TAG, "Array update: appending " + id + " @ " + arrayPos);
                                insertShare(cursor, arrayPos);

                                if (shouldScanFile(arrayPos) && (!scanFile(cursor, arrayPos))) {
                                    keepService = true;
                                }
                                if (visibleNotification(arrayPos)) {
                                    keepService = true;
                                }
                                if (needAction(arrayPos)) {
                                    keepService = true;
                                }
                                ++arrayPos;
                                cursor.moveToNext();
                                isAfterLast = cursor.isAfterLast();
                            }
                        }
                    }
                }

                mNotifier.updateNotification();

                cursor.close();
                if((mBatchs.size()== 0) && (mbStopSelf) && (isAfterLast) && (mBppTransfer.size()== 0)) {
                    if(V) Log.v(TAG," Nothing to Transfer,Service No Longer Required");
                    keepService = false;
                }
            }
        }

    }
    boolean isBppAvailable(BluetoothClass c) {
        if(c.hasService(BluetoothClass.Service.OBJECT_TRANSFER)
                     && c.hasService(BluetoothClass.Service.RENDER)
                     && (c.getDeviceClass () == BluetoothClass.Device.IMAGING_PRINTER))
            return true;
        else
            return false;
        }
    private void insertShare(Cursor cursor, int arrayPos) {
        BluetoothOppShareInfo info = new BluetoothOppShareInfo(
                cursor.getInt(cursor.getColumnIndexOrThrow(BluetoothShare._ID)),
                cursor.getString(cursor.getColumnIndexOrThrow(BluetoothShare.URI)),
                cursor.getString(cursor.getColumnIndexOrThrow(BluetoothShare.FILENAME_HINT)),
                cursor.getString(cursor.getColumnIndexOrThrow(BluetoothShare._DATA)),
                cursor.getString(cursor.getColumnIndexOrThrow(BluetoothShare.MIMETYPE)),
                cursor.getInt(cursor.getColumnIndexOrThrow(BluetoothShare.DIRECTION)),
                cursor.getInt(cursor.getColumnIndexOrThrow(BluetoothShare.OWNER)),
                cursor.getString(cursor.getColumnIndexOrThrow(BluetoothShare.DESTINATION)),
                cursor.getInt(cursor.getColumnIndexOrThrow(BluetoothShare.VISIBILITY)),
                cursor.getInt(cursor.getColumnIndexOrThrow(BluetoothShare.USER_CONFIRMATION)),
                cursor.getInt(cursor.getColumnIndexOrThrow(BluetoothShare.STATUS)),
                cursor.getLong(cursor.getColumnIndexOrThrow(BluetoothShare.TOTAL_BYTES)),
                cursor.getLong(cursor.getColumnIndexOrThrow(BluetoothShare.CURRENT_BYTES)),
                cursor.getLong(cursor.getColumnIndexOrThrow(BluetoothShare.TIMESTAMP)),
                cursor.getInt(cursor.getColumnIndexOrThrow(Constants.MEDIA_SCANNED)) != Constants.MEDIA_SCANNED_NOT_SCANNED);

        if (V) {
            Log.v(TAG, "Service adding new entry");
            Log.v(TAG, "ID      : " + info.mId);
            // Log.v(TAG, "URI     : " + ((info.mUri != null) ? "yes" : "no"));
            Log.v(TAG, "URI     : " + info.mUri);
            Log.v(TAG, "HINT    : " + info.mHint);
            Log.v(TAG, "FILENAME: " + info.mFilename);
            Log.v(TAG, "MIMETYPE: " + info.mMimetype);
            Log.v(TAG, "DIRECTION: " + info.mDirection);
            Log.v(TAG, "OWNER   : " + info.mOwner);
            Log.v(TAG, "DESTINAT: " + info.mDestination);
            Log.v(TAG, "VISIBILI: " + info.mVisibility);
            Log.v(TAG, "CONFIRM : " + info.mConfirm);
            Log.v(TAG, "STATUS  : " + info.mStatus);
            Log.v(TAG, "TOTAL   : " + info.mTotalBytes);
            Log.v(TAG, "CURRENT : " + info.mCurrentBytes);
            Log.v(TAG, "TIMESTAMP : " + info.mTimestamp);
            Log.v(TAG, "SCANNED : " + info.mMediaScanned);
        }

        mShares.add(arrayPos, info);
        /* Mark the info as failed if it's in invalid status */
        if (info.isObsolete()) {
            Constants.updateShareStatus(this, info.mId, BluetoothShare.STATUS_UNKNOWN_ERROR);
        }
        /*
         * Add info into a batch. The logic is
         * 1) Only add valid and readyToStart info
         * 2) If there is no batch, create a batch and insert this transfer into batch,
         * then run the batch
         * 3) If there is existing batch and timestamp match, insert transfer into batch
         * 4) If there is existing batch and timestamp does not match, create a new batch and
         * put in queue
         */

        if (info.isReadyToStart()) {
            if (info.mDirection == BluetoothShare.DIRECTION_OUTBOUND) {
                /* check if the file exists */
                if(!info.mUri.contains("as_multi_vcard")) {
                    InputStream i;
                    try {
                        if (V) Log.v(TAG, "Check The presence of file");
                        i = getContentResolver().openInputStream(Uri.parse(info.mUri));
                    } catch (FileNotFoundException e) {
                        Log.e(TAG, "Can't open file for OUTBOUND info " + info.mId);
                        Constants.updateShareStatus(this, info.mId, BluetoothShare.STATUS_BAD_REQUEST);
                        return;
                    } catch (SecurityException e) {
                        Log.e(TAG, "Exception:" + e.toString() + " for OUTBOUND info " + info.mId);
                        Constants.updateShareStatus(this, info.mId, BluetoothShare.STATUS_BAD_REQUEST);
                        return;
                    }

                    try {
                        i.close();
                    } catch (IOException ex) {
                        Log.e(TAG, "IO error when close file for OUTBOUND info " + info.mId);
                        return;
                    }
                }
            }

            BluetoothAdapter a = BluetoothAdapter.getDefaultAdapter();
            BluetoothDevice  d = a.getRemoteDevice(info.mDestination);
            BluetoothClass   c = d.getBluetoothClass();
            if((c != null) && (info.mDirection == BluetoothShare.DIRECTION_OUTBOUND)) {
                if (V) Log.v(TAG, "BT Device Class: 0x" + Integer.toHexString(c.getDeviceClass()));

                if (isBppAvailable (c)) {
                    /* BPP Profile*/
                    markBatchOwnership(this, info.mId, BluetoothShare.OWNER_BPP);
                    info.mOwner = BluetoothShare.OWNER_BPP;
                } else {
                    /* OPP Profile*/
                    markBatchOwnership(this, info.mId, BluetoothShare.OWNER_OPP);
                    info.mOwner = BluetoothShare.OWNER_OPP;
                }
           } else {
                if(V) Log.v(TAG," deviceClass is null, going ahead with OPP");
                markBatchOwnership(this, info.mId, BluetoothShare.OWNER_OPP);
                info.mOwner = BluetoothShare.OWNER_OPP;
           }

            Log.v(TAG, "New OWNER   : " + info.mOwner);

            if (mBatchs.size() == 0) {
                BluetoothOppBatch newBatch = new BluetoothOppBatch(this, info);
                newBatch.mId = mBatchId;
                mBatchId++;
                mBatchs.add(newBatch);

                if (info.mDirection == BluetoothShare.DIRECTION_OUTBOUND) {
                    if (V) Log.v(TAG, "Service start transfer new Batch " + newBatch.mId
                                + " for info " + info.mId);
                    if ((c != null) && (isBppAvailable (c))) {
                        BluetoothBppTransfer BppTransfer =
                            new BluetoothBppTransfer(this, mPowerManager, newBatch);
                        if (BppTransfer != null) {
                            mBppTransfer.add(BppTransfer);
                            mBppTransId++;
                            BppTransfer.start();
                        } else {
                            Log.e(TAG, "Unexpected error! BppTransfer is null");
                            mShares.remove(arrayPos);
                        }

                        if (V) Log.v(TAG, "New BT BPP Transfer(" + mBppTransId
                            + "/" + mBppTransfer.size() + ") Start !!");
                    } else {
                        if (V) Log.v(TAG, "BT OPP Transfer Start");
                        mTransfer = new BluetoothOppTransfer(this, mPowerManager, newBatch);
                        if (mTransfer != null) {
                            mTransfer.start();
                        } else {
                            Log.e(TAG, "Unexpected error! mTransfer is null");
                            mShares.remove(arrayPos);
                            mBatchId--;
                            mShares.remove(arrayPos);
                        }
                    }
                } else if (info.mDirection == BluetoothShare.DIRECTION_INBOUND) {
                    if (V) Log.v(TAG, "Service start server transfer new Batch " + newBatch.mId
                                + " for info " + info.mId);
                    mServerTransfer = new BluetoothOppTransfer(this, mPowerManager, newBatch,
                            mServerSession);
                    if (mServerTransfer != null) {
                        mServerTransfer.start();
                    } else {
                        Log.e(TAG, "Unexpected error! mServerTransfer is null");
                        mShares.remove(arrayPos);
                        mBatchId--;
                        mShares.remove(arrayPos);
                    }
                }

            } else {
                int i = findBatchWithTimeStamp(info.mTimestamp);
                if ((i != -1)&&(info.mOwner == BluetoothShare.OWNER_OPP)) {
                    if (V) Log.v(TAG, "Service add info " + info.mId + " to existing batch "
                                + mBatchs.get(i).mId);
                    if (V) Log.v(TAG," Batch Status   " + info.mStatus);
                    mBatchs.get(i).addShare(info);
                } else {
                    /* Changing the Timestamp of simultaneously queued BPP Share */
                    if(info.mOwner == BluetoothShare.OWNER_BPP){
                        for ( int k=0;k < mBatchs.size(); k++){
                            if(info.mOwner == BluetoothShare.OWNER_BPP){
                                if(V) Log.v(TAG," Changing TimeStamp BPP");
                                if(mBatchs.get(k).mTimestamp == info.mTimestamp)
                                    info.mTimestamp++;
                            }
                        }
                        mShares.get(arrayPos).mTimestamp = info.mTimestamp;
                        ContentValues values = new ContentValues();
                        Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + info.mId);
                        values.put(BluetoothShare.TIMESTAMP,info.mTimestamp);
                        this.getContentResolver().update(contentUri, values, null, null);
                    }
                    // There is ongoing batch
                    BluetoothOppBatch newBatch = new BluetoothOppBatch(this, info);
                    newBatch.mId = mBatchId;
                    mBatchId++;
                    if (V) Log.v(TAG, "mBatchs.add(newBatch) start!!");
                    mBatchs.add(newBatch);
                    if (V) Log.v(TAG, "Service add new Batch " + newBatch.mId + " for info " +
                            info.mId);
                    if (info.mDirection == BluetoothShare.DIRECTION_OUTBOUND) {
                        if ((c != null) && (isBppAvailable (c))) {
                            BluetoothBppTransfer BppTransfer =
                            new BluetoothBppTransfer(this, mPowerManager, newBatch);
                            if(BppTransfer != null) {
                                if(mBppTransfer.size()==0) {
                                    mBppTransfer.add(BppTransfer);
                                    mBppTransId++;
                                    BppTransfer.start();
                                } else {
                                    mBppTransfer.add(BppTransfer);
                                    mBppTransId++;
                                }
                            } else {
                                Log.e(TAG, "Unexpected error! BppTransfer is null");
                                mBatchs.remove(newBatch);
                                mBatchId--;
                                mShares.remove(arrayPos);
                            }
                            if (V) Log.v(TAG, "Additional BT BPP Transfer(" + mBppTransId
                                    + "/" + mBppTransfer.size() + ") Start !!");
                        } else {
                            if(mTransfer == null) {
                                if (V) Log.v(TAG, "BT OPP Transfer Start");
                                mTransfer = new BluetoothOppTransfer(this, mPowerManager, newBatch);
                                if (mTransfer != null) {
                                    mTransfer.start();
                                } else {
                                    Log.e(TAG, "Unexpected error! mTransfer is null");
                                    mBatchs.remove(newBatch);
                                    mBatchId--;
                                    mShares.remove(arrayPos);
                                }
                            }
                        }
                    } else if (info.mDirection == BluetoothShare.DIRECTION_INBOUND) {
                        if(mServerTransfer == null) {
                            if (V) Log.v(TAG, "Service start server transfer new Batch "
                                + newBatch.mId + " for info " + info.mId);
                            mServerTransfer = new BluetoothOppTransfer(this, mPowerManager,
                                newBatch, mServerSession);
                            if (mServerTransfer != null) {
                                mServerTransfer.start();
                            } else {
                                Log.e(TAG, "Unexpected error! mServerTransfer is null");
                                mBatchs.remove(newBatch);
                                mBatchId--;
                                mShares.remove(arrayPos);
                            }
                        }
                    }

                    if (Constants.USE_TCP_DEBUG && !Constants.USE_TCP_SIMPLE_SERVER) {
                        // only allow  concurrent serverTransfer in debug mode
                        if (info.mDirection == BluetoothShare.DIRECTION_INBOUND) {
                            if (V) Log.v(TAG, "TCP_DEBUG start server transfer new Batch " +
                                    newBatch.mId + " for info " + info.mId);
                            mServerTransfer = new BluetoothOppTransfer(this, mPowerManager,
                                    newBatch, mServerSession);
                            if (mServerTransfer != null) {
                                mServerTransfer.start();
                            } else {
                                Log.e(TAG, "Unexpected error! mServerTransfer is null");
                            }
                        }
                    }
                }
            }
        }
    }

    private void updateShare(Cursor cursor, int arrayPos, boolean userAccepted) {
        BluetoothOppShareInfo info = mShares.get(arrayPos);
        int statusColumn = cursor.getColumnIndexOrThrow(BluetoothShare.STATUS);

        info.mId = cursor.getInt(cursor.getColumnIndexOrThrow(BluetoothShare._ID));
        info.mUri = stringFromCursor(info.mUri, cursor, BluetoothShare.URI);
        info.mHint = stringFromCursor(info.mHint, cursor, BluetoothShare.FILENAME_HINT);
        info.mFilename = stringFromCursor(info.mFilename, cursor, BluetoothShare._DATA);
        info.mMimetype = stringFromCursor(info.mMimetype, cursor, BluetoothShare.MIMETYPE);
        info.mDirection = cursor.getInt(cursor.getColumnIndexOrThrow(BluetoothShare.DIRECTION));
        info.mOwner = cursor.getInt(cursor.getColumnIndexOrThrow(BluetoothShare.OWNER));
        info.mDestination = stringFromCursor(info.mDestination, cursor, BluetoothShare.DESTINATION);
        int newVisibility = cursor.getInt(cursor.getColumnIndexOrThrow(BluetoothShare.VISIBILITY));
        if (V) Log.v(TAG,"UpdateShare: arraypos = " + arrayPos + "Id of this share = " + info.mId);
        boolean confirmed = false;
        int newConfirm = cursor.getInt(cursor
                .getColumnIndexOrThrow(BluetoothShare.USER_CONFIRMATION));

        if (info.mVisibility == BluetoothShare.VISIBILITY_VISIBLE
                && newVisibility != BluetoothShare.VISIBILITY_VISIBLE
                && (BluetoothShare.isStatusCompleted(info.mStatus) || newConfirm == BluetoothShare.USER_CONFIRMATION_PENDING)) {
            mNotifier.mNotificationMgr.cancel(info.mId);
        }

        info.mVisibility = newVisibility;

        if (info.mConfirm == BluetoothShare.USER_CONFIRMATION_PENDING
                && newConfirm != BluetoothShare.USER_CONFIRMATION_PENDING) {
            confirmed = true;
        }
        info.mConfirm = newConfirm;
        int newStatus = cursor.getInt(statusColumn);
        int oldStatus = info.mStatus;
        if (!BluetoothShare.isStatusCompleted(info.mStatus)
                && BluetoothShare.isStatusCompleted(newStatus)) {
            mNotifier.mNotificationMgr.cancel(info.mId);
        }

        if (V) Log.v(TAG," UpdateShare: oldStatus = " + oldStatus + " newStatus = " + newStatus);
        if (V) Log.v(TAG," owner = " + info.mOwner);
        info.mStatus = newStatus;
        if ((!BluetoothShare.isStatusCompleted(oldStatus))
                && (BluetoothShare.isStatusCompleted(newStatus))
                && (info.mOwner == BluetoothShare.OWNER_OPP)) {
            if (V) Log.v(TAG," UpdateShare: Share Completed: oldStatus = " + oldStatus + " newStatus = " + newStatus);
            try {
                if(info.mDirection == BluetoothShare.DIRECTION_OUTBOUND)
                    mTransfer.markShareComplete(newStatus);
                else
                    mServerTransfer.markShareComplete(newStatus);
            } catch (Exception e) {
                Log.e(TAG, "Exception: updateShare: oldStatus: " + oldStatus + " newStatus: " + newStatus);
            }
        }
        info.mTotalBytes = cursor.getLong(cursor.getColumnIndexOrThrow(BluetoothShare.TOTAL_BYTES));
        info.mCurrentBytes = cursor.getLong(cursor
                .getColumnIndexOrThrow(BluetoothShare.CURRENT_BYTES));
        info.mTimestamp = cursor.getLong(cursor.getColumnIndexOrThrow(BluetoothShare.TIMESTAMP));
        info.mMediaScanned = (cursor.getInt(cursor.getColumnIndexOrThrow(Constants.MEDIA_SCANNED)) != Constants.MEDIA_SCANNED_NOT_SCANNED);

        if (confirmed) {
            if (V) Log.v(TAG, "Service handle info " + info.mId + " confirmed");
            /* Inbounds transfer get user confirmation, so we start it */
            int i = findBatchWithTimeStamp(info.mTimestamp);
            if (i != -1) {
                BluetoothOppBatch batch = mBatchs.get(i);
                if (mServerTransfer != null && batch.mId == mServerTransfer.getBatchId()) {
                    mServerTransfer.setConfirmed();
                } //TODO need to think about else
            }
        }
        int i = findBatchWithTimeStamp(info.mTimestamp);
        if (i != -1) {
            BluetoothOppBatch batch = mBatchs.get(i);
            if (batch.mStatus == Constants.BATCH_STATUS_FINISHED
                    || batch.mStatus == Constants.BATCH_STATUS_FAILED) {
                if (V) Log.v(TAG, "Batch " + batch.mId + " is finished");
                if (batch.mDirection == BluetoothShare.DIRECTION_OUTBOUND) {
                    if (info.mOwner == BluetoothShare.OWNER_OPP) {
                        if (mTransfer == null) {
                            Log.e(TAG, "Unexpected error! mTransfer is null");
                        } else if (batch.mId == mTransfer.getBatchId()) {
                            mTransfer.stop();
                        } else {
                            Log.e(TAG, "Unexpected error! batch id " + batch.mId
                                    + " doesn't match mTransfer id " + mTransfer.getBatchId());
                        }
                        mTransfer = null;
                    } else if (info.mOwner == BluetoothShare.OWNER_BPP) {
                                BluetoothBppTransfer BppTransfer = mBppTransfer.get(0);
                                // as every new BPP share is queued up, the
                                // current share is the topmost one. Moreover
                                // we need not check for the complete
                                // array.Just  stop the transfer here, batch
                                // removal and starting of new batch will be
                                // done from the removebatch
                                if (BppTransfer != null && batch.mId == BppTransfer.getBatchId()) {
                                    Log.d(TAG, "BPP Transfer + batch("
                                        + batch.mId + ") are removed!!");
                                        BppTransfer.stop();
                                    } else {
                                        Log.e(TAG, "Unexpected error! BppTransfer is null");
                                    }
                    }
                } else {
                    if (mServerTransfer == null) {
                        Log.e(TAG, "Unexpected error! mServerTransfer is null");
                    } else if (batch.mId == mServerTransfer.getBatchId()) {
                        if(V) Log.v(TAG," Stopping Inbound Transfer ");
                        mServerTransfer.stop();
                    } else {
                        Log.e(TAG, "Unexpected error! batch id " + batch.mId
                                + " doesn't match mServerTransfer id "
                                + mServerTransfer.getBatchId());
                    }
                    mServerTransfer = null;
                }
                removeBatch(batch);
            }
        }
    }

    /**
     * Removes the local copy of the info about a share.
     */
    private void deleteShare(int arrayPos) {
        BluetoothOppShareInfo info = mShares.get(arrayPos);

        /*
         * Delete arrayPos from a batch. The logic is
         * 1) Search existing batch for the info
         * 2) cancel the batch
         * 3) If the batch become empty delete the batch
         */
        int i = findBatchWithTimeStamp(info.mTimestamp);
        if (i != -1) {
            BluetoothOppBatch batch = mBatchs.get(i);
            if (batch.hasShare(info)) {
                if (V) Log.v(TAG, "Service cancel batch for share " + info.mId);
                batch.cancelBatch();
            }
            if (batch.isEmpty()) {
                if (V) Log.v(TAG, "Service remove batch  " + batch.mId);
                removeBatch(batch);
            }
        }
        mShares.remove(arrayPos);
    }

    private String stringFromCursor(String old, Cursor cursor, String column) {
        int index = cursor.getColumnIndexOrThrow(column);
        if (old == null) {
            return cursor.getString(index);
        }
        if (mNewChars == null) {
            mNewChars = new CharArrayBuffer(128);
        }
        cursor.copyStringToBuffer(index, mNewChars);
        int length = mNewChars.sizeCopied;
        if (length != old.length()) {
            return cursor.getString(index);
        }
        if (mOldChars == null || mOldChars.sizeCopied < length) {
            mOldChars = new CharArrayBuffer(length);
        }
        char[] oldArray = mOldChars.data;
        char[] newArray = mNewChars.data;
        old.getChars(0, length, oldArray, 0);
        for (int i = length - 1; i >= 0; --i) {
            if (oldArray[i] != newArray[i]) {
                return new String(newArray, 0, length);
            }
        }
        return old;
    }

    private int findBatchWithTimeStamp(long timestamp) {
        for (int i = mBatchs.size() - 1; i >= 0; i--) {
            if (mBatchs.get(i).mTimestamp == timestamp) {
                return i;
            }
        }
        return -1;
    }
   // Parallel BPP and OPP transfer is possible. In case
   // both OPP and BPP share are queued, we need to start
   // the next available transfer. We should not stop at
   // first instance of running share. At one instant
   // 2 outgoing shares are possible, but they have to
   // be from different Owner. 1 outgoing and 1 incoming
   // share is also possible .
    private void removeBatch(BluetoothOppBatch batch) {
        if (V) Log.v(TAG, "Remove batch " + batch.mId);
            if(batch.mOwner == BluetoothShare.OWNER_BPP){
                if(V) Log.v(TAG,"Removing BPP Share");
                mBppTransfer.remove(0);
                mBppTransId--;
            }
        mBatchs.remove(batch);
        mBatchId--;
        BluetoothOppBatch nextBatch;
        if (mBatchs.size() > 0) {
            int mRunningBatchDirection = -1;
            int mRunningBatchOwner = -1;
            for (int i = 0; i < mBatchs.size(); i++) {
                // we have a running batch
                nextBatch = mBatchs.get(i);
                if (nextBatch.mStatus == Constants.BATCH_STATUS_RUNNING) {
                        if(mRunningBatchDirection == -1){
                            mRunningBatchDirection = nextBatch.mDirection;
                            continue;
                        }
                        if(mRunningBatchOwner == -1){
                            mRunningBatchOwner = nextBatch.mOwner;
                            continue;
                        }
                        /* we have either both direction or both owners, return from here */
                        if((mRunningBatchDirection != -1)&&(mRunningBatchDirection != nextBatch.mDirection))
                            return;
                        if((mRunningBatchOwner != -1)&&(mRunningBatchOwner != nextBatch.mOwner))
                            return;
                } else {
                    // just finish a transfer, start pending outbound transfer
                    if (nextBatch.mDirection == BluetoothShare.DIRECTION_OUTBOUND) {
                         if ((nextBatch.mOwner == BluetoothShare.OWNER_BPP) &&
                             (mRunningBatchOwner != BluetoothShare.OWNER_BPP)){
                              if (V) Log.e(TAG, "Unexpeced Error!!, there is pending batch("
                                         + nextBatch.mId +") on mBppTransfer!!");
                               if(mBppTransfer.size() > 0) {
                                   mBppTransfer.get(0).start();
                                   return;
                               }
                         } else if ((nextBatch.mOwner == BluetoothShare.OWNER_OPP)&&
                                    !((mRunningBatchOwner == BluetoothShare.OWNER_OPP)&&
                                    (mRunningBatchDirection == BluetoothShare.DIRECTION_OUTBOUND))) {
                                      if (V) Log.v(TAG, "Start pending OPP batch(" + nextBatch.mId + ")");
                                      mTransfer = new BluetoothOppTransfer(this, mPowerManager, nextBatch);
                                      mTransfer.start();
                                      return;
                         }
                    } else if ((nextBatch.mDirection == BluetoothShare.DIRECTION_INBOUND
                                &&  mServerSession != null) &&
                               (mRunningBatchDirection != BluetoothShare.DIRECTION_INBOUND)){
                                // have to support pending inbound transfer
                                // if an outbound transfer and incoming socket happens together
                                if (V) Log.v(TAG, "Start pending inbound batch " + nextBatch.mId);
                                mServerTransfer = new BluetoothOppTransfer(this, mPowerManager, nextBatch,
                                                                   mServerSession);
                                mServerTransfer.start();
                                if (nextBatch.getPendingShare().mConfirm ==
                                    BluetoothShare.USER_CONFIRMATION_CONFIRMED) {
                                      mServerTransfer.setConfirmed();
                                }
                                return;
                    }
                }
            }
        }
    }

    public static void markBatchOwnership(Context context, int id, int owner) {
        if (V) Log.v(TAG, "Current ownership on info #" + id +" is " + owner );
        Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + id);
        ContentValues updateValues = new ContentValues();
        updateValues.put(BluetoothShare.OWNER, owner);
        context.getContentResolver().update(contentUri, updateValues, null, null);
    }

    private boolean needAction(int arrayPos) {
        BluetoothOppShareInfo info = mShares.get(arrayPos);
        if (BluetoothShare.isStatusCompleted(info.mStatus)) {
            return false;
        }
        return true;
    }

    private boolean visibleNotification(int arrayPos) {
        BluetoothOppShareInfo info = mShares.get(arrayPos);
        return info.hasCompletionNotification();
    }

    private boolean scanFile(Cursor cursor, int arrayPos) {
        BluetoothOppShareInfo info = mShares.get(arrayPos);
        synchronized (BluetoothOppService.this) {
            if (D) Log.d(TAG, "Scanning file " + info.mFilename);
            if (!mMediaScanInProgress) {
                mMediaScanInProgress = true;
                new MediaScannerNotifier(this, info, mHandler);
                return true;
            } else {
                return false;
            }
        }
    }

    private boolean shouldScanFile(int arrayPos) {
        BluetoothOppShareInfo info = mShares.get(arrayPos);
        return BluetoothShare.isStatusSuccess(info.mStatus)
                && info.mDirection == BluetoothShare.DIRECTION_INBOUND && !info.mMediaScanned &&
                info.mConfirm != BluetoothShare.USER_CONFIRMATION_HANDOVER_CONFIRMED;
    }

    // Run in a background thread at boot.
    private static void trimDatabase(ContentResolver contentResolver) {
        final String INVISIBLE = BluetoothShare.VISIBILITY + "=" +
                BluetoothShare.VISIBILITY_HIDDEN;

        // remove the invisible/complete/outbound shares
        final String WHERE_INVISIBLE_COMPLETE_OUTBOUND = BluetoothShare.DIRECTION + "="
                + BluetoothShare.DIRECTION_OUTBOUND + " AND " + BluetoothShare.STATUS + ">="
                + BluetoothShare.STATUS_QUEUE + " AND " + INVISIBLE;
        int delNum = contentResolver.delete(BluetoothShare.CONTENT_URI,
                WHERE_INVISIBLE_COMPLETE_OUTBOUND, null);
        if (V) Log.v(TAG, "Deleted complete outbound shares, number =  " + delNum);

        // remove the pending outbound shares
        final String WHERE_PENDING_OUTBOUND = BluetoothShare.DIRECTION + "="
                + BluetoothShare.DIRECTION_OUTBOUND + " AND " + BluetoothShare.STATUS + "<"
                + BluetoothShare.STATUS_SUCCESS;
        delNum = contentResolver.delete(BluetoothShare.CONTENT_URI,
                WHERE_PENDING_OUTBOUND, null);
        if (V) Log.v(TAG, "Deleted pending outbound shares, number =  " + delNum);


        // remove the invisible/finished/inbound/failed shares
        final String WHERE_INVISIBLE_COMPLETE_INBOUND_FAILED = BluetoothShare.DIRECTION + "="
                + BluetoothShare.DIRECTION_INBOUND + " AND " + BluetoothShare.STATUS + ">"
                + BluetoothShare.STATUS_SUCCESS + " AND " + INVISIBLE;
        delNum = contentResolver.delete(BluetoothShare.CONTENT_URI,
                WHERE_INVISIBLE_COMPLETE_INBOUND_FAILED, null);
        if (V) Log.v(TAG, "Deleted complete inbound failed shares, number = " + delNum);

        // on boot : remove unconfirmed inbound shares.
        final String WHERE_CONFIRMATION_PENDING_INBOUND = BluetoothShare.DIRECTION + "="
                + BluetoothShare.DIRECTION_INBOUND + " AND " + BluetoothShare.USER_CONFIRMATION
                + "=" + BluetoothShare.USER_CONFIRMATION_PENDING;
        delNum = contentResolver.delete(BluetoothShare.CONTENT_URI,
                 WHERE_CONFIRMATION_PENDING_INBOUND, null);
        if (V) Log.v(TAG, "Deleted unconfirmed incoming shares, number = " + delNum);

        // Only keep the inbound and successful shares for LiverFolder use
        // Keep the latest 1000 to easy db query
        final String WHERE_INBOUND_SUCCESS = BluetoothShare.DIRECTION + "="
                + BluetoothShare.DIRECTION_INBOUND + " AND " + BluetoothShare.STATUS + "="
                + BluetoothShare.STATUS_SUCCESS + " AND " + INVISIBLE;
        Cursor cursor = contentResolver.query(BluetoothShare.CONTENT_URI, new String[] {
            BluetoothShare._ID
        }, WHERE_INBOUND_SUCCESS, null, BluetoothShare._ID); // sort by id

        if (cursor == null) {
            return;
        }

        int recordNum = cursor.getCount();
        if (recordNum > Constants.MAX_RECORDS_IN_DATABASE) {
            int numToDelete = recordNum - Constants.MAX_RECORDS_IN_DATABASE;

            if (cursor.moveToPosition(numToDelete)) {
                int columnId = cursor.getColumnIndexOrThrow(BluetoothShare._ID);
                long id = cursor.getLong(columnId);
                delNum = contentResolver.delete(BluetoothShare.CONTENT_URI,
                        BluetoothShare._ID + " < " + id, null);
                if (V) Log.v(TAG, "Deleted old inbound success share: " + delNum);
            }
        }
        cursor.close();
    }

    private static class MediaScannerNotifier implements MediaScannerConnectionClient {

        private MediaScannerConnection mConnection;

        private BluetoothOppShareInfo mInfo;

        private Context mContext;

        private Handler mCallback;

        public MediaScannerNotifier(Context context, BluetoothOppShareInfo info, Handler handler) {
            mContext = context;
            mInfo = info;
            mCallback = handler;
            mConnection = new MediaScannerConnection(mContext, this);
            if (V) Log.v(TAG, "Connecting to MediaScannerConnection ");
            mConnection.connect();
        }

        public void onMediaScannerConnected() {
            if (V) Log.v(TAG, "MediaScannerConnection onMediaScannerConnected");
            mConnection.scanFile(mInfo.mFilename, mInfo.mMimetype);
        }

        public void onScanCompleted(String path, Uri uri) {
            try {
                if (V) {
                    Log.v(TAG, "MediaScannerConnection onScanCompleted");
                    Log.v(TAG, "MediaScannerConnection path is " + path);
                    Log.v(TAG, "MediaScannerConnection Uri is " + uri);
                }
                if (uri != null) {
                    Message msg = Message.obtain();
                    msg.setTarget(mCallback);
                    msg.what = MEDIA_SCANNED;
                    msg.arg1 = mInfo.mId;
                    msg.obj = uri;
                    msg.sendToTarget();
                } else {
                    Message msg = Message.obtain();
                    msg.setTarget(mCallback);
                    msg.what = MEDIA_SCANNED_FAILED;
                    msg.arg1 = mInfo.mId;
                    msg.sendToTarget();
                }
            } catch (Exception ex) {
                Log.v(TAG, "!!!MediaScannerConnection exception: " + ex);
            } finally {
                if (V) Log.v(TAG, "MediaScannerConnection disconnect");
                mConnection.disconnect();
            }
        }
    }
}
