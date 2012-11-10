/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *        * Redistributions of source code must retain the above copyright
 *           notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above
 *           copyright notice, this list of conditions and the following
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

import com.android.bluetooth.R;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.Socket;
import java.util.Set;

import javax.btobex.ClientOperation;
import javax.btobex.ClientSession;
import javax.btobex.HeaderSet;
import javax.btobex.ObexTransport;
import javax.btobex.ResponseCodes;

import com.android.bluetooth.opp.BluetoothOppService;
import com.android.bluetooth.opp.BluetoothShare;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.Window;
import android.view.View.OnClickListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.AdapterView.OnItemClickListener;
import android.os.Bundle;
import android.os.ParcelUuid;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.Parcelable;
import android.os.Process;
import android.telephony.TelephonyManager;

/**
 * This Activity appears as the first dialog for BPP. However, it is called only
 * when SDP find BPP record from a printer. So, even though a remote device is a
 * printer but it doesn't support BPP, then this activity will never be called.
 * This activity provide user "Setting" and "Print" option. For "Setting", user
 * can set authentication for BPP connection. For "Print", it start connection to
 * the printer.
 */
public class BluetoothBppActivity extends Activity {
/*******************************************************************************
       Class Override & Implement Methods
*******************************************************************************/
    // Debugging
    private static final String TAG = "BluetoothBppActivity";
    private static final boolean D = BluetoothBppConstant.DEBUG;
    private static final boolean V = BluetoothBppConstant.VERBOSE;

    // Member fields
    public static int JobChannel    = -1;
    public static int StatusChannel = -1;

    static Context mContext = null;
    static volatile boolean mOPPstop;
    static volatile boolean mSettingMenu;
    private boolean mButtonClicked;
    BluetoothBppTransfer bf;
/*******************************************************************************
       Class Override & Implement Methods
*******************************************************************************/
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mContext = this;
        mOPPstop = true;
        mSettingMenu = false;
        mButtonClicked = false;

        JobChannel    = getIntent().getIntExtra("jobCh", 0);
        StatusChannel = getIntent().getIntExtra("statCh", 0);
        if (V) Log.v(TAG, "BPP Activity Created - " + JobChannel + "," + StatusChannel);

        // Menu window only show during the last BPP operation.
        int id = BluetoothOppService.mBppTransId - 1;
        bf = BluetoothOppService.mBppTransfer.get(0);

        // Setup the window
        requestWindowFeature(Window.FEATURE_INDETERMINATE_PROGRESS);
        setContentView(R.layout.main);

        // Set result CANCELED incase the user backs out
        setResult(Activity.RESULT_CANCELED);

        // Initialize the button to perform device discovery
        Button scanButtonleft = (Button) findViewById(R.id.bpp_setting_button);
        Button scanButtonRight = (Button) findViewById(R.id.bpp_printing_button);

        scanButtonleft.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                if (V) Log.v(TAG, "Click'd Setting button");
                mSettingMenu = true;

                Intent in = new Intent(mContext, BluetoothBppSetting.class);
                in.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                mContext.startActivity(in);
            }
        });

        /**
         * When "Print" button is pressed, it will start RFCOMM connection with RFCOMM channel
         * number which was passed through intent from BluetoothBppTransfer Class
         */
        scanButtonRight.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                if (V) Log.v(TAG, "Click'd Printing button - " + mButtonClicked);
                // This is for protecting unexpected button click during next step is processing
                if(mButtonClicked) return;
                mButtonClicked = true;

                if (JobChannel != -1) {
                    if (bf.mSessionHandler != null)
                       if(V) Log.v(TAG," Sending Message from BPPActivity");
                       bf.mSessionHandler.obtainMessage(
                                BluetoothBppTransfer.RFCOMM_CONNECT,
                                JobChannel, StatusChannel, -1).sendToTarget();
                }
            }
        });
    }

    @Override
    protected void onResume() {
        if (V) Log.v(TAG, "onResume()");
        super.onResume();
        mSettingMenu = false;
        mContext = this;
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (V) Log.v(TAG, "onDestroy()");
        mContext = null;
    }

    /**
     * onStop() is called when current window focus is changed. There are 5 cases for window
     * focus change: 1) Back key, 2) Incoming call, 3) Home Key, 4) Next menu, 5) Power off
     * Currently, there is no way to find who tries to change current window focus, so this is
     * only way to check the condition and close BPP activity gracefully to prevent from keeping
     * showing BPP menu again when it go to the same menu.
     */
    @Override
    protected void onStop() {
        /* When home button, it will not destroy current Activity context,
                so next time when it comes to the same menu, it will show the previous window.
                To prevent this, it needs to call finish() in onStop().
               */
        if (V) Log.v(TAG, "onStop");
        super.onStop();
         /*   There are five cases for exiting from current window focus
                   1. Back key is pressed -> should stop OppService
                   2. Incoming call -> should not stop Oppservice
                   3. Home key is pressed -> should stop Oppservce
                   4. Next menu -> should not stop Oppservice
                   5. Power off -> should stop Oppservice

                   mOPPstop is set always except for in setting menu and after changing to
                   BluetoothBppPrintPrefActivity Window
             */
        TelephonyManager tm =
            (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        if (V) Log.v(TAG, "Call State: " + tm.getCallState());

        if (bf.mForceClose ||
                (!mSettingMenu && mOPPstop && !bf.mAuthChallProcess &&
                (tm.getCallState() != TelephonyManager.CALL_STATE_RINGING))) {

            if (!bf.mForceClose) {
                bf.mStatusFinal = BluetoothShare.STATUS_CANCELED;
                bf.printResultMsg();                
            }
            finish();
        }
    }
}
