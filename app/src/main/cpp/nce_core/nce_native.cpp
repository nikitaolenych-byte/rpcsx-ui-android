// NCE Native Implementation - Real PS3 Execution

#include "nce_native.h"
#include "nce_engine.h"
#include "ps3_memory_map.h"
#include "ps3_syscall.h"
#include "ppu_interpreter.h"
#include "ppu_jit_compiler.h"
#include "spu_interpreter.h"
#include "spu_jit_compiler.h"

#include "rsx_emulator.h"
#include "shader_cache_manager.h"

#include <android/log.h>
#include <sys/mman.h>
#include <sys/auxv.h>
#include <signal.h>
#include <ucontext.h>
#include <cstring>
#include <chrono>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "NCE-Native", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "NCE-Native", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "NCE-Native", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "NCE-Native", __VA_ARGS__)

namespace rpcsx {
namespace nce {

// Byte Swap Utilities (PS3 is Big Endian, ARM is Little Endian)
static inline uint16_t bswap16(uint16_t x) {
    return __builtin_bswap16(x);
}

static inline uint32_t bswap32(uint32_t x) {
    return __builtin_bswap32(x);
}

static inline uint64_t bswap64(uint64_t x) {
    return __builtin_bswap64(x);
}

// PS3 Memory Manager Implementation
namespace ps3 {

PS3MemoryManager& PS3MemoryManager::Instance() {
    static PS3MemoryManager instance;
    return instance;
}

bool PS3MemoryManager::Initialize() {
    if (initialized_) {
        return true;
    }
    
    LOGI("Initializing PS3 Memory Manager...");
    LOGI("  Main Memory: %zu MB at 0x%08llx", 
         MAIN_MEMORY_SIZE / (1024*1024), 
         static_cast<unsigned long long>(MAIN_MEMORY_BASE));
    LOGI("  Video Memory: %zu MB at 0x%08llx",
         VIDEO_MEMORY_SIZE / (1024*1024),
         static_cast<unsigned long long>(VIDEO_MEMORY_BASE));
    
    // Allocate main memory (256MB)
    main_memory_ = mmap(nullptr, MAIN_MEMORY_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (main_memory_ == MAP_FAILED) {
        LOGE("Failed to allocate main memory");
        return false;
    }
    memset(main_memory_, 0, MAIN_MEMORY_SIZE);
    LOGI("  Main memory allocated at %p", main_memory_);
    
    // Allocate video memory (256MB)
    video_memory_ = mmap(nullptr, VIDEO_MEMORY_SIZE,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (video_memory_ == MAP_FAILED) {
        LOGE("Failed to allocate video memory");
        munmap(main_memory_, MAIN_MEMORY_SIZE);
        return false;
    }
    memset(video_memory_, 0, VIDEO_MEMORY_SIZE);
    LOGI("  Video memory allocated at %p", video_memory_);
    
    // Allocate SPU Local Stores (256KB each)
    for (int i = 0; i < SPU_COUNT; i++) {
        spu_ls_[i] = mmap(nullptr, SPU_LS_SIZE,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (spu_ls_[i] == MAP_FAILED) {
            LOGE("Failed to allocate SPU%d local store", i);
            // Cleanup
            for (int j = 0; j < i; j++) {
                munmap(spu_ls_[j], SPU_LS_SIZE);
            }
            munmap(video_memory_, VIDEO_MEMORY_SIZE);
            munmap(main_memory_, MAIN_MEMORY_SIZE);
            return false;
        }
        memset(spu_ls_[i], 0, SPU_LS_SIZE);
    }
    LOGI("  SPU Local Stores allocated (%d x %zu KB)", SPU_COUNT, SPU_LS_SIZE/1024);
    
    // Allocate MMIO region
    mmio_region_ = mmap(nullptr, MMIO_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mmio_region_ == MAP_FAILED) {
        LOGE("Failed to allocate MMIO region");
        return false;
    }
    LOGI("  MMIO region allocated at %p", mmio_region_);
    
    // Allocate RSX command buffer
    rsx_cmd_buffer_ = mmap(nullptr, RSX_CMD_SIZE,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (rsx_cmd_buffer_ == MAP_FAILED) {
        LOGE("Failed to allocate RSX command buffer");
        return false;
    }
    LOGI("  RSX command buffer allocated at %p", rsx_cmd_buffer_);
    
    initialized_ = true;
    LOGI("PS3 Memory Manager initialized successfully!");
    LOGI("  Total allocated: %zu MB", 
         (MAIN_MEMORY_SIZE + VIDEO_MEMORY_SIZE + SPU_COUNT*SPU_LS_SIZE + 
          MMIO_SIZE + RSX_CMD_SIZE) / (1024*1024));
    
    return true;
}

void PS3MemoryManager::Shutdown() {
    if (!initialized_) {
        return;
    }
    
    LOGI("Shutting down PS3 Memory Manager...");
    
    if (rsx_cmd_buffer_) {
        munmap(rsx_cmd_buffer_, RSX_CMD_SIZE);
        rsx_cmd_buffer_ = nullptr;
    }
    
    if (mmio_region_) {
        munmap(mmio_region_, MMIO_SIZE);
        mmio_region_ = nullptr;
    }
    
    for (int i = 0; i < SPU_COUNT; i++) {
        if (spu_ls_[i]) {
            munmap(spu_ls_[i], SPU_LS_SIZE);
            spu_ls_[i] = nullptr;
        }
    }
    
    if (video_memory_) {
        munmap(video_memory_, VIDEO_MEMORY_SIZE);
        video_memory_ = nullptr;
    }
    
    if (main_memory_) {
        munmap(main_memory_, MAIN_MEMORY_SIZE);
        main_memory_ = nullptr;
    }
    
    initialized_ = false;
    LOGI("PS3 Memory Manager shutdown complete");
}

void* PS3MemoryManager::PS3ToHost(uint64_t ps3_addr) const {
    // Main Memory: 0x00000000 - 0x10000000
    if (ps3_addr < MAIN_MEMORY_SIZE) {
        return static_cast<uint8_t*>(main_memory_) + ps3_addr;
    }
    
    // RSX Command Buffer: 0x40000000 - 0x41000000
    if (ps3_addr >= RSX_CMD_BASE && ps3_addr < RSX_CMD_BASE + RSX_CMD_SIZE) {
        return static_cast<uint8_t*>(rsx_cmd_buffer_) + (ps3_addr - RSX_CMD_BASE);
    }
    
    // Video Memory: 0xC0000000 - 0xD0000000
    if (ps3_addr >= VIDEO_MEMORY_BASE && ps3_addr < VIDEO_MEMORY_BASE + VIDEO_MEMORY_SIZE) {
        return static_cast<uint8_t*>(video_memory_) + (ps3_addr - VIDEO_MEMORY_BASE);
    }
    
    // SPU Local Store: 0xE0000000 + (spu_id * 0x100000)
    if (ps3_addr >= SPU_LS_BASE && ps3_addr < SPU_LS_BASE + SPU_COUNT * 0x100000) {
        int spu_id = (ps3_addr - SPU_LS_BASE) / 0x100000;
        uint32_t offset = (ps3_addr - SPU_LS_BASE) % 0x100000;
        if (offset < SPU_LS_SIZE) {
            return static_cast<uint8_t*>(spu_ls_[spu_id]) + offset;
        }
    }
    
    // MMIO: 0xF0000000 - 0xF1000000
    if (ps3_addr >= MMIO_BASE && ps3_addr < MMIO_BASE + MMIO_SIZE) {
        return static_cast<uint8_t*>(mmio_region_) + (ps3_addr - MMIO_BASE);
    }
    
    LOGW("Invalid PS3 address: 0x%llx", static_cast<unsigned long long>(ps3_addr));
    return nullptr;
}

// Memory read/write implementations
uint8_t PS3MemoryManager::Read8(uint64_t ps3_addr) const {
    void* host = PS3ToHost(ps3_addr);
    return host ? *static_cast<uint8_t*>(host) : 0;
}

uint16_t PS3MemoryManager::Read16(uint64_t ps3_addr) const {
    void* host = PS3ToHost(ps3_addr);
    return host ? *static_cast<uint16_t*>(host) : 0;
}

uint32_t PS3MemoryManager::Read32(uint64_t ps3_addr) const {
    void* host = PS3ToHost(ps3_addr);
    return host ? *static_cast<uint32_t*>(host) : 0;
}

uint64_t PS3MemoryManager::Read64(uint64_t ps3_addr) const {
    void* host = PS3ToHost(ps3_addr);
    return host ? *static_cast<uint64_t*>(host) : 0;
}

void PS3MemoryManager::Write8(uint64_t ps3_addr, uint8_t value) {
    void* host = PS3ToHost(ps3_addr);
    if (host) *static_cast<uint8_t*>(host) = value;
}

void PS3MemoryManager::Write16(uint64_t ps3_addr, uint16_t value) {
    void* host = PS3ToHost(ps3_addr);
    if (host) *static_cast<uint16_t*>(host) = value;
}

void PS3MemoryManager::Write32(uint64_t ps3_addr, uint32_t value) {
    void* host = PS3ToHost(ps3_addr);
    if (host) *static_cast<uint32_t*>(host) = value;
}

void PS3MemoryManager::Write64(uint64_t ps3_addr, uint64_t value) {
    void* host = PS3ToHost(ps3_addr);
    if (host) *static_cast<uint64_t*>(host) = value;
}

// Big-endian versions (PS3 native format)
uint16_t PS3MemoryManager::Read16BE(uint64_t ps3_addr) const {
    return bswap16(Read16(ps3_addr));
}

uint32_t PS3MemoryManager::Read32BE(uint64_t ps3_addr) const {
    return bswap32(Read32(ps3_addr));
}

uint64_t PS3MemoryManager::Read64BE(uint64_t ps3_addr) const {
    return bswap64(Read64(ps3_addr));
}

void PS3MemoryManager::Write16BE(uint64_t ps3_addr, uint16_t value) {
    Write16(ps3_addr, bswap16(value));
}

void PS3MemoryManager::Write32BE(uint64_t ps3_addr, uint32_t value) {
    Write32(ps3_addr, bswap32(value));
}

void PS3MemoryManager::Write64BE(uint64_t ps3_addr, uint64_t value) {
    Write64(ps3_addr, bswap64(value));
}

void* PS3MemoryManager::AllocateMainMemory(uint64_t ps3_addr, size_t size, uint32_t flags) {
    // Check bounds
    if (ps3_addr + size > MAIN_MEMORY_SIZE) {
        LOGE("Main memory allocation out of bounds: 0x%llx + %zu",
             static_cast<unsigned long long>(ps3_addr), size);
        return nullptr;
    }
    
    main_memory_used_ += size;
    return PS3ToHost(ps3_addr);
}

void* PS3MemoryManager::AllocateVideoMemory(uint64_t ps3_addr, size_t size) {
    uint64_t offset = ps3_addr - VIDEO_MEMORY_BASE;
    if (offset + size > VIDEO_MEMORY_SIZE) {
        LOGE("Video memory allocation out of bounds");
        return nullptr;
    }
    
    video_memory_used_ += size;
    return PS3ToHost(ps3_addr);
}

void* PS3MemoryManager::AllocateSPULocalStore(int spu_id) {
    if (spu_id < 0 || spu_id >= SPU_COUNT) {
        return nullptr;
    }
    return spu_ls_[spu_id];
}

size_t PS3MemoryManager::GetFreeMainMemory() const {
    return MAIN_MEMORY_SIZE - main_memory_used_;
}

size_t PS3MemoryManager::GetFreeVideoMemory() const {
    return VIDEO_MEMORY_SIZE - video_memory_used_;
}

// Syscall Translator Implementation

SyscallTranslator& SyscallTranslator::Instance() {
    static SyscallTranslator instance;
    return instance;
}

bool SyscallTranslator::Initialize() {
    if (initialized_) {
        return true;
    }
    
    LOGI("Initializing PS3 Syscall Translator...");
    
    RegisterBuiltinHandlers();
    
    initialized_ = true;
    LOGI("PS3 Syscall Translator initialized with %zu handlers", handlers_.size());
    
    return true;
}

void SyscallTranslator::Shutdown() {
    handlers_.clear();
    syscall_names_.clear();
    initialized_ = false;
}

void SyscallTranslator::RegisterBuiltinHandlers() {
    // Process
    handlers_[syscall::SYS_PROCESS_EXIT] = [this](PPUThreadContext& ctx) {
        return sys_process_exit(ctx);
    };
    syscall_names_[syscall::SYS_PROCESS_EXIT] = "sys_process_exit";
    
    handlers_[syscall::SYS_PROCESS_GETPID] = [this](PPUThreadContext& ctx) {
        return sys_process_getpid(ctx);
    };
    syscall_names_[syscall::SYS_PROCESS_GETPID] = "sys_process_getpid";
    
    // Thread
    handlers_[syscall::SYS_PPU_THREAD_CREATE] = [this](PPUThreadContext& ctx) {
        return sys_ppu_thread_create(ctx);
    };
    syscall_names_[syscall::SYS_PPU_THREAD_CREATE] = "sys_ppu_thread_create";
    
    handlers_[syscall::SYS_PPU_THREAD_EXIT] = [this](PPUThreadContext& ctx) {
        return sys_ppu_thread_exit(ctx);
    };
    syscall_names_[syscall::SYS_PPU_THREAD_EXIT] = "sys_ppu_thread_exit";
    
    handlers_[syscall::SYS_PPU_THREAD_YIELD] = [this](PPUThreadContext& ctx) {
        return sys_ppu_thread_yield(ctx);
    };
    syscall_names_[syscall::SYS_PPU_THREAD_YIELD] = "sys_ppu_thread_yield";
    
    // Memory
    handlers_[syscall::SYS_MEMORY_ALLOCATE] = [this](PPUThreadContext& ctx) {
        return sys_memory_allocate(ctx);
    };
    syscall_names_[syscall::SYS_MEMORY_ALLOCATE] = "sys_memory_allocate";
    
    handlers_[syscall::SYS_MEMORY_FREE] = [this](PPUThreadContext& ctx) {
        return sys_memory_free(ctx);
    };
    syscall_names_[syscall::SYS_MEMORY_FREE] = "sys_memory_free";
    
    // Mutex
    handlers_[syscall::SYS_MUTEX_CREATE] = [this](PPUThreadContext& ctx) {
        return sys_mutex_create(ctx);
    };
    syscall_names_[syscall::SYS_MUTEX_CREATE] = "sys_mutex_create";
    
    handlers_[syscall::SYS_MUTEX_LOCK] = [this](PPUThreadContext& ctx) {
        return sys_mutex_lock(ctx);
    };
    syscall_names_[syscall::SYS_MUTEX_LOCK] = "sys_mutex_lock";
    
    handlers_[syscall::SYS_MUTEX_UNLOCK] = [this](PPUThreadContext& ctx) {
        return sys_mutex_unlock(ctx);
    };
    syscall_names_[syscall::SYS_MUTEX_UNLOCK] = "sys_mutex_unlock";
    
    // Timer
    handlers_[syscall::SYS_TIME_GET_CURRENT_TIME] = [this](PPUThreadContext& ctx) {
        return sys_time_get_current_time(ctx);
    };
    syscall_names_[syscall::SYS_TIME_GET_CURRENT_TIME] = "sys_time_get_current_time";
    
    handlers_[syscall::SYS_TIME_GET_TIMEBASE_FREQ] = [this](PPUThreadContext& ctx) {
        return sys_time_get_timebase_freq(ctx);
    };
    syscall_names_[syscall::SYS_TIME_GET_TIMEBASE_FREQ] = "sys_time_get_timebase_freq";
    
    // File System
    handlers_[syscall::SYS_FS_OPEN] = [this](PPUThreadContext& ctx) {
        return sys_fs_open(ctx);
    };
    syscall_names_[syscall::SYS_FS_OPEN] = "sys_fs_open";
    
    handlers_[syscall::SYS_FS_READ] = [this](PPUThreadContext& ctx) {
        return sys_fs_read(ctx);
    };
    syscall_names_[syscall::SYS_FS_READ] = "sys_fs_read";
    
    handlers_[syscall::SYS_FS_WRITE] = [this](PPUThreadContext& ctx) {
        return sys_fs_write(ctx);
    };
    syscall_names_[syscall::SYS_FS_WRITE] = "sys_fs_write";
    
    handlers_[syscall::SYS_FS_CLOSE] = [this](PPUThreadContext& ctx) {
        return sys_fs_close(ctx);
    };
    syscall_names_[syscall::SYS_FS_CLOSE] = "sys_fs_close";
    
    // PRX
    handlers_[syscall::SYS_PRX_LOAD_MODULE] = [this](PPUThreadContext& ctx) {
        return sys_prx_load_module(ctx);
    };
    syscall_names_[syscall::SYS_PRX_LOAD_MODULE] = "sys_prx_load_module";
    
    handlers_[syscall::SYS_PRX_START_MODULE] = [this](PPUThreadContext& ctx) {
        return sys_prx_start_module(ctx);
    };
    syscall_names_[syscall::SYS_PRX_START_MODULE] = "sys_prx_start_module";
    
    LOGI("Registered %zu syscall handlers", handlers_.size());
}

int64_t SyscallTranslator::HandleSyscall(int syscall_num, PPUThreadContext& ctx) {
    stats_.total_syscalls++;
    stats_.syscall_counts[syscall_num]++;
    
    auto it = handlers_.find(syscall_num);
    if (it != handlers_.end()) {
        return it->second(ctx);
    }
    
    stats_.unimplemented_syscalls++;
    LOGW("Unimplemented syscall: %d (0x%x)", syscall_num, syscall_num);
    return error::CELL_ENOSYS;
}

const char* SyscallTranslator::GetSyscallName(int syscall_num) const {
    auto it = syscall_names_.find(syscall_num);
    return it != syscall_names_.end() ? it->second.c_str() : "unknown";
}

// Syscall implementations
int64_t SyscallTranslator::sys_process_exit(PPUThreadContext& ctx) {
    int status = ctx.gpr[3];
    LOGI("sys_process_exit(%d)", status);
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_process_getpid(PPUThreadContext& ctx) {
    ctx.gpr[3] = 1;  // Return PID 1
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_ppu_thread_create(PPUThreadContext& ctx) {
    // uint64_t* thread_id = ctx.gpr[3]
    // uint64_t entry = ctx.gpr[4]
    // uint64_t arg = ctx.gpr[5]
    // int prio = ctx.gpr[6]
    // uint64_t stack_size = ctx.gpr[7]
    LOGI("sys_ppu_thread_create(entry=0x%llx)", 
         static_cast<unsigned long long>(ctx.gpr[4]));
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_ppu_thread_exit(PPUThreadContext& ctx) {
    LOGI("sys_ppu_thread_exit(%lld)", static_cast<long long>(ctx.gpr[3]));
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_ppu_thread_yield(PPUThreadContext& ctx) {
    std::this_thread::yield();
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_memory_allocate(PPUThreadContext& ctx) {
    size_t size = ctx.gpr[3];
    uint64_t flags = ctx.gpr[4];
    // uint64_t* addr = ctx.gpr[5]
    
    LOGI("sys_memory_allocate(size=%zu, flags=0x%llx)", 
         size, static_cast<unsigned long long>(flags));
    
    // Allocate from main memory
    // For now, just return a fixed address
    static uint64_t next_addr = 0x10000000;
    uint64_t addr = next_addr;
    next_addr += (size + 0xFFF) & ~0xFFF;  // Page align
    
    // Write address back
    PS3MemoryManager::Instance().Write64(ctx.gpr[5], addr);
    
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_memory_free(PPUThreadContext& ctx) {
    uint64_t addr = ctx.gpr[3];
    LOGI("sys_memory_free(addr=0x%llx)", static_cast<unsigned long long>(addr));
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_mutex_create(PPUThreadContext& ctx) {
    LOGI("sys_mutex_create()");
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_mutex_lock(PPUThreadContext& ctx) {
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_mutex_unlock(PPUThreadContext& ctx) {
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_time_get_current_time(PPUThreadContext& ctx) {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch);
    
    ctx.gpr[3] = seconds.count();
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_time_get_timebase_freq(PPUThreadContext& ctx) {
    // PS3 timebase frequency is 79.8 MHz
    ctx.gpr[3] = 79800000;
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_fs_open(PPUThreadContext& ctx) {
    LOGI("sys_fs_open()");
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_fs_read(PPUThreadContext& ctx) {
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_fs_write(PPUThreadContext& ctx) {
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_fs_close(PPUThreadContext& ctx) {
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_prx_load_module(PPUThreadContext& ctx) {
    LOGI("sys_prx_load_module()");
    return error::CELL_OK;
}

int64_t SyscallTranslator::sys_prx_start_module(PPUThreadContext& ctx) {
    LOGI("sys_prx_start_module()");
    return error::CELL_OK;
}

}  // namespace ps3

// NativeCodeExecutor Implementation

NativeCodeExecutor& NativeCodeExecutor::Instance() {
    static NativeCodeExecutor instance;
    return instance;
}

bool NativeCodeExecutor::Initialize(const NCEConfig& config) {
    if (running_) {
        return true;
    }
    
    config_ = config;
    
    LOGI("╔══════════════════════════════════════════════════════════════╗");
    LOGI("║          NCE - Native Code Execution Engine v%s          ║", NCE_VERSION_STRING);
    LOGI("║                Codename: %s                              ║", NCE_CODENAME);
    LOGI("║          Your phone IS now a PlayStation 3!              ║");
    LOGI("╚══════════════════════════════════════════════════════════════╝");
    
    // Initialize PS3 memory
    if (!ps3::PS3MemoryManager::Instance().Initialize()) {
        LOGE("Failed to initialize PS3 memory");
        return false;
    }
    
    // Initialize syscall translator
    if (!ps3::SyscallTranslator::Instance().Initialize()) {
        LOGE("Failed to initialize syscall translator");
        return false;
    }
    
    // Thread pool: 4 потоки (можна зробити динамічно)
    thread_pool_ = std::make_unique<util::ThreadPool>(4);
    // Передаємо thread pool у shader cache
    rpcsx::shaders::SetThreadPool(thread_pool_.get());

    // Game Mode (Android)
    game_mode_enabled_ = true;
    android::EnableGameMode();

    // Initialize PPU engine
    ppu_ = std::make_unique<PPUExecutionEngine>();
    if (!ppu_->Initialize(config)) {
        LOGE("Failed to initialize PPU engine");
        return false;
    }

    // Initialize SPU engine
    if (config.enable_spu) {
        spu_ = std::make_unique<SPUExecutionEngine>();
        if (!spu_->Initialize(config)) {
            LOGE("Failed to initialize SPU engine");
            return false;
        }
    }

    LOGI("NCE initialized successfully! (GameMode=%d)", (int)game_mode_enabled_);
    return true;
}

void NativeCodeExecutor::Shutdown() {
    if (running_) {
        StopGame();
    }

    // Thread pool
    thread_pool_.reset();

    // Game Mode
    if (game_mode_enabled_) {
        android::DisableGameMode();
        game_mode_enabled_ = false;
    }

    rsx_.reset();
    spu_.reset();
    ppu_.reset();

    ps3::SyscallTranslator::Instance().Shutdown();
    ps3::PS3MemoryManager::Instance().Shutdown();

    LOGI("NCE shutdown complete");
}

bool NativeCodeExecutor::LoadGame(const char* eboot_path) {
    LOGI("Loading game: %s", eboot_path);
    
    PS3ModuleInfo* module = PS3ModuleLoader::Instance().LoadModule(eboot_path);
    if (!module) {
        LOGE("Failed to load game");
        return false;
    }
    
    LOGI("Game loaded:");
    LOGI("  Name: %s", module->name.c_str());
    LOGI("  Base: 0x%llx", static_cast<unsigned long long>(module->base_address));
    LOGI("  Entry: 0x%llx", static_cast<unsigned long long>(module->entry_point));
    
    // Create main PPU thread
    main_thread_id_ = ppu_->CreateThread(
        module->entry_point,
        0,  // arg
        0,  // priority
        1024 * 1024  // 1MB stack
    );
    
    return true;
}

bool NativeCodeExecutor::StartGame() {
    if (running_) {
        return false; // Already running
    }
    
    LOGI("Starting game execution...");
    running_ = true;
    
    // Start main PPU thread
    ppu_->StartThread(main_thread_id_);
    return true;
}

void NativeCodeExecutor::StopGame() {
    if (!running_) {
        return;
    }
    
    LOGI("Stopping game...");
    running_ = false;
    
    ppu_->StopThread(main_thread_id_);
}

ps3::PS3MemoryManager& NativeCodeExecutor::GetMemory() {
    return ps3::PS3MemoryManager::Instance();
}

ps3::SyscallTranslator& NativeCodeExecutor::GetSyscalls() {
    return ps3::SyscallTranslator::Instance();
}

PPUExecutionEngine& NativeCodeExecutor::GetPPU() {
    return *ppu_;
}

SPUExecutionEngine& NativeCodeExecutor::GetSPU() {
    return *spu_;
}

PS3ModuleLoader& NativeCodeExecutor::GetLoader() {
    return PS3ModuleLoader::Instance();
}

// PPU Execution Engine

PPUExecutionEngine::PPUExecutionEngine() {
    // Defer heavy allocations to Initialize()
}
PPUExecutionEngine::~PPUExecutionEngine() { Shutdown(); }

bool PPUExecutionEngine::Initialize(const NCEConfig& config) {
    config_ = config;
    
    // Initialize PPU Interpreter
    if (!ppu_interpreter_) ppu_interpreter_ = std::make_unique<ppu::PPUInterpreter>();
    ppu_interpreter_->Initialize(ps3::PS3MemoryManager::Instance().GetMainMemory(),
                                  ps3::MAIN_MEMORY_SIZE);

    // Initialize PPU JIT
    if (config.enable_jit) {
        if (!ppu_jit_) ppu_jit_ = std::make_unique<ppu::PPUJITCompiler>();
        ppu_jit_->Initialize(config.jit_cache_size);
        LOGI("PPU JIT enabled");
    }

    LOGI("PPU Execution Engine initialized (JIT=%d)", (int)config.enable_jit);
    return true;
}

void PPUExecutionEngine::Shutdown() {
    // Stop all threads
    for (auto& thread : threads_) {
        if (thread->running) {
            thread->running = false;
            if (thread->native_thread.joinable()) {
                thread->native_thread.join();
            }
        }
    }
    threads_.clear();
    
    // Shutdown PPU interpreter
    if (ppu_interpreter_) {
        ppu_interpreter_->Shutdown();
    }
    
    // Free JIT cache
    if (jit_cache_) {
        munmap(jit_cache_, config_.jit_cache_size);
        jit_cache_ = nullptr;
    }
}

uint64_t PPUExecutionEngine::CreateThread(uint64_t entry, uint64_t arg, int priority, uint64_t stack_size) {
    auto thread = std::make_unique<PPUThread>();
    thread->id = next_thread_id_++;
    thread->running = false;
    
    // Initialize context
    memset(&thread->context, 0, sizeof(thread->context));
    thread->context.pc = entry;
    thread->context.gpr[3] = arg;
    thread->context.thread_id = thread->id;
    thread->context.priority = priority;
    thread->context.stack_size = stack_size;
    
    // Allocate stack
    uint64_t stack_addr = ps3::STACK_BASE - (thread->id * stack_size);
    thread->context.stack_addr = stack_addr;
    thread->context.gpr[1] = stack_addr + stack_size - 32;  // SP
    
    LOGI("Created PPU thread %llu: entry=0x%llx, stack=0x%llx",
         static_cast<unsigned long long>(thread->id),
         static_cast<unsigned long long>(entry),
         static_cast<unsigned long long>(thread->context.gpr[1]));
    
    uint64_t id = thread->id;
    threads_.push_back(std::move(thread));
    return id;
}

void PPUExecutionEngine::StartThread(uint64_t thread_id) {
    for (auto& thread : threads_) {
        if (thread->id == thread_id) {
            thread->running = true;
            thread->native_thread = std::thread([this, &thread]() {
                LOGI("PPU thread %llu started", static_cast<unsigned long long>(thread->id));
                
                while (thread->running) {
                    Run(thread->id);
                }
                
                LOGI("PPU thread %llu exited", static_cast<unsigned long long>(thread->id));
            });
            return;
        }
    }
}

void PPUExecutionEngine::StopThread(uint64_t thread_id) {
    for (auto& thread : threads_) {
        if (thread->id == thread_id) {
            thread->running = false;
            if (thread->native_thread.joinable()) {
                thread->native_thread.join();
            }
            return;
        }
    }
}

void PPUExecutionEngine::Run(uint64_t thread_id) {
    for (auto& thread : threads_) {
        if (thread->id == thread_id) {
            if (config_.enable_jit && ppu_jit_) {
                // JIT: компілюємо та виконуємо блок
                void* code = ppu_jit_->CompileBlock(thread->context.pc);
                if (code) {
                    thread->context.pc = ppu_jit_->ExecuteBlock(code, thread->context.pc, thread->context.gpr, thread->context.fpr);
                } else {
                    // Fallback: інтерпретатор
                    if (ppu_interpreter_) {
                        ppu::PPUState state;
                        memcpy(state.gpr, thread->context.gpr, sizeof(state.gpr));
                        memcpy(state.fpr, thread->context.fpr, sizeof(state.fpr));
                        state.pc = thread->context.pc;
                        state.lr = thread->context.lr;
                        state.ctr = thread->context.ctr;
                        state.cr = thread->context.cr;
                        state.xer = thread->context.xer;
                        constexpr uint64_t BATCH_SIZE = 10000;
                        ppu_interpreter_->Execute(state, BATCH_SIZE);
                        memcpy(thread->context.gpr, state.gpr, sizeof(state.gpr));
                        memcpy(thread->context.fpr, state.fpr, sizeof(state.fpr));
                        thread->context.pc = state.pc;
                        thread->context.lr = state.lr;
                        thread->context.ctr = state.ctr;
                        thread->context.cr = state.cr;
                        thread->context.xer = state.xer;
                    }
                }
            } else if (ppu_interpreter_) {
                // Інтерпретатор
                ppu::PPUState state;
                memcpy(state.gpr, thread->context.gpr, sizeof(state.gpr));
                memcpy(state.fpr, thread->context.fpr, sizeof(state.fpr));
                state.pc = thread->context.pc;
                state.lr = thread->context.lr;
                state.ctr = thread->context.ctr;
                state.cr = thread->context.cr;
                state.xer = thread->context.xer;
                constexpr uint64_t BATCH_SIZE = 10000;
                ppu_interpreter_->Execute(state, BATCH_SIZE);
                memcpy(thread->context.gpr, state.gpr, sizeof(state.gpr));
                memcpy(thread->context.fpr, state.fpr, sizeof(state.fpr));
                thread->context.pc = state.pc;
                thread->context.lr = state.lr;
                thread->context.ctr = state.ctr;
                thread->context.cr = state.cr;
                thread->context.xer = state.xer;
            }
            return;
        }
    }
}

// SPU Execution Engine

SPUExecutionEngine::SPUExecutionEngine() {
    // Defer allocations to Initialize()
}
SPUExecutionEngine::~SPUExecutionEngine() { Shutdown(); }

bool SPUExecutionEngine::Initialize(const NCEConfig& config) {
    config_ = config;
    
    // Initialize SPU thread group with main memory access
    if (!spu_thread_group_) spu_thread_group_ = std::make_unique<spu::SPUThreadGroup>();
    spu_thread_group_->Initialize(
        ps3::PS3MemoryManager::Instance().GetMainMemory(),
        ps3::MAIN_MEMORY_SIZE
    );

    // Initialize SPU JIT
    if (!spu_jit_) spu_jit_ = std::make_unique<spu::SPUJITCompiler>();
    if (spu_jit_) {
        spu_jit_->Initialize(64 * 1024 * 1024);
        LOGI("SPU JIT enabled");
    }

    LOGI("SPU Engine initialized: %d SPUs with NEON SIMD, JIT=%d", config.spu_thread_count, spu_jit_ ? 1 : 0);
    return true;
}

void SPUExecutionEngine::Shutdown() {
    if (spu_thread_group_) {
        spu_thread_group_->Shutdown();
    }
    thread_groups_.clear();
}

uint32_t SPUExecutionEngine::CreateThreadGroup(const char* name, int num_spus, int priority) {
    auto group = std::make_unique<SPUThreadGroup>();
    group->id = next_group_id_++;
    group->name = name;
    group->running = false;
    
    for (int i = 0; i < num_spus && i < config_.spu_thread_count; i++) {
        SPUThread spu;
        spu.id = i;
        spu.running = false;
        spu.pc = 0;
        memset(spu.local_store, 0, sizeof(spu.local_store));
        memset(spu.gpr, 0, sizeof(spu.gpr));
        group->threads.push_back(std::move(spu));
    }
    
    uint32_t id = group->id;
    thread_groups_.push_back(std::move(group));
    
    LOGI("Created SPU thread group '%s' with %d SPUs", name, num_spus);
    return id;
}

// Module Loader

PS3ModuleLoader& PS3ModuleLoader::Instance() {
    static PS3ModuleLoader instance;
    return instance;
}

PS3ModuleInfo* PS3ModuleLoader::LoadModule(const char* path) {
    LOGI("Loading module: %s", path);
    
    // For now, create a stub module
    auto module = std::make_unique<PS3ModuleInfo>();
    module->name = path;
    module->base_address = 0x10000;
    module->entry_point = 0x10000;
    module->size = 0;
    
    PS3ModuleInfo* ptr = module.get();
    modules_.push_back(std::move(module));
    
    return ptr;
}

// C API

extern "C" {

bool nce_initialize(void) {
    return NativeCodeExecutor::Instance().Initialize();
}

void nce_shutdown(void) {
    NativeCodeExecutor::Instance().Shutdown();
}

bool nce_load_game(const char* path) {
    return NativeCodeExecutor::Instance().LoadGame(path);
}

void nce_start_game(void) {
    NativeCodeExecutor::Instance().StartGame();
}

void nce_stop_game(void) {
    NativeCodeExecutor::Instance().StopGame();
}

bool nce_is_running(void) {
    return NativeCodeExecutor::Instance().IsRunning();
}

}

}  // namespace nce
}  // namespace rpcsx
