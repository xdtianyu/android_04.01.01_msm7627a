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

import com.android.bluetooth.opp.BluetoothShare;

import android.os.Handler;
import android.util.Log;

/**
 * This class contains SOAP message builder and parser
 */
public class BluetoothBppSoap {
    private static final String TAG = "BluetoothBppSoap";
    private static final boolean D = BluetoothBppConstant.DEBUG;
    private static final boolean V = BluetoothBppConstant.VERBOSE;

    // SOAP Request Command
    public static final String SOAP_REQ_GET_PR_ATTR     = "GetPrinterAttributes";
    public static final String SOAP_REQ_CREATE_JOB      = "CreateJob";
    public static final String SOAP_REQ_GET_JOB_ATTRS   = "GetJobAttributes";
    public static final String SOAP_REQ_GET_EVT         = "GetEvent";
    public static final String SOAP_REQ_CANCEL_JOB      = "CancelJob";

    // Document Formats
    public static final int FORMAT_XHTML0_95        = 0x00000001;
    public static final int FORMAT_XHTML1_0         = 0x00000002;
    public static final int FORMAT_TEXT             = 0x00000004;
    public static final int FORMAT_VCARD2_1         = 0x00000008;
    public static final int FORMAT_VCARD3_0         = 0x00000010;
    public static final int FORMAT_VCAL1_0          = 0x00000020;
    public static final int FORMAT_ICAL2_0          = 0x00000040;
    public static final int FORMAT_VMSG1_1          = 0x00000080;
    public static final int FORMAT_POSTSCRIPT2      = 0x00000100;
    public static final int FORMAT_POSTSCRIPT3      = 0x00000200;
    public static final int FORMAT_PCL5E            = 0x00000400;
    public static final int FORMAT_PCL3C            = 0x00000800;
    public static final int FORMAT_PDF              = 0x00001000;
    public static final int FORMAT_JPEG             = 0x00002000;
    public static final int FORMAT_GIF89A           = 0x00004000;
    public static final int FORMAT_XHTML_INLINE_IMG = 0x00008000;

