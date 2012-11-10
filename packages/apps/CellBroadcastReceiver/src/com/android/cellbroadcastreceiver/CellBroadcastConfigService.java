/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

import android.app.IntentService;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.SystemProperties;
import android.preference.PreferenceManager;
import android.telephony.SmsManager;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.telephony.gsm.SmsCbConstants;
import com.android.internal.telephony.cdma.sms.SmsEnvelope;

import static com.android.cellbroadcastreceiver.CellBroadcastReceiver.DBG;

/**
 * This service manages enabling and disabling ranges of message identifiers
 * that the radio should listen for. It operates independently of the other
 * services and runs at boot time and after exiting airplane mode.
 *
 * Note that the entire range of emergency channels is enabled. Test messages
 * and lower priority broadcasts are filtered out in CellBroadcastAlertService
 * if the user has not enabled them in settings.
 *
 * TODO: add notification to re-enable channels after a radio reset.
 */
public class CellBroadcastConfigService extends IntentService {
    private static final String TAG = "CellBroadcastConfigService";

    static final String ACTION_ENABLE_CHANNELS_GSM = "ACTION_ENABLE_CHANNELS_GSM";
    static final String ACTION_ENABLE_CHANNELS_CDMA = "ACTION_ENABLE_CHANNELS_CDMA";

    static final String EMERGENCY_BROADCAST_RANGE_GSM =
            "ro.cb.gsm.emergencyids";

    // system property defining the emergency cdma channel ranges
    // Note: key name cannot exceeds 32 chars.
    static final String EMERGENCY_BROADCAST_RANGE_CDMA =
            "ro.cb.cdma.emergencyids";

    public CellBroadcastConfigService() {
        super(TAG);          // use class name for worker thread name
    }

    private void setChannelRange(SmsManager manager, String ranges,
            boolean enable, boolean isCdma) {
        if (DBG) log("setChannelRange: " + ranges);

        try {
            for (String channelRange : ranges.split(",")) {
                int dashIndex = channelRange.indexOf('-');
                if (dashIndex != -1) {
                    int startId = Integer.decode(channelRange.substring(0, dashIndex).trim());
                    int endId = Integer.decode(channelRange.substring(dashIndex + 1).trim());
                    if (enable) {
                        if (DBG) log("enabling emergency IDs " + startId + '-' + endId);
                        if (isCdma) {
                            manager.enableCdmaBroadcastRange(startId, endId);
                        } else {
                            manager.enableCellBroadcastRange(startId, endId);
                        }
                    } else {
                        if (DBG) log("disabling emergency IDs " + startId + '-' + endId);
                        if (isCdma) {
                            manager.disableCdmaBroadcastRange(startId, endId);
                        } else {
                            manager.disableCellBroadcastRange(startId, endId);
                        }
                    }
                } else {
                    int messageId = Integer.decode(channelRange.trim());
                    if (enable) {
                        if (DBG) log("enabling emergency message ID " + messageId);
                        if (isCdma) {
                            manager.enableCdmaBroadcast(messageId);
                        } else {
                            manager.enableCellBroadcast(messageId);
                        }
                    } else {
                        if (DBG) log("disabling emergency message ID " + messageId);
                        if (isCdma) {
                            manager.disableCdmaBroadcast(messageId);
                        } else {
                            manager.disableCellBroadcast(messageId);
                        }
                    }
                }
            }
        } catch (NumberFormatException e) {
            Log.e(TAG, "Number Format Exception parsing emergency channel range", e);
        }

        // Make sure CMAS Presidential is enabled (See 3GPP TS 22.268 Section 6.2).
        if (DBG) log("setChannelRange: enabling CMAS Presidential");
        if (isCdma) {
            manager.enableCdmaBroadcast(SmsEnvelope.SERVICE_CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT);
        } else {
            manager.enableCellBroadcast(SmsCbConstants.MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL);
        }
    }

