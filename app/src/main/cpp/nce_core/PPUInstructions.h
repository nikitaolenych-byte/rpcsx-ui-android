/**
 * NCE ARM64 PPU Instructions Implementation
 * Реалізація критичних PowerPC інструкцій для ARM64 JIT
 * 
 * Категорії:
 * 1. Арифметичні (ADD, SUB, MUL, DIV)
 * 2. Логічні (AND, OR, XOR, NOT)
 * 3. Load/Store (LWZ, STW, LBZ, STB, etc.)
 * 4. Branch (B, BC, BLR, BCTR)
 * 5. Compare (CMP, CMPI, CMPL)
 * 6. Shift/Rotate (SLW, SRW, RLWINM)
 */

#pragma once

#include <cstdint>
#include <vector>

namespace rpcsx::nce::ppu_insn {

// ARM64 register constants
namespace arm64 {
    // Caller-saved (scratch)
    constexpr int X0 = 0, X1 = 1, X2 = 2, X3 = 3;
    constexpr int X4 = 4, X5 = 5, X6 = 6, X7 = 7;
    constexpr int X8 = 8, X9 = 9, X10 = 10, X11 = 11;
    constexpr int X12 = 12, X13 = 13, X14 = 14, X15 = 15;
    
    // Callee-saved
    constexpr int X19 = 19, X20 = 20, X21 = 21, X22 = 22;
    constexpr int X23 = 23, X24 = 24, X25 = 25, X26 = 26;
    constexpr int X27 = 27, X28 = 28;
    
    // Special
    constexpr int X29 = 29;  // Frame pointer
    constexpr int X30 = 30;  // Link register
    constexpr int XZR = 31;  // Zero register / SP
    
    // Our register allocation
    constexpr int REG_STATE = X19;     // PPU state pointer
    constexpr int REG_MEM = X20;       // Memory base
    constexpr int REG_PC = X21;        // Current PC cache
    constexpr int REG_TMP0 = X9;
    constexpr int REG_TMP1 = X10;
    constexpr int REG_TMP2 = X11;
    constexpr int REG_TMP3 = X12;
}

// PPU opcode fields extraction
struct PPUOpcode {
    uint32_t raw;
    
    // Common fields
    uint32_t primary() const { return (raw >> 26) & 0x3F; }
    uint32_t rd() const { return (raw >> 21) & 0x1F; }
    uint32_t rs() const { return (raw >> 21) & 0x1F; }
    uint32_t ra() const { return (raw >> 16) & 0x1F; }
    uint32_t rb() const { return (raw >> 11) & 0x1F; }
    uint32_t rc() const { return raw & 1; }
    uint32_t oe() const { return (raw >> 10) & 1; }
    
    // Immediate fields
    int16_t simm16() const { return static_cast<int16_t>(raw & 0xFFFF); }
    uint16_t uimm16() const { return raw & 0xFFFF; }
    
    // Branch fields
    uint32_t bo() const { return (raw >> 21) & 0x1F; }
    uint32_t bi() const { return (raw >> 16) & 0x1F; }
    uint32_t lk() const { return raw & 1; }
    uint32_t aa() const { return (raw >> 1) & 1; }
    int32_t bd() const { return static_cast<int16_t>(raw & 0xFFFC); }
    int32_t li() const { 
        int32_t val = (raw & 0x03FFFFFC);
        if (val & 0x02000000) val |= 0xFC000000; // Sign extend
        return val;
    }
    
    // Extended opcode
    uint32_t xo_10() const { return (raw >> 1) & 0x3FF; }
    uint32_t xo_9() const { return (raw >> 1) & 0x1FF; }
    uint32_t xo_5() const { return (raw >> 1) & 0x1F; }
    
    // Rotate/shift
    uint32_t sh() const { return (raw >> 11) & 0x1F; }
    uint32_t mb() const { return (raw >> 6) & 0x1F; }
    uint32_t me() const { return (raw >> 1) & 0x1F; }
    
    // Compare
    uint32_t crfd() const { return (raw >> 23) & 0x7; }
    uint32_t l10() const { return (raw >> 21) & 1; }
};

// Code emitter class
class ARM64Emitter {
public:
    std::vector<uint32_t>& code;
    
    explicit ARM64Emitter(std::vector<uint32_t>& c) : code(c) {}
    