  /***************************************************************************
     * Operation Status Codes
     **************************************************************************/
    // successful-ok
    public static final int OP_STATUS_OK                                = 0x0000;
    // successful-ok-ignored-or-substituted-attributes
    public static final int OP_STATUS_OK_IGNORED_ATTR                   = 0x0001;
    // successful-ok-conflicting-attributes
    public static final int OP_STATUS_OK_CONFLICT_ATTR                  = 0x0002;
    // client-error-bad-request
    public static final int OP_STATUS_CLI_ERR_BAD_REQ                   = 0x0400;
    // client-error-forbidden
    public static final int OP_STATUS_CLI_ERR_FORBIDDEN                 = 0x0401;
    // client-error-not-authenticated
    public static final int OP_STATUS_CLI_ERR_NOT_AUTHENTICATED         = 0x0402;
    // client-error-not-authorized
    public static final int OP_STATUS_CLI_ERR_NOT_AUTHORIZED            = 0x0403;
    // client-error-not-possible
    public static final int OP_STATUS_CLI_ERR_NOT_POSSIBLE              = 0x0404;
    // client-error-timeout
    public static final int OP_STATUS_CLI_ERR_TIME_OUT                  = 0x0405;
    // client-error-not-found
    public static final int OP_STATUS_CLI_ERR_NOT_FOUND                 = 0x0406;
    // client-error-gone
    public static final int OP_STATUS_CLI_ERR_GONE                      = 0x0407;
    // client-error-request-entity-too-large
    public static final int OP_STATUS_CLI_ERR_ENTITY_TOO_LARGE          = 0x0408;
    // client-error-request-value-too-long
    public static final int OP_STATUS_CLI_ERR_VAL_TOO_LONG              = 0x0409;
    // client-error-document-format-not-supported
    public static final int OP_STATUS_CLI_ERR_NOT_SUPPORTED_FORMAT      = 0x040a;
    // client-error-attributes-or-values-not-supported
    public static final int OP_STATUS_CLI_ERR_NOT_SUPPORTED_VAL         = 0x040b;
    // client-error-uri-scheme-not-supported
    public static final int OP_STATUS_CLI_ERR_NOT_SUPPORTED_URI         = 0x040c;
    // client-error-charset-not-supported
    public static final int OP_STATUS_CLI_ERR_NOT_SUPPORTED_CHARSET     = 0x040d;
    // client-error-conflicting-attributes
    public static final int OP_STATUS_CLI_ERR_CONFLICT_ATTR             = 0x040e;
    // client-error-compression-not-supported
    public static final int OP_STATUS_CLI_ERR_NOT_SUPPORTED_COMPRESSION = 0x040f;
    // client-error-compression-error
    public static final int OP_STATUS_CLI_ERR_COMPRESSION_ERROR         = 0x0410;
    // client-error-document-format-error
    public static final int OP_STATUS_CLI_ERR_DOC_FORMAT_ERR            = 0x0411;
    // client-error-document-access-error
    public static final int OP_STATUS_CLI_ERR_DOC_ACCESS_ERR            = 0x0412;
    // client-error-media-not-loaded
    public static final int OP_STATUS_CLI_ERR_MEDIA_NOT_LOADED          = 0x0418;
    // server-error-internal-error
    public static final int OP_STATUS_SRV_ERR_INT_ERR                   = 0x0500;
    // server-error-operation-not-supported
    public static final int OP_STATUS_SRV_ERR_OP_NOT_SUPPORTED          = 0x0501;
    // server-error-service-unavailable
    public static final int OP_STATUS_SRV_ERR_SVC_UNAVAILABLE           = 0x0502;
    // server-error-version-not-supported
    public static final int OP_STATUS_SRV_ERR_VER_NOT_SUPPORTED         = 0x0503;
    // server-error-device-error
    public static final int OP_STATUS_SRV_ERR_DEVICE_ERR                = 0x0504;
    // server-error-temporary-error
    public static final int OP_STATUS_SRV_ERR_TEMP_ERR                  = 0x0505;
    // server-error-not-accepting-jobs
    public static final int OP_STATUS_SRV_ERR_NOT_ACCEPT_JOBS           = 0x0506;
    // server-error-busy
    public static final int OP_STATUS_SRV_ERR_BUSY                      = 0x0507;
    // server-error-job-canceled
    public static final int OP_STATUS_SRV_ERR_JOB_CANCEL                = 0x0508;
    // server-error-multiple-document-jobs-not-supported
    public static final int OP_STATUS_SRV_ERR_MULTI_DOC_JOBS_NOT_SUPPORTED = 0x0509;

    // Parsed database
    public byte[] bppJobId = null;
    public int bppOperStatusCode;
    public String mSoapBodyLen = null;

    // SOAP Printer Attributes
    public String    mPrinter_Name           = null;
    public String    mPrinter_State          = null;
    public String    mPrinter_StateReasons   = null;
    public int       mPrinter_DocFormats     = 0;
    public Boolean   mPrinter_ColorSupported = false;
    public int       mPrinter_MaxCopies      = 0;
    public String[]  mPrinter_Sides          = null;
    public int       mPrinter_NumberUp       = 0;
    public String[]  mPrinter_Orientation    = null;
    public String    mPrinter_JobId          = null;
    public boolean   mFileSending            = false;
    Handler mCallback;
    public String mPrinterStateReason;
    public String mOperationStatus;
    public String mJobStatus;
    public boolean mDocumentSent;
    public int mJobResult;

    public BluetoothBppSoap(Handler handle, String JobId){
        mCallback = handle;
        mPrinterStateReason = "none";
        mOperationStatus = "0x0000";
        mJobStatus = "waiting";
        mDocumentSent = false;
        mPrinter_JobId = JobId;
        mJobResult = BluetoothShare.STATUS_SUCCESS;
    }

