/**
 * PS3 Game Save Compatibility Converter Implementation
 */

#include "save_converter.h"
#include <android/log.h>
#include <mutex>
#include <fstream>
#include <sstream>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

#define LOG_TAG "RPCSX-SaveConverter"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::saves {

// Глобальні атомарні
std::atomic<bool> g_converter_active{false};
std::atomic<uint32_t> g_conversions_count{0};

// =============================================================================
// SFO Magic and Constants
// =============================================================================

constexpr uint32_t SFO_MAGIC = 0x46535000;  // "\0PSF"
constexpr uint32_t SFO_VERSION = 0x00000101;

#pragma pack(push, 1)
struct SFOHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t key_table_offset;
    uint32_t data_table_offset;
    uint32_t entry_count;
};

struct SFOEntry {
    uint16_t key_offset;
    uint16_t param_format;
    uint32_t param_len;
    uint32_t param_max_len;
    uint32_t data_offset;
};
#pragma pack(pop)

// =============================================================================
// Title ID Region Mappings
// =============================================================================

struct TitleIdMapping {
    const char* base_name;
    const char* us_id;
    const char* eu_id;
    const char* jp_id;
    const char* asia_id;
};

static const TitleIdMapping TITLE_MAPPINGS[] = {
    {"Demon's Souls",    "BLUS30443", "BLES00932", "BCJM95402", nullptr},
    {"The Last of Us",   "BCUS98174", "BCES01584", nullptr,     nullptr},
    {"Uncharted 2",      "BCUS98123", "BCES00757", nullptr,     nullptr},
    {"Uncharted 3",      "BCUS98233", "BCES01175", nullptr,     nullptr},
    {"God of War III",   "BCUS98111", "BCES00510", "BCJM95402", nullptr},
    {"Heavy Rain",       "BCUS98164", "BCES00797", nullptr,     nullptr},
    {"Gran Turismo 5",   "BCUS98114", "BCES00569", "BCJM95001", nullptr},
    {"Persona 5",        "BLUS31604", "BLES02247", "BLJM61346", nullptr},
    {"Metal Gear Solid 4", "BLUS30109", "BLES00246", "BLJM60001", nullptr},
    {"Red Dead Redemption", "BLUS30418", "BLES00680", nullptr,  nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}
};

// =============================================================================
// SFOParam Implementation
// =============================================================================

std::string SFOParam::GetString() const {
    if (value.empty()) return "";
    return std::string(reinterpret_cast<const char*>(value.data()), 
                       strnlen(reinterpret_cast<const char*>(value.data()), value.size()));
}

int32_t SFOParam::GetInt32() const {
    if (value.size() < 4) return 0;
    return *reinterpret_cast<const int32_t*>(value.data());
}

void SFOParam::SetString(const std::string& s) {
    type = SFOParamType::UTF8;
    value.resize(s.size() + 1);
    memcpy(value.data(), s.c_str(), s.size() + 1);
}

void SFOParam::SetInt32(int32_t v) {
    type = SFOParamType::INT32;
    value.resize(4);
    memcpy(value.data(), &v, 4);
}

// =============================================================================
// ParamSFO Implementation
// =============================================================================

bool ParamSFO::Parse(const uint8_t* data, size_t size) {
    if (size < sizeof(SFOHeader)) return false;
    
    const SFOHeader* header = reinterpret_cast<const SFOHeader*>(data);
    
    if (header->magic != SFO_MAGIC) {
        LOGE("Invalid SFO magic: 0x%08X", header->magic);
        return false;
    }
    
    const SFOEntry* entries = reinterpret_cast<const SFOEntry*>(data + sizeof(SFOHeader));
    const char* key_table = reinterpret_cast<const char*>(data + header->key_table_offset);
    const uint8_t* data_table = data + header->data_table_offset;
    
    params.clear();
    
    for (uint32_t i = 0; i < header->entry_count; ++i) {
        const SFOEntry& entry = entries[i];
        
        SFOParam param;
        param.key = key_table + entry.key_offset;
        param.type = static_cast<SFOParamType>(entry.param_format);
        param.value.resize(entry.param_len);
        memcpy(param.value.data(), data_table + entry.data_offset, entry.param_len);
        
        // Витягуємо відомі параметри
        if (param.key == "TITLE") {
            title = param.GetString();
        } else if (param.key == "TITLE_ID") {
            title_id = param.GetString();
        } else if (param.key == "SUB_TITLE") {
            subtitle = param.GetString();
        } else if (param.key == "DETAIL") {
            detail = param.GetString();
        } else if (param.key == "SAVEDATA_DIRECTORY") {
            savedata_dir = param.GetString();
        } else if (param.key == "ATTRIBUTE") {
            attribute = param.GetInt32();
        } else if (param.key == "PARENTAL_LEVEL") {
            parental_level = param.GetInt32();
        } else if (param.key == "ACCOUNT_ID") {
            account_id = param.GetString();
        }
        
        params.push_back(param);
    }
    
    return true;
}

