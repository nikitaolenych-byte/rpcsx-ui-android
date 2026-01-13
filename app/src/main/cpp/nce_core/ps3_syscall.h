// ============================================================================
// PS3 LV2 System Call Translation Layer
// ============================================================================
// Перехоплюємо syscalls PS3 і транслюємо на Linux/Android
// Телефон виконує код PS3 нативно, а ми тільки обробляємо syscalls!
// ============================================================================

#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <string>

namespace rpcsx {
namespace nce {
namespace ps3 {

// ============================================================================
// PS3 LV2 System Call Numbers
// ============================================================================
namespace syscall {

// Process Management
constexpr int SYS_PROCESS_EXIT              = 1;
constexpr int SYS_PROCESS_GETPID            = 2;
constexpr int SYS_PROCESS_GETPPID           = 3;
constexpr int SYS_PROCESS_GET_SDK_VERSION   = 4;
constexpr int SYS_PROCESS_SPAWN             = 5;
constexpr int SYS_PROCESS_KILL              = 6;
constexpr int SYS_PROCESS_WAIT              = 7;

// Thread Management (PPU)
constexpr int SYS_PPU_THREAD_CREATE         = 10;
constexpr int SYS_PPU_THREAD_EXIT           = 11;
constexpr int SYS_PPU_THREAD_JOIN           = 12;
constexpr int SYS_PPU_THREAD_DETACH         = 13;
constexpr int SYS_PPU_THREAD_GET_JOIN_STATE = 14;
constexpr int SYS_PPU_THREAD_SET_PRIORITY   = 15;
constexpr int SYS_PPU_THREAD_GET_PRIORITY   = 16;
constexpr int SYS_PPU_THREAD_GET_STACK_INFO = 17;
constexpr int SYS_PPU_THREAD_RENAME         = 18;
constexpr int SYS_PPU_THREAD_YIELD          = 43;

// SPU Thread Management
constexpr int SYS_SPU_THREAD_GROUP_CREATE   = 170;
constexpr int SYS_SPU_THREAD_GROUP_DESTROY  = 171;
constexpr int SYS_SPU_THREAD_GROUP_START    = 172;
constexpr int SYS_SPU_THREAD_GROUP_SUSPEND  = 173;
constexpr int SYS_SPU_THREAD_GROUP_RESUME   = 174;
constexpr int SYS_SPU_THREAD_GROUP_YIELD    = 175;
constexpr int SYS_SPU_THREAD_GROUP_JOIN     = 178;
constexpr int SYS_SPU_THREAD_WRITE_LS       = 181;
constexpr int SYS_SPU_THREAD_READ_LS        = 182;
constexpr int SYS_SPU_THREAD_WRITE_SNR      = 184;

// Memory Management
constexpr int SYS_MEMORY_ALLOCATE           = 348;
constexpr int SYS_MEMORY_FREE               = 349;
constexpr int SYS_MEMORY_ALLOCATE_FROM_CONT = 350;
constexpr int SYS_MEMORY_GET_PAGE_ATTR      = 352;
constexpr int SYS_MEMORY_SET_PAGE_ATTR      = 353;
constexpr int SYS_MEMORY_CONTAINER_CREATE   = 324;
constexpr int SYS_MEMORY_CONTAINER_DESTROY  = 325;
constexpr int SYS_MEMORY_CONTAINER_GET_SIZE = 326;
constexpr int SYS_MMAPPER_ALLOCATE_ADDRESS  = 330;
constexpr int SYS_MMAPPER_FREE_ADDRESS      = 331;
constexpr int SYS_MMAPPER_ALLOCATE_SHARED_MEM = 332;
constexpr int SYS_MMAPPER_MAP_SHARED_MEM    = 333;
constexpr int SYS_MMAPPER_UNMAP_SHARED_MEM  = 334;

// Mutex
constexpr int SYS_MUTEX_CREATE              = 100;
constexpr int SYS_MUTEX_DESTROY             = 101;
constexpr int SYS_MUTEX_LOCK                = 102;
constexpr int SYS_MUTEX_TRYLOCK             = 103;
constexpr int SYS_MUTEX_UNLOCK              = 104;

// Condition Variable
constexpr int SYS_COND_CREATE               = 105;
constexpr int SYS_COND_DESTROY              = 106;
constexpr int SYS_COND_WAIT                 = 107;
constexpr int SYS_COND_SIGNAL               = 108;
constexpr int SYS_COND_SIGNAL_ALL           = 109;

// Semaphore
constexpr int SYS_SEMAPHORE_CREATE          = 90;
constexpr int SYS_SEMAPHORE_DESTROY         = 91;
constexpr int SYS_SEMAPHORE_WAIT            = 92;
constexpr int SYS_SEMAPHORE_TRYWAIT         = 93;
constexpr int SYS_SEMAPHORE_POST            = 94;

// Event Queue
constexpr int SYS_EVENT_QUEUE_CREATE        = 128;
constexpr int SYS_EVENT_QUEUE_DESTROY       = 129;
constexpr int SYS_EVENT_QUEUE_RECEIVE       = 130;
constexpr int SYS_EVENT_QUEUE_TRYRECEIVE    = 131;
constexpr int SYS_EVENT_QUEUE_DRAIN         = 133;
constexpr int SYS_EVENT_PORT_CREATE         = 134;
constexpr int SYS_EVENT_PORT_DESTROY        = 135;
constexpr int SYS_EVENT_PORT_SEND           = 138;

// Event Flag
constexpr int SYS_EVENT_FLAG_CREATE         = 120;
constexpr int SYS_EVENT_FLAG_DESTROY        = 121;
constexpr int SYS_EVENT_FLAG_WAIT           = 122;
constexpr int SYS_EVENT_FLAG_TRYWAIT        = 123;
constexpr int SYS_EVENT_FLAG_SET            = 124;
constexpr int SYS_EVENT_FLAG_CLEAR          = 125;

// Read-Write Lock
constexpr int SYS_RWLOCK_CREATE             = 110;
constexpr int SYS_RWLOCK_DESTROY            = 111;
constexpr int SYS_RWLOCK_RLOCK              = 112;
constexpr int SYS_RWLOCK_TRYRLOCK           = 113;
constexpr int SYS_RWLOCK_RUNLOCK            = 114;
constexpr int SYS_RWLOCK_WLOCK              = 115;
constexpr int SYS_RWLOCK_TRYWLOCK           = 116;
constexpr int SYS_RWLOCK_WUNLOCK            = 117;

// Timer
constexpr int SYS_TIMER_CREATE              = 140;
constexpr int SYS_TIMER_DESTROY             = 141;
constexpr int SYS_TIMER_GET_INFO            = 142;
constexpr int SYS_TIMER_START               = 143;
constexpr int SYS_TIMER_STOP                = 144;
constexpr int SYS_TIMER_CONNECT_EVENT_QUEUE = 145;
constexpr int SYS_TIMER_USLEEP              = 141;
constexpr int SYS_TIMER_SLEEP               = 142;

// Time
constexpr int SYS_TIME_GET_CURRENT_TIME     = 145;
constexpr int SYS_TIME_GET_TIMEBASE_FREQ    = 147;

// PRX (Module) Management
constexpr int SYS_PRX_LOAD_MODULE           = 480;
constexpr int SYS_PRX_UNLOAD_MODULE         = 481;
constexpr int SYS_PRX_START_MODULE          = 482;
constexpr int SYS_PRX_STOP_MODULE           = 483;
constexpr int SYS_PRX_REGISTER_MODULE       = 484;
constexpr int SYS_PRX_GET_MODULE_LIST       = 485;
constexpr int SYS_PRX_GET_MODULE_INFO       = 486;

// File System
constexpr int SYS_FS_OPEN                   = 801;
constexpr int SYS_FS_READ                   = 802;
constexpr int SYS_FS_WRITE                  = 803;
constexpr int SYS_FS_CLOSE                  = 804;
constexpr int SYS_FS_OPENDIR                = 805;
constexpr int SYS_FS_READDIR                = 806;
constexpr int SYS_FS_CLOSEDIR               = 807;
constexpr int SYS_FS_STAT                   = 808;
constexpr int SYS_FS_FSTAT                  = 809;
constexpr int SYS_FS_MKDIR                  = 811;
constexpr int SYS_FS_RENAME                 = 812;
constexpr int SYS_FS_RMDIR                  = 813;
constexpr int SYS_FS_UNLINK                 = 814;
constexpr int SYS_FS_LSEEK                  = 818;
constexpr int SYS_FS_TRUNCATE               = 831;
constexpr int SYS_FS_FTRUNCATE              = 832;

// RSX (GPU)
constexpr int SYS_RSX_MEMORY_ALLOCATE       = 668;
constexpr int SYS_RSX_MEMORY_FREE           = 669;
constexpr int SYS_RSX_CONTEXT_ALLOCATE      = 670;
constexpr int SYS_RSX_CONTEXT_FREE          = 671;
constexpr int SYS_RSX_DEVICE_MAP            = 657;
constexpr int SYS_RSX_DEVICE_UNMAP          = 658;
constexpr int SYS_RSX_CONTEXT_IOMAP         = 674;
constexpr int SYS_RSX_CONTEXT_IOUNMAP       = 675;

// Networking
constexpr int SYS_NET_SOCKET                = 700;
constexpr int SYS_NET_BIND                  = 701;
constexpr int SYS_NET_CONNECT               = 702;
constexpr int SYS_NET_LISTEN                = 703;
constexpr int SYS_NET_ACCEPT                = 704;
constexpr int SYS_NET_SEND                  = 705;
constexpr int SYS_NET_RECV                  = 706;
constexpr int SYS_NET_CLOSE                 = 707;

}  // namespace syscall

// ============================================================================
// PS3 Error Codes
// ============================================================================
namespace error {

constexpr int CELL_OK                       = 0x00000000;
constexpr int CELL_EAGAIN                   = 0x80010001;
constexpr int CELL_EINVAL                   = 0x80010002;
constexpr int CELL_ENOSYS                   = 0x80010003;
constexpr int CELL_ENOMEM                   = 0x80010004;
constexpr int CELL_ESRCH                    = 0x80010005;
constexpr int CELL_ENOENT                   = 0x80010006;
constexpr int CELL_ENOEXEC                  = 0x80010007;
constexpr int CELL_EDEADLK                  = 0x80010008;
constexpr int CELL_EPERM                    = 0x80010009;
constexpr int CELL_EBUSY                    = 0x8001000A;
constexpr int CELL_ETIMEDOUT                = 0x8001000B;
constexpr int CELL_EABORT                   = 0x8001000C;
constexpr int CELL_EFAULT                   = 0x8001000D;
constexpr int CELL_ESTAT                    = 0x8001000F;
constexpr int CELL_EALIGN                   = 0x80010010;
constexpr int CELL_EKRESOURCE               = 0x80010011;
constexpr int CELL_EISDIR                   = 0x80010012;
constexpr int CELL_ECANCELED                = 0x80010013;
constexpr int CELL_EEXIST                   = 0x80010014;
constexpr int CELL_EISCONN                  = 0x80010015;
constexpr int CELL_ENOTCONN                 = 0x80010016;
constexpr int CELL_EAUTHFAIL                = 0x80010017;
constexpr int CELL_ENOTMSELF                = 0x80010018;
constexpr int CELL_ESYSVER                  = 0x80010019;
constexpr int CELL_EAUTHFATAL               = 0x8001001A;
constexpr int CELL_EDOM                     = 0x8001001B;
constexpr int CELL_ERANGE                   = 0x8001001C;
constexpr int CELL_EILSEQ                   = 0x8001001D;
constexpr int CELL_EFPOS                    = 0x8001001E;
constexpr int CELL_EINTR                    = 0x8001001F;
constexpr int CELL_EFBIG                    = 0x80010020;
constexpr int CELL_EMLINK                   = 0x80010021;
constexpr int CELL_ENFILE                   = 0x80010022;
constexpr int CELL_ENOSPC                   = 0x80010023;
constexpr int CELL_ENOTTY                   = 0x80010024;
constexpr int CELL_EPIPE                    = 0x80010025;
constexpr int CELL_EROFS                    = 0x80010026;
constexpr int CELL_ESPIPE                   = 0x80010027;
constexpr int CELL_E2BIG                    = 0x80010028;
constexpr int CELL_EACCES                   = 0x80010029;
constexpr int CELL_EBADF                    = 0x8001002A;
constexpr int CELL_EIO                      = 0x8001002B;
constexpr int CELL_EMFILE                   = 0x8001002C;
constexpr int CELL_ENODEV                   = 0x8001002D;
constexpr int CELL_ENOTDIR                  = 0x8001002E;
constexpr int CELL_ENXIO                    = 0x8001002F;
constexpr int CELL_EXDEV                    = 0x80010030;
constexpr int CELL_EBADMSG                  = 0x80010031;
constexpr int CELL_EINPROGRESS              = 0x80010032;
constexpr int CELL_EMSGSIZE                 = 0x80010033;
constexpr int CELL_ENAMETOOLONG             = 0x80010034;
constexpr int CELL_ENOLCK                   = 0x80010035;
constexpr int CELL_ENOTEMPTY                = 0x80010036;
constexpr int CELL_ENOTSUP                  = 0x80010037;
constexpr int CELL_EFSSPECIFIC              = 0x80010038;
constexpr int CELL_EOVERFLOW                = 0x80010039;
constexpr int CELL_ENOTMOUNTED              = 0x8001003A;
constexpr int CELL_ENOTSDATA                = 0x8001003B;

}  // namespace error

// ============================================================================
// PPU Thread Context (State passed to syscall handlers)
// ============================================================================
struct PPUThreadContext {
    // General Purpose Registers (64-bit)
    uint64_t gpr[32];
    
