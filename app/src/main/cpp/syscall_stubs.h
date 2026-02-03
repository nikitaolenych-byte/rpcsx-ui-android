/**
 * PS3 Syscall Stubbing System
 * 
 * Provides stubs for unimplemented or partially implemented PS3 LV2 syscalls.
 * Allows games to continue running even when encountering unimplemented functionality.
 * 
 * Features:
 * - Syscall hooking and interception
 * - Stub implementations with configurable behavior
 * - Logging of unimplemented syscall usage
 * - Return value spoofing
 * - Per-game syscall overrides
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <unordered_map>

namespace rpcsx::syscalls {

// =============================================================================
// PS3 LV2 Syscall Categories
// =============================================================================

enum class SyscallCategory {
    PROCESS,        // Управління процесами
    THREAD,         // Управління потоками
    MUTEX,          // Мютекси та синхронізація
    COND,           // Умовні змінні
    RWLOCK,         // Read-Write блокування
    SEMAPHORE,      // Семафори
    EVENT_FLAG,     // Прапорці подій
    EVENT_QUEUE,    // Черги подій
    LWMutex,        // Легкі мютекси
    LWCond,         // Легкі умовні змінні
    TIMER,          // Таймери
    TIME,           // Час та дата
    MEMORY,         // Управління пам'яттю
    MODULE,         // Завантаження модулів
    PRX,            // PRX модулі
    SPU,            // SPU управління
    STORAGE,        // Зберігання даних
    FILESYSTEM,     // Файлова система
    NETWORK,        // Мережа
    SOCKET,         // Сокети
    PAD,            // Геймпад
    KEYBOARD,       // Клавіатура
    MOUSE,          // Миша
    AUDIO,          // Аудіо
    VIDEO,          // Відео
    GCM,            // Graphics Command Manager (RSX)
    TROPHY,         // Трофеї
    SAVEDATA,       // Збереження
    SYSUTIL,        // Системні утиліти
    DISC,           // Оптичний диск
    HDD,            // Жорсткий диск
    USB,            // USB
    BLUETOOTH,      // Bluetooth
    CAMERA,         // Камера
    MISC,           // Різне
    UNKNOWN         // Невідомі
};

// =============================================================================
// Stub Behavior
// =============================================================================

enum class StubBehavior {
    RETURN_SUCCESS,     // Повертає 0 (успіх)
    RETURN_ERROR,       // Повертає код помилки
    RETURN_CUSTOM,      // Повертає кастомне значення
    LOG_ONLY,           // Тільки логування, без зміни поведінки
    CRASH,              // Крашить емулятор (для критичних syscalls)
    FORWARD,            // Передати реальній реалізації
    EMULATE,            // Емулювати функціональність
    SKIP                // Пропустити syscall
};

// =============================================================================
// PS3 Error Codes
// =============================================================================

namespace ps3_error {
    constexpr int32_t CELL_OK = 0x00000000;
    constexpr int32_t CELL_CANCEL = 0x00000001;
    constexpr int32_t CELL_ENOENT = 0x80010002;
    constexpr int32_t CELL_ESRCH = 0x80010003;
    constexpr int32_t CELL_EINTR = 0x80010004;
    constexpr int32_t CELL_EIO = 0x80010005;
    constexpr int32_t CELL_ENXIO = 0x80010006;
    constexpr int32_t CELL_E2BIG = 0x80010007;
    constexpr int32_t CELL_ENOEXEC = 0x80010008;
    constexpr int32_t CELL_EBADF = 0x80010009;
    constexpr int32_t CELL_ECHILD = 0x8001000A;
    constexpr int32_t CELL_EAGAIN = 0x8001000B;
    constexpr int32_t CELL_ENOMEM = 0x8001000C;
    constexpr int32_t CELL_EACCES = 0x8001000D;
    constexpr int32_t CELL_EFAULT = 0x8001000E;
    constexpr int32_t CELL_ENOTBLK = 0x8001000F;
    constexpr int32_t CELL_EBUSY = 0x80010010;
    constexpr int32_t CELL_EEXIST = 0x80010011;
    constexpr int32_t CELL_EXDEV = 0x80010012;
    constexpr int32_t CELL_ENODEV = 0x80010013;
    constexpr int32_t CELL_ENOTDIR = 0x80010014;
    constexpr int32_t CELL_EISDIR = 0x80010015;
    constexpr int32_t CELL_EINVAL = 0x80010016;
    constexpr int32_t CELL_ENFILE = 0x80010017;
    constexpr int32_t CELL_EMFILE = 0x80010018;
    constexpr int32_t CELL_ENOTTY = 0x80010019;
    constexpr int32_t CELL_ETXTBSY = 0x8001001A;
    constexpr int32_t CELL_EFBIG = 0x8001001B;
    constexpr int32_t CELL_ENOSPC = 0x8001001C;
    constexpr int32_t CELL_ESPIPE = 0x8001001D;
    constexpr int32_t CELL_EROFS = 0x8001001E;
    constexpr int32_t CELL_EMLINK = 0x8001001F;
    constexpr int32_t CELL_EPIPE = 0x80010020;
    constexpr int32_t CELL_EDOM = 0x80010021;
    constexpr int32_t CELL_ERANGE = 0x80010022;
    constexpr int32_t CELL_EDEADLK = 0x80010023;
    constexpr int32_t CELL_ENAMETOOLONG = 0x80010024;
    constexpr int32_t CELL_ENOLCK = 0x80010025;
    constexpr int32_t CELL_ENOSYS = 0x80010026;  // Не реалізовано
    constexpr int32_t CELL_ENOTEMPTY = 0x80010027;
    constexpr int32_t CELL_ETIMEDOUT = 0x8001006E;
    constexpr int32_t CELL_ENOTINIT = 0x8001F002;
    constexpr int32_t CELL_UNKNOWN = 0x80010FFF;
}

// =============================================================================
// Syscall Definition
// =============================================================================

struct SyscallInfo {
    uint32_t number;                // Номер syscall
    std::string name;               // Назва
    SyscallCategory category;       // Категорія
    bool is_implemented;            // Чи реалізовано
    StubBehavior default_behavior;  // Поведінка за замовчуванням
    int32_t default_return;         // Значення за замовчуванням
    std::string description;        // Опис
    uint64_t call_count;            // Лічильник викликів
    bool log_calls;                 // Логувати виклики
};

// =============================================================================
// Stub Configuration
// =============================================================================

struct StubConfig {
    bool enabled = true;
    bool log_unimplemented = true;
    bool log_stubbed = false;
    bool log_all_calls = false;
    bool crash_on_critical = false;
    StubBehavior default_behavior = StubBehavior::RETURN_SUCCESS;
    int32_t default_error_code = ps3_error::CELL_ENOSYS;
    std::string log_file;
};

// =============================================================================
// Per-Game Override
// =============================================================================

struct GameSyscallOverride {
    std::string title_id;
    uint32_t syscall_number;
    StubBehavior behavior;
    int32_t custom_return;
    std::string reason;
};

// =============================================================================
// Syscall Statistics
// =============================================================================

struct SyscallStats {
    uint64_t total_calls;
    uint64_t implemented_calls;
    uint64_t stubbed_calls;
    uint64_t failed_calls;
    uint32_t unique_syscalls_used;
    uint32_t unimplemented_used;
    std::vector<std::pair<uint32_t, uint64_t>> top_syscalls;
    std::vector<std::pair<uint32_t, uint64_t>> top_unimplemented;
};

// =============================================================================
// Handler Types
// =============================================================================

// Контекст syscall
struct SyscallContext {
    uint32_t syscall_num;
    uint64_t args[8];           // Аргументи syscall (r3-r10)
    uint64_t* result;           // Результат (r3)
    void* thread_context;       // Контекст потоку
    const char* title_id;       // ID гри
};

// Хандлер syscall
using SyscallHandler = std::function<int32_t(SyscallContext& ctx)>;

// =============================================================================
// Global State
// =============================================================================

extern std::atomic<bool> g_stubs_active;
extern std::atomic<uint64_t> g_total_syscalls;
extern std::atomic<uint64_t> g_stubbed_syscalls;

// =============================================================================
// API Functions
// =============================================================================

/**
 * Ініціалізація системи заглушок
 */