size_t ParamSFO::Serialize(uint8_t* buffer, size_t buffer_size) const {
    // Обчислюємо розміри
    size_t key_table_size = 0;
    size_t data_table_size = 0;
    
    for (const auto& param : params) {
        key_table_size += param.key.size() + 1;
        size_t max_len = ((param.value.size() + 3) / 4) * 4;  // Align to 4
        data_table_size += max_len;
    }
    
    size_t total_size = sizeof(SFOHeader) + 
                        params.size() * sizeof(SFOEntry) +
                        key_table_size + data_table_size;
    
    if (buffer_size < total_size) return 0;
    
    memset(buffer, 0, total_size);
    
    // Header
    SFOHeader* header = reinterpret_cast<SFOHeader*>(buffer);
    header->magic = SFO_MAGIC;
    header->version = SFO_VERSION;
    header->entry_count = params.size();
    header->key_table_offset = sizeof(SFOHeader) + params.size() * sizeof(SFOEntry);
    header->data_table_offset = header->key_table_offset + key_table_size;
    
    // Entries, keys, data
    SFOEntry* entries = reinterpret_cast<SFOEntry*>(buffer + sizeof(SFOHeader));
    char* key_table = reinterpret_cast<char*>(buffer + header->key_table_offset);
    uint8_t* data_table = buffer + header->data_table_offset;
    
    uint16_t key_offset = 0;
    uint32_t data_offset = 0;
    
    for (size_t i = 0; i < params.size(); ++i) {
        const auto& param = params[i];
        
        entries[i].key_offset = key_offset;
        entries[i].param_format = static_cast<uint16_t>(param.type);
        entries[i].param_len = param.value.size();
        entries[i].param_max_len = ((param.value.size() + 3) / 4) * 4;
        entries[i].data_offset = data_offset;
        
        memcpy(key_table + key_offset, param.key.c_str(), param.key.size() + 1);
        memcpy(data_table + data_offset, param.value.data(), param.value.size());
        
        key_offset += param.key.size() + 1;
        data_offset += entries[i].param_max_len;
    }
    
    return total_size;
}

SFOParam* ParamSFO::GetParam(const char* key) {
    for (auto& param : params) {
        if (param.key == key) {
            return &param;
        }
    }
    return nullptr;
}

bool ParamSFO::SetParam(const char* key, const std::string& value) {
    for (auto& param : params) {
        if (param.key == key) {
            param.SetString(value);
            
            // Оновлюємо відомі поля
            if (param.key == "TITLE") title = value;
            else if (param.key == "TITLE_ID") title_id = value;
            else if (param.key == "ACCOUNT_ID") account_id = value;
            
            return true;
        }
    }
    return false;
}

bool ParamSFO::SetParam(const char* key, int32_t value) {
    for (auto& param : params) {
        if (param.key == key) {
            param.SetInt32(value);
            return true;
        }
    }
    return false;
}

// =============================================================================
// Internal System
// =============================================================================

class SaveConverterSystem {
public:
    SaveConverterConfig config;
    ConverterStats stats = {};
    std::mutex mutex;
    
    bool Initialize(const SaveConverterConfig& cfg) {
        config = cfg;
        stats = {};
        
        g_converter_active.store(true);
        g_conversions_count.store(0);
        
        LOGI("╔════════════════════════════════════════════════════════════╗");
        LOGI("║       PS3 Game Save Compatibility Converter                ║");
        LOGI("╠════════════════════════════════════════════════════════════╣");
        LOGI("║  Save Directory: %-40s ║", config.save_directory.c_str());
        LOGI("║  Auto Detect Format: %-6s                                 ║", config.auto_detect_format ? "Yes" : "No");
        LOGI("║  Auto Decrypt: %-6s                                       ║", config.auto_decrypt ? "Yes" : "No");
        LOGI("╚════════════════════════════════════════════════════════════╝");
        
        return true;
    }
    
    void Shutdown() {
        g_converter_active.store(false);
        LOGI("Save Converter shutdown. Conversions: %u", g_conversions_count.load());
    }
    
