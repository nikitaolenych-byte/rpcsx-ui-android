/**
 * NCE Standalone - ARM64 JIT for Android UI wrapper
 * PowerPC (Cell PPU) → ARM64 binary translator
 * 
 * Спрощена версія для інтеграції з librpcsx.so через hooking
 */

#pragma once

#include <cstdint>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <vector>

namespace nce {

// PPU стан для JIT (standalone версія)
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
    
    struct XER_t {
        uint32_t raw;
    } xer;
    
    uint32_t fpscr;     // FP Status and Control Register
    
    // Program Counter
    uint64_t pc;
    
    // Memory base pointer
    void* memory_base;
    
    // Original thread (opaque)
    void* thread;
};

// Скомпільований блок
struct CompiledBlock {
    void* code;             // Pointer to native ARM64 code
    uint32_t guest_addr;    // Guest address
    uint32_t guest_size;    // Size in bytes
    uint32_t host_size;     // Host code size
    uint32_t exec_count;    // Execution count
    bool has_branch;        
    uint32_t branch_target; 
};

// Конфігурація JIT
struct JitConfig {
    size_t code_cache_size = 128 * 1024 * 1024;  // 128MB
    uint32_t max_block_size = 256;
    bool enable_block_linking = true;
    int optimization_level = 2;
    bool enable_profiling = false;
};

// Статистика JIT
struct JitStats {
    std::atomic<uint64_t> blocks_compiled{0};
    std::atomic<uint64_t> blocks_executed{0};
    std::atomic<uint64_t> instructions_executed{0};
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> interpreter_fallbacks{0};
};

// ARM64 регістри
namespace arm64 {
    constexpr int X0 = 0, X1 = 1, X2 = 2, X3 = 3, X4 = 4, X5 = 5, X6 = 6, X7 = 7;
    constexpr int X8 = 8, X9 = 9, X10 = 10, X11 = 11, X12 = 12, X13 = 13, X14 = 14, X15 = 15;
    constexpr int X16 = 16, X17 = 17, X18 = 18, X19 = 19, X20 = 20, X21 = 21, X22 = 22, X23 = 23;
    constexpr int X24 = 24, X25 = 25, X26 = 26, X27 = 27, X28 = 28, X29 = 29, X30 = 30;
    constexpr int XZR = 31;
    
    // Призначення регістрів
    constexpr int REG_STATE = X19;   // PPUState pointer
    constexpr int REG_MEM = X20;     // Memory base
    constexpr int REG_TMP0 = X9;
    constexpr int REG_TMP1 = X10;
    constexpr int REG_TMP2 = X11;
}

/**
 * ARM64 Emitter - генерація ARM64 машинного коду
 */
class ARM64Emitter {
public:
    std::vector<uint32_t>& code;
    
    ARM64Emitter(std::vector<uint32_t>& buffer) : code(buffer) {}
    
    void emit(uint32_t insn) { code.push_back(insn); }
    size_t size() const { return code.size(); }
    
    // Basic instructions
    void NOP() { emit(0xD503201F); }
    void RET() { emit(0xD65F03C0); }
    
    // Move
    void MOV(int rd, int rn) { emit(0xAA0003E0 | (rn << 16) | rd); }
    
    void MOV64(int rd, uint64_t imm) {
        emit(0xD2800000 | ((imm & 0xFFFF) << 5) | rd);
        if (imm > 0xFFFF) {
            emit(0xF2A00000 | (((imm >> 16) & 0xFFFF) << 5) | rd);
        }
        if (imm > 0xFFFFFFFF) {
            emit(0xF2C00000 | (((imm >> 32) & 0xFFFF) << 5) | rd);
        }
    }
    
    // Arithmetic
    void ADD(int rd, int rn, int rm) { emit(0x8B000000 | (rm << 16) | (rn << 5) | rd); }
    void ADD_imm(int rd, int rn, uint16_t imm) { emit(0x91000000 | (imm << 10) | (rn << 5) | rd); }
    void SUB(int rd, int rn, int rm) { emit(0xCB000000 | (rm << 16) | (rn << 5) | rd); }
    void SUB_imm(int rd, int rn, uint16_t imm) { emit(0xD1000000 | (imm << 10) | (rn << 5) | rd); }
    void MUL(int rd, int rn, int rm) { emit(0x9B007C00 | (rm << 16) | (rn << 5) | rd); }
    void SDIV(int rd, int rn, int rm) { emit(0x9AC00C00 | (rm << 16) | (rn << 5) | rd); }
    