bool InitializeSyscallStubs(const StubConfig& config);

/**
 * Завершення роботи
 */
void ShutdownSyscallStubs();

/**
 * Чи активна система
 */
bool IsStubSystemActive();

/**
 * Обробка syscall
 * @param syscall_num Номер syscall
 * @param args Аргументи (r3-r10)
 * @param result Результат
 * @return true якщо syscall оброблено
 */
bool HandleSyscall(uint32_t syscall_num, uint64_t* args, uint64_t* result);

/**
 * Отримати інформацію про syscall
 */
bool GetSyscallInfo(uint32_t syscall_num, SyscallInfo* info);

/**
 * Зареєструвати кастомний хандлер
 */
bool RegisterSyscallHandler(uint32_t syscall_num, SyscallHandler handler);

/**
 * Видалити хандлер
 */
bool UnregisterSyscallHandler(uint32_t syscall_num);

/**
 * Встановити поведінку для syscall
 */
bool SetSyscallBehavior(uint32_t syscall_num, StubBehavior behavior, int32_t custom_return = 0);

/**
 * Встановити override для гри
 */
bool SetGameSyscallOverride(const char* title_id, uint32_t syscall_num,
                            StubBehavior behavior, int32_t custom_return);

/**
 * Видалити override для гри
 */
bool RemoveGameSyscallOverride(const char* title_id, uint32_t syscall_num);

/**
 * Завантажити overrides з файлу
 */
bool LoadSyscallOverrides(const char* filepath);

/**
 * Зберегти overrides у файл
 */
bool SaveSyscallOverrides(const char* filepath);

/**
 * Отримати статистику
 */
void GetSyscallStats(SyscallStats* stats);

/**
 * Скинути статистику
 */
void ResetSyscallStats();

/**
 * Отримати список нереалізованих syscalls, які використовувались
 */
size_t GetUnimplementedSyscalls(uint32_t* buffer, size_t buffer_size);

