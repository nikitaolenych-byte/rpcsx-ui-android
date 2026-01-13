/**
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║                    NCE v8 - SVE2 Vectorization Engine                    ║
 * ║                                                                          ║
 * ║  Automatic vectorization of loops using ARM SVE2/NEON                   ║
 * ║  - Loop dependence analysis                                              ║
 * ║  - SLP (Superword Level Parallelism) vectorization                      ║
 * ║  - Predicated execution for tail loops                                   ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 */

#ifndef RPCSX_NCE_V8_VECTORIZER_H
#define RPCSX_NCE_V8_VECTORIZER_H

#include <cstdint>
#include <vector>
#include <array>

#if defined(__aarch64__)
#include <arm_neon.h>
#if __has_include(<arm_sve.h>)
#include <arm_sve.h>
#define HAS_SVE2 1
#else
#define HAS_SVE2 0
#endif
#endif

namespace rpcsx::nce::v8 {

// ============================================================================
// Vector Operation Types
// ============================================================================
enum class VecOp {
    // Integer operations
    ADD_I8, ADD_I16, ADD_I32, ADD_I64,
    SUB_I8, SUB_I16, SUB_I32, SUB_I64,
    MUL_I8, MUL_I16, MUL_I32, MUL_I64,
    
    // Saturating arithmetic
    ADDS_I8, ADDS_I16, ADDS_I32,
    SUBS_I8, SUBS_I16, SUBS_I32,
    
    // Floating point
    ADD_F32, ADD_F64,
    SUB_F32, SUB_F64,
    MUL_F32, MUL_F64,
    DIV_F32, DIV_F64,
    FMA_F32, FMA_F64,        // Fused multiply-add
    
    // Comparison
    CMPEQ_I8, CMPEQ_I16, CMPEQ_I32, CMPEQ_I64,
    CMPGT_I8, CMPGT_I16, CMPGT_I32, CMPGT_I64,
    CMPLT_F32, CMPLT_F64,
    
    // Logical
    AND, OR, XOR, NOT,
    
    // Shift
    SHL_I8, SHL_I16, SHL_I32, SHL_I64,
    SHR_I8, SHR_I16, SHR_I32, SHR_I64,
    
    // Permute
    ZIP, UZP, TRN, REV, TBL,
    
    // Reduction
    ADDV_I8, ADDV_I16, ADDV_I32,     // Horizontal add
    MAXV_I8, MAXV_I16, MAXV_I32,     // Horizontal max
    MINV_I8, MINV_I16, MINV_I32,     // Horizontal min
    
    // Memory
    LOAD, LOAD_GATHER,
    STORE, STORE_SCATTER,
    
    // Broadcast
    DUP, MOV,
};

// ============================================================================
// Loop Vectorization Info
// ============================================================================
struct VectorizationPlan {
    enum class Strategy {
        NONE,           // Cannot vectorize
        NEON_128,       // Use 128-bit NEON
        SVE2_SCALABLE,  // Use scalable SVE2
        PREDICATED,     // Use SVE2 with predication
    };
    
    Strategy strategy;
    uint32_t vector_width;     // Elements per vector
    uint32_t unroll_factor;    // Additional unrolling
    bool needs_epilogue;       // Needs scalar epilogue for remainder
    bool needs_runtime_check;  // Needs runtime alias check
    
    // Estimated speedup
    float estimated_speedup;
    
    // Memory access pattern
    enum class AccessPattern {
        SEQUENTIAL,     // Contiguous access
        STRIDED,        // Fixed stride
        GATHER_SCATTER, // Random access
        UNKNOWN,
    };
    AccessPattern memory_pattern;
    int64_t stride;
};

// ============================================================================
// Dependence Analysis
// ============================================================================
struct MemoryDependence {
    enum class Type {
        NONE,           // No dependence
        RAW,            // Read-after-write (true dependence)
        WAR,            // Write-after-read (anti dependence)
        WAW,            // Write-after-write (output dependence)
    };
    
    Type type;
    int64_t distance;   // Loop iteration distance
    bool is_loop_carried;
};

class DependenceAnalyzer {
public:
    // Analyze memory dependences in a loop
    std::vector<MemoryDependence> Analyze(
        const std::vector<uint64_t>& load_addrs,
        const std::vector<uint64_t>& store_addrs,
        int64_t trip_count);
    
    // Check if vectorization is safe
    bool IsSafeToVectorize(const std::vector<MemoryDependence>& deps);
    
    // Get minimum safe vector width
    uint32_t GetSafeVectorWidth(const std::vector<MemoryDependence>& deps);
};

// ============================================================================
// SVE2 Code Generator
// ============================================================================
class SVE2CodeGenerator {
public:
    SVE2CodeGenerator(void* emit_buffer, size_t buffer_size);
    
