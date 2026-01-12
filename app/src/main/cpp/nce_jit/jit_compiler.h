/**
 * JIT Compiler - PowerPC → ARM64 Translator
 * Транслює блоки PS3 Cell PPU коду в нативний ARM64
 */

#ifndef RPCSX_JIT_COMPILER_H
#define RPCSX_JIT_COMPILER_H

#include "ppc_decoder.h"
#include "arm64_emitter.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <sys/mman.h>

namespace rpcsx::nce {

using namespace ppc;
using namespace arm64;

// Compiled code block
struct CompiledBlock {
    void* code;              // Pointer to generated ARM64 code
    size_t code_size;        // Size of generated code
    uint64_t guest_addr;     // Original PowerPC address
    size_t guest_size;       // Original PowerPC code size (always 4-byte aligned)
    uint64_t entry_count;    // Execution count
};

/**
 * JIT Compiler Configuration
 */
struct JitConfig {
    size_t code_cache_size = 64 * 1024 * 1024;  // 64MB code cache
    size_t max_block_size = 4096;                // Max guest bytes per block
    bool enable_block_linking = true;
    bool enable_fast_memory = true;
    bool enable_profiling = false;
    bool big_endian_memory = true;               // PS3 is big-endian
};

/**
 * PowerPC PPU CPU State (PS3 Cell)
 * 
 * Cell PPU Architecture:
 * - 32 GPR (r0-r31) - 64-bit General Purpose Registers
 * - 32 FPR (f0-f31) - 64-bit Floating Point Registers  
 * - 32 VR (v0-v31) - 128-bit Vector Registers (AltiVec/VMX)
 * - CR - Condition Register (8 x 4-bit fields)
 * - LR - Link Register (branch return address)
 * - CTR - Count Register (loop counter)
 * - XER - Fixed-point Exception Register (carry, overflow)
 * - FPSCR - Floating-Point Status/Control Register
 * - VSCR - Vector Status/Control Register
 */
struct PPCState {
    // General Purpose Registers (r0-r31)
    uint64_t gpr[32];
    
    // Floating Point Registers (f0-f31)
    alignas(8) double fpr[32];
    
    // Vector Registers (v0-v31) - AltiVec/VMX
    alignas(16) uint8_t vr[32][16];
    
    // Special Purpose Registers
    uint64_t pc;             // Program Counter (NIA - Next Instruction Address)
    uint64_t lr;             // Link Register
    uint64_t ctr;            // Count Register
    uint32_t cr;             // Condition Register
    uint32_t xer;            // Fixed-Point Exception Register
    uint32_t fpscr;          // Floating-Point Status/Control Register
    uint32_t vscr;           // Vector Status/Control Register
    uint32_t vrsave;         // Vector Save Register
    
    // Memory base for fast memory access
    void* memory_base;
    size_t memory_size;
    
    // MSR fields (Machine State Register)
    bool msr_pr;             // Problem state (0=supervisor, 1=user)
    bool msr_ee;             // External interrupt enable
    bool msr_fp;             // FP available
    bool msr_vec;            // AltiVec available
    
    // ============================================
    // Condition Register Helpers
    // ============================================
    
    // CR is split into 8 fields (CR0-CR7), each 4 bits
    // Each field: LT(0), GT(1), EQ(2), SO(3)
    
    uint8_t GetCRField(int field) const {
        return (cr >> (28 - field * 4)) & 0xF;
    }
    
    void SetCRField(int field, uint8_t value) {
        uint32_t mask = 0xF << (28 - field * 4);
        cr = (cr & ~mask) | ((value & 0xF) << (28 - field * 4));
    }
    
    bool GetCRBit(int bit) const {
        return (cr >> (31 - bit)) & 1;
    }
    
    void SetCRBit(int bit, bool value) {
        if (value) {
            cr |= (1 << (31 - bit));
        } else {
            cr &= ~(1 << (31 - bit));
        }
    }
    
    // Set CR field from comparison result
    void SetCRFieldFromCompare(int field, int64_t a, int64_t b) {
        uint8_t flags = 0;
        if (a < b) flags |= 8;      // LT
        else if (a > b) flags |= 4; // GT
        else flags |= 2;            // EQ
        if (GetXER_SO()) flags |= 1; // SO (copy from XER)
        SetCRField(field, flags);
    }
    
    // ============================================
    // XER Helpers
    // ============================================
    
    // XER format: SO(0), OV(1), CA(2), bits 3-24 reserved, 
    //             byte count(25-31) for string instructions
    
    bool GetXER_SO() const { return (xer >> 31) & 1; }
    bool GetXER_OV() const { return (xer >> 30) & 1; }
    bool GetXER_CA() const { return (xer >> 29) & 1; }
    