    void emit(uint32_t insn) { code.push_back(insn); }
    
    // ===== Data Processing =====
    
    // ADD Xd, Xn, Xm
    void ADD(int rd, int rn, int rm) {
        emit(0x8B000000 | (rm << 16) | (rn << 5) | rd);
    }
    
    // ADD Xd, Xn, #imm12
    void ADDi(int rd, int rn, uint32_t imm12) {
        emit(0x91000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd);
    }
    
    // ADDS Xd, Xn, Xm (set flags)
    void ADDS(int rd, int rn, int rm) {
        emit(0xAB000000 | (rm << 16) | (rn << 5) | rd);
    }
    
    // SUB Xd, Xn, Xm
    void SUB(int rd, int rn, int rm) {
        emit(0xCB000000 | (rm << 16) | (rn << 5) | rd);
    }
    
    // SUBi Xd, Xn, #imm12
    void SUBi(int rd, int rn, uint32_t imm12) {
        emit(0xD1000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd);
    }
    
    // SUBS Xd, Xn, Xm (set flags)
    void SUBS(int rd, int rn, int rm) {
        emit(0xEB000000 | (rm << 16) | (rn << 5) | rd);
    }
    
    // AND Xd, Xn, Xm
    void AND(int rd, int rn, int rm) {
        emit(0x8A000000 | (rm << 16) | (rn << 5) | rd);
    }
    
    // ANDS Xd, Xn, Xm (set flags)
    void ANDS(int rd, int rn, int rm) {
        emit(0xEA000000 | (rm << 16) | (rn << 5) | rd);
    }
    
    // ORR Xd, Xn, Xm
    void ORR(int rd, int rn, int rm) {
        emit(0xAA000000 | (rm << 16) | (rn << 5) | rd);
    }
    
    // EOR Xd, Xn, Xm (XOR)
    void EOR(int rd, int rn, int rm) {
        emit(0xCA000000 | (rm << 16) | (rn << 5) | rd);
    }
    
    // MVN Xd, Xm (NOT)
    void MVN(int rd, int rm) {
        emit(0xAA200000 | (rm << 16) | (arm64::XZR << 5) | rd);
    }
    
    // MUL Xd, Xn, Xm
    void MUL(int rd, int rn, int rm) {
        emit(0x9B007C00 | (rm << 16) | (rn << 5) | rd);
    }
    
    // SMULH Xd, Xn, Xm (signed multiply high)
    void SMULH(int rd, int rn, int rm) {
        emit(0x9B407C00 | (rm << 16) | (rn << 5) | rd);
    }
    
    // SDIV Xd, Xn, Xm (signed divide)
    void SDIV(int rd, int rn, int rm) {
        emit(0x9AC00C00 | (rm << 16) | (rn << 5) | rd);
    }
    
    // UDIV Xd, Xn, Xm (unsigned divide)
    void UDIV(int rd, int rn, int rm) {
        emit(0x9AC00800 | (rm << 16) | (rn << 5) | rd);
    }
    
    // NEG Xd, Xm
    void NEG(int rd, int rm) {
        SUB(rd, arm64::XZR, rm);
    }
    
    // ===== Shifts =====
    
    // LSL Xd, Xn, Xm
    void LSL(int rd, int rn, int rm) {
        emit(0x9AC02000 | (rm << 16) | (rn << 5) | rd);
    }
    
    // LSR Xd, Xn, Xm
    void LSR(int rd, int rn, int rm) {
        emit(0x9AC02400 | (rm << 16) | (rn << 5) | rd);
    }
    
    // ASR Xd, Xn, Xm
    void ASR(int rd, int rn, int rm) {
        emit(0x9AC02800 | (rm << 16) | (rn << 5) | rd);
    }
    
    // ROR Xd, Xn, Xm
    void ROR(int rd, int rn, int rm) {
        emit(0x9AC02C00 | (rm << 16) | (rn << 5) | rd);
    }
    
    // LSL immediate (via UBFM)
    void LSLi(int rd, int rn, uint32_t shift) {
        uint32_t immr = (64 - shift) & 63;
        uint32_t imms = 63 - shift;
        emit(0xD3400000 | (immr << 16) | (imms << 10) | (rn << 5) | rd);
    }
    
