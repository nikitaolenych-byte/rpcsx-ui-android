#include <jni.h>
#include <string>
#include <android/log.h>

#define LOG_TAG "rpcsx-nbtc-jni"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {
  // Forward to engine implementation
  bool nbtc_init(const char* cache_dir, const char* model_path);
  bool nbtc_analyze_and_cache(const char* game_path);
  bool nbtc_load_cache(const char* game_path);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_Nbtc_init(JNIEnv* env, jclass, jstring jcacheDir, jstring jmodelPath) {
    const char* cacheDir = jcacheDir ? env->GetStringUTFChars(jcacheDir, nullptr) : nullptr;
    const char* modelPath = jmodelPath ? env->GetStringUTFChars(jmodelPath, nullptr) : nullptr;
    bool res = nbtc_init(cacheDir, modelPath);
    if (cacheDir) env->ReleaseStringUTFChars(jcacheDir, cacheDir);
    if (modelPath) env->ReleaseStringUTFChars(jmodelPath, modelPath);
    LOGI("NBTC JNI init -> %d", res);
    return res ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_Nbtc_analyzeAndCache(JNIEnv* env, jclass, jstring jgamePath) {
    const char* gamePath = jgamePath ? env->GetStringUTFChars(jgamePath, nullptr) : nullptr;
    if (!gamePath) return JNI_FALSE;
    bool res = nbtc_analyze_and_cache(gamePath);
    env->ReleaseStringUTFChars(jgamePath, gamePath);
    LOGI("NBTC JNI analyzeAndCache -> %d", res);
    return res ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_Nbtc_loadCache(JNIEnv* env, jclass, jstring jgamePath) {
    const char* gamePath = jgamePath ? env->GetStringUTFChars(jgamePath, nullptr) : nullptr;
    if (!gamePath) return JNI_FALSE;
    bool res = nbtc_load_cache(gamePath);
    env->ReleaseStringUTFChars(jgamePath, gamePath);
    LOGI("NBTC JNI loadCache -> %d", res);
    return res ? JNI_TRUE : JNI_FALSE;
}
