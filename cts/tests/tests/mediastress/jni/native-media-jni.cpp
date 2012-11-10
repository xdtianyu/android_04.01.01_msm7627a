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

/* Original code copied from NDK Native-media sample code */


#include <assert.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

// for __android_log_print(ANDROID_LOG_INFO, "YourApp", "formatted message");
#include <android/log.h>
#define TAG "NativeMedia"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
// for native media
#include <OMXAL/OpenMAXAL.h>
#include <OMXAL/OpenMAXAL_Android.h>

// for native window JNI
#include <android/native_window_jni.h>

// engine interfaces
static XAObjectItf engineObject = NULL;
static XAEngineItf engineEngine = NULL;

// output mix interfaces
static XAObjectItf outputMixObject = NULL;

// streaming media player interfaces
static XAObjectItf             playerObj = NULL;
static XAPlayItf               playerPlayItf = NULL;
static XAAndroidBufferQueueItf playerBQItf = NULL;
static XAStreamInformationItf  playerStreamInfoItf = NULL;
static XAVolumeItf             playerVolItf = NULL;

// number of required interfaces for the MediaPlayer creation
#define NB_MAXAL_INTERFACES 3 // XAAndroidBufferQueueItf, XAStreamInformationItf and XAPlayItf

// video sink for the player
static ANativeWindow* theNativeWindow = NULL;

/**
 * Macro to clean-up already created stuffs and return JNI_FALSE when cond is not true
 */
#define RETURN_ON_ASSERTION_FAILURE(cond, env, clazz)                                   \
        if (!(cond)) {                                                                  \
            LOGE("assertion failure at file %s line %d", __FILE__, __LINE__);           \
            Java_android_mediastress_cts_NativeMediaActivity_shutdown((env), (clazz));  \
            return JNI_FALSE;                                                           \
        }

// callback invoked whenever there is new or changed stream information
static void StreamChangeCallback(XAStreamInformationItf caller,
        XAuint32 eventId,
        XAuint32 streamIndex,
        void * pEventData,
        void * pContext )
{
    LOGV("StreamChangeCallback called for stream %u", streamIndex);
    // pContext was specified as NULL at RegisterStreamChangeCallback and is unused here
    assert(NULL == pContext);
    switch (eventId) {
      case XA_STREAMCBEVENT_PROPERTYCHANGE: {
        /** From spec 1.0.1:
            "This event indicates that stream property change has occurred.
            The streamIndex parameter identifies the stream with the property change.
            The pEventData parameter for this event is not used and shall be ignored."
         */
        XAresult res;
        XAuint32 domain;
        res = (*caller)->QueryStreamType(caller, streamIndex, &domain);
        assert(XA_RESULT_SUCCESS == res);
        switch (domain) {
          case XA_DOMAINTYPE_VIDEO: {
            XAVideoStreamInformation videoInfo;
            res = (*caller)->QueryStreamInformation(caller, streamIndex, &videoInfo);
            assert(XA_RESULT_SUCCESS == res);
            LOGV("Found video size %u x %u, codec ID=%u, frameRate=%u, bitRate=%u, duration=%u ms",
                        videoInfo.width, videoInfo.height, videoInfo.codecId, videoInfo.frameRate,
                        videoInfo.bitRate, videoInfo.duration);
          } break;
          default:
              LOGE("Unexpected domain %u\n", domain);
            break;
        }
      } break;
      default:
          LOGE("Unexpected stream event ID %u\n", eventId);
        break;
    }
}

// shut down the native media system
// force C naming convention for JNI interfaces to avoid registering manually
extern "C" void Java_android_mediastress_cts_NativeMediaActivity_shutdown(JNIEnv* env,
        jclass clazz)
{
    // destroy streaming media player object, and invalidate all associated interfaces
    if (playerObj != NULL) {
        if (playerPlayItf != NULL) {
            (*playerPlayItf)->SetPlayState(playerPlayItf, XA_PLAYSTATE_STOPPED);
        }
        (*playerObj)->Destroy(playerObj);
        playerObj = NULL;
        playerPlayItf = NULL;
        playerBQItf = NULL;
        playerStreamInfoItf = NULL;
        playerVolItf = NULL;
    }

    // destroy output mix object, and invalidate all associated interfaces
    if (outputMixObject != NULL) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }

    // make sure we don't leak native windows
    if (theNativeWindow != NULL) {
        ANativeWindow_release(theNativeWindow);
        theNativeWindow = NULL;
    }
}

// create the engine and output mix objects
extern "C" jboolean Java_android_mediastress_cts_NativeMediaActivity_createEngine(JNIEnv* env,
        jclass clazz)
{
    XAresult res;

    // create engine
    res = xaCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // realize the engine
    res = (*engineObject)->Realize(engineObject, XA_BOOLEAN_FALSE);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // get the engine interface, which is needed in order to create other objects
    res = (*engineObject)->GetInterface(engineObject, XA_IID_ENGINE, &engineEngine);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // create output mix
    res = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, NULL, NULL);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // realize the output mix
    res = (*outputMixObject)->Realize(outputMixObject, XA_BOOLEAN_FALSE);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    return JNI_TRUE;
}


