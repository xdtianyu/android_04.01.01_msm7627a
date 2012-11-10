/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.camera;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences.Editor;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.SurfaceTexture;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Face;
import android.hardware.Camera.FaceDetectionListener;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.PictureCallback;
import android.hardware.Camera.Size;
import android.location.Location;
import android.media.CameraProfile;
import android.media.MediaActionSound;
import android.net.Uri;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.provider.MediaStore;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.OrientationEventListener;
import android.view.View;
import android.view.WindowManager;
import android.widget.ProgressBar;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import com.android.camera.ui.CameraPicker;
import com.android.camera.ui.FaceView;
import com.android.camera.ui.IndicatorControlContainer;
import com.android.camera.ui.PopupManager;
import com.android.camera.ui.Rotatable;
import com.android.camera.ui.RotateImageView;
import com.android.camera.ui.RotateLayout;
import com.android.camera.ui.RotateTextToast;
import android.os.SystemProperties;
import com.android.camera.ui.TwoStateImageView;
import com.android.camera.ui.ZoomControl;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Formatter;
import java.util.List;

import java.util.HashMap;
import android.util.AttributeSet;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;

import android.graphics.Paint.Align;
/** The Camera activity which can preview and take pictures. */
public class Camera extends ActivityBase implements FocusManager.Listener,
        ModePicker.OnModeChangeListener, FaceDetectionListener,
        CameraPreference.OnPreferenceChangedListener, LocationManager.Listener,
        PreviewFrameLayout.OnSizeChangedListener,
        ShutterButton.OnShutterButtonListener {

    private static final String TAG = "camera";

    final String[] OTHER_SETTING_KEYS = {
                CameraSettings.KEY_RECORD_LOCATION,
                CameraSettings.KEY_PICTURE_SIZE,
                CameraSettings.KEY_PICTURE_FORMAT,  //#TODO : Need to Decide
                CameraSettings.KEY_JPEG_QUALITY,
                CameraSettings.KEY_FOCUS_MODE,
                CameraSettings.KEY_ZSL,
                CameraSettings.KEY_COLOR_EFFECT
        };

    final String[] QCOM_SETTING_KEYS = {
                CameraSettings.KEY_FACE_DETECTION,
                CameraSettings.KEY_TOUCH_AF_AEC,
                CameraSettings.KEY_SELECTABLE_ZONE_AF,
                CameraSettings.KEY_SATURATION,
                CameraSettings.KEY_CONTRAST,
                CameraSettings.KEY_SHARPNESS,
                CameraSettings.KEY_AUTOEXPOSURE,
        };

    final String[] QCOM_SETTING_KEYS_1 = {
                CameraSettings.KEY_ANTIBANDING,
                CameraSettings.KEY_ISO,
                CameraSettings.KEY_LENSSHADING,
                CameraSettings.KEY_HISTOGRAM,
                CameraSettings.KEY_DENOISE,
                CameraSettings.KEY_REDEYE_REDUCTION,
                CameraSettings.KEY_AE_BRACKET_HDR
        };

    //QCom data members
    public static boolean mBrightnessVisible = true;
    public HashMap otherSettingKeys = new HashMap(3);
    private static final int MAX_SHARPNESS_LEVEL = 6;
    private boolean mRestartPreview = false;
    private int mSnapshotMode;
    private int mBurstSnapNum = 1;
    private int mReceivedSnapNum = 0;
    public boolean mFaceDetectionEnabled = false;

    /*Histogram variables*/
    private GraphView mGraphView;
    private static final int STATS_DATA = 257;
    public static int statsdata[] = new int[STATS_DATA];
    public static boolean mHiston = false;

    //End Of Qcom data Members

    // We number the request code from 1000 to avoid collision with Gallery.
    private static final int REQUEST_CROP = 1000;

    private static final int FIRST_TIME_INIT = 2;
    private static final int CLEAR_SCREEN_DELAY = 3;
    private static final int SET_CAMERA_PARAMETERS_WHEN_IDLE = 4;
    private static final int CHECK_DISPLAY_ROTATION = 5;
    private static final int SHOW_TAP_TO_FOCUS_TOAST = 6;
    private static final int UPDATE_THUMBNAIL = 7;
    private static final int SWITCH_CAMERA = 8;
    private static final int SWITCH_CAMERA_START_ANIMATION = 9;
    private static final int CAMERA_OPEN_DONE = 10;
    private static final int START_PREVIEW_DONE = 11;
    private static final int OPEN_CAMERA_FAIL = 12;
    private static final int CAMERA_DISABLED = 13;
    private static final int SET_SKIN_TONE_FACTOR = 14;

    // The subset of parameters we need to update in setCameraParameters().
    private static final int UPDATE_PARAM_INITIALIZE = 1;
    private static final int UPDATE_PARAM_ZOOM = 2;
    private static final int UPDATE_PARAM_PREFERENCE = 4;
    private static final int UPDATE_PARAM_ALL = -1;

    //  When setCameraParametersWhenIdle() is called, we accumulate the subsets
    // needed to be updated in mUpdateSet.
    private int mUpdateSet;

    private static final int SCREEN_DELAY = 2 * 60 * 1000;

    private int mZoomValue;  // The current zoom value.
    private int mZoomMax;
    private ZoomControl mZoomControl;

    private Parameters mInitialParams;
    private boolean mFocusAreaSupported;
    private boolean mMeteringAreaSupported;
    private boolean mAeLockSupported;
    private boolean mAwbLockSupported;
    private boolean mContinousFocusSupported;
    private boolean mTouchAfAecFlag;

    private MyOrientationEventListener mOrientationListener;
    // The degrees of the device rotated clockwise from its natural orientation.
    private int mOrientation = OrientationEventListener.ORIENTATION_UNKNOWN;
    // The orientation compensation for icons and thumbnails. Ex: if the value
    // is 90, the UI components should be rotated 90 degrees counter-clockwise.
    private int mOrientationCompensation = 0;
    private ComboPreferences mPreferences;

    private static final String sTempCropFilename = "crop-temp";

    private ContentProviderClient mMediaProviderClient;
    private ShutterButton mShutterButton;
    private boolean mFaceDetectionStarted = false;

    private PreviewFrameLayout mPreviewFrameLayout;
    private SurfaceTexture mSurfaceTexture;
    private RotateDialogController mRotateDialog;

    private static final int MINIMUM_BRIGHTNESS = 0;
    private static final int MAXIMUM_BRIGHTNESS = 6;
    private int mbrightness = 3;
    private int mbrightness_step = 1;
    private ProgressBar brightnessProgressBar;
    private ModePicker mModePicker;
    private FaceView mFaceView;
    private RotateLayout mFocusAreaIndicator;
    private Rotatable mReviewCancelButton;
    private Rotatable mReviewDoneButton;
    private View mReviewRetakeButton;
    // Constant from android.hardware.Camera.Parameters
    private static final String KEY_PICTURE_FORMAT = "picture-format";
    private static final String PIXEL_FORMAT_JPEG = "jpeg";
    private static final String PIXEL_FORMAT_RAW = "raw";

    // mCropValue and mSaveUri are used only if isImageCaptureIntent() is true.
    private String mCropValue;
    private Uri mSaveUri;

    // Small indicators which show the camera settings in the viewfinder.
    private TextView mExposureIndicator;
    private ImageView mGpsIndicator;
    private ImageView mFlashIndicator;
    private ImageView mSceneIndicator;
    private ImageView mWhiteBalanceIndicator;
    private ImageView mFocusIndicator;
    // A view group that contains all the small indicators.
    private Rotatable mOnScreenIndicators;

    // We use a thread in ImageSaver to do the work of saving images and
    // generating thumbnails. This reduces the shot-to-shot time.
    private ImageSaver mImageSaver;
    // Similarly, we use a thread to generate the name of the picture and insert
    // it into MediaStore while picture taking is still in progress.
    private ImageNamer mImageNamer;

    private MediaActionSound mCameraSound;

    private Runnable mDoSnapRunnable = new Runnable() {
        @Override
        public void run() {
            onShutterButtonClick();
        }
    };

    private final StringBuilder mBuilder = new StringBuilder();
    private final Formatter mFormatter = new Formatter(mBuilder);
    private final Object[] mFormatterArgs = new Object[1];

    /**
     * An unpublished intent flag requesting to return as soon as capturing
     * is completed.
     *
     * TODO: consider publishing by moving into MediaStore.
     */
    private static final String EXTRA_QUICK_CAPTURE =
            "android.intent.extra.quickCapture";

    // The display rotation in degrees. This is only valid when mCameraState is
    // not PREVIEW_STOPPED.
    private int mDisplayRotation;
    // The value for android.hardware.Camera.setDisplayOrientation.
    private int mCameraDisplayOrientation;
    // The value for UI components like indicators.
    private int mDisplayOrientation;
    // The value for android.hardware.Camera.Parameters.setRotation.
    private int mJpegRotation;
    private boolean mFirstTimeInitialized;
    private boolean mIsImageCaptureIntent;

    private static final int PREVIEW_STOPPED = 0;
    private static final int IDLE = 1;  // preview is active
    // Focus is in progress. The exact focus state is in Focus.java.
    private static final int FOCUSING = 2;
    private static final int SNAPSHOT_IN_PROGRESS = 3;
    // Switching between cameras.
    private static final int SWITCHING_CAMERA = 4;
    private int mCameraState = PREVIEW_STOPPED;
    private boolean mSnapshotOnIdle = false;

    private ContentResolver mContentResolver;
    private boolean mDidRegister = false;

    private LocationManager mLocationManager;

    private final ShutterCallback mShutterCallback = new ShutterCallback();
    private final PostViewPictureCallback mPostViewPictureCallback =
            new PostViewPictureCallback();
    private final RawPictureCallback mRawPictureCallback =
            new RawPictureCallback();
    private final AutoFocusCallback mAutoFocusCallback =
            new AutoFocusCallback();
    private final AutoFocusMoveCallback mAutoFocusMoveCallback =
            new AutoFocusMoveCallback();
    private final CameraErrorCallback mErrorCallback = new CameraErrorCallback();
    private final StatsCallback mStatsCallback = new StatsCallback();
    private long mFocusStartTime;
    private long mShutterCallbackTime;
    private long mPostViewPictureCallbackTime;
    private long mRawPictureCallbackTime;
    private long mJpegPictureCallbackTime;
    private long mOnResumeTime;
    private long mStorageSpace;
    private byte[] mJpegImageData;

    // These latency time are for the CameraLatency test.
    public long mAutoFocusTime;
    public long mShutterLag;
    public long mShutterToPictureDisplayedTime;
    public long mPictureDisplayedToJpegCallbackTime;
    public long mJpegCallbackFinishTime;
    public long mCaptureStartTime;

    // This handles everything about focus.
    private FocusManager mFocusManager;
    private String mSceneMode;
    private Toast mNotSelectableToast;

    private final Handler mHandler = new MainHandler();
    private IndicatorControlContainer mIndicatorControlContainer;
    private PreferenceGroup mPreferenceGroup;

    private boolean mQuickCapture;

    private static final int MIN_SCE_FACTOR = -10;
    private static final int MAX_SCE_FACTOR = +10;
    private int SCE_FACTOR_STEP = 10;
    private int mskinToneValue = 0;
    private boolean mSkinToneSeekBar= false;
    private boolean mSeekBarInitialized = false;
    private SeekBar skinToneSeekBar;
    private TextView LeftValue;
    private TextView RightValue;
    private TextView Title;
	
    CameraStartUpThread mCameraStartUpThread;
    ConditionVariable mStartPreviewPrerequisiteReady = new ConditionVariable();

    // The purpose is not to block the main thread in onCreate and onResume.
    private class CameraStartUpThread extends Thread {
        private volatile boolean mCancelled;

        public void cancel() {
            mCancelled = true;
        }

        @Override
        public void run() {
            try {
                // We need to check whether the activity is paused before long
                // operations to ensure that onPause() can be done ASAP.
                if (mCancelled) return;
                mCameraDevice = Util.openCamera(Camera.this, mCameraId);
                mParameters = mCameraDevice.getParameters();
                // Wait until all the initialization needed by startPreview are
                // done.
                mStartPreviewPrerequisiteReady.block();

                initializeCapabilities();
                if (mFocusManager == null) initializeFocusManager();
                if (mCancelled) return;
                setCameraParameters(UPDATE_PARAM_ALL);
                mHandler.sendEmptyMessage(CAMERA_OPEN_DONE);
                if (mCancelled) return;
                startPreview();
                mHandler.sendEmptyMessage(START_PREVIEW_DONE);
                mOnResumeTime = SystemClock.uptimeMillis();
                mHandler.sendEmptyMessage(CHECK_DISPLAY_ROTATION);
            } catch (CameraHardwareException e) {
                mHandler.sendEmptyMessage(OPEN_CAMERA_FAIL);
            } catch (CameraDisabledException e) {
                mHandler.sendEmptyMessage(CAMERA_DISABLED);
            }
        }
    }

    /**
     * This Handler is used to post message back onto the main thread of the
     * application
     */
    private class MainHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case CLEAR_SCREEN_DELAY: {
                    getWindow().clearFlags(
                            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    break;
                }

                case FIRST_TIME_INIT: {
                    initializeFirstTime();
                    break;
                }

                case SET_CAMERA_PARAMETERS_WHEN_IDLE: {
                    setCameraParametersWhenIdle(0);
                    break;
                }

                case CHECK_DISPLAY_ROTATION: {
                    // Set the display orientation if display rotation has changed.
                    // Sometimes this happens when the device is held upside
                    // down and camera app is opened. Rotation animation will
                    // take some time and the rotation value we have got may be
                    // wrong. Framework does not have a callback for this now.
                    if (Util.getDisplayRotation(Camera.this) != mDisplayRotation) {
                        setDisplayOrientation();
                    }
                    if (SystemClock.uptimeMillis() - mOnResumeTime < 5000) {
                        mHandler.sendEmptyMessageDelayed(CHECK_DISPLAY_ROTATION, 100);
                    }
                    break;
                }

                case SHOW_TAP_TO_FOCUS_TOAST: {
                    showTapToFocusToast();
                    break;
                }

                case UPDATE_THUMBNAIL: {
                    mImageSaver.updateThumbnail();
                    break;
                }

                case SWITCH_CAMERA: {
                    switchCamera();
                    break;
                }

                case SWITCH_CAMERA_START_ANIMATION: {
                    mCameraScreenNail.animateSwitchCamera();
                    break;
                }

                case CAMERA_OPEN_DONE: {
                    initializeAfterCameraOpen();
                    break;
                }

                case START_PREVIEW_DONE: {
                    mCameraStartUpThread = null;
                    setCameraState(IDLE);
                    startFaceDetection();
                    break;
                }

                case OPEN_CAMERA_FAIL: {
                    mCameraStartUpThread = null;
                    mOpenCameraFail = true;
                    Util.showErrorAndFinish(Camera.this,
                            R.string.cannot_connect_camera);
                    break;
                }

                case CAMERA_DISABLED: {
                    mCameraStartUpThread = null;
                    mCameraDisabled = true;
                    Util.showErrorAndFinish(Camera.this,
                            R.string.camera_disabled);
                    break;
                }
				case SET_SKIN_TONE_FACTOR: {
                    Log.e(TAG, "yyan set tone bar: mSceneMode = " + mSceneMode);
                    setSkinToneFactor();
                    mSeekBarInitialized = true;
                    // skin tone ie enabled only for auto,party and portrait BSM
                    // when color effects are not enabled
                    String colorEffect = mPreferences.getString(
                        CameraSettings.KEY_COLOR_EFFECT,
                        getString(R.string.pref_camera_coloreffect_default));
                    if((Parameters.SCENE_MODE_PARTY.equals(mSceneMode) ||
                        Parameters.SCENE_MODE_PORTRAIT.equals(mSceneMode))&&
                        (Parameters.EFFECT_NONE.equals(colorEffect))) {
                        ;
                    }
                    else{
                        Log.e(TAG, "yyan Skin tone bar: disable");
                        disableSkinToneSeekBar();
                    }
                    break;
                }
            }
        }
    }

    private void initializeAfterCameraOpen() {
        // These depend on camera parameters.
        setPreviewFrameLayoutAspectRatio();
        mFocusManager.setPreviewSize(mPreviewFrameLayout.getWidth(),
                mPreviewFrameLayout.getHeight());
        if (mIndicatorControlContainer == null) {
            initializeIndicatorControl();
        }
        // This should be enabled after preview is started.
        mIndicatorControlContainer.setEnabled(false);
        initializeZoom();
        updateOnScreenIndicators();
        showTapToFocusToastIfNeeded();
    }

    private void resetExposureCompensation() {
        String value = mPreferences.getString(CameraSettings.KEY_EXPOSURE,
                CameraSettings.EXPOSURE_DEFAULT_VALUE);
        if (!CameraSettings.EXPOSURE_DEFAULT_VALUE.equals(value)) {
            Editor editor = mPreferences.edit();
            editor.putString(CameraSettings.KEY_EXPOSURE, "0");
            editor.apply();
        }
    }

    private void keepMediaProviderInstance() {
        // We want to keep a reference to MediaProvider in camera's lifecycle.
        // TODO: Utilize mMediaProviderClient instance to replace
        // ContentResolver calls.
        if (mMediaProviderClient == null) {
            mMediaProviderClient = mContentResolver
                    .acquireContentProviderClient(MediaStore.AUTHORITY);
        }
    }

    // Snapshots can only be taken after this is called. It should be called
    // once only. We could have done these things in onCreate() but we want to
    // make preview screen appear as soon as possible.
    private void initializeFirstTime() {
        if (mFirstTimeInitialized) return;

        // Create orientation listener. This should be done first because it
        // takes some time to get first orientation.
        mOrientationListener = new MyOrientationEventListener(Camera.this);
        mOrientationListener.enable();

        // Initialize location service.
        boolean recordLocation = RecordLocationPreference.get(
                mPreferences, mContentResolver);
        mLocationManager.recordLocation(recordLocation);

        keepMediaProviderInstance();
        checkStorage();

        // Initialize shutter button.
        mShutterButton = (ShutterButton) findViewById(R.id.shutter_button);
        mShutterButton.setOnShutterButtonListener(this);
        mShutterButton.setVisibility(View.VISIBLE);

        mImageSaver = new ImageSaver();
        mImageNamer = new ImageNamer();
        installIntentFilter();

        mGraphView = (GraphView)findViewById(R.id.graph_view);
        if(mGraphView == null)
            Log.e(TAG, "mGraphView is null");
        mGraphView.setCameraObject(Camera.this);

        mFirstTimeInitialized = true;
        addIdleHandler();
    }

    private void showTapToFocusToastIfNeeded() {
        // Show the tap to focus toast if this is the first start.
        if (mFocusAreaSupported &&
                mPreferences.getBoolean(CameraSettings.KEY_CAMERA_FIRST_USE_HINT_SHOWN, true)) {
            // Delay the toast for one second to wait for orientation.
            mHandler.sendEmptyMessageDelayed(SHOW_TAP_TO_FOCUS_TOAST, 1000);
        }
    }

    private void addIdleHandler() {
        MessageQueue queue = Looper.myQueue();
        queue.addIdleHandler(new MessageQueue.IdleHandler() {
            @Override
            public boolean queueIdle() {
                Storage.ensureOSXCompatible();
                return false;
            }
        });
    }

    // If the activity is paused and resumed, this method will be called in
    // onResume.
    private void initializeSecondTime() {
        // Start orientation listener as soon as possible because it takes
        // some time to get first orientation.
        mOrientationListener.enable();

        // Start location update if needed.
        boolean recordLocation = RecordLocationPreference.get(
                mPreferences, mContentResolver);
        mLocationManager.recordLocation(recordLocation);

        installIntentFilter();
        mImageSaver = new ImageSaver();
        mImageNamer = new ImageNamer();
        initializeZoom();
        keepMediaProviderInstance();
        checkStorage();
        hidePostCaptureAlert();

        if(mGraphView != null)
          mGraphView.setCameraObject(Camera.this);

        if (!mIsImageCaptureIntent) {
            mModePicker.setCurrentMode(ModePicker.MODE_CAMERA);
        }
        if (mIndicatorControlContainer != null) {
            mIndicatorControlContainer.reloadPreferences();
        }
    }

    private class ZoomChangeListener implements ZoomControl.OnZoomChangedListener {
        @Override
        public void onZoomValueChanged(int index) {
            // Not useful to change zoom value when the activity is paused.
            if (mPaused) return;
            mZoomValue = index;

            // Set zoom parameters asynchronously
            mParameters.setZoom(mZoomValue);
            mCameraDevice.setParametersAsync(mParameters);
        }
    }

    private void initializeZoom() {
        if (!mParameters.isZoomSupported()) return;
        mZoomMax = mParameters.getMaxZoom();
        // Currently we use immediate zoom for fast zooming to get better UX and
        // there is no plan to take advantage of the smooth zoom.
        mZoomControl.setZoomMax(mZoomMax);
        mZoomControl.setZoomIndex(mParameters.getZoom());
        mZoomControl.setOnZoomChangeListener(new ZoomChangeListener());
    }

    @Override
    public void startFaceDetection() {
        if (mFaceDetectionEnabled == false
                || mFaceDetectionStarted || mCameraState != IDLE) return;
        if (mParameters.getMaxNumDetectedFaces() > 0) {
            mFaceDetectionStarted = true;
            mFaceView.clear();
            mFaceView.setVisibility(View.VISIBLE);
            mFaceView.setDisplayOrientation(mDisplayOrientation);
            CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
            mFaceView.setMirror(info.facing == CameraInfo.CAMERA_FACING_FRONT);
            mFaceView.resume();
            mFocusManager.setFaceView(mFaceView);
            mCameraDevice.setFaceDetectionListener(this);
            mCameraDevice.startFaceDetection();
        }
    }

    @Override
    public void stopFaceDetection() {
        if (mFaceDetectionEnabled == false || !mFaceDetectionStarted)
            return;
        if (mParameters.getMaxNumDetectedFaces() > 0) {
            mFaceDetectionStarted = false;
            mCameraDevice.setFaceDetectionListener(null);
            mCameraDevice.stopFaceDetection();
            if (mFaceView != null) mFaceView.clear();
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent m) {
        if (mCameraState == SWITCHING_CAMERA) return true;

        // Check if the popup window should be dismissed first.
        if (m.getAction() == MotionEvent.ACTION_DOWN) {
            float x = m.getX();
            float y = m.getY();
            // Dismiss the mode selection window if the ACTION_DOWN event is out
            // of its view area.
            if ((mModePicker != null) && !Util.pointInView(x, y, mModePicker)) {
                mModePicker.dismissModeSelection();
            }
            // Check if the popup window is visible. Indicator control can be
            // null if camera is not opened yet.
            if (mIndicatorControlContainer != null) {
                View popup = mIndicatorControlContainer.getActiveSettingPopup();
                if (popup != null) {
                    // Let popup window, indicator control or preview frame
                    // handle the event by themselves. Dismiss the popup window
                    // if users touch on other areas.
                    if (!Util.pointInView(x, y, popup)
                            && !Util.pointInView(x, y, mIndicatorControlContainer)
                            && !Util.pointInView(x, y, mPreviewFrameLayout)) {
                        mIndicatorControlContainer.dismissSettingPopup();
                    }
                }
            }
        }

        return super.dispatchTouchEvent(m);
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            Log.d(TAG, "Received intent action=" + action);
            if (action.equals(Intent.ACTION_MEDIA_MOUNTED)
                    || action.equals(Intent.ACTION_MEDIA_UNMOUNTED)
                    || action.equals(Intent.ACTION_MEDIA_CHECKING)) {
                checkStorage();
            } else if (action.equals(Intent.ACTION_MEDIA_SCANNER_FINISHED)) {
                checkStorage();
                if (!mIsImageCaptureIntent) {
                    getLastThumbnail();
                }
            }
        }
    };

    private void initOnScreenIndicator() {
        mGpsIndicator = (ImageView) findViewById(R.id.onscreen_gps_indicator);
        mExposureIndicator = (TextView) findViewById(R.id.onscreen_exposure_indicator);
        mFlashIndicator = (ImageView) findViewById(R.id.onscreen_flash_indicator);
        mSceneIndicator = (ImageView) findViewById(R.id.onscreen_scene_indicator);
        mWhiteBalanceIndicator =
                (ImageView) findViewById(R.id.onscreen_white_balance_indicator);
        mFocusIndicator = (ImageView) findViewById(R.id.onscreen_focus_indicator);
    }

    @Override
    public void showGpsOnScreenIndicator(boolean hasSignal) {
        if (mGpsIndicator == null) {
            return;
        }
        if (hasSignal) {
            mGpsIndicator.setImageResource(R.drawable.ic_viewfinder_gps_on);
        } else {
            mGpsIndicator.setImageResource(R.drawable.ic_viewfinder_gps_no_signal);
        }
        mGpsIndicator.setVisibility(View.VISIBLE);
    }

    @Override
    public void hideGpsOnScreenIndicator() {
        if (mGpsIndicator == null) {
            return;
        }
        mGpsIndicator.setVisibility(View.GONE);
    }

    private void updateExposureOnScreenIndicator(int value) {
        if (mExposureIndicator == null) {
            return;
        }
        if (value == 0) {
            mExposureIndicator.setText("");
            mExposureIndicator.setVisibility(View.GONE);
        } else {
            float step = mParameters.getExposureCompensationStep();
            mFormatterArgs[0] = value * step;
            mBuilder.delete(0, mBuilder.length());
            mFormatter.format("%+1.1f", mFormatterArgs);
            String exposure = mFormatter.toString();
            mExposureIndicator.setText(exposure);
            mExposureIndicator.setVisibility(View.VISIBLE);
        }
    }

    private void updateFlashOnScreenIndicator(String value) {
        if (mFlashIndicator == null) {
            return;
        }
        if (value == null || Parameters.FLASH_MODE_OFF.equals(value)) {
            mFlashIndicator.setVisibility(View.GONE);
        } else {
            mFlashIndicator.setVisibility(View.VISIBLE);
            if (Parameters.FLASH_MODE_AUTO.equals(value)) {
                mFlashIndicator.setImageResource(R.drawable.ic_indicators_landscape_flash_auto);
            } else if (Parameters.FLASH_MODE_ON.equals(value)) {
                mFlashIndicator.setImageResource(R.drawable.ic_indicators_landscape_flash_on);
            } else {
                // Should not happen.
                mFlashIndicator.setVisibility(View.GONE);
            }
        }
    }

    private void updateSceneOnScreenIndicator(String value) {
        if (mSceneIndicator == null) {
            return;
        }
        boolean isGone = (value == null) || (Parameters.SCENE_MODE_AUTO.equals(value));
        mSceneIndicator.setVisibility(isGone ? View.GONE : View.VISIBLE);
    }

    private void updateWhiteBalanceOnScreenIndicator(String value) {
        if (mWhiteBalanceIndicator == null) {
            return;
        }
        if (value == null || Parameters.WHITE_BALANCE_AUTO.equals(value)) {
            mWhiteBalanceIndicator.setVisibility(View.GONE);
        } else {
            mWhiteBalanceIndicator.setVisibility(View.VISIBLE);
            if (Parameters.WHITE_BALANCE_FLUORESCENT.equals(value)) {
                mWhiteBalanceIndicator.setImageResource(R.drawable.ic_indicators_fluorescent);
            } else if (Parameters.WHITE_BALANCE_INCANDESCENT.equals(value)) {
                mWhiteBalanceIndicator.setImageResource(R.drawable.ic_indicators_incandescent);
            } else if (Parameters.WHITE_BALANCE_DAYLIGHT.equals(value)) {
                mWhiteBalanceIndicator.setImageResource(R.drawable.ic_indicators_sunlight);
            } else if (Parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT.equals(value)) {
                mWhiteBalanceIndicator.setImageResource(R.drawable.ic_indicators_cloudy);
            } else {
                // Should not happen.
                mWhiteBalanceIndicator.setVisibility(View.GONE);
            }
        }
    }

    private void updateFocusOnScreenIndicator(String value) {
        if (mFocusIndicator == null) {
            return;
        }
        // Do not show the indicator if users cannot choose.
        if (mPreferenceGroup.findPreference(CameraSettings.KEY_FOCUS_MODE) == null) {
            mFocusIndicator.setVisibility(View.GONE);
        } else {
            mFocusIndicator.setVisibility(View.VISIBLE);
            if (Parameters.FOCUS_MODE_INFINITY.equals(value)) {
                mFocusIndicator.setImageResource(R.drawable.ic_indicators_landscape);
            } else if (Parameters.FOCUS_MODE_MACRO.equals(value)) {
                mFocusIndicator.setImageResource(R.drawable.ic_indicators_macro);
            } else {
                // Should not happen.
                mFocusIndicator.setVisibility(View.GONE);
            }
        }
    }

    private void updateOnScreenIndicators() {
        updateSceneOnScreenIndicator(mParameters.getSceneMode());
        updateExposureOnScreenIndicator(CameraSettings.readExposure(mPreferences));
        updateFlashOnScreenIndicator(mParameters.getFlashMode());
        updateWhiteBalanceOnScreenIndicator(mParameters.getWhiteBalance());
        updateFocusOnScreenIndicator(mParameters.getFocusMode());
    }

    private final class ShutterCallback
            implements android.hardware.Camera.ShutterCallback {
        @Override
        public void onShutter() {
            mShutterCallbackTime = System.currentTimeMillis();
            mShutterLag = mShutterCallbackTime - mCaptureStartTime;
            Log.v(TAG, "mShutterLag = " + mShutterLag + "ms");
        }
    }

    private final class StatsCallback
           implements android.hardware.Camera.CameraDataCallback {
	    @Override
        public void onCameraData(int [] data, android.hardware.Camera camera) {
            //if(!mPreviewing || !mHiston || !mFirstTimeInitialized){
            if(!mHiston || !mFirstTimeInitialized){
                return;
            }
            /*The first element in the array stores max hist value . Stats data begin from second value*/
            synchronized(statsdata) {
                System.arraycopy(data,0,statsdata,0,STATS_DATA);
            }
            runOnUiThread(new Runnable() {
                public void run() {
                    if(mGraphView != null)
                        mGraphView.PreviewChanged();
                }
           });
        }
    }

    private final class PostViewPictureCallback implements PictureCallback {
        @Override
        public void onPictureTaken(
                byte [] data, android.hardware.Camera camera) {
            mPostViewPictureCallbackTime = System.currentTimeMillis();
            Log.v(TAG, "mShutterToPostViewCallbackTime = "
                    + (mPostViewPictureCallbackTime - mShutterCallbackTime)
                    + "ms");
        }
    }

    private final class RawPictureCallback implements PictureCallback {
        @Override
        public void onPictureTaken(
                byte [] rawData, android.hardware.Camera camera) {
            mRawPictureCallbackTime = System.currentTimeMillis();
            Log.v(TAG, "mShutterToRawCallbackTime = "
                    + (mRawPictureCallbackTime - mShutterCallbackTime) + "ms");
        }
    }

    private final class JpegPictureCallback implements PictureCallback {
        Location mLocation;

        public JpegPictureCallback(Location loc) {
            mLocation = loc;
        }

        @Override
        public void onPictureTaken(
                final byte [] jpegData, final android.hardware.Camera camera) {
            if (mPaused) {
                return;
            }

            mReceivedSnapNum = mReceivedSnapNum + 1;
            mJpegPictureCallbackTime = System.currentTimeMillis();
            // If postview callback has arrived, the captured image is displayed
            // in postview callback. If not, the captured image is displayed in
            // raw picture callback.
            if(mSnapshotMode == CameraInfo.CAMERA_SUPPORT_MODE_ZSL) {
                Log.v(TAG, "In onPictureTaken , in zslmode");
                mParameters = mCameraDevice.getParameters();
                mBurstSnapNum = mParameters.getInt("num-snaps-per-shutter");
            }
            Log.v(TAG, "In onPictureTaken JpegPictureCallback, mReceivedSnapNum = " + mReceivedSnapNum);
            Log.v(TAG, "In onPictureTaken JpegPictureCallback, mBurstSnapNum = " + mBurstSnapNum);
            if (mPostViewPictureCallbackTime != 0) {
                mShutterToPictureDisplayedTime =
                        mPostViewPictureCallbackTime - mShutterCallbackTime;
                mPictureDisplayedToJpegCallbackTime =
                        mJpegPictureCallbackTime - mPostViewPictureCallbackTime;
            } else {
                mShutterToPictureDisplayedTime =
                        mRawPictureCallbackTime - mShutterCallbackTime;
                mPictureDisplayedToJpegCallbackTime =
                        mJpegPictureCallbackTime - mRawPictureCallbackTime;
            }
            Log.v(TAG, "mPictureDisplayedToJpegCallbackTime = "
                    + mPictureDisplayedToJpegCallbackTime + "ms");

            mFocusManager.updateFocusUI(); // Ensure focus indicator is hidden.
            if (!mIsImageCaptureIntent && mSnapshotMode != CameraInfo.CAMERA_SUPPORT_MODE_ZSL
                && mReceivedSnapNum == mBurstSnapNum) {
                startPreview();
                setCameraState(IDLE);
                startFaceDetection();
            }else if(mReceivedSnapNum == mBurstSnapNum){
                setCameraState(IDLE);
            }

            if (!mIsImageCaptureIntent) {
                // Calculate the width and the height of the jpeg.
                Size s = mParameters.getPictureSize();
                int orientation = Exif.getOrientation(jpegData);
                int width, height;
                if ((mJpegRotation + orientation) % 180 == 0) {
                    width = s.width;
                    height = s.height;
                } else {
                    width = s.height;
                    height = s.width;
                }
                if(mReceivedSnapNum > 1){
                    mImageNamer.generateUri(); //added to solve zsl
                }
                Uri uri = mImageNamer.getUri();
                String title = mImageNamer.getTitle();
                mImageSaver.addImage(jpegData, uri, title, mLocation,
                        width, height, mThumbnailViewWidth, orientation);
            } else {
                mJpegImageData = jpegData;
                if (!mQuickCapture) {
                    showPostCaptureAlert();
                } else {
                    doAttach();
                }
            }

            // Check this in advance of each shot so we don't add to shutter
            // latency. It's true that someone else could write to the SD card in
            // the mean time and fill it, but that could have happened between the
            // shutter press and saving the JPEG too.
            checkStorage();

            long now = System.currentTimeMillis();
            mJpegCallbackFinishTime = now - mJpegPictureCallbackTime;
            Log.v(TAG, "mJpegCallbackFinishTime = "
                    + mJpegCallbackFinishTime + "ms");
            if (mReceivedSnapNum == mBurstSnapNum) {
                mJpegPictureCallbackTime = 0;
            }
        }
    }

    private OnSeekBarChangeListener mSeekListener = new OnSeekBarChangeListener() {
        public void onStartTrackingTouch(SeekBar bar) {
        // no support
        }
        public void onProgressChanged(SeekBar bar, int progress, boolean fromtouch) {
        }
        public void onStopTrackingTouch(SeekBar bar) {
        }
    };

    private OnSeekBarChangeListener mskinToneSeekListener = new OnSeekBarChangeListener() {
        public void onStartTrackingTouch(SeekBar bar) {
        // no support
        }

        public void onProgressChanged(SeekBar bar, int progress, boolean fromtouch) {
            int value = (progress + MIN_SCE_FACTOR) * SCE_FACTOR_STEP;
            if(progress > (MAX_SCE_FACTOR - MIN_SCE_FACTOR)/2){
                RightValue.setText(String.valueOf(value));
                LeftValue.setText("");
            } else if (progress < (MAX_SCE_FACTOR - MIN_SCE_FACTOR)/2){
                LeftValue.setText(String.valueOf(value));
                RightValue.setText("");
            } else {
                LeftValue.setText("");
                RightValue.setText("");
            }
            if(value != mskinToneValue && mCameraDevice != null) {
                mskinToneValue = value;
                mParameters = mCameraDevice.getParameters();
                mParameters.set("skinToneEnhancement", String.valueOf(mskinToneValue));
                mCameraDevice.setParameters(mParameters);
            }
        }

        public void onStopTrackingTouch(SeekBar bar) {
            Log.e(TAG, "Set onStopTrackingTouch mskinToneValue = " + mskinToneValue);
            Editor editor = mPreferences.edit();
            editor.putString(CameraSettings.KEY_SKIN_TONE_ENHANCEMENT_FACTOR,
                             Integer.toString(mskinToneValue));
            editor.apply();
        }
    };
    private final class AutoFocusCallback
            implements android.hardware.Camera.AutoFocusCallback {
        @Override
        public void onAutoFocus(
                boolean focused, android.hardware.Camera camera) {
            if (mPaused) return;

            mAutoFocusTime = System.currentTimeMillis() - mFocusStartTime;
            Log.v(TAG, "mAutoFocusTime = " + mAutoFocusTime + "ms");
            setCameraState(IDLE);
            mFocusManager.onAutoFocus(focused);
        }
    }

    private final class AutoFocusMoveCallback
            implements android.hardware.Camera.AutoFocusMoveCallback {
        @Override
        public void onAutoFocusMoving(
            boolean moving, android.hardware.Camera camera) {
                mFocusManager.onAutoFocusMoving(moving);
        }
    }

    // Each SaveRequest remembers the data needed to save an image.
    private static class SaveRequest {
        byte[] data;
        Uri uri;
        String title;
        Location loc;
        int width, height;
        int thumbnailWidth;
        int orientation;
    }

    // We use a queue to store the SaveRequests that have not been completed
    // yet. The main thread puts the request into the queue. The saver thread
    // gets it from the queue, does the work, and removes it from the queue.
    //
    // The main thread needs to wait for the saver thread to finish all the work
    // in the queue, when the activity's onPause() is called, we need to finish
    // all the work, so other programs (like Gallery) can see all the images.
    //
    // If the queue becomes too long, adding a new request will block the main
    // thread until the queue length drops below the threshold (QUEUE_LIMIT).
    // If we don't do this, we may face several problems: (1) We may OOM
    // because we are holding all the jpeg data in memory. (2) We may ANR
    // when we need to wait for saver thread finishing all the work (in
    // onPause() or gotoGallery()) because the time to finishing a long queue
    // of work may be too long.
    private class ImageSaver extends Thread {
        private static final int QUEUE_LIMIT = 3;

        private ArrayList<SaveRequest> mQueue;
        private Thumbnail mPendingThumbnail;
        private Object mUpdateThumbnailLock = new Object();
        private boolean mStop;

        // Runs in main thread
        public ImageSaver() {
            mQueue = new ArrayList<SaveRequest>();
            start();
        }

        // Runs in main thread
        public void addImage(final byte[] data, Uri uri, String title,
                Location loc, int width, int height, int thumbnailWidth,
                int orientation) {
            SaveRequest r = new SaveRequest();
            r.data = data;
            r.uri = uri;
            r.title = title;
            r.loc = (loc == null) ? null : new Location(loc);  // make a copy
            r.width = width;
            r.height = height;
            r.thumbnailWidth = thumbnailWidth;
            r.orientation = orientation;
            synchronized (this) {
                while (mQueue.size() >= QUEUE_LIMIT) {
                    try {
                        wait();
                    } catch (InterruptedException ex) {
                        // ignore.
                    }
                }
                mQueue.add(r);
                notifyAll();  // Tell saver thread there is new work to do.
            }
        }

        // Runs in saver thread
        @Override
        public void run() {
            while (true) {
                SaveRequest r;
                synchronized (this) {
                    if (mQueue.isEmpty()) {
                        notifyAll();  // notify main thread in waitDone

                        // Note that we can only stop after we saved all images
                        // in the queue.
                        if (mStop) break;

                        try {
                            wait();
                        } catch (InterruptedException ex) {
                            // ignore.
                        }
                        continue;
                    }
                    r = mQueue.get(0);
                }
                storeImage(r.data, r.uri, r.title, r.loc, r.width, r.height,
                        r.thumbnailWidth, r.orientation);
                synchronized (this) {
                    mQueue.remove(0);
                    notifyAll();  // the main thread may wait in addImage
                }
            }
        }

        // Runs in main thread
        public void waitDone() {
            synchronized (this) {
                while (!mQueue.isEmpty()) {
                    try {
                        wait();
                    } catch (InterruptedException ex) {
                        // ignore.
                    }
                }
            }
            updateThumbnail();
        }

        // Runs in main thread
        public void finish() {
            waitDone();
            synchronized (this) {
                mStop = true;
                notifyAll();
            }
            try {
                join();
            } catch (InterruptedException ex) {
                // ignore.
            }
        }

        // Runs in main thread (because we need to update mThumbnailView in the
        // main thread)
        public void updateThumbnail() {
            Thumbnail t;
            synchronized (mUpdateThumbnailLock) {
                mHandler.removeMessages(UPDATE_THUMBNAIL);
                t = mPendingThumbnail;
                mPendingThumbnail = null;
            }

            if (t != null) {
                mThumbnail = t;
                mThumbnailView.setBitmap(mThumbnail.getBitmap());
            }
        }

        // Runs in saver thread
        private void storeImage(final byte[] data, Uri uri, String title,
                Location loc, int width, int height, int thumbnailWidth,
                int orientation) {
            String pictureFormat = mParameters.get(KEY_PICTURE_FORMAT);
            if(data == null) {
                Log.v(TAG, "image data null");
                return;
            }
            boolean ok = Storage.updateImage(mContentResolver, uri, title, loc,
                    orientation, data, width, height, pictureFormat);
            if (ok) {
                boolean needThumbnail;
                synchronized (this) {
                    // If the number of requests in the queue (include the
                    // current one) is greater than 1, we don't need to generate
                    // thumbnail for this image. Because we'll soon replace it
                    // with the thumbnail for some image later in the queue.
                    needThumbnail = (mQueue.size() <= 1);
                }
                if (needThumbnail) {
                    // Create a thumbnail whose width is equal or bigger than
                    // that of the thumbnail view.
                    int ratio = (int) Math.ceil((double) width / thumbnailWidth);
                    int inSampleSize = Integer.highestOneBit(ratio);
                    Thumbnail t = Thumbnail.createThumbnail(
                                data, orientation, inSampleSize, uri);
                    synchronized (mUpdateThumbnailLock) {
                        // We need to update the thumbnail in the main thread,
                        // so send a message to run updateThumbnail().
                        mPendingThumbnail = t;
                        mHandler.sendEmptyMessage(UPDATE_THUMBNAIL);
                    }
                }
                Util.broadcastNewPicture(Camera.this, uri);
            }
        }
    }

    private class ImageNamer extends Thread {
        private boolean mRequestPending;
        private ContentResolver mResolver;
        private long mDateTaken;
        private int mWidth, mHeight;
        private boolean mStop;
        private Uri mUri;
        private String mTitle;
        private String mPictureFormat;
        private Parameters mParams;
        // Runs in main thread
        public ImageNamer() {
            start();
        }

        // Runs in main thread
        public synchronized void prepareUri(ContentResolver resolver,
                long dateTaken, int width, int height, int rotation) {
            if (rotation % 180 != 0) {
                int tmp = width;
                width = height;
                height = tmp;
            }
            mRequestPending = true;
            mResolver = resolver;
            mDateTaken = dateTaken;
            mWidth = width;
            mHeight = height;
            notifyAll();
        }

        // Runs in main thread
        public synchronized Uri getUri() {
            // wait until the request is done.
            while (mRequestPending) {
                try {
                    wait();
                } catch (InterruptedException ex) {
                    // ignore.
                }
            }

            // return the uri generated
            Uri uri = mUri;
            mUri = null;
            return uri;
        }

        // Runs in main thread, should be called after getUri().
        public synchronized String getTitle() {
            return mTitle;
        }

        // Runs in namer thread
        @Override
        public synchronized void run() {
            while (true) {
                if (mStop) break;
                if (!mRequestPending) {
                    try {
                        wait();
                    } catch (InterruptedException ex) {
                        // ignore.
                    }
                    continue;
                }
                cleanOldUri();
                generateUri();
                mRequestPending = false;
                notifyAll();
            }
            cleanOldUri();
        }

        // Runs in main thread
        public synchronized void finish() {
            mStop = true;
            notifyAll();
        }

        // Runs in namer thread
        private void generateUri() {
            mTitle = Util.createJpegName(mDateTaken);
            mParams = mCameraDevice.getParameters(); 
            String pictureFormat = mParams.get(KEY_PICTURE_FORMAT);
            mUri = Storage.newImage(mResolver, mTitle, mDateTaken, mWidth, mHeight, pictureFormat);
        }

        // Runs in namer thread
        private void cleanOldUri() {
            if (mUri == null) return;
            Storage.deleteImage(mResolver, mUri);
            mUri = null;
        }
    }

    private void setCameraState(int state) {
        mCameraState = state;
        switch (state) {
            case PREVIEW_STOPPED:
            case SNAPSHOT_IN_PROGRESS:
            case FOCUSING:
            case SWITCHING_CAMERA:
                enableCameraControls(false);
                break;
            case IDLE:
                enableCameraControls(true);
                break;
        }
    }

    @Override
    public boolean capture() {
        // If we are already in the middle of taking a snapshot then ignore.
        if (mCameraDevice == null || mCameraState == SNAPSHOT_IN_PROGRESS
                || mCameraState == SWITCHING_CAMERA) {
            return false;
        }
        mCaptureStartTime = System.currentTimeMillis();
        mPostViewPictureCallbackTime = 0;
        mJpegImageData = null;

        // Set rotation and gps data.
        mJpegRotation = Util.getJpegRotation(mCameraId, mOrientation);
        mParameters.setRotation(mJpegRotation);
        String pictureFormat = mParameters.get(KEY_PICTURE_FORMAT);
        Location loc = null;
        if (pictureFormat != null &&
            PIXEL_FORMAT_JPEG.equalsIgnoreCase(pictureFormat)) {
            loc = mLocationManager.getCurrentLocation();
        }
        Util.setGpsParameters(mParameters, loc);
        mCameraDevice.setParameters(mParameters);

        mCameraDevice.takePicture(mShutterCallback, mRawPictureCallback,
                mPostViewPictureCallback, new JpegPictureCallback(loc));

        if(mHiston) {
            mHiston = false;
            mCameraDevice.setHistogramMode(null);
            runOnUiThread(new Runnable() {
                public void run() {
                    if(mGraphView != null)
                    mGraphView.setVisibility(View.INVISIBLE);
                }
            });
        }

        Size size = mParameters.getPictureSize();
        mImageNamer.prepareUri(mContentResolver, mCaptureStartTime,
                size.width, size.height, mJpegRotation);

        if (!mIsImageCaptureIntent) {
            // Start capture animation.
            mCameraScreenNail.animateCapture(getCameraRotation());
        }
        mFaceDetectionStarted = false;
        setCameraState(SNAPSHOT_IN_PROGRESS);
        mBurstSnapNum = mParameters.getInt("num-snaps-per-shutter");
        mReceivedSnapNum = 0;
        return true;
    }

    private int getCameraRotation() {
        return (mOrientationCompensation - mDisplayRotation + 360) % 360;
    }

    @Override
    public void setFocusParameters() {
        setCameraParameters(UPDATE_PARAM_PREFERENCE);
    }

    @Override
    public void playSound(int soundId) {
        mCameraSound.play(soundId);
    }

    private int getPreferredCameraId(ComboPreferences preferences) {
        int intentCameraId = Util.getCameraFacingIntentExtras(this);
        if (intentCameraId != -1) {
            // Testing purpose. Launch a specific camera through the intent
            // extras.
            return intentCameraId;
        } else {
            return CameraSettings.readPreferredCameraId(preferences);
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPreferences = new ComboPreferences(this);
        CameraSettings.upgradeGlobalPreferences(mPreferences.getGlobal());
        mCameraId = getPreferredCameraId(mPreferences);
        mContentResolver = getContentResolver();

        // To reduce startup time, open the camera and start the preview in
        // another thread.
        mCameraStartUpThread = new CameraStartUpThread();
        mCameraStartUpThread.start();

        setContentView(R.layout.camera);

        // Surface texture is from camera screen nail and startPreview needs it.
        // This must be done before startPreview.
        mIsImageCaptureIntent = isImageCaptureIntent();
        createCameraScreenNail(!mIsImageCaptureIntent);

        mPreferences.setLocalId(this, mCameraId);
        CameraSettings.upgradeLocalPreferences(mPreferences.getLocal());
        mFocusAreaIndicator = (RotateLayout) findViewById(
                R.id.focus_indicator_rotate_layout);
        // we need to reset exposure for the preview
        resetExposureCompensation();
        // Starting the preview needs preferences, camera screen nail, and
        // focus area indicator.
        mStartPreviewPrerequisiteReady.open();

        initializeControlByIntent();
        mRotateDialog = new RotateDialogController(this, R.layout.rotate_dialog);
        mNumberOfCameras = CameraHolder.instance().getNumberOfCameras();
        mQuickCapture = getIntent().getBooleanExtra(EXTRA_QUICK_CAPTURE, false);
        initializeMiscControls();
        brightnessProgressBar = (ProgressBar) findViewById(R.id.progress);
        if (brightnessProgressBar instanceof SeekBar) {
            SeekBar seeker = (SeekBar) brightnessProgressBar;
            seeker.setOnSeekBarChangeListener(mSeekListener);
        }
        brightnessProgressBar.setMax(MAXIMUM_BRIGHTNESS);
        brightnessProgressBar.setProgress(mbrightness);
        skinToneSeekBar = (SeekBar) findViewById(R.id.skintoneseek);
        skinToneSeekBar.setOnSeekBarChangeListener(mskinToneSeekListener);
        skinToneSeekBar.setVisibility(View.INVISIBLE);
        Title = (TextView)findViewById(R.id.skintonetitle);
        RightValue = (TextView)findViewById(R.id.skintoneright);
        LeftValue = (TextView)findViewById(R.id.skintoneleft);
        mLocationManager = new LocationManager(this, this);
        initOnScreenIndicator();
        // Make sure all views are disabled before camera is open.
        enableCameraControls(false);
    }

    private void overrideCameraSettings(final String flashMode,
            final String whiteBalance, final String focusMode,
            final String exposureMode, final String touchMode,
            final String autoExposure) {
        if (mIndicatorControlContainer != null) {
            mIndicatorControlContainer.enableFilter(true);
            mIndicatorControlContainer.overrideSettings(
                    CameraSettings.KEY_FLASH_MODE, flashMode,
                    CameraSettings.KEY_WHITE_BALANCE, whiteBalance,
                    CameraSettings.KEY_FOCUS_MODE, focusMode,
                    CameraSettings.KEY_EXPOSURE, exposureMode,
                    CameraSettings.KEY_TOUCH_AF_AEC, touchMode,
                    CameraSettings.KEY_AUTOEXPOSURE, autoExposure);
            mIndicatorControlContainer.enableFilter(false);
        }
    }

    private void updateSceneModeUI() {
        // If scene mode is set, we cannot set flash mode, white balance, and
        // focus mode, instead, we read it from driver
        if (!Parameters.SCENE_MODE_AUTO.equals(mSceneMode)) {
            overrideCameraSettings(mParameters.getFlashMode(),
                    mParameters.getWhiteBalance(), mParameters.getFocusMode(),
                    Integer.toString(mParameters.getExposureCompensation()),
                    mParameters.getTouchAfAec(), mParameters.getAutoExposure());
        } else {
            overrideCameraSettings(null, null, null, null, null, null);
        }
    }

    private void loadCameraPreferences() {
        CameraSettings settings = new CameraSettings(this, mInitialParams,
                mCameraId, CameraHolder.instance().getCameraInfo());
        mPreferenceGroup = settings.getPreferenceGroup(R.xml.camera_preferences);
    }

    private void initializeIndicatorControl() {
        // setting the indicator buttons.
        mIndicatorControlContainer =
                (IndicatorControlContainer) findViewById(R.id.indicator_control);
        loadCameraPreferences();
        otherSettingKeys.put(0, OTHER_SETTING_KEYS);
        otherSettingKeys.put(1, QCOM_SETTING_KEYS);
        otherSettingKeys.put(2, QCOM_SETTING_KEYS_1);
        final String[] SETTING_KEYS = {
                CameraSettings.KEY_FLASH_MODE,
                CameraSettings.KEY_WHITE_BALANCE,
                CameraSettings.KEY_EXPOSURE,
                CameraSettings.KEY_SCENE_MODE};
        
        CameraPicker.setImageResourceId(R.drawable.ic_switch_photo_facing_holo_light);
        mIndicatorControlContainer.initialize(this, mPreferenceGroup,
                mParameters.isZoomSupported(),
                SETTING_KEYS, otherSettingKeys);
        mCameraPicker = (CameraPicker) mIndicatorControlContainer.findViewById(
                R.id.camera_picker);
        updateSceneModeUI();
        mIndicatorControlContainer.setListener(this);
    }

    private boolean collapseCameraControls() {
        if ((mIndicatorControlContainer != null)
                && mIndicatorControlContainer.dismissSettingPopup()) {
            return true;
        }
        if (mModePicker != null && mModePicker.dismissModeSelection()) return true;
        return false;
    }

    private void enableCameraControls(boolean enable) {
        if (mIndicatorControlContainer != null) {
            mIndicatorControlContainer.setEnabled(enable);
        }
        if (mModePicker != null) mModePicker.setEnabled(enable);
        if (mZoomControl != null) mZoomControl.setEnabled(enable);
        if (mThumbnailView != null) mThumbnailView.setEnabled(enable);
    }

    private class MyOrientationEventListener
            extends OrientationEventListener {
        public MyOrientationEventListener(Context context) {
            super(context);
        }

        @Override
        public void onOrientationChanged(int orientation) {
            // We keep the last known orientation. So if the user first orient
            // the camera then point the camera to floor or sky, we still have
            // the correct orientation.
            if (orientation == ORIENTATION_UNKNOWN) return;
            mOrientation = Util.roundOrientation(orientation, mOrientation);
            // When the screen is unlocked, display rotation may change. Always
            // calculate the up-to-date orientationCompensation.
            int orientationCompensation =
                    (mOrientation + Util.getDisplayRotation(Camera.this)) % 360;
            if (mOrientationCompensation != orientationCompensation) {
                mOrientationCompensation = orientationCompensation;
                setOrientationIndicator(mOrientationCompensation, true);
            }

            // Show the toast after getting the first orientation changed.
            if (mHandler.hasMessages(SHOW_TAP_TO_FOCUS_TOAST)) {
                mHandler.removeMessages(SHOW_TAP_TO_FOCUS_TOAST);
                showTapToFocusToast();
            }
        }
    }

    private void setOrientationIndicator(int orientation, boolean animation) {
        Rotatable[] indicators = {mThumbnailView, mModePicker,
                mIndicatorControlContainer, mZoomControl, mFocusAreaIndicator, mFaceView,
                mReviewDoneButton, mRotateDialog, mOnScreenIndicators};
        for (Rotatable indicator : indicators) {
            if (indicator != null) indicator.setOrientation(orientation, animation);
        }

        // We change the orientation of the review cancel button only for tablet
        // UI because there's a label along with the X icon. For phone UI, we
        // don't change the orientation because there's only a symmetrical X
        // icon.
        if (mReviewCancelButton instanceof RotateLayout) {
            mReviewCancelButton.setOrientation(orientation, animation);
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mMediaProviderClient != null) {
            mMediaProviderClient.release();
            mMediaProviderClient = null;
        }
    }

    private void checkStorage() {
        mStorageSpace = Storage.getAvailableSpace();
        updateStorageHint(mStorageSpace);
    }

    @OnClickAttr
    public void onThumbnailClicked(View v) {
        if (isCameraIdle() && mThumbnail != null) {
            if (mImageSaver != null) mImageSaver.waitDone();
            gotoGallery();
        }
    }

    // onClick handler for R.id.btn_retake
    @OnClickAttr
    public void onReviewRetakeClicked(View v) {
        if (mPaused) return;

        hidePostCaptureAlert();
        startPreview();
        setCameraState(IDLE);
        startFaceDetection();
    }

    // onClick handler for R.id.btn_done
    @OnClickAttr
    public void onReviewDoneClicked(View v) {
        doAttach();
    }

    // onClick handler for R.id.btn_cancel
    @OnClickAttr
    public void onReviewCancelClicked(View v) {
        doCancel();
    }

    private void doAttach() {
        if (mPaused) {
            return;
        }

        byte[] data = mJpegImageData;

        if (mCropValue == null) {
            // First handle the no crop case -- just return the value.  If the
            // caller specifies a "save uri" then write the data to its
            // stream. Otherwise, pass back a scaled down version of the bitmap
            // directly in the extras.
            if (mSaveUri != null) {
                OutputStream outputStream = null;
                try {
                    outputStream = mContentResolver.openOutputStream(mSaveUri);
                    outputStream.write(data);
                    outputStream.close();

                    setResultEx(RESULT_OK);
                    finish();
                } catch (IOException ex) {
                    // ignore exception
                } finally {
                    Util.closeSilently(outputStream);
                }
            } else {
                int orientation = Exif.getOrientation(data);
                Bitmap bitmap = Util.makeBitmap(data, 50 * 1024);
                bitmap = Util.rotate(bitmap, orientation);
                setResultEx(RESULT_OK,
                        new Intent("inline-data").putExtra("data", bitmap));
                finish();
            }
        } else {
            // Save the image to a temp file and invoke the cropper
            Uri tempUri = null;
            FileOutputStream tempStream = null;
            try {
                File path = getFileStreamPath(sTempCropFilename);
                path.delete();
                tempStream = openFileOutput(sTempCropFilename, 0);
                tempStream.write(data);
                tempStream.close();
                tempUri = Uri.fromFile(path);
            } catch (FileNotFoundException ex) {
                setResultEx(Activity.RESULT_CANCELED);
                finish();
                return;
            } catch (IOException ex) {
                setResultEx(Activity.RESULT_CANCELED);
                finish();
                return;
            } finally {
                Util.closeSilently(tempStream);
            }

            Bundle newExtras = new Bundle();
            if (mCropValue.equals("circle")) {
                newExtras.putString("circleCrop", "true");
            }
            if (mSaveUri != null) {
                newExtras.putParcelable(MediaStore.EXTRA_OUTPUT, mSaveUri);
            } else {
                newExtras.putBoolean("return-data", true);
            }

            Intent cropIntent = new Intent("com.android.camera.action.CROP");

            cropIntent.setData(tempUri);
            cropIntent.putExtras(newExtras);

            startActivityForResult(cropIntent, REQUEST_CROP);
        }
    }

    private void doCancel() {
        setResultEx(RESULT_CANCELED, new Intent());
        finish();
    }

    @Override
    public void onShutterButtonFocus(boolean pressed) {
        if (mPaused || collapseCameraControls()
                || (mCameraState == SNAPSHOT_IN_PROGRESS)
                || (mCameraState == PREVIEW_STOPPED)) return;

        // Do not do focus if there is not enough storage.
        if (pressed && !canTakePicture()) return;

        if (pressed) {
            mFocusManager.onShutterDown();
        } else {
            mFocusManager.onShutterUp();
        }
    }

    @Override
    public void onShutterButtonClick() {
        if (mPaused || collapseCameraControls()
                || (mCameraState == SWITCHING_CAMERA)
                || (mCameraState == PREVIEW_STOPPED)) return;

        // Do not take the picture if there is not enough storage.
        if (mStorageSpace <= Storage.LOW_STORAGE_THRESHOLD) {
            Log.i(TAG, "Not enough space or storage not ready. remaining=" + mStorageSpace);
            return;
        }
        Log.v(TAG, "onShutterButtonClick: mCameraState=" + mCameraState);

        //Need to disable focus for ZSL mode
        if(mSnapshotMode == CameraInfo.CAMERA_SUPPORT_MODE_ZSL) {
            mFocusManager.setZslEnable(true);
        }else{
            mFocusManager.setZslEnable(false);
        }

        // If the user wants to do a snapshot while the previous one is still
        // in progress, remember the fact and do it after we finish the previous
        // one and re-start the preview. Snapshot in progress also includes the
        // state that autofocus is focusing and a picture will be taken when
        // focus callback arrives.
        if ((mFocusManager.isFocusingSnapOnFinish() || mCameraState == SNAPSHOT_IN_PROGRESS)
                && !mIsImageCaptureIntent) {
            mSnapshotOnIdle = true;
            return;
        }

        mSnapshotOnIdle = false;
        mFocusManager.doSnap();
    }

    private void installIntentFilter() {
        // install an intent filter to receive SD card related events.
        IntentFilter intentFilter =
                new IntentFilter(Intent.ACTION_MEDIA_MOUNTED);
        intentFilter.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        intentFilter.addAction(Intent.ACTION_MEDIA_SCANNER_FINISHED);
        intentFilter.addAction(Intent.ACTION_MEDIA_CHECKING);
        intentFilter.addDataScheme("file");
        registerReceiver(mReceiver, intentFilter);
        mDidRegister = true;
    }

    @Override
    protected void onResume() {
        mPaused = false;
        super.onResume();
        if (mOpenCameraFail || mCameraDisabled) return;

        mJpegPictureCallbackTime = 0;
        mZoomValue = 0;

        // Start the preview if it is not started.
        if (mCameraState == PREVIEW_STOPPED && mCameraStartUpThread == null) {
            resetExposureCompensation();
            mCameraStartUpThread = new CameraStartUpThread();
            mCameraStartUpThread.start();
        }

        if (!mIsImageCaptureIntent) getLastThumbnail();
        //Check if the skinTone SeekBar could not be enabled during updateCameraParametersPreference()
       //due to the finite latency of loading the seekBar layout when switching modes
       // for same Camera Device instance
        if (mSkinToneSeekBar != true)
        {
            Log.e(TAG, "Send tone bar: mSkinToneSeekBar = " + mSkinToneSeekBar);
            mHandler.sendEmptyMessage(SET_SKIN_TONE_FACTOR);
        }

        // If first time initialization is not finished, put it in the
        // message queue.
        if (!mFirstTimeInitialized) {
            mHandler.sendEmptyMessage(FIRST_TIME_INIT);
        } else {
            initializeSecondTime();
        }
        keepScreenOnAwhile();

        // Dismiss open menu if exists.
        PopupManager.getInstance(this).notifyShowPopup(null);

        if (mCameraSound == null) {
            mCameraSound = new MediaActionSound();
            // Not required, but reduces latency when playback is requested later.
            mCameraSound.load(MediaActionSound.FOCUS_COMPLETE);
        }
    }

    void waitCameraStartUpThread() {
        try {
            if (mCameraStartUpThread != null) {
                mCameraStartUpThread.cancel();
                mCameraStartUpThread.join();
                mCameraStartUpThread = null;
                setCameraState(IDLE);
            }
        } catch (InterruptedException e) {
            // ignore
        }
    }

    @Override
    protected void onPause() {
        mPaused = true;
        super.onPause();

        // Wait the camera start up thread to finish.
        waitCameraStartUpThread();

        if(mGraphView != null)
            mGraphView.setCameraObject(null);

        stopPreview();
        // Close the camera now because other activities may need to use it.
        closeCamera();
        if (mSurfaceTexture != null) {
            mCameraScreenNail.releaseSurfaceTexture();
            mSurfaceTexture = null;
        }
        if (mCameraSound != null) {
            mCameraSound.release();
            mCameraSound = null;
        }
        resetScreenOn();

        // Clear UI.
        collapseCameraControls();
        if (mFaceView != null) mFaceView.clear();

        if (mFirstTimeInitialized) {
            mOrientationListener.disable();
            if (mImageSaver != null) {
                mImageSaver.finish();
                mImageSaver = null;
                mImageNamer.finish();
                mImageNamer = null;
            }
        }

        if (mDidRegister) {
            unregisterReceiver(mReceiver);
            mDidRegister = false;
        }
        if (mLocationManager != null) mLocationManager.recordLocation(false);
        updateExposureOnScreenIndicator(0);

        // If we are in an image capture intent and has taken
        // a picture, we just clear it in onPause.
        mJpegImageData = null;

        // Remove the messages in the event queue.
        mHandler.removeMessages(FIRST_TIME_INIT);
        mHandler.removeMessages(CHECK_DISPLAY_ROTATION);
        mHandler.removeMessages(SWITCH_CAMERA);
        mHandler.removeMessages(SWITCH_CAMERA_START_ANIMATION);
        mHandler.removeMessages(CAMERA_OPEN_DONE);
        mHandler.removeMessages(START_PREVIEW_DONE);
        mHandler.removeMessages(OPEN_CAMERA_FAIL);
        mHandler.removeMessages(CAMERA_DISABLED);

        mPendingSwitchCameraId = -1;
        if (mFocusManager != null) mFocusManager.removeMessages();
    }

    private void initializeControlByIntent() {
        if (mIsImageCaptureIntent) {
            // Cannot use RotateImageView for "done" and "cancel" button because
            // the tablet layout uses RotateLayout, which cannot be cast to
            // RotateImageView.
            mReviewDoneButton = (Rotatable) findViewById(R.id.btn_done);
            mReviewCancelButton = (Rotatable) findViewById(R.id.btn_cancel);
            mReviewRetakeButton = findViewById(R.id.btn_retake);
            findViewById(R.id.btn_cancel).setVisibility(View.VISIBLE);

            // Not grayed out upon disabled, to make the follow-up fade-out
            // effect look smooth. Note that the review done button in tablet
            // layout is not a TwoStateImageView.
            if (mReviewDoneButton instanceof TwoStateImageView) {
                ((TwoStateImageView) mReviewDoneButton).enableFilter(false);
            }

            setupCaptureParams();
        } else {
            mThumbnailView = (RotateImageView) findViewById(R.id.thumbnail);
            mThumbnailView.enableFilter(false);
            mThumbnailView.setVisibility(View.VISIBLE);
            mThumbnailViewWidth = mThumbnailView.getLayoutParams().width;

            mModePicker = (ModePicker) findViewById(R.id.mode_picker);
            mModePicker.setVisibility(View.VISIBLE);
            mModePicker.setOnModeChangeListener(this);
            mModePicker.setCurrentMode(ModePicker.MODE_CAMERA);
        }
    }

    private void initializeFocusManager() {
        // Create FocusManager object. startPreview needs it.
        CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
        boolean mirror = (info.facing == CameraInfo.CAMERA_FACING_FRONT);
        String[] defaultFocusModes = getResources().getStringArray(
                R.array.pref_camera_focusmode_default_array);
        mFocusManager = new FocusManager(mPreferences, defaultFocusModes,
                mFocusAreaIndicator, mInitialParams, this, mirror,
                getMainLooper());
    }

    private void initializeMiscControls() {
        // startPreview needs this.
        mPreviewFrameLayout = (PreviewFrameLayout) findViewById(R.id.frame);
        // Set touch focus listener.
        setSingleTapUpListener(mPreviewFrameLayout);

        mZoomControl = (ZoomControl) findViewById(R.id.zoom_control);
        mOnScreenIndicators = (Rotatable) findViewById(R.id.on_screen_indicators);
        mFaceView = (FaceView) findViewById(R.id.face_view);
        mPreviewFrameLayout.addOnLayoutChangeListener(this);
        mPreviewFrameLayout.setOnSizeChangedListener(this);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        setDisplayOrientation();
        setPreviewFrameLayoutOrientation();
        // Change layout in response to configuration change
        LinearLayout appRoot = (LinearLayout) findViewById(R.id.camera_app_root);
        appRoot.setOrientation(
                newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE
                ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
        appRoot.removeAllViews();
        LayoutInflater inflater = getLayoutInflater();
        inflater.inflate(R.layout.preview_frame, appRoot);
        inflater.inflate(R.layout.camera_control, appRoot);

        // from onCreate()
        initializeControlByIntent();
        initializeFocusManager();
        initializeMiscControls();
        initializeIndicatorControl();
        mFocusAreaIndicator = (RotateLayout) findViewById(
                R.id.focus_indicator_rotate_layout);
        mFocusManager.setFocusAreaIndicator(mFocusAreaIndicator);

        // from onResume()
        if (!mIsImageCaptureIntent) updateThumbnailView();

        // from initializeFirstTime()
        mShutterButton = (ShutterButton) findViewById(R.id.shutter_button);
        mShutterButton.setOnShutterButtonListener(this);
        mShutterButton.setVisibility(View.VISIBLE);
        initializeZoom();
        initOnScreenIndicator();
        updateOnScreenIndicators();
        mFaceView.clear();
        mFaceView.setVisibility(View.VISIBLE);
        mFaceView.setDisplayOrientation(mDisplayOrientation);
        CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
        mFaceView.setMirror(info.facing == CameraInfo.CAMERA_FACING_FRONT);
        mFaceView.resume();
        mFocusManager.setFaceView(mFaceView);
    }

    @Override
    protected void onActivityResult(
            int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        switch (requestCode) {
            case REQUEST_CROP: {
                Intent intent = new Intent();
                if (data != null) {
                    Bundle extras = data.getExtras();
                    if (extras != null) {
                        intent.putExtras(extras);
                    }
                }
                setResultEx(resultCode, intent);
                finish();

                File path = getFileStreamPath(sTempCropFilename);
                path.delete();

                break;
            }
        }
    }

    private boolean canTakePicture() {
        return isCameraIdle() && (mStorageSpace > Storage.LOW_STORAGE_THRESHOLD);
    }

    protected CameraManager.CameraProxy getCamera() {
        return mCameraDevice;
    }

    @Override
    public void autoFocus() {
        mFocusStartTime = System.currentTimeMillis();
        mCameraDevice.autoFocus(mAutoFocusCallback);
        setCameraState(FOCUSING);
    }

    @Override
    public void cancelAutoFocus() {
        mCameraDevice.cancelAutoFocus();
        setCameraState(IDLE);
        setCameraParameters(UPDATE_PARAM_PREFERENCE);
    }

    // Preview area is touched. Handle touch focus.
    @Override
    protected void onSingleTapUp(View view, int x, int y) {
        if (mPaused || mCameraDevice == null || !mFirstTimeInitialized
                || mCameraState == SNAPSHOT_IN_PROGRESS
                || mCameraState == SWITCHING_CAMERA
                || mCameraState == PREVIEW_STOPPED) {
            return;
        }

        // Do not trigger touch focus if popup window is opened.
        if (collapseCameraControls()) return;

        //If Touch AF/AEC is disabled in UI, return
        if(this.mTouchAfAecFlag == false) {
            return;
        }
        // Check if metering area or focus area is supported.
        if (!mFocusAreaSupported && !mMeteringAreaSupported) return;

        mFocusManager.onSingleTapUp(x, y);
    }

    @Override
    public void onBackPressed() {
        if (!isCameraIdle()) {
            // ignore backs while we're taking a picture
            return;
        } else if (!collapseCameraControls()) {
            super.onBackPressed();
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_FOCUS:
                if (mFirstTimeInitialized && event.getRepeatCount() == 0) {
                    onShutterButtonFocus(true);
                }
                return true;
            case KeyEvent.KEYCODE_CAMERA:
                if (mFirstTimeInitialized && event.getRepeatCount() == 0) {
                    onShutterButtonClick();
                }
                return true;
                case KeyEvent.KEYCODE_DPAD_LEFT:
                    if ( (mCameraState != PREVIEW_STOPPED) &&
                            (mFocusManager.getCurrentFocusState() != mFocusManager.STATE_FOCUSING) &&
                            (mFocusManager.getCurrentFocusState() != mFocusManager.STATE_FOCUSING_SNAP_ON_FINISH) ) {
                        if (mbrightness > MINIMUM_BRIGHTNESS) {
                            mbrightness-=mbrightness_step;

                            /* Set the "luma-adaptation" parameter */
                            mParameters = mCameraDevice.getParameters();
                            mParameters.set("luma-adaptation", String.valueOf(mbrightness));
                            mCameraDevice.setParameters(mParameters);
                        }

                        brightnessProgressBar.setProgress(mbrightness);
                        brightnessProgressBar.setVisibility(View.VISIBLE);

                    }
                    break;
                case KeyEvent.KEYCODE_DPAD_RIGHT:
                    if ( (mCameraState != PREVIEW_STOPPED) &&
                            (mFocusManager.getCurrentFocusState() != mFocusManager.STATE_FOCUSING) &&
                            (mFocusManager.getCurrentFocusState() != mFocusManager.STATE_FOCUSING_SNAP_ON_FINISH) ) {
                        if (mbrightness < MAXIMUM_BRIGHTNESS) {
                            mbrightness+=mbrightness_step;

                            /* Set the "luma-adaptation" parameter */
                            mParameters = mCameraDevice.getParameters();
                            mParameters.set("luma-adaptation", String.valueOf(mbrightness));
                            mCameraDevice.setParameters(mParameters);

                        }
                        brightnessProgressBar.setProgress(mbrightness);
                        brightnessProgressBar.setVisibility(View.VISIBLE);

                    }
                    break;
            case KeyEvent.KEYCODE_DPAD_CENTER:
                // If we get a dpad center event without any focused view, move
                // the focus to the shutter button and press it.
                if (mFirstTimeInitialized && event.getRepeatCount() == 0) {
                    // Start auto-focus immediately to reduce shutter lag. After
                    // the shutter button gets the focus, onShutterButtonFocus()
                    // will be called again but it is fine.
                    if (collapseCameraControls()) return true;
                    onShutterButtonFocus(true);
                    if (mShutterButton.isInTouchMode()) {
                        mShutterButton.requestFocusFromTouch();
                    } else {
                        mShutterButton.requestFocus();
                    }
                    mShutterButton.setPressed(true);
                }
                return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_FOCUS:
                if (mFirstTimeInitialized) {
                    onShutterButtonFocus(false);
                }
                return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    private void closeCamera() {
        if (mCameraDevice != null) {
            mCameraDevice.setZoomChangeListener(null);
            mCameraDevice.setFaceDetectionListener(null);
            mCameraDevice.setErrorCallback(null);
            CameraHolder.instance().release();
            mFaceDetectionStarted = false;
            mCameraDevice = null;
            setCameraState(PREVIEW_STOPPED);
            mFocusManager.onCameraReleased();
        }
    }

    private void setDisplayOrientation() {
        mDisplayRotation = Util.getDisplayRotation(this);
        mDisplayOrientation = Util.getDisplayOrientation(mDisplayRotation, mCameraId);
        mCameraDisplayOrientation = Util.getDisplayOrientation(0, mCameraId);
        if (mFaceView != null) {
            mFaceView.setDisplayOrientation(mDisplayOrientation);
        }
        mFocusManager.setDisplayOrientation(mDisplayOrientation);
    }

    private void startPreview() {
        mFocusManager.resetTouchFocus();

        mCameraDevice.setErrorCallback(mErrorCallback);

        // If we're previewing already, stop the preview first (this will blank
        // the screen).
        if (mCameraState != PREVIEW_STOPPED) stopPreview();

        setDisplayOrientation();
        mCameraDevice.setDisplayOrientation(mCameraDisplayOrientation);

        if (!mSnapshotOnIdle) {
            // If the focus mode is continuous autofocus, call cancelAutoFocus to
            // resume it because it may have been paused by autoFocus call.
            if (Parameters.FOCUS_MODE_CONTINUOUS_PICTURE.equals(mFocusManager.getFocusMode())) {
                mCameraDevice.cancelAutoFocus();
            }
            mFocusManager.setAeAwbLock(false); // Unlock AE and AWB.
        }
        setCameraParameters(UPDATE_PARAM_ALL);

        Size size = mParameters.getPreviewSize();
        int oldWidth = mCameraScreenNail.getWidth();
        int oldHeight = mCameraScreenNail.getHeight();
        Log.v(TAG, "New Width x New Height : "+ size.width + "x" + size.height);
        Log.v(TAG, "Old Width x New Height : "+ oldWidth + "x" + oldHeight);
        Log.e(TAG, "mCameraDisplayOrientation : "+ mCameraDisplayOrientation);
        if (mCameraDisplayOrientation % 180 != 0) {// Swap the width and Height for 90 & 270 degress
            int tmp = size.width;
            size.width = size.height;
            size.height = tmp;
        }
        if (oldWidth != size.width || oldHeight != size.height) {
            mCameraScreenNail.setSize(size.width, size.height);
            notifyScreenNailChanged();
        }
        if (mSurfaceTexture == null) {
            mCameraScreenNail.acquireSurfaceTexture();
            mSurfaceTexture = mCameraScreenNail.getSurfaceTexture();
        }

        mCameraDevice.setPreviewTextureAsync(mSurfaceTexture);
        Log.v(TAG, "startPreview");
        mCameraDevice.startPreviewAsync();

        mFocusManager.onPreviewStarted();

        if (mSnapshotOnIdle) {
            mHandler.post(mDoSnapRunnable);
        }
    }

    private void stopPreview() {
        if (mCameraDevice != null && mCameraState != PREVIEW_STOPPED) {
            Log.v(TAG, "stopPreview");
            mCameraDevice.cancelAutoFocus(); // Reset the focus.
            mCameraDevice.stopPreview();
            mFaceDetectionStarted = false;
        }
        setCameraState(PREVIEW_STOPPED);
        if (mFocusManager != null) mFocusManager.onPreviewStopped();
    }

    private static boolean isSupported(String value, List<String> supported) {
        return supported == null ? false : supported.indexOf(value) >= 0;
    }

    @SuppressWarnings("deprecation")
    private void updateCameraParametersInitialize() {
        // Reset preview frame rate to the maximum because it may be lowered by
        // video camera application.
        List<Integer> frameRates = mParameters.getSupportedPreviewFrameRates();
        if (frameRates != null) {
            Integer max = Collections.max(frameRates);
            mParameters.setPreviewFrameRate(max);
        }

        mParameters.setRecordingHint(false);

        // Disable video stabilization. Convenience methods not available in API
        // level <= 14
        String vstabSupported = mParameters.get("video-stabilization-supported");
        if ("true".equals(vstabSupported)) {
            mParameters.set("video-stabilization", "false");
        }
    }

    private void updateCameraParametersZoom() {
        // Set zoom.
        if (mParameters.isZoomSupported()) {
            mParameters.setZoom(mZoomValue);
        }
    }

    private boolean needRestart() {
        mRestartPreview = false;
        String zsl = mPreferences.getString(CameraSettings.KEY_ZSL,
                                  getString(R.string.pref_camera_zsl_default));
        if(zsl.equals("on") && mSnapshotMode != CameraInfo.CAMERA_SUPPORT_MODE_ZSL
           && mCameraState != PREVIEW_STOPPED) {
            //Switch on ZSL Camera mode
            Log.e(TAG, "Switching to ZSL Camera Mode. Restart Preview");
            mRestartPreview = true;
            return mRestartPreview;
        }
        if(zsl.equals("off") && mSnapshotMode != CameraInfo.CAMERA_SUPPORT_MODE_NONZSL
                 && mCameraState != PREVIEW_STOPPED) {
            //Switch on Normal Camera mode
            Log.e(TAG, "Switching to Normal Camera Mode. Restart Preview");
            mRestartPreview = true;
            return mRestartPreview;
        }
        return mRestartPreview;
    }

    private void qcomUpdateCameraParametersPreference(){
        //qcom Related Parameter update
        //Set Brightness.
        mParameters.set("luma-adaptation", String.valueOf(mbrightness));

		if (Parameters.SCENE_MODE_AUTO.equals(mSceneMode)) {
            // Set Touch AF/AEC parameter.
            String touchAfAec = mPreferences.getString(
                 CameraSettings.KEY_TOUCH_AF_AEC,
                 getString(R.string.pref_camera_touchafaec_default));
            if (isSupported(touchAfAec, mParameters.getSupportedTouchAfAec())) {
                mParameters.setTouchAfAec(touchAfAec);
            }
        } else {
            mParameters.setTouchAfAec(mParameters.TOUCH_AF_AEC_OFF);
            mFocusManager.resetTouchFocus();
        }
        try {
            if(mParameters.getTouchAfAec().equals(mParameters.TOUCH_AF_AEC_ON))
                this.mTouchAfAecFlag = true;
            else
                this.mTouchAfAecFlag = false;
        } catch(Exception e){
            Log.e(TAG, "Handled NULL pointer Exception");
        }

        // Set Picture Format
        // Picture Formats specified in UI should be consistent with
        // PIXEL_FORMAT_JPEG and PIXEL_FORMAT_RAW constants
        String pictureFormat = mPreferences.getString(
                CameraSettings.KEY_PICTURE_FORMAT,
                getString(R.string.pref_camera_picture_format_default));
        mParameters.set(KEY_PICTURE_FORMAT, pictureFormat);
        // Set JPEG quality.
        String jpegQuality = mPreferences.getString(
                CameraSettings.KEY_JPEG_QUALITY,
                getString(R.string.pref_camera_jpegquality_default));

        //mUnsupportedJpegQuality = false;
        Size pic_size = mParameters.getPictureSize();
        if (pic_size == null) {
            Log.e(TAG, "error getPictureSize: size is null");
        }
        else{
            if("100".equals(jpegQuality) && (pic_size.width >= 3200)){
                //mUnsupportedJpegQuality = true;
            }else {
                mParameters.setJpegQuality(JpegEncodingQualityMappings.getQualityNumber(jpegQuality));
            }
        }
        // Set Selectable Zone Af parameter.
        String selectableZoneAf = mPreferences.getString(
            CameraSettings.KEY_SELECTABLE_ZONE_AF,
            getString(R.string.pref_camera_selectablezoneaf_default));
        List<String> str = mParameters.getSupportedSelectableZoneAf();
        if (isSupported(selectableZoneAf, mParameters.getSupportedSelectableZoneAf())) {
            mParameters.setSelectableZoneAf(selectableZoneAf);
        }
        /*
        //Set LensShading
        String lensshade = mPreferences.getString(
                CameraSettings.KEY_LENSSHADING,
                getString(R.string.pref_camera_lensshading_default));
        if (isSupported(lensshade,
                mParameters.getSupportedLensShadeModes())) {
                mParameters.setLensShade(lensshade);
        }*/

        //Set AE Bracket
        String hdr = mPreferences.getString(CameraSettings.KEY_AE_BRACKET_HDR,
                                  getString(R.string.pref_camera_ae_bracket_hdr_default));
        mParameters.setAEBracket(hdr);


        if (hdr.equalsIgnoreCase(getString(R.string.pref_camera_ae_bracket_hdr_value_hdr))) {
            mParameters.set("num-snaps-per-shutter", "2");
        } else if (hdr.equalsIgnoreCase(getString(R.string.pref_camera_ae_bracket_hdr_value_ae_bracket))) {
            String burst_exp = SystemProperties.get("persist.capture.burst.exposures", "");
            Log.i(TAG, "capture-burst-exposures = " + burst_exp);
            if ( (burst_exp != null) && (burst_exp.length()>0) ) {
                mParameters.set("capture-burst-exposures", burst_exp);
            }
            if (burst_exp != null) {
                String[] split_values = burst_exp.split(",");
                if (split_values != null) {
                    String snapshot_number = "";
                    snapshot_number += split_values.length;
                    Log.i(TAG, "num-snaps-per-shutter = " + snapshot_number);
                    mParameters.set("num-snaps-per-shutter", snapshot_number);
                }
            }
        }else {
            mParameters.set("num-snaps-per-shutter", "1");
        }

        // Set wavelet denoise mode
        if (mParameters.getSupportedDenoiseModes() != null) {
            String Denoise = mPreferences.getString( CameraSettings.KEY_DENOISE,
                             getString(R.string.pref_camera_denoise_default));
            mParameters.setDenoise(Denoise);
        }
        // Set Redeye Reduction
        String redeyeReduction = mPreferences.getString(
                CameraSettings.KEY_REDEYE_REDUCTION,
                getString(R.string.pref_camera_redeyereduction_default));
        if (isSupported(redeyeReduction,
            mParameters.getSupportedRedeyeReductionModes())) {
            mParameters.setRedeyeReductionMode(redeyeReduction);
        }
        // Set ISO parameter
        String iso = mPreferences.getString(
                CameraSettings.KEY_ISO,
                getString(R.string.pref_camera_iso_default));
        if (isSupported(iso,
                mParameters.getSupportedIsoValues())) {
                mParameters.setISOValue(iso);
        }

        // Set color effect parameter.
        String colorEffect = mPreferences.getString(
                CameraSettings.KEY_COLOR_EFFECT,
                getString(R.string.pref_camera_coloreffect_default));
        Log.v(TAG, "Color effect value =" + colorEffect);
        if (isSupported(colorEffect, mParameters.getSupportedColorEffects())) {
            mParameters.setColorEffect(colorEffect);
        }

        //Set Saturation
        String saturationStr = mPreferences.getString(
                CameraSettings.KEY_SATURATION,
                getString(R.string.pref_camera_saturation_default));
        int saturation = Integer.parseInt(saturationStr);
        Log.v(TAG, "Saturation value =" + saturation);
        if((0 <= saturation) && (saturation <= mParameters.getMaxSaturation())){
            mParameters.setSaturation(saturation);
        }

        // Set contrast parameter.
        String contrastStr = mPreferences.getString(
                CameraSettings.KEY_CONTRAST,
                getString(R.string.pref_camera_contrast_default));
        int contrast = Integer.parseInt(contrastStr);
        Log.v(TAG, "Contrast value =" +contrast);
        if((0 <= contrast) && (contrast <= mParameters.getMaxContrast())){
            mParameters.setContrast(contrast);
        }

        // Set sharpness parameter.
        String sharpnessStr = mPreferences.getString(
                CameraSettings.KEY_SHARPNESS,
                getString(R.string.pref_camera_sharpness_default));
        int sharpness = Integer.parseInt(sharpnessStr) *
                (mParameters.getMaxSharpness()/MAX_SHARPNESS_LEVEL);
        Log.v(TAG, "Sharpness value =" + sharpness);
        if((0 <= sharpness) && (sharpness <= mParameters.getMaxSharpness())){
            mParameters.setSharpness(sharpness);
        }

        // Set auto exposure parameter.
        String autoExposure = mPreferences.getString(
                CameraSettings.KEY_AUTOEXPOSURE,
                getString(R.string.pref_camera_autoexposure_default));
        Log.v(TAG, "autoExposure value =" + autoExposure);
        if (isSupported(autoExposure, mParameters.getSupportedAutoexposure())) {
            mParameters.setAutoExposure(autoExposure);
        }

         // Set anti banding parameter.
         String antiBanding = mPreferences.getString(
                 CameraSettings.KEY_ANTIBANDING,
                 getString(R.string.pref_camera_antibanding_default));
         Log.v(TAG, "antiBanding value =" + antiBanding);
         if (isSupported(antiBanding, mParameters.getSupportedAntibanding())) {
             mParameters.setAntibanding(antiBanding);
         }

         String zsl = mPreferences.getString(CameraSettings.KEY_ZSL,
                                  getString(R.string.pref_camera_zsl_default));
        if(zsl.equals("on")) {
            //Switch on ZSL Camera mode
            mSnapshotMode = CameraInfo.CAMERA_SUPPORT_MODE_ZSL;
            mParameters.setCameraMode(1);
            mFocusManager.setZslEnable(true);

            // Currently HDR is not supported under ZSL mode
            Editor editor = mPreferences.edit();
            editor.putString(CameraSettings.KEY_AE_BRACKET_HDR, getString(R.string.pref_camera_ae_bracket_hdr_value_off));
            editor.apply();
        }else if(zsl.equals("off")) {
            mSnapshotMode = CameraInfo.CAMERA_SUPPORT_MODE_NONZSL;
            mParameters.setCameraMode(0);
            mFocusManager.setZslEnable(false);
        }

         // Set face detetction parameter.
         String faceDetection = mPreferences.getString(
             CameraSettings.KEY_FACE_DETECTION,
             getString(R.string.pref_camera_facedetection_default));

         if (isSupported(faceDetection, mParameters.getSupportedFaceDetectionModes())) {
             mParameters.setFaceDetectionMode(faceDetection);
             if(faceDetection.equals("on") && mFaceDetectionEnabled == false) {
               mFaceDetectionEnabled = true;
               startFaceDetection();
             }
             if(faceDetection.equals("off") && mFaceDetectionEnabled == true) {
               stopFaceDetection();
               mFaceDetectionEnabled = false;
             }
        }
		 // skin tone ie enabled only for auto,party and portrait BSM
         // when color effects are not enabled
         if((Parameters.SCENE_MODE_PARTY.equals(mSceneMode) ||
             Parameters.SCENE_MODE_PORTRAIT.equals(mSceneMode))&&
             (Parameters.EFFECT_NONE.equals(colorEffect))) {
             //Set Skin Tone Correction factor
             Log.e(TAG, "yyan set tone bar: mSceneMode = " + mSceneMode);
             if(mSeekBarInitialized == true)
                 setSkinToneFactor();
         }

         //Set Histogram
        String histogram = mPreferences.getString(
                CameraSettings.KEY_HISTOGRAM,
                getString(R.string.pref_camera_histogram_default));
        if (isSupported(histogram,
                mParameters.getSupportedHistogramModes()) && mCameraDevice != null) {
                // Call for histogram
                if(histogram.equals("enable")) {
                runOnUiThread(new Runnable() {
                     public void run() {
                         if(mGraphView != null)
                             mGraphView.setVisibility(View.VISIBLE);
                         }
                    });
                    mCameraDevice.setHistogramMode(mStatsCallback);
                    mHiston = true;
                } else {
                    mHiston = false;
                    runOnUiThread(new Runnable() {
                         public void run() {
                             if(mGraphView != null)
                                 mGraphView.setVisibility(View.INVISIBLE);
                         }
                    });
                    mCameraDevice.setHistogramMode(null);
                }
        }
    }

    private void updateCameraParametersPreference() {
        if (mAeLockSupported) {
            mParameters.setAutoExposureLock(mFocusManager.getAeAwbLock());
        }

        if (mAwbLockSupported) {
            mParameters.setAutoWhiteBalanceLock(mFocusManager.getAeAwbLock());
        }

        if (mFocusAreaSupported) {
            mParameters.setFocusAreas(mFocusManager.getFocusAreas());
        }

        if (mMeteringAreaSupported) {
            // Use the same area for focus and metering.
            mParameters.setMeteringAreas(mFocusManager.getMeteringAreas());
        }
        // Set picture size.
        String pictureSize = mPreferences.getString(
                CameraSettings.KEY_PICTURE_SIZE, null);

        if (pictureSize == null) {
            CameraSettings.initialCameraPictureSize(this, mParameters);
        } else {
            Size old_size = mParameters.getPictureSize();
            List<Size> supported = mParameters.getSupportedPictureSizes();
            CameraSettings.setCameraPictureSize(
                    pictureSize, supported, mParameters);
            Size size = mParameters.getPictureSize();
            if (old_size != null && size != null) {
                if(!size.equals(old_size) && mCameraState != PREVIEW_STOPPED) {
                    Log.v(TAG, "Picture Size changed. Restart Preview");
                    mRestartPreview = true;
                }
            }
        }
        Size size = mParameters.getPictureSize();

        // Set a preview size that is closest to the viewfinder height and has
        // the right aspect ratio.
        List<Size> sizes = mParameters.getSupportedPreviewSizes();
        Size optimalSize = Util.getOptimalPreviewSize(this, sizes,
                (double) size.width / size.height);
        Size original = mParameters.getPreviewSize();
        if (!original.equals(optimalSize)) {
            mParameters.setPreviewSize(optimalSize.width, optimalSize.height);
            // Zoom related settings will be changed for different preview
            // sizes, so set and read the parameters to get latest values
            mCameraDevice.setParameters(mParameters);
            mParameters = mCameraDevice.getParameters();
            Log.v(TAG, "Preview Size changed. Restart Preview");
            mRestartPreview = true;
        }
        // Since change scene mode may change supported values,
        // Set scene mode first,

        mSceneMode = mPreferences.getString(
                CameraSettings.KEY_SCENE_MODE,
                getString(R.string.pref_camera_scenemode_default));
        Log.v(TAG, "mSceneMode " + mSceneMode);
        if (isSupported(mSceneMode, mParameters.getSupportedSceneModes())) {
            if (!mParameters.getSceneMode().equals(mSceneMode)) {
                mParameters.setSceneMode(mSceneMode);

                // Setting scene mode will change the settings of flash mode,
                // white balance, and focus mode. Here we read back the
                // parameters, so we can know those settings.
                mCameraDevice.setParameters(mParameters);
                mParameters = mCameraDevice.getParameters();
            }
        } else {
            mSceneMode = mParameters.getSceneMode();
            if (mSceneMode == null) {
                mSceneMode = Parameters.SCENE_MODE_AUTO;
            }
        }

        // Set JPEG quality.
        int jpegQuality = CameraProfile.getJpegEncodingQualityParameter(mCameraId,
                CameraProfile.QUALITY_HIGH);
        mParameters.setJpegQuality(jpegQuality);

        // For the following settings, we need to check if the settings are
        // still supported by latest driver, if not, ignore the settings.

        // Set exposure compensation
        int value = CameraSettings.readExposure(mPreferences);
        int max = mParameters.getMaxExposureCompensation();
        int min = mParameters.getMinExposureCompensation();
        if (value >= min && value <= max) {
            mParameters.setExposureCompensation(value);
        } else {
            Log.w(TAG, "invalid exposure range: " + value);
        }

        if (Parameters.SCENE_MODE_AUTO.equals(mSceneMode)) {
            // Set flash mode.
            String flashMode = mPreferences.getString(
                    CameraSettings.KEY_FLASH_MODE,
                    getString(R.string.pref_camera_flashmode_default));
            List<String> supportedFlash = mParameters.getSupportedFlashModes();
            if (isSupported(flashMode, supportedFlash)) {
                mParameters.setFlashMode(flashMode);
            } else {
                flashMode = mParameters.getFlashMode();
                if (flashMode == null) {
                    flashMode = getString(
                            R.string.pref_camera_flashmode_no_flash);
                }
            }

            // Set white balance parameter.
            String whiteBalance = mPreferences.getString(
                    CameraSettings.KEY_WHITE_BALANCE,
                    getString(R.string.pref_camera_whitebalance_default));
            if (isSupported(whiteBalance,
                    mParameters.getSupportedWhiteBalance())) {
                mParameters.setWhiteBalance(whiteBalance);
            } else {
                whiteBalance = mParameters.getWhiteBalance();
                if (whiteBalance == null) {
                    whiteBalance = Parameters.WHITE_BALANCE_AUTO;
                }
            }

            // Set focus mode.
            mFocusManager.overrideFocusMode(null);
            mParameters.setFocusMode(mFocusManager.getFocusMode());
        } else {
            mFocusManager.overrideFocusMode(mParameters.getFocusMode());
        }

        if (mContinousFocusSupported) {
            if (mParameters.getFocusMode().equals(Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)) {
                mCameraDevice.setAutoFocusMoveCallback(mAutoFocusMoveCallback);
            } else {
                mCameraDevice.setAutoFocusMoveCallback(null);
            }
        }
        //QCom related parameters updated here.
        qcomUpdateCameraParametersPreference();
    }

    // We separate the parameters into several subsets, so we can update only
    // the subsets actually need updating. The PREFERENCE set needs extra
    // locking because the preference can be changed from GLThread as well.
    private void setCameraParameters(int updateSet) {
        if ((updateSet & UPDATE_PARAM_INITIALIZE) != 0) {
            updateCameraParametersInitialize();
        }

        if ((updateSet & UPDATE_PARAM_ZOOM) != 0) {
            updateCameraParametersZoom();
        }

        if ((updateSet & UPDATE_PARAM_PREFERENCE) != 0) {
            updateCameraParametersPreference();
        }

        mCameraDevice.setParameters(mParameters);
    }

    // If the Camera is idle, update the parameters immediately, otherwise
    // accumulate them in mUpdateSet and update later.
    private void setCameraParametersWhenIdle(int additionalUpdateSet) {
        mUpdateSet |= additionalUpdateSet;
        if (mCameraDevice == null) {
            // We will update all the parameters when we open the device, so
            // we don't need to do anything now.
            mUpdateSet = 0;
            return;
        } else if (isCameraIdle()) {
            setCameraParameters(mUpdateSet);
            if(mRestartPreview && mCameraState != PREVIEW_STOPPED) {
                Log.e(TAG, "Restarting Preview...");
                stopPreview();
                setPreviewFrameLayoutAspectRatio();
                startPreview();
                setCameraState(IDLE);
            }
            mRestartPreview = false;
            updateSceneModeUI();
            mUpdateSet = 0;
        } else {
            if (!mHandler.hasMessages(SET_CAMERA_PARAMETERS_WHEN_IDLE)) {
                mHandler.sendEmptyMessageDelayed(
                        SET_CAMERA_PARAMETERS_WHEN_IDLE, 1000);
            }
        }

    }

    private boolean isCameraIdle() {
        return (mCameraState == IDLE) ||
                ((mFocusManager != null) && mFocusManager.isFocusCompleted()
                        && (mCameraState != SWITCHING_CAMERA));
    }

    private boolean isImageCaptureIntent() {
        String action = getIntent().getAction();
        return (MediaStore.ACTION_IMAGE_CAPTURE.equals(action));
    }

    private void setupCaptureParams() {
        Bundle myExtras = getIntent().getExtras();
        if (myExtras != null) {
            mSaveUri = (Uri) myExtras.getParcelable(MediaStore.EXTRA_OUTPUT);
            mCropValue = myExtras.getString("crop");
        }
    }

    private void showPostCaptureAlert() {
        if (mIsImageCaptureIntent) {
            Util.fadeOut(mIndicatorControlContainer);
            Util.fadeOut(mShutterButton);

            Util.fadeIn(mReviewRetakeButton);
            Util.fadeIn((View) mReviewDoneButton);
        }
    }

    private void hidePostCaptureAlert() {
        if (mIsImageCaptureIntent) {
            Util.fadeOut(mReviewRetakeButton);
            Util.fadeOut((View) mReviewDoneButton);

            Util.fadeIn(mShutterButton);
            if (mIndicatorControlContainer != null) {
                Util.fadeIn(mIndicatorControlContainer);
            }
        }
    }

    private void switchToOtherMode(int mode) {
        if (isFinishing()) return;
        if (mImageSaver != null) mImageSaver.waitDone();
        if (mThumbnail != null) ThumbnailHolder.keep(mThumbnail);
        MenuHelper.gotoMode(mode, Camera.this);
        mHandler.removeMessages(FIRST_TIME_INIT);
        finish();
    }

    @Override
    public void onModeChanged(int mode) {
        if (mode != ModePicker.MODE_CAMERA) switchToOtherMode(mode);
    }

    @Override
    public void onSharedPreferenceChanged() {
        // ignore the events after "onPause()"
        if (mPaused) return;

        boolean recordLocation = RecordLocationPreference.get(
                mPreferences, mContentResolver);
        mLocationManager.recordLocation(recordLocation);

        if(needRestart()){
            Log.e(TAG, "Restarting Preview... Camera Mode Changhed");
            stopPreview();
            startPreview();
            setCameraState(IDLE);
            mRestartPreview = false;
        }
        setCameraParametersWhenIdle(UPDATE_PARAM_PREFERENCE);
        setPreviewFrameLayoutAspectRatio();
        updateOnScreenIndicators();
        if (mSeekBarInitialized == true){
            Log.e(TAG, "yyan onSharedPreferenceChanged Skin tone bar: change");
                    // skin tone ie enabled only for auto,party and portrait BSM
                    // when color effects are not enabled
                    String colorEffect = mPreferences.getString(
                        CameraSettings.KEY_COLOR_EFFECT,
                        getString(R.string.pref_camera_coloreffect_default));
                    if((Parameters.SCENE_MODE_PARTY.equals(mSceneMode) ||
                        Parameters.SCENE_MODE_PORTRAIT.equals(mSceneMode))&&
                        (Parameters.EFFECT_NONE.equals(colorEffect))) {
                        ;
                    }
                    else{
                        disableSkinToneSeekBar();
                    }
		}
    }

    @Override
    public void onCameraPickerClicked(int cameraId) {
        if (mPaused || mPendingSwitchCameraId != -1) return;

        Log.v(TAG, "Start to copy texture. cameraId=" + cameraId);
        // We need to keep a preview frame for the animation before
        // releasing the camera. This will trigger onPreviewTextureCopied.
        mCameraScreenNail.copyTexture();
        mPendingSwitchCameraId = cameraId;
        // Disable all camera controls.
        setCameraState(SWITCHING_CAMERA);
    }

    private void switchCamera() {
        if (mPaused) return;

        Log.v(TAG, "Start to switch camera. id=" + mPendingSwitchCameraId);
        mCameraId = mPendingSwitchCameraId;
        mPendingSwitchCameraId = -1;
        mCameraPicker.setCameraId(mCameraId);

        // from onPause
        closeCamera();
        mSurfaceTexture = null;
        collapseCameraControls();
        if (mFaceView != null) mFaceView.clear();
        if (mFocusManager != null) mFocusManager.removeMessages();

        // Restart the camera and initialize the UI. From onCreate.
        mPreferences.setLocalId(Camera.this, mCameraId);
        CameraSettings.upgradeLocalPreferences(mPreferences.getLocal());
        CameraOpenThread cameraOpenThread = new CameraOpenThread();
        cameraOpenThread.start();
        try {
            cameraOpenThread.join();
        } catch (InterruptedException ex) {
            // ignore
        }
        initializeCapabilities();
        CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
        boolean mirror = (info.facing == CameraInfo.CAMERA_FACING_FRONT);
        mFocusManager.setMirror(mirror);
        mFocusManager.setParameters(mInitialParams);
        setPreviewFrameLayoutAspectRatio();
        startPreview();
        setCameraState(IDLE);
        startFaceDetection();
        initializeIndicatorControl();

        // from onResume
        setOrientationIndicator(mOrientationCompensation, false);
        // from initializeFirstTime
        initializeZoom();
        updateOnScreenIndicators();
        showTapToFocusToastIfNeeded();

        // Start switch camera animation. Post a message because
        // onFrameAvailable from the old camera may already exist.
        mHandler.sendEmptyMessage(SWITCH_CAMERA_START_ANIMATION);
    }

    // Preview texture has been copied. Now camera can be released and the
    // animation can be started.
    @Override
    protected void onPreviewTextureCopied() {
        mHandler.sendEmptyMessage(SWITCH_CAMERA);
    }

    @Override
    public void onUserInteraction() {
        super.onUserInteraction();
        keepScreenOnAwhile();
    }

    private void resetScreenOn() {
        mHandler.removeMessages(CLEAR_SCREEN_DELAY);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    private void keepScreenOnAwhile() {
        mHandler.removeMessages(CLEAR_SCREEN_DELAY);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mHandler.sendEmptyMessageDelayed(CLEAR_SCREEN_DELAY, SCREEN_DELAY);
    }

    @Override
    public void onRestorePreferencesClicked() {
        if (mPaused) return;
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                restorePreferences();
            }
        };
        mRotateDialog.showAlertDialog(
                null,
                getString(R.string.confirm_restore_message),
                getString(android.R.string.ok), runnable,
                getString(android.R.string.cancel), null);
    }

    private void restorePreferences() {
        // Reset the zoom. Zoom value is not stored in preference.
        if (mParameters.isZoomSupported()) {
            mZoomValue = 0;
            setCameraParametersWhenIdle(UPDATE_PARAM_ZOOM);
            mZoomControl.setZoomIndex(0);
        }
        if (mIndicatorControlContainer != null) {
            mIndicatorControlContainer.dismissSettingPopup();
            CameraSettings.restorePreferences(Camera.this, mPreferences,
                    mParameters);
            mIndicatorControlContainer.reloadPreferences();
            onSharedPreferenceChanged();
        }
    }

    @Override
    public void onOverriddenPreferencesClicked() {
        if (mPaused) return;
        if (mNotSelectableToast == null) {
            String str = getResources().getString(R.string.not_selectable_in_scene_mode);
            mNotSelectableToast = Toast.makeText(Camera.this, str, Toast.LENGTH_SHORT);
        }
        mNotSelectableToast.show();
    }

    @Override
    public void onFaceDetection(Face[] faces, android.hardware.Camera camera) {
        mFaceView.setFaces(faces);
    }

    private void showTapToFocusToast() {
        new RotateTextToast(this, R.string.tap_to_focus, mOrientationCompensation).show();
        // Clear the preference.
        Editor editor = mPreferences.edit();
        editor.putBoolean(CameraSettings.KEY_CAMERA_FIRST_USE_HINT_SHOWN, false);
        editor.apply();
    }

    private void initializeCapabilities() {
        mInitialParams = mCameraDevice.getParameters();
        mFocusAreaSupported = (mInitialParams.getMaxNumFocusAreas() > 0
                && isSupported(Parameters.FOCUS_MODE_AUTO,
                        mInitialParams.getSupportedFocusModes()));
        mMeteringAreaSupported = (mInitialParams.getMaxNumMeteringAreas() > 0);
        mAeLockSupported = mInitialParams.isAutoExposureLockSupported();
        mAwbLockSupported = mInitialParams.isAutoWhiteBalanceLockSupported();
        mContinousFocusSupported = mInitialParams.getSupportedFocusModes().contains(
                Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
    }

    // PreviewFrameLayout size has changed.
    @Override
    public void onSizeChanged(int width, int height) {
        if (mFocusManager != null) mFocusManager.setPreviewSize(width, height);
    }

    void setPreviewFrameLayoutOrientation(){
       boolean set = true;
       Size size = mParameters.getPictureSize();
       CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];

       setDisplayOrientation();

       if(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT) {
          if( (mDisplayRotation == 0) || (mDisplayRotation == 180) ) {
             if(info.orientation % 180 != 0)
                set = false;
             else
                set = true;
          }
          else { // mDisplayRotation = 90 or 270
             if(info.orientation % 180 != 0)
                set = true;
             else
                set = false;
          }
       }
       else {  // ORIENTATION_LANDSCAPE case
          if( (mDisplayRotation == 0) || (mDisplayRotation == 180) ) {
             if(info.orientation % 180 != 0)
                set = true;
             else
                set = false;
          }
          else { // mDisplayRotation = 90 or 270
             if(info.orientation % 180 != 0)
                set = false;
             else
                set = true;
          }
       }
       mPreviewFrameLayout.setCameraOrientation(set);
    }

    void setPreviewFrameLayoutAspectRatio() {
        setPreviewFrameLayoutOrientation();
        // Set the preview frame aspect ratio according to the picture size.
        Size size = mParameters.getPictureSize();
        mPreviewFrameLayout.setAspectRatio((double) size.width / size.height);      
    }

    private void setSkinToneFactor(){
       if(mCameraDevice == null || mParameters == null || skinToneSeekBar == null) return;
       String skinToneEnhancementPref = "enable";
       if(isSupported(skinToneEnhancementPref,
               mParameters.getSupportedSkinToneEnhancementModes())){
         if(skinToneEnhancementPref.equals("enable")) {
             int skinToneValue =0;
             int progress;
               //get the value for the first time!
               if (mskinToneValue ==0){
                  String factor = mPreferences.getString(CameraSettings.KEY_SKIN_TONE_ENHANCEMENT_FACTOR, "0");
                  skinToneValue = Integer.parseInt(factor);
               }

               Log.e(TAG, "yyan Skin tone bar: enable = " + mskinToneValue);
               enableSkinToneSeekBar();
               //As a wrokaround set progress again to show the actually progress on screen.
               if(skinToneValue != 0) {
                   progress = (skinToneValue/SCE_FACTOR_STEP)-MIN_SCE_FACTOR;
                   skinToneSeekBar.setProgress(progress);
               }
          } else {
              Log.e(TAG, "yyan Skin tone bar: disable");
               disableSkinToneSeekBar();
          }
       } else {
           Log.e(TAG, "yyan Skin tone bar: Not supported");
          skinToneSeekBar.setVisibility(View.INVISIBLE);
       }
    }

    private void enableSkinToneSeekBar() {
        int progress;
        if(brightnessProgressBar != null)
           brightnessProgressBar.setVisibility(View.INVISIBLE);
        skinToneSeekBar.setMax(MAX_SCE_FACTOR-MIN_SCE_FACTOR);
        skinToneSeekBar.setVisibility(View.VISIBLE);
        skinToneSeekBar.requestFocus();
        if (mskinToneValue != 0) {
            progress = (mskinToneValue/SCE_FACTOR_STEP)-MIN_SCE_FACTOR;
            mskinToneSeekListener.onProgressChanged(skinToneSeekBar,progress,false);
        }else {
            progress = (MAX_SCE_FACTOR-MIN_SCE_FACTOR)/2;
            RightValue.setText("");
            LeftValue.setText("");
        }
        skinToneSeekBar.setProgress(progress);
        Title.setText("Skin Tone Enhancement");
        Title.setVisibility(View.VISIBLE);
        RightValue.setVisibility(View.VISIBLE);
        LeftValue.setVisibility(View.VISIBLE);
        mSkinToneSeekBar = true;
    }

    private void disableSkinToneSeekBar() {
         skinToneSeekBar.setVisibility(View.INVISIBLE);
         Title.setVisibility(View.INVISIBLE);
         RightValue.setVisibility(View.INVISIBLE);
         LeftValue.setVisibility(View.INVISIBLE);
         mskinToneValue = 0;
         mSkinToneSeekBar = false;
         Editor editor = mPreferences.edit();
         editor.putString(CameraSettings.KEY_SKIN_TONE_ENHANCEMENT_FACTOR,
                            Integer.toString(mskinToneValue - MIN_SCE_FACTOR));
         editor.apply();
         if(brightnessProgressBar != null)
             brightnessProgressBar.setVisibility(View.VISIBLE);
    }
}

/*
 * Provide a mapping for Jpeg encoding quality levels
 * from String representation to numeric representation.
 */
class JpegEncodingQualityMappings {
    private static final String TAG = "JpegEncodingQualityMappings";
    private static final int DEFAULT_QUALITY = 85;
    private static HashMap<String, Integer> mHashMap =
            new HashMap<String, Integer>();

    static {
        mHashMap.put("normal",    CameraProfile.QUALITY_LOW);
        mHashMap.put("fine",      CameraProfile.QUALITY_MEDIUM);
        mHashMap.put("superfine", CameraProfile.QUALITY_HIGH);
    }

    // Retrieve and return the Jpeg encoding quality number
    // for the given quality level.
    public static int getQualityNumber(String jpegQuality) {
        try{
            int qualityPercentile = Integer.parseInt(jpegQuality);
            if(qualityPercentile >= 0 && qualityPercentile <=100)
                return qualityPercentile;
            else
                return DEFAULT_QUALITY;
        } catch(NumberFormatException nfe){
            //chosen quality is not a number, continue
        }
        Integer quality = mHashMap.get(jpegQuality);
        if (quality == null) {
            Log.w(TAG, "Unknown Jpeg quality: " + jpegQuality);
            return DEFAULT_QUALITY;
        }
        return CameraProfile.getJpegEncodingQualityParameter(quality.intValue());
    }
}

//-------------
 //Graph View Class

class GraphView extends View {
    private Bitmap  mBitmap;
    private Paint   mPaint = new Paint();
    private Canvas  mCanvas = new Canvas();
    private float   mScale = (float)3;
    private float   mWidth;
    private float   mHeight;
    private Camera mCamera;
    private CameraManager.CameraProxy mGraphCameraDevice;
    private float scaled;
    private static final int STATS_SIZE = 256;
    private static final String TAG = "GraphView";


    public GraphView(Context context, AttributeSet attrs) {
        super(context,attrs);

        mPaint.setFlags(Paint.ANTI_ALIAS_FLAG);
    }
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mBitmap = Bitmap.createBitmap(w, h, Bitmap.Config.RGB_565);
        mCanvas.setBitmap(mBitmap);
        mWidth = w;
        mHeight = h;
        super.onSizeChanged(w, h, oldw, oldh);
    }
    @Override
    protected void onDraw(Canvas canvas) {
        Log.v(TAG, "in Camera.java ondraw");
        if(!Camera.mHiston ) {
            Log.e(TAG, "returning as histogram is off ");
            return;
        }
    if (mBitmap != null) {
        final Paint paint = mPaint;
        final Canvas cavas = mCanvas;
        final float border = 5;
        float graphheight = mHeight - (2 * border);
        float graphwidth = mWidth - (2 * border);
        float left,top,right,bottom;
        float bargap = 0.0f;
        float barwidth = 1.0f;

        cavas.drawColor(0xFFAAAAAA);
        paint.setColor(Color.BLACK);

        for (int k = 0; k <= (graphheight /32) ; k++) {
            float y = (float)(32 * k)+ border;
            cavas.drawLine(border, y, graphwidth + border , y, paint);
        }
        for (int j = 0; j <= (graphwidth /32); j++) {
            float x = (float)(32 * j)+ border;
            cavas.drawLine(x, border, x, graphheight + border, paint);
        }
        paint.setColor(0xFFFFFFFF);
        synchronized(Camera.statsdata) {
            for(int i=1 ; i<=STATS_SIZE ; i++)  {
                scaled = Camera.statsdata[i]/mScale;
                if(scaled >= (float)STATS_SIZE)
                    scaled = (float)STATS_SIZE;
                left = (bargap * (i+1)) + (barwidth * i) + border;
                top = graphheight + border;
                right = left + barwidth;
                bottom = top - scaled;
                cavas.drawRect(left, top, right, bottom, paint);
            }
        }
        canvas.drawBitmap(mBitmap, 0, 0, null);

    }
        if(Camera.mHiston && mCamera!= null) {
            mGraphCameraDevice = mCamera.getCamera();
            if(mGraphCameraDevice != null){
                mGraphCameraDevice.sendHistogramData();
            }
        }
    }
    public void PreviewChanged() {
        invalidate();
    }
    public void setCameraObject(Camera camera) {
        mCamera = camera;
    }
}
