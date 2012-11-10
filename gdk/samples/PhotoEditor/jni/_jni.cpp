#if !defined(__clang__)

#include <nativehelper/jni.h>
#include <android/log.h>
#include <bcc/bcc.h>
#include <dlfcn.h>


#define LOG_TAG "libjni_photoeditor"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#include "_jni.h"

#define DEFINE(f) { #f, 0 },
JNIFuncType JNIFunc[JNI_max] =
{
#include "_jnif.h"
};
#undef DEFINE

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    LOGI("JNI_OnLoad\n");
   
#define DEFINE(f) JNIFunc[ JNI_ ## f ].func_ptr = (void *)f;
   #include "_jnif.h"
#undef DEFINE   

    return JNI_VERSION_1_4;
}

static void* lookupSymbol(void* pContext, const char* name)
{
    return (void*) dlsym(RTLD_DEFAULT, name);
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_android_photoeditor_filters_ImageUtils_init(
    JNIEnv *env, jobject obj, jbyteArray scriptRef, jint length)
{   
    void *new_func_ptr[JNI_max];
    int i, all_func_found = 1;

    BCCScriptRef script_ref = bccCreateScript();
    jbyte* script_ptr = (jbyte *)env->GetPrimitiveArrayCritical(scriptRef, (jboolean *)0);
    LOGI("BCC Script Len: %d", length);
    if (bccReadBC(script_ref, "libjni_photoeditor_portable.bc", (const char*)script_ptr, length, 0)) {
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
    if (bccPrepareExecutableEx(script_ref, ".", "/data/data/com.android.photoeditor/photoeditorLLVM", 0)) {
        LOGE("Error! Cannot bccPrepareExecutableEx");
        return JNI_FALSE;
    }
    for(i=0; i<JNI_max; i++) {
        new_func_ptr[i] = bccGetFuncAddr(script_ref, JNIFunc[i].func_name);
        if (new_func_ptr[i] == NULL) {
	    LOGE("Error! Cannot find %s()\n", JNIFunc[i].func_name);
	    all_func_found = 0;
          //return JNI_FALSE;
	} else 
            LOGI("Found %s() @ 0x%x", JNIFunc[i].func_name, (unsigned)new_func_ptr[i]);
    }

   //bccDisposeScript(script_ref);

    if (all_func_found)
    {
        LOGI("Use LLVM version");
        for(i=0; i<JNI_max; i++)
	     JNIFunc[i].func_ptr = new_func_ptr[i];
    }

    return JNI_TRUE;
}

#endif // __clang__