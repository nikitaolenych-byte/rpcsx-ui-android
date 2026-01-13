// ============================================================================
// NCE - Native Code Execution Engine (Real Implementation)
// ============================================================================
// Справжня NCE - телефон думає що він PS3!
//
// Як це працює:
// 1. Завантажуємо PS3 ELF/SELF бінарник у PS3 memory space
// 2. Запускаємо PPU код НАТИВНО на ARM64 (через JIT)
// 3. Перехоплюємо syscalls і транслюємо на Android
// 4. SPU виконуємо на ARM NEON (6 threads)
// 5. RSX GPU команди транслюємо на Vulkan
//
// Результат: PS3 код працює майже нативно!
// ============================================================================

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

#include "ps3_memory_map.h"
#include "ps3_syscall.h"

#include "ps3_memory_map.h"
#include "ps3_syscall.h"
#include "thread_pool.h"
#include "profiler.h"
#include "game_mode_android.h"

// Forward declarations
namespace rpcsx {
namespace ppu { class PPUInterpreter; class PPUJITCompiler; }
namespace spu { class SPUThreadGroup; class SPUJITCompiler; }
namespace rsx { class RSXEmulator; }
}

namespace rpcsx {
namespace nce {

// ============================================================================
// NCE Version
// ============================================================================
constexpr int NCE_VERSION_MAJOR = 2;
constexpr int NCE_VERSION_MINOR = 0;
constexpr const char* NCE_VERSION_STRING = "2.0.0-native";
// NCE Version (defined in nce_engine.h)
// ============================================================================
// constexpr int NCE_VERSION_MAJOR = 2;
// constexpr int NCE_VERSION_MINOR = 0;
// constexpr const char* NCE_VERSION_STRING = "2.0.0-native";
constexpr const char* NCE_CODENAME = "Native";

// ============================================================================
// NCE Configuration
// ============================================================================
struct NCEConfig {
    // Memory
    bool enable_fastmem = true;          // Use mmap for fast memory access
    bool enable_memory_protection = true; // Use mprotect for write tracking
    
    // JIT
    bool enable_jit = true;              // Enable JIT compilation
    bool enable_block_linking = true;    // Link JIT blocks together
    size_t jit_cache_size = 256 * 1024 * 1024;  // 256MB JIT cache
    
    // SPU
    bool enable_spu = true;              // Enable SPU emulation
    int spu_thread_count = 6;            // Number of SPU threads
    bool spu_use_neon = true;            // Use NEON for SPU SIMD
    
    // RSX
    bool enable_rsx = true;              // Enable RSX emulation
    bool rsx_use_vulkan = true;          // Use Vulkan for RSX
    
    // Debug
    bool enable_logging = false;
    bool enable_syscall_trace = false;
    bool enable_instruction_trace = false;
};

// ============================================================================
// PS3 ELF/SELF Loader
// ============================================================================
struct PS3ModuleInfo {
    std::string name;
    uint64_t base_address;
    size_t size;
    uint64_t entry_point;
    
    // Sections
    struct Section {
        std::string name;
        uint64_t address;
        size_t size;
        uint32_t flags;
    };
    std::vector<Section> sections;
    
    // Exports/Imports
    struct Export {
        uint32_t nid;        // Name ID (hashed function name)
        uint64_t address;
    };
    struct Import {
        uint32_t nid;
        std::string module;
        uint64_t stub_address;
    };
    std::vector<Export> exports;
    std::vector<Import> imports;
};

class PS3ModuleLoader {
public:
    static PS3ModuleLoader& Instance();
    
    // Load SELF/ELF file
    PS3ModuleInfo* LoadModule(const char* path);
    PS3ModuleInfo* LoadModuleFromMemory(const uint8_t* data, size_t size);
    
    // Unload module
    void UnloadModule(PS3ModuleInfo* module);
    
    // Get loaded modules
    std::vector<PS3ModuleInfo*> GetLoadedModules() const;
    
    // Resolve import
    uint64_t ResolveImport(const char* module_name, uint32_t nid);
    
private:
    PS3ModuleLoader() = default;
    
    std::vector<std::unique_ptr<PS3ModuleInfo>> modules_;
    
    // SELF decryption (simplified - real implementation needs keys)
    bool DecryptSELF(const uint8_t* data, size_t size, std::vector<uint8_t>& out);
    
    // ELF loading
    bool LoadELF(const uint8_t* data, size_t size, PS3ModuleInfo& info);
};

// ============================================================================
// PPU Execution Engine (Cell PPU → ARM64)
// ============================================================================
class PPUExecutionEngine {
public:
    PPUExecutionEngine();
    ~PPUExecutionEngine();
    
    // Initialize
    bool Initialize(const NCEConfig& config);
    void Shutdown();
    
