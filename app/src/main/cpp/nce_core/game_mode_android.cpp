// ============================================================================
// Game Mode / Performance Mode (Android)
// ============================================================================
#include "game_mode_android.h"
#include <android/log.h>

#define LOG_TAG "GameMode"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace rpcsx {
namespace android {

void EnableGameMode() {
    // TODO: Використати Android Game Mode API (NDK/Java)
    LOGI("Enable Game Mode (Performance Mode)");
}

void DisableGameMode() {
    LOGI("Disable Game Mode");
}

} // namespace android
} // namespace rpcsx
