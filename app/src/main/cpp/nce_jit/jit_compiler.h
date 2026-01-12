/**
 * JIT Compiler - x86-64 → ARM64 Translator
 * Транслює блоки PS4 коду в нативний ARM64
 */

#ifndef RPCSX_JIT_COMPILER_H
#define RPCSX_JIT_COMPILER_H

#include "x86_decoder.h"
#include "arm64_emitter.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <sys/mman.h>

namespace rpcsx::nce {

using namespace x86;
using namespace arm64;

// x86-64 to ARM64 register mapping
// PS4 uses System V AMD64 ABI
// ARM64 uses AAPCS64 ABI
constexpr GpReg kX86ToArm64[] = {
    GpReg::X0,   // RAX -> X0 (return value)
    GpReg::X9,   // RCX -> X9 (4th arg in x86, not in ARM)
    GpReg::X2,   // RDX -> X2 (3rd arg)
    GpReg::X19,  // RBX -> X19 (callee-saved)
    GpReg::SP,   // RSP -> SP
    GpReg::X29,  // RBP -> X29/FP
    GpReg::X1,   // RSI -> X1 (2nd arg)
    GpReg::X7,   // RDI -> X7 (1st arg)
    GpReg::X3,   // R8  -> X3 (5th arg)
    GpReg::X4,   // R9  -> X4 (6th arg)
    GpReg::X10,  // R10 -> X10
    GpReg::X11,  // R11 -> X11
    GpReg::X20,  // R12 -> X20 (callee-saved)
    GpReg::X21,  // R13 -> X21 (callee-saved)
    GpReg::X22,  // R14 -> X22 (callee-saved)
    GpReg::X23,  // R15 -> X23 (callee-saved)
};

// Map x86 XMM to ARM64 NEON
constexpr VecReg kXmmToNeon[] = {
    VecReg::V0, VecReg::V1, VecReg::V2, VecReg::V3,
    VecReg::V4, VecReg::V5, VecReg::V6, VecReg::V7,
    VecReg::V8, VecReg::V9, VecReg::V10, VecReg::V11,
    VecReg::V12, VecReg::V13, VecReg::V14, VecReg::V15
};

// Compiled code block
struct CompiledBlock {
    void* code;              // Pointer to generated ARM64 code
    size_t code_size;        // Size of generated code
    uint64_t guest_addr;     // Original x86 address
    size_t guest_size;       // Original x86 code size
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
};

/**
 * x86-64 CPU State (emulated)
 */
struct X86State {
    uint64_t gpr[16];        // RAX-R15
    uint64_t rip;            // Instruction pointer
    uint64_t rflags;         // Flags register
    
    // SSE state
    alignas(16) uint8_t xmm[16][16];  // XMM0-XMM15
    uint32_t mxcsr;
    
    // Memory base for fast memory access
    void* memory_base;
    
    // Helper for flags
    bool GetZF() const { return (rflags >> 6) & 1; }
    bool GetCF() const { return rflags & 1; }
    bool GetSF() const { return (rflags >> 7) & 1; }
    bool GetOF() const { return (rflags >> 11) & 1; }
    
