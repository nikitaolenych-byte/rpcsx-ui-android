#include "signal_handler.h"
#include "nce_engine.h"

#include <android/log.h>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <setjmp.h>
#include <sys/mman.h>
#include <unistd.h>

#if defined(__ANDROID__)
#include <ucontext.h>
#endif

#define LOG_TAG "RPCSX-Signal"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace rpcsx::crash {

namespace {

struct FaultInfo {
    int sig = 0;
    void* addr = nullptr;
};

static std::atomic<bool> g_installed{false};
static std::atomic<bool> g_jit_handler_active{false};

static thread_local sigjmp_buf* tl_jmp = nullptr;
static thread_local int tl_depth = 0;
static thread_local FaultInfo tl_fault;
static thread_local const char* tl_scope = nullptr;

static void write_stderr(const char* msg) {
    if (!msg) return;
    (void)!::write(STDERR_FILENO, msg, ::strlen(msg));
}

/**
 * JIT signal handler for SIGILL - attempts to JIT compile and execute x86 code
 */
static bool try_jit_execute(void* fault_addr, ucontext_t* uctx) {
    if (!rpcsx::nce::IsNCEActive()) return false;
    if (!fault_addr) return false;
    
#if defined(__aarch64__)
    // Get the faulting PC
    uint64_t pc = uctx->uc_mcontext.pc;
    
    LOGD("JIT intercept: SIGILL at PC=0x%llx, addr=0x%llx", 
         (unsigned long long)pc, (unsigned long long)fault_addr);
    
    // Try to JIT compile this code block
    void* jit_code = rpcsx::nce::TranslatePPUToARM(
        reinterpret_cast<const uint8_t*>(pc), 256);  // Compile up to 256 bytes
    
    if (jit_code) {
        LOGI("JIT compiled block at 0x%llx -> 0x%llx", 
             (unsigned long long)pc, (unsigned long long)jit_code);
        // Update PC to point to JIT'd code
        uctx->uc_mcontext.pc = reinterpret_cast<uint64_t>(jit_code);
        return true;
    }
#endif
    return false;
}

static void signal_handler(int sig, siginfo_t* info, void* ucontext) {
    tl_fault.sig = sig;
    tl_fault.addr = info ? info->si_addr : nullptr;
    
    // Try JIT execution for SIGILL (illegal instruction)
    if (sig == SIGILL && g_jit_handler_active.load() && ucontext) {
        ucontext_t* uctx = static_cast<ucontext_t*>(ucontext);
        if (try_jit_execute(info->si_addr, uctx)) {
            return;  // JIT handled it, resume execution
        }
    }

    if (tl_jmp && tl_depth > 0) {
        siglongjmp(*tl_jmp, 1);
    }

    // No guard active: fall back to default behaviour.
    write_stderr("RPCSX: fatal signal without guard\n");

    struct sigaction sa {};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    (void)!sigaction(sig, &sa, nullptr);
    raise(sig);
}

static bool install_altstack_and_handlers() {
    // Alternate stack to handle stack overflows / corrupted stacks.
    constexpr size_t kStackSize = 64 * 1024;

    void* stack = mmap(nullptr, kStackSize, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) {
        LOGE("sigaltstack mmap failed: %s", strerror(errno));
        return false;
    }

    stack_t ss {};
    ss.ss_sp = stack;
    ss.ss_size = kStackSize;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, nullptr) != 0) {
        LOGE("sigaltstack failed: %s", strerror(errno));
        munmap(stack, kStackSize);
        return false;
    }

    struct sigaction sa {};
    sa.sa_sigaction = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;

    const int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE};
    for (int s : signals) {
        if (sigaction(s, &sa, nullptr) != 0) {
            LOGE("sigaction(%d) failed: %s", s, strerror(errno));
            return false;
        }
    }

    return true;
}

} // namespace

bool InstallSignalHandlers() {
    bool expected = false;
    if (!g_installed.compare_exchange_strong(expected, true)) {
        return true;
    }

    const bool ok = install_altstack_and_handlers();
    if (ok) {
        LOGI("Crash signal handlers installed");
    } else {
        LOGE("Failed to install crash signal handlers");
    }
    return ok;
}

void EnableJITHandler(bool enable) {
    g_jit_handler_active.store(enable);
    LOGI("JIT signal handler %s", enable ? "enabled" : "disabled");
}

bool IsJITHandlerEnabled() {
    return g_jit_handler_active.load();
}

CrashGuard::CrashGuard(const char* scope) : scope_(scope), ok_(true) {
    // Nesting support.
    ++tl_depth;

    sigjmp_buf env;
    tl_jmp = &env;
    tl_scope = scope_;

    if (sigsetjmp(env, 1) != 0) {
        ok_ = false;
    }

    // If ok_ is false, we returned here due to a signal. Keep the guard active
    // until destruction so callers can query fault info.
}

CrashGuard::~CrashGuard() {
    if (tl_depth > 0) {
        --tl_depth;
    }
    if (tl_depth == 0) {
        tl_jmp = nullptr;
        tl_scope = nullptr;
        tl_fault = {};
    }
}

bool CrashGuard::ok() const { return ok_; }
int CrashGuard::signal() const { return tl_fault.sig; }
void* CrashGuard::faultAddress() const { return tl_fault.addr; }
const char* CrashGuard::scope() const { return scope_; }

} // namespace rpcsx::crash
