/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

import com.android.bluetooth.R;
import com.android.bluetooth.opp.BluetoothOppService;
import com.android.bluetooth.opp.BluetoothShare;

import android.app.Activity;
import android.content.Intent;
import android.content.Context;
import android.os.Bundle;
import android.preference.EditTextPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;
import android.preference.Preference.OnPreferenceChangeListener;
import android.util.Log;
import android.telephony.TelephonyManager;

/**
 * This class is for Printer Preference setting.
 * However, most BPP printers do not support this feature.
 */
public class BluetoothBppPrintPrefActivity extends PreferenceActivity implements OnPreferenceChangeListener{
    private static final String TAG = "BluetoothBppPrintPrefActivity";

    private static final boolean D = BluetoothBppConstant.DEBUG;

    private static final boolean V = BluetoothBppConstant.VERBOSE;

    private EditTextPreference mListCopies;

    private ListPreference mListNumberUp;

    private ListPreference mListOrient;

    private ListPreference mListSides;

    private PreferenceScreen mPrintStart;

    private static final String TEXT_COPIES_KEY 	=  "text_copies";

    private static final String LIST_NUMBERUP_KEY 	=  "list_numberup";

    private static final String LIST_ORIENT_KEY 	=  "list_orient";

    private static final String LIST_SIDES_KEY 		=  "list_sides";

    private static final String SCREEN_PRINT_KEY	=  "print";

    static String mCopies   = "1";

    static String mNumUp    = "1";

    static String mOrient   = "portrait";

    static String mSides    = "one-sided";

    static public boolean mOPPstop = false;

    static public boolean mBPPstop = false;

    static Context mContext = null;

    BluetoothBppTransfer bf;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mOPPstop = true;
        mBPPstop = true;
        mContext = this;

        // Menu window only show during the last BPP operation.
        int id = BluetoothOppService.mBppTransId - 1;
        bf = BluetoothOppService.mBppTransfer.get(0);

        addPreferencesFromResource(R.xml.printing_pref);

        PreferenceScreen prefSet = getPreferenceScreen();
        mListCopies = (EditTextPreference) prefSet.findPreference(TEXT_COPIES_KEY);
        mListNumberUp = (ListPreference) prefSet.findPreference(LIST_NUMBERUP_KEY);
        mListOrient = (ListPreference) prefSet.findPreference(LIST_ORIENT_KEY);
        mListSides = (ListPreference) prefSet.findPreference(LIST_SIDES_KEY);
        mPrintStart = (PreferenceScreen) prefSet.findPreference(SCREEN_PRINT_KEY);

        mListCopies.setOnPreferenceChangeListener(this);
        mListNumberUp.setOnPreferenceChangeListener(this);
        mListOrient.setOnPreferenceChangeListener(this);
        mListSides.setOnPreferenceChangeListener(this);
        mPrintStart.setOnPreferenceChangeListener(this);

        getPreferenceScreen().setEnabled(true);

        if(bf.mSession.bs.mPrinter_MaxCopies > 1){
            mListCopies.setEnabled(true);
            mListCopies.setSummary("Current Setting: "+ mCopies + ", Click to change");
        }else{
            mListCopies.setEnabled(false);
            mListCopies.setSummary("Printer support only 1 copy");
        }
        if(bf.mSession.bs.mPrinter_NumberUp > 1){
            mListNumberUp.setEnabled(true);
            mListNumberUp.setSummary("Current Setting: "+ mNumUp+ ", Click to change");
        }
        else{
            mListNumberUp.setEnabled(false);
            mListNumberUp.setSummary("Printer support only 1 page per side");
        }

        if(bf.mSession.bs.mPrinter_Orientation != null){
            mListOrient.setEnabled(true);
            mListOrient.setSummary("Current Setting: "+ mOrient+ ", Click to change");
        }
        else{
            mListOrient.setEnabled(false);
            mListOrient.setSummary("Printer support only portrait");
        }
        if(bf.mSession.bs.mPrinter_Sides != null){
            mListSides.setEnabled(true);
            mListSides.setSummary("Current Setting: "+ mSides+ ", Click to change");
        }
        else{
            mListSides.setEnabled(false);
            mListSides.setSummary("Printer support only one-sided");
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        mContext = this;
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen,
        Preference preference) {

        if(V)Log.v(TAG,"preferenceScreen: " +  preferenceScreen + ", preference: " + preference );

        if (preference == mPrintStart) {
            if(V)Log.v(TAG,"mPrintStart: ");
            bf.mSessionHandler.obtainMessage(
                        BluetoothBppTransfer.CREATE_JOB, -1).sendToTarget();

            Intent in = new Intent(mContext, BluetoothBppStatusActivity.class);
            in.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivity(in);
            mOPPstop = false;
            mBPPstop = false;
            finish();
        }
        return true;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {

        if(V)Log.v(TAG,"preference: " +  preference + ", newValue: " + newValue );

        if (preference == mListCopies) {
            if(V)Log.v(TAG,"mListCopies: " + newValue );
            mCopies = (String)newValue;
        }
        else if(preference == mListNumberUp) {
            if(V)Log.v(TAG,"mListNumberUp: " + newValue );
            mNumUp= (String)newValue;
        }
        else if(preference == mListOrient) {
            if(V)Log.v(TAG,"mListOrient: " + newValue );
            mOrient= (String)newValue;
        }
        else if(preference == mListSides) {
            if(V)Log.v(TAG,"mListSides: " + newValue );
            mSides= (String)newValue;
        }

        return true;
    }

    @Override
    protected void onDestroy() {
        if (V) Log.v(TAG, "onDestroy()");
        super.onDestroy();
        mContext = null;
    }

    @Override
    protected void onStop() {
        if (V) Log.v(TAG, "onStop()");
         /*   There are five cases for exiting from current window focus
                   1. Back key is pressed -> should stop OppService
                   2. Incoming call -> should not stop Oppservice
                   3. Home key is pressed -> should stop Oppservce
                   4. Next menu -> should not stop Oppservice
                   5. Power off -> should stop Oppservice

                   mOPPstop is set always except for changing to BluetoothBppStatusActivity Window.
             */
        TelephonyManager tm =
            (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        if (V) Log.v(TAG, "Call State: " + tm.getCallState());

        if(mBPPstop){
            if (bf.mSession != null && bf.mSessionHandler != null) {
                bf.mSessionHandler.obtainMessage(
                        BluetoothBppTransfer.CANCEL, -1).sendToTarget();
            }
        }
        if(bf.mForceClose
            ||(mOPPstop && (tm.getCallState() != TelephonyManager.CALL_STATE_RINGING))) {
            if (!bf.mForceClose) {
                bf.mStatusFinal = BluetoothShare.STATUS_CANCELED;
                bf.printResultMsg();
            }
            finish();
        }
        super.onStop();
    }
}
