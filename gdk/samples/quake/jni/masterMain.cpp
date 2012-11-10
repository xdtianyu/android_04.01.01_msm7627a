/* //device/apps/Quake/quake/src/QW/client/main.c
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <nativehelper/jni.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>

#if !defined(__clang__)
#include <bcc/bcc.h>
#endif

#include <android/log.h>

#define LOG_TAG "Quake masterMain"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#if !defined(__clang__)
int AndroidInit();
int AndroidEvent2(int type, int value);
int AndroidMotionEvent(unsigned long long eventTime, int action, float x, float y, float pressure, float size, int deviceId);
int AndroidTrackballEvent(unsigned long long eventTime, int action, float x, float y);
int AndroidStep(int width, int height);
void AndroidQuit();

typedef int (*pAndroidInitType)();
typedef int (*pAndroidEvent2Type)(int type, int value);
typedef int (*pAndroidMotionEventType)(unsigned long long eventTime, int action, float x, float y, float pressure, float size, int deviceId);
typedef int (*pAndroidTrackballEventType)(unsigned long long eventTime, int action, float x, float y);
typedef int (*pAndroidStepType)(int width, int height);
typedef void (*pAndroidQuitType)();

static pAndroidInitType pAndroidInit;
static pAndroidEvent2Type pAndroidEvent2;
static pAndroidMotionEventType pAndroidMotionEvent;
static pAndroidTrackballEventType pAndroidTrackballEvent;
static pAndroidStepType pAndroidStep;
static pAndroidQuitType pAndroidQuit;

static int use_llvm = 1;

jboolean
qinit(JNIEnv *env, jobject thiz) {
    LOGI("qinit");
    return pAndroidInit() ? JNI_TRUE : JNI_FALSE;
 }

jboolean
qevent(JNIEnv *env, jobject thiz, jint type, jint value) {
    return pAndroidEvent2(type, value) ? JNI_TRUE : JNI_FALSE;
}

jboolean
qmotionevent(JNIEnv *env, jobject thiz, jlong eventTime, jint action,
        jfloat x, jfloat y, jfloat pressure, jfloat size, jint deviceId) {
    return pAndroidMotionEvent((unsigned long long) eventTime,
            action, x, y, pressure, size,
            deviceId) ? JNI_TRUE : JNI_FALSE;
}

jboolean
qtrackballevent(JNIEnv *env, jobject thiz, jlong eventTime, jint action,
        jfloat x, jfloat y) {
    return pAndroidTrackballEvent((unsigned long long) eventTime,
            action, x, y) ? JNI_TRUE : JNI_FALSE;
}

jboolean
qstep(JNIEnv *env, jobject thiz, jint width, jint height) {
    return pAndroidStep(width, height)  ? JNI_TRUE : JNI_FALSE;
}

void
qquit(JNIEnv *env, jobject thiz) {
    LOGI("qquit");
    return pAndroidQuit();
}

static void* lookupSymbol(void* pContext, const char* name)
{
    return (void*) dlsym(RTLD_DEFAULT, name);
}

jboolean
qcompile_bc(JNIEnv *env, jobject thiz, jbyteArray scriptRef, jint length)
{
    if (!use_llvm)
       return JNI_TRUE;

    pAndroidInitType new_pAndroidInit;
    pAndroidEvent2Type new_pAndroidEvent2;
    pAndroidMotionEventType new_pAndroidMotionEvent;
    pAndroidTrackballEventType new_pAndroidTrackballEvent;
    pAndroidStepType new_pAndroidStep;
    pAndroidQuitType new_pAndroidQuit;
    int all_func_found = 1;

    BCCScriptRef script_ref = bccCreateScript();
    jbyte* script_ptr = (jbyte *)env->GetPrimitiveArrayCritical(scriptRef, (jboolean *)0);
    LOGI("BCC Script Len: %d", length);
    if (bccReadBC(script_ref, "libquake_portable.bc", (const char*)script_ptr, length, 0)) {
        LOGE("Error! Cannot bccReadBc");
        return JNI_FALSE;
    }
    if (script_ptr) {
        env->ReleasePrimitiveArrayCritical(scriptRef, script_ptr, 0);
    }
  #if 0
    if (bccLinkFile(script_ref, "/system/lib/libclcore.bc", 0)) {
        LOGE("Error! Cannot bccLinkBC");
        return JNI_FALSE;
    }
  #endif
    bccRegisterSymbolCallback(script_ref, lookupSymbol, NULL);
    if (bccPrepareExecutableEx(script_ref, ".", "/data/data/com.android.quake.llvm/quakeLLVM", 0)) {
        LOGE("Error! Cannot bccPrepareExecutableEx");
        return JNI_FALSE;
    }

    new_pAndroidInit = (pAndroidInitType)bccGetFuncAddr(script_ref, "AndroidInit_LLVM");
    if (new_pAndroidInit == NULL) {
        LOGE("Error! Cannot find AndroidInit_LLVM()");
        all_func_found = 0;
        //return JNI_FALSE;
    }
    LOGI("Found AndroidInit_LLVM() @ 0x%x", (unsigned)new_pAndroidInit);

    new_pAndroidEvent2 = (pAndroidEvent2Type)bccGetFuncAddr(script_ref, "AndroidEvent2_LLVM");
    if (new_pAndroidEvent2 == NULL) {
        LOGE("Error! Cannot find AndroidEvent2_LLVM()");
        all_func_found = 0;
        //return JNI_FALSE;
    }
    LOGI("Found AndroidEvent2_LLVM() @ 0x%x", (unsigned)new_pAndroidEvent2);

    new_pAndroidMotionEvent = (pAndroidMotionEventType)bccGetFuncAddr(script_ref, "AndroidMotionEvent_LLVM");
    if (new_pAndroidMotionEvent == NULL) {
        LOGE("Error! Cannot find AndroidMotionEvent_LLVM()");
        all_func_found = 0;
       //return JNI_FALSE;
    }
    LOGI("Found AndroidMotionEvent_LLVM() @ 0x%x", (unsigned)new_pAndroidMotionEvent);

    new_pAndroidTrackballEvent = (pAndroidTrackballEventType)bccGetFuncAddr(script_ref, "AndroidTrackballEvent_LLVM");
    if (new_pAndroidTrackballEvent == NULL) {
        LOGE("Error! Cannot find AndroidTrackballEvent_LLVM()");
        all_func_found = 0;
       //return JNI_FALSE;
    }
    LOGI("Found AndroidTrackballEvent_LLVM() @ 0x%x", (unsigned)new_pAndroidTrackballEvent);

    new_pAndroidStep = (pAndroidStepType)bccGetFuncAddr(script_ref, "AndroidStep_LLVM");
    if (new_pAndroidStep == NULL) {
        LOGE("Error! Cannot find AndroidStep_LLVM()");
        all_func_found = 0;
       //return JNI_FALSE;
    }
    LOGI("Found AndroidStep_LLVM() @ 0x%x", (unsigned)new_pAndroidStep);

    new_pAndroidQuit = (pAndroidQuitType)bccGetFuncAddr(script_ref, "AndroidQuit_LLVM");
    if (new_pAndroidQuit == NULL) {
        LOGE("Error! Cannot find AndroidQuit_LLVM()");
        all_func_found = 0;
       //return JNI_FALSE;
    }
    LOGI("Found AndroidQuit_LLVM() @ 0x%x", (unsigned)new_pAndroidQuit);

    //bccDisposeScript(script_ref);

  //Uncomment the following
    if (all_func_found)
    {
        LOGI("Use LLVM version");
        pAndroidInit = new_pAndroidInit;
        pAndroidEvent2 = new_pAndroidEvent2;
        pAndroidMotionEvent = new_pAndroidMotionEvent;
        pAndroidTrackballEvent = new_pAndroidTrackballEvent;
        pAndroidStep = new_pAndroidStep;
        pAndroidQuit = new_pAndroidQuit;
    }

    return JNI_TRUE;
}


static const char *classPathName = "com/android/quake/llvm/QuakeLib";

static JNINativeMethod methods[] = {
  {"compile_bc", "([BI)Z", (void*)qcompile_bc },
  {"init", "()Z", (void*)qinit },
  {"event", "(II)Z", (void*)qevent },
  {"motionEvent", "(JIFFFFI)Z", (void*) qmotionevent },
  {"trackballEvent", "(JIFF)Z", (void*) qtrackballevent },
  {"step", "(II)Z", (void*)qstep },
  {"quit", "()V", (void*)qquit },
};

/*
 * Register several native methods for one class.
 */
