/**
 * PS3 Syscall Stubbing System Implementation
 */

#include "syscall_stubs.h"
#include <android/log.h>
#include <unordered_map>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <ctime>

#define LOG_TAG "RPCSX-SyscallStubs"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace rpcsx::syscalls {

// Глобальні атомарні
std::atomic<bool> g_stubs_active{false};
std::atomic<uint64_t> g_total_syscalls{0};
std::atomic<uint64_t> g_stubbed_syscalls{0};

// =============================================================================
// Вбудована база syscalls
// =============================================================================

struct SyscallDef {
    uint32_t num;
    const char* name;
    SyscallCategory category;
    bool implemented;
    StubBehavior behavior;
    int32_t default_ret;
};

static const SyscallDef SYSCALL_DEFS[] = {
    // Process
    {1,   "sys_process_getpid",          SyscallCategory::PROCESS, true,  StubBehavior::FORWARD, 0},
    {2,   "sys_process_getppid",         SyscallCategory::PROCESS, true,  StubBehavior::FORWARD, 0},
    {3,   "sys_process_exit",            SyscallCategory::PROCESS, true,  StubBehavior::FORWARD, 0},
    {22,  "sys_process_get_paramsfo",    SyscallCategory::PROCESS, true,  StubBehavior::FORWARD, 0},
    {25,  "sys_process_get_sdk_version", SyscallCategory::PROCESS, true,  StubBehavior::FORWARD, 0},
    
    // Thread
    {41,  "sys_ppu_thread_create",       SyscallCategory::THREAD, true,  StubBehavior::FORWARD, 0},
    {42,  "sys_ppu_thread_exit",         SyscallCategory::THREAD, true,  StubBehavior::FORWARD, 0},
    {43,  "sys_ppu_thread_yield",        SyscallCategory::THREAD, true,  StubBehavior::FORWARD, 0},
    {44,  "sys_ppu_thread_join",         SyscallCategory::THREAD, true,  StubBehavior::FORWARD, 0},
    {45,  "sys_ppu_thread_detach",       SyscallCategory::THREAD, true,  StubBehavior::FORWARD, 0},
    {46,  "sys_ppu_thread_get_id",       SyscallCategory::THREAD, true,  StubBehavior::FORWARD, 0},
    {47,  "sys_ppu_thread_set_priority", SyscallCategory::THREAD, true,  StubBehavior::FORWARD, 0},
    {48,  "sys_ppu_thread_get_priority", SyscallCategory::THREAD, true,  StubBehavior::FORWARD, 0},
    {49,  "sys_ppu_thread_get_stack_information", SyscallCategory::THREAD, true, StubBehavior::FORWARD, 0},
    {56,  "sys_ppu_thread_rename",       SyscallCategory::THREAD, false, StubBehavior::RETURN_SUCCESS, 0},
    
    // Mutex
    {100, "sys_mutex_create",            SyscallCategory::MUTEX, true,  StubBehavior::FORWARD, 0},
    {101, "sys_mutex_destroy",           SyscallCategory::MUTEX, true,  StubBehavior::FORWARD, 0},
    {102, "sys_mutex_lock",              SyscallCategory::MUTEX, true,  StubBehavior::FORWARD, 0},
    {103, "sys_mutex_trylock",           SyscallCategory::MUTEX, true,  StubBehavior::FORWARD, 0},
    {104, "sys_mutex_unlock",            SyscallCategory::MUTEX, true,  StubBehavior::FORWARD, 0},
    
    // Cond
    {110, "sys_cond_create",             SyscallCategory::COND, true,  StubBehavior::FORWARD, 0},
    {111, "sys_cond_destroy",            SyscallCategory::COND, true,  StubBehavior::FORWARD, 0},
    {112, "sys_cond_wait",               SyscallCategory::COND, true,  StubBehavior::FORWARD, 0},
    {113, "sys_cond_signal",             SyscallCategory::COND, true,  StubBehavior::FORWARD, 0},
    {114, "sys_cond_signal_all",         SyscallCategory::COND, true,  StubBehavior::FORWARD, 0},
    
    // RWLock
    {120, "sys_rwlock_create",           SyscallCategory::RWLOCK, true,  StubBehavior::FORWARD, 0},
    {121, "sys_rwlock_destroy",          SyscallCategory::RWLOCK, true,  StubBehavior::FORWARD, 0},
    {122, "sys_rwlock_rlock",            SyscallCategory::RWLOCK, true,  StubBehavior::FORWARD, 0},
    {123, "sys_rwlock_tryrlock",         SyscallCategory::RWLOCK, true,  StubBehavior::FORWARD, 0},
    {124, "sys_rwlock_runlock",          SyscallCategory::RWLOCK, true,  StubBehavior::FORWARD, 0},
    {125, "sys_rwlock_wlock",            SyscallCategory::RWLOCK, true,  StubBehavior::FORWARD, 0},
    {126, "sys_rwlock_trywlock",         SyscallCategory::RWLOCK, true,  StubBehavior::FORWARD, 0},
    {127, "sys_rwlock_wunlock",          SyscallCategory::RWLOCK, true,  StubBehavior::FORWARD, 0},
    
    // Semaphore
    {90,  "sys_semaphore_create",        SyscallCategory::SEMAPHORE, true,  StubBehavior::FORWARD, 0},
    {91,  "sys_semaphore_destroy",       SyscallCategory::SEMAPHORE, true,  StubBehavior::FORWARD, 0},
    {92,  "sys_semaphore_wait",          SyscallCategory::SEMAPHORE, true,  StubBehavior::FORWARD, 0},
    {93,  "sys_semaphore_trywait",       SyscallCategory::SEMAPHORE, true,  StubBehavior::FORWARD, 0},
    {94,  "sys_semaphore_post",          SyscallCategory::SEMAPHORE, true,  StubBehavior::FORWARD, 0},
    
    // Event Flag
    {130, "sys_event_flag_create",       SyscallCategory::EVENT_FLAG, true,  StubBehavior::FORWARD, 0},
    {131, "sys_event_flag_destroy",      SyscallCategory::EVENT_FLAG, true,  StubBehavior::FORWARD, 0},
    {132, "sys_event_flag_wait",         SyscallCategory::EVENT_FLAG, true,  StubBehavior::FORWARD, 0},
    {133, "sys_event_flag_trywait",      SyscallCategory::EVENT_FLAG, true,  StubBehavior::FORWARD, 0},
    {134, "sys_event_flag_set",          SyscallCategory::EVENT_FLAG, true,  StubBehavior::FORWARD, 0},
    {135, "sys_event_flag_clear",        SyscallCategory::EVENT_FLAG, true,  StubBehavior::FORWARD, 0},
    
    // Event Queue
    {140, "sys_event_queue_create",      SyscallCategory::EVENT_QUEUE, true,  StubBehavior::FORWARD, 0},
    {141, "sys_event_queue_destroy",     SyscallCategory::EVENT_QUEUE, true,  StubBehavior::FORWARD, 0},
    {142, "sys_event_queue_receive",     SyscallCategory::EVENT_QUEUE, true,  StubBehavior::FORWARD, 0},
    
    // Timer
    {141, "sys_timer_usleep",            SyscallCategory::TIMER, true,  StubBehavior::FORWARD, 0},
    {142, "sys_timer_sleep",             SyscallCategory::TIMER, true,  StubBehavior::FORWARD, 0},
    {145, "sys_time_get_current_time",   SyscallCategory::TIME, true,  StubBehavior::FORWARD, 0},
    {146, "sys_time_get_timezone",       SyscallCategory::TIME, true,  StubBehavior::FORWARD, 0},
    
    // Memory
    {330, "sys_mmapper_allocate_address", SyscallCategory::MEMORY, true,  StubBehavior::FORWARD, 0},
    {331, "sys_mmapper_free_address",     SyscallCategory::MEMORY, true,  StubBehavior::FORWARD, 0},
    {348, "sys_memory_allocate",          SyscallCategory::MEMORY, true,  StubBehavior::FORWARD, 0},
    {349, "sys_memory_free",              SyscallCategory::MEMORY, true,  StubBehavior::FORWARD, 0},
    {352, "sys_memory_get_page_attribute", SyscallCategory::MEMORY, true,  StubBehavior::FORWARD, 0},
    
    // SPU
    {160, "sys_spu_thread_group_create",    SyscallCategory::SPU, true,  StubBehavior::FORWARD, 0},
    {161, "sys_spu_thread_group_destroy",   SyscallCategory::SPU, true,  StubBehavior::FORWARD, 0},
    {162, "sys_spu_thread_group_start",     SyscallCategory::SPU, true,  StubBehavior::FORWARD, 0},
    {163, "sys_spu_thread_group_suspend",   SyscallCategory::SPU, true,  StubBehavior::FORWARD, 0},
    {164, "sys_spu_thread_group_terminate", SyscallCategory::SPU, true,  StubBehavior::FORWARD, 0},
    {168, "sys_spu_thread_initialize",      SyscallCategory::SPU, true,  StubBehavior::FORWARD, 0},
    {169, "sys_spu_thread_group_join",      SyscallCategory::SPU, true,  StubBehavior::FORWARD, 0},
    
    // PRX
    {480, "sys_prx_load_module",         SyscallCategory::PRX, true,  StubBehavior::FORWARD, 0},
    {481, "sys_prx_start_module",        SyscallCategory::PRX, true,  StubBehavior::FORWARD, 0},
    {482, "sys_prx_stop_module",         SyscallCategory::PRX, true,  StubBehavior::FORWARD, 0},
    {483, "sys_prx_unload_module",       SyscallCategory::PRX, true,  StubBehavior::FORWARD, 0},
    {484, "sys_prx_get_module_list",     SyscallCategory::PRX, false, StubBehavior::RETURN_SUCCESS, 0},
    {485, "sys_prx_get_module_info",     SyscallCategory::PRX, false, StubBehavior::RETURN_SUCCESS, 0},
    {486, "sys_prx_get_module_id_by_name", SyscallCategory::PRX, false, StubBehavior::RETURN_SUCCESS, 0},
    
    // Filesystem
    {801, "sys_fs_open",                 SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    {802, "sys_fs_read",                 SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    {803, "sys_fs_write",                SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    {804, "sys_fs_close",                SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    {805, "sys_fs_opendir",              SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    {806, "sys_fs_readdir",              SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    {807, "sys_fs_closedir",             SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    {808, "sys_fs_lseek",                SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    {811, "sys_fs_mkdir",                SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    {814, "sys_fs_unlink",               SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    {818, "sys_fs_stat",                 SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    {819, "sys_fs_fstat",                SyscallCategory::FILESYSTEM, true,  StubBehavior::FORWARD, 0},
    
    // USB - зазвичай не потрібні
    {600, "sys_usbd_initialize",         SyscallCategory::USB, false, StubBehavior::RETURN_SUCCESS, 0},
    {601, "sys_usbd_finalize",           SyscallCategory::USB, false, StubBehavior::RETURN_SUCCESS, 0},
    
    // Bluetooth - не підтримується
    {610, "sys_bt_init",                 SyscallCategory::BLUETOOTH, false, StubBehavior::RETURN_SUCCESS, 0},
    {611, "sys_bt_end",                  SyscallCategory::BLUETOOTH, false, StubBehavior::RETURN_SUCCESS, 0},
    
    // Camera - не підтримується
    {620, "sys_camera_init",             SyscallCategory::CAMERA, false, StubBehavior::RETURN_ERROR, ps3_error::CELL_ENODEV},
    {621, "sys_camera_end",              SyscallCategory::CAMERA, false, StubBehavior::RETURN_SUCCESS, 0},
    
    // Термінатор
    {0, nullptr, SyscallCategory::UNKNOWN, false, StubBehavior::RETURN_ERROR, 0}
};

// =============================================================================
// Internal System
// =============================================================================

class SyscallStubSystem {
public:
    StubConfig config;
    
    std::unordered_map<uint32_t, SyscallInfo> syscall_db;
    std::unordered_map<uint32_t, SyscallHandler> custom_handlers;
    std::unordered_map<std::string, std::vector<GameSyscallOverride>> game_overrides;
    
    SyscallStats stats = {};
    std::mutex stats_mutex;
    std::mutex handler_mutex;
    
    const char* current_title_id = nullptr;
    
    bool Initialize(const StubConfig& cfg) {
        config = cfg;
        stats = {};
        
        // Завантажуємо базу syscalls
        LoadSyscallDatabase();
        
        g_stubs_active.store(true);
        g_total_syscalls.store(0);
        g_stubbed_syscalls.store(0);
        
        LOGI("╔════════════════════════════════════════════════════════════╗");
        LOGI("║          PS3 Syscall Stubbing System                       ║");
        LOGI("╠════════════════════════════════════════════════════════════╣");
        LOGI("║  Syscalls in DB: %-5zu                                     ║", syscall_db.size());
        LOGI("║  Log Unimplemented: %-6s                                  ║", config.log_unimplemented ? "Yes" : "No");
        LOGI("║  Default Behavior: %-8s                                ║", GetBehaviorName(config.default_behavior));
        LOGI("╚════════════════════════════════════════════════════════════╝");
        
        return true;
    }
    
    void Shutdown() {
        g_stubs_active.store(false);
        
        LOGI("Syscall Stubs shutdown. Total: %llu, Stubbed: %llu",
             (unsigned long long)g_total_syscalls.load(),
             (unsigned long long)g_stubbed_syscalls.load());
    }
    
    void LoadSyscallDatabase() {
        for (int i = 0; SYSCALL_DEFS[i].name != nullptr; ++i) {
            const auto& def = SYSCALL_DEFS[i];
            
            SyscallInfo info;
            info.number = def.num;
            info.name = def.name;
            info.category = def.category;
            info.is_implemented = def.implemented;
            info.default_behavior = def.behavior;
            info.default_return = def.default_ret;
            info.call_count = 0;
            info.log_calls = config.log_all_calls || (!def.implemented && config.log_unimplemented);
            
            syscall_db[def.num] = info;
        }
    }
    
    bool HandleSyscallInternal(uint32_t num, uint64_t* args, uint64_t* result) {
        g_total_syscalls++;
        stats.total_calls++;
        
        // Шукаємо в базі
        auto it = syscall_db.find(num);
        SyscallInfo* info = (it != syscall_db.end()) ? &it->second : nullptr;
        
        if (info) {
            info->call_count++;
        }
        
        // Перевіряємо game override
        StubBehavior behavior = config.default_behavior;
        int32_t return_value = config.default_error_code;
        
        if (current_title_id) {
            auto git = game_overrides.find(current_title_id);
            if (git != game_overrides.end()) {
                for (const auto& override : git->second) {
                    if (override.syscall_number == num) {
                        behavior = override.behavior;
                        return_value = override.custom_return;
                        break;
                    }
                }
            }
        }
        
        if (info) {
            if (info->default_behavior != StubBehavior::FORWARD) {
                behavior = info->default_behavior;
                return_value = info->default_return;
            }
        }
        
        // Перевіряємо custom handler
        {
            std::lock_guard<std::mutex> lock(handler_mutex);
            auto hit = custom_handlers.find(num);
            if (hit != custom_handlers.end()) {
                SyscallContext ctx;
                ctx.syscall_num = num;
                memcpy(ctx.args, args, sizeof(ctx.args));
                ctx.result = result;
                ctx.title_id = current_title_id;
                
                *result = hit->second(ctx);
                return true;
            }
        }
        
        // Якщо реалізовано - не обробляємо
        if (info && info->is_implemented && behavior == StubBehavior::FORWARD) {
            stats.implemented_calls++;
            return false;
        }
        
        // Обробляємо як stub
        g_stubbed_syscalls++;
        stats.stubbed_calls++;
        
        if (info && !info->is_implemented) {
            stats.unimplemented_used++;
        }
        
        // Логування
        if (info && info->log_calls) {
            LOGW("STUB: %s (syscall %u) - args: %llx %llx %llx %llx",
                 info->name.c_str(), num,
                 (unsigned long long)args[0], (unsigned long long)args[1],
                 (unsigned long long)args[2], (unsigned long long)args[3]);
        } else if (!info && config.log_unimplemented) {
            LOGW("STUB: Unknown syscall %u - args: %llx %llx %llx %llx",
                 num,
                 (unsigned long long)args[0], (unsigned long long)args[1],
                 (unsigned long long)args[2], (unsigned long long)args[3]);
        }
        
        // Застосовуємо behavior
        switch (behavior) {
            case StubBehavior::RETURN_SUCCESS:
                *result = ps3_error::CELL_OK;
                break;
                
            case StubBehavior::RETURN_ERROR:
            case StubBehavior::RETURN_CUSTOM:
                *result = return_value;
                break;
                
            case StubBehavior::LOG_ONLY:
                return false; // Передаємо далі
                
            case StubBehavior::CRASH:
                LOGE("CRITICAL: Syscall %u requires implementation!", num);
                abort();
                break;
                
            case StubBehavior::SKIP:
                *result = 0;
                break;
                
            default:
                *result = ps3_error::CELL_ENOSYS;
                break;
        }
        
        return true;
    }
    
    const char* GetBehaviorName(StubBehavior b) {
        switch (b) {
            case StubBehavior::RETURN_SUCCESS: return "SUCCESS";
            case StubBehavior::RETURN_ERROR: return "ERROR";
            case StubBehavior::RETURN_CUSTOM: return "CUSTOM";
            case StubBehavior::LOG_ONLY: return "LOG";
            case StubBehavior::CRASH: return "CRASH";
            case StubBehavior::FORWARD: return "FORWARD";
            case StubBehavior::EMULATE: return "EMULATE";
            case StubBehavior::SKIP: return "SKIP";
            default: return "UNKNOWN";
        }
    }
};

static SyscallStubSystem g_system;

// =============================================================================
// API Implementation
// =============================================================================

bool InitializeSyscallStubs(const StubConfig& config) {
    return g_system.Initialize(config);
}

void ShutdownSyscallStubs() {
    g_system.Shutdown();
}

bool IsStubSystemActive() {
    return g_stubs_active.load();
}

bool HandleSyscall(uint32_t syscall_num, uint64_t* args, uint64_t* result) {
    if (!g_stubs_active.load()) return false;
    return g_system.HandleSyscallInternal(syscall_num, args, result);
}

bool GetSyscallInfo(uint32_t syscall_num, SyscallInfo* info) {
    if (!info) return false;
    
    auto it = g_system.syscall_db.find(syscall_num);
    if (it == g_system.syscall_db.end()) return false;
    
    *info = it->second;
    return true;
}

bool RegisterSyscallHandler(uint32_t syscall_num, SyscallHandler handler) {
    std::lock_guard<std::mutex> lock(g_system.handler_mutex);
    g_system.custom_handlers[syscall_num] = std::move(handler);
    return true;
}

bool UnregisterSyscallHandler(uint32_t syscall_num) {
    std::lock_guard<std::mutex> lock(g_system.handler_mutex);
    return g_system.custom_handlers.erase(syscall_num) > 0;
}

bool SetSyscallBehavior(uint32_t syscall_num, StubBehavior behavior, int32_t custom_return) {
    auto it = g_system.syscall_db.find(syscall_num);
    if (it == g_system.syscall_db.end()) return false;
    
    it->second.default_behavior = behavior;
    it->second.default_return = custom_return;
    return true;
}

bool SetGameSyscallOverride(const char* title_id, uint32_t syscall_num,
                            StubBehavior behavior, int32_t custom_return) {
    if (!title_id) return false;
    
    GameSyscallOverride override;
    override.title_id = title_id;
    override.syscall_number = syscall_num;
    override.behavior = behavior;
    override.custom_return = custom_return;
    
    g_system.game_overrides[title_id].push_back(override);
    return true;
}

bool RemoveGameSyscallOverride(const char* title_id, uint32_t syscall_num) {
    if (!title_id) return false;
    
    auto it = g_system.game_overrides.find(title_id);
    if (it == g_system.game_overrides.end()) return false;
    
    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [syscall_num](const GameSyscallOverride& o) {
            return o.syscall_number == syscall_num;
        }), vec.end());
    
    return true;
}

bool LoadSyscallOverrides(const char* filepath) {
    // TODO: Реалізувати
    return false;
}

bool SaveSyscallOverrides(const char* filepath) {
    // TODO: Реалізувати
    return false;
}

void GetSyscallStats(SyscallStats* stats) {
    if (stats) {
        std::lock_guard<std::mutex> lock(g_system.stats_mutex);
        *stats = g_system.stats;
        
        // Збираємо топ syscalls
        std::vector<std::pair<uint32_t, uint64_t>> all;
        for (const auto& [num, info] : g_system.syscall_db) {
            if (info.call_count > 0) {
                all.emplace_back(num, info.call_count);
            }
        }
        std::sort(all.begin(), all.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        stats->top_syscalls.clear();
        for (size_t i = 0; i < std::min((size_t)10, all.size()); ++i) {
            stats->top_syscalls.push_back(all[i]);
        }
        
        stats->unique_syscalls_used = all.size();
    }
}

void ResetSyscallStats() {
    std::lock_guard<std::mutex> lock(g_system.stats_mutex);
    g_system.stats = {};
    g_total_syscalls.store(0);
    g_stubbed_syscalls.store(0);
    
    for (auto& [num, info] : g_system.syscall_db) {
        info.call_count = 0;
    }
}

size_t GetUnimplementedSyscalls(uint32_t* buffer, size_t buffer_size) {
    size_t count = 0;
    for (const auto& [num, info] : g_system.syscall_db) {
        if (!info.is_implemented && info.call_count > 0) {
            if (count < buffer_size) {
                buffer[count] = num;
            }
            count++;
        }
    }
    return count;
}

size_t ExportSyscallLogJson(char* buffer, size_t buffer_size) {
    std::stringstream ss;
    ss << "{\n  \"syscalls\": [\n";
    
    bool first = true;
    for (const auto& [num, info] : g_system.syscall_db) {
        if (info.call_count > 0) {
            if (!first) ss << ",\n";
            first = false;
            ss << "    {\"num\": " << num
               << ", \"name\": \"" << info.name << "\""
               << ", \"calls\": " << info.call_count
               << ", \"implemented\": " << (info.is_implemented ? "true" : "false")
               << "}";
        }
    }
    
    ss << "\n  ],\n";
    ss << "  \"total_calls\": " << g_total_syscalls.load() << ",\n";
    ss << "  \"stubbed_calls\": " << g_stubbed_syscalls.load() << "\n";
    ss << "}\n";
    
    std::string json = ss.str();
    if (json.size() >= buffer_size) return 0;
    
    memcpy(buffer, json.c_str(), json.size() + 1);
    return json.size();
}

bool SetSyscallLogging(uint32_t syscall_num, bool enable) {
    auto it = g_system.syscall_db.find(syscall_num);
    if (it == g_system.syscall_db.end()) return false;
    
    it->second.log_calls = enable;
    return true;
}

bool IsSyscallImplemented(uint32_t syscall_num) {
    auto it = g_system.syscall_db.find(syscall_num);
    return it != g_system.syscall_db.end() && it->second.is_implemented;
}

const char* GetSyscallName(uint32_t syscall_num) {
    auto it = g_system.syscall_db.find(syscall_num);
    if (it == g_system.syscall_db.end()) return "unknown";
    return it->second.name.c_str();
}

SyscallCategory GetSyscallCategory(uint32_t syscall_num) {
    auto it = g_system.syscall_db.find(syscall_num);
    if (it == g_system.syscall_db.end()) return SyscallCategory::UNKNOWN;
    return it->second.category;
}

} // namespace rpcsx::syscalls
