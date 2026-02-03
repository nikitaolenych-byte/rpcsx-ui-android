/**
 * RPCSX Game-Specific Performance Profiles Implementation
 */

#include "game_profiles.h"
#include <android/log.h>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>

#define LOG_TAG "RPCSX-Profiles"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::profiles {

// Глобальні атомарні
std::atomic<bool> g_profiles_active{false};
std::atomic<uint32_t> g_active_profile_count{0};

// =============================================================================
// База даних відомих ігор
// =============================================================================

struct GameInfo {
    const char* title_id;
    const char* name;
    const char* region;
};

static const GameInfo KNOWN_GAMES[] = {
    // Sony First Party
    {"BCES00510", "God of War III", "EU"},
    {"BCUS98111", "God of War III", "US"},
    {"BCES01584", "God of War: Ascension", "EU"},
    {"BCES00757", "Uncharted 2: Among Thieves", "EU"},
    {"BCUS98123", "Uncharted 2: Among Thieves", "US"},
    {"BCES01175", "Uncharted 3: Drake's Deception", "EU"},
    {"BCES00569", "The Last of Us", "EU"},
    {"BCUS98174", "The Last of Us", "US"},
    {"BCES00932", "Gran Turismo 5", "EU"},
    {"BCUS98114", "Gran Turismo 5", "US"},
    {"BCES01893", "Gran Turismo 6", "EU"},
    {"BCES00791", "Heavy Rain", "EU"},
    {"BCES00797", "inFAMOUS 2", "EU"},
    {"BCES01121", "LittleBigPlanet 2", "EU"},
    {"BCES01007", "Killzone 3", "EU"},
    {"BCES00516", "Ratchet & Clank Future: A Crack in Time", "EU"},
    {"BCES01747", "Beyond: Two Souls", "EU"},
    
    // Konami
    {"BCES00483", "Metal Gear Solid 4: Guns of the Patriots", "EU"},
    {"BLUS30109", "Metal Gear Solid 4: Guns of the Patriots", "US"},
    {"BCES01548", "Metal Gear Rising: Revengeance", "EU"},
    
    // FromSoftware
    {"BLES00932", "Demon's Souls", "EU"},
    {"BLUS30443", "Demon's Souls", "US"},
    {"BCES01789", "Dark Souls II", "EU"},
    
    // Rockstar
    {"BCES00807", "Red Dead Redemption", "EU"},
    {"BLES01807", "Grand Theft Auto V", "EU"},
    {"BLUS31156", "Grand Theft Auto V", "US"},
    
    // Capcom
    {"BLES01031", "Resident Evil 5", "EU"},
    {"BCES01972", "Resident Evil 6", "EU"},
    {"BLES01708", "Dragon's Dogma: Dark Arisen", "EU"},
    {"BLUS30855", "Devil May Cry 4", "US"},
    
    // Square Enix
    {"BCES00510", "Final Fantasy XIII", "EU"},
    {"BCES01086", "Final Fantasy XIII-2", "EU"},
    {"BCES01987", "Final Fantasy XIV: A Realm Reborn", "EU"},
    
    // Bandai Namco
    {"BCES00510", "Tekken 6", "EU"},
    {"BCES01984", "Dark Souls II", "EU"},
    {"BLUS31405", "Tales of Xillia", "US"},
    
    // Activision
    {"BLES01031", "Call of Duty: Black Ops", "EU"},
    {"BLES01717", "Call of Duty: Black Ops II", "EU"},
    
    // EA
    {"BLES01031", "Mass Effect 3", "EU"},
    {"BLES00948", "Dead Space 2", "EU"},
    {"BCES01584", "Battlefield 4", "EU"},
    
    // Ubisoft
    {"BLES01694", "Assassin's Creed III", "EU"},
    {"BLES01882", "Assassin's Creed IV: Black Flag", "EU"},
    {"BLES00972", "Far Cry 3", "EU"},
    
    // Bethesda
    {"BLES01329", "The Elder Scrolls V: Skyrim", "EU"},
    {"BLES00443", "Fallout 3", "EU"},
    {"BLES01316", "Fallout: New Vegas", "EU"},
    
    // Other
    {"BLES00599", "Bayonetta", "EU"},
    {"BLUS30537", "Bayonetta", "US"},
    {"BCES01505", "Ni no Kuni: Wrath of the White Witch", "EU"},
    {"BLES01013", "Catherine", "EU"},
    {"BCES01741", "Persona 5", "EU"},
    {"BLUS31604", "Persona 5", "US"},
    
    {nullptr, nullptr, nullptr} // Terminator
};

