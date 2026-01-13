/**
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║                    NCE v8 - Multi-Tier JIT Compiler                      ║
 * ║                                                                          ║
 * ║  Tier 0: Interpreter (immediate, no compile)                             ║
 * ║  Tier 1: Baseline JIT (fast compile, ~10x speedup)                       ║
 * ║  Tier 2: Optimizing JIT (slow compile, ~50x speedup)                     ║
 * ║  Tier 3: Megamorphic (polymorphic specialization)                        ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 */

#ifndef RPCSX_NCE_V8_TIERED_JIT_H
#define RPCSX_NCE_V8_TIERED_JIT_H

#include "nce_v8.h"
#include <unordered_map>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace rpcsx::nce::v8 {

// ============================================================================
// Baseline Compiler (Tier 1) - Fast, template-based translation
// ============================================================================
class BaselineCompiler {
public:
    BaselineCompiler();
    ~BaselineCompiler();
    
    bool Initialize(void* code_cache, size_t cache_size);
    void Shutdown();
    
    CompiledBlockV8* Compile(const uint8_t* ppc_code, uint64_t address, size_t size);
    
    size_t GetCacheUsage() const { return cache_used_; }
    
private:
    // Template-based instruction translation
    void* EmitPPCToARM64Template(uint32_t ppc_instr, void* emit_ptr);
    
    // Fast path for common instructions
    void* EmitAdd(uint32_t instr, void* emit_ptr);
    void* EmitSub(uint32_t instr, void* emit_ptr);
    void* EmitLoad(uint32_t instr, void* emit_ptr);
    void* EmitStore(uint32_t instr, void* emit_ptr);
    void* EmitBranch(uint32_t instr, void* emit_ptr);
    void* EmitCompare(uint32_t instr, void* emit_ptr);
    
    void* code_cache_;
    size_t cache_size_;
    size_t cache_used_;
    
    std::mutex compile_mutex_;
};

// ============================================================================
// Optimizing Compiler (Tier 2) - SSA-based, full optimizations
// ============================================================================

// SSA Value
struct SSAValue {
    uint32_t id;
    enum Type { INT32, INT64, FLOAT32, FLOAT64, VEC128, VEC256 } type;
    int reg_hint;  // -1 = no preference
    bool is_constant;
    uint64_t constant_value;
    std::vector<uint32_t> uses;
};

// SSA Instruction
struct SSAInstr {
    enum Op {
        // Arithmetic
        ADD, SUB, MUL, DIV, MOD, NEG,
        AND, OR, XOR, NOT, SHL, SHR, SAR, ROL, ROR,
        
        // Memory
        LOAD8, LOAD16, LOAD32, LOAD64, LOAD_VEC,
        STORE8, STORE16, STORE32, STORE64, STORE_VEC,
        
        // Control flow
        BRANCH, BRANCH_COND, CALL, RET, SWITCH,
        
        // Comparison
        CMP_EQ, CMP_NE, CMP_LT, CMP_LE, CMP_GT, CMP_GE,
        
        // Floating point
        FADD, FSUB, FMUL, FDIV, FNEG, FSQRT, FABS,
        FCMP_EQ, FCMP_LT, FCMP_LE,
        
        // Vector (NEON/SVE2)
        VEC_ADD, VEC_SUB, VEC_MUL, VEC_DIV,
        VEC_AND, VEC_OR, VEC_XOR,
        VEC_SHUFFLE, VEC_PERMUTE, VEC_BLEND,
        VEC_LOAD, VEC_STORE, VEC_BROADCAST,
        
        // Special
        PHI, SELECT, TRAP, NOP,
    };
    
    Op opcode;
    uint32_t dest;           // SSA value ID
    std::vector<uint32_t> sources;
    uint64_t immediate;
    uint32_t flags;
};

// Basic Block
struct BasicBlock {
    uint64_t start_address;
    uint64_t end_address;
    std::vector<SSAInstr> instructions;
    std::vector<uint32_t> predecessors;
    std::vector<uint32_t> successors;
    uint32_t loop_depth;
    bool is_loop_header;
    bool is_hot;
};

// Control Flow Graph
struct CFG {
    std::vector<BasicBlock> blocks;
    uint32_t entry_block;
    std::vector<uint32_t> exit_blocks;
    
    // Dominator tree
    std::vector<uint32_t> idom;  // Immediate dominator
    std::vector<std::vector<uint32_t>> dom_children;
    
    // Loop info
    struct LoopNest {
        uint32_t header;
        std::vector<uint32_t> body;
        uint32_t back_edge_block;
        uint32_t depth;
    };
    std::vector<LoopNest> loops;
};

class OptimizingCompiler {
public:
    OptimizingCompiler(const OptimizationFlags& flags);
    ~OptimizingCompiler();
    
    bool Initialize(void* code_cache, size_t cache_size);
    void Shutdown();
    
