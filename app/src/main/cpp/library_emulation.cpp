/**
 * Missing PS3 Library Emulation Implementation
 */

#include "library_emulation.h"
#include <android/log.h>
#include <mutex>
#include <sstream>
#include <cstring>
#include <algorithm>

#define LOG_TAG "RPCSX-LibraryEmulation"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace rpcsx::libraries {

// Глобальні атомарні
std::atomic<bool> g_library_emulation_active{false};
std::atomic<uint64_t> g_library_calls{0};

// =============================================================================
// Built-in Library Database
// =============================================================================

struct BuiltinLibrary {
    const char* name;
    const char* filename;
    LibraryCategory category;
    ImplStatus status;
    const char* description;
};

static const BuiltinLibrary BUILTIN_LIBS[] = {
    // System
    {libs::LIBLV2, "liblv2.sprx", LibraryCategory::SYSTEM, ImplStatus::COMPLETE, "LV2 Core Library"},
    {libs::LIBSYSUTIL, "libsysutil.sprx", LibraryCategory::SYSTEM, ImplStatus::PARTIAL, "System Utilities"},
    {libs::LIBSYSUTIL_SAVEDATA, "libsysutil_savedata.sprx", LibraryCategory::STORAGE, ImplStatus::PARTIAL, "Save Data"},
    {libs::LIBSYSUTIL_GAME, "libsysutil_game.sprx", LibraryCategory::GAME, ImplStatus::PARTIAL, "Game Utilities"},
    
    // Graphics
    {libs::LIBGCM_SYS, "libgcm_sys.sprx", LibraryCategory::GRAPHICS, ImplStatus::PARTIAL, "Graphics Command Manager"},
    {libs::LIBRESC, "libresc.sprx", LibraryCategory::GRAPHICS, ImplStatus::STUB, "Resolution Converter"},
    
    // Audio
    {libs::LIBAUDIO, "libaudio.sprx", LibraryCategory::AUDIO, ImplStatus::PARTIAL, "Audio Output"},
    {libs::LIBADEC, "libadec.sprx", LibraryCategory::AUDIO, ImplStatus::STUB, "Audio Decoder"},
    
    // Video
    {libs::LIBVDEC, "libvdec.sprx", LibraryCategory::VIDEO, ImplStatus::STUB, "Video Decoder"},
    
    // Network
    {libs::LIBNET, "libnet.sprx", LibraryCategory::NETWORK, ImplStatus::PARTIAL, "Network Core"},
    {libs::LIBNETCTL, "libnetctl.sprx", LibraryCategory::NETWORK, ImplStatus::PARTIAL, "Network Control"},
    {libs::LIBHTTP, "libhttp.sprx", LibraryCategory::NETWORK, ImplStatus::STUB, "HTTP Client"},
    {libs::LIBSSL, "libssl.sprx", LibraryCategory::NETWORK, ImplStatus::STUB, "SSL/TLS"},
    
    // Input
    {libs::LIBPAD, "libpad.sprx", LibraryCategory::INPUT, ImplStatus::COMPLETE, "Gamepad Input"},
    {libs::LIBKB, "libkb.sprx", LibraryCategory::INPUT, ImplStatus::PARTIAL, "Keyboard Input"},
    {libs::LIBMOUSE, "libmouse.sprx", LibraryCategory::INPUT, ImplStatus::STUB, "Mouse Input"},
    
    // Storage
    {libs::LIBFS, "libfs.sprx", LibraryCategory::STORAGE, ImplStatus::COMPLETE, "File System"},
    {libs::LIBIO, "libio.sprx", LibraryCategory::STORAGE, ImplStatus::PARTIAL, "I/O Operations"},
    
    // Media
    {libs::LIBPNGDEC, "libpngdec.sprx", LibraryCategory::MEDIA, ImplStatus::PARTIAL, "PNG Decoder"},
    {libs::LIBJPGDEC, "libjpgdec.sprx", LibraryCategory::MEDIA, ImplStatus::PARTIAL, "JPEG Decoder"},
    
    // Font
    {libs::LIBFONT, "libfont.sprx", LibraryCategory::FONT, ImplStatus::STUB, "Font Rendering"},
    {libs::LIBFONTFT, "libfontft.sprx", LibraryCategory::FONT, ImplStatus::STUB, "FreeType Font"},
    {libs::LIBFREETYPE, "libfreetype.sprx", LibraryCategory::FONT, ImplStatus::STUB, "FreeType Library"},
    
    // Game
    {libs::LIBTROPHY, "libnp_trophy.sprx", LibraryCategory::GAME, ImplStatus::PARTIAL, "Trophy System"},
    
    // SPU
    {libs::LIBSPURS, "libspurs_jq.sprx", LibraryCategory::SPU, ImplStatus::PARTIAL, "SPURS Job Queue"},
    
    // PlayStation Move
    {libs::LIBGEM, "libgem.sprx", LibraryCategory::MOVE, ImplStatus::STUB, "PlayStation Move"},
    {libs::LIBCAMERA, "libcamera.sprx", LibraryCategory::CAMERA, ImplStatus::STUB, "PlayStation Eye"},
    
    // Misc
    {libs::LIBRTC, "librtc.sprx", LibraryCategory::MISC, ImplStatus::COMPLETE, "Real Time Clock"},
    {libs::LIBUSBD, "libusbd.sprx", LibraryCategory::MISC, ImplStatus::STUB, "USB Driver"},
    {libs::LIBL10N, "libl10n.sprx", LibraryCategory::MISC, ImplStatus::PARTIAL, "Localization"},
    {libs::LIBMIC, "libmic.sprx", LibraryCategory::AUDIO, ImplStatus::STUB, "Microphone"},
    {libs::LIBVOICE, "libvoice.sprx", LibraryCategory::AUDIO, ImplStatus::STUB, "Voice Chat"},
    
    {nullptr, nullptr, LibraryCategory::MISC, ImplStatus::NOT_IMPLEMENTED, nullptr}
};

