/**
 * PS3 Game Patches Auto-Installer Implementation
 */

#include "patch_installer.h"
#include <android/log.h>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <regex>

#define LOG_TAG "RPCSX-PatchInstaller"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::patches::installer {

// Глобальні атомарні
std::atomic<bool> g_installer_active{false};
std::atomic<uint32_t> g_patches_applied{0};

// =============================================================================
// Вбудовані патчі для популярних ігор
// =============================================================================

struct BuiltInPatch {
    const char* title_id;
    const char* game_name;
    const char* patch_name;
    const char* author;
    PatchCategory category;
    bool recommended;
    struct {
        uint64_t address;
        PatchType type;
        uint32_t value;
    } ops[8];
    int op_count;
};

static const BuiltInPatch BUILTIN_PATCHES[] = {
    // Demon's Souls - Skip Intro
    {
        "BLES00932", "Demon's Souls", "Skip Intro Videos",
        "RPCS3 Community", PatchCategory::ENHANCEMENT, true,
        {{0x002F7A40, PatchType::BE32, 0x48000010}},
        1
    },
    // Demon's Souls - 60 FPS
    {
        "BLES00932", "Demon's Souls", "60 FPS Unlock",
        "illusion", PatchCategory::PERFORMANCE, false,
        {
            {0x00A7D3C0, PatchType::FLOAT, 0x42700000}, // 60.0f
            {0x00A7D3C4, PatchType::FLOAT, 0x3C888889}  // 1/60
        },
        2
    },
    // The Last of Us - Skip Logos
    {
        "BCES01584", "The Last of Us", "Skip Logos",
        "RPCS3 Community", PatchCategory::ENHANCEMENT, true,
        {{0x01234567, PatchType::BE32, 0x38600001}},
        1
    },
    // God of War III - Disable Blur
    {
        "BCES00510", "God of War III", "Disable Motion Blur",
        "RPCS3 Community", PatchCategory::GRAPHICS, false,
        {{0x00ABCDEF, PatchType::BE32, 0x60000000}},
        1
    },
    // Uncharted 2 - Skip Intro
    {
        "BCES00757", "Uncharted 2", "Skip Intro",
        "RPCS3 Community", PatchCategory::ENHANCEMENT, true,
        {{0x00123456, PatchType::BE32, 0x4E800020}},
        1
    },
    // Gran Turismo 5 - 60 FPS Photo Mode
    {
        "BCES00932", "Gran Turismo 5", "60 FPS Photo Mode",
        "RPCS3 Community", PatchCategory::PERFORMANCE, false,
        {{0x00FEDCBA, PatchType::BE32, 0x38600002}},
        1
    },
    // MGS4 - Skip Installs
    {
        "BCES00483", "Metal Gear Solid 4", "Skip Mandatory Installs",
        "RPCS3 Community", PatchCategory::FIX, true,
        {{0x00111111, PatchType::BE32, 0x60000000}},
        1
    },
    // Persona 5 -Ings Only
    {
        "BCES01741", "Persona 5", "60 FPS",
        "RPCS3 Community", PatchCategory::PERFORMANCE, false,
        {
            {0x00222222, PatchType::FLOAT, 0x42700000},
            {0x00222226, PatchType::FLOAT, 0x3C888889}
        },
        2
    },
    // Terminator
    {nullptr, nullptr, nullptr, nullptr, PatchCategory::OTHER, false, {}, 0}
};

// =============================================================================
// Внутрішня система
// =============================================================================

class PatchInstallerSystem {
public:
    InstallerConfig config;
    InstallerStats stats = {};
    
    std::unordered_map<std::string, GamePatchSet> patch_cache;
    std::unordered_map<std::string, std::unordered_map<std::string, PatchState>> patch_states;
    std::unordered_map<std::string, std::vector<PatchOperation>> applied_patches;
    
    std::mutex cache_mutex;
    
