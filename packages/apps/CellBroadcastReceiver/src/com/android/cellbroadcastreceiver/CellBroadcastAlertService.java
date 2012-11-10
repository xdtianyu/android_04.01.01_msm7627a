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

import android.app.KeyguardManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.IBinder;
import android.os.PowerManager;
import android.preference.PreferenceManager;
import android.provider.Telephony;
import android.telephony.CellBroadcastMessage;
import android.telephony.SmsCbCmasInfo;
import android.telephony.SmsCbMessage;
import android.util.Log;

/**
 * This service manages the display and animation of broadcast messages.
 * Emergency messages display with a flashing animated exclamation mark icon,
 * and an alert tone is played when the alert is first shown to the user
 * (but not when the user views a previously received broadcast).
 */
public class CellBroadcastAlertService extends Service {
    private static final String TAG = "CellBroadcastAlertService";

    /** Identifier for notification ID extra. */
    public static final String SMS_CB_NOTIFICATION_ID_EXTRA =
            "com.android.cellbroadcastreceiver.SMS_CB_NOTIFICATION_ID";

    /** Intent extra to indicate a previously unread alert. */
    static final String NEW_ALERT_EXTRA = "com.android.cellbroadcastreceiver.NEW_ALERT";

    /** Intent action to display alert dialog/notification, after verifying the alert is new. */
    static final String SHOW_NEW_ALERT_ACTION = "cellbroadcastreceiver.SHOW_NEW_ALERT";

    /** Use the same notification ID for non-emergency alerts. */
    static final int NOTIFICATION_ID = 1;

    /** CPU wake lock while handling emergency alert notification. */
    private PowerManager.WakeLock mWakeLock;