    // Get current position
    void* GetCurrentPos() const { return emit_ptr_; }
    size_t GetCodeSize() const { return static_cast<uint8_t*>(emit_ptr_) - static_cast<uint8_t*>(buffer_); }
    
    // ========== Predicate operations ==========
    void PTRUE(uint8_t pd, uint8_t pattern = 31);  // All-true predicate
    void PFALSE(uint8_t pd);
    void WHILELT(uint8_t pd, uint8_t rn, uint8_t rm, uint8_t size);
    void PTEST(uint8_t pg, uint8_t pn);
    void PNEXT(uint8_t pd, uint8_t pg, uint8_t pn, uint8_t size);
    
    // ========== Load/Store ==========
    void LD1B(uint8_t zt, uint8_t pg, uint8_t rn, int64_t imm = 0);
    void LD1H(uint8_t zt, uint8_t pg, uint8_t rn, int64_t imm = 0);
    void LD1W(uint8_t zt, uint8_t pg, uint8_t rn, int64_t imm = 0);
    void LD1D(uint8_t zt, uint8_t pg, uint8_t rn, int64_t imm = 0);
    
    void ST1B(uint8_t zt, uint8_t pg, uint8_t rn, int64_t imm = 0);
    void ST1H(uint8_t zt, uint8_t pg, uint8_t rn, int64_t imm = 0);
    void ST1W(uint8_t zt, uint8_t pg, uint8_t rn, int64_t imm = 0);
    void ST1D(uint8_t zt, uint8_t pg, uint8_t rn, int64_t imm = 0);
    
    // Gather/Scatter
    void LD1W_GATHER(uint8_t zt, uint8_t pg, uint8_t rn, uint8_t zm);
    void ST1W_SCATTER(uint8_t zt, uint8_t pg, uint8_t rn, uint8_t zm);
    
    // ========== Integer arithmetic ==========
    void ADD_Z(uint8_t zd, uint8_t zn, uint8_t zm, uint8_t size);
    void SUB_Z(uint8_t zd, uint8_t zn, uint8_t zm, uint8_t size);
    void MUL_Z(uint8_t zd, uint8_t zn, uint8_t zm, uint8_t size);
    
    // Predicated operations
    void ADD_Z_P(uint8_t zd, uint8_t pg, uint8_t zn, uint8_t zm, uint8_t size);
    void SUB_Z_P(uint8_t zd, uint8_t pg, uint8_t zn, uint8_t zm, uint8_t size);
    
    // ========== Floating point ==========
    void FADD_Z(uint8_t zd, uint8_t zn, uint8_t zm, uint8_t size);
    void FSUB_Z(uint8_t zd, uint8_t zn, uint8_t zm, uint8_t size);
    void FMUL_Z(uint8_t zd, uint8_t zn, uint8_t zm, uint8_t size);
    void FDIV_Z(uint8_t zd, uint8_t pg, uint8_t zn, uint8_t zm, uint8_t size);
    void FMLA_Z(uint8_t zda, uint8_t zn, uint8_t zm, uint8_t size);  // zda += zn * zm
    void FMLS_Z(uint8_t zda, uint8_t zn, uint8_t zm, uint8_t size);  // zda -= zn * zm
    
    // ========== Comparison ==========
    void CMPEQ_Z(uint8_t pd, uint8_t pg, uint8_t zn, uint8_t zm, uint8_t size);
    void CMPGT_Z(uint8_t pd, uint8_t pg, uint8_t zn, uint8_t zm, uint8_t size);
    void CMPLT_Z(uint8_t pd, uint8_t pg, uint8_t zn, uint8_t zm, uint8_t size);
    void FCMLT_Z(uint8_t pd, uint8_t pg, uint8_t zn, uint8_t zm, uint8_t size);
    
    // ========== Logical ==========
    void AND_Z(uint8_t zd, uint8_t zn, uint8_t zm);
    void ORR_Z(uint8_t zd, uint8_t zn, uint8_t zm);
    void EOR_Z(uint8_t zd, uint8_t zn, uint8_t zm);
    void NOT_Z(uint8_t zd, uint8_t pg, uint8_t zn);
    