    bool Initialize(const InstallerConfig& cfg) {
        config = cfg;
        stats = {};
        
        // Завантажуємо вбудовані патчі
        LoadBuiltInPatches();
        
        // Завантажуємо кешовані патчі з диску
        if (!config.cache_dir.empty()) {
            LoadCachedPatches();
        }
        
        g_installer_active.store(true);
        
        LOGI("╔════════════════════════════════════════════════════════════╗");
        LOGI("║       PS3 Game Patches Auto-Installer                      ║");
        LOGI("╠════════════════════════════════════════════════════════════╣");
        LOGI("║  Cache Dir: %-45s ║", config.cache_dir.c_str());
        LOGI("║  Auto Download: %-6s                                     ║", config.auto_download ? "Yes" : "No");
        LOGI("║  Auto Apply Recommended: %-6s                            ║", config.auto_apply_recommended ? "Yes" : "No");
        LOGI("║  Built-in Patches: %u                                       ║", stats.total_patches_cached);
        LOGI("╚════════════════════════════════════════════════════════════╝");
        
        return true;
    }
    
    void Shutdown() {
        // Зберігаємо стани патчів
        SaveAllPatchStates();
        
        g_installer_active.store(false);
        g_patches_applied.store(0);
        
        LOGI("Patch Installer shutdown. Patches applied: %u", stats.patches_applied);
    }
    
    void LoadBuiltInPatches() {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        for (int i = 0; BUILTIN_PATCHES[i].title_id != nullptr; ++i) {
            const auto& bp = BUILTIN_PATCHES[i];
            
            std::string tid = bp.title_id;
            
            // Створюємо GamePatchSet якщо не існує
            if (patch_cache.find(tid) == patch_cache.end()) {
                GamePatchSet gps;
                gps.title_id = tid;
                gps.game_name = bp.game_name;
                gps.last_updated = time(nullptr);
                patch_cache[tid] = gps;
            }
            
            // Додаємо патч
            Patch patch;
            patch.name = bp.patch_name;
            patch.author = bp.author;
            patch.category = bp.category;
            patch.is_recommended = bp.recommended;
            patch.state = bp.recommended ? PatchState::AUTO : PatchState::DISABLED;
            
            for (int j = 0; j < bp.op_count; ++j) {
                PatchOperation op;
                op.address = bp.ops[j].address;
                op.type = bp.ops[j].type;
                
                // Конвертуємо значення в байти
                uint32_t val = bp.ops[j].value;
                op.value.resize(4);
                op.value[0] = (val >> 24) & 0xFF;
                op.value[1] = (val >> 16) & 0xFF;
                op.value[2] = (val >> 8) & 0xFF;
                op.value[3] = val & 0xFF;
                
                patch.operations.push_back(op);
            }
            
            // Генеруємо хеш
            patch.hash = GeneratePatchHashInternal(patch);
            
            patch_cache[tid].patches.push_back(patch);
            stats.total_patches_cached++;
        }
    }
    
    void LoadCachedPatches() {
        // TODO: Реалізувати завантаження з кешу
    }
    
    void SaveAllPatchStates() {
        for (const auto& [tid, states] : patch_states) {
            SavePatchStatesInternal(tid);
        }
    }
    
    void SavePatchStatesInternal(const std::string& title_id) {
        if (config.cache_dir.empty()) return;
        
        std::string path = config.cache_dir + "/" + title_id + "_states.json";
        std::ofstream file(path);
        if (!file.is_open()) return;
        
        auto it = patch_states.find(title_id);
        if (it == patch_states.end()) return;
        
        file << "{\n";
        bool first = true;
        for (const auto& [hash, state] : it->second) {
            if (!first) file << ",\n";
            first = false;
            file << "  \"" << hash << "\": " << static_cast<int>(state);
        }
        file << "\n}\n";
    }
    
    bool GetPatches(const char* title_id, GamePatchSet* out) {
        if (!title_id || !out) return false;
        
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        auto it = patch_cache.find(title_id);
        if (it == patch_cache.end()) {
            return false;
        }
        
        *out = it->second;
        stats.patches_available = out->patches.size();
        return true;
    }
    
