/**
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║                         NCE v8 - Ultimate Edition                        ║
 * ║            Native Code Execution Engine for ARM64 / Cortex-X4            ║
 * ║                                                                          ║
 * ║  🚀 Multi-tier JIT: Interpreter → Baseline → Optimizing                  ║
 * ║  ⚡ Hotspot Detection with Profile-Guided Optimization                   ║
 * ║  🧠 Advanced Branch Prediction + Speculative Execution                   ║
 * ║  🔥 Loop Optimization + Auto-Vectorization (SVE2/NEON)                   ║
 * ║  💾 L1/L2 Cache-Aware Memory Prefetching                                 ║
 * ║  🛡️ Crash Recovery + JIT Signal Handler                                  ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 */

#ifndef RPCSX_NCE_V8_H
#define RPCSX_NCE_V8_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <functional>

namespace rpcsx::nce::v8 {

// ============================================================================
// Version Info
// ============================================================================
constexpr int NCE_VERSION_MAJOR = 8;
constexpr int NCE_VERSION_MINOR = 0;
constexpr int NCE_VERSION_PATCH = 0;
constexpr const char* NCE_VERSION_STRING = "8.0.0-ultimate";
constexpr const char* NCE_CODENAME = "Thunderbolt";

// ============================================================================
// Compilation Tiers
// ============================================================================
enum class CompilationTier : uint8_t {
    INTERPRETER = 0,      // Slowest, immediate execution
    BASELINE_JIT = 1,     // Fast compile, moderate speed (~10x)
    OPTIMIZING_JIT = 2,   // SSA compile, good speed (~50x)
    TIER_3_LLVM = 3,      // LLVM backend, maximum speed (~100-200x)
    MEGAMORPHIC = 4,      // Polymorphic call site optimization
};

// ============================================================================
// Optimization Flags
// ============================================================================
struct OptimizationFlags {
    // Tier selection
    bool enable_tiered_compilation = true;
    uint32_t tier_up_threshold = 100;        // Executions before tier-up
    uint32_t osr_threshold = 1000;           // On-Stack Replacement threshold
    
    // Branch optimization
    bool enable_branch_prediction = true;
    bool enable_speculative_execution = true;
    uint32_t branch_history_size = 64;
    
    // Loop optimization
    bool enable_loop_unrolling = true;
    bool enable_loop_vectorization = true;
    uint32_t max_unroll_factor = 8;
    
    // Memory optimization
    bool enable_prefetching = true;
    bool enable_memory_coalescing = true;
    uint32_t prefetch_distance = 256;
    
    // Inline caching
    bool enable_inline_caching = true;
    uint32_t max_ic_entries = 8;
    
    // Register allocation
    bool enable_linear_scan_ra = true;
    bool enable_register_coalescing = true;
    
    // Dead code elimination
    bool enable_dce = true;
    bool enable_constant_folding = true;
    bool enable_strength_reduction = true;
    
    // Platform-specific
    bool enable_sve2_vectorization = true;
    bool enable_neon_fallback = true;
    bool enable_lse_atomics = true;          // Large System Extensions
    bool enable_mte = false;                 // Memory Tagging Extension
};

// ============================================================================
// Profiling Data
// ============================================================================
struct HotspotProfile {
    uint64_t address;
    uint64_t execution_count;
    uint64_t cycle_count;
    uint32_t branch_taken_count;
    uint32_t branch_not_taken_count;
    CompilationTier current_tier;
    bool is_loop_header;
    bool is_polymorphic;
};

struct ProfilingStats {
    std::atomic<uint64_t> total_executions{0};
    std::atomic<uint64_t> total_compilations{0};
    std::atomic<uint64_t> tier_ups{0};
    std::atomic<uint64_t> tier_downs{0};
    std::atomic<uint64_t> osr_entries{0};
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> branch_mispredictions{0};
    std::atomic<uint64_t> deoptimizations{0};
    std::atomic<uint64_t> inline_cache_hits{0};
    std::atomic<uint64_t> inline_cache_misses{0};
};

// ============================================================================
// Compiled Code Block (Enhanced)
// ============================================================================
struct CompiledBlockV8 {
    void* native_code;
    size_t code_size;
    
    uint64_t guest_address;
    size_t guest_size;
    
