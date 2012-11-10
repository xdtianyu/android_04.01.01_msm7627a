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
import com.android.bluetooth.opp.BluetoothOppService;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.preference.Preference;
import android.util.Log;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Button;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.text.InputFilter;
import android.text.TextWatcher;
import android.text.InputFilter.LengthFilter;

import com.android.internal.app.AlertActivity;
import com.android.internal.app.AlertController;

/**
 * This activity start Bluetooth Pincode user input dialog. This is different from bonding
 * which is general secure method for new link. This activity is dedicatedly used for BPP
 * authentication operation.
 */
public class BluetoothBppAuthActivity extends AlertActivity implements
        DialogInterface.OnClickListener, Preference.OnPreferenceChangeListener, TextWatcher {
    private static final String TAG = "BluetoothBppActivity";

    private static final boolean V = BluetoothBppConstant.VERBOSE;

    private static final int BLUETOOTH_OBEX_AUTHKEY_MAX_LENGTH = 16;

    private static final int DIALOG_YES_NO_CONNECT = 1;

    private static final int DIALOG_YES_NO_AUTH = 2;

    private static final String KEY_USER_TIMEOUT = "user_timeout";

    private View mView;

    private EditText mKeyView;

    private TextView messageView;

    private String mSessionKey = "";

    private int mCurrentDialog;

    private Button mOkButton;

    private CheckBox mAlwaysAllowed;

    private boolean mTimeout = false;

    private boolean mAlwaysAllowedValue = true;

    private static final int DISMISS_TIMEOUT_DIALOG = 0;

    private static final int DISMISS_TIMEOUT_DIALOG_VALUE = 2000;

    BluetoothBppTransfer bf;

    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!BluetoothBppTransfer.USER_CONFIRM_TIMEOUT_ACTION.equals(intent.getAction())) {
                return;
            }
            onTimeout();
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent i = getIntent();
        // Menu window only show during the last BPP operation.
        int idx = BluetoothOppService.mBppTransId - 1;
        bf = BluetoothOppService.mBppTransfer.get(idx);

        showBppDialog(DIALOG_YES_NO_AUTH);
        mCurrentDialog = DIALOG_YES_NO_AUTH;

        registerReceiver(mReceiver, new IntentFilter(
                BluetoothBppTransfer.USER_CONFIRM_TIMEOUT_ACTION));
    }

    /*
     * Creates a Button with Yes/No dialog
     */
    private void showBppDialog(int id) {
        final AlertController.AlertParams p = mAlertParams;
        switch (id) {
            case DIALOG_YES_NO_AUTH:
                p.mIconId = android.R.drawable.ic_dialog_info;
                p.mTitle = getString(R.string.bpp_session_key_dialog_header);
                p.mView = createView(DIALOG_YES_NO_AUTH);
                p.mPositiveButtonText = getString(android.R.string.ok);
                p.mPositiveButtonListener = this;
                p.mNegativeButtonText = getString(android.R.string.cancel);
                p.mNegativeButtonListener = this;
                setupAlert();
                mOkButton = mAlert.getButton(DialogInterface.BUTTON_POSITIVE);
                mOkButton.setEnabled(false);
                break;
            default:
                break;
        }
    }

    private String createDisplayText(final int id) {

        String mRemoteName = bf.getRemoteDeviceName();
        switch (id) {
            case DIALOG_YES_NO_AUTH:
                String mMessage = getString(R.string.bpp_session_key_dialog_title, mRemoteName);
                return mMessage;
            default:
                return null;
        }
    }

    /*
     * Creates a view for the dialog and text to get the user inputs
     */
    private View createView(final int id) {
        switch (id) {
            case DIALOG_YES_NO_AUTH:
                mView = getLayoutInflater().inflate(R.layout.auth, null);
                messageView = (TextView)mView.findViewById(R.id.message);
                messageView.setText(createDisplayText(id));
                mKeyView = (EditText)mView.findViewById(R.id.text);
                mKeyView.addTextChangedListener(this);
                mKeyView.setFilters(new InputFilter[] {
                    new LengthFilter(BLUETOOTH_OBEX_AUTHKEY_MAX_LENGTH)
                });
                return mView;
            default:
                return null;
        }
    }

    private void onPositive() {
        if (!mTimeout) {
            if (mCurrentDialog == DIALOG_YES_NO_AUTH) {
                BluetoothBppTransfer.notifyAuthKeyInput(mSessionKey);
                mKeyView.removeTextChangedListener(this);
            }
        }
        mTimeout = false;
        finish();
    }

    private void onNegative() {
        if (mCurrentDialog == DIALOG_YES_NO_AUTH) {
            BluetoothBppTransfer.notifyAuthCancelled();
            mKeyView.removeTextChangedListener(this);
        }
        finish();
    }

    public void onClick(DialogInterface dialog, int which) {
        switch (which) {
            case DialogInterface.BUTTON_POSITIVE:
                if (mCurrentDialog == DIALOG_YES_NO_AUTH) {
                    mSessionKey = mKeyView.getText().toString();
                }
                onPositive();
                break;

            case DialogInterface.BUTTON_NEGATIVE:
                onNegative();
                break;
            default:
                break;
        }
    }

    private void onTimeout() {
        mTimeout = true;
        Button mAlertButton;
        if (mCurrentDialog == DIALOG_YES_NO_AUTH) {
            messageView.setText(getString(R.string.bpp_authentication_timeout_message,
                bf.getRemoteDeviceName()));
            mKeyView.setVisibility(View.GONE);
            mKeyView.clearFocus();
            mKeyView.removeTextChangedListener(this);
            mOkButton.setEnabled(true);
            mAlertButton = mAlert.getButton(DialogInterface.BUTTON_NEGATIVE);
            if (mAlertButton != null) {
                mAlertButton.setVisibility(View.GONE);
            } else {
                Log.e(TAG, "Error! mAlertButton is null, can't set visibility");
            }
        }

        mTimeoutHandler.sendMessageDelayed(mTimeoutHandler.obtainMessage(DISMISS_TIMEOUT_DIALOG),
                DISMISS_TIMEOUT_DIALOG_VALUE);
    }

    @Override
    protected void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        mTimeout = savedInstanceState.getBoolean(KEY_USER_TIMEOUT);
        if (V) Log.v(TAG, "onRestoreInstanceState() mTimeout: " + mTimeout);
        if (mTimeout) {
            onTimeout();
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean(KEY_USER_TIMEOUT, mTimeout);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(mReceiver);
    }

    public boolean onPreferenceChange(Preference preference, Object newValue) {
        return true;
    }

    public void beforeTextChanged(CharSequence s, int start, int before, int after) {
    }

    public void onTextChanged(CharSequence s, int start, int before, int count) {
    }

    public void afterTextChanged(android.text.Editable s) {
        if (s.length() > 0) {
            mOkButton.setEnabled(true);
        }
    }

    private final Handler mTimeoutHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case DISMISS_TIMEOUT_DIALOG:
                    if (V) Log.v(TAG, "Received DISMISS_TIMEOUT_DIALOG msg.");
                    finish();
                    break;
                default:
                    break;
            }
        }
    };
}