    // LSR immediate (via UBFM)
    void LSRi(int rd, int rn, uint32_t shift) {
        emit(0xD340FC00 | (shift << 16) | (rn << 5) | rd);
    }
    
    // ===== Move =====
    
    // MOV Xd, Xm
    void MOV(int rd, int rm) {
        ORR(rd, arm64::XZR, rm);
    }
    
    // MOV Xd, #imm16 (MOVZ)
    void MOVi(int rd, uint16_t imm16, int shift = 0) {
        emit(0xD2800000 | (shift << 21) | (imm16 << 5) | rd);
    }
    
    // MOVK Xd, #imm16, LSL #shift
    void MOVK(int rd, uint16_t imm16, int shift = 0) {
        emit(0xF2800000 | (shift << 21) | (imm16 << 5) | rd);
    }
    
    // Load 64-bit immediate
    void MOV64(int rd, uint64_t imm) {
        MOVi(rd, imm & 0xFFFF, 0);
        if (imm >> 16) MOVK(rd, (imm >> 16) & 0xFFFF, 1);
        if (imm >> 32) MOVK(rd, (imm >> 32) & 0xFFFF, 2);
        if (imm >> 48) MOVK(rd, (imm >> 48) & 0xFFFF, 3);
    }
    
    // ===== Load/Store =====
    
    // LDR Xt, [Xn, #offset]
    void LDR(int rt, int rn, int32_t offset = 0) {
        if (offset >= 0 && (offset & 7) == 0 && offset < 32768) {
            emit(0xF9400000 | ((offset / 8) << 10) | (rn << 5) | rt);
        } else {
            // Use LDUR for unaligned/negative
            emit(0xF8400000 | ((offset & 0x1FF) << 12) | (rn << 5) | rt);
        }
    }
    
    // LDR Wt, [Xn, #offset] (32-bit)
    void LDR32(int rt, int rn, int32_t offset = 0) {
        if (offset >= 0 && (offset & 3) == 0 && offset < 16384) {
            emit(0xB9400000 | ((offset / 4) << 10) | (rn << 5) | rt);
        } else {
            emit(0xB8400000 | ((offset & 0x1FF) << 12) | (rn << 5) | rt);
        }
    }
    
    // LDRB Wt, [Xn, #offset]
    void LDRB(int rt, int rn, int32_t offset = 0) {
        if (offset >= 0 && offset < 4096) {
            emit(0x39400000 | (offset << 10) | (rn << 5) | rt);
        } else {
            emit(0x38400000 | ((offset & 0x1FF) << 12) | (rn << 5) | rt);
        }
    }
    
    // LDRH Wt, [Xn, #offset]
    void LDRH(int rt, int rn, int32_t offset = 0) {
        if (offset >= 0 && (offset & 1) == 0 && offset < 8192) {
            emit(0x79400000 | ((offset / 2) << 10) | (rn << 5) | rt);
        } else {
            emit(0x78400000 | ((offset & 0x1FF) << 12) | (rn << 5) | rt);
        }
    }
    
    // LDR Xt, [Xn, Xm] (register offset)
    void LDRr(int rt, int rn, int rm) {
        emit(0xF8606800 | (rm << 16) | (rn << 5) | rt);
    }
    
    // STR Xt, [Xn, #offset]
    void STR(int rt, int rn, int32_t offset = 0) {
        if (offset >= 0 && (offset & 7) == 0 && offset < 32768) {
            emit(0xF9000000 | ((offset / 8) << 10) | (rn << 5) | rt);
        } else {
            emit(0xF8000000 | ((offset & 0x1FF) << 12) | (rn << 5) | rt);
        }
    }
    
    // STR Wt, [Xn, #offset] (32-bit)
    void STR32(int rt, int rn, int32_t offset = 0) {
        if (offset >= 0 && (offset & 3) == 0 && offset < 16384) {
            emit(0xB9000000 | ((offset / 4) << 10) | (rn << 5) | rt);
        } else {
            emit(0xB8000000 | ((offset & 0x1FF) << 12) | (rn << 5) | rt);
        }
    }
    
    // STRB Wt, [Xn, #offset]
    void STRB(int rt, int rn, int32_t offset = 0) {
        if (offset >= 0 && offset < 4096) {
            emit(0x39000000 | (offset << 10) | (rn << 5) | rt);
        } else {
            emit(0x38000000 | ((offset & 0x1FF) << 12) | (rn << 5) | rt);
        }
    }
    
