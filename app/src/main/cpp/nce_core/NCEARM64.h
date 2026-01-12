/**
 * NCE ARM64 JIT Recompiler for PPU
 * Native Code Execution - компіляція PowerPC → ARM64
 * 
 * Оптимізовано для Snapdragon 8s Gen 3 (ARMv9 + SVE2)
 */

#pragma once

#include <cstdint>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <functional>

// Forward declarations
class ppu_thread;

namespace rpcsx::nce {

// PPU стан для JIT
struct PPUState {
    // Загальні регістри (GPR)
    uint64_t gpr[32];
    
    // Floating-point регістри
    double fpr[32];
    
    // Vector регістри (128-bit)
    alignas(16) uint8_t vr[32][16];
    
    // Спеціальні регістри
    uint64_t lr;        // Link Register
    uint64_t ctr;       // Count Register
    uint32_t cr;        // Condition Register
    uint32_t xer;       // Fixed-Point Exception Register
    uint32_t fpscr;     // FP Status and Control Register
    uint32_t vscr;      // Vector Status and Control Register
    
    // Program Counter
    uint64_t pc;
    uint64_t npc;       // Next PC (for branches)
    
    // Memory base pointer
    void* memory_base;
    
    // Thread pointer (for callbacks)
    ppu_thread* thread;
};

// Скомпільований блок
struct CompiledBlock {
    void* code;             // Pointer to native ARM64 code
    uint32_t guest_addr;    // Guest address
    uint32_t guest_size;    // Size in bytes
    uint32_t host_size;     // Host code size
    uint32_t exec_count;    // Execution count for profiling
    bool has_branch;        // Contains branch instruction
    uint32_t branch_target; // Branch target if known
};

// JIT конфігурація
struct JitConfig {
    size_t code_cache_size = 128 * 1024 * 1024;  // 128MB default
    uint32_t max_block_size = 4096;               // Max instructions per block
    bool enable_block_linking = true;
    bool enable_fastmem = true;
    bool enable_profiling = false;
    uint32_t optimization_level = 2;              // 0=none, 1=basic, 2=full
};

// Статистика
struct JitStats {
    std::atomic<uint64_t> blocks_compiled{0};
    std::atomic<uint64_t> blocks_executed{0};
    std::atomic<uint64_t> instructions_executed{0};
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> interpreter_fallbacks{0};
};

/**
 * NCE ARM64 JIT Compiler
 */
class NCEARM64Compiler {
public:
    NCEARM64Compiler();
    ~NCEARM64Compiler();
    
    // Initialize compiler with config
    bool Initialize(const JitConfig& config);
    
    // Shutdown and cleanup
    void Shutdown();
    
    // Compile a block starting at guest_addr
    CompiledBlock* CompileBlock(ppu_thread& ppu, uint32_t guest_addr);
    
    // Lookup cached block
    CompiledBlock* LookupBlock(uint32_t guest_addr);
    
    // Execute a compiled block
    void ExecuteBlock(ppu_thread& ppu, CompiledBlock* block);
    
    // Invalidate cache for address range
    void InvalidateRange(uint32_t start, uint32_t size);
    
    // Clear entire cache
    void ClearCache();
    
    // Get statistics
    const JitStats& GetStats() const { return m_stats; }
    
    // Reset statistics
    void ResetStats();
    
    // Check if initialized
    bool IsInitialized() const { return m_initialized; }

private:
    // Code cache management
    void* AllocateCode(size_t size);
    void FreeCode(void* ptr, size_t size);
    
    // Block compilation internals
    bool CompileInstruction(uint32_t opcode, uint32_t addr);
    void EmitPrologue();
    void EmitEpilogue();
    void EmitBranchCheck(uint32_t target);
    
    // ARM64 code emission helpers
    void EmitLoadGPR(int host_reg, int guest_reg);
    void EmitStoreGPR(int guest_reg, int host_reg);
    void EmitLoadMem32(int host_reg, uint32_t guest_addr);
    void EmitStoreMem32(uint32_t guest_addr, int host_reg);
    
private:
    JitConfig m_config;
    JitStats m_stats;
    
    // Code cache
    void* m_code_cache = nullptr;
    size_t m_code_cache_size = 0;
    size_t m_code_cache_used = 0;
    
    // Block lookup table
    std::unordered_map<uint32_t, std::unique_ptr<CompiledBlock>> m_blocks;
    std::mutex m_blocks_mutex;
    
    // Current compilation state
    uint8_t* m_emit_ptr = nullptr;
    size_t m_emit_size = 0;
    
    bool m_initialized = false;
};

/**
 * Global NCE interface for PPUThread
 */
class NCEExecutor {
public:
    static NCEExecutor& Instance();
    
    // Initialize NCE for this session
    bool Initialize();
    
    // Shutdown NCE
    void Shutdown();
    
    // Execute PPU code using NCE JIT
    // Returns false if should fallback to interpreter
    bool Execute(ppu_thread& ppu);
    
    // Get compiler instance
    NCEARM64Compiler& GetCompiler() { return m_compiler; }
    
    // Check if NCE is available and active
    bool IsActive() const { return m_active; }
    
private:
    NCEExecutor() = default;
    
    NCEARM64Compiler m_compiler;
    bool m_active = false;
};

} // namespace rpcsx::nce