// =============================================================================
// Built-in Function Stubs
// =============================================================================

struct BuiltinFunction {
    const char* library;
    uint32_t nid;
    const char* name;
    ImplStatus status;
    bool critical;
    int32_t stub_return;
};

static const BuiltinFunction BUILTIN_FUNCS[] = {
    // libsysutil
    {libs::LIBSYSUTIL, nid::cellSysutilCheckCallback, "cellSysutilCheckCallback", ImplStatus::HLE, true, 0},
    {libs::LIBSYSUTIL, nid::cellSysutilRegisterCallback, "cellSysutilRegisterCallback", ImplStatus::HLE, true, 0},
    {libs::LIBSYSUTIL, nid::cellSysutilUnregisterCallback, "cellSysutilUnregisterCallback", ImplStatus::HLE, false, 0},
    {libs::LIBSYSUTIL, nid::cellSysutilGetSystemParamInt, "cellSysutilGetSystemParamInt", ImplStatus::HLE, true, 0},
    {libs::LIBSYSUTIL, nid::cellSysutilGetSystemParamString, "cellSysutilGetSystemParamString", ImplStatus::HLE, false, 0},
    
    // libsysutil_savedata
    {libs::LIBSYSUTIL_SAVEDATA, nid::cellSaveDataListSave2, "cellSaveDataListSave2", ImplStatus::PARTIAL, true, 0},
    {libs::LIBSYSUTIL_SAVEDATA, nid::cellSaveDataListLoad2, "cellSaveDataListLoad2", ImplStatus::PARTIAL, true, 0},
    {libs::LIBSYSUTIL_SAVEDATA, nid::cellSaveDataAutoSave2, "cellSaveDataAutoSave2", ImplStatus::PARTIAL, false, 0},
    {libs::LIBSYSUTIL_SAVEDATA, nid::cellSaveDataAutoLoad2, "cellSaveDataAutoLoad2", ImplStatus::PARTIAL, false, 0},
    
    // libgcm_sys
    {libs::LIBGCM_SYS, nid::cellGcmInit, "cellGcmInit", ImplStatus::HLE, true, 0},
    {libs::LIBGCM_SYS, nid::cellGcmSetFlipMode, "cellGcmSetFlipMode", ImplStatus::HLE, false, 0},
    {libs::LIBGCM_SYS, nid::cellGcmGetConfiguration, "cellGcmGetConfiguration", ImplStatus::HLE, true, 0},
    {libs::LIBGCM_SYS, nid::cellGcmAddressToOffset, "cellGcmAddressToOffset", ImplStatus::HLE, true, 0},
    
    // libaudio
    {libs::LIBAUDIO, nid::cellAudioInit, "cellAudioInit", ImplStatus::HLE, true, 0},
    {libs::LIBAUDIO, nid::cellAudioQuit, "cellAudioQuit", ImplStatus::HLE, false, 0},
    {libs::LIBAUDIO, nid::cellAudioPortOpen, "cellAudioPortOpen", ImplStatus::HLE, true, 0},
    {libs::LIBAUDIO, nid::cellAudioPortStart, "cellAudioPortStart", ImplStatus::HLE, false, 0},
    
    // libnet
    {libs::LIBNETCTL, nid::cellNetCtlInit, "cellNetCtlInit", ImplStatus::HLE, false, 0},
    {libs::LIBNETCTL, nid::cellNetCtlTerm, "cellNetCtlTerm", ImplStatus::HLE, false, 0},
    {libs::LIBNETCTL, nid::cellNetCtlGetState, "cellNetCtlGetState", ImplStatus::HLE, false, 0},
    {libs::LIBNETCTL, nid::cellNetCtlGetInfo, "cellNetCtlGetInfo", ImplStatus::STUB, false, 0},
    
    // libtrophy
    {libs::LIBTROPHY, nid::cellTrophyInit, "cellTrophyInit", ImplStatus::HLE, false, 0},
    {libs::LIBTROPHY, nid::cellTrophyTerm, "cellTrophyTerm", ImplStatus::HLE, false, 0},
    {libs::LIBTROPHY, nid::cellTrophyRegisterContext, "cellTrophyRegisterContext", ImplStatus::HLE, false, 0},
    {libs::LIBTROPHY, nid::cellTrophyUnlockTrophy, "cellTrophyUnlockTrophy", ImplStatus::HLE, false, 0},
    
    {nullptr, 0, nullptr, ImplStatus::NOT_IMPLEMENTED, false, 0}
};