    SaveFormat DetectFormat(const char* path) {
        if (!path) return SaveFormat::UNKNOWN;
        
        std::string p = path;
        
        // Перевіряємо наявність PARAM.SFO
        std::string sfo_path = p + "/PARAM.SFO";
        struct stat st;
        if (stat(sfo_path.c_str(), &st) != 0) {
            return SaveFormat::UNKNOWN;
        }
        
        // Перевіряємо PARAM.PFD для зашифрованих
        std::string pfd_path = p + "/PARAM.PFD";
        if (stat(pfd_path.c_str(), &st) == 0) {
            return SaveFormat::PS3_INTERNAL;
        }
        
        // Перевіряємо RPCSX маркер
        std::string rpcsx_marker = p + "/.rpcsx_save";
        if (stat(rpcsx_marker.c_str(), &st) == 0) {
            return SaveFormat::RPCSX;
        }
        
        // RPCS3 формат (без шифрування, без маркера)
        return SaveFormat::RPCS3;
    }
    
    RegionCode DetectRegion(const char* title_id) {
        if (!title_id || strlen(title_id) < 4) return RegionCode::UNKNOWN;
        
        char prefix[5] = {0};
        strncpy(prefix, title_id, 4);
        
        if (strcmp(prefix, "BLUS") == 0 || strcmp(prefix, "BCUS") == 0) {
            return RegionCode::NTSC_U;
        }
        if (strcmp(prefix, "BLES") == 0 || strcmp(prefix, "BCES") == 0) {
            return RegionCode::PAL;
        }
        if (strcmp(prefix, "BLJM") == 0 || strcmp(prefix, "BCJM") == 0) {
            return RegionCode::NTSC_J;
        }
        if (strcmp(prefix, "BLAS") == 0 || strcmp(prefix, "BCAS") == 0) {
            return RegionCode::ASIA;
        }
        
        return RegionCode::UNKNOWN;
    }
    
    bool LoadSFO(const char* path, ParamSFO* sfo) {
        if (!path || !sfo) return false;
        
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        
        return sfo->Parse(data.data(), data.size());
    }
    
    bool SaveSFO(const char* path, const ParamSFO& sfo) {
        std::vector<uint8_t> buffer(4096);
        size_t size = sfo.Serialize(buffer.data(), buffer.size());
        if (size == 0) return false;
        
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        file.write(reinterpret_cast<const char*>(buffer.data()), size);
        return true;
    }
    
    bool CopyFile(const char* src, const char* dst) {
        std::ifstream in(src, std::ios::binary);
        if (!in.is_open()) return false;
        
        std::ofstream out(dst, std::ios::binary);
        if (!out.is_open()) return false;
        
        out << in.rdbuf();
        return true;
    }
    
    bool CopyDirectory(const char* src, const char* dst) {
        mkdir(dst, 0755);
        
        DIR* dir = opendir(src);
        if (!dir) return false;
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            std::string src_path = std::string(src) + "/" + entry->d_name;
            std::string dst_path = std::string(dst) + "/" + entry->d_name;
            
            struct stat st;
            if (stat(src_path.c_str(), &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    CopyDirectory(src_path.c_str(), dst_path.c_str());
                } else {
                    CopyFile(src_path.c_str(), dst_path.c_str());
                }
            }
        }
        
        closedir(dir);
        return true;
    }
};

static SaveConverterSystem g_system;

// =============================================================================
// API Implementation
// =============================================================================

bool InitializeSaveConverter(const SaveConverterConfig& config) {
    return g_system.Initialize(config);
}

void ShutdownSaveConverter() {
    g_system.Shutdown();
}

bool IsConverterActive() {
    return g_converter_active.load();
}

size_t ScanSaveDirectory(const char* path, SaveData* buffer, size_t buffer_size) {
    if (!path) return 0;
    
    DIR* dir = opendir(path);
    if (!dir) return 0;
    
    size_t count = 0;
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        
        std::string save_path = std::string(path) + "/" + entry->d_name;
        
        struct stat st;
        if (stat(save_path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }
        
        // Перевіряємо чи це збереження (має PARAM.SFO)
        std::string sfo_path = save_path + "/PARAM.SFO";
        if (stat(sfo_path.c_str(), &st) != 0) {
            continue;
        }
        
        if (count < buffer_size) {
            LoadSaveData(save_path.c_str(), &buffer[count]);
        }
        count++;
        
        g_system.stats.saves_scanned++;
    }
    
    closedir(dir);
    return count;
}

