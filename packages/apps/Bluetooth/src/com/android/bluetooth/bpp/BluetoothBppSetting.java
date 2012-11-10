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
import com.android.bluetooth.opp.BluetoothOppService;
import com.android.bluetooth.opp.BluetoothShare;

import com.android.bluetooth.R;
import android.app.ListActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.telephony.TelephonyManager;
import android.content.Context;
import android.content.Intent;

/**
 * This activity class is for OBEX authentication setting.
 * It will show check box to enable OBEX authentication.
 */
public class BluetoothBppSetting extends ListActivity{
    TextView selection;
    String[] items = {"OBEX Authentication"};
    private static final String TAG = "BluetoothBppSetting";
    private static final boolean D = BluetoothBppConstant.DEBUG;
    private static final boolean V = BluetoothBppConstant.VERBOSE;
    public static boolean bpp_auth = false;
    private boolean mBackKeyPressed = false;
    static Context mContext = null;
    BluetoothBppTransfer bf;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.bpp_setting);
        mBackKeyPressed = false;
        mContext = this;

        // Menu window only show during the last BPP operation.
        int id = BluetoothOppService.mBppTransId - 1;
        bf = BluetoothOppService.mBppTransfer.get(id);

        setListAdapter(new ArrayAdapter<String> (this,
                android.R.layout.simple_list_item_multiple_choice,items));

        final ListView listView = getListView();
        listView.setItemsCanFocus(false);
        listView.setChoiceMode(ListView.CHOICE_MODE_SINGLE);
        listView.setItemChecked(0, bpp_auth);

        selection=(TextView)findViewById(R.id.selection);
    }

    @Override
    public void onListItemClick(ListView parent, View v, int position, long id) {
        bpp_auth =(bpp_auth)? false : true ;
        parent.setItemChecked(position, bpp_auth);
        Log.d(TAG, "new bpp_auth: " + bpp_auth);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (V) Log.v(TAG, "onDestroy()");
        mContext = null;
    }

    @Override
    protected void onStop() {
        if (V) Log.v(TAG, "onStop");
        super.onStop();
        BluetoothBppActivity.mSettingMenu = false;

         /*   There are three cases for exiting from current window focus
                   1. Back key is pressed -> should not stop OppService
                   2. Incoming call -> should not stop Oppservice
                   3. Home key is pressed -> should stop Oppservce
                   4. Power off -> should stop Oppservice
             */
        TelephonyManager tm =
            (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        if (V) Log.v(TAG, "Call State: " + tm.getCallState());
         if (bf.mForceClose ||
             (!mBackKeyPressed && (tm.getCallState() != TelephonyManager.CALL_STATE_RINGING))) {
             if (!bf.mForceClose) {
                 bf.mStatusFinal = BluetoothShare.STATUS_CANCELED;
                 bf.printResultMsg();
             }
             finish();
         }
    }

    @Override
    public void onBackPressed() {
    if (V) Log.v(TAG, "onBackPressed ");
        mBackKeyPressed = true;
        super.onBackPressed();
    }
}
