/*
 * Copyright (C) 2012 The Android Open Source Project
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

import android.app.Activity;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.provider.Telephony;
import android.telephony.CellBroadcastMessage;
import android.view.LayoutInflater;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Full-screen emergency alert with flashing warning icon.
 * Alert audio and text-to-speech handled by {@link CellBroadcastAlertAudio}.
 * Keyguard handling based on {@code AlarmAlertFullScreen} class from DeskClock app.
 */
public class CellBroadcastAlertFullScreen extends Activity {

    /**
     * Intent extra for full screen alert launched from dialog subclass as a result of the
     * screen turning off.
     */
    static final String SCREEN_OFF = "screen_off";

    /** Whether to show the flashing warning icon. */
    private boolean mIsEmergencyAlert;

    /** The cell broadcast message to display. */
    CellBroadcastMessage mMessage;

    /** Length of time for the warning icon to be visible. */
    private static final int WARNING_ICON_ON_DURATION_MSEC = 800;

    /** Length of time for the warning icon to be off. */
    private static final int WARNING_ICON_OFF_DURATION_MSEC = 800;

    /** Warning icon state. false = visible, true = off */
    private boolean mIconAnimationState;

    /** Stop animating icon after {@link #onStop()} is called. */
    private boolean mStopAnimation;

    /** The warning icon Drawable. */
    private Drawable mWarningIcon;

    /** The View containing the warning icon. */
    private ImageView mWarningIconView;

    /** Icon animation handler for flashing warning alerts. */
    private final Handler mAnimationHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            if (mIconAnimationState) {
                mWarningIconView.setImageAlpha(255);
                if (!mStopAnimation) {
                    mAnimationHandler.sendEmptyMessageDelayed(0, WARNING_ICON_ON_DURATION_MSEC);
                }
            } else {
                mWarningIconView.setImageAlpha(0);
                if (!mStopAnimation) {
                    mAnimationHandler.sendEmptyMessageDelayed(0, WARNING_ICON_OFF_DURATION_MSEC);
                }
            }
            mIconAnimationState = !mIconAnimationState;
            mWarningIconView.invalidateDrawable(mWarningIcon);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Window win = getWindow();

        // We use a custom title, so remove the standard dialog title bar
        win.requestFeature(Window.FEATURE_NO_TITLE);

        // Full screen alerts display above the keyguard and when device is locked.
        win.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN
                | WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED
                | WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);

        // Turn on the screen unless we're being launched from the dialog subclass as a result of
        // the screen turning off.
        if (!getIntent().getBooleanExtra(SCREEN_OFF, false)) {
            win.addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON
                    | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }

        // Save message for passing from dialog to fullscreen activity, and for marking read.
        mMessage = getIntent().getParcelableExtra(
                CellBroadcastMessage.SMS_CB_MESSAGE_EXTRA);

        updateLayout(mMessage);
    }

    protected int getLayoutResId() {
        return R.layout.cell_broadcast_alert_fullscreen;
    }

    private void updateLayout(CellBroadcastMessage message) {
        LayoutInflater inflater = LayoutInflater.from(this);

        setContentView(inflater.inflate(getLayoutResId(), null));

        /* Initialize dialog text from alert message. */
        int titleId = CellBroadcastResources.getDialogTitleResource(message);
        setTitle(titleId);
        ((TextView) findViewById(R.id.alertTitle)).setText(titleId);
        ((TextView) findViewById(R.id.message)).setText(
                CellBroadcastResources.getFormattedMessageBody(this, message));

        /* dismiss button: close notification */
        findViewById(R.id.dismissButton).setOnClickListener(
                new Button.OnClickListener() {
                    public void onClick(View v) {
                        dismiss();
                    }
                });

        mIsEmergencyAlert = message.isPublicAlertMessage() || CellBroadcastConfigService
                .isOperatorDefinedEmergencyId(message.getServiceCategory());

        if (mIsEmergencyAlert) {
            mWarningIcon = getResources().getDrawable(R.drawable.ic_warning_large);
            mWarningIconView = (ImageView) findViewById(R.id.icon);
            if (mWarningIconView != null) {
                mWarningIconView.setImageDrawable(mWarningIcon);
            }

            // Dismiss the notification that brought us here
            ((NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE))
                    .cancel((int) message.getDeliveryTime());
        }
    }

    /**
     * Start animating warning icon.
     */
    @Override
    protected void onStart() {
        super.onStart();
        if (mIsEmergencyAlert) {
            // start icon animation
            mAnimationHandler.sendEmptyMessageDelayed(0, WARNING_ICON_ON_DURATION_MSEC);
        }
    }

    /**
     * Stop animating warning icon and stop the {@link CellBroadcastAlertAudio}
     * service if necessary.
     */
    void dismiss() {
        // Stop playing alert sound/vibration/speech (if started)
        stopService(new Intent(this, CellBroadcastAlertAudio.class));

        final long deliveryTime = mMessage.getDeliveryTime();

        // Mark broadcast as read on a background thread.
        new CellBroadcastContentProvider.AsyncCellBroadcastTask(getContentResolver())
                .execute(new CellBroadcastContentProvider.CellBroadcastOperation() {
                    @Override
                    public boolean execute(CellBroadcastContentProvider provider) {
                        return provider.markBroadcastRead(
                                Telephony.CellBroadcasts.DELIVERY_TIME, deliveryTime);
                    }
                });

        if (mIsEmergencyAlert) {
            // stop animating emergency alert icon
            mStopAnimation = true;
        } else {
            // decrement unread non-emergency alert count
            CellBroadcastReceiverApp.decrementUnreadAlertCount();
        }
        finish();
    }

    /**
     * Ignore the back button for emergency alerts (overridden by alert dialog so that the dialog
     * is dismissed).
     */
    @Override
    public void onBackPressed() {
        // ignored
    }
}