    // STRH Wt, [Xn, #offset]
    void STRH(int rt, int rn, int32_t offset = 0) {
        if (offset >= 0 && (offset & 1) == 0 && offset < 8192) {
            emit(0x79000000 | ((offset / 2) << 10) | (rn << 5) | rt);
        } else {
            emit(0x78000000 | ((offset & 0x1FF) << 12) | (rn << 5) | rt);
        }
    }
    
    // STR Xt, [Xn, Xm] (register offset)
    void STRr(int rt, int rn, int rm) {
        emit(0xF8206800 | (rm << 16) | (rn << 5) | rt);
    }
    
    // ===== Byte swap (for big-endian) =====
    
    // REV Xd, Xn (reverse bytes 64-bit)
    void REV(int rd, int rn) {
        emit(0xDAC00C00 | (rn << 5) | rd);
    }
    
    // REV32 Xd, Xn (reverse bytes in 32-bit words)
    void REV32(int rd, int rn) {
        emit(0xDAC00800 | (rn << 5) | rd);
    }
    
    // REV16 Xd, Xn (reverse bytes in 16-bit halfwords)
    void REV16(int rd, int rn) {
        emit(0xDAC00400 | (rn << 5) | rd);
    }
    
    // ===== Branches =====
    
    // B offset (unconditional)
    void B(int32_t offset) {
        emit(0x14000000 | ((offset / 4) & 0x03FFFFFF));
    }
    
    // BL offset (branch with link)
    void BL(int32_t offset) {
        emit(0x94000000 | ((offset / 4) & 0x03FFFFFF));
    }
    
    // BR Xn (branch to register)
    void BR(int rn) {
        emit(0xD61F0000 | (rn << 5));
    }
    
    // BLR Xn (branch with link to register)
    void BLR(int rn) {
        emit(0xD63F0000 | (rn << 5));
    }
    
    // RET (return via X30)
    void RET() {
        emit(0xD65F03C0);
    }
    
    // Conditional branches
    void Bcond(int cond, int32_t offset) {
        emit(0x54000000 | (((offset / 4) & 0x7FFFF) << 5) | cond);
    }
    
    void BEQ(int32_t offset) { Bcond(0, offset); }
    void BNE(int32_t offset) { Bcond(1, offset); }
    void BLT(int32_t offset) { Bcond(11, offset); }
    void BGE(int32_t offset) { Bcond(10, offset); }
    void BLE(int32_t offset) { Bcond(13, offset); }
    void BGT(int32_t offset) { Bcond(12, offset); }
    
    // CBZ Xt, offset
    void CBZ(int rt, int32_t offset) {
        emit(0xB4000000 | (((offset / 4) & 0x7FFFF) << 5) | rt);
    }
    
    // CBNZ Xt, offset
    void CBNZ(int rt, int32_t offset) {
        emit(0xB5000000 | (((offset / 4) & 0x7FFFF) << 5) | rt);
    }
    
    // ===== Compare =====
    
    // CMP Xn, Xm (alias for SUBS XZR, Xn, Xm)
    void CMP(int rn, int rm) {
        SUBS(arm64::XZR, rn, rm);
    }
    
    // CMP Xn, #imm12
    void CMPi(int rn, uint32_t imm12) {
        emit(0xF100001F | ((imm12 & 0xFFF) << 10) | (rn << 5));
    }
    
    // TST Xn, Xm (alias for ANDS XZR, Xn, Xm)
    void TST(int rn, int rm) {
        ANDS(arm64::XZR, rn, rm);
    }
    
    // ===== Sign/Zero extend =====
    
    // SXTB Xd, Wn (sign extend byte)
    void SXTB(int rd, int rn) {
        emit(0x93401C00 | (rn << 5) | rd);
    }
    
    // SXTH Xd, Wn (sign extend halfword)
    void SXTH(int rd, int rn) {
        emit(0x93403C00 | (rn << 5) | rd);
    }
    
    // SXTW Xd, Wn (sign extend word)
    void SXTW(int rd, int rn) {
        emit(0x93407C00 | (rn << 5) | rd);
    }
    
    // UXTB Xd, Wn (zero extend byte)
    void UXTB(int rd, int rn) {
        emit(0xD3401C00 | (rn << 5) | rd);
    }
    