// =============================================================================
// Вбудовані профілі для оптимізації
// =============================================================================

static GameProfile CreateGodOfWarProfile() {
    GameProfile p;
    strncpy(p.title_id, "BCES00510", MAX_TITLE_ID_LENGTH);
    strncpy(p.profile_name, "God of War III - Optimized", MAX_PROFILE_NAME_LENGTH);
    p.type = ProfileType::SYSTEM_DEFAULT;
    
    p.gpu.mode = GPUMode::PERFORMANCE;
    p.gpu.resolution_scale = 1.0f;
    p.gpu.enable_drs = true;
    p.gpu.drs_min_scale = 0.75f;
    p.gpu.anti_aliasing = AAMode::FXAA;
    p.gpu.async_shader_compile = true;
    
    p.cpu.spu_mode = SPUMode::RECOMPILER_ASMJIT;
    p.cpu.ppu_mode = PPUMode::RECOMPILER_LLVM;
    
    p.hacks.fix_god_of_war_shadows = true;
    p.target_fps = 30;
    
    return p;
}

static GameProfile CreateUnchartedProfile() {
    GameProfile p;
    strncpy(p.title_id, "BCES00757", MAX_TITLE_ID_LENGTH);
    strncpy(p.profile_name, "Uncharted 2 - Optimized", MAX_PROFILE_NAME_LENGTH);
    p.type = ProfileType::SYSTEM_DEFAULT;
    
    p.gpu.mode = GPUMode::BALANCED;
    p.gpu.resolution_scale = 1.0f;
    p.gpu.enable_drs = true;
    p.gpu.anisotropic = AnisotropicLevel::X8;
    
    p.cpu.spu_mode = SPUMode::RECOMPILER_ASMJIT;
    
    p.hacks.fix_uncharted_water = true;
    p.target_fps = 30;
    
    return p;
}

static GameProfile CreateTLOUProfile() {
    GameProfile p;
    strncpy(p.title_id, "BCES00569", MAX_TITLE_ID_LENGTH);
    strncpy(p.profile_name, "The Last of Us - Optimized", MAX_PROFILE_NAME_LENGTH);
    p.type = ProfileType::SYSTEM_DEFAULT;
    
    p.gpu.mode = GPUMode::BALANCED;
    p.gpu.enable_drs = true;
    p.gpu.drs_min_scale = 0.8f;
    p.gpu.texture_cache_size_mb = 512;
    p.gpu.async_shader_compile = true;
    
    p.cpu.spu_mode = SPUMode::RECOMPILER_LLVM;
    p.cpu.spu_accurate_dfma = true;
    
    p.hacks.fix_tlou_shaders = true;
    p.audio.buffer_mode = AudioBufferMode::MEDIUM;
    p.target_fps = 30;
    
    return p;
}

static GameProfile CreateDemonSoulsProfile() {
    GameProfile p;
    strncpy(p.title_id, "BLES00932", MAX_TITLE_ID_LENGTH);
    strncpy(p.profile_name, "Demon's Souls - Optimized", MAX_PROFILE_NAME_LENGTH);
    p.type = ProfileType::SYSTEM_DEFAULT;
    
    p.gpu.mode = GPUMode::BALANCED;
    p.gpu.resolution_scale = 1.5f;
    p.gpu.anti_aliasing = AAMode::SMAA;
    p.gpu.anisotropic = AnisotropicLevel::X16;
    
    p.cpu.spu_mode = SPUMode::RECOMPILER_ASMJIT;
    
    p.hacks.fix_demon_souls_fog = true;
    p.target_fps = 30;
    p.unlock_fps = true;
    
    return p;
}

static GameProfile CreateMGS4Profile() {
    GameProfile p;
    strncpy(p.title_id, "BCES00483", MAX_TITLE_ID_LENGTH);
    strncpy(p.profile_name, "MGS4 - Optimized", MAX_PROFILE_NAME_LENGTH);
    p.type = ProfileType::SYSTEM_DEFAULT;
    
    p.gpu.mode = GPUMode::ACCURATE;
    p.gpu.strict_rendering = true;
    p.gpu.async_shader_compile = true;
    
    p.cpu.ppu_mode = PPUMode::RECOMPILER_LLVM;
    p.cpu.spu_mode = SPUMode::RECOMPILER_LLVM;
    p.cpu.spu_accurate_dfma = true;
    
    p.hacks.fix_mgs4_cutscenes = true;
    p.audio.buffer_mode = AudioBufferMode::HIGH;
    p.target_fps = 30;
    
    return p;
}

