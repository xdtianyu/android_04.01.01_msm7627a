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

package com.android.cts.writeexternalstorageapp;

import android.os.Environment;
import android.test.AndroidTestCase;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Test if {@link Environment#getExternalStorageDirectory()} is writable.
 */
public class WriteExternalStorageTest extends AndroidTestCase {

    private static final String TEST_FILE = "meow";

    private void assertExternalStorageMounted() {
        assertEquals(Environment.MEDIA_MOUNTED, Environment.getExternalStorageState());
    }

    private void readExternalStorage() throws IOException {
        final File file = new File(Environment.getExternalStorageDirectory(), TEST_FILE);
        final InputStream is = new FileInputStream(file);
        try {
            is.read();
        } finally {
            is.close();
        }
    }

    private void writeExternalStorage() throws IOException {
        final File file = new File(Environment.getExternalStorageDirectory(), TEST_FILE);
        final OutputStream os = new FileOutputStream(file);
        try {
            os.write(32);
        } finally {
            os.close();
        }
    }

    public void testReadExternalStorage() throws Exception {
        assertExternalStorageMounted();
        try {
            readExternalStorage();
        } catch (IOException e) {
            fail("unable to read external file");
        }
    }

    public void testWriteExternalStorage() throws Exception {
        assertExternalStorageMounted();
        try {
            writeExternalStorage();
        } catch (IOException e) {
            fail("unable to read external file");
        }
    }

}
