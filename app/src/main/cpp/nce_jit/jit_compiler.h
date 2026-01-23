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
        existing->entry_count++;
        return existing;
    }
    
    // Allocate code buffer
    void* code_start = static_cast<uint8_t*>(code_cache_) + code_cache_used_;
    size_t remaining = code_cache_size_ - code_cache_used_;
    
    if (remaining < 8192) {
        // Cache full - flush and restart
        FlushCache();
        code_start = code_cache_;
        remaining = code_cache_size_;
    }
    
    // Create code buffer and emitter
    CodeBuffer codebuf(code_start, std::min(remaining, config_.max_block_size * 4));
    Emitter emit(codebuf);
    PPCTranslator translator;
    
    size_t guest_offset = 0;
    size_t instr_count = 0;
    bool block_end = false;
    
    // Compile PowerPC instructions until block end
    while (!block_end && guest_offset < config_.max_block_size && 
           codebuf.GetRemaining() > 256) {
        
        // Decode PowerPC instruction (big-endian on PS3)
        const uint8_t* instr_ptr = guest_code + guest_offset;
        uint32_t raw_instr;
        if (config_.big_endian_memory) {
            raw_instr = (static_cast<uint32_t>(instr_ptr[0]) << 24) |
                       (static_cast<uint32_t>(instr_ptr[1]) << 16) |
                       (static_cast<uint32_t>(instr_ptr[2]) << 8) |
                       static_cast<uint32_t>(instr_ptr[3]);
        } else {
            memcpy(&raw_instr, instr_ptr, 4);
        }
        
        // Decode the instruction
        ppc::DecodedInstr instr = ppc::Decode(raw_instr);
        uint64_t current_addr = guest_addr + guest_offset;
        uint64_t next_addr = current_addr + 4;
        
        // Translate based on primary opcode
        switch (instr.primary) {
            case ppc::PrimaryOp::ADDI:
                // addi rD, rA, SIMM → add xD, xA, #imm (or mov if rA=0)
                if (instr.rA == 0) {
                    emit.MOVi(static_cast<GpReg>(instr.rD), instr.simm);
                } else {
                    emit.ADDi(static_cast<GpReg>(instr.rD), 
                             static_cast<GpReg>(instr.rA), instr.simm);
                }
                break;
                
            case ppc::PrimaryOp::ADDIS:
                // addis rD, rA, SIMM → add xD, xA, #(imm << 16)
                if (instr.rA == 0) {
                    emit.MOVi(static_cast<GpReg>(instr.rD), 
                             static_cast<int64_t>(instr.simm) << 16);
                } else {
                    emit.ADDi(static_cast<GpReg>(instr.rD), 
                             static_cast<GpReg>(instr.rA), 
                             static_cast<int64_t>(instr.simm) << 16);
                }
                break;
                
            case ppc::PrimaryOp::ORI:
                // ori rA, rS, UIMM → orr xA, xS, #imm
                if (instr.uimm == 0 && instr.rA == instr.rS) {
                    // NOP (ori r0, r0, 0)
                    emit.NOP();
                } else {
                    emit.ORRi(static_cast<GpReg>(instr.rA),
                             static_cast<GpReg>(instr.rS), instr.uimm);
                }
                break;
                
            case ppc::PrimaryOp::ORIS:
                emit.ORRi(static_cast<GpReg>(instr.rA),
                         static_cast<GpReg>(instr.rS), 
                         static_cast<uint64_t>(instr.uimm) << 16);
                break;
                
            case ppc::PrimaryOp::LWZ:
                // lwz rD, d(rA) → ldr wD, [xA, #d]
                emit.LDRWi(static_cast<GpReg>(instr.rD),
                          instr.rA == 0 ? GpReg::ZR : static_cast<GpReg>(instr.rA),
                          instr.simm);
                break;
                
            case ppc::PrimaryOp::STW:
                // stw rS, d(rA) → str wS, [xA, #d]
                emit.STRWi(static_cast<GpReg>(instr.rS),
                          instr.rA == 0 ? GpReg::ZR : static_cast<GpReg>(instr.rA),
                          instr.simm);
                break;
                
            case ppc::PrimaryOp::B:
                // Branch - end of block
                emit.B(instr.li);
                block_end = true;
                break;
                
            case ppc::PrimaryOp::BC:
                // Branch conditional
                emit.Bcond(TranslatePPCCondition(instr.bi, (instr.bo & 8) != 0), 
                          instr.bd);
                if (instr.bo == 20) { // Unconditional
                    block_end = true;
                }
                break;
                
            case ppc::PrimaryOp::OP31:
                // Extended ALU operations
                TranslateOp31(emit, instr);
                break;
                
            default:
                // Unsupported - emit breakpoint or interpreter call
                emit.BRK(static_cast<uint16_t>(instr.primary));
                break;
        }
        
        guest_offset += 4;
        instr_count++;
        total_instructions_++;
    }
    
    // Emit block epilogue - return to dispatcher
    emit.RET();
    
    // Finalize block
    size_t code_size = codebuf.GetOffset();
    
    // Make code executable
    __builtin___clear_cache(static_cast<char*>(code_start),
                            static_cast<char*>(code_start) + code_size);
    
    auto block = std::make_unique<CompiledBlock>();
    block->code = code_start;
    block->code_size = code_size;
    block->guest_addr = guest_addr;
    block->guest_size = guest_offset;
    block->entry_count = 1;
    
    // Update cache usage (align to 16 bytes)
    code_cache_used_ += (code_size + 15) & ~15;
    
    CompiledBlock* result = block.get();
    block_cache_[guest_addr] = std::move(block);
    
    return result;
}

