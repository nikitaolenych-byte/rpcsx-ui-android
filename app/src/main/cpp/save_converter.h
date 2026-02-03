/**
 * PS3 Game Save Compatibility Converter
 * 
 * Converts and manages PS3 save data between different formats and versions.
 * Handles encryption/decryption, format conversion, and cross-region compatibility.
 * 
 * Features:
 * - Save file format detection and conversion
 * - PFD/SFO parsing and modification
 * - Cross-region save compatibility
 * - Save file encryption/decryption
 * - Save data backup and restoration
 * - Trophy save synchronization
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <unordered_map>

namespace rpcsx::saves {

// =============================================================================
// Save Format Types
// =============================================================================

enum class SaveFormat {
    PS3_INTERNAL,       // Внутрішній формат PS3 (зашифрований)
    PS3_DECRYPTED,      // Розшифрований PS3 формат
    RPCS3,              // RPCS3 формат
    RPCSX,              // RPCSX формат
    PSV,                // PlayStation Vita формат (деякі ігри)
    UNKNOWN
};

// =============================================================================
// Region Codes
// =============================================================================

enum class RegionCode {
    NTSC_U,     // США/Канада (BLUS/BCUS)
    NTSC_J,     // Японія (BLJM/BCJM)
    PAL,        // Європа (BLES/BCES)
    ASIA,       // Азія (BLAS/BCAS)
    UNKNOWN
};

// =============================================================================
// Save File Types
// =============================================================================

enum class SaveFileType {
    DATA,           // Звичайні дані
    ICON,           // Іконка (ICON0.PNG)
    PIC,            // Картинка (PIC1.PNG)
    SND,            // Звук (SND0.AT3)
    PARAM_SFO,      // Параметри (PARAM.SFO)
    PARAM_PFD,      // Protected File Descriptor
    TROPSYS,        // Системні трофеї
    TROPUSR,        // Трофеї користувача
    OTHER
};

// =============================================================================
// SFO Parameter Types
// =============================================================================

enum class SFOParamType {
    UTF8_SPECIAL = 0x0004,
    UTF8 = 0x0204,
    INT32 = 0x0404
};

// =============================================================================
// SFO Parameter
// =============================================================================

struct SFOParam {
    std::string key;
    SFOParamType type;
    std::vector<uint8_t> value;
    
    // Helper getters
    std::string GetString() const;
    int32_t GetInt32() const;
    void SetString(const std::string& s);
    void SetInt32(int32_t v);
};

// =============================================================================
// PARAM.SFO Structure
// =============================================================================

struct ParamSFO {
    std::string title;          // TITLE
    std::string title_id;       // TITLE_ID
    std::string subtitle;       // SUB_TITLE
    std::string detail;         // DETAIL
    std::string savedata_dir;   // SAVEDATA_DIRECTORY
    uint32_t attribute;         // ATTRIBUTE
    uint32_t parental_level;    // PARENTAL_LEVEL
    std::string account_id;     // ACCOUNT_ID
    
    std::vector<SFOParam> params;  // Всі параметри
    
    bool Parse(const uint8_t* data, size_t size);
    size_t Serialize(uint8_t* buffer, size_t buffer_size) const;
    
    SFOParam* GetParam(const char* key);
    bool SetParam(const char* key, const std::string& value);
    bool SetParam(const char* key, int32_t value);
};

// =============================================================================
// Save File Entry
// =============================================================================

struct SaveFileEntry {
    std::string filename;
    SaveFileType type;
    uint64_t size;
    uint64_t compressed_size;
    bool is_encrypted;
    uint8_t hash[32];           // SHA-256
    std::string path;           // Повний шлях
};

// =============================================================================
// Save Data
// =============================================================================

struct SaveData {
    std::string title_id;
    std::string dir_name;
    SaveFormat format;
    RegionCode region;
    ParamSFO param_sfo;
    std::vector<SaveFileEntry> files;
    
    uint64_t total_size;
    uint64_t creation_time;
    uint64_t modification_time;
    
    bool is_valid;
    std::string source_path;
};

// =============================================================================
// Conversion Options
// =============================================================================

struct ConversionOptions {
    bool preserve_encryption = false;
    bool convert_region = false;
    RegionCode target_region = RegionCode::UNKNOWN;
    bool update_account_id = true;
    std::string new_account_id;
    bool preserve_timestamps = true;
    bool validate_after_conversion = true;
    bool create_backup = true;
};

// =============================================================================
// Conversion Result
// =============================================================================

struct ConversionResult {
    bool success;
    std::string error_message;
    std::string output_path;
    size_t files_converted;
    size_t files_failed;
    std::vector<std::string> warnings;
};

// =============================================================================
// Configuration
// =============================================================================

struct SaveConverterConfig {
    bool enabled = true;
    std::string save_directory;         // Шлях до збережень
    std::string backup_directory;       // Шлях до бекапів
    std::string temp_directory;         // Тимчасова директорія
    bool auto_detect_format = true;
    bool auto_decrypt = true;
    bool log_operations = true;
};

// =============================================================================
// Statistics
// =============================================================================

struct ConverterStats {
    uint32_t saves_scanned;
    uint32_t saves_converted;
    uint32_t saves_failed;
    uint32_t backups_created;
    uint64_t total_bytes_processed;
};

// =============================================================================
// Global State
// =============================================================================

extern std::atomic<bool> g_converter_active;
extern std::atomic<uint32_t> g_conversions_count;

// =============================================================================
// API Functions
// =============================================================================

/**
 * Ініціалізація конвертера
 */