    // Create/destroy PPU threads
    uint64_t CreateThread(uint64_t entry, uint64_t arg, int priority, uint64_t stack_size);
    void DestroyThread(uint64_t thread_id);
    
    // Thread control
    void StartThread(uint64_t thread_id);
    void StopThread(uint64_t thread_id);
    void JoinThread(uint64_t thread_id);
    
    // Execute single step (for debugging)
    void Step(uint64_t thread_id);
    
    // Run until syscall or exception
    void Run(uint64_t thread_id);
    
    // Get/set thread context
    ps3::PPUThreadContext* GetThreadContext(uint64_t thread_id);
    
private:
    NCEConfig config_;
<<<<<<< HEAD
    
=======

>>>>>>> c3fa6c4 (build: ARMv9 NCE, thread pool, SIMD, shader cache, UI NCE button)
    struct PPUThread {
        uint64_t id;
        std::thread native_thread;
        std::atomic<bool> running;
        ps3::PPUThreadContext context;
    };
<<<<<<< HEAD
    
    std::vector<std::unique_ptr<PPUThread>> threads_;
    std::atomic<uint64_t> next_thread_id_{1};
    
    // JIT compiler
    void* jit_cache_ = nullptr;
    size_t jit_cache_used_ = 0;
    
    // Compile and execute block
    void* CompileBlock(uint64_t address);
    void ExecuteBlock(PPUThread& thread, void* code);
    
=======

    std::vector<std::unique_ptr<PPUThread>> threads_;
    std::atomic<uint64_t> next_thread_id_{1};

    // JIT code cache (legacy)
    void* jit_cache_ = nullptr;
    size_t jit_cache_used_ = 0;

    // PPU Interpreter (legacy)
    std::unique_ptr<ppu::PPUInterpreter> ppu_interpreter_;

    // PPU JIT Compiler (новий)
    std::unique_ptr<ppu::PPUJITCompiler> ppu_jit_;

    // Compile and execute block
    void* CompileBlock(uint64_t address);
    void ExecuteBlock(PPUThread& thread, void* code);

>>>>>>> c3fa6c4 (build: ARMv9 NCE, thread pool, SIMD, shader cache, UI NCE button)
    // Handle syscall (called from JIT code)
    static void HandleSyscall(PPUThread* thread);
};

// ============================================================================
// SPU Execution Engine (Cell SPU → ARM64 NEON)
// ============================================================================
class SPUExecutionEngine {
public:
    SPUExecutionEngine();
    ~SPUExecutionEngine();
    
    // Initialize
    bool Initialize(const NCEConfig& config);
    void Shutdown();
    
    // SPU Thread Group management
    uint32_t CreateThreadGroup(const char* name, int num_spus, int priority);
    void DestroyThreadGroup(uint32_t group_id);
    void StartThreadGroup(uint32_t group_id);
    void JoinThreadGroup(uint32_t group_id);
    
    // SPU Local Store access
    void WriteLS(int spu_id, uint32_t offset, const void* data, size_t size);
    void ReadLS(int spu_id, uint32_t offset, void* data, size_t size);
    
    // SPU Mailbox
    void WriteSNR(int spu_id, int snr, uint32_t value);
    uint32_t ReadOutMbox(int spu_id);
    
private:
    NCEConfig config_;
<<<<<<< HEAD
    
=======

>>>>>>> c3fa6c4 (build: ARMv9 NCE, thread pool, SIMD, shader cache, UI NCE button)
    struct SPUThread {
        int id;
        std::thread native_thread;
        std::atomic<bool> running;
<<<<<<< HEAD
        
        // Local Store (256KB)
        uint8_t local_store[256 * 1024];
        
        // Registers
        __uint128_t gpr[128];     // 128 x 128-bit registers
        uint32_t pc;
        
=======

        // Local Store (256KB)
        uint8_t local_store[256 * 1024];

        // Registers
        __uint128_t gpr[128];     // 128 x 128-bit registers
        uint32_t pc;

>>>>>>> c3fa6c4 (build: ARMv9 NCE, thread pool, SIMD, shader cache, UI NCE button)
        // Channels
        uint32_t snr[2];          // Signal Notification Registers
        uint32_t out_mbox;
        uint32_t in_mbox;
    };
<<<<<<< HEAD
    
=======

>>>>>>> c3fa6c4 (build: ARMv9 NCE, thread pool, SIMD, shader cache, UI NCE button)
    struct SPUThreadGroup {
        uint32_t id;
        std::string name;
        std::vector<SPUThread> threads;
        std::atomic<bool> running;
    };
<<<<<<< HEAD
    
    std::vector<std::unique_ptr<SPUThreadGroup>> thread_groups_;
    std::atomic<uint32_t> next_group_id_{1};
    
=======

    std::vector<std::unique_ptr<SPUThreadGroup>> thread_groups_;
    std::atomic<uint32_t> next_group_id_{1};

