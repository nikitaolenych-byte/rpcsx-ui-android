/**
 * PPU Interceptor - Перехоплює виконання PPU коду в librpcsx.so
 * та перенаправляє на NCE ARM64 JIT
 * 
 * Стратегія:
 * 1. При завантаженні librpcsx.so шукаємо функції PPU interpreter
 * 2. Хукаємо їх через PLT hooking
 * 3. Перенаправляємо гарячі шляхи на наш JIT
 */

#include "ppu_interceptor.h"
#include "plt_hook.h"
#include "nce_jit/jit_compiler.h"
#include "nce_jit/ppc_decoder.h"

#include <android/log.h>
#include <dlfcn.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>

#define LOG_TAG "RPCSX-PPU-Intercept"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace rpcsx::ppu {

namespace {

// ===== JIT Engine =====
std::unique_ptr<nce::JitCompiler> g_jit;
nce::PPCState g_ppu_state;
std::mutex g_jit_mutex;

// ===== Statistics =====
std::atomic<uint64_t> g_blocks_executed{0};
std::atomic<uint64_t> g_blocks_jitted{0};
std::atomic<uint64_t> g_interpreter_fallbacks{0};
std::atomic<uint64_t> g_total_instructions{0};

// ===== Hotspot tracking =====
struct BlockStats {
    uint64_t execute_count = 0;
    bool is_jitted = false;
    void* jit_code = nullptr;
};
std::unordered_map<uint32_t, BlockStats> g_block_stats;
std::mutex g_stats_mutex;

// Threshold for JIT compilation (execute count before we JIT)
constexpr uint64_t JIT_THRESHOLD = 10;

// ===== Original function pointers =====
// These are the original interpreter functions from librpcsx.so
typedef void (*ppu_interpreter_fn)(void* thread, uint32_t pc);
typedef void (*ppu_execute_fn)(void* thread);
typedef void (*ppu_run_fn)(void* context);

ppu_interpreter_fn g_original_interpreter = nullptr;
ppu_execute_fn g_original_execute = nullptr;
ppu_run_fn g_original_run = nullptr;

// ===== Interceptor state =====
bool g_interceptor_active = false;
void* g_librpcsx_handle = nullptr;

// ===== JIT Helper Functions =====

bool InitializeJIT() {
    if (g_jit) return true;
    
    LOGI("Initializing NCE JIT compiler...");
    
    nce::JitConfig config;
    config.code_cache_size = 128 * 1024 * 1024;  // 128MB
    config.max_block_size = 4096;
    config.enable_block_linking = true;
    config.big_endian_memory = true;  // PS3 is big-endian
    
    g_jit = std::make_unique<nce::JitCompiler>(config);
    
    if (!g_jit->Initialize()) {
        LOGE("Failed to initialize JIT compiler");
        g_jit.reset();
        return false;
    }
    
    memset(&g_ppu_state, 0, sizeof(g_ppu_state));
    
    LOGI("NCE JIT compiler initialized successfully!");
    return true;
}

void ShutdownJIT() {
    if (g_jit) {
        g_jit->Shutdown();
        g_jit.reset();
    }
}

// Try to JIT execute a block
// Returns true if JIT handled it, false for interpreter fallback
bool TryJITExecute(void* memory_base, uint32_t pc) {
    if (!g_jit) return false;
    
    std::lock_guard<std::mutex> lock(g_jit_mutex);
    
    // Look up cached block
    auto* block = g_jit->LookupBlock(pc);
    
    if (!block) {
        // Track execution count
        {
            std::lock_guard<std::mutex> stats_lock(g_stats_mutex);
            auto& stats = g_block_stats[pc];
            stats.execute_count++;
            
            // Only JIT hot blocks
            if (stats.execute_count < JIT_THRESHOLD) {
                return false;
            }
            
            if (stats.is_jitted) {
                // Already tried and failed?
                return false;
            }
        }
        
        // Compile new block
        const uint8_t* guest_code = static_cast<const uint8_t*>(memory_base) + pc;
        block = g_jit->CompileBlock(guest_code, pc);
        
        if (!block) {
            g_interpreter_fallbacks++;
            return false;
        }
        
        g_blocks_jitted++;
        
        {
            std::lock_guard<std::mutex> stats_lock(g_stats_mutex);
            g_block_stats[pc].is_jitted = true;
            g_block_stats[pc].jit_code = block->code;
        }
    }
    
    // Execute JIT block
    g_ppu_state.pc = pc;
    g_ppu_state.memory_base = memory_base;
    
    g_jit->Execute(&g_ppu_state, block);
    
    g_blocks_executed++;
    g_total_instructions += block->guest_size / 4;
    
    return true;
}

// ===== Hook Functions =====
// These replace the librpcsx.so interpreter functions

void HookedPPUInterpreter(void* thread, uint32_t pc) {
    // Try JIT first
    // Note: We need to get memory_base from thread context
    // For now, just call original and track stats
    
    g_blocks_executed++;
    
    if (g_original_interpreter) {
        g_original_interpreter(thread, pc);
    }
}

void HookedPPUExecute(void* thread) {
    g_blocks_executed++;
    
    if (g_original_execute) {
        g_original_execute(thread);
    }
}

void HookedPPURun(void* context) {
    LOGD("PPU Run intercepted!");
    
    if (g_original_run) {
        g_original_run(context);
    }
}

// ===== Symbol candidates to hook =====
// These are potential function names in librpcsx.so
const char* PPU_SYMBOLS[] = {
    // RPCS3-style names
    "ppu_interpreter",
    "ppu_interpreter_fast",
    "ppu_execute",
    "ppu_run",
    "_ZN3ppu7executeEv",  // ppu::execute() mangled
    "_ZN3ppu3runEv",      // ppu::run() mangled
    
    // RPCSX-specific names (might differ)
    "executePPU",
    "runPPU", 
    "ppu_step",
    "ppu_single_step",
    
    // Cell/SPU related
    "cell_execute",
    "cell_ppu_run",
    
    nullptr
};

} // anonymous namespace

