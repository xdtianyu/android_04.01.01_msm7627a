/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *        * Redistributions of source code must retain the above copyright
 *           notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above
 *          copyright notice, this list of conditions and the following
 *           disclaimer in the documentation and/or other materials provided
 *           with the distribution.
 *        * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *           contributors may be used to endorse or promote products derived
 *           from this software without specific prior written permission.
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
 */
package com.android.bluetooth.bpp;

import android.os.ParcelUuid;

/**
 * Bluetooth BPP internal constants definition
 */
public class BluetoothBppConstant {
    /** Tag used for debugging/logging */
    public static final boolean DEBUG = true;

    /** Tag used for verbose logging */
    public static final boolean VERBOSE = true;

    public static final String BTBPP_NAME_PREFERENCE = "btbpp_names";

    public static final String BTBPP_CHANNEL_PREFERENCE = "btbpp_channels";

    /** MIME Media Type header */
    public static final String MIME_TYPE_XHTML_Print0_95            = "application/vnd.pwg-xhtml-print+xml:0.95";
    public static final String MIME_TYPE_XHTML_Print1_0             = "application/vnd.pwg-xhtml-print+xml:1.0";
    public static final String MIME_TYPE_XHTML_Print_Inline_Img     = "application/vnd.pwg-multiplexed";
    public static final String MIME_TYPE_Basic_Text     = "text/plain";
    public static final String MIME_TYPE_vCard2_1       = "text/x-vcard:2.1";
    public static final String MIME_TYPE_vCard3_0       = "text/x-vcard:3.0";
    public static final String MIME_TYPE_vCal1_0        = "text/x-vcalendar:1.0";
    public static final String MIME_TYPE_iCal2_0        = "text/calendar:2.0";
    public static final String MIME_TYPE_vMsg1_1        = "text/x-vmessage:1.1";
    public static final String MIME_TYPE_PostScript2    = "application/PostScript:2";
    public static final String MIME_TYPE_PostScript3    = "application/PostScript:3";
    public static final String MIME_TYPE_PCL5E          = "application/vnd.hp-PCL:5E";
    public static final String MIME_TYPE_PCL3C          = "application/vnd.hp-PCL:3C";
    public static final String MIME_TYPE_PDF            = "application/PDF";
    public static final String MIME_TYPE_JPEG           = "image/jpeg";
    public static final String MIME_TYPE_GIF89a         = "image/gif:89A";
    public static final String MIME_TYPE_REF_SIMPLE     = "text/x-ref-simple";
    public static final String MIME_TYPE_REF_XML        = "text/x-ref-xml";
    public static final String MIME_TYPE_REF_LIST       = "text/x-ref-list";
    public static final String MIME_TYPE_SOAP           = "x-obex/bt-SOAP";
    public static final String MIME_TYPE_REFOBJ         = "x-obex/referencedobject";
    public static final String MIME_TYPE_RUI            = "x-obex/RUI";

    /** Constants that indicate the current connection state */
    public static final short DPS_UUID16 = 0x1118; /* Direct Printing */
    public static final short PBR_UUID16 = 0x1119; /* Printing by Reference */
    public static final short RUI_UUID16 = 0x1121; /* Reflected UI */
    public static final short STS_UUID16 = 0x1123; /* Printing Status */
    public static String mSupportedDocs = null;

    /** Direct Printing UUID128 */
    public static final ParcelUuid DPS_UUID128 =
        ParcelUuid.fromString("00001118-0000-1000-8000-00805F9B34FB");

    /** Printing by Reference UUID128 */
    public static final ParcelUuid PBR_UUID128 =
        ParcelUuid.fromString("00001119-0000-1000-8000-00805F9B34FB");

    /** Reflected UI UUID128 */
    public static final ParcelUuid RUI_UUID128 =
        ParcelUuid.fromString("00001121-0000-1000-8000-00805F9B34FB");

    /** Printing Status UUID128 */
    public static final ParcelUuid STS_UUID128 =
        ParcelUuid.fromString("00001123-0000-1000-8000-00805F9B34FB");

    /** Direct Printing OBEX Target UUID */
    public static final byte[] DPS_Target_UUID =
        { (byte) 0x00, (byte) 0x00, (byte) 0x11, (byte) 0x18,
          (byte) 0x00, (byte) 0x00, (byte) 0x10, (byte) 0x00,
          (byte) 0x80, (byte) 0x00, (byte) 0x00, (byte) 0x80,
          (byte) 0x5F, (byte) 0x9B, (byte) 0x34, (byte) 0xFB };