static int registerNativeMethods(JNIEnv* env, const char* className,
    JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    clazz = env->FindClass(className);
    if (clazz == NULL) {
        fprintf(stderr,
            "Native registration unable to find class '%s'\n", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        fprintf(stderr, "RegisterNatives failed for '%s'\n", className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

/*
 * Register native methods for all classes we know about.
 */
static int registerNatives(JNIEnv* env)
{
  if (!registerNativeMethods(env, classPathName,
                 methods, sizeof(methods) / sizeof(methods[0]))) {
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

/*
 * Set some test stuff up.
 *
 * Returns the JNI version on success, -1 on failure.
 */

typedef union {
    JNIEnv* env;
    void* venv;
} UnionJNIEnvToVoid;

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    UnionJNIEnvToVoid uenv;
    uenv.venv = NULL;
    jint result = -1;
    JNIEnv* env = NULL;

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK) {
        fprintf(stderr, "ERROR: GetEnv failed\n");
        goto bail;
    }
    env = uenv.env;

    assert(env != NULL);

    printf("In mgmain JNI_OnLoad\n");

    if (!registerNatives(env)) {
        fprintf(stderr, "ERROR: quakemaster native registration failed\n");
        goto bail;
    }

    /* success -- return valid version number */
    result = JNI_VERSION_1_4;

    pAndroidInit = AndroidInit;
    pAndroidEvent2 = AndroidEvent2;
    pAndroidMotionEvent = AndroidMotionEvent;
    pAndroidTrackballEvent = AndroidTrackballEvent;
    pAndroidStep = AndroidStep;
    pAndroidQuit = AndroidQuit;

bail:
    return result;
}


#endif // __clang__