    void SetXER_SO(bool v) { xer = v ? (xer | (1 << 31)) : (xer & ~(1u << 31)); }
    void SetXER_OV(bool v) { 
        xer = v ? (xer | (1 << 30)) : (xer & ~(1u << 30)); 
        if (v) SetXER_SO(true); // OV sets SO
    }
    void SetXER_CA(bool v) { xer = v ? (xer | (1 << 29)) : (xer & ~(1u << 29)); }
    
    // ============================================
    // Stack Pointer (r1 by convention)
    // ============================================
    
    uint64_t& GetSP() { return gpr[1]; }
    const uint64_t& GetSP() const { return gpr[1]; }
    
    // r13 is Small Data Area pointer on PPC64
    uint64_t& GetTOC() { return gpr[2]; }
};

/**
 * JIT Compiler - Main Translation Engine for PS3 PPU
 */
class JitCompiler {
public:
    explicit JitCompiler(const JitConfig& config = JitConfig());
    ~JitCompiler();
    
    // Initialize compiler
    bool Initialize();
    void Shutdown();
    
    // Compile and execute
    CompiledBlock* CompileBlock(const uint8_t* guest_code, uint64_t guest_addr);
    void Execute(PPCState* state, CompiledBlock* block);
    
    // Cache management
    CompiledBlock* LookupBlock(uint64_t guest_addr);
    void InvalidateBlock(uint64_t guest_addr);
    void InvalidateRange(uint64_t start, uint64_t end);
    void FlushCache();
    
    // Stats
    size_t GetCacheUsage() const { return code_cache_used_; }
    size_t GetBlockCount() const { return block_cache_.size(); }
    uint64_t GetExecutionCount() const { return total_executions_; }
    
private:
    // Translate single instruction
    bool TranslateInstruction(const ppc::DecodedInstr& instr, PPCTranslator& translator,
                              uint64_t guest_addr, uint64_t next_addr);
    
    // Map PowerPC CR condition to ARM64 condition
    Cond TranslatePPCCondition(uint8_t bi, bool branch_true);
    
    // Emit CR update code
    void EmitCR0Update(Emitter& emit, GpReg result);
    
    JitConfig config_;
    
    // Code cache
    void* code_cache_;
    size_t code_cache_size_;
    size_t code_cache_used_;
    
    // Block cache: guest_addr -> compiled block
    std::unordered_map<uint64_t, std::unique_ptr<CompiledBlock>> block_cache_;
    
    // Stats
    uint64_t total_executions_;
    uint64_t total_instructions_;
    