    // SPU Interpreter (real execution with NEON!)
    std::unique_ptr<spu::SPUThreadGroup> spu_thread_group_;

    // SPU JIT Compiler (новий)
    std::unique_ptr<spu::SPUJITCompiler> spu_jit_;

>>>>>>> c3fa6c4 (build: ARMv9 NCE, thread pool, SIMD, shader cache, UI NCE button)
    // SPU instruction execution (using NEON)
    void ExecuteSPUInstruction(SPUThread& spu, uint32_t instr);
};

// ============================================================================
// RSX Execution Engine (RSX GPU → Vulkan)
// ============================================================================
class RSXExecutionEngine {
public:
    RSXExecutionEngine();
    ~RSXExecutionEngine();
    
    // Initialize with Vulkan
    bool Initialize(const NCEConfig& config, void* vulkan_instance, void* vulkan_device);
    void Shutdown();
    
    // Command buffer processing
    void ProcessCommandBuffer(uint64_t address, size_t size);
    
    // Flip (present)
    void Flip();
    
    // Memory mapping
    void* MapVideoMemory(uint64_t ps3_addr, size_t size);
    void UnmapVideoMemory(uint64_t ps3_addr);
    
private:
    NCEConfig config_;
    
    // Vulkan state
    void* vk_instance_ = nullptr;
    void* vk_device_ = nullptr;
    void* vk_queue_ = nullptr;
    void* vk_command_pool_ = nullptr;
    void* vk_command_buffer_ = nullptr;
    
    // RSX command handlers
    void HandleNOP(uint32_t* cmd);
    void HandleJUMP(uint32_t* cmd);
    void HandleCALL(uint32_t* cmd);
    void HandleRETURN(uint32_t* cmd);
    void HandleSURFACE_COLOR_TARGET(uint32_t* cmd);
    void HandleCLEAR_SURFACE(uint32_t* cmd);
    void HandleDRAW_ARRAYS(uint32_t* cmd);
    void HandleDRAW_INDEX_ARRAY(uint32_t* cmd);
    // ... 200+ more RSX commands
};

// ============================================================================
// Main NCE Interface
// ============================================================================
class NativeCodeExecutor {
public:
    static NativeCodeExecutor& Instance();
    
    // Initialize NCE
    bool Initialize(const NCEConfig& config = NCEConfig{});
    void Shutdown();
    
    // Load and run PS3 game
    bool LoadGame(const char* eboot_path);
<<<<<<< HEAD
    void StartGame();
=======
    bool StartGame();
>>>>>>> c3fa6c4 (build: ARMv9 NCE, thread pool, SIMD, shader cache, UI NCE button)
    void StopGame();
    
    // State
    bool IsRunning() const { return running_; }
    
    // Get subsystems
    ps3::PS3MemoryManager& GetMemory();
    ps3::SyscallTranslator& GetSyscalls();
    PPUExecutionEngine& GetPPU();
    SPUExecutionEngine& GetSPU();
    RSXExecutionEngine& GetRSX();
    PS3ModuleLoader& GetLoader();
    
    // Statistics
    struct Stats {
        uint64_t ppu_instructions;
        uint64_t spu_instructions;
        uint64_t rsx_commands;
        uint64_t syscalls;
        double fps;
    };
    Stats GetStats() const;
    
<<<<<<< HEAD
private:
    NativeCodeExecutor() = default;
    ~NativeCodeExecutor() = default;
    
    NCEConfig config_;
    
    std::unique_ptr<PPUExecutionEngine> ppu_;
    std::unique_ptr<SPUExecutionEngine> spu_;
    std::unique_ptr<RSXExecutionEngine> rsx_;
    
    std::atomic<bool> running_{false};
    
=======
public:
    NativeCodeExecutor() = default;
    ~NativeCodeExecutor() = default;

private:

    std::unique_ptr<PPUExecutionEngine> ppu_;
    std::unique_ptr<SPUExecutionEngine> spu_;
    std::unique_ptr<RSXExecutionEngine> rsx_;

    // Thread pool for async tasks (IO, shader compile, etc)
    std::unique_ptr<util::ThreadPool> thread_pool_;

    // Profiler for performance stats
    util::Profiler profiler_;

    // Game Mode (Android performance mode)
    bool game_mode_enabled_ = false;

    std::atomic<bool> running_{false};

>>>>>>> c3fa6c4 (build: ARMv9 NCE, thread pool, SIMD, shader cache, UI NCE button)
    // Main PPU thread ID
    uint64_t main_thread_id_ = 0;
};

// ============================================================================
// C API for JNI
// ============================================================================
extern "C" {
    bool nce_initialize(void);
    void nce_shutdown(void);
    bool nce_load_game(const char* path);
    void nce_start_game(void);
    void nce_stop_game(void);
    bool nce_is_running(void);
}

}  // namespace nce
}  // namespace rpcsx