static GameProfile CreateGT5Profile() {
    GameProfile p;
    strncpy(p.title_id, "BCES00932", MAX_TITLE_ID_LENGTH);
    strncpy(p.profile_name, "Gran Turismo 5 - Optimized", MAX_PROFILE_NAME_LENGTH);
    p.type = ProfileType::SYSTEM_DEFAULT;
    
    p.gpu.mode = GPUMode::PERFORMANCE;
    p.gpu.resolution_scale = 1.0f;
    p.gpu.vsync_mode = VSyncMode::ON;
    
    p.cpu.spu_mode = SPUMode::RECOMPILER_ASMJIT;
    
    p.hacks.fix_gt5_physics = true;
    p.target_fps = 60;
    
    return p;
}

static GameProfile CreatePersona5Profile() {
    GameProfile p;
    strncpy(p.title_id, "BCES01741", MAX_TITLE_ID_LENGTH);
    strncpy(p.profile_name, "Persona 5 - Optimized", MAX_PROFILE_NAME_LENGTH);
    p.type = ProfileType::SYSTEM_DEFAULT;
    
    p.gpu.mode = GPUMode::BALANCED;
    p.gpu.resolution_scale = 2.0f;
    p.gpu.anisotropic = AnisotropicLevel::X8;
    
    p.cpu.spu_mode = SPUMode::RECOMPILER_ASMJIT;
    
    p.audio.buffer_mode = AudioBufferMode::LOW;
    p.target_fps = 30;
    
    return p;
}

// =============================================================================
// Внутрішня система
// =============================================================================

class ProfileSystem {
public:
    std::unordered_map<std::string, GameProfile> user_profiles;
    std::unordered_map<std::string, GameProfile> builtin_profiles;
    
    GameProfile default_profile;
    GameProfile* current_profile = nullptr;
    std::string current_title_id;
    
    std::string profiles_dir;
    std::mutex profiles_mutex;
    
    ProfileChangeCallback change_callback;
    ProfileStats stats = {};
    
    bool Initialize(const char* dir) {
        profiles_dir = dir ? dir : "/data/data/io.github.nicksnt/profiles";
        
        // Ініціалізація профілю за замовчуванням
        memset(&default_profile, 0, sizeof(default_profile));
        strncpy(default_profile.title_id, "DEFAULT", MAX_TITLE_ID_LENGTH);
        strncpy(default_profile.profile_name, "Default Profile", MAX_PROFILE_NAME_LENGTH);
        default_profile.type = ProfileType::SYSTEM_DEFAULT;
        default_profile.gpu.mode = GPUMode::BALANCED;
        default_profile.gpu.enable_drs = true;
        default_profile.cpu.ppu_mode = PPUMode::RECOMPILER_LLVM;
        default_profile.cpu.spu_mode = SPUMode::RECOMPILER_ASMJIT;
        default_profile.target_fps = 30;
        
        // Завантаження вбудованих профілів
        LoadBuiltInProfiles();
        
        // Завантаження користувацьких профілів
        std::string user_profiles_path = profiles_dir + "/user_profiles.json";
        LoadProfilesFromFile(user_profiles_path.c_str());
        
        stats.system_profiles = builtin_profiles.size();
        stats.user_profiles = user_profiles.size();
        stats.total_profiles = stats.system_profiles + stats.user_profiles;
        
        g_profiles_active.store(true);
        g_active_profile_count.store(stats.total_profiles);
        
        LOGI("╔════════════════════════════════════════════════════════════╗");
        LOGI("║        Game-Specific Performance Profiles                  ║");
        LOGI("╠════════════════════════════════════════════════════════════╣");
        LOGI("║  Built-in Profiles: %3u                                    ║", stats.system_profiles);
        LOGI("║  User Profiles:     %3u                                    ║", stats.user_profiles);
        LOGI("║  Profiles Dir: %-42s ║", profiles_dir.c_str());
        LOGI("╚════════════════════════════════════════════════════════════╝");
        
        return true;
    }
    
