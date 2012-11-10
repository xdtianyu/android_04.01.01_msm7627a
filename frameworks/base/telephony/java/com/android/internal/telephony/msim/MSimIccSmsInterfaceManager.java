/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (c) 2012 Code Aurora Forum. All rights reserved.
 * Not a Contribution, Apache license notifications and license are retained
 * for attribution purposes only.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.internal.telephony.msim;

import android.util.Log;

import com.android.internal.telephony.IccSmsInterfaceManager;
import com.android.internal.telephony.PhoneBase;

/**
 * MSimIccSmsInterfaceManager to provide an inter-process communication to
 * access Sms in Icc.
 */
public class MSimIccSmsInterfaceManager extends IccSmsInterfaceManager {

    protected MSimIccSmsInterfaceManager(PhoneBase phone){
        super(phone);
        if(DBG) Log.d(LOG_TAG, "MSimIccSmsInterfaceManager created");
    }

    @Override
    protected void initDispatchers() {
        if(DBG) Log.d(LOG_TAG, "MSimIccSmsInterfaceManager: initDispatchers()");
        mDispatcher = new MSimImsSMSDispatcher(mPhone,
                mPhone.mSmsStorageMonitor, mPhone.mSmsUsageMonitor);
    }
}
