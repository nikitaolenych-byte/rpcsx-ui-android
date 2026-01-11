#include "fsr31.h"
#include <android/log.h>
#include <atomic>

#define LOG_TAG "RPCSX-FSR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace rpcsx::fsr {

static FSRContext g_ctx{};
static std::atomic<bool> g_initialized{false};

bool InitializeFSR(uint32_t input_width, uint32_t input_height,
                   uint32_t output_width, uint32_t output_height,
                   FSRQuality quality) {
    g_ctx.input_width = input_width;
    g_ctx.input_height = input_height;
    g_ctx.output_width = output_width;
    g_ctx.output_height = output_height;
    g_ctx.quality = quality;
    g_ctx.sharpening_enabled = true;
    g_ctx.sharpness = 0.2f;

    g_initialized.store(true, std::memory_order_release);
    LOGI("Initializing FSR 3.1 (stub): %ux%u -> %ux%u", input_width, input_height, output_width, output_height);
    return true;
}

bool UpscaleFrame(void* /*input_texture*/, void* /*output_texture*/) {
    if (!g_initialized.load(std::memory_order_acquire)) {
        return false;
    }
    // Stub: no-op
    return true;
}

void SetSharpness(float sharpness) {
    g_ctx.sharpness = sharpness;
    g_ctx.sharpening_enabled = true;
}

void ShutdownFSR() {
    g_initialized.store(false, std::memory_order_release);
}

void GetOptimalRenderResolution(uint32_t target_width, uint32_t target_height,
                                FSRQuality quality,
                                uint32_t* out_render_width, uint32_t* out_render_height) {
    float scale = 1.5f;
    switch (quality) {
        case FSRQuality::ULTRA_QUALITY: scale = 1.3f; break;
        case FSRQuality::QUALITY: scale = 1.5f; break;
        case FSRQuality::BALANCED: scale = 1.7f; break;
        case FSRQuality::PERFORMANCE: scale = 2.0f; break;
        case FSRQuality::ULTRA_PERFORMANCE: scale = 3.0f; break;
    }

    if (out_render_width) {
        *out_render_width = static_cast<uint32_t>(static_cast<float>(target_width) / scale);
    }
    if (out_render_height) {
        *out_render_height = static_cast<uint32_t>(static_cast<float>(target_height) / scale);
    }
}

} // namespace rpcsx::fsr