    // ========== Reduction ==========
    void ADDV(uint8_t vd, uint8_t pg, uint8_t zn, uint8_t size);    // Sum all elements
    void SMAXV(uint8_t vd, uint8_t pg, uint8_t zn, uint8_t size);   // Max (signed)
    void SMINV(uint8_t vd, uint8_t pg, uint8_t zn, uint8_t size);   // Min (signed)
    void FADDV(uint8_t vd, uint8_t pg, uint8_t zn, uint8_t size);   // FP sum
    void FMAXV(uint8_t vd, uint8_t pg, uint8_t zn, uint8_t size);   // FP max
    void FMINV(uint8_t vd, uint8_t pg, uint8_t zn, uint8_t size);   // FP min
    
    // ========== Permute ==========
    void DUP_Z(uint8_t zd, uint8_t rn, uint8_t size);     // Broadcast scalar to vector
    void INDEX_Z(uint8_t zd, int64_t start, int64_t step, uint8_t size);  // Create index vector
    void SEL_Z(uint8_t zd, uint8_t pg, uint8_t zn, uint8_t zm);  // Select elements
    void COMPACT(uint8_t zd, uint8_t pg, uint8_t zn, uint8_t size);  // Compress active elements
    void SPLICE(uint8_t zd, uint8_t pg, uint8_t zn, uint8_t zm);  // Concatenate vectors
    
    // ========== Increment ==========
    void INCB(uint8_t xd, uint8_t pattern = 31);  // Increment by vector length (bytes)
    void INCH(uint8_t xd, uint8_t pattern = 31);  // Increment by vector length (halfwords)
    void INCW(uint8_t xd, uint8_t pattern = 31);  // Increment by vector length (words)
    void INCD(uint8_t xd, uint8_t pattern = 31);  // Increment by vector length (doublewords)
    
    // ========== Counter operations ==========
    void CNTB(uint8_t rd, uint8_t pattern = 31);  // Count bytes in vector
    void CNTH(uint8_t rd, uint8_t pattern = 31);  // Count halfwords
    void CNTW(uint8_t rd, uint8_t pattern = 31);  // Count words
    void CNTD(uint8_t rd, uint8_t pattern = 31);  // Count doublewords
    
private:
    void Emit32(uint32_t instr);
    
    void* buffer_;
    void* emit_ptr_;
    size_t buffer_size_;
};

// ============================================================================
// NEON Fallback Code Generator
// ============================================================================
class NEONCodeGenerator {
public:
    NEONCodeGenerator(void* emit_buffer, size_t buffer_size);
    
    void* GetCurrentPos() const { return emit_ptr_; }
    size_t GetCodeSize() const { return static_cast<uint8_t*>(emit_ptr_) - static_cast<uint8_t*>(buffer_); }
    
    // Load/Store
    void LD1(uint8_t vt, uint8_t rn, uint8_t size);     // Load single structure
    void ST1(uint8_t vt, uint8_t rn, uint8_t size);     // Store single structure
    void LDP_Q(uint8_t vt1, uint8_t vt2, uint8_t rn);   // Load pair of Q registers
    void STP_Q(uint8_t vt1, uint8_t vt2, uint8_t rn);   // Store pair of Q registers
    
    // Arithmetic
    void ADD_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    void SUB_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    void MUL_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    
    // Floating point
    void FADD_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    void FSUB_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    void FMUL_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    void FMLA_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    
    // Comparison
    void CMEQ_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    void CMGT_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    void FCMLT_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    
    // Logical
    void AND_V(uint8_t vd, uint8_t vn, uint8_t vm, bool is_quad);
    void ORR_V(uint8_t vd, uint8_t vn, uint8_t vm, bool is_quad);
    void EOR_V(uint8_t vd, uint8_t vn, uint8_t vm, bool is_quad);
    void NOT_V(uint8_t vd, uint8_t vn, bool is_quad);
    
    // Permute
    void DUP_V(uint8_t vd, uint8_t rn, uint8_t size, bool is_quad);
    void ZIP1_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    void ZIP2_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    void TRN1_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    void TRN2_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad);
    
    // Reduction
    void ADDV_V(uint8_t vd, uint8_t vn, uint8_t size, bool is_quad);
    void SMAXV_V(uint8_t vd, uint8_t vn, uint8_t size, bool is_quad);
    void SMINV_V(uint8_t vd, uint8_t vn, uint8_t size, bool is_quad);
    void FMAXV_V(uint8_t vd, uint8_t vn, uint8_t size);
    void FMINV_V(uint8_t vd, uint8_t vn, uint8_t size);
    
private:
    void Emit32(uint32_t instr);
    
    void* buffer_;
    void* emit_ptr_;
    size_t buffer_size_;
};

// ============================================================================
// Loop Vectorizer
// ============================================================================
class LoopVectorizer {
public:
    LoopVectorizer(bool prefer_sve2 = true) : prefer_sve2_(prefer_sve2) {}
    
