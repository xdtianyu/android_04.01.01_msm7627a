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

package com.android.camera.ui;

import com.android.camera.R;
import com.android.camera.Util;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.hardware.Camera.Face;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
/* ###QOALCOMM_CAMERA_ADDS_ON_END### */
import com.qualcomm.camera.QCFace;
/* ###QOALCOMM_CAMERA_ADDS_ON_END### */

public class FaceView extends View implements FocusIndicator, Rotatable {
    private static final String TAG = "FaceView";
    private final boolean LOGV = false;
    // The value for android.hardware.Camera.setDisplayOrientation.
    private int mDisplayOrientation;
    // The orientation compensation for the face indicator to make it look
    // correctly in all device orientations. Ex: if the value is 90, the
    // indicator should be rotated 90 degrees counter-clockwise.
    private int mOrientation;
    private boolean mMirror;
    private boolean mPause;
    private Matrix mMatrix = new Matrix();
    private RectF mRect = new RectF();
    private Face[] mFaces;
    private Drawable mFaceIndicator;
    private final Drawable mDrawableFocusing;
    private final Drawable mDrawableFocused;
    private final Drawable mDrawableFocusFailed;

    /* ###QOALCOMM_CAMERA_ADDS_ON_START### */
    private Paint mPaint;
    private final int smile_threashold_no_smile = 30;
    private final int smile_threashold_small_smile = 60;
    private final int blink_threashold = 60;
    /* ###QOALCOMM_CAMERA_ADDS_ON_END### */
    public FaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mDrawableFocusing = getResources().getDrawable(R.drawable.ic_focus_focusing);
        mDrawableFocused = getResources().getDrawable(R.drawable.ic_focus_face_focused);
        mDrawableFocusFailed = getResources().getDrawable(R.drawable.ic_focus_failed);
        mFaceIndicator = mDrawableFocusing;
        /* ###QOALCOMM_CAMERA_ADDS_ON_START### */
        mPaint = new Paint();
        mPaint.setAntiAlias(true);
        mPaint.setDither(true);
        mPaint.setColor(Color.WHITE);//setColor(0xFFFFFF00);
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setStrokeCap(Paint.Cap.ROUND);
        mPaint.setStrokeWidth(10);
        /* ###QOALCOMM_CAMERA_ADDS_ON_END### */
    }

    public void setFaces(Face[] faces) {
        if (LOGV) Log.v(TAG, "Num of faces=" + faces.length);
        if (mPause) return;
        mFaces = faces;
        invalidate();
    }

    public void setDisplayOrientation(int orientation) {
        mDisplayOrientation = orientation;
        if (LOGV) Log.v(TAG, "mDisplayOrientation=" + orientation);
    }

    @Override
    public void setOrientation(int orientation, boolean animation) {
        mOrientation = orientation;
        invalidate();
    }

    public void setMirror(boolean mirror) {
        mMirror = mirror;
        if (LOGV) Log.v(TAG, "mMirror=" + mirror);
    }

    public boolean faceExists() {
        return (mFaces != null && mFaces.length > 0);
    }

    @Override
    public void showStart() {
        mFaceIndicator = mDrawableFocusing;
        invalidate();
    }

    // Ignore the parameter. No autofocus animation for face detection.
    @Override
    public void showSuccess(boolean timeout) {
        mFaceIndicator = mDrawableFocused;
        invalidate();
    }

    // Ignore the parameter. No autofocus animation for face detection.
    @Override
    public void showFail(boolean timeout) {
        mFaceIndicator = mDrawableFocusFailed;
        invalidate();
    }

    @Override
    public void clear() {
        // Face indicator is displayed during preview. Do not clear the
        // drawable.
        mFaceIndicator = mDrawableFocusing;
        mFaces = null;
        invalidate();
    }

    public void pause() {
        mPause = true;
    }

    public void resume() {
        mPause = false;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (mFaces != null && mFaces.length > 0) {
            // Prepare the matrix.
            Util.prepareMatrix(mMatrix, mMirror, mDisplayOrientation, getWidth(), getHeight());

            // Focus indicator is directional. Rotate the matrix and the canvas
            // so it looks correctly in all orientations.
            canvas.save();
            mMatrix.postRotate(mOrientation); // postRotate is clockwise
            canvas.rotate(-mOrientation); // rotate is counter-clockwise (for canvas)
            for (int i = 0; i < mFaces.length; i++) {
                // Transform the coordinates.
                mRect.set(mFaces[i].rect);
                if (LOGV) Util.dumpRect(mRect, "Original rect");
                mMatrix.mapRect(mRect);
                if (LOGV) Util.dumpRect(mRect, "Transformed rect");

                mFaceIndicator.setBounds(Math.round(mRect.left), Math.round(mRect.top),
                        Math.round(mRect.right), Math.round(mRect.bottom));
                mFaceIndicator.draw(canvas);
                if (mFaces[i] instanceof QCFace) {
                    QCFace face = (QCFace)mFaces[i];
                    /* ###QOALCOMM_CAMERA_ADDS_ON_START### */
                    float[] point = new float[4];
                    int delta_x = mFaces[i].rect.width() / 12;
                    int delta_y = mFaces[i].rect.height() / 12;
                    Log.e(TAG, "blink: (" + face.getLeftEyeBlinkDegree()+ ", " + face.getRightEyeBlinkDegree() + ")");
                    if (face.leftEye != null) {
                        point[0] = face.leftEye.x;
                        point[1] = face.leftEye.y-delta_y/2;
                        point[2] = face.leftEye.x;
                        point[3] = face.leftEye.y+delta_y/2;
                        mMatrix.mapPoints (point);
                        if (face.getLeftEyeBlinkDegree() >= blink_threashold) {
                            canvas.drawLine(point[0], point[1], point[2], point[3], mPaint);
                        }
                    }

                    if (face.rightEye != null) {
                        point[0] = face.rightEye.x;
                        point[1] = face.rightEye.y-delta_y/2;
                        point[2] = face.rightEye.x;
                        point[3] = face.rightEye.y+delta_y/2;
                        mMatrix.mapPoints (point);
                        if (face.getRightEyeBlinkDegree() >= blink_threashold) {
                            canvas.drawLine(point[0], point[1], point[2], point[3], mPaint);
                        }
                    }

                    if (face.getLeftRightGazeDegree() != 0 || face.getTopBottomGazeDegree() != 0 ) {
                        double length = Math.sqrt((face.leftEye.x - face.rightEye.x) *
                            (face.leftEye.x - face.rightEye.x) +
                            (face.leftEye.y - face.rightEye.y) *
                            (face.leftEye.y - face.rightEye.y)) / 2.0;
                        double nGazeYaw = -face.getLeftRightGazeDegree();
                        double nGazePitch = -face.getTopBottomGazeDegree();
                        float gazeRollX = (float)((-Math.sin(nGazeYaw/180.0*Math.PI) *
                            Math.cos(-face.getRollDirection()/ 180.0*Math.PI)+
                            Math.sin(nGazePitch/180.0*Math.PI)* Math.cos(nGazeYaw/180.0*Math.PI)
                            * Math.sin(-face.getRollDirection()/180.0*Math.PI))
                            * (-length) + 0.5);
                        float gazeRollY = (float)((Math.sin(-nGazeYaw/180.0*Math.PI) *
                            Math.sin(-face.getRollDirection()/180.0*Math.PI)-
                            Math.sin(nGazePitch/180.0*Math.PI)* Math.cos(nGazeYaw/180.0*Math.PI)*
                            Math.cos(-face.getRollDirection()/180.0*Math.PI))* (-length) + 0.5);

                        if (face.getLeftEyeBlinkDegree() < blink_threashold) {
                            point[0] = face.leftEye.x;
                            point[1] = face.leftEye.y;
                            point[2] = face.leftEye.x + gazeRollX;
                            point[3] = face.leftEye.y + gazeRollY;
                            mMatrix.mapPoints (point);
                            canvas.drawLine(point[0], point[1], point[2], point[3], mPaint);
                        }

                        if (face.getRightEyeBlinkDegree() < blink_threashold) {
                            point[0] = face.rightEye.x;
                            point[1] = face.rightEye.y;
                            point[2] = face.rightEye.x + gazeRollX;
                            point[3] = face.rightEye.y + gazeRollY;
                            mMatrix.mapPoints (point);
                            canvas.drawLine(point[0], point[1], point[2], point[3], mPaint);
                        }
                    }

                    if (face.mouth != null) {
                        Log.e(TAG, "smile: " + face.getSmileDegree() + "," + face.getSmileScore());
                        if (face.getSmileDegree() < smile_threashold_no_smile) {
                            point[0] = face.mouth.x;
                            point[1] = face.mouth.y-delta_y;
                            point[2] = face.mouth.x;
                            point[3] = face.mouth.y+delta_y;
                            mMatrix.mapPoints (point);
                            canvas.drawLine(point[0], point[1], point[2], point[3], mPaint);
                        } else if (face.getSmileDegree() < smile_threashold_small_smile) {
                            mRect.set(face.mouth.x-delta_x, face.mouth.y-delta_y,
                                      face.mouth.x+delta_x, face.mouth.y+delta_y);
                            mMatrix.mapRect(mRect);
                            canvas.drawArc(mRect, 0, 180, true, mPaint);
                        } else {
                            mRect.set(face.mouth.x-delta_x, face.mouth.y-delta_y,
                                      face.mouth.x+delta_x, face.mouth.y+delta_y);
                            mMatrix.mapRect(mRect);
                            canvas.drawOval(mRect, mPaint);
                        }
                    }
                    /* ###QOALCOMM_CAMERA_ADDS_ON_END### */
                }
            }
            canvas.restore();
        }
        super.onDraw(canvas);
    }
}