    // Logic
    void AND(int rd, int rn, int rm) { emit(0x8A000000 | (rm << 16) | (rn << 5) | rd); }
    void ORR(int rd, int rn, int rm) { emit(0xAA000000 | (rm << 16) | (rn << 5) | rd); }
    void EOR(int rd, int rn, int rm) { emit(0xCA000000 | (rm << 16) | (rn << 5) | rd); }
    void ORR_imm(int rd, int rn, uint16_t imm) { emit(0xB2400000 | (imm << 10) | (rn << 5) | rd); }
    
    // Shifts
    void LSL(int rd, int rn, int rm) { emit(0x9AC02000 | (rm << 16) | (rn << 5) | rd); }
    void LSR(int rd, int rn, int rm) { emit(0x9AC02400 | (rm << 16) | (rn << 5) | rd); }
    void ASR(int rd, int rn, int rm) { emit(0x9AC02800 | (rm << 16) | (rn << 5) | rd); }
    void LSL_imm(int rd, int rn, uint8_t shift) { 
        uint8_t immr = (64 - shift) & 63;
        uint8_t imms = 63 - shift;
        emit(0xD3400000 | (immr << 16) | (imms << 10) | (rn << 5) | rd);
    }
    void LSR_imm(int rd, int rn, uint8_t shift) {
        emit(0xD340FC00 | (shift << 16) | (rn << 5) | rd);
    }
    
    // Load/Store
    void LDR(int rt, int rn, int16_t offset) { 
        uint32_t uoffset = (offset >> 3) & 0xFFF;
        emit(0xF9400000 | (uoffset << 10) | (rn << 5) | rt); 
    }
    void LDRW(int rt, int rn, int16_t offset) {
        uint32_t uoffset = (offset >> 2) & 0xFFF;
        emit(0xB9400000 | (uoffset << 10) | (rn << 5) | rt);
    }
    void LDRB(int rt, int rn, int16_t offset) {
        emit(0x39400000 | ((offset & 0xFFF) << 10) | (rn << 5) | rt);
    }
    void STR(int rt, int rn, int16_t offset) {
        uint32_t uoffset = (offset >> 3) & 0xFFF;
        emit(0xF9000000 | (uoffset << 10) | (rn << 5) | rt);
    }
    void STRW(int rt, int rn, int16_t offset) {
        uint32_t uoffset = (offset >> 2) & 0xFFF;
        emit(0xB9000000 | (uoffset << 10) | (rn << 5) | rt);
    }
    void STRB(int rt, int rn, int16_t offset) {
        emit(0x39000000 | ((offset & 0xFFF) << 10) | (rn << 5) | rt);
    }
    void LDR_reg(int rt, int rn, int rm) {
        emit(0xF8606800 | (rm << 16) | (rn << 5) | rt);
    }
    void STR_reg(int rt, int rn, int rm) {
        emit(0xF8206800 | (rm << 16) | (rn << 5) | rt);
    }
    
    // Stack ops
    void STP_pre(int rt, int rt2, int rn, int16_t offset) {
        uint32_t imm7 = (offset >> 3) & 0x7F;
        emit(0xA9800000 | (imm7 << 15) | (rt2 << 10) | (rn << 5) | rt);
    }
    void LDP_post(int rt, int rt2, int rn, int16_t offset) {
        uint32_t imm7 = (offset >> 3) & 0x7F;
        emit(0xA8C00000 | (imm7 << 15) | (rt2 << 10) | (rn << 5) | rt);
    }
    
    // Compare
    void CMP(int rn, int rm) { emit(0xEB00001F | (rm << 16) | (rn << 5)); }
    void CMP_imm(int rn, uint16_t imm) { emit(0xF100001F | (imm << 10) | (rn << 5)); }
    
    // Byte swap (big-endian)
    void REV(int rd, int rn) { emit(0xDAC00C00 | (rn << 5) | rd); }
    void REV32(int rd, int rn) { emit(0xDAC00800 | (rn << 5) | rd); }
    void REV16(int rd, int rn) { emit(0xDAC00400 | (rn << 5) | rd); }
    