// ===== Public API =====

bool InitializeInterceptor(const char* librpcsx_path) {
    if (g_interceptor_active) {
        LOGI("Interceptor already active");
        return true;
    }
    
    LOGI("Initializing PPU Interceptor for: %s", librpcsx_path);
    
    // Initialize PLT hook system
    if (!hook::InitializePLTHook()) {
        LOGE("Failed to initialize PLT hook system");
        return false;
    }
    
    // Initialize JIT
    if (!InitializeJIT()) {
        LOGE("Failed to initialize JIT compiler");
        return false;
    }
    
    // Open library handle
    g_librpcsx_handle = dlopen(librpcsx_path, RTLD_NOLOAD);
    if (!g_librpcsx_handle) {
        LOGE("librpcsx.so not loaded yet");
        // Not an error - we'll hook when it's loaded
    }
    
    // Try to find and hook PPU functions
    int hooks_installed = 0;
    
    for (const char** sym = PPU_SYMBOLS; *sym != nullptr; sym++) {
        void* original = nullptr;
        
        auto result = hook::HookFunction(
            librpcsx_path,
            *sym,
            reinterpret_cast<void*>(HookedPPURun),
            &original
        );
        
        if (result.success) {
            LOGI("✓ Hooked: %s", *sym);
            hooks_installed++;
            
            // Save original based on function type
            // (In reality we'd need to know which function this is)
            if (strstr(*sym, "run") || strstr(*sym, "Run")) {
                g_original_run = reinterpret_cast<ppu_run_fn>(original);
            } else if (strstr(*sym, "execute") || strstr(*sym, "Execute")) {
                g_original_execute = reinterpret_cast<ppu_execute_fn>(original);
            } else {
                g_original_interpreter = reinterpret_cast<ppu_interpreter_fn>(original);
            }
        } else {
            LOGD("✗ Not found: %s", *sym);
        }
    }
    
    if (hooks_installed > 0) {
        LOGI("PPU Interceptor active with %d hooks", hooks_installed);
        g_interceptor_active = true;
        return true;
    } else {
        LOGI("No PPU functions found to hook - will use callback mode");
        // Not necessarily a failure - librpcsx might use different symbols
        g_interceptor_active = true;
        return true;
    }
}

void ShutdownInterceptor() {
    if (!g_interceptor_active) return;
    
    LOGI("Shutting down PPU Interceptor");
    
    // Print statistics
    LOGI("=== PPU Interceptor Statistics ===");
    LOGI("Blocks executed: %llu", (unsigned long long)g_blocks_executed.load());
    LOGI("Blocks JIT'd: %llu", (unsigned long long)g_blocks_jitted.load());
    LOGI("Interpreter fallbacks: %llu", (unsigned long long)g_interpreter_fallbacks.load());
    LOGI("Total instructions: %llu", (unsigned long long)g_total_instructions.load());
    
    // Restore hooks
    hook::ShutdownPLTHook();
    
    // Shutdown JIT
    ShutdownJIT();
    
    // Clear stats
    {
        std::lock_guard<std::mutex> lock(g_stats_mutex);
        g_block_stats.clear();
    }
    
    g_interceptor_active = false;
    
    LOGI("PPU Interceptor shutdown complete");
}

bool IsInterceptorActive() {
    return g_interceptor_active;
}

InterceptorStats GetStats() {
    InterceptorStats stats;
    stats.blocks_executed = g_blocks_executed.load();
    stats.blocks_jitted = g_blocks_jitted.load();
    stats.interpreter_fallbacks = g_interpreter_fallbacks.load();
    stats.total_instructions = g_total_instructions.load();
    
    {
        std::lock_guard<std::mutex> lock(g_stats_mutex);
        stats.unique_blocks = g_block_stats.size();
    }
    
    return stats;
}

void ResetStats() {
    g_blocks_executed = 0;
    g_blocks_jitted = 0;
    g_interpreter_fallbacks = 0;
    g_total_instructions = 0;
    
    {
        std::lock_guard<std::mutex> lock(g_stats_mutex);
        for (auto& [pc, stats] : g_block_stats) {
            stats.execute_count = 0;
        }
    }
}

// ===== Manual execution API =====
// For direct calls from native-lib.cpp

bool ExecuteBlock(void* memory_base, uint32_t pc, uint32_t* next_pc) {
    if (!g_jit) return false;
    
    bool success = TryJITExecute(memory_base, pc);
    
    if (success && next_pc) {
        *next_pc = static_cast<uint32_t>(g_ppu_state.pc);
    }
    
    return success;
}

void InvalidateBlock(uint32_t pc) {
    if (g_jit) {
        g_jit->InvalidateBlock(pc);
    }
    
    {
        std::lock_guard<std::mutex> lock(g_stats_mutex);
        auto it = g_block_stats.find(pc);
        if (it != g_block_stats.end()) {
            it->second.is_jitted = false;
            it->second.jit_code = nullptr;
        }
    }
}

void InvalidateRange(uint32_t start, uint32_t size) {
    if (g_jit) {
        g_jit->InvalidateRange(start, size);
    }
    
    {
        std::lock_guard<std::mutex> lock(g_stats_mutex);
        for (auto& [pc, stats] : g_block_stats) {
            if (pc >= start && pc < start + size) {
                stats.is_jitted = false;
                stats.jit_code = nullptr;
            }
        }
    }
}

} // namespace rpcsx::ppu