    // Analyze loop and create vectorization plan
    VectorizationPlan Analyze(
        uint64_t header_addr,
        uint64_t back_edge_addr,
        const std::vector<uint64_t>& load_addrs,
        const std::vector<uint64_t>& store_addrs,
        int64_t estimated_trip_count) {
        VectorizationPlan plan;
        plan.strategy = VectorizationPlan::Strategy::NONE;
        plan.vector_width = 4;  // Default NEON width
        plan.unroll_factor = 1;
        plan.needs_epilogue = false;
        plan.needs_runtime_check = false;
        plan.estimated_speedup = 1.0f;
        plan.memory_pattern = VectorizationPlan::AccessPattern::UNKNOWN;
        plan.stride = 0;
        
        // Check for dependencies
        auto deps = dep_analyzer_.Analyze(load_addrs, store_addrs, estimated_trip_count);
        if (dep_analyzer_.IsSafeToVectorize(deps)) {
            uint32_t safe_width = dep_analyzer_.GetSafeVectorWidth(deps);
            
            // Check if SVE2 is available and preferred
            if (prefer_sve2_) {
                #if defined(__ARM_FEATURE_SVE2)
                plan.strategy = VectorizationPlan::Strategy::SVE2_SCALABLE;
                plan.vector_width = 8;  // Variable, but assume 256-bit
                #else
                plan.strategy = VectorizationPlan::Strategy::NEON_128;
                plan.vector_width = 4;  // 128-bit
                #endif
            } else {
                plan.strategy = VectorizationPlan::Strategy::NEON_128;
                plan.vector_width = 4;
            }
            
            // Clamp to safe width
            if (plan.vector_width > safe_width) {
                plan.vector_width = safe_width;
            }
            
            // Calculate speedup estimate
            if (estimated_trip_count >= static_cast<int64_t>(plan.vector_width) * 2) {
                plan.estimated_speedup = plan.vector_width * 0.8f;  // Account for overhead
                plan.needs_epilogue = (estimated_trip_count % plan.vector_width) != 0;
            }
        }
        
        return plan;
    }
    
    // Generate vectorized code
    void* Vectorize(
        const VectorizationPlan& plan,
        const uint8_t* original_code,
        size_t code_size,
        void* emit_buffer,
        size_t buffer_size) {
        if (plan.strategy == VectorizationPlan::Strategy::NONE) return nullptr;
        // Implementation placeholder - would generate SVE2/NEON code
        return emit_buffer;
    }
    
    // Generate scalar epilogue
    void* EmitEpilogue(
        const VectorizationPlan& plan,
        const uint8_t* original_code,
        size_t code_size,
        void* emit_buffer,
        size_t buffer_size) {
        // Implementation placeholder
        return emit_buffer;
    }
    
private:
    bool prefer_sve2_;
    DependenceAnalyzer dep_analyzer_;
};

// ============================================================================
// SVE2 Implementation (inline)
// ============================================================================

inline SVE2CodeGenerator::SVE2CodeGenerator(void* emit_buffer, size_t buffer_size)
    : buffer_(emit_buffer), emit_ptr_(emit_buffer), buffer_size_(buffer_size) {}

inline void SVE2CodeGenerator::Emit32(uint32_t instr) {
    *static_cast<uint32_t*>(emit_ptr_) = instr;
    emit_ptr_ = static_cast<uint8_t*>(emit_ptr_) + 4;
}

inline void SVE2CodeGenerator::PTRUE(uint8_t pd, uint8_t pattern) {
    // PTRUE <Pd>.<T>, <pattern>
    // For size=S (32-bit): 00100101 10 0 11000 111 00 pattern Pd
    uint32_t instr = 0x2518E000 | (pattern << 5) | pd;
    Emit32(instr);
}

inline void SVE2CodeGenerator::WHILELT(uint8_t pd, uint8_t rn, uint8_t rm, uint8_t size) {
    // WHILELT <Pd>.<T>, <Xn>, <Xm>
    // 00100101 size 1 Rm 000 100 Rn 0 Pd
    uint32_t instr = 0x25200400 | (size << 22) | (rm << 16) | (rn << 5) | pd;
    Emit32(instr);
}

inline void SVE2CodeGenerator::LD1W(uint8_t zt, uint8_t pg, uint8_t rn, int64_t imm) {
    // LD1W { <Zt>.S }, <Pg>/Z, [<Xn|SP>{, #<imm>, MUL VL}]
    // 10100101 010 imm4 101 Pg Rn Zt
    uint32_t instr = 0xA540A000 | ((imm & 0xF) << 16) | (pg << 10) | (rn << 5) | zt;
    Emit32(instr);
}