// create streaming media player
extern "C" jboolean Java_android_mediastress_cts_NativeMediaActivity_createMediaPlayer(JNIEnv* env,
        jclass clazz, jstring fileUri)
{
    XAresult res;

    // convert Java string to UTF-8
    const char *utf8 = env->GetStringUTFChars(fileUri, NULL);
    RETURN_ON_ASSERTION_FAILURE((NULL != utf8), env, clazz);

    XADataLocator_URI uri = {XA_DATALOCATOR_URI, (XAchar*) utf8};
    XADataFormat_MIME format_mime = {
            XA_DATAFORMAT_MIME, XA_ANDROID_MIME_MP2TS, XA_CONTAINERTYPE_MPEG_TS };
    XADataSource dataSrc = {&uri, &format_mime};

    // configure audio sink
    XADataLocator_OutputMix loc_outmix = { XA_DATALOCATOR_OUTPUTMIX, outputMixObject };
    XADataSink audioSnk = { &loc_outmix, NULL };

    // configure image video sink
    XADataLocator_NativeDisplay loc_nd = {
            XA_DATALOCATOR_NATIVEDISPLAY,        // locatorType
            // the video sink must be an ANativeWindow created from a Surface or SurfaceTexture
            (void*)theNativeWindow,              // hWindow
            // must be NULL
            NULL                                 // hDisplay
    };
    XADataSink imageVideoSink = {&loc_nd, NULL};

    // declare interfaces to use
    XAboolean     required[NB_MAXAL_INTERFACES]
                           = {XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE};
    XAInterfaceID iidArray[NB_MAXAL_INTERFACES]
                           = {XA_IID_PLAY,
                              XA_IID_ANDROIDBUFFERQUEUESOURCE,
                              XA_IID_STREAMINFORMATION};

    // create media player
    res = (*engineEngine)->CreateMediaPlayer(engineEngine, &playerObj, &dataSrc,
            NULL, &audioSnk, &imageVideoSink, NULL, NULL,
            NB_MAXAL_INTERFACES /*XAuint32 numInterfaces*/,
            iidArray /*const XAInterfaceID *pInterfaceIds*/,
            required /*const XAboolean *pInterfaceRequired*/);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // release the Java string and UTF-8
    env->ReleaseStringUTFChars(fileUri, utf8);

    // realize the player
    res = (*playerObj)->Realize(playerObj, XA_BOOLEAN_FALSE);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // get the play interface
    res = (*playerObj)->GetInterface(playerObj, XA_IID_PLAY, &playerPlayItf);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // get the stream information interface (for video size)
    res = (*playerObj)->GetInterface(playerObj, XA_IID_STREAMINFORMATION, &playerStreamInfoItf);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // get the volume interface
    res = (*playerObj)->GetInterface(playerObj, XA_IID_VOLUME, &playerVolItf);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // we want to be notified of the video size once it's found, so we register a callback for that
    res = (*playerStreamInfoItf)->RegisterStreamChangeCallback(playerStreamInfoItf,
            StreamChangeCallback, NULL);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // prepare the player
    res = (*playerPlayItf)->SetPlayState(playerPlayItf, XA_PLAYSTATE_PAUSED);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // set the volume
    res = (*playerVolItf)->SetVolumeLevel(playerVolItf, 0);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    // start the playback
    res = (*playerPlayItf)->SetPlayState(playerPlayItf, XA_PLAYSTATE_PLAYING);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);

    return JNI_TRUE;
}


// set the playing state for the streaming media player
extern "C" jboolean Java_android_mediastress_cts_NativeMediaActivity_playOrPauseMediaPlayer(
        JNIEnv* env, jclass clazz, jboolean play)
{
    XAresult res;

    // make sure the streaming media player was created
    RETURN_ON_ASSERTION_FAILURE((NULL != playerPlayItf), env, clazz);
    // set the player's state
    res = (*playerPlayItf)->SetPlayState(playerPlayItf, play ?
        XA_PLAYSTATE_PLAYING : XA_PLAYSTATE_PAUSED);
    RETURN_ON_ASSERTION_FAILURE((XA_RESULT_SUCCESS == res), env, clazz);
    return JNI_TRUE;
}

// set the surface
extern "C" jboolean Java_android_mediastress_cts_NativeMediaActivity_setSurface(JNIEnv *env,
        jclass clazz, jobject surface)
{
    // obtain a native window from a Java surface
    theNativeWindow = ANativeWindow_fromSurface(env, surface);
    return JNI_TRUE;
}
