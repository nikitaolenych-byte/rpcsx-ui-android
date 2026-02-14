#include <jni.h>
#include <dlfcn.h>
#include <string>
#include <android/log.h>

#define LOG_TAG "RPCSX-Bootstrap"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static std::string g_last_bootstrap_error;

extern "C" JNIEXPORT jboolean JNICALL Java_net_rpcsx_RPCSX_openLibrary(JNIEnv* env, jobject thiz, jstring jpath) {
    const char* path = env->GetStringUTFChars(jpath, nullptr);
    if (!path) {
        g_last_bootstrap_error = "invalid path string";
        return JNI_FALSE;
    }

    LOGI("Bootstrap: dlopen('%s')", path);
    // Use RTLD_LOCAL to avoid symbol interposition returning bootstrap symbol
    // when querying the plugin handle with dlsym.
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* err = dlerror();
        g_last_bootstrap_error = err ? err : "dlopen failed (unknown)";
        LOGE("Bootstrap: dlopen failed: %s", g_last_bootstrap_error.c_str());
        env->ReleaseStringUTFChars(jpath, path);
        return JNI_FALSE;
    }

    // Try to find the real JNI symbol in the loaded plugin and call it.
    typedef jboolean (*open_fn_t)(JNIEnv*, jobject, jstring);
    void* sym = dlsym(handle, "Java_net_rpcsx_RPCSX_openLibrary");
    if (!sym) {
        const char* err = dlerror();
        g_last_bootstrap_error = err ? err : "dlsym failed to find Java_net_rpcsx_RPCSX_openLibrary";
        LOGE("Bootstrap: dlsym failed: %s", g_last_bootstrap_error.c_str());
        env->ReleaseStringUTFChars(jpath, path);
        return JNI_FALSE;
    }

    open_fn_t fn = reinterpret_cast<open_fn_t>(sym);
    if (!fn) {
        g_last_bootstrap_error = "symbol found but cast failed";
        env->ReleaseStringUTFChars(jpath, path);
        return JNI_FALSE;
    }

    // Delegate the call to the plugin's implementation.
    jboolean result = JNI_FALSE;
    try {
        result = fn(env, thiz, jpath);
    } catch (...) {
        g_last_bootstrap_error = "exception when calling plugin openLibrary";
        LOGE("Bootstrap: exception calling plugin openLibrary");
        result = JNI_FALSE;
    }

    env->ReleaseStringUTFChars(jpath, path);
    return result;
}

extern "C" JNIEXPORT jstring JNICALL Java_net_rpcsx_RPCSX_getLastOpenLibraryError(JNIEnv* env, jobject /*thiz*/) {
    if (g_last_bootstrap_error.empty()) return nullptr;
    return env->NewStringUTF(g_last_bootstrap_error.c_str());
}