    /**
     * SOAP Message Parser
     * @param SoapReq Soap Request for current Soap Response
     * @param SoapRsp Soap Response String data
     * @return When it return true, it will invoke "ABORT" OBEX request to stop GetEvent Operation.
     */
    synchronized public boolean Parser(String SoapReq, String SoapRsp){

        if (V) Log.v(TAG, "Parsing SOAP Response...");

        boolean result = false;

        // GetPrinterAttributes SOAP Response
        if( SoapReq.compareTo(SOAP_REQ_GET_PR_ATTR) == 0 ){
            // PrinterState
            String paramString[] = new String[20];
            int countParam = 0;

            // PrinterName
            if( (countParam = ExtractParameter(SoapRsp, "PrinterName", paramString )) != 0){
                mPrinter_Name = paramString[0];
            }
            // PrinterState
            if( (countParam = ExtractParameter(SoapRsp, "PrinterState", paramString )) != 0){
                mPrinter_State = paramString[0];
            }
            // PrinterStateReasons
            if( (countParam = ExtractParameter(SoapRsp, "PrinterStateReasons",
                paramString )) != 0){
                mPrinter_StateReasons = paramString[0];
            }
            // DocumentFormat
            if( (countParam = ExtractParameter(SoapRsp, "DocumentFormat", paramString )) != 0){
                mPrinter_DocFormats = 0;
                for(int i=0; i< countParam; i++){
                    AddPrintDocFormat(paramString[i]);
                }
            }
            //ColorSupported
            if( (countParam = ExtractParameter(SoapRsp, "ColorSupported", paramString )) != 0){
                mPrinter_ColorSupported = Boolean.valueOf(paramString[0]);
            }
            // MaxCopiesSupported
            if( (countParam = ExtractParameter(SoapRsp, "MaxCopiesSupported", paramString )) != 0){
                mPrinter_MaxCopies = Integer.valueOf(paramString[0]);
            }
            // Sides
            if( (countParam = ExtractParameter(SoapRsp, "Sides", mPrinter_Sides )) != 0){

            }
            // NumberUpSupported
            if( (countParam = ExtractParameter(SoapRsp, "NumberUpSupported", paramString )) != 0){
                mPrinter_NumberUp = Integer.valueOf(paramString[0]);
            }
            // Orientation
            if( (countParam = ExtractParameter(SoapRsp, "Orientation",
                                                                    mPrinter_Orientation )) != 0){

            }
        }
        // CreateJobResponse SOAP Response
        else if( SoapReq.compareTo(SOAP_REQ_CREATE_JOB) == 0 ){
            String paramString[] = new String[20];

            // JobId
            if(ExtractParameter(SoapRsp, "JobId", paramString ) != 0) {
                mPrinter_JobId = paramString[0];
                int tempId = Integer.parseInt(paramString[0]);

                int i =0;
                // bppJobId is valid only when it get CreateJob Response.
                bppJobId = new byte[4];

                do{
                    bppJobId[i++] =  (byte) (tempId & 0xFF) ;
                }while((tempId = ((tempId >> 8) & 0xFF)) != 0);

                for(int j=0; j< i; j++){
                    if(V) Log.v(TAG, "bppJobId : " + bppJobId[j] );
                }

                mCallback.obtainMessage(
                BluetoothBppTransfer.START_EVENT_THREAD, -1).sendToTarget();
            }

            // OperationStatus
            ExtractParameter(SoapRsp, "OperationStatus", paramString );
            bppOperStatusCode = Integer.decode(paramString[0]);
            if(V) Log.v(TAG, "bppOperStatusCode : 0x" + Integer.toHexString(bppOperStatusCode));

        }
        else if( SoapReq.compareTo(SOAP_REQ_CANCEL_JOB) == 0 ){
            String paramString[] = new String[2];

            ExtractParameter(SoapRsp, "JobId", paramString );
            ExtractParameter(SoapRsp, "OperationStatus", paramString );
            mCallback.obtainMessage(
                        BluetoothBppObexClientSession.MSG_SESSION_EVENT_COMPLETE,
                        1, -1).sendToTarget();
            result = true;

        }
        else if( SoapReq.compareTo(SOAP_REQ_GET_EVT) == 0 ){
            if(mDocumentSent == false){
                mDocumentSent = true;
                mCallback.obtainMessage(
                        BluetoothBppTransfer.SEND_DOCUMENT, -1).sendToTarget();
            }

            String paramString[] = new String[20];
            if(ExtractParameter(SoapRsp, "JobState", paramString ) != 0){
                if(V) Log.v(TAG, "Current Job Status : " +  paramString[0]);
                setStatus(paramString[0]);
                if((paramString[0].compareTo("stopped") == 0) ||
                       (paramString[0].compareTo("aborted") == 0) ||
                       (paramString[0].compareTo("cancelled") == 0) ||
                       (paramString[0].compareTo("unknown") == 0)) {
                    mCallback.obtainMessage(
                        BluetoothBppTransfer.CANCEL, mJobResult, -1).sendToTarget();
                    result = true;
                }

                if(paramString[0].compareTo("completed") == 0) {
                    mCallback.obtainMessage(
                    BluetoothBppObexClientSession.MSG_SESSION_EVENT_COMPLETE, -1).sendToTarget();
                    result = true;
                 }
            }

            if(ExtractParameter(SoapRsp, "PrinterState", paramString ) != 0){
                setStatus(paramString[0]);
                if(paramString[0].compareTo("stopped") == 0) {
                    mCallback.obtainMessage(
                            BluetoothBppTransfer.CANCEL, mJobResult, -1).sendToTarget();
                    result = true;
                }
            }

            if(ExtractParameter(SoapRsp, "PrinterStateReasons", paramString ) != 0){
                setStatus(paramString[0]);
                if(!((paramString[0].compareTo("none") == 0) ||
                        (paramString[0].compareTo("None") == 0))) {
                    mCallback.obtainMessage(
                            BluetoothBppTransfer.CANCEL, mJobResult, -1).sendToTarget();
                    result = true;
                }
            }

            if(ExtractParameter(SoapRsp, "OperationStatus", paramString ) != 0){
                mOperationStatus = paramString[0];
                if(paramString[0].compareTo("0x0000") != 0)
                {
                    mJobResult = BluetoothShare.STATUS_BPP_REFUSED_BY_PRINTER;
                    mCallback.obtainMessage(
                            BluetoothBppTransfer.CANCEL, mJobResult, -1).sendToTarget();
                    result = true;
                }
            }
        }
        else{
            if(V) Log.v(TAG, "Unexpected SOAP Response ");
        }

        return result;
    }