// =============================================================================
// NID Name Database
// =============================================================================

static std::unordered_map<uint32_t, std::string> g_nid_names;

// =============================================================================
// Internal System
// =============================================================================

class LibraryEmulationSystem {
public:
    LibraryEmulationConfig config;
    LibraryStats stats = {};
    
    std::unordered_map<std::string, LibraryInfo> library_db;
    std::unordered_map<std::string, std::unordered_map<uint32_t, FunctionExport>> function_db;
    std::unordered_map<std::string, std::unordered_map<uint32_t, FunctionHandler>> hle_handlers;
    std::unordered_map<std::string, std::vector<GameLibraryOverride>> game_overrides;
    std::unordered_map<uint32_t, uint64_t> missing_function_counts;
    
    std::mutex mutex;
    std::string current_game;
    uint32_t next_handle = 1;
    
    bool Initialize(const LibraryEmulationConfig& cfg) {
        config = cfg;
        stats = {};
        
        // Завантажуємо базу бібліотек
        LoadLibraryDatabase();
        
        // Завантажуємо базу функцій
        LoadFunctionDatabase();
        
        // Реєструємо NID імена
        RegisterNIDNames();
        
        g_library_emulation_active.store(true);
        g_library_calls.store(0);
        
        LOGI("╔════════════════════════════════════════════════════════════╗");
        LOGI("║       Missing PS3 Library Emulation Layer                  ║");
        LOGI("╠════════════════════════════════════════════════════════════╣");
        LOGI("║  Libraries in DB: %-5zu                                    ║", library_db.size());
        LOGI("║  Auto Stub Missing: %-6s                                  ║", config.auto_stub_missing ? "Yes" : "No");
        LOGI("║  Log Missing: %-6s                                        ║", config.log_missing_functions ? "Yes" : "No");
        LOGI("╚════════════════════════════════════════════════════════════╝");
        
        return true;
    }
    
