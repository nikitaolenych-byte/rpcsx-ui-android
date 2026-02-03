/**
 * PS3 Firmware Version Spoofing Implementation
 */

#include "firmware_spoof.h"
#include <android/log.h>
#include <mutex>
#include <sstream>
#include <fstream>
#include <cstring>

#define LOG_TAG "RPCSX-FirmwareSpoof"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::firmware {

// Глобальні атомарні
std::atomic<bool> g_spoof_active{false};
std::atomic<uint64_t> g_spoof_count{0};

// =============================================================================
// Version Database
// =============================================================================

struct VersionInfo {
    FirmwareVersion version;
    const char* description;
    uint32_t features;
};

static const VersionInfo VERSION_DB[] = {
    {{1, 0, 0},   "Initial Release", 0},
    {{2, 0, 0},   "Game Data Utility", (1 << 4)},
    {{2, 40, 0},  "Trophy Support", (1 << 0) | (1 << 4)},
    {{2, 41, 0},  "In-game XMB", (1 << 0) | (1 << 4) | (1 << 5)},
    {{3, 0, 0},   "Voice Chat, Themes", 0x000001FF},
    {{3, 50, 0},  "Remote Play, Blu-ray 3D", 0x000003FF},
    {{3, 55, 0},  "CFW Popular Version", 0x000003FF},
    {{4, 0, 0},   "Store V2, Auto-Update", 0x00001FFF},
    {{4, 10, 0},  "Vita Connectivity", 0x00003FFF},
    {{4, 21, 0},  "Security Update", 0x00003FFF},
    {{4, 50, 0},  "Stability Improvements", 0x00007FFF},
    {{4, 84, 0},  "Security Update", 0x00007FFF},
    {{4, 85, 0},  "Security Update", 0x00007FFF},
    {{4, 86, 0},  "Security Update", 0x00007FFF},
    {{4, 87, 0},  "Security Update", 0x00007FFF},
    {{4, 88, 0},  "Security Update", 0x00007FFF},
    {{4, 89, 0},  "Minor Update", 0x00007FFF},
    {{4, 90, 0},  "Latest Official", 0x00007FFF},
    {{0, 0, 0},   nullptr, 0}  // Термінатор
};

// =============================================================================
// Game Requirements Database
// =============================================================================

struct GameRequirement {
    const char* title_id;
    const char* game_name;
    FirmwareVersion min_version;
    FirmwareVersion recommended_version;
};

static const GameRequirement GAME_REQUIREMENTS[] = {
    // Ігри з високими вимогами до версії
    {"BCES01584", "The Last of Us",         {3, 55, 0}, {4, 90, 0}},
    {"BCUS98174", "The Last of Us (US)",    {3, 55, 0}, {4, 90, 0}},
    {"BCES01741", "Persona 5",              {4, 21, 0}, {4, 90, 0}},
    {"BLES01807", "Grand Theft Auto V",     {4, 55, 0}, {4, 90, 0}},
    {"BCES00569", "Uncharted 3",            {3, 70, 0}, {4, 90, 0}},
    {"BCES01175", "God of War: Ascension",  {4, 30, 0}, {4, 90, 0}},
    {"BCES00757", "Uncharted 2",            {2, 40, 0}, {4, 90, 0}},
    {"BCES01893", "Beyond: Two Souls",      {4, 40, 0}, {4, 90, 0}},
    {"BCES00510", "God of War III",         {2, 60, 0}, {4, 90, 0}},
    {"BCES00932", "Demon's Souls",          {1, 50, 0}, {4, 90, 0}},
    {"BLES00559", "Red Dead Redemption",    {3, 21, 0}, {4, 90, 0}},
    {"BCES00850", "Heavy Rain",             {3, 15, 0}, {4, 90, 0}},
    {"BCES00483", "Metal Gear Solid 4",     {2, 36, 0}, {4, 90, 0}},
    {nullptr, nullptr, {0,0,0}, {0,0,0}}  // Термінатор
};

// =============================================================================
// Internal System
// =============================================================================

class FirmwareSpoofSystem {
public:
    SpoofConfig config;
    SpoofStats stats = {};
    
    std::unordered_map<std::string, GameFirmwareOverride> game_overrides;
    std::mutex override_mutex;
    
    std::string current_game;
    char system_string[32] = "4.90";
    
    bool Initialize(const SpoofConfig& cfg) {
        config = cfg;
        stats = {};
        
        snprintf(system_string, sizeof(system_string), "%s",
                 config.system_software_string.c_str());
        
        g_spoof_active.store(true);
        g_spoof_count.store(0);
        
        LOGI("╔════════════════════════════════════════════════════════════╗");
        LOGI("║        PS3 Firmware Version Spoofing                       ║");
        LOGI("╠════════════════════════════════════════════════════════════╣");
        LOGI("║  Spoofed Version: %-8s                                  ║", config.global_version.ToString().c_str());
        LOGI("║  SDK Version: 0x%08X                                   ║", config.sdk_version.version);
        LOGI("║  All Features: %-6s                                      ║", config.enable_all_features ? "Yes" : "No");
        LOGI("╚════════════════════════════════════════════════════════════╝");
        
        return true;
    }
    