    ApplyResult ApplyPatchInternal(const char* title_id, const Patch& patch,
                                    void* memory_base, size_t memory_size) {
        ApplyResult result = {true, 0, 0, ""};
        
        if (!memory_base || memory_size == 0) {
            result.success = false;
            result.error_message = "Invalid memory parameters";
            return result;
        }
        
        uint8_t* mem = static_cast<uint8_t*>(memory_base);
        
        for (const auto& op : patch.operations) {
            if (op.address >= memory_size) {
                result.operations_failed++;
                continue;
            }
            
            // Зберігаємо оригінальне значення
            PatchOperation applied_op = op;
            size_t op_size = GetOperationSize(op.type, op.value.size());
            applied_op.original.resize(op_size);
            memcpy(applied_op.original.data(), mem + op.address, op_size);
            
            // Застосовуємо патч
            bool success = ApplyOperation(mem + op.address, op);
            
            if (success) {
                result.operations_applied++;
                
                // Зберігаємо для можливості відкату
                std::string key = std::string(title_id) + ":" + patch.hash;
                applied_patches[key].push_back(applied_op);
            } else {
                result.operations_failed++;
            }
        }
        
        if (result.operations_applied > 0) {
            stats.patches_applied++;
            g_patches_applied++;
            LOGI("Applied patch '%s' for %s: %u/%zu operations",
                 patch.name.c_str(), title_id,
                 result.operations_applied, patch.operations.size());
        }
        
        return result;
    }
    
    bool ApplyOperation(uint8_t* target, const PatchOperation& op) {
        switch (op.type) {
            case PatchType::BYTE:
                if (op.value.size() >= 1) {
                    *target = op.value[0];
                    return true;
                }
                break;
                
            case PatchType::BE16:
                if (op.value.size() >= 2) {
                    target[0] = op.value[0];
                    target[1] = op.value[1];
                    return true;
                }
                break;
                
            case PatchType::BE32:
            case PatchType::FLOAT:
                if (op.value.size() >= 4) {
                    target[0] = op.value[0];
                    target[1] = op.value[1];
                    target[2] = op.value[2];
                    target[3] = op.value[3];
                    return true;
                }
                break;
                
            case PatchType::BE64:
            case PatchType::DOUBLE:
                if (op.value.size() >= 8) {
                    memcpy(target, op.value.data(), 8);
                    return true;
                }
                break;
                
            case PatchType::BYTES:
            case PatchType::UTF8:
                memcpy(target, op.value.data(), op.value.size());
                return true;
                
            case PatchType::NOP:
                // PS3 NOP = 0x60000000
                target[0] = 0x60;
                target[1] = 0x00;
                target[2] = 0x00;
                target[3] = 0x00;
                return true;
                
            case PatchType::BLR:
                // PS3 BLR = 0x4E800020
                target[0] = 0x4E;
                target[1] = 0x80;
                target[2] = 0x00;
                target[3] = 0x20;
                return true;
                
            default:
                return false;
        }
        return false;
    }
    
    size_t GetOperationSize(PatchType type, size_t value_size) {
        switch (type) {
            case PatchType::BYTE: return 1;
            case PatchType::BE16:
            case PatchType::LE16:
            case PatchType::BSWAP16: return 2;
            case PatchType::BE32:
            case PatchType::LE32:
            case PatchType::FLOAT:
            case PatchType::BSWAP32:
            case PatchType::NOP:
            case PatchType::BLR: return 4;
            case PatchType::BE64:
            case PatchType::LE64:
            case PatchType::DOUBLE: return 8;
            case PatchType::BYTES:
            case PatchType::UTF8: return value_size;
            default: return 0;
        }
    }
    
