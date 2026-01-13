/**
 * Game-specific Patches and Optimizations
 * Патчі та оптимізації для конкретних ігор PS3
 * 
 * Підтримувані ігри:
 * - Demon's Souls
 * - Saw
 * - Saw II: Flesh & Blood
 * - inFamous
 * - inFamous 2
 * - Real Steel
 */

#ifndef RPCSX_GAME_PATCHES_H
#define RPCSX_GAME_PATCHES_H

#include <cstdint>
#include <string>

namespace rpcsx::patches {

/**
 * Типи ігор для оптимізацій
 */
enum class GameType {
    UNKNOWN,
    DEMONS_SOULS,
    SAW,
    SAW_2,
    INFAMOUS,
    INFAMOUS_2,
    REAL_STEEL
};

/**
 * Універсальна конфігурація для всіх ігор
 */
struct GameConfig {
    GameType type;
    const char* name;
    int target_fps;
    float physics_timestep;
    bool vsync_enabled;
    bool frame_limiter;
    bool async_shaders;
    bool spu_loop_detection;
    bool accurate_rsxfifio;
    int spu_threads;
    int ppu_threads;
};

// ============================================================================
// Demon's Souls - FromSoftware
// ============================================================================
namespace demons_souls {
    bool IsGame(const char* title_id);
    bool Initialize(const char* title_id);
    void OptimizeSoulsEngine();
    void FixBloodEffects();
    void OptimizeLighting();
    void FixAudioDesync();
    void OptimizeAI();
    void StabilizeFPS();
}

// ============================================================================
// Saw - Zombie Studios / Konami
// ============================================================================
namespace saw {
    bool IsGame(const char* title_id);
    bool Initialize(const char* title_id);
    void OptimizeUnrealEngine();
    void FixGoreEffects();
    void OptimizeTrapPhysics();
    void FixDarkAreas();
    void OptimizeAI();
}

// ============================================================================
// Saw II: Flesh & Blood - Zombie Studios / Konami
// ============================================================================
namespace saw2 {
    bool IsGame(const char* title_id);
    bool Initialize(const char* title_id);
    void OptimizeUnrealEngine();
    void FixGoreEffects();
    void OptimizePuzzlePhysics();
    void FixLightingBugs();
    void OptimizeQTE();
}

// ============================================================================
// inFamous - Sucker Punch
// ============================================================================
namespace infamous {
    bool IsGame(const char* title_id);
    bool Initialize(const char* title_id);
    void OptimizeOpenWorld();
    void FixElectricityEffects();
    void OptimizePhysicsDestruction();
    void FixPopIn();
    void OptimizeStreaming();
    void StabilizeFPS();
}

// ============================================================================
// inFamous 2 - Sucker Punch
// ============================================================================
namespace infamous2 {
    bool IsGame(const char* title_id);
    bool Initialize(const char* title_id);
    void OptimizeOpenWorld();
    void FixElectricityEffects();
    void OptimizePhysicsDestruction();
    void FixUserGeneratedContent();
    void OptimizeStreaming();
    void StabilizeFPS();
}

// ============================================================================
// Main API
// ============================================================================

/**
 * Визначення типу гри за Title ID
 */
GameType DetectGame(const char* title_id);

/**
 * Ініціалізація патчів для гри
 */
bool InitializeGamePatches(const char* title_id);

/**
 * Отримання конфігурації для гри
 */
const GameConfig& GetGameConfig(GameType type);

/**
 * Застосування FPS стабілізації
 */
void ApplyFPSStabilization(int target_fps);

/**
 * Очищення ресурсів
 */
void Shutdown();

} // namespace rpcsx::patches

#endif // RPCSX_GAME_PATCHES_H