    // UXTH Xd, Wn (zero extend halfword)
    void UXTH(int rd, int rn) {
        emit(0xD3403C00 | (rn << 5) | rd);
    }
    
    // ===== Stack =====
    
    // STP Xt1, Xt2, [SP, #offset]!
    void STP_pre(int rt1, int rt2, int rn, int32_t offset) {
        emit(0xA9800000 | (((offset / 8) & 0x7F) << 15) | (rt2 << 10) | (rn << 5) | rt1);
    }
    
    // LDP Xt1, Xt2, [SP], #offset
    void LDP_post(int rt1, int rt2, int rn, int32_t offset) {
        emit(0xA8C00000 | (((offset / 8) & 0x7F) << 15) | (rt2 << 10) | (rn << 5) | rt1);
    }
    
    // ===== Misc =====
    
    // NOP
    void NOP() {
        emit(0xD503201F);
    }
    
    // BRK #imm16 (breakpoint)
    void BRK(uint16_t imm = 0) {
        emit(0xD4200000 | (imm << 5));
    }
};

// ============================================================================
// PPU Instruction Translators
// ============================================================================

class PPUTranslator {
public:
    ARM64Emitter& emit;
    
    explicit PPUTranslator(ARM64Emitter& e) : emit(e) {}
    
    // GPR offset in PPUState
    static constexpr int GPR_OFFSET = 0;  // gpr[32] at start
    static constexpr int LR_OFFSET = 32 * 8;
    static constexpr int CTR_OFFSET = 32 * 8 + 8;
    static constexpr int CR_OFFSET = 32 * 8 + 16;
    static constexpr int PC_OFFSET = 32 * 8 + 32;
    
    // Load GPR[n] into ARM64 register
    void LoadGPR(int arm_reg, int ppu_reg) {
        emit.LDR(arm_reg, arm64::REG_STATE, GPR_OFFSET + ppu_reg * 8);
    }
    
    // Store ARM64 register into GPR[n]
    void StoreGPR(int ppu_reg, int arm_reg) {
        emit.STR(arm_reg, arm64::REG_STATE, GPR_OFFSET + ppu_reg * 8);
    }
    
    // Load from guest memory (with byte swap for big-endian)
    void LoadMem32(int arm_reg, int addr_reg) {
        emit.LDR32(arm_reg, addr_reg);
        emit.REV32(arm_reg, arm_reg);  // Big-endian swap
    }
    
    void StoreMem32(int addr_reg, int val_reg) {
        emit.REV32(arm64::REG_TMP3, val_reg);  // Swap before store
        emit.STR32(arm64::REG_TMP3, addr_reg);
    }
    
    // Update PC in state
    void UpdatePC(uint32_t pc) {
        emit.MOV64(arm64::REG_TMP0, pc);
        emit.STR(arm64::REG_TMP0, arm64::REG_STATE, PC_OFFSET);
    }
    
    // ===== Arithmetic Instructions =====
    
