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
 *
 * #############################################################################
 * ######     SNAPDRAGON_SDK_FOR_ANDROID REQUIRED CLASS FOR CAMERA BURST CAPTURE
 * ######
 * ######     Remove file if not supporting Burst Capture via Camera.Parameters
 * ######     Dependencies:
 * ######           hardware/qcom/camera/QCameraHWI_Parm.cpp
 * ######           hardware/qcom/camera/QualcommHardwareCamera.cpp
 * #############################################################################
 *
 */
package com.qualcomm.camera;

import android.hardware.Camera;

import java.util.List;
import java.util.ArrayList;
import android.os.Bundle;

import com.qualcomm.snapdragon.util.QCCapabilitiesInterface;

/**
 * {@hide} Additional Qualcomm parameters
 *
 * @see Camera.Parameters
 */
public class QCParameters implements QCCapabilitiesInterface {
    public static final String KEY_ZSL_CAMERA_MODE = "camera-mode";
    public static final String KEY_ZSL_PREFERENCE_KEY = "pref_camera_zsl_key";
    public static final String KEY_NUM_SNAPS_PER_SHUTTER = "num-snaps-per-shutter";

    private static String KEY_FRAME_CAPTURE_KEYS = "key_frame_capture_keys";

    @Override
    public Bundle getCapabilities() {
        Bundle constantFieldBundle = new Bundle();
        ArrayList<String> cameraParametersList = new ArrayList<String>();
        cameraParametersList.add("KEY_ZSL_CAMERA_MODE");
        cameraParametersList.add("KEY_ZSL_PREFERENCE_KEY");
        cameraParametersList.add("KEY_NUM_SNAPS_PER_SHUTTER");

        constantFieldBundle.putStringArrayList(KEY_FRAME_CAPTURE_KEYS, cameraParametersList);

        Bundle capabilitiesBundle = new Bundle();
        capabilitiesBundle.putBundle(QCCapabilitiesInterface.KEY_CONSTANT_FIELD_VALUES,
                constantFieldBundle);

        return capabilitiesBundle;
    }
}