    CompilationTier tier;
    uint64_t execution_count;
    uint64_t last_execution_time;
    
    // Branch prediction data
    struct BranchInfo {
        uint64_t address;
        uint64_t target;
        uint32_t taken_count;
        uint32_t not_taken_count;
        bool predicted_taken;
    };
    std::vector<BranchInfo> branches;
    
    // Inline cache entries
    struct ICEntry {
        uint64_t expected_type;
        void* cached_handler;
        uint32_t hit_count;
    };
    std::vector<ICEntry> inline_caches;
    
    // Loop info
    struct LoopInfo {
        uint64_t header_addr;
        uint64_t back_edge_addr;
        uint32_t iteration_count;
        bool is_vectorizable;
        uint8_t unroll_factor;
    };
    std::vector<LoopInfo> loops;
    
    // Deoptimization info
    bool has_deopt_points;
    std::vector<uint64_t> deopt_addresses;
};

// ============================================================================
// NCE v8 Engine Interface
// ============================================================================

/**
 * Initialize NCE v8 with maximum optimizations
 */
bool Initialize(const OptimizationFlags& flags = OptimizationFlags{});

/**
 * Shutdown and cleanup
 */
void Shutdown();

/**
 * Check if NCE v8 is active
 */
bool IsActive();

/**
 * Get version string
 */
const char* GetVersionString();

/**
 * Set compilation tier
 */
void SetCompilationTier(CompilationTier tier);
CompilationTier GetCompilationTier();

/**
 * Compile PowerPC code to ARM64
 */
CompiledBlockV8* Compile(const uint8_t* ppc_code, uint64_t address, size_t size);

/**
 * Execute compiled block
 */
void Execute(CompiledBlockV8* block, void* cpu_state);

/**
 * On-Stack Replacement: transition from interpreter to JIT mid-loop
 */
bool PerformOSR(uint64_t loop_header, void* cpu_state);

/**
 * Deoptimize block and fall back to lower tier
 */
void Deoptimize(CompiledBlockV8* block);

/**
 * Invalidate code at address
 */
void InvalidateCode(uint64_t address, size_t size);

/**
 * Flush all caches
 */
void FlushAllCaches();

/**
 * Get profiling statistics
 */
const ProfilingStats& GetStats();

/**
 * Get hotspot profile for address
 */
HotspotProfile* GetHotspotProfile(uint64_t address);

/**
 * Memory prefetch hint
 */
void PrefetchMemory(const void* address, size_t size, bool for_write = false);

/**
 * Branch prediction hint
 */
void RecordBranch(uint64_t address, bool taken);

// ============================================================================
// Advanced Features
// ============================================================================

/**
 * Enable/disable speculative execution
 */
void SetSpeculativeExecution(bool enable);

/**
 * Register callback for tier-up events
 */
using TierUpCallback = std::function<void(uint64_t address, CompilationTier old_tier, CompilationTier new_tier)>;
void SetTierUpCallback(TierUpCallback callback);

/**
 * Force tier-up for hot code
 */
void ForceTierUp(uint64_t address);

/**
 * Get loop optimization info
 */
bool AnalyzeLoop(uint64_t header_addr, CompiledBlockV8::LoopInfo* out_info);

/**
 * Apply SVE2 vectorization to loop
 */
bool VectorizeLoop(CompiledBlockV8* block, const CompiledBlockV8::LoopInfo& loop);

// ============================================================================
// Platform Detection
// ============================================================================

struct PlatformCapabilities {
    bool has_sve2;
    bool has_sve2p1;
    bool has_sme;        // Scalable Matrix Extension
    bool has_mte;        // Memory Tagging Extension
    bool has_lse;        // Large System Extensions (atomics)
    bool has_bf16;       // BFloat16
    bool has_i8mm;       // Int8 Matrix Multiply
    bool has_fhm;        // FP16 multiplication (FHM)
    uint32_t sve_vector_length;
    uint32_t l1d_cache_size;
    uint32_t l1i_cache_size;
    uint32_t l2_cache_size;
    uint32_t l3_cache_size;
    uint32_t num_cores;
    uint32_t big_core_mask;
    uint32_t little_core_mask;
};

PlatformCapabilities DetectPlatform();

} // namespace rpcsx::nce::v8

#endif // RPCSX_NCE_V8_H