inline void SVE2CodeGenerator::ST1W(uint8_t zt, uint8_t pg, uint8_t rn, int64_t imm) {
    // ST1W { <Zt>.S }, <Pg>, [<Xn|SP>{, #<imm>, MUL VL}]
    // 11100101 010 imm4 111 Pg Rn Zt
    uint32_t instr = 0xE540E000 | ((imm & 0xF) << 16) | (pg << 10) | (rn << 5) | zt;
    Emit32(instr);
}

inline void SVE2CodeGenerator::FADD_Z(uint8_t zd, uint8_t zn, uint8_t zm, uint8_t size) {
    // FADD <Zd>.<T>, <Zn>.<T>, <Zm>.<T>
    // 01100101 size 0 Zm 000 000 Zn Zd
    uint32_t instr = 0x65000000 | (size << 22) | (zm << 16) | (zn << 5) | zd;
    Emit32(instr);
}

inline void SVE2CodeGenerator::FMUL_Z(uint8_t zd, uint8_t zn, uint8_t zm, uint8_t size) {
    // FMUL <Zd>.<T>, <Zn>.<T>, <Zm>.<T>
    // 01100101 size 0 Zm 000 010 Zn Zd
    uint32_t instr = 0x65000800 | (size << 22) | (zm << 16) | (zn << 5) | zd;
    Emit32(instr);
}

inline void SVE2CodeGenerator::FMLA_Z(uint8_t zda, uint8_t zn, uint8_t zm, uint8_t size) {
    // FMLA <Zda>.<T>, <Zn>.<T>, <Zm>.<T>
    // 01100101 size 1 Zm 000 000 Zn Zda
    uint32_t instr = 0x65200000 | (size << 22) | (zm << 16) | (zn << 5) | zda;
    Emit32(instr);
}

inline void SVE2CodeGenerator::INCW(uint8_t xd, uint8_t pattern) {
    // INCW <Xdn>{, <pattern>{, MUL #<imm>}}
    // 00000100 10 1 0 pattern 11100 0 Rd
    uint32_t instr = 0x04A0E000 | (pattern << 5) | xd;
    Emit32(instr);
}

inline void SVE2CodeGenerator::CNTW(uint8_t rd, uint8_t pattern) {
    // CNTW <Xd>{, <pattern>{, MUL #<imm>}}
    // 00000100 10 1 0 pattern 11100 0 Rd
    uint32_t instr = 0x04A0E000 | (pattern << 5) | rd;
    Emit32(instr);
}

// ============================================================================
// NEON Implementation (inline)
// ============================================================================

inline NEONCodeGenerator::NEONCodeGenerator(void* emit_buffer, size_t buffer_size)
    : buffer_(emit_buffer), emit_ptr_(emit_buffer), buffer_size_(buffer_size) {}

inline void NEONCodeGenerator::Emit32(uint32_t instr) {
    *static_cast<uint32_t*>(emit_ptr_) = instr;
    emit_ptr_ = static_cast<uint8_t*>(emit_ptr_) + 4;
}

inline void NEONCodeGenerator::FADD_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad) {
    // FADD <Vd>.<T>, <Vn>.<T>, <Vm>.<T>
    // 0 Q 0 01110 0 sz 1 Rm 11010 1 Rn Rd
    uint32_t q = is_quad ? 1 : 0;
    uint32_t instr = 0x0E20D400 | (q << 30) | (size << 22) | (vm << 16) | (vn << 5) | vd;
    Emit32(instr);
}

inline void NEONCodeGenerator::FMUL_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad) {
    // FMUL <Vd>.<T>, <Vn>.<T>, <Vm>.<T>
    // 0 Q 1 01110 0 sz 1 Rm 11011 1 Rn Rd
    uint32_t q = is_quad ? 1 : 0;
    uint32_t instr = 0x2E20DC00 | (q << 30) | (size << 22) | (vm << 16) | (vn << 5) | vd;
    Emit32(instr);
}

inline void NEONCodeGenerator::FMLA_V(uint8_t vd, uint8_t vn, uint8_t vm, uint8_t size, bool is_quad) {
    // FMLA <Vd>.<T>, <Vn>.<T>, <Vm>.<T>
    // 0 Q 0 01110 0 sz 1 Rm 11001 1 Rn Rd
    uint32_t q = is_quad ? 1 : 0;
    uint32_t instr = 0x0E20CC00 | (q << 30) | (size << 22) | (vm << 16) | (vn << 5) | vd;
    Emit32(instr);
}

} // namespace rpcsx::nce::v8

#endif // RPCSX_NCE_V8_VECTORIZER_H