    // Floating Point Registers
    double fpr[32];
    
    // Vector Registers (128-bit)
    __uint128_t vr[32];
    
    // Special Purpose Registers
    uint64_t lr;      // Link Register
    uint64_t ctr;     // Count Register
    uint32_t cr;      // Condition Register
    uint32_t xer;     // Fixed-Point Exception Register
    uint64_t pc;      // Program Counter
    
    // FPSCR (Floating-Point Status and Control Register)
    uint32_t fpscr;
    
    // VSCR (Vector Status and Control Register)
    uint32_t vscr;
    
    // Thread info
    uint64_t thread_id;
    uint64_t stack_addr;
    uint64_t stack_size;
    int priority;
    
    // Syscall return value
    uint64_t retval;
};

// ============================================================================
// Syscall Handler Type
// ============================================================================
using SyscallHandler = std::function<int64_t(PPUThreadContext& ctx)>;

// ============================================================================
// PS3 Syscall Translator
// ============================================================================
class SyscallTranslator {
public:
    static SyscallTranslator& Instance();
    
    // Initialize syscall handlers
    bool Initialize();
    void Shutdown();
    
    // Handle PS3 syscall
    int64_t HandleSyscall(int syscall_num, PPUThreadContext& ctx);
    
    // Register custom handler
    void RegisterHandler(int syscall_num, SyscallHandler handler);
    