    private void setStatus(String status) {
        if (status.compareTo("media-jam") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_MEDIA_JAM;
        } else if (status.compareTo("paused") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_PAUSED;
        } else if (status.compareTo("door-open") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_DOOR_OPEN;
        } else if (status.compareTo("media-low") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_MEDIA_LOW;
        } else if (status.compareTo("media-empty") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_MEDIA_EMPTY;
        } else if (status.compareTo("output-area-almost-full") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_OUTPUT_AREA_ALMOST_FULL;
        } else if (status.compareTo("output-area-full") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_OUTPUT_AREA_FULL;
        } else if (status.compareTo("marker-supply-low") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_MARKER_SUPPLY_LOW;
        } else if (status.compareTo("marker-supply-empty") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_MARKER_SUPPLY_EMPTY;
        } else if (status.compareTo("marker-failure") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_MARKER_FAILURE;
        } else if (status.compareTo("stopped") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_STOPPED_BY_PRINTER;
        } else if (status.compareTo("aborted") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_ABORTED_BY_PRINTER;
        } else if (status.compareTo("cancelled") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_CANCELED_BY_PRINTER;
        } else if (status.compareTo("unknown") == 0) {
            mJobResult = BluetoothShare.STATUS_BPP_UNKNOWN_ERROR_BY_PRINTER;
        }
    }

    private int ExtractParameter(String SoapRsp, String AttrName, String[] paramStr ){
        int startIdx = 0;
        int endIdx = 0;
        int paramCount = 0;
        int paramLen = 0;
        String AttrPrefix;
        String AttrSuffix;

        if(V) Log.v(TAG, "Extract Attribute: " + AttrName);

        AttrPrefix = "<" + AttrName + ">";
        AttrSuffix = "</" + AttrName + ">";

        // First, check if the parameter is valid in the response
        while(((startIdx = SoapRsp.indexOf(AttrPrefix, startIdx)) != -1) &&
            ((endIdx   = SoapRsp.indexOf(AttrSuffix, endIdx)  ) != -1)){

            startIdx += AttrPrefix.length();
            paramLen = endIdx - startIdx;
            endIdx   += AttrSuffix.length();
            byte stringBuf[] = new byte[paramLen];

            SoapRsp.getBytes(startIdx, startIdx+ paramLen, stringBuf, 0);
            paramStr[paramCount] = new String(stringBuf);

            if(V) Log.v(TAG, "Value: " + paramStr[paramCount]);
            paramCount++;
        }
        return paramCount;
    }

