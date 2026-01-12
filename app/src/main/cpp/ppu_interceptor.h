/**
 * PPU Interceptor - перехоплює виконання PPU для NCE JIT
 */

#pragma once

#include <cstdint>

namespace rpcsx::ppu {

// Statistics structure
struct InterceptorStats {
    uint64_t blocks_executed = 0;
    uint64_t blocks_jitted = 0;
    uint64_t interpreter_fallbacks = 0;
    uint64_t total_instructions = 0;
    uint64_t unique_blocks = 0;
};

/**
 * Initialize the PPU interceptor
 * @param librpcsx_path Path to librpcsx.so
 * @return true if initialized successfully
 */
bool InitializeInterceptor(const char* librpcsx_path);

/**
 * Shutdown the interceptor, restore original functions
 */
void ShutdownInterceptor();

/**
 * Check if interceptor is active
 */
bool IsInterceptorActive();

/**
 * Get execution statistics
 */
InterceptorStats GetStats();

/**
 * Reset statistics counters
 */
void ResetStats();

/**
 * Manually execute a block of PPU code
 * @param memory_base Base address of guest memory
 * @param pc Program counter (guest address)
 * @param next_pc Output: next PC after execution
 * @return true if JIT executed, false if fallback needed
 */
bool ExecuteBlock(void* memory_base, uint32_t pc, uint32_t* next_pc);

/**
 * Invalidate JIT cache for a specific block
 */
void InvalidateBlock(uint32_t pc);

/**
 * Invalidate JIT cache for a memory range
 */
void InvalidateRange(uint32_t start, uint32_t size);

} // namespace rpcsx::ppu
