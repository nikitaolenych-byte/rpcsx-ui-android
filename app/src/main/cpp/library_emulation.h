/**
 * Missing PS3 Library Emulation Layer
 * 
 * Provides stub implementations and emulation for PS3 system libraries
 * that may be missing or incompletely implemented.
 * 
 * Features:
 * - HLE (High Level Emulation) of common PS3 libraries
 * - PRX module loading simulation
 * - Function export/import resolution
 * - LV2 library function stubs
 * - Per-game library overrides
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <unordered_map>

namespace rpcsx::libraries {

// =============================================================================
// Library Categories
// =============================================================================

enum class LibraryCategory {
    SYSTEM,         // Системні бібліотеки (liblv2, libsysutil)
    GRAPHICS,       // Графіка (libgcm, libresc)
    AUDIO,          // Аудіо (libaudio, libatrac)
    VIDEO,          // Відео (libvdec, libdmux)
    NETWORK,        // Мережа (libnet, libhttp)
    STORAGE,        // Зберігання (libfs, libsavedata)
    INPUT,          // Введення (libpad, libkb)
    GAME,           // Ігрові (libsysutil_game, libtrophy)
    MEDIA,          // Медіа (libpngdec, libjpgdec)
    FONT,           // Шрифти (libfont, libfontft)
    SCRIPT,         // Скрипти (liblua)
    PHYSICS,        // Фізика (libphysics)
    SPU,            // SPU бібліотеки (libspurs, libspu)
    MOVE,           // PlayStation Move
    CAMERA,         // PlayStation Eye
    MISC            // Інші
};

// =============================================================================
// Implementation Status
// =============================================================================

enum class ImplStatus {
    NOT_IMPLEMENTED,    // Не реалізовано
    STUB,               // Заглушка (повертає успіх)
    PARTIAL,            // Часткова реалізація
    COMPLETE,           // Повна реалізація
    HLE                 // High Level Emulation
};

// =============================================================================
// Library Information
// =============================================================================

struct LibraryInfo {
    std::string name;           // Назва (e.g., "libsysutil")
    std::string filename;       // Ім'я файлу (e.g., "libsysutil.sprx")
    LibraryCategory category;
    ImplStatus status;
    uint32_t version;           // Версія бібліотеки
    std::string description;
    uint32_t export_count;      // Кількість експортованих функцій
    uint32_t implemented_count; // Кількість реалізованих
};

// =============================================================================
// Function Export
// =============================================================================

struct FunctionExport {
    std::string name;           // Назва функції
    uint32_t nid;               // Native Interface Descriptor
    ImplStatus status;
    bool is_critical;           // Критична для роботи гри
    uint64_t call_count;        // Лічильник викликів
    std::string stub_behavior;  // Поведінка заглушки
};

// =============================================================================
// Emulation Configuration
// =============================================================================

struct LibraryEmulationConfig {
    bool enabled = true;
    bool log_missing_functions = true;
    bool log_stub_calls = false;
    bool auto_stub_missing = true;
    bool crash_on_critical_missing = false;
    std::string library_path;           // Шлях до PRX файлів
    std::string hle_config_path;        // Шлях до HLE конфігурації
};

// =============================================================================
// Per-Game Library Override
// =============================================================================

struct GameLibraryOverride {
    std::string title_id;
    std::string library_name;
    bool force_stub;            // Завжди використовувати заглушку
    bool force_hle;             // Завжди використовувати HLE
    bool disabled;              // Вимкнути бібліотеку
    std::string reason;
};

// =============================================================================
// Library Statistics
// =============================================================================

struct LibraryStats {
    uint32_t total_libraries;
    uint32_t loaded_libraries;
    uint32_t stubbed_libraries;
    uint32_t hle_libraries;
    uint64_t total_function_calls;
    uint64_t stubbed_function_calls;
    uint64_t missing_function_calls;
    std::vector<std::pair<std::string, uint64_t>> top_missing_functions;
};

// =============================================================================
// Function Handler
// =============================================================================

struct FunctionContext {
    uint32_t nid;
    uint64_t args[8];           // r3-r10
    uint64_t* result;           // r3 output
    void* thread_context;
    const char* library_name;
    const char* function_name;
};

using FunctionHandler = std::function<int32_t(FunctionContext& ctx)>;

// =============================================================================
// Global State
// =============================================================================

extern std::atomic<bool> g_library_emulation_active;
extern std::atomic<uint64_t> g_library_calls;

// =============================================================================
// API Functions
// =============================================================================

/**
 * Ініціалізація системи емуляції бібліотек
 */
