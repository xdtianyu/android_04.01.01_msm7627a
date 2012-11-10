/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
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
 * ###############################################################################################
 * ######     SNAPDRAGON_SDK_FOR_ANDROID REQUIRED CLASS FOR SENSOR GESTURES
 * ######
 * ######     Remove file if not supporting sensor gestures via DSPS
 * ###############################################################################################
 */

package com.qualcomm.sensor;

import java.util.ArrayList;

import android.os.Bundle;

import com.qualcomm.snapdragon.util.QCCapabilitiesInterface;

public class QCSensor implements QCCapabilitiesInterface{

    private static String KEY_SENSOR_TYPES = "key_sensor_types";
    private static String KEY_EVENT_TYPES = "key_event_types";

    private static int QC_SENSOR_TYPE_BASE        = 33171000;

    // QC Sensor type values
    public static int SENSOR_TYPE_BASIC_GESTURES = QC_SENSOR_TYPE_BASE;
    public static int SENSOR_TYPE_TAP            = QC_SENSOR_TYPE_BASE + 1;
    public static int SENSOR_TYPE_FACING         = QC_SENSOR_TYPE_BASE + 2;
    public static int SENSOR_TYPE_TILT           = QC_SENSOR_TYPE_BASE + 3;

    // QC Gesture values returned

    // Basic Gestures
    public static int BASIC_GESTURE_PUSH_V01 = 1; /*  Phone is jerked away from the user, in the direction perpendicular to the screen   */
    public static int BASIC_GESTURE_PULL_V01 = 2; /*  Phone is jerked toward  from the user, in the direction perpendicular to the screen   */
    public static int BASIC_GESTURE_SHAKE_AXIS_LEFT_V01 = 3; /*  Phone is shaken toward the left   */
    public static int BASIC_GESTURE_SHAKE_AXIS_RIGHT_V01 = 4; /*  Phone is shaken toward the right   */
    public static int BASIC_GESTURE_SHAKE_AXIS_TOP_V01 = 5; /*  Phone is shaken toward the top   */
    public static int BASIC_GESTURE_SHAKE_AXIS_BOTTOM_V01 = 6; /*  Phone is shaken toward the bottom */

    // Tap Gestures
    public static int GYRO_TAP_LEFT_V01 = 1; /*  Phone is tapped on the left. # */
    public static int GYRO_TAP_RIGHT_V01 = 2; /*  Phone is tapped on the right.  */
    public static int GYRO_TAP_TOP_V01 = 3; /*  Phone is tapped on the top.  */
    public static int GYRO_TAP_BOTTOM_V01 = 4; /*  Phone is tapped on the bottom.  */

    // Facing Gestures
    public static int FACING_UP_V01 = 1; /*  Phone has just moved to a facing-up phone posture, which is defined as screen up   */
    public static int FACING_DOWN_V01 = 2; /*  Phone has just moved to a facing-down phone posture, which is defined as screen down */


    /**
     * Returns the Sensor capabilities of the hardware
     * @param None
     * @return a Bundle which looks like -
     * <p>
     * <pre class="language-java">
     * Bundle(KEY_CONSTANT_FIELD_VALUES,[Bundle{(KEY_SENSOR_TYPES, ArrayList<String>), (KEY_EVENT_TYPES, ArrayList<String>)}])
     *
     * KEY_CONSTANT_FIELD_VALUES => |KEY_SENSOR_TYPES=> | SENSOR_TYPE_BASIC_GESTURES,
     *                              |                   | SENSOR_TYPE_TAP,
     *                              |                   | ...
     *                              |------------------------------------------------
     *                              |KEY_EVENT_TYPES=>  | BASIC_GESTURE_PUSH_V01,
     *                              |                   | BASIC_GESTURE_PULL_V01,
     *                              |                   | ...
     * </pre>
     * </p>
     */

    @Override
    public Bundle getCapabilities(){

        Bundle constantFieldBundle = new Bundle();
        ArrayList<String> sensorTypesList = new ArrayList<String>();
        sensorTypesList.add("SENSOR_TYPE_BASIC_GESTURES");
        sensorTypesList.add("SENSOR_TYPE_TAP");
        sensorTypesList.add("SENSOR_TYPE_FACING");
        sensorTypesList.add("SENSOR_TYPE_TILT");

        constantFieldBundle.putStringArrayList(KEY_SENSOR_TYPES, sensorTypesList);

        ArrayList<String> eventTypesList = new ArrayList<String>();
        eventTypesList.add("BASIC_GESTURE_PUSH_V01");
        eventTypesList.add("BASIC_GESTURE_PULL_V01");
        eventTypesList.add("BASIC_GESTURE_SHAKE_AXIS_LEFT_V01");
        eventTypesList.add("BASIC_GESTURE_SHAKE_AXIS_RIGHT_V01");
        eventTypesList.add("BASIC_GESTURE_SHAKE_AXIS_TOP_V01");
        eventTypesList.add("BASIC_GESTURE_SHAKE_AXIS_BOTTOM_V01");
        eventTypesList.add("GYRO_TAP_LEFT_V01");
        eventTypesList.add("GYRO_TAP_RIGHT_V01");
        eventTypesList.add("GYRO_TAP_TOP_V01");
        eventTypesList.add("GYRO_TAP_BOTTOM_V01");
        eventTypesList.add("FACING_UP_V01");
        eventTypesList.add("FACING_DOWN_V01");

        constantFieldBundle.putStringArrayList(KEY_EVENT_TYPES, eventTypesList);

        Bundle capabilitiesBundle = new Bundle();
        capabilitiesBundle.putBundle(QCCapabilitiesInterface.KEY_CONSTANT_FIELD_VALUES, constantFieldBundle);

        return capabilitiesBundle;
    }
}
