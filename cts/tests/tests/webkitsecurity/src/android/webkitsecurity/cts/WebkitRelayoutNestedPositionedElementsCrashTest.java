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

package android.webkitsecurity.cts;

import java.util.Date;

import android.net.Uri;
import android.util.Log;

import junit.framework.Assert;

import java.io.File;
import java.io.FileInputStream;

import android.webkit.WebView;
import android.webkit.WebSettings;
import android.webkit.MimeTypeMap;

import android.cts.util.PollingCheck;

import android.content.Context;
import android.content.res.AssetManager;

import android.test.UiThreadTest;
import android.test.ActivityInstrumentationTestCase2;

import android.webkitsecurity.cts.WebViewStubActivity;

/*
 * This file acts as a template for the generation of other webkit tests.
 * 
 * The contents of the assets/webkitsecuritytests directory will be scanned
 * for html files, and for each one found a new class will be generated based
 * on this template.
 *
 * The specific things that have to be done to this template are:
 *
 *     1. Change the name to Webkit + javify(testname) + Test
 *     2. Change the private TEST_PATH value to the test's name
 *     3. Change the logtag to shellify(testname)
 *     4. Change the constructor name to <classname>
 *     5. Save this as <classname>.java
 *     6. TODO: Remove this comment
 *
 */

public class WebkitRelayoutNestedPositionedElementsCrashTest extends ActivityInstrumentationTestCase2<WebViewStubActivity> {
    private static final String LOGTAG = "WebkitRelayoutNestedPositionedElementsCrashTest";
    private static final String TEST_PATH = "relayout-nested-positioned-elements-crash.html";
    private static final int INITIAL_PROGRESS = 100;
    private static long TEST_TIMEOUT = 20000L;
    private static long TIME_FOR_LAYOUT = 1000L;

    private WebView mWebView;
    private boolean mIsUiThreadDone;

    public WebkitRelayoutNestedPositionedElementsCrashTest() {
        super("android.webkitsecurity.cts", WebViewStubActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mWebView = getActivity().getWebView();
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
    }

    @UiThreadTest
    public void testWebkitCrashes() throws Exception {

        // set up the webview
        mWebView = new WebView(getActivity());
        getActivity().setContentView(mWebView);

        // We need to be able to run JS for most of these tests
        WebSettings settings = mWebView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setJavaScriptCanOpenWindowsAutomatically(true);

        // Get the url for the test
        Log.d(LOGTAG, TEST_PATH);
        String url = "file:///android_asset/" + TEST_PATH;

        Log.d(LOGTAG, url.toString());

        // Run the test
        assertLoadUrlSuccessfully(url);
    }

    private void assertLoadUrlSuccessfully(String url) {
        mWebView.loadUrl(url);
        waitForLoadComplete();
    }

    private void waitForLoadComplete() {
        new PollingCheck(TEST_TIMEOUT) {
            @Override
            protected boolean check() {
                return mWebView.getProgress() == 100;
            }
        }.run();
        try {
            Thread.sleep(TIME_FOR_LAYOUT);
        } catch (InterruptedException e) {
            Log.w(LOGTAG, "waitForLoadComplete() interrupted while sleeping for layout delay.");
        }
    }
}