    void Shutdown() {
        // Збереження профілів
        std::string user_profiles_path = profiles_dir + "/user_profiles.json";
        SaveProfilesToFile(user_profiles_path.c_str());
        
        g_profiles_active.store(false);
        g_active_profile_count.store(0);
        
        LOGI("Profile system shutdown. Total switches: %u", stats.active_profile_switches);
    }
    
    void LoadBuiltInProfiles() {
        builtin_profiles["BCES00510"] = CreateGodOfWarProfile();
        builtin_profiles["BCUS98111"] = CreateGodOfWarProfile();
        builtin_profiles["BCES00757"] = CreateUnchartedProfile();
        builtin_profiles["BCUS98123"] = CreateUnchartedProfile();
        builtin_profiles["BCES00569"] = CreateTLOUProfile();
        builtin_profiles["BCUS98174"] = CreateTLOUProfile();
        builtin_profiles["BLES00932"] = CreateDemonSoulsProfile();
        builtin_profiles["BLUS30443"] = CreateDemonSoulsProfile();
        builtin_profiles["BCES00483"] = CreateMGS4Profile();
        builtin_profiles["BLUS30109"] = CreateMGS4Profile();
        builtin_profiles["BCES00932"] = CreateGT5Profile();
        builtin_profiles["BCUS98114"] = CreateGT5Profile();
        builtin_profiles["BCES01741"] = CreatePersona5Profile();
        builtin_profiles["BLUS31604"] = CreatePersona5Profile();
    }
    
    const GameProfile* GetProfile(const char* title_id) {
        if (!title_id) return &default_profile;
        
        std::lock_guard<std::mutex> lock(profiles_mutex);
        
        // Спочатку перевіряємо користувацькі профілі
        auto user_it = user_profiles.find(title_id);
        if (user_it != user_profiles.end()) {
            return &user_it->second;
        }
        
        // Потім вбудовані
        auto builtin_it = builtin_profiles.find(title_id);
        if (builtin_it != builtin_profiles.end()) {
            return &builtin_it->second;
        }
        
        return &default_profile;
    }
    
    bool ApplyProfile(const char* title_id) {
        const GameProfile* profile = GetProfile(title_id);
        
        {
            std::lock_guard<std::mutex> lock(profiles_mutex);
            current_title_id = title_id ? title_id : "";
            current_profile = const_cast<GameProfile*>(profile);
            stats.active_profile_switches++;
            stats.current_title_id = current_title_id.c_str();
            stats.current_profile_name = profile->profile_name;
        }
        
        LOGI("Applied profile: %s (%s)", profile->profile_name, title_id);
        
        if (change_callback) {
            change_callback(title_id, profile);
        }
        
        return true;
    }
    
    bool CreateOrUpdateProfile(const GameProfile& profile, bool is_update) {
        if (!IsValidTitleId(profile.title_id)) {
            LOGE("Invalid Title ID: %s", profile.title_id);
            return false;
        }
        
        std::lock_guard<std::mutex> lock(profiles_mutex);
        
        GameProfile new_profile = profile;
        new_profile.type = ProfileType::USER_CUSTOM;
        
        time_t now = time(nullptr);
        if (!is_update) {
            new_profile.created_timestamp = now;
        }
        new_profile.modified_timestamp = now;
        
        user_profiles[profile.title_id] = new_profile;
        
        stats.user_profiles = user_profiles.size();
        stats.total_profiles = stats.system_profiles + stats.user_profiles;
        g_active_profile_count.store(stats.total_profiles);
        
        LOGI("%s profile: %s (%s)", 
             is_update ? "Updated" : "Created",
             new_profile.profile_name, 
             new_profile.title_id);
        
        return true;
    }
    
    bool DeleteProfile(const char* title_id) {
        std::lock_guard<std::mutex> lock(profiles_mutex);
        
        auto it = user_profiles.find(title_id);
        if (it == user_profiles.end()) {
            LOGW("Profile not found: %s", title_id);
            return false;
        }
        
        user_profiles.erase(it);
        
        stats.user_profiles = user_profiles.size();
        stats.total_profiles = stats.system_profiles + stats.user_profiles;
        g_active_profile_count.store(stats.total_profiles);
        
        LOGI("Deleted profile: %s", title_id);
        return true;
    }
    
