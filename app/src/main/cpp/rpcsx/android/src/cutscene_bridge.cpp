#include <jni.h>
#include <string>
#include <android/log.h>

#define LOG_TAG "rpcsx-cutscene"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static JavaVM* gJvm = nullptr;

extern "C" void rpcsx_request_playback(const char* path) {
    if (!gJvm || !path) return;

    JNIEnv* env = nullptr;
    bool attached = false;
    if (gJvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (gJvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            LOGE("Failed to attach current thread to JVM");
            return;
        }
        attached = true;
    }

    jclass bridge = env->FindClass("net/rpcsx/media/CutsceneBridge");
    if (!bridge) {
        LOGE("CutsceneBridge class not found");
        if (attached) gJvm->DetachCurrentThread();
        return;
    }

    jmethodID mid = env->GetStaticMethodID(bridge, "playFromNative", "(Ljava/lang/String;)V");
    if (!mid) {
        LOGE("playFromNative method not found");
        env->DeleteLocalRef(bridge);
        if (attached) gJvm->DetachCurrentThread();
        return;
    }

    jstring jpath = env->NewStringUTF(path);
    env->CallStaticVoidMethod(bridge, mid, jpath);
    env->DeleteLocalRef(jpath);
    env->DeleteLocalRef(bridge);

    if (attached) gJvm->DetachCurrentThread();
}

extern "C" void rpcsx_request_stop() {
    if (!gJvm) return;

    JNIEnv* env = nullptr;
    bool attached = false;
    if (gJvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (gJvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            LOGE("Failed to attach current thread to JVM");
            return;
        }
        attached = true;
    }

    jclass bridge = env->FindClass("net/rpcsx/media/CutsceneBridge");
    if (!bridge) {
        LOGE("CutsceneBridge class not found");
        if (attached) gJvm->DetachCurrentThread();
        return;
    }

    jmethodID mid = env->GetStaticMethodID(bridge, "stopFromNative", "()V");
    if (!mid) {
        LOGE("stopFromNative method not found");
        env->DeleteLocalRef(bridge);
        if (attached) gJvm->DetachCurrentThread();
        return;
    }

    env->CallStaticVoidMethod(bridge, mid);
    env->DeleteLocalRef(bridge);

    if (attached) gJvm->DetachCurrentThread();
}

// Setter called from another native module to provide JavaVM
extern "C" void rpcsx_set_jvm(JavaVM* vm) {
    gJvm = vm;
}