bool LoadSaveData(const char* path, SaveData* save) {
    if (!path || !save) return false;
    
    *save = SaveData();
    save->source_path = path;
    
    // Завантажуємо PARAM.SFO
    std::string sfo_path = std::string(path) + "/PARAM.SFO";
    if (!g_system.LoadSFO(sfo_path.c_str(), &save->param_sfo)) {
        save->is_valid = false;
        return false;
    }
    
    save->title_id = save->param_sfo.title_id;
    save->dir_name = save->param_sfo.savedata_dir;
    save->format = g_system.DetectFormat(path);
    save->region = g_system.DetectRegion(save->title_id.c_str());
    save->is_valid = true;
    
    // Отримуємо список файлів
    DIR* dir = opendir(path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            
            std::string file_path = std::string(path) + "/" + entry->d_name;
            struct stat st;
            if (stat(file_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                SaveFileEntry file_entry;
                file_entry.filename = entry->d_name;
                file_entry.size = st.st_size;
                file_entry.path = file_path;
                
                // Визначаємо тип
                if (strcmp(entry->d_name, "PARAM.SFO") == 0) {
                    file_entry.type = SaveFileType::PARAM_SFO;
                } else if (strcmp(entry->d_name, "PARAM.PFD") == 0) {
                    file_entry.type = SaveFileType::PARAM_PFD;
                } else if (strcmp(entry->d_name, "ICON0.PNG") == 0) {
                    file_entry.type = SaveFileType::ICON;
                } else if (strcmp(entry->d_name, "PIC1.PNG") == 0) {
                    file_entry.type = SaveFileType::PIC;
                } else {
                    file_entry.type = SaveFileType::DATA;
                }
                
                save->files.push_back(file_entry);
                save->total_size += st.st_size;
            }
        }
        closedir(dir);
    }
    
    return true;
}

SaveFormat DetectSaveFormat(const char* path) {
    return g_system.DetectFormat(path);
}

RegionCode DetectRegionFromTitleId(const char* title_id) {
    return g_system.DetectRegion(title_id);
}

ConversionResult ConvertSave(const char* source_path, const char* dest_path,
                              SaveFormat target_format, const ConversionOptions& options) {
    ConversionResult result = {false, "", "", 0, 0, {}};
    
    if (!source_path || !dest_path) {
        result.error_message = "Invalid paths";
        return result;
    }
    
    // Завантажуємо вихідне збереження
    SaveData save;
    if (!LoadSaveData(source_path, &save)) {
        result.error_message = "Failed to load source save";
        return result;
    }
    
    // Створюємо бекап якщо потрібно
    if (options.create_backup) {
        std::string backup_path = std::string(source_path) + ".backup";
        g_system.CopyDirectory(source_path, backup_path.c_str());
        g_system.stats.backups_created++;
    }
    
    // Копіюємо файли
    if (!g_system.CopyDirectory(source_path, dest_path)) {
        result.error_message = "Failed to copy save files";
        return result;
    }
    
    // Модифікуємо PARAM.SFO якщо потрібно
    if (options.update_account_id && !options.new_account_id.empty()) {
        save.param_sfo.SetParam("ACCOUNT_ID", options.new_account_id);
    }
    
    // Зберігаємо модифікований SFO
    std::string dest_sfo = std::string(dest_path) + "/PARAM.SFO";
    g_system.SaveSFO(dest_sfo.c_str(), save.param_sfo);
    
    // Додаємо RPCSX маркер
    if (target_format == SaveFormat::RPCSX) {
        std::string marker = std::string(dest_path) + "/.rpcsx_save";
        std::ofstream(marker).close();
    }
    
    result.success = true;
    result.output_path = dest_path;
    result.files_converted = save.files.size();
    
    g_system.stats.saves_converted++;
    g_conversions_count++;
    
    LOGI("Converted save %s -> %s", source_path, dest_path);
    
    return result;
}

ConversionResult ConvertSaveRegion(const char* path, RegionCode target_region) {
    ConversionResult result = {false, "", "", 0, 0, {}};
    
    SaveData save;
    if (!LoadSaveData(path, &save)) {
        result.error_message = "Failed to load save";
        return result;
    }
    
    // Отримуємо еквівалентний Title ID
    char new_title_id[16] = {0};
    if (!GetEquivalentTitleId(save.title_id.c_str(), target_region, new_title_id, sizeof(new_title_id))) {
        result.error_message = "No equivalent title ID found";
        return result;
    }
    
    // Модифікуємо Title ID
    save.param_sfo.SetParam("TITLE_ID", new_title_id);
    
    std::string sfo_path = std::string(path) + "/PARAM.SFO";
    if (!g_system.SaveSFO(sfo_path.c_str(), save.param_sfo)) {
        result.error_message = "Failed to save PARAM.SFO";
        return result;
    }
    
    result.success = true;
    result.output_path = path;
    
    return result;
}

