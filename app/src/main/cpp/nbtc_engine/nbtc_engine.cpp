// Lightweight NBTC prototype implementation (no real ML yet)
#include "nbtc_engine.h"
#include <android/log.h>
#include <fstream>

#define LOG_TAG "rpcsx-nbtc"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace nbtc {

bool Initialize(const std::string& model_path) {
    LOGI("NBTC: Initialize called, model_path=%s", model_path.c_str());
    // Prototype: no model bundled. In future load TFLite model here.
    return true;
}

bool AnalyzeAndCache(const std::string& game_path) {
    LOGI("NBTC: AnalyzeAndCache called for %s", game_path.c_str());
    // Prototype: create a tiny marker file next to game_path as cache proof
    try {
        std::string marker = game_path + ".nbtc.cache";
        std::ofstream f(marker, std::ios::binary);
        f << "nbtc-prototype";
        f.close();
        LOGI("NBTC: cache written %s", marker.c_str());
        return true;
    } catch (...) {
        LOGE("NBTC: failed to write cache for %s", game_path.c_str());
        return false;
    }
}

bool LoadCacheForGame(const std::string& game_id) {
    LOGI("NBTC: LoadCacheForGame called for %s", game_id.c_str());
    // Prototype: check for marker file
    std::string marker = game_id + ".nbtc.cache";
    std::ifstream f(marker, std::ios::binary);
    bool ok = f.good();
    LOGI("NBTC: cache %s for %s", ok ? "found" : "not found", game_id.c_str());
    return ok;
}

void Shutdown() {
    LOGI("NBTC: Shutdown");
}

} // namespace nbtc