    CompiledBlockV8* Compile(const uint8_t* ppc_code, uint64_t address, size_t size,
                              const HotspotProfile* profile = nullptr);
    
    size_t GetCacheUsage() const { return cache_used_; }
    
private:
    // Frontend: PPC → SSA IR
    CFG BuildCFG(const uint8_t* code, uint64_t address, size_t size);
    void ConvertToSSA(CFG& cfg);
    
    // Middle-end: Optimizations
    void RunOptimizationPasses(CFG& cfg);
    void ConstantFolding(CFG& cfg);
    void DeadCodeElimination(CFG& cfg);
    void CommonSubexpressionElimination(CFG& cfg);
    void LoopInvariantCodeMotion(CFG& cfg);
    void InductionVariableOptimization(CFG& cfg);
    void LoopUnrolling(CFG& cfg);
    void Vectorization(CFG& cfg);
    void StrengthReduction(CFG& cfg);
    void TailDuplication(CFG& cfg);
    
    // Backend: SSA IR → ARM64
    void* EmitARM64(const CFG& cfg);
    void RegisterAllocation(CFG& cfg);
    void InstructionScheduling(CFG& cfg);
    void EmitPrologue(void*& emit_ptr);
    void EmitEpilogue(void*& emit_ptr);
    void EmitBlock(const BasicBlock& block, void*& emit_ptr);
    void EmitInstruction(const SSAInstr& instr, void*& emit_ptr);
    
    // SVE2 vectorization
    void EmitSVE2VectorOp(const SSAInstr& instr, void*& emit_ptr);
    void EmitNEONVectorOp(const SSAInstr& instr, void*& emit_ptr);
    
    OptimizationFlags flags_;
    void* code_cache_;
    size_t cache_size_;
    size_t cache_used_;
    
    // Register allocator state
    struct RegAllocState {
        int gp_regs[32];     // Which SSA value is in each GP reg
        int fp_regs[32];     // Which SSA value is in each FP reg
        int vec_regs[32];    // Which SSA value is in each vector reg
    };
    RegAllocState reg_state_;
    
    std::mutex compile_mutex_;
};

// ============================================================================
// Background Compilation Thread
// ============================================================================
class BackgroundCompiler {
public:
    BackgroundCompiler(BaselineCompiler* baseline, OptimizingCompiler* optimizing);
    ~BackgroundCompiler();
    
    void Start();
    void Stop();
    
    // Queue for background compilation
    void QueueForOptimization(const uint8_t* code, uint64_t address, size_t size,
                               const HotspotProfile& profile);
    
    // Check if compilation is ready
    CompiledBlockV8* GetCompiledBlock(uint64_t address);
    
private:
    void CompilerThread();
    
    BaselineCompiler* baseline_;
    OptimizingCompiler* optimizing_;
    
    struct CompileJob {
        const uint8_t* code;
        uint64_t address;
        size_t size;
        HotspotProfile profile;
    };
    
    std::queue<CompileJob> job_queue_;
    std::unordered_map<uint64_t, CompiledBlockV8*> completed_;
    
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread compile_thread_;
    std::atomic<bool> running_{false};
};

// ============================================================================
// Tiered Compilation Manager
// ============================================================================
class TieredCompilationManager {
public:
    TieredCompilationManager(const OptimizationFlags& flags);
    ~TieredCompilationManager();
    
    bool Initialize();
    void Shutdown();
    
    // Get or compile code at address
    CompiledBlockV8* GetOrCompile(const uint8_t* code, uint64_t address, size_t size);
    
    // Record execution for tier-up decision
    void RecordExecution(uint64_t address);
    
    // Force tier-up
    void ForceTierUp(uint64_t address);
    
    // Invalidate
    void Invalidate(uint64_t address, size_t size);
    void InvalidateAll();
    
    // Stats
    const ProfilingStats& GetStats() const { return stats_; }
    
    // Callbacks
    void SetTierUpCallback(TierUpCallback callback) { tier_up_callback_ = callback; }
    
private:
    void CheckTierUp(uint64_t address);
    
    OptimizationFlags flags_;
    
    std::unique_ptr<BaselineCompiler> baseline_;
    std::unique_ptr<OptimizingCompiler> optimizing_;
    std::unique_ptr<BackgroundCompiler> background_;
    
    // Code cache
    void* code_cache_;
    size_t code_cache_size_;
    
    // Block cache
    std::unordered_map<uint64_t, std::unique_ptr<CompiledBlockV8>> block_cache_;
    std::mutex cache_mutex_;
    
    // Profiling
    std::unordered_map<uint64_t, HotspotProfile> profiles_;
    std::mutex profile_mutex_;
    
    ProfilingStats stats_;
    TierUpCallback tier_up_callback_;
};

} // namespace rpcsx::nce::v8

#endif // RPCSX_NCE_V8_TIERED_JIT_H