bool InitializeLibraryEmulation(const LibraryEmulationConfig& config);

/**
 * Завершення роботи
 */
void ShutdownLibraryEmulation();

/**
 * Чи активна емуляція
 */
bool IsLibraryEmulationActive();

/**
 * Завантажити бібліотеку
 * @param name Назва бібліотеки (без .sprx)
 * @return Handle бібліотеки або 0 при помилці
 */
uint32_t LoadLibrary(const char* name);

/**
 * Вивантажити бібліотеку
 */
bool UnloadLibrary(uint32_t handle);

/**
 * Отримати інформацію про бібліотеку
 */
bool GetLibraryInfo(const char* name, LibraryInfo* info);

/**
 * Отримати список всіх бібліотек
 */
size_t GetAllLibraries(LibraryInfo* buffer, size_t buffer_size);

/**
 * Отримати функцію за NID
 * @param library_name Назва бібліотеки
 * @param nid Native Interface Descriptor
 * @param address Вихідна адреса функції
 */
bool ResolveFunctionByNID(const char* library_name, uint32_t nid, uint64_t* address);

/**
 * Отримати функцію за іменем
 */
bool ResolveFunctionByName(const char* library_name, const char* func_name, uint64_t* address);

/**
 * Зареєструвати HLE обробник
 */
bool RegisterHLEHandler(const char* library_name, uint32_t nid, FunctionHandler handler);

/**
 * Видалити HLE обробник
 */
bool UnregisterHLEHandler(const char* library_name, uint32_t nid);

/**
 * Викликати функцію бібліотеки
 */
bool CallLibraryFunction(const char* library_name, uint32_t nid,
                          uint64_t* args, uint64_t* result);

/**
 * Отримати інформацію про функцію
 */
bool GetFunctionInfo(const char* library_name, uint32_t nid, FunctionExport* info);

/**
 * Отримати список функцій бібліотеки
 */
size_t GetLibraryFunctions(const char* library_name, FunctionExport* buffer, size_t buffer_size);

/**
 * Встановити override для гри
 */
bool SetGameLibraryOverride(const char* title_id, const char* library_name,
                             bool force_stub, bool disabled);

/**
 * Видалити override для гри
 */
bool RemoveGameLibraryOverride(const char* title_id, const char* library_name);

/**
 * Встановити поточний контекст гри
 */
void SetCurrentGame(const char* title_id);

/**
 * Перевірити чи бібліотека доступна
 */
bool IsLibraryAvailable(const char* name);

/**
 * Перевірити статус реалізації бібліотеки
 */
ImplStatus GetLibraryStatus(const char* name);

/**
 * Отримати список відсутніх функцій
 */
size_t GetMissingFunctions(const char* library_name, uint32_t* nid_buffer, size_t buffer_size);

/**
 * Отримати статистику
 */
void GetLibraryStats(LibraryStats* stats);

/**
 * Скинути статистику
 */
void ResetLibraryStats();

/**
 * Експорт статистики в JSON
 */
size_t ExportLibraryStatsJson(char* buffer, size_t buffer_size);

/**
 * Завантажити HLE конфігурацію
 */
bool LoadHLEConfig(const char* filepath);

/**
 * Зберегти HLE конфігурацію
 */
bool SaveHLEConfig(const char* filepath);

// =============================================================================
// NID Helper Functions
// =============================================================================

/**
 * Обчислити NID з назви функції
 */
uint32_t CalculateNID(const char* function_name);

/**
 * Отримати назву функції за NID (якщо відомо)
 */
const char* GetFunctionNameByNID(uint32_t nid);

/**
 * Зареєструвати відповідність NID->ім'я
 */
void RegisterNIDName(uint32_t nid, const char* name);

// =============================================================================
// Commonly Used NIDs
// =============================================================================

