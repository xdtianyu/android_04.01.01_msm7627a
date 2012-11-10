/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.cellbroadcastreceiver;

import android.app.Application;
import android.util.Log;
import android.preference.PreferenceManager;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * The application class loads the default preferences at first start,
 * and remembers the time of the most recently received broadcast.
 */
public class CellBroadcastReceiverApp extends Application {
    private static final String TAG = "CellBroadcastReceiverApp";

    @Override
    public void onCreate() {
        super.onCreate();
        // TODO: fix strict mode violation from the following method call during app creation
        PreferenceManager.setDefaultValues(this, R.xml.preferences, false);
    }

    /** Number of unread non-emergency alerts since the device was booted. */
    private static AtomicInteger sUnreadAlertCount = new AtomicInteger();

    /**
     * Increments the number of unread non-emergency alerts, returning the new value.
     * @return the updated number of unread non-emergency alerts, after incrementing
     */
    static int incrementUnreadAlertCount() {
        return sUnreadAlertCount.incrementAndGet();
    }

    /**
     * Decrements the number of unread non-emergency alerts after the user reads it.
     */
    static void decrementUnreadAlertCount() {
        if (sUnreadAlertCount.decrementAndGet() < 0) {
            Log.e(TAG, "mUnreadAlertCount < 0, resetting to 0");
            sUnreadAlertCount.set(0);
        }
    }

    /** Resets the unread alert count to zero after user deletes all alerts. */
    static void resetUnreadAlertCount() {
        sUnreadAlertCount.set(0);
    }
}
