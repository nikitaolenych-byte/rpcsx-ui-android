/**
 * Frostbite 3 Hacks заголовковий файл
 */

#ifndef RPCSX_FROSTBITE_HACKS_H
#define RPCSX_FROSTBITE_HACKS_H

namespace rpcsx::frostbite {

struct FrostbiteConfig;

/**
 * Перевірка чи гра використовує Frostbite 3
 */
bool IsFrostbite3Game(const char* title_id);

/**
 * Ініціалізація Frostbite-специфічних хаків
 */
bool InitializeFrostbiteHacks(const char* title_id);

/**
 * Write Color Buffers hack (виправлення transparency issues)
 */
void EnableWriteColorBuffers();

/**
 * MLLE (Modular SPU Emulation) mode
 */
void EnableMLLEMode();

/**
 * Terrain LOD patching
 */
void PatchTerrainLOD();

/**
 * Deferred Lighting optimization
 */
void OptimizeDeferredLighting();

/**
 * Frame pacing
 */
void OptimizeFramePacing(int target_fps);

/**
 * Shader complexity reduction
 */
void ReduceShaderComplexity();

/**
 * Animation system optimization
 */
void OptimizeAnimationSystem();

/**
 * Particle system optimization
 */
void OptimizeParticleSystem();

/**
 * Network adjustments
 */
void AdjustNetworkTickRate();

/**
 * Memory optimization
 */
void OptimizeMemoryUsage();

/**
 * Culling optimization
 */
void OptimizeCulling();

/**
 * Runtime patches
 */
void ApplyRuntimePatches();

/**
 * Отримання конфігурації
 */
const FrostbiteConfig& GetConfig();

/**
 * Статистика
 */
void PrintOptimizationStats();

} // namespace rpcsx::frostbite

#endif // RPCSX_FROSTBITE_HACKS_H
