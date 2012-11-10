/*
 * Copyright (C) 2007 The Android Open Source Project
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

package com.android.quake.llvm;

import android.app.Activity;
import android.content.res.Resources;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuInflater;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import android.view.View;


public class QuakeActivity extends Activity {

    QuakeView mQuakeView;

    static QuakeLib mQuakeLib;

    boolean mKeepScreenOn = true;

    @Override protected void onCreate(Bundle icicle) {
        Log.i("QuakeActivity", "onCreate");
        super.onCreate(icicle);
        if (USE_DOWNLOADER) {
            if (! DownloaderActivity.ensureDownloaded(this,
                    getString(R.string.quake_customDownloadText), FILE_CONFIG_URL,
                    CONFIG_VERSION, SDCARD_DATA_PATH, USER_AGENT)) {
                return;
            }
        }

        if (foundQuakeData()) {
          // HACK: create faked view in order to read bitcode in resource
                View view = new View(getApplication());
                byte[] pgm;
                int pgmLength;

            // read bitcode in res
            InputStream is = view.getResources().openRawResource(R.raw.libquake_portable);
            try {
               try {
                  pgm = new byte[1024];
                  pgmLength = 0;

                  while(true) {
                     int bytesLeft = pgm.length - pgmLength;
                     if (bytesLeft == 0) {
                         byte[] buf2 = new byte[pgm.length * 2];
                         System.arraycopy(pgm, 0, buf2, 0, pgm.length);
                         pgm = buf2;
                         bytesLeft = pgm.length - pgmLength;
                     }
                     int bytesRead = is.read(pgm, pgmLength, bytesLeft);
                     if (bytesRead <= 0) {
                        break;
                     }
                     pgmLength += bytesRead;
    	          }
               } finally {
                  is.close();
               }
            } catch(IOException e) {
               throw new Resources.NotFoundException();
            }

          //
            if (mQuakeLib == null) {
                mQuakeLib = new QuakeLib(pgm, pgmLength);
                if(! mQuakeLib.init()) {
                    setContentView(new QuakeViewNoData(
                            getApplication(),
                            QuakeViewNoData.E_INITFAILED));
                    return;
                }
            }

            if (mKeepScreenOn) {
                getWindow().setFlags(
                        WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                        WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }

            if (mQuakeView == null) {
                mQuakeView = new
                QuakeView(getApplication());
                mQuakeView.setQuakeLib(mQuakeLib);
            }
            setContentView(mQuakeView);
        }
        else {
            setContentView(new QuakeViewNoData(getApplication(),
                            QuakeViewNoData.E_NODATA));
        }
    }

    @Override protected void onPause() {
        super.onPause();
        if (mQuakeView != null) {
            mQuakeView.onPause();
        }
    }

    @Override protected void onResume() {
        super.onResume();
        if (mQuakeView != null) {
            mQuakeView.onResume();
        }
    }

    @Override protected void onDestroy() {
        super.onDestroy();
        if (mQuakeLib != null) {
            mQuakeLib.quit();
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.switch_mode:
                if (mQuakeView != null) {
                    mQuakeView.switchMode();
                }
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    private boolean foundQuakeData() {
        return fileExists(SDCARD_DATA_PATH + PAK0_PATH)
                || fileExists(INTERNAL_DATA_PATH + PAK0_PATH);
    }

    private boolean fileExists(String s) {
        File f = new File(s);
        return f.exists();
    }

    private final static boolean USE_DOWNLOADER = false;

    private final static String FILE_CONFIG_URL =
        "http://example.com/android/quake/quake11.config";
    private final static String CONFIG_VERSION = "1.1";
    private final static String SDCARD_DATA_PATH = "/sdcard/data/quake";
    private final static String INTERNAL_DATA_PATH = "/data/quake";
    private final static String PAK0_PATH = "/id1/pak0.pak";
    private final static String USER_AGENT = "Android Quake Downloader";

}