    // Get syscall name (for debugging)
    const char* GetSyscallName(int syscall_num) const;
    
    // Statistics
    struct Stats {
        uint64_t total_syscalls;
        uint64_t unimplemented_syscalls;
        std::unordered_map<int, uint64_t> syscall_counts;
    };
    Stats GetStats() const;
    
private:
    SyscallTranslator() = default;
    
    // Syscall handlers
    std::unordered_map<int, SyscallHandler> handlers_;
    std::unordered_map<int, std::string> syscall_names_;
    
    // Statistics
    Stats stats_;
    
    bool initialized_ = false;
    
    // Built-in handlers
    void RegisterBuiltinHandlers();
    
    // Process syscalls
    int64_t sys_process_exit(PPUThreadContext& ctx);
    int64_t sys_process_getpid(PPUThreadContext& ctx);
    
    // Thread syscalls
    int64_t sys_ppu_thread_create(PPUThreadContext& ctx);
    int64_t sys_ppu_thread_exit(PPUThreadContext& ctx);
    int64_t sys_ppu_thread_join(PPUThreadContext& ctx);
    int64_t sys_ppu_thread_yield(PPUThreadContext& ctx);
    
    // Memory syscalls
    int64_t sys_memory_allocate(PPUThreadContext& ctx);
    int64_t sys_memory_free(PPUThreadContext& ctx);
    
    // Mutex syscalls
    int64_t sys_mutex_create(PPUThreadContext& ctx);
    int64_t sys_mutex_destroy(PPUThreadContext& ctx);
    int64_t sys_mutex_lock(PPUThreadContext& ctx);
    int64_t sys_mutex_unlock(PPUThreadContext& ctx);
    
    // Timer syscalls
    int64_t sys_time_get_current_time(PPUThreadContext& ctx);
    int64_t sys_time_get_timebase_freq(PPUThreadContext& ctx);
    int64_t sys_timer_usleep(PPUThreadContext& ctx);
    
    // File system syscalls
    int64_t sys_fs_open(PPUThreadContext& ctx);
    int64_t sys_fs_read(PPUThreadContext& ctx);
    int64_t sys_fs_write(PPUThreadContext& ctx);
    int64_t sys_fs_close(PPUThreadContext& ctx);
    int64_t sys_fs_stat(PPUThreadContext& ctx);
    
    // PRX syscalls
    int64_t sys_prx_load_module(PPUThreadContext& ctx);
    int64_t sys_prx_start_module(PPUThreadContext& ctx);
};

}  // namespace ps3
}  // namespace nce
}  // namespace rpcsx