    /** Hold the wake lock for 5 seconds, which should be enough time to display the alert. */
    private static final int WAKE_LOCK_TIMEOUT = 5000;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent.getAction();
        if (Telephony.Sms.Intents.SMS_EMERGENCY_CB_RECEIVED_ACTION.equals(action) ||
                Telephony.Sms.Intents.SMS_CB_RECEIVED_ACTION.equals(action)) {
            handleCellBroadcastIntent(intent);
        } else if (SHOW_NEW_ALERT_ACTION.equals(action)) {
            showNewAlert(intent);
        } else {
            Log.e(TAG, "Unrecognized intent action: " + action);
        }
        return START_NOT_STICKY;
    }

    private void handleCellBroadcastIntent(Intent intent) {
        Bundle extras = intent.getExtras();
        if (extras == null) {
            Log.e(TAG, "received SMS_CB_RECEIVED_ACTION with no extras!");
            return;
        }

        SmsCbMessage message = (SmsCbMessage) extras.get("message");

        if (message == null) {
            Log.e(TAG, "received SMS_CB_RECEIVED_ACTION with no message extra");
            return;
        }

        final CellBroadcastMessage cbm = new CellBroadcastMessage(message);
        if (!isMessageEnabledByUser(cbm)) {
            Log.d(TAG, "ignoring alert of type " + cbm.getServiceCategory() +
                    " by user preference");
            return;
        }

        final Intent alertIntent = new Intent(SHOW_NEW_ALERT_ACTION);
        alertIntent.setClass(this, CellBroadcastAlertService.class);
        alertIntent.putExtra("message", cbm);

        // write to database on a background thread
        new CellBroadcastContentProvider.AsyncCellBroadcastTask(getContentResolver())
                .execute(new CellBroadcastContentProvider.CellBroadcastOperation() {
                    @Override
                    public boolean execute(CellBroadcastContentProvider provider) {
                        if (provider.insertNewBroadcast(cbm)) {
                            // new message, show the alert or notification on UI thread
                            startService(alertIntent);
                            return true;
                        } else {
                            return false;
                        }
                    }
                });
    }

    private void showNewAlert(Intent intent) {
        Bundle extras = intent.getExtras();
        if (extras == null) {
            Log.e(TAG, "received SHOW_NEW_ALERT_ACTION with no extras!");
            return;
        }

        CellBroadcastMessage cbm = (CellBroadcastMessage) extras.get("message");

        if (cbm == null) {
            Log.e(TAG, "received SHOW_NEW_ALERT_ACTION with no message extra");
            return;
        }

        if (cbm.isEmergencyAlertMessage() || CellBroadcastConfigService
                .isOperatorDefinedEmergencyId(cbm.getServiceCategory())) {
            // start alert sound / vibration / TTS and display full-screen alert
            openEmergencyAlertNotification(cbm);
        } else {
            // add notification to the bar
            addToNotificationBar(cbm);
        }
    }

    /**
     * Filter out broadcasts on the test channels that the user has not enabled,
     * and types of notifications that the user is not interested in receiving.
     * This allows us to enable an entire range of message identifiers in the
     * radio and not have to explicitly disable the message identifiers for
     * test broadcasts. In the unlikely event that the default shared preference
     * values were not initialized in CellBroadcastReceiverApp, the second parameter
     * to the getBoolean() calls match the default values in res/xml/preferences.xml.
     *
     * @param message the message to check
     * @return true if the user has enabled this message type; false otherwise
     */
    private boolean isMessageEnabledByUser(CellBroadcastMessage message) {
        if (message.isEtwsTestMessage()) {
            return PreferenceManager.getDefaultSharedPreferences(this)
                    .getBoolean(CellBroadcastSettings.KEY_ENABLE_ETWS_TEST_ALERTS, false);
        }

        if (message.isCmasMessage()) {
            switch (message.getCmasMessageClass()) {
                case SmsCbCmasInfo.CMAS_CLASS_EXTREME_THREAT:
                    return PreferenceManager.getDefaultSharedPreferences(this).getBoolean(
                            CellBroadcastSettings.KEY_ENABLE_CMAS_EXTREME_THREAT_ALERTS, true);

                case SmsCbCmasInfo.CMAS_CLASS_SEVERE_THREAT:
                    return PreferenceManager.getDefaultSharedPreferences(this).getBoolean(
                            CellBroadcastSettings.KEY_ENABLE_CMAS_SEVERE_THREAT_ALERTS, true);

                case SmsCbCmasInfo.CMAS_CLASS_CHILD_ABDUCTION_EMERGENCY:
                    return PreferenceManager.getDefaultSharedPreferences(this)
                            .getBoolean(CellBroadcastSettings.KEY_ENABLE_CMAS_AMBER_ALERTS, true);

                case SmsCbCmasInfo.CMAS_CLASS_REQUIRED_MONTHLY_TEST:
                case SmsCbCmasInfo.CMAS_CLASS_CMAS_EXERCISE:
                case SmsCbCmasInfo.CMAS_CLASS_OPERATOR_DEFINED_USE:
                    return PreferenceManager.getDefaultSharedPreferences(this)
                            .getBoolean(CellBroadcastSettings.KEY_ENABLE_CMAS_TEST_ALERTS, false);

                default:
                    return true;    // presidential-level CMAS alerts are always enabled
            }
        }

        return true;    // other broadcast messages are always enabled
    }

    private void acquireTimedWakelock(int timeout) {
        if (mWakeLock == null) {
            PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
            // Note: acquiring a PARTIAL_WAKE_LOCK and setting window flag FLAG_TURN_SCREEN_ON in
            // CellBroadcastAlertFullScreen is not sufficient to turn on the screen by itself.
            // Use SCREEN_BRIGHT_WAKE_LOCK here as a workaround to ensure the screen turns on.
            mWakeLock = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK
                    | PowerManager.ACQUIRE_CAUSES_WAKEUP, TAG);
        }
        mWakeLock.acquire(timeout);
    }

    /**
     * Display a full-screen alert message for emergency alerts.
     * @param message the alert to display
     */
    private void openEmergencyAlertNotification(CellBroadcastMessage message) {
        // Acquire a CPU wake lock until the alert dialog and audio start playing.
        acquireTimedWakelock(WAKE_LOCK_TIMEOUT);

        // Close dialogs and window shade
        Intent closeDialogs = new Intent(Intent.ACTION_CLOSE_SYSTEM_DIALOGS);
        sendBroadcast(closeDialogs);

        // start audio/vibration/speech service for emergency alerts
        Intent audioIntent = new Intent(this, CellBroadcastAlertAudio.class);
        audioIntent.setAction(CellBroadcastAlertAudio.ACTION_START_ALERT_AUDIO);
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        String duration = prefs.getString(CellBroadcastSettings.KEY_ALERT_SOUND_DURATION,
                CellBroadcastSettings.ALERT_SOUND_DEFAULT_DURATION);
        audioIntent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_DURATION_EXTRA,
                Integer.parseInt(duration));

        int channelTitleId = CellBroadcastResources.getDialogTitleResource(message);
        CharSequence channelName = getText(channelTitleId);
        String messageBody = message.getMessageBody();

        if (prefs.getBoolean(CellBroadcastSettings.KEY_ENABLE_ALERT_SPEECH, true)) {
            audioIntent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_MESSAGE_BODY, messageBody);

            String language = message.getLanguageCode();
            if (message.isEtwsMessage() && !"ja".equals(language)) {
                Log.w(TAG, "bad language code for ETWS - using Japanese TTS");
                language = "ja";
            } else if (message.isCmasMessage() && !"en".equals(language)) {
                Log.w(TAG, "bad language code for CMAS - using English TTS");
                language = "en";
            }
            audioIntent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_MESSAGE_LANGUAGE,
                    language);
        }
        startService(audioIntent);

        // Use lower 32 bits of emergency alert delivery time for notification ID
        int notificationId = (int) message.getDeliveryTime();

        // Decide which activity to start based on the state of the keyguard.
        Class c = CellBroadcastAlertDialog.class;
        KeyguardManager km = (KeyguardManager) getSystemService(Context.KEYGUARD_SERVICE);
        if (km.inKeyguardRestrictedInputMode()) {
            // Use the full screen activity for security.
            c = CellBroadcastAlertFullScreen.class;
        }

        Intent notify = createDisplayMessageIntent(this, c, message, notificationId);
        PendingIntent pi = PendingIntent.getActivity(this, notificationId, notify, 0);

        Notification.Builder builder = new Notification.Builder(this)
                .setSmallIcon(R.drawable.ic_notify_alert)
                .setTicker(getText(CellBroadcastResources.getDialogTitleResource(message)))
                .setWhen(System.currentTimeMillis())
                .setContentIntent(pi)
                .setFullScreenIntent(pi, true)
                .setContentTitle(channelName)
                .setContentText(messageBody)
                .setDefaults(Notification.DEFAULT_LIGHTS);

        NotificationManager notificationManager =
            (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);

        notificationManager.notify(notificationId, builder.getNotification());
    }

    /**
     * Add the new alert to the notification bar (non-emergency alerts), or launch a
     * high-priority immediate intent for emergency alerts.
     * @param message the alert to display
     */
    private void addToNotificationBar(CellBroadcastMessage message) {
        int channelTitleId = CellBroadcastResources.getDialogTitleResource(message);
        CharSequence channelName = getText(channelTitleId);
        String messageBody = message.getMessageBody();

        // Use the same ID to create a single notification for multiple non-emergency alerts.
        int notificationId = NOTIFICATION_ID;

        PendingIntent pi = PendingIntent.getActivity(this, 0, createDisplayMessageIntent(
                this, CellBroadcastListActivity.class, message, notificationId), 0);

        // use default sound/vibration/lights for non-emergency broadcasts
        Notification.Builder builder = new Notification.Builder(this)
                .setSmallIcon(R.drawable.ic_notify_alert)
                .setTicker(channelName)
                .setWhen(System.currentTimeMillis())
                .setContentIntent(pi)
                .setDefaults(Notification.DEFAULT_ALL);

        builder.setDefaults(Notification.DEFAULT_ALL);

        // increment unread alert count (decremented when user dismisses alert dialog)
        int unreadCount = CellBroadcastReceiverApp.incrementUnreadAlertCount();
        if (unreadCount > 1) {
            // use generic count of unread broadcasts if more than one unread
            builder.setContentTitle(getString(R.string.notification_multiple_title));
            builder.setContentText(getString(R.string.notification_multiple, unreadCount));
        } else {
            builder.setContentTitle(channelName).setContentText(messageBody);
        }

        Log.i(TAG, "addToNotificationBar notificationId: " + notificationId);

        NotificationManager notificationManager =
            (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);

        notificationManager.notify(notificationId, builder.getNotification());
    }

    static Intent createDisplayMessageIntent(Context context, Class intentClass,
            CellBroadcastMessage message, int notificationId) {
        // Trigger the list activity to fire up a dialog that shows the received messages
        Intent intent = new Intent(context, intentClass);
        intent.putExtra(CellBroadcastMessage.SMS_CB_MESSAGE_EXTRA, message);
        intent.putExtra(SMS_CB_NOTIFICATION_ID_EXTRA, notificationId);
        intent.putExtra(NEW_ALERT_EXTRA, true);

        // This line is needed to make this intent compare differently than the other intents
        // created here for other messages. Without this line, the PendingIntent always gets the
        // intent of a previous message and notification.
        intent.setType(Integer.toString(notificationId));

        return intent;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;    // clients can't bind to this service
    }
}