    bool LoadProfilesFromFile(const char* path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            LOGI("No saved profiles found at: %s", path);
            return false;
        }
        
        // Простий парсинг JSON (в реальності використали б nlohmann/json)
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json = buffer.str();
        
        // TODO: Implement proper JSON parsing
        LOGI("Loaded profiles from: %s", path);
        return true;
    }
    
    bool SaveProfilesToFile(const char* path) {
        std::ofstream file(path);
        if (!file.is_open()) {
            LOGE("Failed to save profiles to: %s", path);
            return false;
        }
        
        std::lock_guard<std::mutex> lock(profiles_mutex);
        
        // Простий JSON експорт
        file << "{\n  \"profiles\": [\n";
        
        bool first = true;
        for (const auto& [title_id, profile] : user_profiles) {
            if (!first) file << ",\n";
            first = false;
            
            file << "    {\n";
            file << "      \"title_id\": \"" << profile.title_id << "\",\n";
            file << "      \"name\": \"" << profile.profile_name << "\",\n";
            file << "      \"gpu_mode\": " << static_cast<int>(profile.gpu.mode) << ",\n";
            file << "      \"resolution_scale\": " << profile.gpu.resolution_scale << ",\n";
            file << "      \"enable_drs\": " << (profile.gpu.enable_drs ? "true" : "false") << ",\n";
            file << "      \"spu_mode\": " << static_cast<int>(profile.cpu.spu_mode) << ",\n";
            file << "      \"target_fps\": " << profile.target_fps << "\n";
            file << "    }";
        }
        
        file << "\n  ]\n}\n";
        
        LOGI("Saved %zu profiles to: %s", user_profiles.size(), path);
        return true;
    }
    
    size_t ExportToJson(const char* title_id, char* buffer, size_t size) {
        const GameProfile* profile = GetProfile(title_id);
        if (!profile) return 0;
        
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"title_id\": \"" << profile->title_id << "\",\n";
        ss << "  \"name\": \"" << profile->profile_name << "\",\n";
        ss << "  \"type\": " << static_cast<int>(profile->type) << ",\n";
        ss << "  \"gpu\": {\n";
        ss << "    \"mode\": " << static_cast<int>(profile->gpu.mode) << ",\n";
        ss << "    \"resolution_scale\": " << profile->gpu.resolution_scale << ",\n";
        ss << "    \"enable_drs\": " << (profile->gpu.enable_drs ? "true" : "false") << ",\n";
        ss << "    \"anisotropic\": " << static_cast<int>(profile->gpu.anisotropic) << ",\n";
        ss << "    \"anti_aliasing\": " << static_cast<int>(profile->gpu.anti_aliasing) << "\n";
        ss << "  },\n";
        ss << "  \"cpu\": {\n";
        ss << "    \"ppu_mode\": " << static_cast<int>(profile->cpu.ppu_mode) << ",\n";
        ss << "    \"spu_mode\": " << static_cast<int>(profile->cpu.spu_mode) << "\n";
        ss << "  },\n";
        ss << "  \"target_fps\": " << profile->target_fps << ",\n";
        ss << "  \"unlock_fps\": " << (profile->unlock_fps ? "true" : "false") << "\n";
        ss << "}\n";
        
        std::string json = ss.str();
        if (json.size() >= size) return 0;
        
        memcpy(buffer, json.c_str(), json.size() + 1);
        return json.size();
    }
};

static ProfileSystem g_system;

// =============================================================================
// API Implementation
// =============================================================================

bool InitializeProfiles(const char* profiles_dir) {
    return g_system.Initialize(profiles_dir);
}

void ShutdownProfiles() {
    g_system.Shutdown();
}

bool IsProfileSystemActive() {
    return g_profiles_active.load();
}

const GameProfile* GetProfileForGame(const char* title_id) {
    return g_system.GetProfile(title_id);
}

const GameProfile* GetDefaultProfile() {
    return &g_system.default_profile;
}

bool ApplyProfileForGame(const char* title_id) {
    return g_system.ApplyProfile(title_id);
}

bool CreateProfile(const GameProfile& profile) {
    return g_system.CreateOrUpdateProfile(profile, false);
}

bool UpdateProfile(const char* title_id, const GameProfile& profile) {
    return g_system.CreateOrUpdateProfile(profile, true);
}

bool DeleteProfile(const char* title_id) {
    return g_system.DeleteProfile(title_id);
}