    /** Printing by Reference OBEX Target UUID */
    public static final byte[] PBR_Target_UUID =
        { (byte) 0x00, (byte) 0x00, (byte) 0x11, (byte) 0x19,
          (byte) 0x00, (byte) 0x00, (byte) 0x10, (byte) 0x00,
          (byte) 0x80, (byte) 0x00, (byte) 0x00, (byte) 0x80,
          (byte) 0x5F, (byte) 0x9B, (byte) 0x34, (byte) 0xFB };

    /** Reflected UI OBEX Target UUID */
    public static final byte[] RUI_Target_UUID =
        { (byte) 0x00, (byte) 0x00, (byte) 0x11, (byte) 0x21,
          (byte) 0x00, (byte) 0x00, (byte) 0x10, (byte) 0x00,
          (byte) 0x80, (byte) 0x00, (byte) 0x00, (byte) 0x80,
          (byte) 0x5F, (byte) 0x9B, (byte) 0x34, (byte) 0xFB };

    /** Printing Status OBEX Target UUID */
    public static final byte[] STS_Target_UUID =
        { (byte) 0x00, (byte) 0x00, (byte) 0x11, (byte) 0x23,
          (byte) 0x00, (byte) 0x00, (byte) 0x10, (byte) 0x00,
          (byte) 0x80, (byte) 0x00, (byte) 0x00, (byte) 0x80,
          (byte) 0x5F, (byte) 0x9B, (byte) 0x34, (byte) 0xFB };


    /** BPP OBEX Application Parameter Header Tag: Offset */
    public static final int OBEX_HDR_APP_PARAM_OFFSET = 1;

    /** BPP OBEX Application Parameter Header Tag: Count */
    public static final int OBEX_HDR_APP_PARAM_COUNT = 2;

    /** BPP OBEX Application Parameter Header Tag: JobId */
    public static final int OBEX_HDR_APP_PARAM_JOBID = 3;

    /** BPP OBEX Application Parameter Header Tag: FileSize */
    public static final int OBEX_HDR_APP_PARAM_FILESIZE = 4;

    /**
     * This transfer hasn't stated yet
     */
    public static final int STATUS_PENDING = 190;

    /**
     * This transfer has started
     */
    public static final int STATUS_RUNNING = 192;

    /**
     * This transfer has successfully completed. Warning: there might be other
     * status values that indicate success in the future. Use isSucccess() to
     * capture the entire category.
     */
    public static final int STATUS_SUCCESS = 200;

    /**
     * This request couldn't be parsed. This is also used when processing
     * requests with unknown/unsupported URI schemes.
     */
    public static final int STATUS_BAD_REQUEST = 400;

    /**
     * This transfer is forbidden by target device.
     */
    public static final int STATUS_FORBIDDEN = 403;

    /**
     * This transfer can't be performed because the content cannot be handled.
     */
    public static final int STATUS_NOT_ACCEPTABLE = 406;

    /**
     * This transfer cannot be performed because the length cannot be determined
     * accurately. This is the code for the HTTP error "Length Required", which
     * is typically used when making requests that require a content length but
     * don't have one, and it is also used in the client when a response is
     * received whose length cannot be determined accurately (therefore making
     * it impossible to know when a transfer completes).
     */
    public static final int STATUS_LENGTH_REQUIRED = 411;

    /**
     * This transfer was interrupted and cannot be resumed. This is the code for
     * the OBEX error "Precondition Failed", and it is also used in situations
     * where the client doesn't have an ETag at all.
     */
    public static final int STATUS_PRECONDITION_FAILED = 412;

    /**
     * This transfer was canceled
     */
    public static final int STATUS_CANCELED = 490;

    /**
     * This transfer has completed with an error. Warning: there will be other
     * status values that indicate errors in the future. Use isStatusError() to
     * capture the entire category.
     */
    public static final int STATUS_UNKNOWN_ERROR = 491;

    /**
     * This transfer couldn't be completed because of a storage issue.
     * Typically, that's because the file system is missing or full.
     */
    public static final int STATUS_FILE_ERROR = 492;

    /**
     * This transfer couldn't be completed because of no sdcard.
     */
    public static final int STATUS_ERROR_NO_SDCARD = 493;

    /**
     * This transfer couldn't be completed because of sdcard full.
     */
    public static final int STATUS_ERROR_SDCARD_FULL = 494;

    /**
     * This transfer couldn't be completed because of an unspecified un-handled
     * OBEX code.
     */
    public static final int STATUS_UNHANDLED_OBEX_CODE = 495;

    /**
     * This transfer couldn't be completed because of an error receiving or
     * processing data at the OBEX level.
     */
    public static final int STATUS_OBEX_DATA_ERROR = 496;

    /**
     * This transfer couldn't be completed because of an error when establishing
     * connection.
     */
    public static final int STATUS_CONNECTION_ERROR = 497;


}