    void SetZF(bool v) { rflags = v ? (rflags | 0x40) : (rflags & ~0x40ULL); }
    void SetCF(bool v) { rflags = v ? (rflags | 1) : (rflags & ~1ULL); }
    void SetSF(bool v) { rflags = v ? (rflags | 0x80) : (rflags & ~0x80ULL); }
    void SetOF(bool v) { rflags = v ? (rflags | 0x800) : (rflags & ~0x800ULL); }
};

/**
 * JIT Compiler - Main Translation Engine
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
    void Execute(X86State* state, CompiledBlock* block);
    
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
    bool TranslateInstruction(const DecodedInstr& instr, Emitter& emit, 
                              uint64_t guest_addr, uint64_t next_addr);
    
    // Map registers
    GpReg MapGpReg(Reg64 x86_reg) const {
        if (x86_reg == Reg64::NONE || static_cast<int>(x86_reg) >= 16) {
            return GpReg::X24;  // Temp register
        }
        return kX86ToArm64[static_cast<int>(x86_reg)];
    }
    
    // Translate condition codes
    Cond TranslateCondition(ConditionCode x86_cc);
    
    // Emit flag calculations
    void EmitFlagUpdate(Emitter& emit, OpcodeType op, GpReg result, 
                        GpReg op1, GpReg op2);
    
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

inline Cond JitCompiler::TranslateCondition(ConditionCode x86_cc) {
    // x86 condition -> ARM64 condition
    switch (x86_cc) {
        case ConditionCode::O:  return Cond::VS;  // Overflow
        case ConditionCode::NO: return Cond::VC;  // No overflow
        case ConditionCode::B:  return Cond::CC;  // Below (CF=1)
        case ConditionCode::AE: return Cond::CS;  // Above or equal (CF=0)
        case ConditionCode::E:  return Cond::EQ;  // Equal (ZF=1)
        case ConditionCode::NE: return Cond::NE;  // Not equal (ZF=0)
        case ConditionCode::BE: return Cond::LS;  // Below or equal
        case ConditionCode::A:  return Cond::HI;  // Above
        case ConditionCode::S:  return Cond::MI;  // Sign (negative)
        case ConditionCode::NS: return Cond::PL;  // Not sign
        case ConditionCode::L:  return Cond::LT;  // Less (signed)
        case ConditionCode::GE: return Cond::GE;  // Greater or equal
        case ConditionCode::LE: return Cond::LE;  // Less or equal
        case ConditionCode::G:  return Cond::GT;  // Greater
        default:                return Cond::AL;  // Always
    }
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
    
    // Decode and translate instructions
    size_t guest_offset = 0;
    size_t instr_count = 0;
    bool block_end = false;
    
    while (!block_end && guest_offset < config_.max_block_size) {
        DecodedInstr instr;
        size_t len = DecodeInstruction(guest_code + guest_offset, 
                                        config_.max_block_size - guest_offset, 
                                        &instr);
        
        if (len == 0 || instr.type == OpcodeType::UNKNOWN) {
            // Undecodable instruction - emit trap
            emit.BRK(0xFFFF);
            break;
        }
        
        uint64_t next_addr = guest_addr + guest_offset + len;
        
        if (!TranslateInstruction(instr, emit, guest_addr + guest_offset, next_addr)) {
            // Translation failed
            emit.BRK(0xFFFE);
            break;
        }
        
        guest_offset += len;
        instr_count++;
        
        // Check for block-ending instructions
        switch (instr.type) {
            case OpcodeType::RET:
            case OpcodeType::JMP_REL:
            case OpcodeType::JMP_ABS:
            case OpcodeType::CALL_REL:
            case OpcodeType::CALL_ABS:
            case OpcodeType::JCC:
            case OpcodeType::SYSCALL:
            case OpcodeType::INT3:
                block_end = true;
                break;
            default:
                break;
        }
    }
    
    // Create compiled block
    auto block = std::make_unique<CompiledBlock>();
    block->code = code_start;
    block->code_size = buf.GetOffset();
    block->guest_addr = guest_addr;
    block->guest_size = guest_offset;
    block->entry_count = 0;
    
    // Update cache usage
    code_cache_used_ += (buf.GetOffset() + 15) & ~15;  // 16-byte align
    
    // Flush instruction cache
    __builtin___clear_cache(static_cast<char*>(code_start),
                            static_cast<char*>(code_start) + block->code_size);
    
    total_instructions_ += instr_count;
    
    CompiledBlock* result = block.get();
    block_cache_[guest_addr] = std::move(block);
    
    return result;
}

inline bool JitCompiler::TranslateInstruction(const DecodedInstr& instr, 
                                               Emitter& emit,
                                               uint64_t guest_addr,
                                               uint64_t next_addr) {
    switch (instr.type) {
        case OpcodeType::NOP:
            emit.NOP();
            return true;
            
        case OpcodeType::MOV_REG_REG:
            emit.MOV(MapGpReg(instr.dst_reg), MapGpReg(instr.src_reg));
            return true;
            
        case OpcodeType::MOV_REG_IMM:
            emit.MOV_IMM64(MapGpReg(instr.dst_reg), instr.immediate);
            return true;
            
        case OpcodeType::PUSH:
            // SUB SP, SP, #8; STR Xn, [SP]
            emit.SUB_IMM(GpReg::SP, GpReg::SP, 8);
            emit.STR(MapGpReg(instr.src_reg), GpReg::SP, 0);
            return true;
            
        case OpcodeType::POP:
            // LDR Xn, [SP]; ADD SP, SP, #8
            emit.LDR(MapGpReg(instr.dst_reg), GpReg::SP, 0);
            emit.ADD_IMM(GpReg::SP, GpReg::SP, 8);
            return true;
            
        case OpcodeType::ADD_REG_REG:
            emit.ADDS(MapGpReg(instr.dst_reg), MapGpReg(instr.dst_reg), 
                     MapGpReg(instr.src_reg));
            return true;
            
        case OpcodeType::ADD_REG_IMM:
            if (instr.immediate >= 0 && instr.immediate < 4096) {
                emit.ADD_IMM(MapGpReg(instr.dst_reg), MapGpReg(instr.dst_reg), 
                            static_cast<uint16_t>(instr.immediate));
            } else {
                emit.MOV_IMM64(GpReg::X24, instr.immediate);
                emit.ADDS(MapGpReg(instr.dst_reg), MapGpReg(instr.dst_reg), GpReg::X24);
            }
            return true;
            
        case OpcodeType::SUB_REG_REG:
            emit.SUBS(MapGpReg(instr.dst_reg), MapGpReg(instr.dst_reg), 
                     MapGpReg(instr.src_reg));
            return true;
            
        case OpcodeType::SUB_REG_IMM:
            if (instr.immediate >= 0 && instr.immediate < 4096) {
                emit.SUB_IMM(MapGpReg(instr.dst_reg), MapGpReg(instr.dst_reg), 
                            static_cast<uint16_t>(instr.immediate));
            } else {
                emit.MOV_IMM64(GpReg::X24, instr.immediate);
                emit.SUBS(MapGpReg(instr.dst_reg), MapGpReg(instr.dst_reg), GpReg::X24);
            }
            return true;
            
        case OpcodeType::AND_REG_REG:
            emit.ANDS(MapGpReg(instr.dst_reg), MapGpReg(instr.dst_reg), 
                     MapGpReg(instr.src_reg));
            return true;
            
        case OpcodeType::OR_REG_REG:
            emit.ORR(MapGpReg(instr.dst_reg), MapGpReg(instr.dst_reg), 
                    MapGpReg(instr.src_reg));
            return true;
            
        case OpcodeType::XOR_REG_REG:
            emit.EOR(MapGpReg(instr.dst_reg), MapGpReg(instr.dst_reg), 
                    MapGpReg(instr.src_reg));
            return true;
            
        case OpcodeType::CMP_REG_REG:
            emit.CMP(MapGpReg(instr.dst_reg), MapGpReg(instr.src_reg));
            return true;
            
        case OpcodeType::CMP_REG_IMM:
            if (instr.immediate >= 0 && instr.immediate < 4096) {
                emit.CMP_IMM(MapGpReg(instr.dst_reg), static_cast<uint16_t>(instr.immediate));
            } else {
                emit.MOV_IMM64(GpReg::X24, instr.immediate);
                emit.CMP(MapGpReg(instr.dst_reg), GpReg::X24);
            }
            return true;
            
        case OpcodeType::TEST_REG_REG:
            emit.TST(MapGpReg(instr.dst_reg), MapGpReg(instr.src_reg));
            return true;
            
        case OpcodeType::JMP_REL:
            // For now, emit return to interpreter with target address
            emit.MOV_IMM64(GpReg::X0, next_addr + instr.immediate);
            emit.RET();
            return true;
            
        case OpcodeType::JCC:
            // Conditional: B.cond to taken, fall through to not taken
            {
                Cond arm_cond = TranslateCondition(instr.cond);
                // Emit: B.cond +8; MOV X0, next_addr; RET; MOV X0, target; RET
                emit.B_COND(arm_cond, 12);  // Skip to taken
                emit.MOV_IMM64(GpReg::X0, next_addr);  // Not taken
                emit.RET();
                emit.MOV_IMM64(GpReg::X0, next_addr + instr.immediate);  // Taken
                emit.RET();
            }
            return true;
            
        case OpcodeType::CALL_REL:
            // Push return address, jump to target
            emit.MOV_IMM64(GpReg::X24, next_addr);
            emit.SUB_IMM(GpReg::SP, GpReg::SP, 8);
            emit.STR(GpReg::X24, GpReg::SP, 0);
            emit.MOV_IMM64(GpReg::X0, next_addr + instr.immediate);
            emit.RET();
            return true;
            
        case OpcodeType::RET:
            // Pop return address, return to interpreter
            emit.LDR(GpReg::X0, GpReg::SP, 0);
            emit.ADD_IMM(GpReg::SP, GpReg::SP, 8);
            emit.RET();
            return true;
            
        case OpcodeType::SYSCALL:
            // Return to interpreter for syscall handling
            emit.MOV_IMM64(GpReg::X0, next_addr);
            emit.MOVZ(GpReg::X1, 1);  // Flag: syscall
            emit.RET();
            return true;
            
        case OpcodeType::INT3:
            emit.BRK(3);
            return true;
            
        // SSE translations
        case OpcodeType::ADDPS:
            emit.FADD_4S(VecReg::V0, VecReg::V0, VecReg::V1);  // Simplified
            return true;
            
        case OpcodeType::MULPS:
            emit.FMUL_4S(VecReg::V0, VecReg::V0, VecReg::V1);  // Simplified
            return true;
            
        default:
            // Unknown instruction - emit trap
            return false;
    }
}

inline void JitCompiler::Execute(X86State* state, CompiledBlock* block) {
    if (!state || !block || !block->code) return;
    
    block->entry_count++;
    total_executions_++;
    
    // Call generated code
    using JitFunc = uint64_t (*)(X86State*);
    JitFunc func = reinterpret_cast<JitFunc>(block->code);
    
    // The generated code returns next guest address in X0
    state->rip = func(state);
}

} // namespace rpcsx::nce

#endif // RPCSX_JIT_COMPILER_H