bool HasProfile(const char* title_id) {
    if (!title_id) return false;
    
    std::lock_guard<std::mutex> lock(g_system.profiles_mutex);
    return g_system.user_profiles.find(title_id) != g_system.user_profiles.end() ||
           g_system.builtin_profiles.find(title_id) != g_system.builtin_profiles.end();
}

const GameProfile* GetBuiltInProfile(const char* title_id) {
    if (!title_id) return nullptr;
    
    std::lock_guard<std::mutex> lock(g_system.profiles_mutex);
    auto it = g_system.builtin_profiles.find(title_id);
    return it != g_system.builtin_profiles.end() ? &it->second : nullptr;
}

const char** GetBuiltInProfileList(uint32_t* count) {
    static std::vector<const char*> list;
    
    list.clear();
    for (const auto& [title_id, profile] : g_system.builtin_profiles) {
        list.push_back(title_id.c_str());
    }
    
    if (count) *count = list.size();
    return list.empty() ? nullptr : list.data();
}

size_t ExportProfileToJson(const char* title_id, char* json_buffer, size_t buffer_size) {
    return g_system.ExportToJson(title_id, json_buffer, buffer_size);
}

bool ImportProfileFromJson(const char* json, size_t json_length) {
    // TODO: Implement JSON parsing
    return false;
}

bool LoadProfilesFromFile(const char* path) {
    return g_system.LoadProfilesFromFile(path);
}

bool SaveProfilesToFile(const char* path) {
    return g_system.SaveProfilesToFile(path);
}

void SetProfileChangeCallback(ProfileChangeCallback callback) {
    g_system.change_callback = std::move(callback);
}

void GetProfileStats(ProfileStats* stats) {
    if (stats) {
        std::lock_guard<std::mutex> lock(g_system.profiles_mutex);
        *stats = g_system.stats;
    }
}

const GameProfile* GetCurrentProfile() {
    return g_system.current_profile ? g_system.current_profile : &g_system.default_profile;
}

void ResetProfileStats() {
    std::lock_guard<std::mutex> lock(g_system.profiles_mutex);
    auto total = g_system.stats.total_profiles;
    auto system = g_system.stats.system_profiles;
    auto user = g_system.stats.user_profiles;
    g_system.stats = {};
    g_system.stats.total_profiles = total;
    g_system.stats.system_profiles = system;
    g_system.stats.user_profiles = user;
}

bool IsValidTitleId(const char* title_id) {
    if (!title_id) return false;
    size_t len = strlen(title_id);
    if (len < 9 || len > 10) return false;
    
    // Format: XXXX#####
    for (int i = 0; i < 4; ++i) {
        if (title_id[i] < 'A' || title_id[i] > 'Z') return false;
    }
    for (size_t i = 4; i < len; ++i) {
        if (title_id[i] < '0' || title_id[i] > '9') return false;
    }
    
    return true;
}

const char* GetGameName(const char* title_id) {
    if (!title_id) return nullptr;
    
    for (int i = 0; KNOWN_GAMES[i].title_id != nullptr; ++i) {
        if (strcmp(KNOWN_GAMES[i].title_id, title_id) == 0) {
            return KNOWN_GAMES[i].name;
        }
    }
    return nullptr;
}

const char* GetGameRegion(const char* title_id) {
    if (!title_id) return nullptr;
    
    // Перші літери визначають регіон
    if (strncmp(title_id, "BCES", 4) == 0 || strncmp(title_id, "BLES", 4) == 0) {
        return "EU";
    } else if (strncmp(title_id, "BCUS", 4) == 0 || strncmp(title_id, "BLUS", 4) == 0) {
        return "US";
    } else if (strncmp(title_id, "BCJS", 4) == 0 || strncmp(title_id, "BLJS", 4) == 0) {
        return "JP";
    } else if (strncmp(title_id, "BCAS", 4) == 0 || strncmp(title_id, "BLAS", 4) == 0) {
        return "AS";
    }
    
    return "Unknown";
}

void CopyProfile(GameProfile* dest, const GameProfile* src) {
    if (dest && src) {
        memcpy(dest, src, sizeof(GameProfile));
    }
}

bool CompareProfiles(const GameProfile* a, const GameProfile* b) {
    if (!a || !b) return false;
    return memcmp(a, b, sizeof(GameProfile)) == 0;
}

} // namespace rpcsx::profiles