/**
 * Експорт логу в JSON
 */
size_t ExportSyscallLogJson(char* buffer, size_t buffer_size);

/**
 * Увімкнути/вимкнути логування для конкретного syscall
 */
bool SetSyscallLogging(uint32_t syscall_num, bool enable);

/**
 * Перевірити чи syscall реалізовано
 */
bool IsSyscallImplemented(uint32_t syscall_num);

/**
 * Отримати назву syscall
 */
const char* GetSyscallName(uint32_t syscall_num);

/**
 * Отримати категорію syscall
 */
SyscallCategory GetSyscallCategory(uint32_t syscall_num);

// =============================================================================
// Відомі PS3 Syscall номери
// =============================================================================

namespace syscall_num {
    // Process
    constexpr uint32_t SYS_PROCESS_GETPID = 1;
    constexpr uint32_t SYS_PROCESS_GETPPID = 2;
    constexpr uint32_t SYS_PROCESS_EXIT = 3;
    constexpr uint32_t SYS_PROCESS_GET_SDK_VERSION = 25;
    
    // Thread
    constexpr uint32_t SYS_PPU_THREAD_CREATE = 41;
    constexpr uint32_t SYS_PPU_THREAD_EXIT = 42;
    constexpr uint32_t SYS_PPU_THREAD_YIELD = 43;
    constexpr uint32_t SYS_PPU_THREAD_JOIN = 44;
    constexpr uint32_t SYS_PPU_THREAD_DETACH = 45;
    constexpr uint32_t SYS_PPU_THREAD_GET_ID = 46;
    
    // Mutex
    constexpr uint32_t SYS_MUTEX_CREATE = 100;
    constexpr uint32_t SYS_MUTEX_DESTROY = 101;
    constexpr uint32_t SYS_MUTEX_LOCK = 102;
    constexpr uint32_t SYS_MUTEX_TRYLOCK = 103;
    constexpr uint32_t SYS_MUTEX_UNLOCK = 104;
    
    // Cond
    constexpr uint32_t SYS_COND_CREATE = 110;
    constexpr uint32_t SYS_COND_DESTROY = 111;
    constexpr uint32_t SYS_COND_WAIT = 112;
    constexpr uint32_t SYS_COND_SIGNAL = 113;
    constexpr uint32_t SYS_COND_SIGNAL_ALL = 114;
    
    // Memory
    constexpr uint32_t SYS_MEMORY_ALLOCATE = 348;
    constexpr uint32_t SYS_MEMORY_FREE = 349;
    constexpr uint32_t SYS_MEMORY_GET_PAGE_ATTRIBUTE = 352;
    constexpr uint32_t SYS_MMAPPER_ALLOCATE_ADDRESS = 330;
    constexpr uint32_t SYS_MMAPPER_FREE_ADDRESS = 331;
    
    // Time
    constexpr uint32_t SYS_TIME_GET_CURRENT_TIME = 145;
    constexpr uint32_t SYS_TIME_GET_TIMEZONE = 146;
    constexpr uint32_t SYS_TIMER_USLEEP = 142;
    constexpr uint32_t SYS_TIMER_SLEEP = 143;
    
    // PRX
    constexpr uint32_t SYS_PRX_LOAD_MODULE = 480;
    constexpr uint32_t SYS_PRX_START_MODULE = 481;
    constexpr uint32_t SYS_PRX_STOP_MODULE = 482;
    constexpr uint32_t SYS_PRX_UNLOAD_MODULE = 483;
    constexpr uint32_t SYS_PRX_GET_MODULE_LIST = 484;
    constexpr uint32_t SYS_PRX_GET_MODULE_INFO = 485;
    
    // Filesystem
    constexpr uint32_t SYS_FS_OPEN = 801;
    constexpr uint32_t SYS_FS_READ = 802;
    constexpr uint32_t SYS_FS_WRITE = 803;
    constexpr uint32_t SYS_FS_CLOSE = 804;
    constexpr uint32_t SYS_FS_STAT = 818;
    constexpr uint32_t SYS_FS_FSTAT = 819;
    constexpr uint32_t SYS_FS_MKDIR = 811;
    constexpr uint32_t SYS_FS_RMDIR = 812;
    constexpr uint32_t SYS_FS_UNLINK = 814;
    
    // SPU
    constexpr uint32_t SYS_SPU_THREAD_GROUP_CREATE = 160;
    constexpr uint32_t SYS_SPU_THREAD_GROUP_DESTROY = 161;
    constexpr uint32_t SYS_SPU_THREAD_INITIALIZE = 168;
    constexpr uint32_t SYS_SPU_THREAD_GROUP_START = 162;
    constexpr uint32_t SYS_SPU_THREAD_GROUP_TERMINATE = 164;
    
    // Network
    constexpr uint32_t SYS_NET_SOCKET = 700;
    constexpr uint32_t SYS_NET_BIND = 701;
    constexpr uint32_t SYS_NET_CONNECT = 702;
    constexpr uint32_t SYS_NET_SEND = 704;
    constexpr uint32_t SYS_NET_RECV = 705;
}

} // namespace rpcsx::syscalls
