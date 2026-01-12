/**
 * NCE PPU Hook - LD_PRELOAD бібліотека для перехоплення PPU виконання
 * Завантажується перед librpcsx.so і перехоплює виклики PPU interpreter
 */

#include <dlfcn.h>
#include <android/log.h>
#include <cstring>
#include <atomic>

#include "nce_jit/jit_compiler.h"
#include "nce_jit/ppc_decoder.h"
#include "nce_jit/arm64_emitter.h"

#define LOG_TAG "RPCSX-NCE-Hook"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

// NCE JIT compiler instance
static std::unique_ptr<rpcsx::nce::JitCompiler> g_jit;
static rpcsx::nce::PPCState g_ppu_state;
static std::atomic<bool> g_nce_enabled{false};
static std::atomic<uint64_t> g_instructions_jitted{0};

// Original function pointers (from librpcsx.so)
typedef void (*ppu_execute_fn)(void* ppu_thread, uint32_t addr);
static ppu_execute_fn g_original_ppu_execute = nullptr;

bool InitializeNCEHook() {
    if (g_jit) return true;
    
    LOGI("Initializing NCE PPU Hook...");
    
    rpcsx::nce::JitConfig config;
    config.code_cache_size = 128 * 1024 * 1024;  // 128MB
    config.max_block_size = 4096;
    config.enable_block_linking = true;
    config.big_endian_memory = true;
    
    g_jit = std::make_unique<rpcsx::nce::JitCompiler>(config);
    
    if (!g_jit->Initialize()) {
        LOGE("Failed to initialize NCE JIT");
        g_jit.reset();
        return false;
    }
    
    memset(&g_ppu_state, 0, sizeof(g_ppu_state));
    g_nce_enabled = true;
    
    LOGI("NCE PPU Hook initialized successfully!");
    return true;
}

void ShutdownNCEHook() {
    if (g_jit) {
        g_jit->Shutdown();
        g_jit.reset();
    }
    g_nce_enabled = false;
    LOGI("NCE PPU Hook shutdown. Total instructions JIT'd: %llu", 
         (unsigned long long)g_instructions_jitted.load());
}

// Try to JIT execute a PPU block
// Returns true if JIT handled it, false to fall back to interpreter
bool TryNCEExecute(void* memory_base, uint32_t addr, uint32_t* next_addr) {
    if (!g_nce_enabled || !g_jit) return false;
    
    // Look up or compile block
    auto* block = g_jit->LookupBlock(addr);
    
    if (!block) {
        // Get pointer to guest code
        const uint8_t* guest_code = static_cast<const uint8_t*>(memory_base) + addr;
        
        block = g_jit->CompileBlock(guest_code, addr);
        if (!block) {
            return false;  // Can't compile, use interpreter
        }
        
        g_instructions_jitted += block->guest_size / 4;
    }
    
    // Setup state and execute
    g_ppu_state.pc = addr;
    g_ppu_state.memory_base = memory_base;
    
    g_jit->Execute(&g_ppu_state, block);
    
    if (next_addr) {
        *next_addr = static_cast<uint32_t>(g_ppu_state.pc);
    }
    
    return true;
}

} // anonymous namespace

// Export functions for librpcsx integration
extern "C" {

__attribute__((visibility("default")))
bool nce_hook_initialize() {
    return InitializeNCEHook();
}

__attribute__((visibility("default")))
void nce_hook_shutdown() {
    ShutdownNCEHook();
}

__attribute__((visibility("default")))
bool nce_hook_execute(void* memory_base, uint32_t addr, uint32_t* next_addr) {
    return TryNCEExecute(memory_base, addr, next_addr);
}

__attribute__((visibility("default")))
bool nce_hook_is_enabled() {
    return g_nce_enabled.load();
}

__attribute__((visibility("default")))
uint64_t nce_hook_get_stats() {
    return g_instructions_jitted.load();
}

// Constructor - runs when library is loaded
__attribute__((constructor))
static void nce_hook_init() {
    LOGI("NCE Hook library loaded");
    // Don't auto-initialize - wait for explicit call
}

// Destructor - runs when library is unloaded
__attribute__((destructor))
static void nce_hook_fini() {
    ShutdownNCEHook();
    LOGI("NCE Hook library unloaded");
}

} // extern "C"
