/**
 * FSR 3.1 (FidelityFX Super Resolution) Integration
 * Апскейлінг 720p -> 1440p з високою якістю
 */

#ifndef RPCSX_FSR31_H
#define RPCSX_FSR31_H

#include <cstdint>
#include <cstddef>

namespace rpcsx::fsr {

enum class FSRQuality {
    ULTRA_QUALITY,      // 1.3x upscale
    QUALITY,            // 1.5x upscale  
    BALANCED,           // 1.7x upscale
    PERFORMANCE,        // 2.0x upscale (720p->1440p)
    ULTRA_PERFORMANCE   // 3.0x upscale
};

struct FSRContext {
    uint32_t input_width;
    uint32_t input_height;
    uint32_t output_width;
    uint32_t output_height;
    FSRQuality quality;
    bool sharpening_enabled;
    float sharpness;
};

/**
 * Ініціалізація FSR 3.1
 */
bool InitializeFSR(uint32_t input_width, uint32_t input_height,
                   uint32_t output_width, uint32_t output_height,
                   FSRQuality quality);

/**
 * Апскейлінг кадру через FSR
 */
bool UpscaleFrame(void* input_texture, void* output_texture);

/**
 * Налаштування sharpening
 */
void SetSharpness(float sharpness);

/**
 * Очищення FSR ресурсів
 */
void ShutdownFSR();

/**
 * Отримання рекомендованого render resolution для target
 */
void GetOptimalRenderResolution(uint32_t target_width, uint32_t target_height,
                                FSRQuality quality,
                                uint32_t* out_render_width, uint32_t* out_render_height);

} // namespace rpcsx::fsr

#endif // RPCSX_FSR31_H
