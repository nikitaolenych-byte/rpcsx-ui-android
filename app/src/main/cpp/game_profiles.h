/**
 * RPCSX Game-Specific Performance Profiles
 * 
 * Система оптимізаційних профілів для конкретних PS3 ігор.
 * Дозволяє налаштовувати параметри емуляції індивідуально
 * для кожної гри на основі її Title ID.
 */

#ifndef RPCSX_GAME_PROFILES_H
#define RPCSX_GAME_PROFILES_H

#include <cstdint>
#include <atomic>
#include <string>
#include <functional>

namespace rpcsx::profiles {

// =============================================================================
// Константи та типи
// =============================================================================

#define MAX_TITLE_ID_LENGTH 16
#define MAX_PROFILE_NAME_LENGTH 64
#define MAX_PROFILES_COUNT 256

/**
 * Тип профілю
 */
enum class ProfileType : uint8_t {
    SYSTEM_DEFAULT = 0,     // Вбудований системний профіль
    GAME_SPECIFIC,          // Профіль для конкретної гри
    USER_CUSTOM,            // Користувацький профіль
    COMMUNITY               // Профіль від спільноти
};

/**
 * Режим GPU емуляції
 */
enum class GPUMode : uint8_t {
    ACCURATE = 0,           // Максимальна точність
    BALANCED,               // Баланс точності та швидкості
    PERFORMANCE,            // Максимальна швидкість
    CUSTOM                  // Користувацькі налаштування
};

/**
 * Режим SPU емуляції
 */
enum class SPUMode : uint8_t {
    INTERPRETER = 0,        // Інтерпретатор (повільний, точний)
    RECOMPILER_LLVM,        // LLVM JIT рекомпілятор
    RECOMPILER_ASMJIT,      // ASM JIT рекомпілятор (швидкий)
    AUTO                    // Автовибір
};

/**
 * Режим PPU емуляції
 */
enum class PPUMode : uint8_t {
    INTERPRETER = 0,        // Інтерпретатор
    RECOMPILER_LLVM,        // LLVM JIT
    AUTO                    // Автовибір
};

/**
 * Рівень анізотропної фільтрації
 */
enum class AnisotropicLevel : uint8_t {
    OFF = 0,
    X2 = 2,
    X4 = 4,
    X8 = 8,
    X16 = 16
};

/**
 * Режим антиаліасингу
 */
enum class AAMode : uint8_t {
    OFF = 0,
    FXAA,
    SMAA,
    TAA,
    MSAA_2X,
    MSAA_4X
};

/**
 * Режим VSync
 */
enum class VSyncMode : uint8_t {
    OFF = 0,
    ON,
    ADAPTIVE,
    HALF_RATE               // 30 FPS lock
};

/**
 * Режим аудіо буферизації
 */
enum class AudioBufferMode : uint8_t {
    ULTRA_LOW = 0,          // ~16ms (можливі артефакти)
    LOW,                    // ~32ms
    MEDIUM,                 // ~64ms
    HIGH,                   // ~128ms (стабільний)
    AUTO
};

// =============================================================================
// Налаштування профілю
// =============================================================================

/**
 * GPU налаштування
 */
struct GPUSettings {
    GPUMode mode = GPUMode::BALANCED;
    
    // Resolution
    float resolution_scale = 1.0f;          // 0.5 - 3.0
    bool enable_drs = true;                  // Dynamic Resolution Scaling
    float drs_min_scale = 0.5f;
    float drs_max_scale = 1.5f;
    
    // Filtering
    AnisotropicLevel anisotropic = AnisotropicLevel::X4;
    AAMode anti_aliasing = AAMode::OFF;
    
    // Texture
    bool enable_texture_streaming = true;
    uint32_t texture_cache_size_mb = 256;
    bool compress_textures = true;
    
    // Shader
    bool async_shader_compile = true;
    bool enable_shader_cache = true;
    uint32_t shader_cache_size_mb = 128;
    