    static boolean isOperatorDefinedEmergencyId(int messageId) {
        // Check for system property defining the emergency channel ranges to enable
        String emergencyIdRange = (CellBroadcastReceiver.phoneIsCdma()) ?
                SystemProperties.get(EMERGENCY_BROADCAST_RANGE_CDMA) :
                    SystemProperties.get(EMERGENCY_BROADCAST_RANGE_GSM);

        if (TextUtils.isEmpty(emergencyIdRange)) {
            return false;
        }
        try {
            for (String channelRange : emergencyIdRange.split(",")) {
                int dashIndex = channelRange.indexOf('-');
                if (dashIndex != -1) {
                    int startId = Integer.decode(channelRange.substring(0, dashIndex).trim());
                    int endId = Integer.decode(channelRange.substring(dashIndex + 1).trim());
                    if (messageId >= startId && messageId <= endId) {
                        return true;
                    }
                } else {
                    int emergencyMessageId = Integer.decode(channelRange.trim());
                    if (emergencyMessageId == messageId) {
                        return true;
                    }
                }
            }
        } catch (NumberFormatException e) {
            Log.e(TAG, "Number Format Exception parsing emergency channel range", e);
        }
        return false;
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        if (ACTION_ENABLE_CHANNELS_GSM.equals(intent.getAction())) {
            configGsmChannels();
        } else if (ACTION_ENABLE_CHANNELS_CDMA.equals(intent.getAction())) {
            configCdmaChannels();
        }
    }

