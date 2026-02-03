/**
 * PS3 Game Patches Auto-Installer
 * 
 * Автоматичне завантаження, кешування та застосування патчів
 * для PS3 ігор з онлайн репозиторіїв (RPCS3 wiki).
 * 
 * Особливості:
 * - Автоматичний пошук патчів за Title ID
 * - Кешування патчів локально
 * - YAML парсинг patch.yml формату
 * - Застосування патчів до пам'яті гри
 * - Підтримка різних типів патчів (byte, be16, be32, be64, str)
 * - Умовні патчі на основі версії гри
 */

#ifndef RPCSX_PATCH_INSTALLER_H
#define RPCSX_PATCH_INSTALLER_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <string>
#include <vector>
#include <functional>

namespace rpcsx::patches::installer {

// =============================================================================
// Константи
// =============================================================================

#define PATCH_CACHE_VERSION 2
#define MAX_PATCH_SIZE (16 * 1024 * 1024)  // 16 MB max patch file
#define PATCH_REPO_URL "https://raw.githubusercontent.com/RPCS3/rpcs3/master/bin/patches/patch.yml"

// =============================================================================
// Типи патчів
// =============================================================================

/**
 * Тип даних патча
 */
enum class PatchType : uint8_t {
    BYTE = 0,       // 1 байт
    BE16 = 1,       // 2 байти Big Endian
    LE16 = 2,       // 2 байти Little Endian
    BE32 = 3,       // 4 байти Big Endian
    LE32 = 4,       // 4 байти Little Endian
    BE64 = 5,       // 8 байт Big Endian
    LE64 = 6,       // 8 байт Little Endian
    FLOAT = 7,      // 4 байти float
    DOUBLE = 8,     // 8 байт double
    BYTES = 9,      // Масив байт
    UTF8 = 10,      // UTF-8 рядок
    BSWAP16 = 11,   // Byte swap 16-bit
    BSWAP32 = 12,   // Byte swap 32-bit
    NOP = 13,       // NOP інструкція PS3 (0x60000000)
    BLR = 14,       // BLR інструкція (return)
};

/**
 * Категорія патча
 */
enum class PatchCategory : uint8_t {
    FIX = 0,            // Виправлення багів
    CHEAT = 1,          // Чіти
    ENHANCEMENT = 2,    // Покращення
    PERFORMANCE = 3,    // Оптимізація продуктивності
    GRAPHICS = 4,       // Графічні покращення
    UNLOCK = 5,         // Розблокування функцій
    OTHER = 6           // Інше
};

/**
 * Стан патча
 */
enum class PatchState : uint8_t {
    DISABLED = 0,
    ENABLED = 1,
    AUTO = 2            // Автоматично для рекомендованих
};

// =============================================================================
// Структури даних
// =============================================================================

/**
 * Одна операція патча
 */
struct PatchOperation {
    uint64_t address;           // Адреса в пам'яті PS3
    PatchType type;             // Тип даних
    std::vector<uint8_t> value; // Нове значення
    std::vector<uint8_t> original; // Оригінальне значення (для undo)
    std::string comment;        // Коментар
};

/**
 * Окремий патч
 */
struct Patch {
    std::string hash;           // Унікальний хеш патча
    std::string name;           // Назва патча
    std::string author;         // Автор
    std::string notes;          // Примітки
    std::string version;        // Версія гри для якої патч
    PatchCategory category;     // Категорія
    PatchState state;           // Стан (enabled/disabled)
    bool is_recommended;        // Рекомендований патч
    std::vector<PatchOperation> operations; // Операції патча
};

/**
 * Набір патчів для гри
 */
struct GamePatchSet {
    std::string title_id;       // Title ID гри
    std::string game_name;      // Назва гри
    std::string game_version;   // Версія гри
    uint64_t last_updated;      // Час останнього оновлення
    std::vector<Patch> patches; // Список патчів
};

/**
 * Конфігурація інсталятора
 */
struct InstallerConfig {
    std::string cache_dir;              // Директорія для кешу
    std::string patch_repo_url = PATCH_REPO_URL;
    bool auto_download = true;          // Автозавантаження патчів
    bool auto_apply_recommended = true; // Автозастосування рекомендованих
    bool check_updates = true;          // Перевірка оновлень
    uint32_t update_interval_hours = 24;// Інтервал перевірки
    uint32_t max_cache_age_days = 30;   // Максимальний вік кешу
};

/**
 * Статистика
 */
struct InstallerStats {
    uint32_t total_patches_cached;      // Всього закешованих патчів
    uint32_t patches_applied;           // Застосованих патчів
    uint32_t patches_available;         // Доступних для поточної гри
    uint32_t download_errors;           // Помилок завантаження
    uint64_t cache_size_bytes;          // Розмір кешу
    uint64_t last_update_check;         // Останняк перевірка оновлень
    float apply_success_ratio;          // Відсоток успішного застосування
};

/**
 * Результат завантаження
 */
struct DownloadResult {
    bool success;
    std::string error_message;
    std::string cached_path;
    uint64_t download_time_ms;
};

/**
 * Результат застосування патча
 */
struct ApplyResult {
    bool success;
    uint32_t operations_applied;
    uint32_t operations_failed;
    std::string error_message;
};

// =============================================================================
// Callbacks
// =============================================================================

using DownloadProgressCallback = std::function<void(uint64_t downloaded, uint64_t total)>;
using PatchApplyCallback = std::function<void(const std::string& patch_name, bool success)>;

// =============================================================================
// Глобальний стан
// =============================================================================

extern std::atomic<bool> g_installer_active;
extern std::atomic<uint32_t> g_patches_applied;

// =============================================================================
// API функції
// =============================================================================

/**
 * Ініціалізація інсталятора
 */
bool InitializePatchInstaller(const InstallerConfig& config = InstallerConfig());

/**
 * Завершення роботи
 */
void ShutdownPatchInstaller();

/**
 * Перевірка активності
 */
bool IsInstallerActive();

// --- Завантаження патчів ---

/**
 * Завантажити патчі для гри з репозиторію
 */
DownloadResult DownloadPatchesForGame(const char* title_id,
                                      DownloadProgressCallback progress = nullptr);

/**
 * Оновити кеш патчів
 */
bool UpdatePatchCache();

/**
 * Перевірити наявність оновлень
 */
bool CheckForUpdates(const char* title_id);

// --- Застосування патчів ---

/**
 * Отримати доступні патчі для гри
 */
bool GetAvailablePatches(const char* title_id, GamePatchSet* out_patches);

/**
 * Застосувати патч
 */
ApplyResult ApplyPatch(const char* title_id, const char* patch_hash,
                       void* memory_base, size_t memory_size);

/**
 * Застосувати всі увімкнені патчі для гри
 */
ApplyResult ApplyAllEnabledPatches(const char* title_id,
                                    void* memory_base, size_t memory_size);

/**
 * Застосувати рекомендовані патчі
 */
ApplyResult ApplyRecommendedPatches(const char* title_id,
                                     void* memory_base, size_t memory_size);

/**
 * Скасувати патч (відновити оригінальні значення)
 */
bool RevertPatch(const char* title_id, const char* patch_hash,
                 void* memory_base, size_t memory_size);

/**
 * Скасувати всі патчі
 */
bool RevertAllPatches(const char* title_id, void* memory_base, size_t memory_size);

// --- Керування станом патчів ---

/**
 * Увімкнути/вимкнути патч
 */
bool SetPatchState(const char* title_id, const char* patch_hash, PatchState state);

/**
 * Отримати стан патча
 */
PatchState GetPatchState(const char* title_id, const char* patch_hash);

/**
 * Зберегти стан патчів
 */
bool SavePatchStates(const char* title_id);

/**
 * Завантажити стан патчів
 */
bool LoadPatchStates(const char* title_id);

// --- Кеш ---

/**
 * Очистити кеш патчів
 */
void ClearPatchCache();

/**
 * Очистити кеш для конкретної гри
 */
void ClearGamePatchCache(const char* title_id);

/**
 * Отримати шлях до кешу
 */
const char* GetCachePath();

// --- Статистика ---

/**
 * Отримати статистику
 */
void GetInstallerStats(InstallerStats* stats);

/**
 * Скинути статистику
 */
void ResetInstallerStats();

// --- Утиліти ---

/**
 * Парсинг YAML патча
 */
bool ParsePatchYAML(const char* yaml_content, size_t content_size,
                    const char* title_id, GamePatchSet* out_patches);

/**
 * Експорт патчів у JSON
 */
size_t ExportPatchesToJson(const char* title_id, char* buffer, size_t buffer_size);

/**
 * Імпорт патчів з JSON
 */
bool ImportPatchesFromJson(const char* json, size_t json_size);

/**
 * Валідація патча
 */
bool ValidatePatch(const Patch& patch);

/**
 * Генерація хешу патча
 */
std::string GeneratePatchHash(const Patch& patch);

} // namespace rpcsx::patches::installer

#endif // RPCSX_PATCH_INSTALLER_H
