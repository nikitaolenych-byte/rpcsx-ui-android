/**
 * Real Steel Game Optimizations
 * Специфічні оптимізації для гри Real Steel (Yuke's Engine)
 * 
 * Real Steel - бойова гра про роботів-боксерів
 * Розробник: Yuke's
 * Видавець: Yuke's Media Creations
 */

#ifndef RPCSX_REALSTEEL_HACKS_H
#define RPCSX_REALSTEEL_HACKS_H

#include <cstdint>

namespace rpcsx::realsteel {

/**
 * Конфігурація оптимізацій для Real Steel
 */
struct RealSteelConfig {
    bool physics_optimization;      // Оптимізація фізики роботів
    bool animation_blending;        // Покращене змішування анімацій
    bool metal_shader_fix;          // Виправлення металевих шейдерів
    bool ragdoll_optimization;      // Оптимізація ragdoll фізики
    bool particle_system_fix;       // Виправлення частинок (іскри, дим)
    bool audio_sync_fix;            // Синхронізація аудіо з ударами
    bool crowd_rendering_opt;       // Оптимізація рендерингу натовпу
    bool arena_lighting_fix;        // Виправлення освітлення арени
    int target_fps;                 // Цільовий FPS
    float physics_timestep;         // Крок фізичної симуляції
};

/**
 * Перевірка чи це гра Real Steel
 */
bool IsRealSteelGame(const char* title_id);

/**
 * Ініціалізація всіх оптимізацій для Real Steel
 */
bool InitializeRealSteelHacks(const char* title_id);

/**
 * Оптимізація фізики роботів
 * - Покращення collision detection для суглобів
 * - Оптимізація joint constraints
 */
void OptimizeRobotPhysics();

/**
 * Виправлення металевих шейдерів
 * - PBR reflections для металевих поверхонь
 * - Environment mapping fixes
 */
void FixMetalShaders();

/**
 * Оптимізація анімаційної системи
 * - Покращене blending між анімаціями ударів
 * - IK (Inverse Kinematics) optimization
 */
void OptimizeAnimationBlending();

/**
 * Ragdoll оптимізація
 * - Покращення ragdoll при нокаутах
 * - Стабільність фізики при падіннях
 */
void OptimizeRagdollPhysics();

/**
 * Виправлення системи частинок
 * - Іскри при ударах металу
 * - Дим та пар від пошкоджених роботів
 */
void FixParticleSystem();

/**
 * Синхронізація аудіо
 * - Звуки ударів синхронізовані з анімацією
 * - Crowd audio optimization
 */
void FixAudioSync();

/**
 * Оптимізація рендерингу натовпу
 * - LOD для глядачів на арені
 * - Instanced rendering для crowd
 */
void OptimizeCrowdRendering();

/**
 * Виправлення освітлення арени
 * - Dynamic lighting fixes
 * - Spot lights для боксерського рингу
 */
void FixArenaLighting();

/**
 * Оптимізація frame pacing
 * - Стабільний frametime для fighting games
 * - Input lag reduction
 */
void OptimizeFramePacing(int target_fps);

/**
 * Отримання поточної конфігурації
 */
const RealSteelConfig& GetConfig();

/**
 * Очищення ресурсів
 */
void Shutdown();

} // namespace rpcsx::realsteel

#endif // RPCSX_REALSTEEL_HACKS_H