// Translate OP31 extended ALU instructions
inline void TranslateOp31(Emitter& emit, const ppc::DecodedInstr& instr) {
    switch (static_cast<ppc::ExtOp31>(instr.xo)) {
        case ppc::ExtOp31::ADD:
            emit.ADD(static_cast<GpReg>(instr.rD),
                    static_cast<GpReg>(instr.rA),
                    static_cast<GpReg>(instr.rB));
            break;
        case ppc::ExtOp31::SUBF:
            emit.SUB(static_cast<GpReg>(instr.rD),
                    static_cast<GpReg>(instr.rB),  // Note: sub rD, rB, rA
                    static_cast<GpReg>(instr.rA));
            break;
        case ppc::ExtOp31::AND:
            emit.AND(static_cast<GpReg>(instr.rA),
                    static_cast<GpReg>(instr.rS),
                    static_cast<GpReg>(instr.rB));
            break;
        case ppc::ExtOp31::OR:
            emit.ORR(static_cast<GpReg>(instr.rA),
                    static_cast<GpReg>(instr.rS),
                    static_cast<GpReg>(instr.rB));
            break;
        case ppc::ExtOp31::XOR:
            emit.EOR(static_cast<GpReg>(instr.rA),
                    static_cast<GpReg>(instr.rS),
                    static_cast<GpReg>(instr.rB));
            break;
        case ppc::ExtOp31::CMP:
            emit.CMP(static_cast<GpReg>(instr.rA),
                    static_cast<GpReg>(instr.rB));
            break;
        case ppc::ExtOp31::LBZX:
            emit.LDRB(static_cast<GpReg>(instr.rD),
                     static_cast<GpReg>(instr.rA),
                     static_cast<GpReg>(instr.rB));
            break;
        case ppc::ExtOp31::STWX:
            emit.STRW(static_cast<GpReg>(instr.rS),
                     static_cast<GpReg>(instr.rA),
                     static_cast<GpReg>(instr.rB));
            break;
        default:
            emit.BRK(static_cast<uint16_t>(instr.xo));
            break;
    }
}

inline bool JitCompiler::TranslateInstruction(const ppc::DecodedInstr& instr,
                                               PPCTranslator& translator,
                                               uint64_t guest_addr,
                                               uint64_t next_addr) {
    // This is now handled inline in CompileBlock for better performance
    return true;
}

// Function pointer type for JIT compiled code
typedef void (*JitCodePtr)(PPCState*);

inline void JitCompiler::Execute(PPCState* state, CompiledBlock* block) {
    if (!state || !block || !block->code) return;
    
    block->entry_count++;
    total_executions_++;
    
    // Cast compiled code to function pointer and execute
    // The generated code expects PPCState* in X0
    JitCodePtr code_fn = reinterpret_cast<JitCodePtr>(block->code);
    
    // Execute the compiled ARM64 code
    // This will modify state->gpr, state->pc, etc.
    code_fn(state);
    
    // Update PC to next instruction after the block
    state->pc = block->guest_addr + block->guest_size;
}

} // namespace rpcsx::nce

#endif // RPCSX_JIT_COMPILER_H