    private void configGsmChannels() {
        try {
            SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
            Resources res = getResources();

            // Check for system property defining the emergency channel ranges to enable
            String emergencyIdRange = SystemProperties.get(EMERGENCY_BROADCAST_RANGE_GSM);

            boolean enableEmergencyAlerts = prefs.getBoolean(
                    CellBroadcastSettings.KEY_ENABLE_EMERGENCY_ALERTS, true);

            boolean enableChannel50Alerts = res.getBoolean(R.bool.show_brazil_settings) &&
                    prefs.getBoolean(CellBroadcastSettings.KEY_ENABLE_CHANNEL_50_ALERTS, true);

            boolean enableEtwsTestAlerts = prefs.getBoolean(
                    CellBroadcastSettings.KEY_ENABLE_ETWS_TEST_ALERTS, false);

            boolean enableCmasExtremeAlerts = prefs.getBoolean(
                    CellBroadcastSettings.KEY_ENABLE_CMAS_EXTREME_THREAT_ALERTS, true);

            boolean enableCmasSevereAlerts = prefs.getBoolean(
                    CellBroadcastSettings.KEY_ENABLE_CMAS_SEVERE_THREAT_ALERTS, true);

            boolean enableCmasAmberAlerts = prefs.getBoolean(
                    CellBroadcastSettings.KEY_ENABLE_CMAS_AMBER_ALERTS, true);

            boolean enableCmasTestAlerts = prefs.getBoolean(
                    CellBroadcastSettings.KEY_ENABLE_CMAS_TEST_ALERTS, false);

            SmsManager manager = SmsManager.getDefault();
            if (enableEmergencyAlerts) {
                if (DBG) log("enabling emergency cell broadcast channels");
                if (!TextUtils.isEmpty(emergencyIdRange)) {
                    setChannelRange(manager, emergencyIdRange, true, false);
                } else {
                    // No emergency channel system property, enable all
                    // emergency channels
                    manager.enableCellBroadcastRange(
                            SmsCbConstants.MESSAGE_ID_ETWS_EARTHQUAKE_WARNING,
                            SmsCbConstants.MESSAGE_ID_ETWS_EARTHQUAKE_AND_TSUNAMI_WARNING);
                    if (enableEtwsTestAlerts) {
                        manager.enableCellBroadcast(
                                SmsCbConstants.MESSAGE_ID_ETWS_TEST_MESSAGE);
                    }
                    manager.enableCellBroadcast(
                            SmsCbConstants.MESSAGE_ID_ETWS_OTHER_EMERGENCY_TYPE);
                    if (enableCmasExtremeAlerts) {
                        manager.enableCellBroadcastRange(
                                SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_OBSERVED,
                                SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_EXPECTED_LIKELY);
                    }
                    if (enableCmasSevereAlerts) {
                        manager.enableCellBroadcastRange(
                                SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_IMMEDIATE_OBSERVED,
                                SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_EXPECTED_LIKELY);
                    }
                    if (enableCmasAmberAlerts) {
                        manager.enableCellBroadcast(
                                SmsCbConstants.MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY);
                    }
                    if (enableCmasTestAlerts) {
                        manager.enableCellBroadcastRange(
                                SmsCbConstants.MESSAGE_ID_CMAS_ALERT_REQUIRED_MONTHLY_TEST,
                                SmsCbConstants.MESSAGE_ID_CMAS_ALERT_OPERATOR_DEFINED_USE);
                    }
                    // CMAS Presidential must be on (See 3GPP TS 22.268 Section 6.2).
                    manager.enableCellBroadcast(
                            SmsCbConstants.MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL);
                }
                if (DBG) log("enabled emergency cell broadcast channels");
            } else {
                // we may have enabled these channels previously, so try to disable them
                if (DBG) log("disabling emergency cell broadcast channels");
                if (!TextUtils.isEmpty(emergencyIdRange)) {
                    setChannelRange(manager, emergencyIdRange, false, false);
                } else {
                    // No emergency channel system property, disable all emergency channels
                    // except for CMAS Presidential (See 3GPP TS 22.268 Section 6.2)
                    manager.disableCellBroadcastRange(
                            SmsCbConstants.MESSAGE_ID_ETWS_EARTHQUAKE_WARNING,
                            SmsCbConstants.MESSAGE_ID_ETWS_EARTHQUAKE_AND_TSUNAMI_WARNING);
                    manager.disableCellBroadcast(
                            SmsCbConstants.MESSAGE_ID_ETWS_TEST_MESSAGE);
                    manager.disableCellBroadcast(
                            SmsCbConstants.MESSAGE_ID_ETWS_OTHER_EMERGENCY_TYPE);
                    manager.disableCellBroadcastRange(
                            SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_OBSERVED,
                            SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_EXPECTED_LIKELY);
                    manager.disableCellBroadcastRange(
                            SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_IMMEDIATE_OBSERVED,
                            SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_EXPECTED_LIKELY);
                    manager.disableCellBroadcast(
                            SmsCbConstants.MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY);
                    manager.disableCellBroadcastRange(
                            SmsCbConstants.MESSAGE_ID_CMAS_ALERT_REQUIRED_MONTHLY_TEST,
                            SmsCbConstants.MESSAGE_ID_CMAS_ALERT_OPERATOR_DEFINED_USE);
                    manager.enableCellBroadcast(
                            SmsCbConstants.MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL);
                }
                if (DBG) log("disabled emergency cell broadcast channels");
            }

            if (enableChannel50Alerts) {
                if (DBG) log("enabling cell broadcast channel 50");
                manager.enableCellBroadcast(50);
                if (DBG) log("enabled cell broadcast channel 50");
            } else {
                if (DBG) log("disabling cell broadcast channel 50");
                manager.disableCellBroadcast(50);
                if (DBG) log("disabled cell broadcast channel 50");
            }

            if (!enableEtwsTestAlerts) {
                if (DBG) Log.d(TAG, "disabling cell broadcast ETWS test messages");
                manager.disableCellBroadcast(
                        SmsCbConstants.MESSAGE_ID_ETWS_TEST_MESSAGE);
            }
            if (!enableCmasExtremeAlerts) {
                if (DBG) Log.d(TAG, "disabling cell broadcast CMAS extreme");
                manager.disableCellBroadcastRange(
                        SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_OBSERVED,
                        SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_EXPECTED_LIKELY);
            }
            if (!enableCmasSevereAlerts) {
                if (DBG) Log.d(TAG, "disabling cell broadcast CMAS severe");
                manager.disableCellBroadcastRange(
                        SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_IMMEDIATE_OBSERVED,
                        SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_EXPECTED_LIKELY);
            }
            if (!enableCmasAmberAlerts) {
                if (DBG) Log.d(TAG, "disabling cell broadcast CMAS amber");
                manager.disableCellBroadcast(
                        SmsCbConstants.MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY);
            }
            if (!enableCmasTestAlerts) {
                if (DBG) Log.d(TAG, "disabling cell broadcast CMAS test messages");
                manager.disableCellBroadcastRange(
                        SmsCbConstants.MESSAGE_ID_CMAS_ALERT_REQUIRED_MONTHLY_TEST,
                        SmsCbConstants.MESSAGE_ID_CMAS_ALERT_OPERATOR_DEFINED_USE);
            }
        } catch (Exception ex) {
            Log.e(TAG, "exception enabling cell broadcast channels", ex);
        }
    }