bool EncryptSaveData(const char* path, const char* output_path) {
    // TODO: Реалізувати шифрування
    LOGW("Save encryption not implemented");
    return false;
}

bool DecryptSaveData(const char* path, const char* output_path) {
    // TODO: Реалізувати розшифрування
    LOGW("Save decryption not implemented");
    return false;
}

bool ParseParamSFO(const char* path, ParamSFO* sfo) {
    return g_system.LoadSFO(path, sfo);
}

bool SaveParamSFO(const char* path, const ParamSFO& sfo) {
    return g_system.SaveSFO(path, sfo);
}

bool ModifyTitleId(const char* save_path, const char* new_title_id) {
    if (!save_path || !new_title_id) return false;
    
    std::string sfo_path = std::string(save_path) + "/PARAM.SFO";
    ParamSFO sfo;
    
    if (!g_system.LoadSFO(sfo_path.c_str(), &sfo)) return false;
    if (!sfo.SetParam("TITLE_ID", new_title_id)) return false;
    
    return g_system.SaveSFO(sfo_path.c_str(), sfo);
}

bool ModifyAccountId(const char* save_path, const char* new_account_id) {
    if (!save_path || !new_account_id) return false;
    
    std::string sfo_path = std::string(save_path) + "/PARAM.SFO";
    ParamSFO sfo;
    
    if (!g_system.LoadSFO(sfo_path.c_str(), &sfo)) return false;
    if (!sfo.SetParam("ACCOUNT_ID", new_account_id)) return false;
    
    return g_system.SaveSFO(sfo_path.c_str(), sfo);
}

bool CreateBackup(const char* save_path, const char* backup_path) {
    if (g_system.CopyDirectory(save_path, backup_path)) {
        g_system.stats.backups_created++;
        return true;
    }
    return false;
}

bool RestoreFromBackup(const char* backup_path, const char* dest_path) {
    return g_system.CopyDirectory(backup_path, dest_path);
}

bool ValidateSaveData(const char* path, std::string* error_message) {
    SaveData save;
    if (!LoadSaveData(path, &save)) {
        if (error_message) *error_message = "Failed to load save data";
        return false;
    }
    
    if (save.title_id.empty()) {
        if (error_message) *error_message = "Missing TITLE_ID";
        return false;
    }
    
    return true;
}

bool RepairSaveData(const char* path) {
    // TODO: Реалізувати відновлення
    return false;
}

size_t GetSaveFiles(const char* save_path, SaveFileEntry* buffer, size_t buffer_size) {
    SaveData save;
    if (!LoadSaveData(save_path, &save)) return 0;
    
    size_t count = 0;
    for (const auto& file : save.files) {
        if (count < buffer_size) {
            buffer[count] = file;
        }
        count++;
    }
    return count;
}

bool ExportSaveFile(const char* save_path, const char* filename, const char* dest_path) {
    std::string src = std::string(save_path) + "/" + filename;
    return g_system.CopyFile(src.c_str(), dest_path);
}

bool ImportSaveFile(const char* save_path, const char* filename, const char* source_path) {
    std::string dst = std::string(save_path) + "/" + filename;
    return g_system.CopyFile(source_path, dst.c_str());
}

size_t GetSaveIcon(const char* save_path, uint8_t* buffer, size_t buffer_size) {
    std::string icon_path = std::string(save_path) + "/ICON0.PNG";
    
    std::ifstream file(icon_path, std::ios::binary);
    if (!file.is_open()) return 0;
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (size > buffer_size) return 0;
    
    file.read(reinterpret_cast<char*>(buffer), size);
    return size;
}

void GetConverterStats(ConverterStats* stats) {
    if (stats) {
        *stats = g_system.stats;
    }
}

void ResetConverterStats() {
    g_system.stats = {};
    g_conversions_count.store(0);
}