    // Vulkan
    bool use_pipeline_cache = true;
    bool prefer_vsync = true;
    VSyncMode vsync_mode = VSyncMode::ADAPTIVE;
    
    // Accuracy
    bool strict_rendering = false;
    bool accurate_framebuffer = false;
    bool force_cpu_blit = false;
};

/**
 * CPU налаштування
 */
struct CPUSettings {
    // PPU
    PPUMode ppu_mode = PPUMode::RECOMPILER_LLVM;
    uint32_t ppu_threads = 0;               // 0 = auto
    bool ppu_accurate_nj = false;
    bool ppu_accurate_vnan = false;
    bool ppu_accurate_fpcc = false;
    
    // SPU
    SPUMode spu_mode = SPUMode::RECOMPILER_ASMJIT;
    uint32_t spu_threads = 0;               // 0 = auto
    bool spu_accurate_dfma = false;
    bool spu_accurate_getllar = false;
    bool spu_accurate_putlluc = false;
    
    // Scheduler
    uint32_t max_cpu_preempt_count = 0;
    int32_t thread_priority_offset = 0;
    bool enable_smt = true;
};

/**
 * Аудіо налаштування
 */
struct AudioSettings {
    bool enable_audio = true;
    float master_volume = 1.0f;             // 0.0 - 1.0
    AudioBufferMode buffer_mode = AudioBufferMode::MEDIUM;
    uint32_t sample_rate = 48000;           // 44100 or 48000
    bool enable_time_stretching = true;
    float stretch_threshold = 0.05f;
    bool downmix_to_stereo = false;
};

/**
 * Мережеві налаштування
 */
struct NetworkSettings {
    bool enable_network = false;
    bool enable_upnp = false;
    bool enable_psnp = false;
    uint16_t bind_port = 0;                 // 0 = auto
};

/**
 * Хаки/патчі для сумісності
 */
struct CompatibilityHacks {
    // Загальні
    bool skip_intro = false;
    bool disable_vertex_cache = false;
    bool force_gpu_flush = false;
    
    // Специфічні
    bool fix_god_of_war_shadows = false;
    bool fix_uncharted_water = false;
    bool fix_tlou_shaders = false;
    bool fix_gt5_physics = false;
    bool fix_mgs4_cutscenes = false;
    bool fix_demon_souls_fog = false;
    
    // Продуктивність
    bool skip_mipmaps = false;
    bool disable_async_compute = false;
    bool limit_spu_threads = false;
    uint32_t max_spu_threads = 6;
};

/**
 * Повний профіль гри
 */
struct GameProfile {
    // Ідентифікація
    char title_id[MAX_TITLE_ID_LENGTH] = {0};   // e.g., "BCES00510"
    char profile_name[MAX_PROFILE_NAME_LENGTH] = {0};
    ProfileType type = ProfileType::USER_CUSTOM;
    
    // Версія та метадані
    uint32_t version = 1;
    uint64_t created_timestamp = 0;
    uint64_t modified_timestamp = 0;
    
    // Налаштування
    GPUSettings gpu;
    CPUSettings cpu;
    AudioSettings audio;
    NetworkSettings network;
    CompatibilityHacks hacks;
    
    // Target FPS
    uint32_t target_fps = 30;               // 30 or 60
    bool unlock_fps = false;
    
    // Priority
    int32_t priority = 0;                   // Вищий = більш пріоритетний
};

/**
 * Статистика профілів
 */
struct ProfileStats {
    uint32_t total_profiles;
    uint32_t system_profiles;
    uint32_t game_profiles;
    uint32_t user_profiles;
    uint32_t community_profiles;
    
    uint32_t active_profile_switches;
    uint64_t total_playtime_seconds;
    