    // ADD rD, rA, rB
    void INSN_ADD(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.ra());
        LoadGPR(arm64::REG_TMP1, op.rb());
        emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.rd(), arm64::REG_TMP0);
    }
    
    // ADDI rD, rA, SIMM
    void INSN_ADDI(PPUOpcode op) {
        if (op.ra() == 0) {
            // li rD, SIMM
            emit.MOV64(arm64::REG_TMP0, static_cast<uint64_t>(static_cast<int64_t>(op.simm16())));
        } else {
            LoadGPR(arm64::REG_TMP0, op.ra());
            if (op.simm16() >= 0) {
                emit.ADDi(arm64::REG_TMP0, arm64::REG_TMP0, op.simm16());
            } else {
                emit.SUBi(arm64::REG_TMP0, arm64::REG_TMP0, -op.simm16());
            }
        }
        StoreGPR(op.rd(), arm64::REG_TMP0);
    }
    
    // ADDIS rD, rA, SIMM (add immediate shifted)
    void INSN_ADDIS(PPUOpcode op) {
        int32_t imm = static_cast<int32_t>(op.simm16()) << 16;
        if (op.ra() == 0) {
            // lis rD, SIMM
            emit.MOV64(arm64::REG_TMP0, static_cast<uint64_t>(static_cast<int64_t>(imm)));
        } else {
            LoadGPR(arm64::REG_TMP0, op.ra());
            emit.MOV64(arm64::REG_TMP1, static_cast<uint64_t>(static_cast<int64_t>(imm)));
            emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        }
        StoreGPR(op.rd(), arm64::REG_TMP0);
    }
    
    // SUBF rD, rA, rB (rD = rB - rA)
    void INSN_SUBF(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rb());
        LoadGPR(arm64::REG_TMP1, op.ra());
        emit.SUB(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.rd(), arm64::REG_TMP0);
    }
    
    // MULLW rD, rA, rB (32-bit multiply)
    void INSN_MULLW(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.ra());
        LoadGPR(arm64::REG_TMP1, op.rb());
        emit.MUL(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        // Sign-extend 32-bit result
        emit.SXTW(arm64::REG_TMP0, arm64::REG_TMP0);
        StoreGPR(op.rd(), arm64::REG_TMP0);
    }
    
    // DIVW rD, rA, rB (32-bit signed divide)
    void INSN_DIVW(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.ra());
        LoadGPR(arm64::REG_TMP1, op.rb());
        emit.SXTW(arm64::REG_TMP0, arm64::REG_TMP0);
        emit.SXTW(arm64::REG_TMP1, arm64::REG_TMP1);
        emit.SDIV(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.rd(), arm64::REG_TMP0);
    }
    
    // ===== Logical Instructions =====
    
    // AND rA, rS, rB
    void INSN_AND(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        LoadGPR(arm64::REG_TMP1, op.rb());
        emit.AND(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.ra(), arm64::REG_TMP0);
    }
    
    // ANDI. rA, rS, UIMM
    void INSN_ANDI(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        emit.MOV64(arm64::REG_TMP1, op.uimm16());
        emit.AND(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.ra(), arm64::REG_TMP0);
        // TODO: Update CR0
    }
    
    // OR rA, rS, rB
    void INSN_OR(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        LoadGPR(arm64::REG_TMP1, op.rb());
        emit.ORR(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.ra(), arm64::REG_TMP0);
    }
    
    // ORI rA, rS, UIMM
    void INSN_ORI(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        emit.MOV64(arm64::REG_TMP1, op.uimm16());
        emit.ORR(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.ra(), arm64::REG_TMP0);
    }
    
    // ORIS rA, rS, UIMM
    void INSN_ORIS(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        emit.MOV64(arm64::REG_TMP1, static_cast<uint32_t>(op.uimm16()) << 16);
        emit.ORR(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.ra(), arm64::REG_TMP0);
    }
    
    // XOR rA, rS, rB
    void INSN_XOR(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        LoadGPR(arm64::REG_TMP1, op.rb());
        emit.EOR(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.ra(), arm64::REG_TMP0);
    }
    
    // XORI rA, rS, UIMM
    void INSN_XORI(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        emit.MOV64(arm64::REG_TMP1, op.uimm16());
        emit.EOR(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.ra(), arm64::REG_TMP0);
    }
    
    // ===== Load/Store Instructions =====
    
    // LWZ rD, d(rA) - Load Word and Zero
    void INSN_LWZ(PPUOpcode op) {
        if (op.ra() == 0) {
            emit.MOV64(arm64::REG_TMP0, static_cast<uint64_t>(static_cast<int64_t>(op.simm16())));
        } else {
            LoadGPR(arm64::REG_TMP0, op.ra());
            if (op.simm16() != 0) {
                emit.MOV64(arm64::REG_TMP1, static_cast<uint64_t>(static_cast<int64_t>(op.simm16())));
                emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
            }
        }
        // Add memory base
        emit.ADD(arm64::REG_TMP0, arm64::REG_MEM, arm64::REG_TMP0);
        // Load and byte-swap
        emit.LDR32(arm64::REG_TMP0, arm64::REG_TMP0);
        emit.REV32(arm64::REG_TMP0, arm64::REG_TMP0);
        StoreGPR(op.rd(), arm64::REG_TMP0);
    }
    
    // STW rS, d(rA) - Store Word
    void INSN_STW(PPUOpcode op) {
        // Calculate address
        if (op.ra() == 0) {
            emit.MOV64(arm64::REG_TMP0, static_cast<uint64_t>(static_cast<int64_t>(op.simm16())));
        } else {
            LoadGPR(arm64::REG_TMP0, op.ra());
            if (op.simm16() != 0) {
                emit.MOV64(arm64::REG_TMP1, static_cast<uint64_t>(static_cast<int64_t>(op.simm16())));
                emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
            }
        }
        emit.ADD(arm64::REG_TMP0, arm64::REG_MEM, arm64::REG_TMP0);
        // Get value and byte-swap
        LoadGPR(arm64::REG_TMP1, op.rs());
        emit.REV32(arm64::REG_TMP1, arm64::REG_TMP1);
        emit.STR32(arm64::REG_TMP1, arm64::REG_TMP0);
    }
    
    // LBZ rD, d(rA) - Load Byte and Zero
    void INSN_LBZ(PPUOpcode op) {
        if (op.ra() == 0) {
            emit.MOV64(arm64::REG_TMP0, static_cast<uint64_t>(static_cast<int64_t>(op.simm16())));
        } else {
            LoadGPR(arm64::REG_TMP0, op.ra());
            if (op.simm16() != 0) {
                emit.MOV64(arm64::REG_TMP1, static_cast<uint64_t>(static_cast<int64_t>(op.simm16())));
                emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
            }
        }
        emit.ADD(arm64::REG_TMP0, arm64::REG_MEM, arm64::REG_TMP0);
        emit.LDRB(arm64::REG_TMP0, arm64::REG_TMP0);
        StoreGPR(op.rd(), arm64::REG_TMP0);
    }
    
    // STB rS, d(rA) - Store Byte
    void INSN_STB(PPUOpcode op) {
        if (op.ra() == 0) {
            emit.MOV64(arm64::REG_TMP0, static_cast<uint64_t>(static_cast<int64_t>(op.simm16())));
        } else {
            LoadGPR(arm64::REG_TMP0, op.ra());
            if (op.simm16() != 0) {
                emit.MOV64(arm64::REG_TMP1, static_cast<uint64_t>(static_cast<int64_t>(op.simm16())));
                emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
            }
        }
        emit.ADD(arm64::REG_TMP0, arm64::REG_MEM, arm64::REG_TMP0);
        LoadGPR(arm64::REG_TMP1, op.rs());
        emit.STRB(arm64::REG_TMP1, arm64::REG_TMP0);
    }
    
    // LD rD, ds(rA) - Load Doubleword
    void INSN_LD(PPUOpcode op) {
        int32_t offset = static_cast<int16_t>(op.raw & 0xFFFC);
        if (op.ra() == 0) {
            emit.MOV64(arm64::REG_TMP0, static_cast<uint64_t>(static_cast<int64_t>(offset)));
        } else {
            LoadGPR(arm64::REG_TMP0, op.ra());
            if (offset != 0) {
                emit.MOV64(arm64::REG_TMP1, static_cast<uint64_t>(static_cast<int64_t>(offset)));
                emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
            }
        }
        emit.ADD(arm64::REG_TMP0, arm64::REG_MEM, arm64::REG_TMP0);
        emit.LDR(arm64::REG_TMP0, arm64::REG_TMP0);
        emit.REV(arm64::REG_TMP0, arm64::REG_TMP0);  // 64-bit byte swap
        StoreGPR(op.rd(), arm64::REG_TMP0);
    }
    
    // STD rS, ds(rA) - Store Doubleword
    void INSN_STD(PPUOpcode op) {
        int32_t offset = static_cast<int16_t>(op.raw & 0xFFFC);
        if (op.ra() == 0) {
            emit.MOV64(arm64::REG_TMP0, static_cast<uint64_t>(static_cast<int64_t>(offset)));
        } else {
            LoadGPR(arm64::REG_TMP0, op.ra());
            if (offset != 0) {
                emit.MOV64(arm64::REG_TMP1, static_cast<uint64_t>(static_cast<int64_t>(offset)));
                emit.ADD(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
            }
        }
        emit.ADD(arm64::REG_TMP0, arm64::REG_MEM, arm64::REG_TMP0);
        LoadGPR(arm64::REG_TMP1, op.rs());
        emit.REV(arm64::REG_TMP1, arm64::REG_TMP1);
        emit.STR(arm64::REG_TMP1, arm64::REG_TMP0);
    }
    
    // ===== Shift/Rotate Instructions =====
    
    // SLW rA, rS, rB (shift left word)
    void INSN_SLW(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        LoadGPR(arm64::REG_TMP1, op.rb());
        // Mask shift amount to 5 bits
        emit.MOV64(arm64::REG_TMP2, 31);
        emit.AND(arm64::REG_TMP1, arm64::REG_TMP1, arm64::REG_TMP2);
        emit.LSL(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        // Clear upper 32 bits
        emit.MOV64(arm64::REG_TMP1, 0xFFFFFFFF);
        emit.AND(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.ra(), arm64::REG_TMP0);
    }
    
    // SRW rA, rS, rB (shift right word)
    void INSN_SRW(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        // Clear upper 32 bits first
        emit.MOV64(arm64::REG_TMP2, 0xFFFFFFFF);
        emit.AND(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP2);
        LoadGPR(arm64::REG_TMP1, op.rb());
        emit.MOV64(arm64::REG_TMP2, 31);
        emit.AND(arm64::REG_TMP1, arm64::REG_TMP1, arm64::REG_TMP2);
        emit.LSR(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.ra(), arm64::REG_TMP0);
    }
    
    // RLWINM rA, rS, SH, MB, ME (rotate left word immediate then and with mask)
    void INSN_RLWINM(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        // Rotate left by SH
        if (op.sh() != 0) {
            emit.MOV64(arm64::REG_TMP1, op.sh());
            emit.ROR(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
            // Actually need rotate left = rotate right by (32 - sh)
            // But PPU is 64-bit... simplified for now
        }
        // Apply mask
        uint32_t mb = op.mb();
        uint32_t me = op.me();
        uint32_t mask = 0;
        if (mb <= me) {
            for (uint32_t i = mb; i <= me; i++) mask |= (1u << (31 - i));
        } else {
            for (uint32_t i = 0; i <= me; i++) mask |= (1u << (31 - i));
            for (uint32_t i = mb; i <= 31; i++) mask |= (1u << (31 - i));
        }
        emit.MOV64(arm64::REG_TMP1, mask);
        emit.AND(arm64::REG_TMP0, arm64::REG_TMP0, arm64::REG_TMP1);
        StoreGPR(op.ra(), arm64::REG_TMP0);
    }
    
    // ===== Compare Instructions =====
    
    // CMP crfD, L, rA, rB
    void INSN_CMP(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.ra());
        LoadGPR(arm64::REG_TMP1, op.rb());
        emit.CMP(arm64::REG_TMP0, arm64::REG_TMP1);
        // TODO: Set CR field based on comparison result
        // This requires reading ARM64 flags and setting PPU CR
    }
    
    // CMPI crfD, L, rA, SIMM
    void INSN_CMPI(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.ra());
        emit.MOV64(arm64::REG_TMP1, static_cast<uint64_t>(static_cast<int64_t>(op.simm16())));
        emit.CMP(arm64::REG_TMP0, arm64::REG_TMP1);
        // TODO: Set CR field
    }
    
    // CMPLI crfD, L, rA, UIMM
    void INSN_CMPLI(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.ra());
        emit.MOV64(arm64::REG_TMP1, op.uimm16());
        emit.CMP(arm64::REG_TMP0, arm64::REG_TMP1);
        // TODO: Set CR field (unsigned comparison)
    }
    
    // ===== Move to/from Special Registers =====
    
    // MFLR rD
    void INSN_MFLR(PPUOpcode op) {
        emit.LDR(arm64::REG_TMP0, arm64::REG_STATE, LR_OFFSET);
        StoreGPR(op.rd(), arm64::REG_TMP0);
    }
    
    // MTLR rS
    void INSN_MTLR(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        emit.STR(arm64::REG_TMP0, arm64::REG_STATE, LR_OFFSET);
    }
    
    // MFCTR rD
    void INSN_MFCTR(PPUOpcode op) {
        emit.LDR(arm64::REG_TMP0, arm64::REG_STATE, CTR_OFFSET);
        StoreGPR(op.rd(), arm64::REG_TMP0);
    }
    
    // MTCTR rS
    void INSN_MTCTR(PPUOpcode op) {
        LoadGPR(arm64::REG_TMP0, op.rs());
        emit.STR(arm64::REG_TMP0, arm64::REG_STATE, CTR_OFFSET);
    }
};

} // namespace rpcsx::nce::ppu_insn