    private void AddPrintDocFormat(String DocFormat){

        if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_XHTML_Print0_95) == 0){
            mPrinter_DocFormats |= FORMAT_XHTML0_95;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_XHTML_Print1_0) == 0){
            mPrinter_DocFormats |= FORMAT_XHTML1_0;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_XHTML_Print_Inline_Img) == 0){
            mPrinter_DocFormats |= FORMAT_XHTML_INLINE_IMG;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_Basic_Text) == 0){
            mPrinter_DocFormats |= FORMAT_TEXT;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_vCard2_1) == 0){
            mPrinter_DocFormats |= FORMAT_VCARD2_1;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_vCard3_0) == 0){
            mPrinter_DocFormats |= FORMAT_VCARD3_0;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_vCal1_0) == 0){
            mPrinter_DocFormats |= FORMAT_VCAL1_0;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_iCal2_0) == 0){
            mPrinter_DocFormats |= FORMAT_ICAL2_0;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_vMsg1_1) == 0){
            mPrinter_DocFormats |= FORMAT_VMSG1_1;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_PostScript2) == 0){
            mPrinter_DocFormats |= FORMAT_POSTSCRIPT2;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_PostScript3) == 0){
            mPrinter_DocFormats |= FORMAT_POSTSCRIPT3;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_PCL5E) == 0){
            mPrinter_DocFormats |= FORMAT_PCL5E;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_PCL3C) == 0){
            mPrinter_DocFormats |= FORMAT_PCL3C;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_PDF) == 0){
            mPrinter_DocFormats |= FORMAT_PDF;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_JPEG) == 0){
            mPrinter_DocFormats |= FORMAT_JPEG;
        }
        else if (DocFormat.compareTo(BluetoothBppConstant.MIME_TYPE_GIF89a) == 0){
            mPrinter_DocFormats |= FORMAT_GIF89A;
        }
    }

    public boolean CheckPrintDocFormat(String FileName){
        int PrintSupported = 0;

        if(FileName.lastIndexOf(".txt") != -1){
            PrintSupported = mPrinter_DocFormats & (FORMAT_TEXT);
        }
        else if(FileName.lastIndexOf(".pdf") != -1){
            PrintSupported = mPrinter_DocFormats & (FORMAT_PDF);
        }
        else if(FileName.lastIndexOf(".vcf") != -1){
            PrintSupported = mPrinter_DocFormats & (FORMAT_VCARD2_1 | FORMAT_VCARD3_0);
        }
        else if(FileName.lastIndexOf(".vcal") != -1){
            PrintSupported = mPrinter_DocFormats & (FORMAT_VCAL1_0);
        }
        else if(FileName.lastIndexOf(".jpg") != -1){
            PrintSupported = mPrinter_DocFormats & (FORMAT_JPEG);
        }
        else if(FileName.lastIndexOf(".jpeg") != -1){
            PrintSupported = mPrinter_DocFormats & (FORMAT_JPEG);
        }
        else if(FileName.lastIndexOf(".gif") != -1){
            PrintSupported = mPrinter_DocFormats & (FORMAT_GIF89A);
        }
        else{
            if (V) Log.v(TAG, "Soap Parser: Unkwon File extension");
        }
        return ((PrintSupported == 0)? false: true);
    }

    /**
     * SOAP Message Builder
     * @param SoapCmd Soap Request String value which can add into Soap message.
     * @return It will return completed SOAP request message.
     */
    synchronized public String Builder(String SoapCmd){
        String SoapReqHeader = null;
        String SoapReqBody = null;
        String SoapReq = null;
        long timestamp;

        timestamp = System.currentTimeMillis();

        if((SoapReqBody = CreateSoapBody(SoapCmd))!= null ){
            mSoapBodyLen = Integer.toString(SoapReqBody.length());
            SoapReqHeader = CreateSoapHeader(SoapCmd);
            SoapReq = SoapReqHeader + SoapReqBody;

            if (V) Log.v(TAG, "Soap Builder created: " +
                (System.currentTimeMillis()- timestamp) + "ms\r\n" + SoapReq
                + "Soap Body Length: " + mSoapBodyLen );
        }
        return SoapReq;
    }

    private String CreateSoapHeader(String SoapCmd){
        String SoapHdr = null;

        SoapHdr =
            "CONTENT-LENGTH: " + mSoapBodyLen +"\r\n"
            + "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
            + "SOAPACTION: \"urn:schemas-bluetooth-org:service:Printer:1#" + SoapCmd + "\"\r\n\r\n";
        return SoapHdr;
    }

    private String CreateSoapBody(String SoapCmd){
        String SoapBodyBegin 	= null;
        String SoapBodyMain 	= null;
        String SoapBodyEnd      = null;

        SoapBodyBegin =
           "<s:Envelope\r\n"
            + "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"\r\n"
            + "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
            + "<s:Body>\r\n"
            + "<u:" + SoapCmd +" xmlns:u=\"urn:schemas-bluetooth-org:service:Printer:1\">\r\n";

        //GetPrinterAttributes
        if(SoapCmd.compareTo(SOAP_REQ_GET_PR_ATTR) == 0){
            SoapBodyMain =
                "<RequestedPrinterAttributes>\r\n"
                + "<PrinterAttribute>PrinterName</PrinterAttribute>\r\n"
                + "<PrinterAttribute>PrinterState</PrinterAttribute>\r\n"
                + "<PrinterAttribute>PrinterStateReasons</PrinterAttribute>\r\n"
                + "<PrinterAttribute>DocumentFormatsSupported</PrinterAttribute>\r\n"
                + "<PrinterAttribute>ColorSupported</PrinterAttribute>\r\n"
                + "<PrinterAttribute>MaxCopiesSupported</PrinterAttribute>\r\n"
                + "<PrinterAttribute>SideSupported</PrinterAttribute>\r\n"
                + "<PrinterAttribute>NumberUpSupported</PrinterAttribute>\r\n"
                + "<PrinterAttribute>OrientationSupported</PrinterAttribute>\r\n"
                + "<PrinterAttribute>OperationStatus</PrinterAttribute>\r\n"
            + "</RequestedPrinterAttributes>\r\n";
        }
        else if(SoapCmd.compareTo(SOAP_REQ_CREATE_JOB) == 0){
            // Need to be changed by UI request
            SoapBodyMain =
                "<JobName>"                         + "MyJob"
                + "</JobName>\r\n"
                + "<Copies>"                        + BluetoothBppPrintPrefActivity.mCopies
                + "</Copies>\r\n"
                + "<Sides>"                         + BluetoothBppPrintPrefActivity.mSides
                + "</Sides>\r\n"
                + "<NumberUp>"                      + BluetoothBppPrintPrefActivity.mNumUp
                + "</NumberUp>\r\n"
                + "<OrientationRequested>"          + BluetoothBppPrintPrefActivity.mOrient
                + "</OrientationRequested>\r\n";
        }
        else if ((SoapCmd.compareTo(SOAP_REQ_CANCEL_JOB) == 0)
             || (SoapCmd.compareTo(SOAP_REQ_GET_EVT) == 0)) {
            SoapBodyMain =
                    "<JobId>"                       + mPrinter_JobId
                    + "</JobId>\r\n";
        }
        else{
            if (V) Log.v(TAG, "Soap Builder: \"" + SoapCmd + "\" is not supported Request");
        }

        SoapBodyEnd =
            "</u:" + SoapCmd + ">\r\n"
            + "</s:Body>\r\n"
            + "</s:Envelope>\r\n";
        return((SoapBodyMain == null)? null:(SoapBodyBegin + SoapBodyMain + SoapBodyEnd));
    }
}