    // Branch
    void B(int32_t offset) {
        int32_t imm26 = (offset >> 2) & 0x3FFFFFF;
        emit(0x14000000 | imm26);
    }
    void BL(int32_t offset) {
        int32_t imm26 = (offset >> 2) & 0x3FFFFFF;
        emit(0x94000000 | imm26);
    }
    void BR(int rn) { emit(0xD61F0000 | (rn << 5)); }
    void BLR(int rn) { emit(0xD63F0000 | (rn << 5)); }
};

/**
 * PPU Opcode decoder
 */
struct PPUOpcode {
    uint32_t raw;
    
    uint32_t primary() const { return (raw >> 26) & 0x3F; }
    uint32_t rd() const { return (raw >> 21) & 0x1F; }
    uint32_t rs() const { return (raw >> 21) & 0x1F; }
    uint32_t ra() const { return (raw >> 16) & 0x1F; }
    uint32_t rb() const { return (raw >> 11) & 0x1F; }
    int16_t simm16() const { return static_cast<int16_t>(raw & 0xFFFF); }
    uint16_t uimm16() const { return raw & 0xFFFF; }
    uint32_t xo_9() const { return (raw >> 1) & 0x1FF; }
    uint32_t xo_10() const { return (raw >> 1) & 0x3FF; }
    bool rc() const { return raw & 1; }
    bool aa() const { return (raw >> 1) & 1; }
    bool lk() const { return raw & 1; }
    int32_t li() const { 
        int32_t val = raw & 0x03FFFFFC;
        if (val & 0x02000000) val |= 0xFC000000;
        return val;
    }
    int16_t bd() const {
        int16_t val = raw & 0xFFFC;
        if (val & 0x8000) val |= 0xFFFF0000;
        return val;
    }
    uint32_t sh() const { return (raw >> 11) & 0x1F; }
    uint32_t mb() const { return (raw >> 6) & 0x1F; }
    uint32_t me() const { return (raw >> 1) & 0x1F; }
    uint32_t bf() const { return (raw >> 23) & 0x7; }
    bool l10() const { return (raw >> 21) & 1; }
};

/**
 * PPU Translator - генерує ARM64 код з PPU інструкцій
 */
class PPUTranslator {
public:
    ARM64Emitter& emit;
    
    static constexpr int GPR_OFFSET = offsetof(PPUState, gpr);
    static constexpr int FPR_OFFSET = offsetof(PPUState, fpr);
    static constexpr int LR_OFFSET = offsetof(PPUState, lr);
    static constexpr int CTR_OFFSET = offsetof(PPUState, ctr);
    static constexpr int CR_OFFSET = offsetof(PPUState, cr);
    static constexpr int PC_OFFSET = offsetof(PPUState, pc);
    
    PPUTranslator(ARM64Emitter& e) : emit(e) {}
    
    void LoadGPR(int arm_reg, uint32_t ppu_gpr) {
        int offset = GPR_OFFSET + ppu_gpr * 8;
        emit.LDR(arm_reg, arm64::REG_STATE, offset);
    }
    
    void StoreGPR(uint32_t ppu_gpr, int arm_reg) {
        int offset = GPR_OFFSET + ppu_gpr * 8;
        emit.STR(arm_reg, arm64::REG_STATE, offset);
    }
    
    void UpdatePC(uint32_t pc) {
        emit.MOV64(arm64::REG_TMP0, pc);
        emit.STR(arm64::REG_TMP0, arm64::REG_STATE, PC_OFFSET);
    }
    
    // PPU Instructions
    void INSN_ADDI(const PPUOpcode& op) {
        uint32_t rd = op.rd();
        uint32_t ra = op.ra();
        int16_t imm = op.simm16();
        
        if (ra == 0) {
            emit.MOV64(arm64::REG_TMP0, imm);
        } else {
            LoadGPR(arm64::REG_TMP0, ra);
            if (imm >= 0) {
                emit.ADD_imm(arm64::REG_TMP0, arm64::REG_TMP0, imm);
            } else {
                emit.SUB_imm(arm64::REG_TMP0, arm64::REG_TMP0, -imm);
            }
        }
        StoreGPR(rd, arm64::REG_TMP0);
    }
    