bool InitializeSaveConverter(const SaveConverterConfig& config);

/**
 * Завершення роботи
 */
void ShutdownSaveConverter();

/**
 * Чи активний конвертер
 */
bool IsConverterActive();

/**
 * Сканувати директорію збережень
 */
size_t ScanSaveDirectory(const char* path, SaveData* buffer, size_t buffer_size);

/**
 * Завантажити збереження
 */
bool LoadSaveData(const char* path, SaveData* save);

/**
 * Визначити формат збереження
 */
SaveFormat DetectSaveFormat(const char* path);

/**
 * Визначити регіон за Title ID
 */
RegionCode DetectRegionFromTitleId(const char* title_id);

/**
 * Конвертувати збереження
 */
ConversionResult ConvertSave(const char* source_path, const char* dest_path,
                              SaveFormat target_format, const ConversionOptions& options);

/**
 * Конвертувати регіон збереження
 */
ConversionResult ConvertSaveRegion(const char* path, RegionCode target_region);

/**
 * Шифрувати збереження
 */
bool EncryptSaveData(const char* path, const char* output_path);

/**
 * Розшифрувати збереження
 */
bool DecryptSaveData(const char* path, const char* output_path);

/**
 * Парсити PARAM.SFO
 */
bool ParseParamSFO(const char* path, ParamSFO* sfo);

/**
 * Зберегти PARAM.SFO
 */
bool SaveParamSFO(const char* path, const ParamSFO& sfo);

/**
 * Модифікувати Title ID
 */
bool ModifyTitleId(const char* save_path, const char* new_title_id);

/**
 * Модифікувати Account ID
 */
bool ModifyAccountId(const char* save_path, const char* new_account_id);

/**
 * Створити бекап
 */
bool CreateBackup(const char* save_path, const char* backup_path);

/**
 * Відновити з бекапу
 */
bool RestoreFromBackup(const char* backup_path, const char* dest_path);

/**
 * Валідувати збереження
 */
bool ValidateSaveData(const char* path, std::string* error_message = nullptr);

/**
 * Виправити пошкоджене збереження
 */
bool RepairSaveData(const char* path);

/**
 * Отримати список файлів збереження
 */
size_t GetSaveFiles(const char* save_path, SaveFileEntry* buffer, size_t buffer_size);

/**
 * Експортувати файл зі збереження
 */
bool ExportSaveFile(const char* save_path, const char* filename, const char* dest_path);

/**
 * Імпортувати файл у збереження
 */
bool ImportSaveFile(const char* save_path, const char* filename, const char* source_path);

/**
 * Отримати ICON0.PNG
 */
size_t GetSaveIcon(const char* save_path, uint8_t* buffer, size_t buffer_size);

/**
 * Отримати статистику
 */
void GetConverterStats(ConverterStats* stats);

/**
 * Скинути статистику
 */
void ResetConverterStats();

/**
 * Експорт інформації в JSON
 */
size_t ExportSaveInfoJson(const SaveData& save, char* buffer, size_t buffer_size);

/**
 * Отримати Title ID з збереження
 */
bool GetSaveTitleId(const char* path, char* buffer, size_t buffer_size);

/**
 * Перевірити сумісність збережень
 */
bool CheckSaveCompatibility(const char* save_path, const char* game_title_id);

// =============================================================================
// Region Conversion Helpers
// =============================================================================

/**
 * Отримати еквівалентний Title ID для іншого регіону
 */
bool GetEquivalentTitleId(const char* title_id, RegionCode target_region,
                           char* buffer, size_t buffer_size);

/**
 * Перевірити чи Title ID відповідає регіону
 */
bool IsTitleIdForRegion(const char* title_id, RegionCode region);

/**
 * Отримати назву регіону
 */
const char* GetRegionName(RegionCode region);

/**
 * Отримати код регіону з Title ID
 */
RegionCode GetRegionFromTitleId(const char* title_id);

// =============================================================================
// Known Title ID Mappings
// =============================================================================

namespace title_mapping {
    // Demon's Souls
    constexpr const char* DEMONS_SOULS_US = "BLUS30443";
    constexpr const char* DEMONS_SOULS_EU = "BLES00932";
    constexpr const char* DEMONS_SOULS_JP = "BCJM95402";
    
    // The Last of Us
    constexpr const char* TLOU_US = "BCUS98174";
    constexpr const char* TLOU_EU = "BCES01584";
    
    // Uncharted 2
    constexpr const char* UNCHARTED2_US = "BCUS98123";
    constexpr const char* UNCHARTED2_EU = "BCES00757";
    
    // God of War III
    constexpr const char* GOW3_US = "BCUS98111";
    constexpr const char* GOW3_EU = "BCES00510";
    
    // Gran Turismo 5
    constexpr const char* GT5_US = "BCUS98114";
    constexpr const char* GT5_EU = "BCES00569";
}

} // namespace rpcsx::saves