    const char* current_title_id;
    const char* current_profile_name;
};

// =============================================================================
// Callback типи
// =============================================================================

using ProfileChangeCallback = std::function<void(const char* title_id, const GameProfile* profile)>;
using ProfileLoadCallback = std::function<void(bool success, const char* error)>;

// =============================================================================
// Глобальний стан
// =============================================================================

extern std::atomic<bool> g_profiles_active;
extern std::atomic<uint32_t> g_active_profile_count;

// =============================================================================
// API функції
// =============================================================================

/**
 * Ініціалізація системи профілів
 * @param profiles_dir Директорія для збереження профілів
 * @return true якщо успішно
 */
bool InitializeProfiles(const char* profiles_dir);

/**
 * Завершення роботи системи профілів
 */
void ShutdownProfiles();

/**
 * Перевірка чи система активна
 */
bool IsProfileSystemActive();

// --- Керування профілями ---

/**
 * Отримати профіль для гри за Title ID
 * @param title_id Title ID гри (e.g., "BCES00510")
 * @return Вказівник на профіль або nullptr
 */
const GameProfile* GetProfileForGame(const char* title_id);

/**
 * Отримати профіль за замовчуванням
 */
const GameProfile* GetDefaultProfile();

/**
 * Застосувати профіль
 * @param title_id Title ID гри
 * @return true якщо успішно
 */
bool ApplyProfileForGame(const char* title_id);

/**
 * Створити новий профіль
 * @param profile Налаштування профілю
 * @return true якщо успішно
 */
bool CreateProfile(const GameProfile& profile);

/**
 * Оновити існуючий профіль
 * @param title_id Title ID гри
 * @param profile Нові налаштування
 * @return true якщо успішно
 */
bool UpdateProfile(const char* title_id, const GameProfile& profile);

/**
 * Видалити профіль
 * @param title_id Title ID гри
 * @return true якщо успішно
 */
bool DeleteProfile(const char* title_id);

/**
 * Перевірити наявність профілю
 */
bool HasProfile(const char* title_id);

// --- Вбудовані профілі ---

/**
 * Отримати вбудований профіль для відомої гри
 */
const GameProfile* GetBuiltInProfile(const char* title_id);

/**
 * Отримати список всіх вбудованих профілів
 * @param count Вихідний параметр - кількість профілів
 * @return Масив Title ID
 */
const char** GetBuiltInProfileList(uint32_t* count);

// --- Імпорт/Експорт ---

/**
 * Експортувати профіль у JSON
 * @param title_id Title ID гри
 * @param json_buffer Буфер для результату
 * @param buffer_size Розмір буфера
 * @return Довжина JSON або 0 при помилці
 */
size_t ExportProfileToJson(const char* title_id, char* json_buffer, size_t buffer_size);

/**
 * Імпортувати профіль з JSON
 * @param json JSON рядок
 * @param json_length Довжина JSON
 * @return true якщо успішно
 */
bool ImportProfileFromJson(const char* json, size_t json_length);

/**
 * Завантажити профілі з файлу
 */
bool LoadProfilesFromFile(const char* path);

/**
 * Зберегти профілі у файл
 */
bool SaveProfilesToFile(const char* path);

// --- Callbacks ---

/**
 * Встановити callback для зміни профілю
 */
void SetProfileChangeCallback(ProfileChangeCallback callback);

// --- Статистика ---

/**
 * Отримати статистику
 */
void GetProfileStats(ProfileStats* stats);

/**
 * Отримати поточний активний профіль
 */
const GameProfile* GetCurrentProfile();

/**
 * Скинути статистику
 */
void ResetProfileStats();

// --- Утиліти ---

/**
 * Перевірити валідність Title ID
 */
bool IsValidTitleId(const char* title_id);

/**
 * Отримати назву гри за Title ID (якщо відома)
 */
const char* GetGameName(const char* title_id);

/**
 * Отримати регіон за Title ID
 */
const char* GetGameRegion(const char* title_id);

/**
 * Скопіювати профіль
 */
void CopyProfile(GameProfile* dest, const GameProfile* src);

/**
 * Порівняти профілі
 */
bool CompareProfiles(const GameProfile* a, const GameProfile* b);

} // namespace rpcsx::profiles

#endif // RPCSX_GAME_PROFILES_H
