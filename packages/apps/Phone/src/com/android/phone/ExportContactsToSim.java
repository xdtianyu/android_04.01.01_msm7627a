/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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
 *
 */

package com.android.phone;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ContentValues;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.provider.ContactsContract;
import android.telephony.MSimTelephonyManager;
import android.telephony.PhoneNumberUtils;
import android.util.Log;
import android.view.Window;
import android.widget.TextView;

import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;

import static android.view.Window.PROGRESS_VISIBILITY_OFF;
import static android.view.Window.PROGRESS_VISIBILITY_ON;
import static com.android.internal.telephony.MSimConstants.SUBSCRIPTION_KEY;
import static com.android.internal.telephony.MSimConstants.SUB1;
import static com.android.internal.telephony.MSimConstants.SUB2;

public class ExportContactsToSim extends Activity {
    private static final String TAG = "ExportContactsToSim";
    private TextView mEmptyText;
    private int mResult = 1;

    private static final int CONTACTS_EXPORTED = 1;
    private static final String[] COLUMN_NAMES = new String[] {
            "name",
            "number",
            "emails"
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().requestFeature(Window.FEATURE_INDETERMINATE_PROGRESS);
        setContentView(R.layout.export_contact_screen);
        mEmptyText = (TextView) findViewById(android.R.id.empty);
        doExportToSim();
    }

    private void doExportToSim() {

        displayProgress(true);

        new Thread(new Runnable() {
            public void run() {
                //Local adnList will be empty till query the SIM contacts
                //So unable to export phone contacts to SIM.
                //Before export contacts to SIM need to query SIM contacts.
                Uri uri = getUri();
                if (uri == null)  return;
                Cursor simContactsCur = getContentResolver().query(uri,
                        COLUMN_NAMES, null, null, null);

                Cursor contactsCursor = getContactsContentCursor();
                for (int i=0; contactsCursor.moveToNext(); i++) {
                    String id = getContactIdFromCursor(contactsCursor);
                    Cursor dataCursor = getDataCursorRelatedToId(id);
                    populateContactDataFromCursor(dataCursor );
                    dataCursor.close();
                }
                contactsCursor.close();
                Message message = Message.obtain(mHandler, CONTACTS_EXPORTED, (Integer)mResult);
                mHandler.sendMessage(message);
            }
        }).start();
    }

    private Cursor getContactsContentCursor() {
        Uri phoneBookContentUri = ContactsContract.Contacts.CONTENT_URI;
        String recordsWithPhoneNumberOnly = ContactsContract.Contacts.HAS_PHONE_NUMBER
                + "='1'";

        Cursor contactsCursor = managedQuery(phoneBookContentUri, null,
                recordsWithPhoneNumberOnly, null, null);
        return contactsCursor;
    }

    private String getContactIdFromCursor(Cursor contactsCursor) {
        String id = contactsCursor.getString(contactsCursor
                .getColumnIndex(ContactsContract.Contacts._ID));
        return id;
    }

    private Cursor getDataCursorRelatedToId(String id) {
        String where = ContactsContract.Data.CONTACT_ID + " = " + id;


        Cursor dataCursor = getContentResolver().query(
                ContactsContract.Data.CONTENT_URI, null, where, null, null);
        return dataCursor;
    }

    private void populateContactDataFromCursor(final Cursor dataCursor) {
        Uri uri = getUri();
        if (uri == null) {
            Log.d(TAG," populateContactDataFromCursor: uri is null, return ");
            return;
        }
        Uri contactUri;
        int nameIdx = dataCursor
                .getColumnIndex(ContactsContract.Data.DISPLAY_NAME);
        int phoneIdx = dataCursor
                .getColumnIndex(ContactsContract.CommonDataKinds.Phone.NUMBER);

        if (dataCursor.moveToFirst()) {
            // Extract the name.
            String name = dataCursor.getString(nameIdx);
            // Extract the phone number.
            String rawNumber = dataCursor.getString(phoneIdx);
            String number = PhoneNumberUtils.normalizeNumber(rawNumber);
            ContentValues values = new ContentValues();
            values.put("tag", name);
            values.put("number", number);
            Log.d("ExportContactsToSim", "name : " + name + " number : " + number);
            contactUri = getContentResolver().insert(uri, values);
            if (contactUri == null) {
                Log.e("ExportContactsToSim", "Failed to export contact to SIM for " +
                        "name : " + name + " number : " + number);
                mResult = 0;
            }
        }
    }

    private void showAlertDialog(String value) {
        AlertDialog alertDialog = new AlertDialog.Builder(this).create();
        alertDialog.setTitle("Result...");
        alertDialog.setMessage(value);
        alertDialog.setButton("OK", new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int which) {
                // finish contacts activity
                finish();
            }
        });
        alertDialog.show();
    }

    private void displayProgress(boolean loading) {
        mEmptyText.setText(R.string.exportContacts);
        getWindow().setFeatureInt(
                Window.FEATURE_INDETERMINATE_PROGRESS,
                loading ? PROGRESS_VISIBILITY_ON : PROGRESS_VISIBILITY_OFF);
    }

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch(msg.what) {
                case CONTACTS_EXPORTED:
                    int result = (Integer)msg.obj;
                    if (result == 1) {
                        showAlertDialog(getString(R.string.exportAllcontatsSuccess));
                    } else {
                        showAlertDialog(getString(R.string.exportAllcontatsFailed));
                    }
                    break;
            }
        }
    };

    private Uri getUri() {
        Intent intent = getIntent();
        Bundle extras = intent.getExtras();
        if (MSimTelephonyManager.getDefault().isMultiSimEnabled()) {
            int subscription  = extras.getInt(SUBSCRIPTION_KEY);
            Log.d("ExportContactsToSim"," subscription : " + subscription);
            if (subscription == SUB1) {
                return Uri.parse("content://iccmsim/adn");
            } else if (subscription == SUB2) {
                return Uri.parse("content://iccmsim/adn_sub2");
            } else {
                Log.e("ExportContactsToSim", "Invalid subcription");
                return null;
            }
        } else {
            return Uri.parse("content://icc/adn");
        }
    }
}
