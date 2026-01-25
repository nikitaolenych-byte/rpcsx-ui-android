// ============================================================================
// LLVM Optimized PPU/SPU Headers
// ============================================================================

#ifndef LLVM_OPTIMIZED_PPU_SPU_H
#define LLVM_OPTIMIZED_PPU_SPU_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PPU LLVM Optimized API
// ============================================================================

/**
 * Initialize the optimized PPU LLVM JIT engine
 * @return 0 on success, -1 on failure
 */
int ppu_llvm_opt_init(void);

/**
 * Shutdown the PPU engine and free resources
 */
void ppu_llvm_opt_shutdown(void);

/**
 * Compile a block of PPU (PowerPC64) code to native ARM64
 * @param code Pointer to PPU instructions
 * @param address Guest virtual address
 * @param size Size in bytes
 * @return Compiled block handle, or NULL on failure
 */
void* ppu_llvm_opt_compile(const uint8_t* code, uint64_t address, size_t size);

/**
 * Execute a compiled PPU block
 * @param block Compiled block handle
 * @param gpr Pointer to 32 x 64-bit general purpose registers
 * @param fpr Pointer to 32 x 64-bit floating point registers
 * @param mem Memory base pointer
 * @return Next program counter
 */
uint64_t ppu_llvm_opt_execute(void* block, uint64_t* gpr, double* fpr, void* mem);

/**
 * Print PPU JIT statistics
 */
void ppu_llvm_opt_print_stats(void);

// ============================================================================
// SPU LLVM Optimized API
// ============================================================================

/**
 * Initialize the optimized SPU LLVM JIT engine
 * @return 0 on success, -1 on failure
 */
int spu_llvm_opt_init(void);

/**
 * Shutdown the SPU engine
 */
void spu_llvm_opt_shutdown(void);

/**
 * Compile a block of SPU code to native ARM64 NEON
 * @param code Pointer to SPU instructions
 * @param address SPU local store address
 * @param size Size in bytes
 * @return Compiled block handle
 */
void* spu_llvm_opt_compile(const uint8_t* code, uint32_t address, size_t size);

/**
 * Execute a compiled SPU block
 * @param block Compiled block handle
 * @param regs Pointer to 128 x 128-bit SPU registers
 * @param ls Local Store pointer (256KB)
 * @return Next program counter
 */
uint32_t spu_llvm_opt_execute(void* block, void* regs, void* ls);

/**
 * Print SPU JIT statistics
 */
void spu_llvm_opt_print_stats(void);

// ============================================================================
// Configuration API
// ============================================================================

/**
 * Set optimization level (0-3, default 3)
 */
void llvm_opt_set_level(int level);

/**
 * Enable/disable NEON acceleration
 */
void llvm_opt_enable_neon(int enable);

/**
 * Enable/disable SVE2 (for Snapdragon 8 Gen 3+)
 */
void llvm_opt_enable_sve2(int enable);

/**
 * Enable/disable fast math
 */
void llvm_opt_enable_fast_math(int enable);

/**
 * Enable/disable mega blocks
 */
void llvm_opt_enable_mega_blocks(int enable);

/**
 * Get detected CPU features as string
 */
const char* llvm_opt_get_cpu_features(void);

#ifdef __cplusplus
}
#endif

#endif // LLVM_OPTIMIZED_PPU_SPU_H
