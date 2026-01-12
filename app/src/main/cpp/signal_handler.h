#ifndef RPCSX_SIGNAL_HANDLER_H
#define RPCSX_SIGNAL_HANDLER_H

namespace rpcsx::crash {

// Installs SIGSEGV/SIGBUS/SIGILL/SIGFPE handlers with an alternate signal stack.
// Safe to call multiple times.
bool InstallSignalHandlers();

// Enable/disable JIT handler for SIGILL signals
// When enabled, SIGILL will attempt to JIT compile x86 code to ARM64
void EnableJITHandler(bool enable);
bool IsJITHandlerEnabled();

// Lightweight scoped guard that can recover from native crashes inside a guarded
// region (via sigsetjmp/siglongjmp). Intended to wrap calls into native/JIT/core
// code so the app can fail gracefully instead of hard-crashing.
class CrashGuard {
public:
    explicit CrashGuard(const char* scope);
    CrashGuard(const CrashGuard&) = delete;
    CrashGuard& operator=(const CrashGuard&) = delete;
    ~CrashGuard();

    // True on the normal execution path; false if we returned here due to a signal.
    bool ok() const;

    int signal() const;
    void* faultAddress() const;
    const char* scope() const;

private:
    const char* scope_;
    bool ok_;
};

} // namespace rpcsx::crash

#endif // RPCSX_SIGNAL_HANDLER_H
