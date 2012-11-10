/*
 * Copyright (C) 2006 The Android Open Source Project
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

package com.android.internal.telephony;

import android.util.Log;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Maintain the Apn context
 */
public class ApnContext {

    public final String LOG_TAG;

    protected static final boolean DBG = true;

    private final String mApnType;

    private final int mPriority;

    private DataConnectionTracker.State mState;

    private ArrayList<DataProfile> mWaitingApns = null;

    /** A zero indicates that all waiting APNs had a permanent error */
    private AtomicInteger mWaitingApnsPermanentFailureCountDown;

    private DataProfile mDataProfile;

    DataConnection mDataConnection;

    DataConnectionAc mDataConnectionAc;

    String mReason;

    int mRetryCount;

    /**
     * user/app requested connection on this APN
     */
    AtomicBoolean mDataEnabled;

    /**
     * carrier requirements met
     */
    AtomicBoolean mDependencyMet;

    public ApnContext(String apnType, String logTag) {
        mApnType = apnType;
        mPriority = DataConnectionTracker.mApnPriorities.get(apnType);
        mState = DataConnectionTracker.State.IDLE;
        setReason(Phone.REASON_DATA_ENABLED);
        setRetryCount(0);
        mDataEnabled = new AtomicBoolean(false);
        mDependencyMet = new AtomicBoolean(true);
        mWaitingApnsPermanentFailureCountDown = new AtomicInteger(0);
        LOG_TAG = logTag;
    }

    public String getApnType() {
        return mApnType;
    }

    public synchronized DataConnection getDataConnection() {
        return mDataConnection;
    }

    public synchronized void setDataConnection(DataConnection dc) {
        if (DBG) {
            log("setDataConnection: old dc=" + mDataConnection + " new dc=" + dc + " this=" + this);
        }
        mDataConnection = dc;
    }


    public synchronized DataConnectionAc getDataConnectionAc() {
        return mDataConnectionAc;
    }

    public synchronized void setDataConnectionAc(DataConnectionAc dcac) {
        if (DBG) {
            log("setDataConnectionAc: old dcac=" + mDataConnectionAc + " new dcac=" + dcac);
        }
        if (mDataConnectionAc == dcac) {
            // Nothing needs to be done
            return;
        }
        if (mDataConnectionAc != null) {
            mDataConnectionAc.removeApnContextSync(this);
        }
        if (dcac != null) {
            dcac.addApnContextSync(this);
        }
        mDataConnectionAc = dcac;
    }

    public synchronized DataProfile getApnSetting() {
        return mDataProfile;
    }

    public synchronized void setApnSetting(DataProfile apnSetting) {
        mDataProfile = apnSetting;
    }

    public synchronized void setWaitingApns(ArrayList<DataProfile> waitingApns) {
        mWaitingApns = waitingApns;
        mWaitingApnsPermanentFailureCountDown.set(mWaitingApns.size());
    }

    public int getWaitingApnsPermFailCount() {
        return mWaitingApnsPermanentFailureCountDown.get();
    }

    public void decWaitingApnsPermFailCount() {
        mWaitingApnsPermanentFailureCountDown.decrementAndGet();
    }

    public synchronized DataProfile getNextWaitingApn() {
        ArrayList<DataProfile> list = mWaitingApns;
        DataProfile apn = null;

        if (list != null) {
            if (!list.isEmpty()) {
                apn = list.get(0);
            }
        }
        return apn;
    }

    public synchronized void removeWaitingApn(DataProfile apn) {
        if (mWaitingApns != null) {
            mWaitingApns.remove(apn);
        }
    }

    public synchronized ArrayList<DataProfile> getWaitingApns() {
        return mWaitingApns;
    }

    public synchronized int getPriority() {
        return mPriority;
    }

    public synchronized boolean isHigherPriority(ApnContext context) {
        return this.mPriority > context.getPriority();
    }

    public synchronized boolean isLowerPriority(ApnContext context) {
        return this.mPriority < context.getPriority();
    }

    public synchronized boolean isEqualPriority(ApnContext context) {
        return this.mPriority == context.getPriority();
    }

    public synchronized void setState(DataConnectionTracker.State s) {
        if (DBG) {
            log("setState: " + s + ", previous state:" + mState);
        }

        mState = s;

        if (mState == DataConnectionTracker.State.FAILED) {
            if (mWaitingApns != null) {
                mWaitingApns.clear(); // when teardown the connection and set to IDLE
            }
        }
    }

    public synchronized DataConnectionTracker.State getState() {
        return mState;
    }

    public boolean isDisconnected() {
        DataConnectionTracker.State currentState = getState();
        return ((currentState == DataConnectionTracker.State.IDLE) ||
                    currentState == DataConnectionTracker.State.FAILED);
    }

    public synchronized void setReason(String reason) {
        if (DBG) {
            log("set reason as " + reason + ",current state " + mState);
        }
        mReason = reason;
    }

    public synchronized String getReason() {
        return mReason;
    }

    public synchronized void setRetryCount(int retryCount) {
        if (DBG) {
            log("setRetryCount: " + retryCount);
        }
        mRetryCount = retryCount;
        DataConnection dc = mDataConnection;
        if (dc != null) {
            dc.setRetryCount(retryCount);
        }
    }

    public synchronized int getRetryCount() {
        return mRetryCount;
    }

    public boolean isReady() {
        return mDataEnabled.get() && mDependencyMet.get() && !getTetheredCallOn();
    }

    public void setEnabled(boolean enabled) {
        if (DBG) {
            log("set enabled as " + enabled + ", current state is " + mDataEnabled.get());
        }
        mDataEnabled.set(enabled);
    }

    public boolean isEnabled() {
        return mDataEnabled.get();
    }

    public void setDependencyMet(boolean met) {
        if (DBG) {
            log("set mDependencyMet as " + met + " current state is " + mDependencyMet.get());
        }
        mDependencyMet.set(met);
    }

    public boolean getDependencyMet() {
       return mDependencyMet.get();
    }

    @Override
    public String toString() {
        // We don't print mDataConnection because its recursive.
        return "{mApnType=" + mApnType + " mState=" + getState() + " mWaitingApns=" + mWaitingApns +
                " mWaitingApnsPermanentFailureCountDown=" + mWaitingApnsPermanentFailureCountDown +
                " mDataConnectionAc=" + mDataConnectionAc +
                " mReason=" + mReason + " mRetryCount=" + mRetryCount +
                " mDataEnabled=" + mDataEnabled + " mDependencyMet=" + mDependencyMet + "}";
    }

    protected void log(String s) {
        Log.d(LOG_TAG, "[ApnContext:" + mApnType + "] " + s);
    }

    public void dump(FileDescriptor fd, PrintWriter pw, String[] args) {
        pw.println("ApnContext: " + this.toString());
    }

    public void setTetheredCallOn(boolean tetheredCallOn) {
        if (mDataProfile != null) mDataProfile.setTetheredCallOn(tetheredCallOn);
    }

    public boolean getTetheredCallOn() {
        return mDataProfile == null ? false : mDataProfile.getTetheredCallOn();
    }
}