    void Shutdown() {
        g_library_emulation_active.store(false);
        
        LOGI("Library Emulation shutdown. Total calls: %llu",
             (unsigned long long)g_library_calls.load());
    }
    
    void LoadLibraryDatabase() {
        for (int i = 0; BUILTIN_LIBS[i].name != nullptr; ++i) {
            const auto& lib = BUILTIN_LIBS[i];
            
            LibraryInfo info;
            info.name = lib.name;
            info.filename = lib.filename;
            info.category = lib.category;
            info.status = lib.status;
            info.description = lib.description;
            info.version = 0x00010000;  // 1.0
            info.export_count = 0;
            info.implemented_count = 0;
            
            library_db[lib.name] = info;
        }
        
        stats.total_libraries = library_db.size();
    }
    
    void LoadFunctionDatabase() {
        for (int i = 0; BUILTIN_FUNCS[i].library != nullptr; ++i) {
            const auto& func = BUILTIN_FUNCS[i];
            
            FunctionExport exp;
            exp.name = func.name;
            exp.nid = func.nid;
            exp.status = func.status;
            exp.is_critical = func.critical;
            exp.call_count = 0;
            exp.stub_behavior = "return " + std::to_string(func.stub_return);
            
            function_db[func.library][func.nid] = exp;
            
            // Оновлюємо лічильники бібліотеки
            auto it = library_db.find(func.library);
            if (it != library_db.end()) {
                it->second.export_count++;
                if (func.status != ImplStatus::NOT_IMPLEMENTED && func.status != ImplStatus::STUB) {
                    it->second.implemented_count++;
                }
            }
        }
    }
    
    void RegisterNIDNames() {
        for (int i = 0; BUILTIN_FUNCS[i].name != nullptr; ++i) {
            g_nid_names[BUILTIN_FUNCS[i].nid] = BUILTIN_FUNCS[i].name;
        }
    }
    
    bool CallFunction(const char* library, uint32_t nid, uint64_t* args, uint64_t* result) {
        g_library_calls++;
        stats.total_function_calls++;
        
        std::string lib_name = library ? library : "";
        
        // Шукаємо HLE обробник
        {
            std::lock_guard<std::mutex> lock(mutex);
            
            auto lit = hle_handlers.find(lib_name);
            if (lit != hle_handlers.end()) {
                auto fit = lit->second.find(nid);
                if (fit != lit->second.end()) {
                    FunctionContext ctx;
                    ctx.nid = nid;
                    memcpy(ctx.args, args, sizeof(ctx.args));
                    ctx.result = result;
                    ctx.library_name = library;
                    ctx.function_name = GetFunctionNameByNID(nid);
                    
                    *result = fit->second(ctx);
                    return true;
                }
            }
        }
        
        // Шукаємо в базі
        auto lit = function_db.find(lib_name);
        if (lit != function_db.end()) {
            auto fit = lit->second.find(nid);
            if (fit != lit->second.end()) {
                fit->second.call_count++;
                
                if (fit->second.status == ImplStatus::NOT_IMPLEMENTED) {
                    stats.missing_function_calls++;
                    missing_function_counts[nid]++;
                    
                    if (config.log_missing_functions) {
                        LOGW("Missing function: %s::%s (NID 0x%08X)",
                             library, fit->second.name.c_str(), nid);
                    }
                    
                    if (fit->second.is_critical && config.crash_on_critical_missing) {
                        LOGE("CRITICAL: Missing critical function %s::%s",
                             library, fit->second.name.c_str());
                        abort();
                    }
                }
                
                // Stub - повертаємо успіх
                if (config.auto_stub_missing || fit->second.status == ImplStatus::STUB) {
                    stats.stubbed_function_calls++;
                    *result = 0;  // CELL_OK
                    return true;
                }
            }
        }
        
        // Невідома функція
        if (config.log_missing_functions) {
            LOGW("Unknown function: %s::NID_0x%08X", library, nid);
        }
        
        missing_function_counts[nid]++;
        stats.missing_function_calls++;
        
        if (config.auto_stub_missing) {
            *result = 0;
            return true;
        }
        
        return false;
    }
};

