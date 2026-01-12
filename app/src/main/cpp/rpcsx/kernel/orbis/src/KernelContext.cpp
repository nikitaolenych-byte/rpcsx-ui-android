#include "orbis/KernelContext.hpp"
#include "KernelObject.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/ProcessOps.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/SharedAtomic.hpp"
#include <bit>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <mutex>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <utility>

namespace orbis {
GlobalObjectRef<KernelContext> g_context = createGlobalObject<KernelContext>();
thread_local Thread *g_currentThread;

KernelContext::KernelContext() {
  // std::printf("orbis::KernelContext initialized, addr=%p\n", this);
  // std::printf("TSC frequency: %lu\n", getTscFreq());
}
KernelContext::~KernelContext() {}

long KernelContext::getTscFreq() {
  auto cal_tsc = []() -> long {
    const long timer_freq = 1'000'000'000;

    // Calibrate TSC
    constexpr int samples = 40;
    long rdtsc_data[samples];
    long timer_data[samples];
    long error_data[samples];

    struct ::timespec ts0;
    clock_gettime(CLOCK_MONOTONIC, &ts0);
    long sec_base = ts0.tv_sec;

    for (int i = 0; i < samples; i++) {
      usleep(200);
      error_data[i] = (__builtin_ia32_lfence(), __builtin_ia32_rdtsc());
      struct ::timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      rdtsc_data[i] = (__builtin_ia32_lfence(), __builtin_ia32_rdtsc());
      timer_data[i] = ts.tv_nsec + (ts.tv_sec - sec_base) * 1'000'000'000;
    }

    // Compute average TSC
    long acc = 0;
    for (int i = 0; i < samples - 1; i++) {
      acc += (rdtsc_data[i + 1] - rdtsc_data[i]) * timer_freq /
             (timer_data[i + 1] - timer_data[i]);
    }

    // Rounding
    acc /= (samples - 1);
    constexpr long grain = 1'000'000;
    return grain * (acc / grain + long{(acc % grain) > (grain / 2)});
  };

  long freq = m_tsc_freq.load();
  if (freq)
    return freq;
  m_tsc_freq.compare_exchange_strong(freq, cal_tsc());
  return m_tsc_freq.load();
}

inline namespace logs {
template <>
void log_class_string<kstring>::format(std::string &out, const void *arg) {
  out += get_object(arg);
}
} // namespace logs

void Thread::suspend() { sendSignal(-1); }
void Thread::resume() { sendSignal(-2); }

void Thread::sendSignal(int signo) {
  if (signo >= 0) {
    if (!sigMask.test(signo)) {
      return;
    }
  }

  // TODO: suspend uses another delivery confirmation
  if (signo != -1) {
    interruptedMtx.store(1, std::memory_order::release);

    if (pthread_sigqueue(getNativeHandle(), SIGUSR1, {.sival_int = signo})) {
      perror("pthread_sigqueue");
    }

    while (interruptedMtx.wait(1, std::chrono::microseconds(1000)) !=
           std::errc{}) {
      if (interruptedMtx.load() == 0) {
        return;
      }

      if (pthread_sigqueue(getNativeHandle(), SIGUSR1, {.sival_int = -2})) {
        perror("pthread_sigqueue");
      }
    }
  } else {
    if (pthread_sigqueue(getNativeHandle(), SIGUSR1, {.sival_int = signo})) {
      perror("pthread_sigqueue");
    }
  }
}

void Thread::notifyUnblockedSignal(int signo) {
  for (std::size_t i = 0; i < blockedSignals.size();) {
    if (blockedSignals[i].signo != signo) {
      ++i;
      continue;
    }

    queuedSignals.push_back(blockedSignals[i]);
    blockedSignals.erase(blockedSignals.begin() + i);
  }
}

void Thread::setSigMask(SigSet newSigMask) {
  newSigMask.clear(kSigKill);
  newSigMask.clear(kSigStop);

  auto oldSigMask = std::exchange(sigMask, newSigMask);

  for (std::size_t word = 0; word < std::size(newSigMask.bits); ++word) {
    auto unblockedBits = ~oldSigMask.bits[word] & newSigMask.bits[word];
    std::uint32_t offset = word * 32 + 1;

    for (std::uint32_t i = std::countr_zero(unblockedBits); i < 32;
         i += std::countr_zero(unblockedBits >> (i + 1)) + 1) {
      notifyUnblockedSignal(offset + i);
    }
  }
}

void Thread::where() { tproc->ops->where(this); }

bool Thread::unblock() {
  std::uint32_t prev = interruptedMtx.exchange(0, std::memory_order::relaxed);
  if (prev != 0) {
    interruptedMtx.notify_one();
    return false;
  }

  tproc->ops->unblock(this);

  return true;
}

bool Thread::block() {
  tproc->ops->block(this);

  std::uint32_t prev = interruptedMtx.exchange(0, std::memory_order::relaxed);
  if (prev != 0) {
    interruptedMtx.notify_one();
  }

  return prev == 0;
}

scoped_unblock::scoped_unblock() {
  if (g_currentThread && g_currentThread->context) {
    rx::g_scopedUnblock = [](bool unblock) {
      if (unblock) {
        return g_currentThread->unblock();
      }

      return g_currentThread->block();
    };
  }
}

scoped_unblock::~scoped_unblock() { rx::g_scopedUnblock = nullptr; }
} // namespace orbis
