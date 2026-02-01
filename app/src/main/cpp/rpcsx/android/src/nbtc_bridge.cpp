#include <jni.h>
#include <string>
#include <android/log.h>
#include "../../nbtc_engine/nbtc_engine.h"

#define LOG_TAG "rpcsx-nbtc-bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_nbtc_NbtcBridge_initialize(JNIEnv* env, jclass, jstring modelPath) {
    const char* path = modelPath ? env->GetStringUTFChars(modelPath, nullptr) : nullptr;
    bool ok = nbtc::Initialize(path ? path : "");
    if (path) env->ReleaseStringUTFChars(modelPath, path);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_nbtc_NbtcBridge_analyzeAndCache(JNIEnv* env, jclass, jstring gamePath) {
    const char* path = env->GetStringUTFChars(gamePath, nullptr);
    bool ok = nbtc::AnalyzeAndCache(path);
    env->ReleaseStringUTFChars(gamePath, path);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_nbtc_NbtcBridge_loadCacheForGame(JNIEnv* env, jclass, jstring gameId) {
    const char* id = env->GetStringUTFChars(gameId, nullptr);
    bool ok = nbtc::LoadCacheForGame(id);
    env->ReleaseStringUTFChars(gameId, id);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_net_rpcsx_nbtc_NbtcBridge_shutdown(JNIEnv* env, jclass) {
    nbtc::Shutdown();
}