size_t ExportSaveInfoJson(const SaveData& save, char* buffer, size_t buffer_size) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"title_id\": \"" << save.title_id << "\",\n";
    ss << "  \"title\": \"" << save.param_sfo.title << "\",\n";
    ss << "  \"dir_name\": \"" << save.dir_name << "\",\n";
    ss << "  \"format\": " << static_cast<int>(save.format) << ",\n";
    ss << "  \"region\": " << static_cast<int>(save.region) << ",\n";
    ss << "  \"total_size\": " << save.total_size << ",\n";
    ss << "  \"file_count\": " << save.files.size() << ",\n";
    ss << "  \"valid\": " << (save.is_valid ? "true" : "false") << "\n";
    ss << "}\n";
    
    std::string json = ss.str();
    if (json.size() >= buffer_size) return 0;
    
    memcpy(buffer, json.c_str(), json.size() + 1);
    return json.size();
}

bool GetSaveTitleId(const char* path, char* buffer, size_t buffer_size) {
    SaveData save;
    if (!LoadSaveData(path, &save)) return false;
    if (save.title_id.size() >= buffer_size) return false;
    
    strcpy(buffer, save.title_id.c_str());
    return true;
}

bool CheckSaveCompatibility(const char* save_path, const char* game_title_id) {
    char save_tid[16] = {0};
    if (!GetSaveTitleId(save_path, save_tid, sizeof(save_tid))) {
        return false;
    }
    
    // Перевіряємо пряму відповідність
    if (strcmp(save_tid, game_title_id) == 0) {
        return true;
    }
    
    // Перевіряємо крос-регіональну сумісність
    RegionCode save_region = DetectRegionFromTitleId(save_tid);
    RegionCode game_region = DetectRegionFromTitleId(game_title_id);
    
    // Шукаємо в таблиці відповідностей
    for (int i = 0; TITLE_MAPPINGS[i].base_name != nullptr; ++i) {
        const auto& mapping = TITLE_MAPPINGS[i];
        
        bool save_matches = (mapping.us_id && strcmp(save_tid, mapping.us_id) == 0) ||
                           (mapping.eu_id && strcmp(save_tid, mapping.eu_id) == 0) ||
                           (mapping.jp_id && strcmp(save_tid, mapping.jp_id) == 0);
        
        bool game_matches = (mapping.us_id && strcmp(game_title_id, mapping.us_id) == 0) ||
                           (mapping.eu_id && strcmp(game_title_id, mapping.eu_id) == 0) ||
                           (mapping.jp_id && strcmp(game_title_id, mapping.jp_id) == 0);
        
        if (save_matches && game_matches) {
            return true;  // Сумісні (та сама гра, різні регіони)
        }
    }
    
    return false;
}

bool GetEquivalentTitleId(const char* title_id, RegionCode target_region,
                           char* buffer, size_t buffer_size) {
    if (!title_id || !buffer || buffer_size < 10) return false;
    
    for (int i = 0; TITLE_MAPPINGS[i].base_name != nullptr; ++i) {
        const auto& mapping = TITLE_MAPPINGS[i];
        
        bool matches = (mapping.us_id && strcmp(title_id, mapping.us_id) == 0) ||
                      (mapping.eu_id && strcmp(title_id, mapping.eu_id) == 0) ||
                      (mapping.jp_id && strcmp(title_id, mapping.jp_id) == 0) ||
                      (mapping.asia_id && strcmp(title_id, mapping.asia_id) == 0);
        
        if (matches) {
            const char* target = nullptr;
            switch (target_region) {
                case RegionCode::NTSC_U: target = mapping.us_id; break;
                case RegionCode::PAL: target = mapping.eu_id; break;
                case RegionCode::NTSC_J: target = mapping.jp_id; break;
                case RegionCode::ASIA: target = mapping.asia_id; break;
                default: break;
            }
            
            if (target) {
                strncpy(buffer, target, buffer_size - 1);
                buffer[buffer_size - 1] = '\0';
                return true;
            }
        }
    }
    
    return false;
}

bool IsTitleIdForRegion(const char* title_id, RegionCode region) {
    return DetectRegionFromTitleId(title_id) == region;
}

const char* GetRegionName(RegionCode region) {
    switch (region) {
        case RegionCode::NTSC_U: return "NTSC-U (USA)";
        case RegionCode::PAL: return "PAL (Europe)";
        case RegionCode::NTSC_J: return "NTSC-J (Japan)";
        case RegionCode::ASIA: return "Asia";
        default: return "Unknown";
    }
}

RegionCode GetRegionFromTitleId(const char* title_id) {
    return DetectRegionFromTitleId(title_id);
}

} // namespace rpcsx::saves