    bool initialized_;
};

// =========================================
// Implementation
// =========================================

inline JitCompiler::JitCompiler(const JitConfig& config)
    : config_(config)
    , code_cache_(nullptr)
    , code_cache_size_(0)
    , code_cache_used_(0)
    , total_executions_(0)
    , total_instructions_(0)
    , initialized_(false) {}

inline JitCompiler::~JitCompiler() {
    Shutdown();
}

inline bool JitCompiler::Initialize() {
    if (initialized_) return true;
    
    // Allocate executable code cache
    code_cache_size_ = config_.code_cache_size;
    code_cache_ = mmap(nullptr, code_cache_size_,
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (code_cache_ == MAP_FAILED) {
        code_cache_ = nullptr;
        return false;
    }
    
    code_cache_used_ = 0;
    initialized_ = true;
    return true;
}

inline void JitCompiler::Shutdown() {
    if (!initialized_) return;
    
    FlushCache();
    
    if (code_cache_) {
        munmap(code_cache_, code_cache_size_);
        code_cache_ = nullptr;
    }
    
    initialized_ = false;
}

inline CompiledBlock* JitCompiler::LookupBlock(uint64_t guest_addr) {
    auto it = block_cache_.find(guest_addr);
    if (it != block_cache_.end()) {
        return it->second.get();
    }
    return nullptr;
}

inline void JitCompiler::InvalidateBlock(uint64_t guest_addr) {
    block_cache_.erase(guest_addr);
}

inline void JitCompiler::InvalidateRange(uint64_t start, uint64_t end) {
    for (auto it = block_cache_.begin(); it != block_cache_.end(); ) {
        if (it->first >= start && it->first < end) {
            it = block_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

inline void JitCompiler::FlushCache() {
    block_cache_.clear();
    code_cache_used_ = 0;
}

inline Cond JitCompiler::TranslatePPCCondition(uint8_t bi, bool branch_true) {
    // bi is the CR bit index (0-31)
    // Each CR field has 4 bits: LT(0), GT(1), EQ(2), SO(3)
    int bit_in_field = bi & 3;
    
    // Map PPC CR bit to ARM64 condition
    // After a CMP, ARM64 flags: N=negative, Z=zero, C=carry, V=overflow
    switch (bit_in_field) {
        case 0: // LT (Less Than)
            return branch_true ? Cond::LT : Cond::GE;
        case 1: // GT (Greater Than)
            return branch_true ? Cond::GT : Cond::LE;
        case 2: // EQ (Equal)
            return branch_true ? Cond::EQ : Cond::NE;
        case 3: // SO (Summary Overflow)
            return branch_true ? Cond::VS : Cond::VC;
        default:
            return Cond::AL;
    }
}

inline void JitCompiler::EmitCR0Update(Emitter& emit, GpReg result) {
    // CR0 update: compare result with 0 and set LT/GT/EQ/SO
    // This is done automatically by ADDS/SUBS in ARM64
    // We just need to store the flags to CR
    // For now, rely on ARM64 flags being set by previous operation
}

inline CompiledBlock* JitCompiler::CompileBlock(const uint8_t* guest_code, 
                                                  uint64_t guest_addr) {
    if (!initialized_ || !guest_code) return nullptr;
    
    // Check if already compiled
    if (auto* existing = LookupBlock(guest_addr)) {
        return existing;
    }
    
    // Allocate code buffer
    void* code_start = static_cast<uint8_t*>(code_cache_) + code_cache_used_;
    size_t remaining = code_cache_size_ - code_cache_used_;
    
    if (remaining < 4096) {
        // Cache full - flush and restart
        FlushCache();
        code_start = code_cache_;
        remaining = code_cache_size_;
    }
    
    CodeBuffer buf(code_start, remaining);
    Emitter emit(buf);
    PPCTranslator translator(emit);
    
    // Decode and translate PowerPC instructions
    size_t guest_offset = 0;
    size_t instr_count = 0;
    bool block_end = false;
    
    // PowerPC instructions are always 4 bytes (32-bit fixed width)
    while (!block_end && guest_offset < config_.max_block_size) {
        // Decode PowerPC instruction (big-endian!)
        ppc::DecodedInstr instr = ppc::DecodeInstruction(
            guest_code + guest_offset, 
            guest_addr + guest_offset
        );
        
        if (instr.type == ppc::InstrType::UNKNOWN) {
            // Undecodable instruction - emit trap
            emit.BRK(0xFFFF);
            break;
        }
        
        uint64_t next_addr = guest_addr + guest_offset + 4;
        
        if (!TranslateInstruction(instr, translator, guest_addr + guest_offset, next_addr)) {
            // Translation failed - emit trap
            emit.BRK(0xFFFE);
            break;
        }
        
        guest_offset += 4;  // PPC instructions are always 4 bytes
        instr_count++;
        
        // Check for block-ending instructions
        if (ppc::IsBlockTerminator(instr.type)) {
            block_end = true;
        }
    }
    
    // Emit block epilogue - return to dispatcher
    emit.RET();
    
    // Create compiled block
    auto block = std::make_unique<CompiledBlock>();
    block->code = code_start;
    block->code_size = buf.GetOffset();
    block->guest_addr = guest_addr;
    block->guest_size = guest_offset;
    block->entry_count = 0;
    
    // Update cache usage (16-byte aligned)
    code_cache_used_ += (buf.GetOffset() + 15) & ~15;
    
    // Flush instruction cache
    __builtin___clear_cache(static_cast<char*>(code_start),
                            static_cast<char*>(code_start) + block->code_size);
    
    total_instructions_ += instr_count;
    
    CompiledBlock* result = block.get();
    block_cache_[guest_addr] = std::move(block);
    
    return result;
}

inline bool JitCompiler::TranslateInstruction(const ppc::DecodedInstr& instr,
                                               PPCTranslator& translator,
                                               uint64_t guest_addr,
                                               uint64_t next_addr) {
    // Use the PPCTranslator to emit ARM64 code for this PowerPC instruction
    return translator.Translate(instr);
}

inline void JitCompiler::Execute(PPCState* state, CompiledBlock* block) {
    if (!state || !block || !block->code) return;
    
    block->entry_count++;
    total_executions_++;
    
    // Setup registers from state before calling JIT code
    // The generated ARM64 code expects:
    // - X29 (REG_STATE) to point to PPCState
    // - X30 (LR) for return
    // - X22 (REG_CTR) = state->ctr
    // - X24 (REG_CR) = state->cr
    
    // Call generated code
    // Signature: void jit_block(PPCState* state)
    using JitFunc = void (*)(PPCState*);
    JitFunc func = reinterpret_cast<JitFunc>(block->code);
    
    func(state);
    
    // State is updated in-place by the JIT code
}

} // namespace rpcsx::nce

#endif // RPCSX_JIT_COMPILER_H
