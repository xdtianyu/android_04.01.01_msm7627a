/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.provider.cts;


import android.content.ContentResolver;
import android.content.res.Configuration;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Settings.SettingNotFoundException;
import android.provider.Settings.System;
import android.test.AndroidTestCase;

public class Settings_SystemTest extends AndroidTestCase {
    private ContentResolver cr;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        cr = mContext.getContentResolver();
        assertNotNull(cr);
    }

    public void testSystemSettings() throws SettingNotFoundException {
        /**
         * first query the exist settings in System table, and then insert six
         * rows: an int, a long, a float, a String, a configuration and a
         * ShowGTalkServiceStatus. Get these six rows to check whether insert
         * success and then delete them.
         */
        // first query exist rows
        Cursor c = cr.query(System.CONTENT_URI, null, null, null, null);
        try {
            assertNotNull(c);
            int origCount = c.getCount();
            c.close();

            String intField = "IntField";
            String longField = "LongField";
            String floatField = "FloatField";
            String stringField = "StringField";
            String stringValue = "cts";

            // insert 5 rows, and update 1 rows
            assertTrue(System.putInt(cr, intField, 10));
            assertTrue(System.putLong(cr, longField, 20l));
            assertTrue(System.putFloat(cr, floatField, 30.0f));
            assertTrue(System.putString(cr, stringField, stringValue));
            System.setShowGTalkServiceStatus(cr, true);

            c = cr.query(System.CONTENT_URI, null, null, null, null);
            assertNotNull(c);
            assertEquals(origCount + 5, c.getCount());
            c.close();

            // get these rows to assert
            assertEquals(10, System.getInt(cr, intField));
            assertEquals(20l, System.getLong(cr, longField));
            assertEquals(30.0f, System.getFloat(cr, floatField), 0.001);

            assertEquals(stringValue, System.getString(cr, stringField));
            assertTrue(System.getShowGTalkServiceStatus(cr));

            // delete these rows
            String selection = System.NAME + "=\"" + intField + "\"";
            cr.delete(System.CONTENT_URI, selection, null);

            selection = System.NAME + "=\"" + longField + "\"";
            cr.delete(System.CONTENT_URI, selection, null);

            selection = System.NAME + "=\"" + floatField + "\"";
            cr.delete(System.CONTENT_URI, selection, null);

            selection = System.NAME + "=\"" + stringField + "\"";
            cr.delete(System.CONTENT_URI, selection, null);

            selection = System.NAME + "=\"" + System.SHOW_GTALK_SERVICE_STATUS + "\"";
            cr.delete(System.CONTENT_URI, selection, null);

            c = cr.query(System.CONTENT_URI, null, null, null, null);
            assertNotNull(c);
            assertEquals(origCount, c.getCount());

            // backup fontScale
            Configuration cfg = new Configuration();
            System.getConfiguration(cr, cfg);
            float store = cfg.fontScale;

            // update fontScale row
            cfg = new Configuration();
            cfg.fontScale = 10.0f;
            assertTrue(System.putConfiguration(cr, cfg));

            System.getConfiguration(cr, cfg);
            assertEquals(10.0f, cfg.fontScale);

            // restore the fontScale
            cfg.fontScale = store;
            assertTrue(System.putConfiguration(cr, cfg));
        } finally {
            // TODO should clean up more better
            c.close();
        }
    }

    public void testGetDefaultValues() {
        assertEquals(10, System.getInt(cr, "int", 10));
        assertEquals(20, System.getLong(cr, "long", 20l));
        assertEquals(30.0f, System.getFloat(cr, "float", 30.0f), 0.001);
    }

    public void testGetUriFor() {
        String name = "table";

        Uri uri = System.getUriFor(name);
        assertNotNull(uri);
        assertEquals(Uri.withAppendedPath(System.CONTENT_URI, name), uri);
    }
}
