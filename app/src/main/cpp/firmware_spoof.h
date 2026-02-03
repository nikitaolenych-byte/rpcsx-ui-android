/**
 * PS3 Firmware Version Spoofing System
 * 
 * Allows spoofing the PS3 firmware version to improve game compatibility.
 * Some games check for specific firmware versions and refuse to run on older versions.
 * 
 * Features:
 * - Global firmware version spoofing
 * - Per-game firmware version overrides
 * - SDK version spoofing
 * - System software version reporting
 * - Firmware feature flag emulation
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <unordered_map>

namespace rpcsx::firmware {

// =============================================================================
// PS3 Firmware Version Structure
// =============================================================================

struct FirmwareVersion {
    uint8_t major;      // Major version (e.g., 4)
    uint8_t minor;      // Minor version (e.g., 90)
    uint16_t build;     // Build number (e.g., 2)
    
    // Повертає версію як рядок "X.YY"
    std::string ToString() const {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d.%02d", major, minor);
        return buf;
    }
    
    // Повертає повну версію як рядок "X.YY.BBBB"
    std::string ToFullString() const {
        char buf[24];
        snprintf(buf, sizeof(buf), "%d.%02d.%04d", major, minor, build);
        return buf;
    }
    
    // Конвертує у 32-бітне значення PS3 формату
    uint32_t ToPS3Format() const {
        // PS3 format: 0xMMmmbbbb (major, minor, build)
        return (major << 24) | (minor << 16) | build;
    }
    
    // Парсить з PS3 формату
    static FirmwareVersion FromPS3Format(uint32_t value) {
        FirmwareVersion v;
        v.major = (value >> 24) & 0xFF;
        v.minor = (value >> 16) & 0xFF;
        v.build = value & 0xFFFF;
        return v;
    }
    
    // Парсить з рядка "X.YY" або "X.YY.BBBB"
    static FirmwareVersion Parse(const char* str);
    
    // Порівняння
    bool operator<(const FirmwareVersion& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return build < other.build;
    }
    
    bool operator>=(const FirmwareVersion& other) const {
        return !(*this < other);
    }
    
    bool operator==(const FirmwareVersion& other) const {
        return major == other.major && minor == other.minor && build == other.build;
    }
};

// =============================================================================
// SDK Version
// =============================================================================

struct SDKVersion {
    uint32_t version;   // SDK version in PS3 format
    
    std::string ToString() const {
        char buf[24];
        snprintf(buf, sizeof(buf), "0x%08X", version);
        return buf;
    }
    
    // Отримати major.minor
    uint8_t Major() const { return (version >> 24) & 0xFF; }
    uint8_t Minor() const { return (version >> 16) & 0xFF; }
};

// =============================================================================
// Firmware Features (introduced in different versions)
// =============================================================================

enum class FirmwareFeature : uint32_t {
    TROPHY_SUPPORT          = (1 << 0),   // 2.40+
    VIDEO_EDITOR            = (1 << 1),   // 3.00+
    REMOTE_PLAY             = (1 << 2),   // 3.50+
    PHOTO_GALLERY           = (1 << 3),   // 3.00+
    GAME_DATA_UTILITY       = (1 << 4),   // 2.00+
    IN_GAME_XMB             = (1 << 5),   // 2.41+
    DYNAMIC_THEMES          = (1 << 6),   // 3.00+
    BLU_RAY_3D              = (1 << 7),   // 3.50+
    VOICE_CHAT              = (1 << 8),   // 3.00+
    STORE_V2                = (1 << 9),   // 4.00+
    VITA_CONNECTIVITY       = (1 << 10),  // 4.10+
    MOVE_CALIBRATION        = (1 << 11),  // 3.60+
    AUTO_UPDATE             = (1 << 12),  // 4.00+
    CLOUD_SAVES             = (1 << 13),  // 4.11+
    SUBTITLE_DOWNLOAD       = (1 << 14),  // 4.30+
    ALL_FEATURES            = 0xFFFFFFFF
};

// =============================================================================
// Known Firmware Versions
// =============================================================================

namespace known_versions {
    // Популярні версії прошивок
    constexpr FirmwareVersion V1_00 = {1, 0, 0};
    constexpr FirmwareVersion V2_00 = {2, 0, 0};
    constexpr FirmwareVersion V2_40 = {2, 40, 0};  // Trophies
    constexpr FirmwareVersion V2_41 = {2, 41, 0};  // In-game XMB
    constexpr FirmwareVersion V3_00 = {3, 0, 0};
    constexpr FirmwareVersion V3_50 = {3, 50, 0};
    constexpr FirmwareVersion V3_55 = {3, 55, 0};  // Популярна CFW версія
    constexpr FirmwareVersion V4_00 = {4, 0, 0};
    constexpr FirmwareVersion V4_21 = {4, 21, 0};
    constexpr FirmwareVersion V4_40 = {4, 40, 0};
    constexpr FirmwareVersion V4_50 = {4, 50, 0};
    constexpr FirmwareVersion V4_60 = {4, 60, 0};
    constexpr FirmwareVersion V4_70 = {4, 70, 0};
    constexpr FirmwareVersion V4_80 = {4, 80, 0};
    constexpr FirmwareVersion V4_84 = {4, 84, 0};
    constexpr FirmwareVersion V4_85 = {4, 85, 0};
    constexpr FirmwareVersion V4_86 = {4, 86, 0};
    constexpr FirmwareVersion V4_87 = {4, 87, 0};
    constexpr FirmwareVersion V4_88 = {4, 88, 0};
    constexpr FirmwareVersion V4_89 = {4, 89, 0};
    constexpr FirmwareVersion V4_90 = {4, 90, 0};  // Остання
    
    // За замовчуванням спуфимо останню
    constexpr FirmwareVersion DEFAULT = V4_90;
}

// =============================================================================
// Spoof Configuration
// =============================================================================

struct SpoofConfig {
    bool enabled = true;
    FirmwareVersion global_version = known_versions::DEFAULT;
    bool spoof_sdk_version = true;
    SDKVersion sdk_version = {0x00490001};  // 4.90.0001
    bool enable_all_features = true;
    uint32_t enabled_features = static_cast<uint32_t>(FirmwareFeature::ALL_FEATURES);
    std::string system_software_string = "4.90";
};

// =============================================================================
// Per-Game Override
// =============================================================================

struct GameFirmwareOverride {
    std::string title_id;
    FirmwareVersion version;
    SDKVersion sdk;
    uint32_t features;
    std::string reason;
};

// =============================================================================
// Spoof Statistics
// =============================================================================

struct SpoofStats {
    uint64_t version_checks;
    uint64_t sdk_checks;
    uint64_t feature_checks;
    uint64_t spoofed_responses;
    uint32_t unique_games_spoofed;
};

// =============================================================================
// Global State
// =============================================================================

extern std::atomic<bool> g_spoof_active;
extern std::atomic<uint64_t> g_spoof_count;

// =============================================================================
// API Functions
// =============================================================================

/**
 * Ініціалізація системи спуфінгу
 */