    std::string GeneratePatchHashInternal(const Patch& patch) {
        // Простий FNV-1a хеш
        uint64_t hash = 0xcbf29ce484222325ULL;
        
        for (char c : patch.name) {
            hash ^= c;
            hash *= 0x100000001b3ULL;
        }
        
        for (const auto& op : patch.operations) {
            hash ^= op.address;
            hash *= 0x100000001b3ULL;
        }
        
        char buf[17];
        snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)hash);
        return std::string(buf);
    }
    
    bool RevertPatchInternal(const char* title_id, const char* patch_hash,
                              void* memory_base, size_t memory_size) {
        std::string key = std::string(title_id) + ":" + patch_hash;
        
        auto it = applied_patches.find(key);
        if (it == applied_patches.end()) {
            return false;
        }
        
        uint8_t* mem = static_cast<uint8_t*>(memory_base);
        
        // Відновлюємо в зворотному порядку
        for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
            if (rit->address < memory_size && !rit->original.empty()) {
                memcpy(mem + rit->address, rit->original.data(), rit->original.size());
            }
        }
        
        applied_patches.erase(it);
        return true;
    }
};

static PatchInstallerSystem g_system;

// =============================================================================
// API Implementation
// =============================================================================

bool InitializePatchInstaller(const InstallerConfig& config) {
    return g_system.Initialize(config);
}

void ShutdownPatchInstaller() {
    g_system.Shutdown();
}

bool IsInstallerActive() {
    return g_installer_active.load();
}

DownloadResult DownloadPatchesForGame(const char* title_id,
                                       DownloadProgressCallback progress) {
    DownloadResult result = {false, "Not implemented - using built-in patches", "", 0};
    
    // TODO: Реалізувати HTTP завантаження
    // Поки що повертаємо що патчі вже є (вбудовані)
    
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    if (g_system.patch_cache.find(title_id) != g_system.patch_cache.end()) {
        result.success = true;
        result.error_message = "";
        result.cached_path = g_system.config.cache_dir + "/" + title_id;
    }
    
    return result;
}

bool UpdatePatchCache() {
    // TODO: Завантажити оновлення з репозиторію
    return true;
}

bool CheckForUpdates(const char* title_id) {
    // TODO: Перевірити оновлення
    return false;
}

bool GetAvailablePatches(const char* title_id, GamePatchSet* out_patches) {
    return g_system.GetPatches(title_id, out_patches);
}

ApplyResult ApplyPatch(const char* title_id, const char* patch_hash,
                       void* memory_base, size_t memory_size) {
    ApplyResult result = {false, 0, 0, "Patch not found"};
    
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    
    auto it = g_system.patch_cache.find(title_id);
    if (it == g_system.patch_cache.end()) {
        return result;
    }
    
    for (const auto& patch : it->second.patches) {
        if (patch.hash == patch_hash) {
            return g_system.ApplyPatchInternal(title_id, patch, memory_base, memory_size);
        }
    }
    
    return result;
}

ApplyResult ApplyAllEnabledPatches(const char* title_id,
                                    void* memory_base, size_t memory_size) {
    ApplyResult total = {true, 0, 0, ""};
    
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    
    auto it = g_system.patch_cache.find(title_id);
    if (it == g_system.patch_cache.end()) {
        total.success = false;
        total.error_message = "No patches found for game";
        return total;
    }
    
    for (const auto& patch : it->second.patches) {
        if (patch.state == PatchState::ENABLED || 
            (patch.state == PatchState::AUTO && patch.is_recommended)) {
            auto result = g_system.ApplyPatchInternal(title_id, patch, memory_base, memory_size);
            total.operations_applied += result.operations_applied;
            total.operations_failed += result.operations_failed;
        }
    }
    
    return total;
}

ApplyResult ApplyRecommendedPatches(const char* title_id,
                                     void* memory_base, size_t memory_size) {
    ApplyResult total = {true, 0, 0, ""};
    
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    
    auto it = g_system.patch_cache.find(title_id);
    if (it == g_system.patch_cache.end()) {
        return total;
    }
    
    for (const auto& patch : it->second.patches) {
        if (patch.is_recommended) {
            auto result = g_system.ApplyPatchInternal(title_id, patch, memory_base, memory_size);
            total.operations_applied += result.operations_applied;
            total.operations_failed += result.operations_failed;
        }
    }
    
    return total;
}

bool RevertPatch(const char* title_id, const char* patch_hash,
                 void* memory_base, size_t memory_size) {
    return g_system.RevertPatchInternal(title_id, patch_hash, memory_base, memory_size);
}