namespace nid {
    // libsysutil
    constexpr uint32_t cellSysutilCheckCallback = 0xE7F78BE7;
    constexpr uint32_t cellSysutilRegisterCallback = 0x9D98AAED;
    constexpr uint32_t cellSysutilUnregisterCallback = 0x02FF3C1D;
    constexpr uint32_t cellSysutilGetSystemParamInt = 0x40D34A52;
    constexpr uint32_t cellSysutilGetSystemParamString = 0x938013A0;
    
    // libsysutil_savedata
    constexpr uint32_t cellSaveDataListSave2 = 0x6B4E4817;
    constexpr uint32_t cellSaveDataListLoad2 = 0x7D3A5F4A;
    constexpr uint32_t cellSaveDataAutoSave2 = 0x4DD03A4E;
    constexpr uint32_t cellSaveDataAutoLoad2 = 0xF4CF16CB;
    
    // libgcm_sys
    constexpr uint32_t cellGcmInit = 0xE315A0B2;
    constexpr uint32_t cellGcmSetFlipMode = 0xD9C9D49D;
    constexpr uint32_t cellGcmGetConfiguration = 0x5E2EE0F0;
    constexpr uint32_t cellGcmAddressToOffset = 0xDB7D2DC9;
    
    // libaudio
    constexpr uint32_t cellAudioInit = 0x0B168F92;
    constexpr uint32_t cellAudioQuit = 0x4129FE2D;
    constexpr uint32_t cellAudioPortOpen = 0x74A66AF0;
    constexpr uint32_t cellAudioPortStart = 0x89BE28F2;
    
    // libnet
    constexpr uint32_t cellNetCtlInit = 0xBD5A59FC;
    constexpr uint32_t cellNetCtlTerm = 0x105EE2CB;
    constexpr uint32_t cellNetCtlGetState = 0x04459230;
    constexpr uint32_t cellNetCtlGetInfo = 0x2A72F9ED;
    
    // libtrophy
    constexpr uint32_t cellTrophyInit = 0x3741E81E;
    constexpr uint32_t cellTrophyTerm = 0xE3BF9A28;
    constexpr uint32_t cellTrophyRegisterContext = 0xFF299E03;
    constexpr uint32_t cellTrophyUnlockTrophy = 0x8CEEDD21;
}

// =============================================================================
// Known PS3 Libraries
// =============================================================================

namespace libs {
    constexpr const char* LIBSYSUTIL = "libsysutil";
    constexpr const char* LIBSYSUTIL_SAVEDATA = "libsysutil_savedata";
    constexpr const char* LIBSYSUTIL_GAME = "libsysutil_game";
    constexpr const char* LIBGCM_SYS = "libgcm_sys";
    constexpr const char* LIBAUDIO = "libaudio";
    constexpr const char* LIBSPURS = "libspurs_jq";
    constexpr const char* LIBNET = "libnet";
    constexpr const char* LIBNETCTL = "libnetctl";
    constexpr const char* LIBHTTP = "libhttp";
    constexpr const char* LIBTROPHY = "libnp_trophy";
    constexpr const char* LIBPNGDEC = "libpngdec";
    constexpr const char* LIBJPGDEC = "libjpgdec";
    constexpr const char* LIBVDEC = "libvdec";
    constexpr const char* LIBADEC = "libadec";
    constexpr const char* LIBPAD = "libpad";
    constexpr const char* LIBKB = "libkb";
    constexpr const char* LIBMOUSE = "libmouse";
    constexpr const char* LIBRESC = "libresc";
    constexpr const char* LIBFONT = "libfont";
    constexpr const char* LIBFONTFT = "libfontft";
    constexpr const char* LIBFREETYPE = "libfreetype";
    constexpr const char* LIBLV2 = "liblv2";
    constexpr const char* LIBFS = "libfs";
    constexpr const char* LIBIO = "libio";
    constexpr const char* LIBCAMERA = "libcamera";
    constexpr const char* LIBGEM = "libgem";  // PlayStation Move
    constexpr const char* LIBMIC = "libmic";
    constexpr const char* LIBVOICE = "libvoice";
    constexpr const char* LIBSSL = "libssl";
    constexpr const char* LIBRTC = "librtc";
    constexpr const char* LIBUSBD = "libusbd";
    constexpr const char* LIBL10N = "libl10n";  // Localization
}

} // namespace rpcsx::libraries