bool InitializeFirmwareSpoof(const SpoofConfig& config);

/**
 * Завершення роботи
 */
void ShutdownFirmwareSpoof();

/**
 * Чи активний спуфінг
 */
bool IsSpoofActive();

/**
 * Встановити глобальну версію прошивки
 */
void SetGlobalFirmwareVersion(const FirmwareVersion& version);

/**
 * Отримати поточну спуфнуту версію прошивки
 */
FirmwareVersion GetSpoofedFirmwareVersion(const char* title_id = nullptr);

/**
 * Отримати версію прошивки у PS3 форматі
 */
uint32_t GetSpoofedFirmwarePS3Format(const char* title_id = nullptr);

/**
 * Встановити SDK версію
 */
void SetSDKVersion(const SDKVersion& sdk);

/**
 * Отримати спуфнуту SDK версію
 */
SDKVersion GetSpoofedSDKVersion(const char* title_id = nullptr);

/**
 * Перевірити чи підтримується фіча
 */
bool IsFeatureSupported(FirmwareFeature feature);

/**
 * Увімкнути/вимкнути фічу
 */
void SetFeatureEnabled(FirmwareFeature feature, bool enabled);

/**
 * Отримати рядок версії для системи
 */
const char* GetSystemSoftwareString();

/**
 * Встановити override для гри
 */
bool SetGameFirmwareOverride(const char* title_id, const FirmwareVersion& version,
                              const char* reason = nullptr);

/**
 * Видалити override для гри
 */
bool RemoveGameFirmwareOverride(const char* title_id);

/**
 * Отримати override для гри
 */
bool GetGameFirmwareOverride(const char* title_id, GameFirmwareOverride* out);

/**
 * Отримати всі overrides
 */
size_t GetAllGameOverrides(GameFirmwareOverride* buffer, size_t buffer_size);

/**
 * Завантажити overrides з файлу
 */
bool LoadFirmwareOverrides(const char* filepath);

/**
 * Зберегти overrides у файл
 */
bool SaveFirmwareOverrides(const char* filepath);

/**
 * Отримати рекомендовану версію для гри
 * (на основі вимог гри)
 */
FirmwareVersion GetRecommendedVersion(const char* title_id);

/**
 * Отримати мінімальну версію для гри
 */
FirmwareVersion GetMinimumRequiredVersion(const char* title_id);

/**
 * Встановити поточний контекст гри
 */
void SetCurrentGameContext(const char* title_id);

/**
 * Отримати статистику
 */
void GetSpoofStats(SpoofStats* stats);

/**
 * Скинути статистику
 */
void ResetSpoofStats();

/**
 * Експорт конфігурації в JSON
 */
size_t ExportConfigJson(char* buffer, size_t buffer_size);

/**
 * Імпорт конфігурації з JSON
 */
bool ImportConfigJson(const char* json, size_t json_size);

/**
 * Отримати список відомих версій
 */
size_t GetKnownVersions(FirmwareVersion* buffer, size_t buffer_size);

/**
 * Отримати опис версії
 */
const char* GetVersionDescription(const FirmwareVersion& version);

} // namespace rpcsx::firmware