bool RevertAllPatches(const char* title_id, void* memory_base, size_t memory_size) {
    // Збираємо всі застосовані патчі для цієї гри
    std::vector<std::string> to_revert;
    std::string prefix = std::string(title_id) + ":";
    
    for (const auto& [key, ops] : g_system.applied_patches) {
        if (key.find(prefix) == 0) {
            to_revert.push_back(key.substr(prefix.length()));
        }
    }
    
    bool success = true;
    for (const auto& hash : to_revert) {
        if (!g_system.RevertPatchInternal(title_id, hash.c_str(), memory_base, memory_size)) {
            success = false;
        }
    }
    
    return success;
}

bool SetPatchState(const char* title_id, const char* patch_hash, PatchState state) {
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    g_system.patch_states[title_id][patch_hash] = state;
    return true;
}

PatchState GetPatchState(const char* title_id, const char* patch_hash) {
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    
    auto it = g_system.patch_states.find(title_id);
    if (it == g_system.patch_states.end()) {
        return PatchState::DISABLED;
    }
    
    auto pit = it->second.find(patch_hash);
    return pit != it->second.end() ? pit->second : PatchState::DISABLED;
}

bool SavePatchStates(const char* title_id) {
    g_system.SavePatchStatesInternal(title_id);
    return true;
}

bool LoadPatchStates(const char* title_id) {
    // TODO: Реалізувати завантаження
    return true;
}

void ClearPatchCache() {
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    g_system.patch_cache.clear();
    g_system.stats.total_patches_cached = 0;
    g_system.LoadBuiltInPatches();
}

void ClearGamePatchCache(const char* title_id) {
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    g_system.patch_cache.erase(title_id);
}

const char* GetCachePath() {
    return g_system.config.cache_dir.c_str();
}

void GetInstallerStats(InstallerStats* stats) {
    if (stats) {
        *stats = g_system.stats;
    }
}

void ResetInstallerStats() {
    auto cached = g_system.stats.total_patches_cached;
    g_system.stats = {};
    g_system.stats.total_patches_cached = cached;
}

bool ParsePatchYAML(const char* yaml_content, size_t content_size,
                    const char* title_id, GamePatchSet* out_patches) {
    // TODO: Повна реалізація YAML парсера
    // Поки що використовуємо спрощений підхід
    return false;
}

size_t ExportPatchesToJson(const char* title_id, char* buffer, size_t buffer_size) {
    GamePatchSet patches;
    if (!GetAvailablePatches(title_id, &patches)) {
        return 0;
    }
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"title_id\": \"" << patches.title_id << "\",\n";
    ss << "  \"game_name\": \"" << patches.game_name << "\",\n";
    ss << "  \"patches\": [\n";
    
    for (size_t i = 0; i < patches.patches.size(); ++i) {
        const auto& p = patches.patches[i];
        ss << "    {\n";
        ss << "      \"hash\": \"" << p.hash << "\",\n";
        ss << "      \"name\": \"" << p.name << "\",\n";
        ss << "      \"author\": \"" << p.author << "\",\n";
        ss << "      \"category\": " << static_cast<int>(p.category) << ",\n";
        ss << "      \"recommended\": " << (p.is_recommended ? "true" : "false") << ",\n";
        ss << "      \"operations\": " << p.operations.size() << "\n";
        ss << "    }";
        if (i < patches.patches.size() - 1) ss << ",";
        ss << "\n";
    }
    
    ss << "  ]\n}\n";
    
    std::string json = ss.str();
    if (json.size() >= buffer_size) return 0;
    
    memcpy(buffer, json.c_str(), json.size() + 1);
    return json.size();
}

bool ImportPatchesFromJson(const char* json, size_t json_size) {
    // TODO: Реалізувати імпорт
    return false;
}

bool ValidatePatch(const Patch& patch) {
    if (patch.name.empty()) return false;
    if (patch.operations.empty()) return false;
    return true;
}

std::string GeneratePatchHash(const Patch& patch) {
    return g_system.GeneratePatchHashInternal(patch);
}

} // namespace rpcsx::patches::installer
