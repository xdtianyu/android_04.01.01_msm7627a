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

package com.android.cellbroadcastreceiver;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.FragmentManager;
import android.app.ListFragment;
import android.app.LoaderManager;
import android.app.NotificationManager;
import android.content.Context;
import android.content.CursorLoader;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.Loader;
import android.database.Cursor;
import android.os.Bundle;
import android.provider.Telephony;
import android.telephony.CellBroadcastMessage;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;
import android.view.ViewGroup;
import android.widget.CursorAdapter;
import android.widget.ListView;

/**
 * This activity provides a list view of received cell broadcasts. Most of the work is handled
 * in the inner CursorLoaderListFragment class.
 */
public class CellBroadcastListActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Dismiss the notification that brought us here (if any).
        ((NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE))
                .cancel(CellBroadcastAlertService.NOTIFICATION_ID);

        FragmentManager fm = getFragmentManager();

        // Create the list fragment and add it as our sole content.
        if (fm.findFragmentById(android.R.id.content) == null) {
            CursorLoaderListFragment listFragment = new CursorLoaderListFragment();
            fm.beginTransaction().add(android.R.id.content, listFragment).commit();
        }
    }

    /**
     * List fragment queries SQLite database on worker thread.
     */
    public static class CursorLoaderListFragment extends ListFragment
            implements LoaderManager.LoaderCallbacks<Cursor> {

        // IDs of the main menu items.
        private static final int MENU_DELETE_ALL           = 3;
        private static final int MENU_PREFERENCES          = 4;

        // IDs of the context menu items (package local, accessed from inner DeleteThreadListener).
        static final int MENU_DELETE               = 0;
        static final int MENU_VIEW                 = 1;

        // This is the Adapter being used to display the list's data.
        CursorAdapter mAdapter;

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            // We have a menu item to show in action bar.
            setHasOptionsMenu(true);
        }

        @Override
        public View onCreateView(LayoutInflater inflater, ViewGroup container,
                Bundle savedInstanceState) {
            return inflater.inflate(R.layout.cell_broadcast_list_screen, container, false);
        }

        @Override
        public void onActivityCreated(Bundle savedInstanceState) {
            super.onActivityCreated(savedInstanceState);

            // Set context menu for long-press.
            ListView listView = getListView();
            listView.setOnCreateContextMenuListener(mOnCreateContextMenuListener);

            // Create a cursor adapter to display the loaded data.
            mAdapter = new CellBroadcastCursorAdapter(getActivity(), null);
            setListAdapter(mAdapter);

            // Prepare the loader.  Either re-connect with an existing one,
            // or start a new one.
            getLoaderManager().initLoader(0, null, this);
        }

        @Override
        public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
            menu.add(0, MENU_DELETE_ALL, 0, R.string.menu_delete_all).setIcon(
                    android.R.drawable.ic_menu_delete);
            menu.add(0, MENU_PREFERENCES, 0, R.string.menu_preferences).setIcon(
                    android.R.drawable.ic_menu_preferences);
        }

        @Override
        public void onPrepareOptionsMenu(Menu menu) {
            menu.findItem(MENU_DELETE_ALL).setVisible(!mAdapter.isEmpty());
        }

        @Override
        public void onListItemClick(ListView l, View v, int position, long id) {
            CellBroadcastListItem cbli = (CellBroadcastListItem) v;
            showDialogAndMarkRead(cbli.getMessage());
        }

        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            return new CursorLoader(getActivity(), CellBroadcastContentProvider.CONTENT_URI,
                    Telephony.CellBroadcasts.QUERY_COLUMNS, null, null,
                    Telephony.CellBroadcasts.DELIVERY_TIME + " DESC");
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor data) {
            // Swap the new cursor in.  (The framework will take care of closing the
            // old cursor once we return.)
            mAdapter.swapCursor(data);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            // This is called when the last Cursor provided to onLoadFinished()
            // above is about to be closed.  We need to make sure we are no
            // longer using it.
            mAdapter.swapCursor(null);
        }

        private void showDialogAndMarkRead(CellBroadcastMessage cbm) {
            // show emergency alerts with the warning icon, but don't play alert tone
            Intent i = new Intent(getActivity(), CellBroadcastAlertDialog.class);
            i.putExtra(CellBroadcastMessage.SMS_CB_MESSAGE_EXTRA, cbm);
            startActivity(i);
        }

        private final OnCreateContextMenuListener mOnCreateContextMenuListener =
                new OnCreateContextMenuListener() {
                    @Override
                    public void onCreateContextMenu(ContextMenu menu, View v,
                            ContextMenuInfo menuInfo) {
                        menu.setHeaderTitle(R.string.message_options);
                        menu.add(0, MENU_VIEW, 0, R.string.menu_view);
                        menu.add(0, MENU_DELETE, 0, R.string.menu_delete);
                    }
                };

        @Override
        public boolean onContextItemSelected(MenuItem item) {
            Cursor cursor = mAdapter.getCursor();
            if (cursor != null && cursor.getPosition() >= 0) {
                switch (item.getItemId()) {
                    case MENU_DELETE:
                        // We need to decrement the unread alert count if deleting unread alert
                        boolean isUnread = (cursor.getInt(cursor.getColumnIndexOrThrow(
                                Telephony.CellBroadcasts.MESSAGE_READ)) == 0);
                        confirmDeleteThread(cursor.getLong(cursor.getColumnIndexOrThrow(
                                Telephony.CellBroadcasts._ID)), isUnread);
                        break;

                    case MENU_VIEW:
                        showDialogAndMarkRead(CellBroadcastMessage.createFromCursor(cursor));
                        break;

                    default:
                        break;
                }
            }
            return super.onContextItemSelected(item);
        }

        @Override
        public boolean onOptionsItemSelected(MenuItem item) {
            switch(item.getItemId()) {
                case MENU_DELETE_ALL:
                    confirmDeleteThread(-1, false);
                    break;

                case MENU_PREFERENCES:
                    Intent intent = new Intent(getActivity(), CellBroadcastSettings.class);
                    startActivity(intent);
                    break;

                default:
                    return true;
            }
            return false;
        }

        /**
         * Start the process of putting up a dialog to confirm deleting a broadcast.
         * @param rowId the row ID of the broadcast to delete, or -1 to delete all broadcasts
         * @param unread true if the alert was not already marked as read
         */
        public void confirmDeleteThread(long rowId, boolean unread) {
            DeleteThreadListener listener = new DeleteThreadListener(rowId, unread);
            confirmDeleteThreadDialog(listener, (rowId == -1), getActivity());
        }

        /**
         * Build and show the proper delete broadcast dialog. The UI is slightly different
         * depending on whether there are locked messages in the thread(s) and whether we're
         * deleting a single broadcast or all broadcasts.
         * @param listener gets called when the delete button is pressed
         * @param deleteAll whether to show a single thread or all threads UI
         * @param context used to load the various UI elements
         */
        public static void confirmDeleteThreadDialog(DeleteThreadListener listener,
                boolean deleteAll, Context context) {
            AlertDialog.Builder builder = new AlertDialog.Builder(context);
            builder.setIcon(android.R.drawable.ic_dialog_alert)
                    .setCancelable(true)
                    .setPositiveButton(R.string.button_delete, listener)
                    .setNegativeButton(R.string.button_cancel, null)
                    .setMessage(deleteAll ? R.string.confirm_delete_all_broadcasts
                            : R.string.confirm_delete_broadcast)
                    .show();
        }

        public class DeleteThreadListener implements OnClickListener {
            private final long mRowId;
            private final boolean mIsUnread;

            public DeleteThreadListener(long rowId, boolean unread) {
                mRowId = rowId;
                mIsUnread = unread;
            }

            @Override
            public void onClick(DialogInterface dialog, int whichButton) {
                // delete from database on a background thread
                new CellBroadcastContentProvider.AsyncCellBroadcastTask(
                        getActivity().getContentResolver()).execute(
                        new CellBroadcastContentProvider.CellBroadcastOperation() {
                            @Override
                            public boolean execute(CellBroadcastContentProvider provider) {
                                if (mRowId != -1) {
                                    return provider.deleteBroadcast(mRowId, mIsUnread);
                                } else {
                                    return provider.deleteAllBroadcasts();
                                }
                            }
                        });

                dialog.dismiss();
            }
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (intent == null) {
            return;
        }

        Bundle extras = intent.getExtras();
        if (extras == null) {
            return;
        }

        CellBroadcastMessage cbm = extras.getParcelable(CellBroadcastMessage.SMS_CB_MESSAGE_EXTRA);
        int notificationId = extras.getInt(CellBroadcastAlertService.SMS_CB_NOTIFICATION_ID_EXTRA);

        // Dismiss the notification that brought us here.
        NotificationManager notificationManager =
            (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.cancel(notificationId);

        // launch the dialog activity to show the alert
        Intent i = new Intent(this, CellBroadcastAlertDialog.class);
        i.putExtra(CellBroadcastMessage.SMS_CB_MESSAGE_EXTRA, cbm);
        startActivity(i);
    }
}