    void Shutdown() {
        g_spoof_active.store(false);
        LOGI("Firmware Spoof shutdown. Spoof count: %llu",
             (unsigned long long)g_spoof_count.load());
    }
    
    FirmwareVersion GetVersionForGame(const char* title_id) {
        if (!title_id) return config.global_version;
        
        // Перевіряємо override
        {
            std::lock_guard<std::mutex> lock(override_mutex);
            auto it = game_overrides.find(title_id);
            if (it != game_overrides.end()) {
                return it->second.version;
            }
        }
        
        // Повертаємо глобальну
        return config.global_version;
    }
    
    SDKVersion GetSDKForGame(const char* title_id) {
        if (!title_id) return config.sdk_version;
        
        std::lock_guard<std::mutex> lock(override_mutex);
        auto it = game_overrides.find(title_id);
        if (it != game_overrides.end()) {
            return it->second.sdk;
        }
        
        return config.sdk_version;
    }
    
    FirmwareVersion GetMinVersionForGame(const char* title_id) {
        if (!title_id) return known_versions::V1_00;
        
        for (int i = 0; GAME_REQUIREMENTS[i].title_id != nullptr; ++i) {
            if (strcmp(GAME_REQUIREMENTS[i].title_id, title_id) == 0) {
                return GAME_REQUIREMENTS[i].min_version;
            }
        }
        
        return known_versions::V1_00;
    }
    
    FirmwareVersion GetRecommendedForGame(const char* title_id) {
        if (!title_id) return known_versions::DEFAULT;
        
        for (int i = 0; GAME_REQUIREMENTS[i].title_id != nullptr; ++i) {
            if (strcmp(GAME_REQUIREMENTS[i].title_id, title_id) == 0) {
                return GAME_REQUIREMENTS[i].recommended_version;
            }
        }
        
        return known_versions::DEFAULT;
    }
};

static FirmwareSpoofSystem g_system;

// =============================================================================
// API Implementation
// =============================================================================

FirmwareVersion FirmwareVersion::Parse(const char* str) {
    FirmwareVersion v = {0, 0, 0};
    if (!str) return v;
    
    int major, minor, build = 0;
    if (sscanf(str, "%d.%d.%d", &major, &minor, &build) >= 2) {
        v.major = major;
        v.minor = minor;
        v.build = build;
    }
    
    return v;
}

bool InitializeFirmwareSpoof(const SpoofConfig& config) {
    return g_system.Initialize(config);
}

void ShutdownFirmwareSpoof() {
    g_system.Shutdown();
}

bool IsSpoofActive() {
    return g_spoof_active.load();
}

void SetGlobalFirmwareVersion(const FirmwareVersion& version) {
    g_system.config.global_version = version;
    snprintf(g_system.system_string, sizeof(g_system.system_string),
             "%s", version.ToString().c_str());
    LOGI("Global firmware version set to %s", version.ToString().c_str());
}

FirmwareVersion GetSpoofedFirmwareVersion(const char* title_id) {
    if (!g_spoof_active.load()) {
        return known_versions::DEFAULT;
    }
    
    g_system.stats.version_checks++;
    g_spoof_count++;
    
    return g_system.GetVersionForGame(title_id);
}

uint32_t GetSpoofedFirmwarePS3Format(const char* title_id) {
    return GetSpoofedFirmwareVersion(title_id).ToPS3Format();
}

void SetSDKVersion(const SDKVersion& sdk) {
    g_system.config.sdk_version = sdk;
    LOGI("SDK version set to 0x%08X", sdk.version);
}

SDKVersion GetSpoofedSDKVersion(const char* title_id) {
    if (!g_spoof_active.load()) {
        return {0x00490001};  // 4.90
    }
    
    g_system.stats.sdk_checks++;
    return g_system.GetSDKForGame(title_id);
}

bool IsFeatureSupported(FirmwareFeature feature) {
    g_system.stats.feature_checks++;
    
    if (g_system.config.enable_all_features) {
        return true;
    }
    
    return (g_system.config.enabled_features & static_cast<uint32_t>(feature)) != 0;
}

void SetFeatureEnabled(FirmwareFeature feature, bool enabled) {
    if (enabled) {
        g_system.config.enabled_features |= static_cast<uint32_t>(feature);
    } else {
        g_system.config.enabled_features &= ~static_cast<uint32_t>(feature);
    }
}