static LibraryEmulationSystem g_system;

// =============================================================================
// API Implementation
// =============================================================================

bool InitializeLibraryEmulation(const LibraryEmulationConfig& config) {
    return g_system.Initialize(config);
}

void ShutdownLibraryEmulation() {
    g_system.Shutdown();
}

bool IsLibraryEmulationActive() {
    return g_library_emulation_active.load();
}

uint32_t LoadLibrary(const char* name) {
    if (!name) return 0;
    
    auto it = g_system.library_db.find(name);
    if (it == g_system.library_db.end()) {
        LOGW("Library not found: %s", name);
        return 0;
    }
    
    g_system.stats.loaded_libraries++;
    LOGI("Loaded library: %s (%s)", name, it->second.description.c_str());
    
    return g_system.next_handle++;
}

bool UnloadLibrary(uint32_t handle) {
    // TODO: Відстежувати завантажені бібліотеки
    return true;
}

bool GetLibraryInfo(const char* name, LibraryInfo* info) {
    if (!name || !info) return false;
    
    auto it = g_system.library_db.find(name);
    if (it == g_system.library_db.end()) return false;
    
    *info = it->second;
    return true;
}

size_t GetAllLibraries(LibraryInfo* buffer, size_t buffer_size) {
    size_t count = 0;
    for (const auto& [name, info] : g_system.library_db) {
        if (count < buffer_size) {
            buffer[count] = info;
        }
        count++;
    }
    return count;
}

bool ResolveFunctionByNID(const char* library_name, uint32_t nid, uint64_t* address) {
    // Повертаємо фіктивну адресу для HLE
    if (address) {
        *address = 0x80000000 | nid;
    }
    return true;
}

bool ResolveFunctionByName(const char* library_name, const char* func_name, uint64_t* address) {
    uint32_t nid = CalculateNID(func_name);
    return ResolveFunctionByNID(library_name, nid, address);
}

bool RegisterHLEHandler(const char* library_name, uint32_t nid, FunctionHandler handler) {
    if (!library_name) return false;
    
    std::lock_guard<std::mutex> lock(g_system.mutex);
    g_system.hle_handlers[library_name][nid] = std::move(handler);
    
    LOGI("Registered HLE handler for %s::0x%08X", library_name, nid);
    return true;
}

bool UnregisterHLEHandler(const char* library_name, uint32_t nid) {
    if (!library_name) return false;
    
    std::lock_guard<std::mutex> lock(g_system.mutex);
    
    auto it = g_system.hle_handlers.find(library_name);
    if (it == g_system.hle_handlers.end()) return false;
    
    return it->second.erase(nid) > 0;
}

bool CallLibraryFunction(const char* library_name, uint32_t nid,
                          uint64_t* args, uint64_t* result) {
    if (!g_library_emulation_active.load()) return false;
    return g_system.CallFunction(library_name, nid, args, result);
}

bool GetFunctionInfo(const char* library_name, uint32_t nid, FunctionExport* info) {
    if (!library_name || !info) return false;
    
    auto lit = g_system.function_db.find(library_name);
    if (lit == g_system.function_db.end()) return false;
    
    auto fit = lit->second.find(nid);
    if (fit == lit->second.end()) return false;
    
    *info = fit->second;
    return true;
}

size_t GetLibraryFunctions(const char* library_name, FunctionExport* buffer, size_t buffer_size) {
    if (!library_name) return 0;
    
    auto it = g_system.function_db.find(library_name);
    if (it == g_system.function_db.end()) return 0;
    
    size_t count = 0;
    for (const auto& [nid, func] : it->second) {
        if (count < buffer_size) {
            buffer[count] = func;
        }
        count++;
    }
    return count;
}