    void INSN_ADDIS(const PPUOpcode& op) {
        uint32_t rd = op.rd();
        uint32_t ra = op.ra();
        int32_t imm = static_cast<int32_t>(op.simm16()) << 16;
        
        if (ra == 0) {
            emit.MOV64(arm64::REG_TMP0, imm);
        } else {
            LoadGPR(arm64::REG_TMP0, ra);
            emit.MOV64(arm64::REG_TMP1, imm);
            emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        }
        StoreGPR(rd, arm64::REG_TMP0);
    }
    
    void INSN_ADD(const PPUOpcode& op) {
        uint32_t rd = op.rd();
        uint32_t ra = op.ra();
        uint32_t rb = op.rb();
        
        LoadGPR(arm64::REG_TMP0, ra);
        LoadGPR(arm64::REG_TMP1, rb);
        emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(rd, arm64::REG_TMP0);
    }
    
    void INSN_SUBF(const PPUOpcode& op) {
        uint32_t rd = op.rd();
        uint32_t ra = op.ra();
        uint32_t rb = op.rb();
        
        LoadGPR(arm64::REG_TMP0, rb);
        LoadGPR(arm64::REG_TMP1, ra);
        emit.SUB(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(rd, arm64::REG_TMP0);
    }
    
    void INSN_MULLW(const PPUOpcode& op) {
        uint32_t rd = op.rd();
        uint32_t ra = op.ra();
        uint32_t rb = op.rb();
        
        LoadGPR(arm64::REG_TMP0, ra);
        LoadGPR(arm64::REG_TMP1, rb);
        emit.MUL(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(rd, arm64::REG_TMP0);
    }
    
    void INSN_DIVW(const PPUOpcode& op) {
        uint32_t rd = op.rd();
        uint32_t ra = op.ra();
        uint32_t rb = op.rb();
        
        LoadGPR(arm64::REG_TMP0, ra);
        LoadGPR(arm64::REG_TMP1, rb);
        emit.SDIV(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(rd, arm64::REG_TMP0);
    }
    
    void INSN_AND(const PPUOpcode& op) {
        uint32_t ra = op.ra();
        uint32_t rs = op.rs();
        uint32_t rb = op.rb();
        
        LoadGPR(arm64::REG_TMP0, rs);
        LoadGPR(arm64::REG_TMP1, rb);
        emit.AND(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(ra, arm64::REG_TMP0);
    }
    
    void INSN_OR(const PPUOpcode& op) {
        uint32_t ra = op.ra();
        uint32_t rs = op.rs();
        uint32_t rb = op.rb();
        
        LoadGPR(arm64::REG_TMP0, rs);
        LoadGPR(arm64::REG_TMP1, rb);
        emit.ORR(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(ra, arm64::REG_TMP0);
    }
    
    void INSN_XOR(const PPUOpcode& op) {
        uint32_t ra = op.ra();
        uint32_t rs = op.rs();
        uint32_t rb = op.rb();
        
        LoadGPR(arm64::REG_TMP0, rs);
        LoadGPR(arm64::REG_TMP1, rb);
        emit.EOR(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(ra, arm64::REG_TMP0);
    }
    
    void INSN_ORI(const PPUOpcode& op) {
        uint32_t ra = op.ra();
        uint32_t rs = op.rs();
        uint16_t imm = op.uimm16();
        
        LoadGPR(arm64::REG_TMP0, rs);
        emit.MOV64(arm64::REG_TMP1, imm);
        emit.ORR(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(ra, arm64::REG_TMP0);
    }
    
    void INSN_SLW(const PPUOpcode& op) {
        uint32_t ra = op.ra();
        uint32_t rs = op.rs();
        uint32_t rb = op.rb();
        
        LoadGPR(arm64::REG_TMP0, rs);
        LoadGPR(arm64::REG_TMP1, rb);
        emit.LSL(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(ra, arm64::REG_TMP0);
    }
    
    void INSN_SRW(const PPUOpcode& op) {
        uint32_t ra = op.ra();
        uint32_t rs = op.rs();
        uint32_t rb = op.rb();
        
        LoadGPR(arm64::REG_TMP0, rs);
        LoadGPR(arm64::REG_TMP1, rb);
        emit.LSR(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(ra, arm64::REG_TMP0);
    }
    
    void INSN_RLWINM(const PPUOpcode& op) {
        uint32_t ra = op.ra();
        uint32_t rs = op.rs();
        uint32_t sh = op.sh();
        
        LoadGPR(arm64::REG_TMP0, rs);
        if (sh > 0) {
            emit.LSL_imm(arm64::REG_TMP0, arm64::REG_TMP0, sh);
        }
        StoreGPR(ra, arm64::REG_TMP0);
    }
    
    void INSN_LWZ(const PPUOpcode& op) {
        uint32_t rd = op.rd();
        uint32_t ra = op.ra();
        int16_t offset = op.simm16();
        
        if (ra == 0) {
            emit.MOV64(arm64::REG_TMP0, offset);
        } else {
            LoadGPR(arm64::REG_TMP0, ra);
            if (offset != 0) {
                if (offset > 0) {
                    emit.ADD_imm(arm64::REG_TMP0, arm64::REG_TMP0, offset);
                } else {
                    emit.SUB_imm(arm64::REG_TMP0, arm64::REG_TMP0, -offset);
                }
            }
        }
        emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_MEM);
        emit.LDRW(arm64::REG_TMP0, arm64::REG_TMP0, 0);
        emit.REV32(arm64::REG_TMP0, arm64::REG_TMP0);
        StoreGPR(rd, arm64::REG_TMP0);
    }
    
    void INSN_STW(const PPUOpcode& op) {
        uint32_t rs = op.rs();
        uint32_t ra = op.ra();
        int16_t offset = op.simm16();
        
        if (ra == 0) {
            emit.MOV64(arm64::REG_TMP0, offset);
        } else {
            LoadGPR(arm64::REG_TMP0, ra);
            if (offset != 0) {
                if (offset > 0) {
                    emit.ADD_imm(arm64::REG_TMP0, arm64::REG_TMP0, offset);
                } else {
                    emit.SUB_imm(arm64::REG_TMP0, arm64::REG_TMP0, -offset);
                }
            }
        }
        emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_MEM);
        LoadGPR(arm64::REG_TMP1, rs);
        emit.REV32(arm64::REG_TMP1, arm64::REG_TMP1);
        emit.STRW(arm64::REG_TMP1, arm64::REG_TMP0, 0);
    }
    
    void INSN_LBZ(const PPUOpcode& op) {
        uint32_t rd = op.rd();
        uint32_t ra = op.ra();
        int16_t offset = op.simm16();
        
        if (ra == 0) {
            emit.MOV64(arm64::REG_TMP0, offset);
        } else {
            LoadGPR(arm64::REG_TMP0, ra);
            if (offset != 0) {
                if (offset > 0) {
                    emit.ADD_imm(arm64::REG_TMP0, arm64::REG_TMP0, offset);
                } else {
                    emit.SUB_imm(arm64::REG_TMP0, arm64::REG_TMP0, -offset);
                }
            }
        }
        emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_MEM);
        emit.LDRB(arm64::REG_TMP0, arm64::REG_TMP0, 0);
        StoreGPR(rd, arm64::REG_TMP0);
    }
    
    void INSN_STB(const PPUOpcode& op) {
        uint32_t rs = op.rs();
        uint32_t ra = op.ra();
        int16_t offset = op.simm16();
        
        if (ra == 0) {
            emit.MOV64(arm64::REG_TMP0, offset);
        } else {
            LoadGPR(arm64::REG_TMP0, ra);
            if (offset != 0) {
                if (offset > 0) {
                    emit.ADD_imm(arm64::REG_TMP0, arm64::REG_TMP0, offset);
                } else {
                    emit.SUB_imm(arm64::REG_TMP0, arm64::REG_TMP0, -offset);
                }
            }
        }
        emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_MEM);
        LoadGPR(arm64::REG_TMP1, rs);
        emit.STRB(arm64::REG_TMP1, arm64::REG_TMP0, 0);
    }
    
    void INSN_LD(const PPUOpcode& op) {
        uint32_t rd = op.rd();
        uint32_t ra = op.ra();
        int16_t offset = op.simm16() & ~3;
        
        if (ra == 0) {
            emit.MOV64(arm64::REG_TMP0, offset);
        } else {
            LoadGPR(arm64::REG_TMP0, ra);
            if (offset != 0) {
                if (offset > 0) {
                    emit.ADD_imm(arm64::REG_TMP0, arm64::REG_TMP0, offset);
                } else {
                    emit.SUB_imm(arm64::REG_TMP0, arm64::REG_TMP0, -offset);
                }
            }
        }
        emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_MEM);
        emit.LDR(arm64::REG_TMP0, arm64::REG_TMP0, 0);
        emit.REV(arm64::REG_TMP0, arm64::REG_TMP0);
        StoreGPR(rd, arm64::REG_TMP0);
    }
    
    void INSN_STD(const PPUOpcode& op) {
        uint32_t rs = op.rs();
        uint32_t ra = op.ra();
        int16_t offset = op.simm16() & ~3;
        
        if (ra == 0) {
            emit.MOV64(arm64::REG_TMP0, offset);
        } else {
            LoadGPR(arm64::REG_TMP0, ra);
            if (offset != 0) {
                if (offset > 0) {
                    emit.ADD_imm(arm64::REG_TMP0, arm64::REG_TMP0, offset);
                } else {
                    emit.SUB_imm(arm64::REG_TMP0, arm64::REG_TMP0, -offset);
                }
            }
        }
        emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_MEM);
        LoadGPR(arm64::REG_TMP1, rs);
        emit.REV(arm64::REG_TMP1, arm64::REG_TMP1);
        emit.STR(arm64::REG_TMP1, arm64::REG_TMP0, 0);
    }
    
    void INSN_MFLR(const PPUOpcode& op) {
        uint32_t rd = op.rd();
        emit.LDR(arm64::REG_TMP0, arm64::REG_STATE, LR_OFFSET);
        StoreGPR(rd, arm64::REG_TMP0);
    }
    
    void INSN_MTLR(const PPUOpcode& op) {
        uint32_t rs = op.rs();
        LoadGPR(arm64::REG_TMP0, rs);
        emit.STR(arm64::REG_TMP0, arm64::REG_STATE, LR_OFFSET);
    }
    
    void INSN_MFCTR(const PPUOpcode& op) {
        uint32_t rd = op.rd();
        emit.LDR(arm64::REG_TMP0, arm64::REG_STATE, CTR_OFFSET);
        StoreGPR(rd, arm64::REG_TMP0);
    }
    
    void INSN_MTCTR(const PPUOpcode& op) {
        uint32_t rs = op.rs();
        LoadGPR(arm64::REG_TMP0, rs);
        emit.STR(arm64::REG_TMP0, arm64::REG_STATE, CTR_OFFSET);
    }
    
    void INSN_CMP(const PPUOpcode& op) {
        uint32_t ra = op.ra();
        uint32_t rb = op.rb();
        
        LoadGPR(arm64::REG_TMP0, ra);
        LoadGPR(arm64::REG_TMP1, rb);
        emit.CMP(arm64::REG_TMP0, arm64::REG_TMP1);
    }
    
    void INSN_CMPI(const PPUOpcode& op) {
        uint32_t ra = op.ra();
        int16_t imm = op.simm16();
        
        LoadGPR(arm64::REG_TMP0, ra);
        if (imm >= 0 && imm < 4096) {
            emit.CMP_imm(arm64::REG_TMP0, imm);
        } else {
            emit.MOV64(arm64::REG_TMP1, imm);
            emit.CMP(arm64::REG_TMP0, arm64::REG_TMP1);
        }
    }
};

/**
 * NCE JIT Compiler
 */
class NCECompiler {
public:
    NCECompiler();
    ~NCECompiler();
    
    bool Initialize(const JitConfig& config = JitConfig{});
    void Shutdown();
    
    CompiledBlock* CompileBlock(void* memory_base, uint32_t guest_addr);
    void ExecuteBlock(PPUState& state, CompiledBlock* block);
    
    CompiledBlock* LookupBlock(uint32_t guest_addr);
    void InvalidateRange(uint32_t start, uint32_t size);
    void ClearCache();
    
    const JitStats& GetStats() const { return m_stats; }
    void ResetStats();
    
private:
    void* AllocateCode(size_t size);
    
    JitConfig m_config;
    JitStats m_stats;
    
    void* m_code_cache = nullptr;
    size_t m_code_cache_size = 0;
    size_t m_code_cache_used = 0;
    
    std::unordered_map<uint32_t, std::unique_ptr<CompiledBlock>> m_blocks;
    std::mutex m_blocks_mutex;
    
    bool m_initialized = false;
};

} // namespace nce