const char* GetSystemSoftwareString() {
    return g_system.system_string;
}

bool SetGameFirmwareOverride(const char* title_id, const FirmwareVersion& version,
                              const char* reason) {
    if (!title_id) return false;
    
    std::lock_guard<std::mutex> lock(g_system.override_mutex);
    
    GameFirmwareOverride override;
    override.title_id = title_id;
    override.version = version;
    override.sdk = {version.ToPS3Format()};
    override.features = 0xFFFFFFFF;
    override.reason = reason ? reason : "";
    
    g_system.game_overrides[title_id] = override;
    
    LOGI("Set firmware override for %s: %s", title_id, version.ToString().c_str());
    return true;
}

bool RemoveGameFirmwareOverride(const char* title_id) {
    if (!title_id) return false;
    
    std::lock_guard<std::mutex> lock(g_system.override_mutex);
    return g_system.game_overrides.erase(title_id) > 0;
}

bool GetGameFirmwareOverride(const char* title_id, GameFirmwareOverride* out) {
    if (!title_id || !out) return false;
    
    std::lock_guard<std::mutex> lock(g_system.override_mutex);
    
    auto it = g_system.game_overrides.find(title_id);
    if (it == g_system.game_overrides.end()) return false;
    
    *out = it->second;
    return true;
}

size_t GetAllGameOverrides(GameFirmwareOverride* buffer, size_t buffer_size) {
    std::lock_guard<std::mutex> lock(g_system.override_mutex);
    
    size_t count = 0;
    for (const auto& [tid, override] : g_system.game_overrides) {
        if (count < buffer_size) {
            buffer[count] = override;
        }
        count++;
    }
    
    return count;
}

bool LoadFirmwareOverrides(const char* filepath) {
    if (!filepath) return false;
    
    // TODO: Реалізувати завантаження з JSON
    return false;
}

bool SaveFirmwareOverrides(const char* filepath) {
    if (!filepath) return false;
    
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    
    std::lock_guard<std::mutex> lock(g_system.override_mutex);
    
    file << "{\n  \"overrides\": [\n";
    bool first = true;
    for (const auto& [tid, override] : g_system.game_overrides) {
        if (!first) file << ",\n";
        first = false;
        file << "    {\n";
        file << "      \"title_id\": \"" << tid << "\",\n";
        file << "      \"version\": \"" << override.version.ToString() << "\",\n";
        file << "      \"reason\": \"" << override.reason << "\"\n";
        file << "    }";
    }
    file << "\n  ]\n}\n";
    
    return true;
}

FirmwareVersion GetRecommendedVersion(const char* title_id) {
    return g_system.GetRecommendedForGame(title_id);
}

FirmwareVersion GetMinimumRequiredVersion(const char* title_id) {
    return g_system.GetMinVersionForGame(title_id);
}

void SetCurrentGameContext(const char* title_id) {
    g_system.current_game = title_id ? title_id : "";
}

void GetSpoofStats(SpoofStats* stats) {
    if (stats) {
        *stats = g_system.stats;
        stats->unique_games_spoofed = g_system.game_overrides.size();
    }
}

void ResetSpoofStats() {
    g_system.stats = {};
    g_spoof_count.store(0);
}

size_t ExportConfigJson(char* buffer, size_t buffer_size) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"enabled\": " << (g_system.config.enabled ? "true" : "false") << ",\n";
    ss << "  \"global_version\": \"" << g_system.config.global_version.ToString() << "\",\n";
    ss << "  \"sdk_version\": \"0x" << std::hex << g_system.config.sdk_version.version << std::dec << "\",\n";
    ss << "  \"enable_all_features\": " << (g_system.config.enable_all_features ? "true" : "false") << ",\n";
    ss << "  \"spoof_count\": " << g_spoof_count.load() << "\n";
    ss << "}\n";
    
    std::string json = ss.str();
    if (json.size() >= buffer_size) return 0;
    
    memcpy(buffer, json.c_str(), json.size() + 1);
    return json.size();
}

bool ImportConfigJson(const char* json, size_t json_size) {
    // TODO: Реалізувати парсинг JSON
    return false;
}

size_t GetKnownVersions(FirmwareVersion* buffer, size_t buffer_size) {
    size_t count = 0;
    for (int i = 0; VERSION_DB[i].description != nullptr; ++i) {
        if (count < buffer_size) {
            buffer[count] = VERSION_DB[i].version;
        }
        count++;
    }
    return count;
}

const char* GetVersionDescription(const FirmwareVersion& version) {
    for (int i = 0; VERSION_DB[i].description != nullptr; ++i) {
        if (VERSION_DB[i].version == version) {
            return VERSION_DB[i].description;
        }
    }
    return "Unknown version";
}

} // namespace rpcsx::firmware