    private void configCdmaChannels() {
        try {
            SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
            Resources res = getResources();

            // Check for system property defining the emergency channel ranges to enable
            String emergencyIdRange = SystemProperties.get(EMERGENCY_BROADCAST_RANGE_CDMA);

            boolean enableEmergencyAlerts = prefs.getBoolean(
                    CellBroadcastSettings.KEY_ENABLE_EMERGENCY_ALERTS, true);

            boolean enableCmasExtremeAlerts = prefs.getBoolean(
                    CellBroadcastSettings.KEY_ENABLE_CMAS_EXTREME_THREAT_ALERTS, true);

            boolean enableCmasSevereAlerts = prefs.getBoolean(
                    CellBroadcastSettings.KEY_ENABLE_CMAS_SEVERE_THREAT_ALERTS, true);

            boolean enableCmasAmberAlerts = prefs.getBoolean(
                    CellBroadcastSettings.KEY_ENABLE_CMAS_AMBER_ALERTS, true);

            boolean enableCmasTestAlerts = prefs.getBoolean(
                    CellBroadcastSettings.KEY_ENABLE_CMAS_TEST_ALERTS, false);

            SmsManager manager = SmsManager.getDefault();
            if (enableEmergencyAlerts) {
                if (DBG) log("enabling emergency cdma broadcast channels");
                if (!TextUtils.isEmpty(emergencyIdRange)) {
                    setChannelRange(manager, emergencyIdRange, true, true);
                } else {
                    // No emergency channel system property, enable all emergency channels
                    // that have checkbox checked
                    if (enableCmasExtremeAlerts) {
                        manager.enableCdmaBroadcast(
                                SmsEnvelope.SERVICE_CATEGORY_CMAS_EXTREME_THREAT);
                    }
                    if (enableCmasSevereAlerts) {
                        manager.enableCdmaBroadcast(
                                SmsEnvelope.SERVICE_CATEGORY_CMAS_SEVERE_THREAT);
                    }
                    if (enableCmasAmberAlerts) {
                        manager.enableCdmaBroadcast(
                                SmsEnvelope.SERVICE_CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY);
                    }
                    if (enableCmasTestAlerts) {
                        manager.enableCdmaBroadcast(SmsEnvelope.SERVICE_CATEGORY_CMAS_TEST_MESSAGE);
                    }

                    // CMAS Presidential must be on.
                    manager.enableCdmaBroadcast(
                            SmsEnvelope.SERVICE_CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT);
                }
                if (DBG) log("enabled emergency cdma broadcast channels");
            } else {
                // we may have enabled these channels previously, so try to
                // disable them
                if (DBG) log("disabling emergency cdma broadcast channels");
                if (!TextUtils.isEmpty(emergencyIdRange)) {
                    setChannelRange(manager, emergencyIdRange, false, true);
                } else {
                    // No emergency channel system property, disable all emergency channels
                    manager.disableCdmaBroadcast(
                            SmsEnvelope.SERVICE_CATEGORY_CMAS_EXTREME_THREAT);
                    manager.disableCdmaBroadcast(
                            SmsEnvelope.SERVICE_CATEGORY_CMAS_SEVERE_THREAT);
                    manager.disableCdmaBroadcast(
                            SmsEnvelope.SERVICE_CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY);
                    manager.disableCdmaBroadcast(SmsEnvelope.SERVICE_CATEGORY_CMAS_TEST_MESSAGE);

                    // CMAS Presidential must be on.
                    manager.enableCdmaBroadcast(
                            SmsEnvelope.SERVICE_CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT);
                }
                if (DBG) log("disabled emergency cdma broadcast channels");
            }

            if (!enableCmasExtremeAlerts) {
                if (DBG) Log.d(TAG, "disabling cell broadcast CMAS extreme");
                manager.disableCdmaBroadcast(SmsEnvelope.SERVICE_CATEGORY_CMAS_EXTREME_THREAT);
            }
            if (enableCmasSevereAlerts) {
                if (DBG) Log.d(TAG, "disabling cell broadcast CMAS severe");
                manager.disableCdmaBroadcast(
                        SmsEnvelope.SERVICE_CATEGORY_CMAS_SEVERE_THREAT);
            }
            if (!enableCmasAmberAlerts) {
                if (DBG) Log.d(TAG, "disabling cell broadcast CMAS amber");
                manager.disableCdmaBroadcast(
                        SmsEnvelope.SERVICE_CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY);
            }
            if (!enableCmasTestAlerts) {
                if (DBG) Log.d(TAG, "disabling cell broadcast CMAS test");
                manager.disableCdmaBroadcast(SmsEnvelope.SERVICE_CATEGORY_CMAS_TEST_MESSAGE);
            }

        } catch (Exception ex) {
            Log.e(TAG, "exception enabling cdma broadcast channels", ex);
        }
    }

    private static void log(String msg) {
        Log.d(TAG, msg);
    }
}