bool SetGameLibraryOverride(const char* title_id, const char* library_name,
                             bool force_stub, bool disabled) {
    if (!title_id || !library_name) return false;
    
    GameLibraryOverride override;
    override.title_id = title_id;
    override.library_name = library_name;
    override.force_stub = force_stub;
    override.disabled = disabled;
    
    g_system.game_overrides[title_id].push_back(override);
    return true;
}

bool RemoveGameLibraryOverride(const char* title_id, const char* library_name) {
    if (!title_id || !library_name) return false;
    
    auto it = g_system.game_overrides.find(title_id);
    if (it == g_system.game_overrides.end()) return false;
    
    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [library_name](const GameLibraryOverride& o) {
            return o.library_name == library_name;
        }), vec.end());
    
    return true;
}

void SetCurrentGame(const char* title_id) {
    g_system.current_game = title_id ? title_id : "";
}

bool IsLibraryAvailable(const char* name) {
    if (!name) return false;
    return g_system.library_db.find(name) != g_system.library_db.end();
}

ImplStatus GetLibraryStatus(const char* name) {
    if (!name) return ImplStatus::NOT_IMPLEMENTED;
    
    auto it = g_system.library_db.find(name);
    if (it == g_system.library_db.end()) return ImplStatus::NOT_IMPLEMENTED;
    
    return it->second.status;
}

size_t GetMissingFunctions(const char* library_name, uint32_t* nid_buffer, size_t buffer_size) {
    size_t count = 0;
    for (const auto& [nid, cnt] : g_system.missing_function_counts) {
        if (count < buffer_size) {
            nid_buffer[count] = nid;
        }
        count++;
    }
    return count;
}

void GetLibraryStats(LibraryStats* stats) {
    if (stats) {
        *stats = g_system.stats;
        
        // Збираємо топ missing functions
        std::vector<std::pair<uint32_t, uint64_t>> missing;
        for (const auto& [nid, cnt] : g_system.missing_function_counts) {
            missing.emplace_back(nid, cnt);
        }
        std::sort(missing.begin(), missing.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        stats->top_missing_functions.clear();
        for (size_t i = 0; i < std::min((size_t)10, missing.size()); ++i) {
            const char* name = GetFunctionNameByNID(missing[i].first);
            stats->top_missing_functions.emplace_back(name ? name : "unknown", missing[i].second);
        }
    }
}

void ResetLibraryStats() {
    g_system.stats = {};
    g_system.stats.total_libraries = g_system.library_db.size();
    g_system.missing_function_counts.clear();
    g_library_calls.store(0);
}

size_t ExportLibraryStatsJson(char* buffer, size_t buffer_size) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"total_libraries\": " << g_system.stats.total_libraries << ",\n";
    ss << "  \"loaded_libraries\": " << g_system.stats.loaded_libraries << ",\n";
    ss << "  \"total_calls\": " << g_system.stats.total_function_calls << ",\n";
    ss << "  \"stubbed_calls\": " << g_system.stats.stubbed_function_calls << ",\n";
    ss << "  \"missing_calls\": " << g_system.stats.missing_function_calls << "\n";
    ss << "}\n";
    
    std::string json = ss.str();
    if (json.size() >= buffer_size) return 0;
    
    memcpy(buffer, json.c_str(), json.size() + 1);
    return json.size();
}

bool LoadHLEConfig(const char* filepath) {
    // TODO: Реалізувати
    return false;
}

bool SaveHLEConfig(const char* filepath) {
    // TODO: Реалізувати
    return false;
}

uint32_t CalculateNID(const char* function_name) {
    if (!function_name) return 0;
    
    // SHA-1 based NID calculation (simplified)
    uint32_t hash = 0;
    while (*function_name) {
        hash = hash * 31 + *function_name++;
    }
    return hash;
}

const char* GetFunctionNameByNID(uint32_t nid) {
    auto it = g_nid_names.find(nid);
    if (it != g_nid_names.end()) {
        return it->second.c_str();
    }
    return nullptr;
}

void RegisterNIDName(uint32_t nid, const char* name) {
    if (name) {
        g_nid_names[nid] = name;
    }
}

} // namespace rpcsx::libraries
