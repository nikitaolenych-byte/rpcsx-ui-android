/**
 * ARM64 Code Emitter для NCE JIT
 * Генерує нативний ARM64 код з PowerPC інструкцій (PS3 Cell PPU)
 */

#ifndef RPCSX_ARM64_EMITTER_H
#define RPCSX_ARM64_EMITTER_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "ppc_decoder.h"

namespace rpcsx::nce::arm64 {

// ARM64 General Purpose Registers
enum class GpReg : uint8_t {
    X0 = 0, X1 = 1, X2 = 2, X3 = 3, X4 = 4, X5 = 5, X6 = 6, X7 = 7,
    X8 = 8, X9 = 9, X10 = 10, X11 = 11, X12 = 12, X13 = 13, X14 = 14, X15 = 15,
    X16 = 16, X17 = 17, X18 = 18, X19 = 19, X20 = 20, X21 = 21, X22 = 22, X23 = 23,
    X24 = 24, X25 = 25, X26 = 26, X27 = 27, X28 = 28, X29 = 29, X30 = 30,
    SP = 31,  // Stack pointer
    ZR = 31   // Zero register (context dependent)
};

// ARM64 SIMD/FP Registers
enum class VecReg : uint8_t {
    V0 = 0, V1 = 1, V2 = 2, V3 = 3, V4 = 4, V5 = 5, V6 = 6, V7 = 7,
    V8 = 8, V9 = 9, V10 = 10, V11 = 11, V12 = 12, V13 = 13, V14 = 14, V15 = 15,
    V16 = 16, V17 = 17, V18 = 18, V19 = 19, V20 = 20, V21 = 21, V22 = 22, V23 = 23,
    V24 = 24, V25 = 25, V26 = 26, V27 = 27, V28 = 28, V29 = 29, V30 = 30, V31 = 31
};

// Condition codes for ARM64
enum class Cond : uint8_t {
    EQ = 0,   // Equal
    NE = 1,   // Not equal
    CS = 2,   // Carry set (unsigned >=)
    CC = 3,   // Carry clear (unsigned <)
    MI = 4,   // Minus/negative
    PL = 5,   // Plus/positive
    VS = 6,   // Overflow
    VC = 7,   // No overflow
    HI = 8,   // Unsigned >
    LS = 9,   // Unsigned <=
    GE = 10,  // Signed >=
    LT = 11,  // Signed <
    GT = 12,  // Signed >
    LE = 13,  // Signed <=
    AL = 14   // Always
};

// Special registers on ARM64 (must be defined before Emitter class)
constexpr GpReg REG_STATE = GpReg::X29;   // PPU state pointer
constexpr GpReg REG_LR = GpReg::X30;      // Link Register
constexpr GpReg REG_CTR = GpReg::X22;     // Count Register
constexpr GpReg REG_XER = GpReg::X23;     // XER (carry, overflow)
constexpr GpReg REG_CR = GpReg::X24;      // Condition Register
constexpr GpReg REG_TMP1 = GpReg::X25;    // Temp register 1
constexpr GpReg REG_TMP2 = GpReg::X26;    // Temp register 2

/**
 * ARM64 Code Buffer with instruction emission
 */
class CodeBuffer {
public:
    CodeBuffer(void* buffer, size_t capacity)
        : base_(static_cast<uint32_t*>(buffer))
        , current_(static_cast<uint32_t*>(buffer))
        , capacity_(capacity / 4) {}
    
    // Current position in buffer
    uint32_t* GetCurrent() const { return current_; }
    size_t GetOffset() const { return (current_ - base_) * 4; }
    size_t GetRemaining() const { return (capacity_ - (current_ - base_)) * 4; }
    
    // Emit raw instruction
    void Emit(uint32_t instr) {
        if (current_ - base_ < static_cast<ptrdiff_t>(capacity_)) {
            *current_++ = instr;
        }
    }
    
    // Reset buffer
    void Reset() { current_ = base_; }
    
private:
    uint32_t* base_;
    uint32_t* current_;
    size_t capacity_;
};

/**
 * ARM64 Instruction Emitter
 * Reference: ARM Architecture Reference Manual ARMv8
 */
class Emitter {
public:
    explicit Emitter(CodeBuffer& buf) : buf_(buf) {}
    
    // Raw instruction emission (for direct encoding)
    void Emit(uint32_t instr) {
        buf_.Emit(instr);
    }
    
    // ============================================
    // Data Movement
    // ============================================
    
    // MOV Xd, Xm (ORR Xd, XZR, Xm)
    void MOV(GpReg rd, GpReg rm) {
        // ORR Xd, XZR, Xm: 10101010000mmmmm00000011111ddddd
        buf_.Emit(0xAA0003E0 | (static_cast<uint32_t>(rm) << 16) | static_cast<uint32_t>(rd));
    }
    
    // MOV Xd, #imm16
    void MOVZ(GpReg rd, uint16_t imm, uint8_t shift = 0) {
        // MOVZ Xd, #imm16, LSL #shift
        // 110100101ssiiiiiiiiiiiiiiiiddddd
        uint8_t hw = shift / 16;
        buf_.Emit(0xD2800000 | (hw << 21) | (imm << 5) | static_cast<uint32_t>(rd));
    }
    
    // MOVK Xd, #imm16, LSL #shift (keep other bits)
    void MOVK(GpReg rd, uint16_t imm, uint8_t shift = 0) {
        uint8_t hw = shift / 16;
        buf_.Emit(0xF2800000 | (hw << 21) | (imm << 5) | static_cast<uint32_t>(rd));
    }
    
    // Full 64-bit immediate load
    void MOV_IMM64(GpReg rd, uint64_t imm) {
        MOVZ(rd, imm & 0xFFFF, 0);
        if (imm > 0xFFFF) {
            MOVK(rd, (imm >> 16) & 0xFFFF, 16);
        }
        if (imm > 0xFFFFFFFF) {
            MOVK(rd, (imm >> 32) & 0xFFFF, 32);
            MOVK(rd, (imm >> 48) & 0xFFFF, 48);
        }
    }
    
    // LDR Xt, [Xn, #offset]
    void LDR(GpReg rt, GpReg rn, int32_t offset = 0) {
        // Unsigned offset (must be 8-byte aligned for 64-bit)
        uint32_t imm12 = (offset >> 3) & 0xFFF;
        buf_.Emit(0xF9400000 | (imm12 << 10) | (static_cast<uint32_t>(rn) << 5) | 
                  static_cast<uint32_t>(rt));
    }
    
    // STR Xt, [Xn, #offset]
    void STR(GpReg rt, GpReg rn, int32_t offset = 0) {
        uint32_t imm12 = (offset >> 3) & 0xFFF;
        buf_.Emit(0xF9000000 | (imm12 << 10) | (static_cast<uint32_t>(rn) << 5) | 
                  static_cast<uint32_t>(rt));
    }
    
    // STP Xt1, Xt2, [Xn, #offset]! (pre-index)
    void STP_PRE(GpReg rt1, GpReg rt2, GpReg rn, int32_t offset) {
        int32_t imm7 = (offset >> 3) & 0x7F;
        buf_.Emit(0xA9800000 | (imm7 << 15) | (static_cast<uint32_t>(rt2) << 10) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rt1));
    }
    
    // LDP Xt1, Xt2, [Xn], #offset (post-index)
    void LDP_POST(GpReg rt1, GpReg rt2, GpReg rn, int32_t offset) {
        int32_t imm7 = (offset >> 3) & 0x7F;
        buf_.Emit(0xA8C00000 | (imm7 << 15) | (static_cast<uint32_t>(rt2) << 10) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rt1));
    }
    
    // ============================================
    // Arithmetic
    // ============================================
    
    // ADD Xd, Xn, Xm
    void ADD(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x8B000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ADD Xd, Xn, #imm12
    void ADD_IMM(GpReg rd, GpReg rn, uint16_t imm12) {
        buf_.Emit(0x91000000 | ((imm12 & 0xFFF) << 10) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ADDS Xd, Xn, Xm (set flags)
    void ADDS(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0xAB000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SUB Xd, Xn, Xm
    void SUB(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0xCB000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SUB Xd, Xn, #imm12
    void SUB_IMM(GpReg rd, GpReg rn, uint16_t imm12) {
        buf_.Emit(0xD1000000 | ((imm12 & 0xFFF) << 10) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SUBS Xd, Xn, Xm (set flags)
    void SUBS(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0xEB000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // MUL Xd, Xn, Xm
    void MUL(GpReg rd, GpReg rn, GpReg rm) {
        // MADD Xd, Xn, Xm, XZR
        buf_.Emit(0x9B007C00 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SDIV Xd, Xn, Xm
    void SDIV(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x9AC00C00 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // UDIV Xd, Xn, Xm
    void UDIV(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x9AC00800 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // NEG Xd, Xm (SUB Xd, XZR, Xm)
    void NEG(GpReg rd, GpReg rm) {
        SUB(rd, GpReg::ZR, rm);
    }
    
    // ============================================
    // Logic
    // ============================================
    
    // AND Xd, Xn, Xm
    void AND(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x8A000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ANDS Xd, Xn, Xm (set flags)
    void ANDS(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0xEA000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ORR Xd, Xn, Xm
    void ORR(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0xAA000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // EOR Xd, Xn, Xm (XOR)
    void EOR(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0xCA000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // MVN Xd, Xm (ORN Xd, XZR, Xm)
    void MVN(GpReg rd, GpReg rm) {
        buf_.Emit(0xAA2003E0 | (static_cast<uint32_t>(rm) << 16) | static_cast<uint32_t>(rd));
    }
    
    // LSL Xd, Xn, Xm
    void LSL(GpReg rd, GpReg rn, GpReg rm) {
        // LSLV Xd, Xn, Xm
        buf_.Emit(0x9AC02000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // LSR Xd, Xn, Xm
    void LSR(GpReg rd, GpReg rn, GpReg rm) {
        // LSRV Xd, Xn, Xm
        buf_.Emit(0x9AC02400 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ASR Xd, Xn, Xm
    void ASR(GpReg rd, GpReg rn, GpReg rm) {
        // ASRV Xd, Xn, Xm
        buf_.Emit(0x9AC02800 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ============================================
    // Compare
    // ============================================
    
    // CMP Xn, Xm (SUBS XZR, Xn, Xm)
    void CMP(GpReg rn, GpReg rm) {
        SUBS(GpReg::ZR, rn, rm);
    }
    
    // CMP Xn, #imm12 (signed)
    void CMP_IMM(GpReg rn, int16_t imm) {
        if (imm >= 0) {
            buf_.Emit(0xF1000000 | ((imm & 0xFFF) << 10) | 
                      (static_cast<uint32_t>(rn) << 5) | 31);
        } else {
            // Use CMN (add) for negative immediate
            buf_.Emit(0xB1000000 | (((-imm) & 0xFFF) << 10) | 
                      (static_cast<uint32_t>(rn) << 5) | 31);
        }
    }
    
    // TST Xn, Xm (ANDS XZR, Xn, Xm)
    void TST(GpReg rn, GpReg rm) {
        ANDS(GpReg::ZR, rn, rm);
    }
    
    // ============================================
    // Additional Arithmetic for PowerPC
    // ============================================
    
    // ADC Xd, Xn, Xm (add with carry)
    void ADC(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x9A000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ADCS Xd, Xn, Xm (add with carry, set flags)
    void ADCS(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0xBA000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SBC Xd, Xn, Xm (subtract with carry/borrow)
    void SBC(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0xDA000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SBCS Xd, Xn, Xm (subtract with carry, set flags)
    void SBCS(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0xFA000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // MADD Xd, Xn, Xm, Xa (multiply-add: Xd = Xa + Xn*Xm)
    void MADD(GpReg rd, GpReg rn, GpReg rm, GpReg ra) {
        buf_.Emit(0x9B000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(ra) << 10) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // MSUB Xd, Xn, Xm, Xa (multiply-subtract: Xd = Xa - Xn*Xm)
    void MSUB(GpReg rd, GpReg rn, GpReg rm, GpReg ra) {
        buf_.Emit(0x9B008000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(ra) << 10) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SMULH Xd, Xn, Xm (signed multiply high)
    void SMULH(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x9B407C00 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // UMULH Xd, Xn, Xm (unsigned multiply high)
    void UMULH(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x9BC07C00 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // CLZ Xd, Xn (count leading zeros)
    void CLZ(GpReg rd, GpReg rn) {
        buf_.Emit(0xDAC01000 | (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // CLZ Wd, Wn (count leading zeros, 32-bit)
    void CLZ_W(GpReg rd, GpReg rn) {
        buf_.Emit(0x5AC01000 | (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // RBIT Xd, Xn (reverse bits)
    void RBIT(GpReg rd, GpReg rn) {
        buf_.Emit(0xDAC00000 | (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ============================================
    // Additional Logic for PowerPC
    // ============================================
    
    // BIC Xd, Xn, Xm (AND NOT: Xd = Xn & ~Xm)
    void BIC(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x8A200000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ORN Xd, Xn, Xm (OR NOT: Xd = Xn | ~Xm)
    void ORN(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0xAA200000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // EON Xd, Xn, Xm (XOR NOT: Xd = Xn ^ ~Xm)
    void EON(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0xCA200000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // AND Xd, Xn, #imm (logical immediate)
    void AND_IMM(GpReg rd, GpReg rn, uint64_t imm) {
        // Note: ARM64 bitmask immediates have specific encoding rules
        // For simplicity, use MOV + AND if immediate is complex
        uint32_t n, immr, imms;
        if (EncodeBitmaskImm(imm, n, immr, imms)) {
            buf_.Emit(0x92000000 | (n << 22) | (immr << 16) | (imms << 10) |
                      (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
        }
    }
    
    // ORR Xd, Xn, #imm (logical immediate)
    void ORR_IMM(GpReg rd, GpReg rn, uint64_t imm) {
        uint32_t n, immr, imms;
        if (EncodeBitmaskImm(imm, n, immr, imms)) {
            buf_.Emit(0xB2000000 | (n << 22) | (immr << 16) | (imms << 10) |
                      (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
        }
    }
    
    // EOR Xd, Xn, #imm (logical immediate)  
    void EOR_IMM(GpReg rd, GpReg rn, uint64_t imm) {
        uint32_t n, immr, imms;
        if (EncodeBitmaskImm(imm, n, immr, imms)) {
            buf_.Emit(0xD2000000 | (n << 22) | (immr << 16) | (imms << 10) |
                      (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
        }
    }
    
    // LSL Xd, Xn, #imm (shift left immediate via UBFM)
    void LSL_IMM(GpReg rd, GpReg rn, uint8_t imm) {
        // LSL Xd, Xn, #shift = UBFM Xd, Xn, #(-shift MOD 64), #(63-shift)
        uint8_t immr = (-imm) & 63;
        uint8_t imms = 63 - imm;
        buf_.Emit(0xD3400000 | (immr << 16) | (imms << 10) |
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // LSR Xd, Xn, #imm (shift right immediate via UBFM)
    void LSR_IMM(GpReg rd, GpReg rn, uint8_t imm) {
        // LSR Xd, Xn, #shift = UBFM Xd, Xn, #shift, #63
        buf_.Emit(0xD340FC00 | (imm << 16) |
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ============================================
    // Conditional Select (for PowerPC isel equivalent)
    // ============================================
    
    // CSEL Xd, Xn, Xm, cond (conditional select)
    void CSEL(GpReg rd, GpReg rn, GpReg rm, Cond cond) {
        buf_.Emit(0x9A800000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(cond) << 12) |
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // CSINC Xd, Xn, Xm, cond (conditional select increment)
    void CSINC(GpReg rd, GpReg rn, GpReg rm, Cond cond) {
        buf_.Emit(0x9A800400 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(cond) << 12) |
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // CSINV Xd, Xn, Xm, cond (conditional select invert)
    void CSINV(GpReg rd, GpReg rn, GpReg rm, Cond cond) {
        buf_.Emit(0xDA800000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(cond) << 12) |
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // CSNEG Xd, Xn, Xm, cond (conditional select negate)
    void CSNEG(GpReg rd, GpReg rn, GpReg rm, Cond cond) {
        buf_.Emit(0xDA800400 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(cond) << 12) |
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // CSET Xd, cond (set to 1 if condition, else 0)
    void CSET(GpReg rd, Cond cond) {
        // CSET Xd, cond = CSINC Xd, XZR, XZR, invert(cond)
        Cond inv_cond = static_cast<Cond>(static_cast<uint8_t>(cond) ^ 1);
        CSINC(rd, GpReg::ZR, GpReg::ZR, inv_cond);
    }
    
    // ============================================
    // Bitfield operations (for PowerPC rotate/mask)
    // ============================================
    
    // UBFM Xd, Xn, #immr, #imms (unsigned bitfield move)
    void UBFM(GpReg rd, GpReg rn, uint8_t immr, uint8_t imms) {
        buf_.Emit(0xD3400000 | (immr << 16) | (imms << 10) |
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SBFM Xd, Xn, #immr, #imms (signed bitfield move)
    void SBFM(GpReg rd, GpReg rn, uint8_t immr, uint8_t imms) {
        buf_.Emit(0x93400000 | (immr << 16) | (imms << 10) |
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // BFM Xd, Xn, #immr, #imms (bitfield move - for insert)
    void BFM(GpReg rd, GpReg rn, uint8_t immr, uint8_t imms) {
        buf_.Emit(0xB3400000 | (immr << 16) | (imms << 10) |
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // EXTR Xd, Xn, Xm, #lsb (extract bits from pair)
    void EXTR(GpReg rd, GpReg rn, GpReg rm, uint8_t lsb) {
        buf_.Emit(0x93C00000 | (static_cast<uint32_t>(rm) << 16) | 
                  (lsb << 10) | (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ============================================
    // 32-bit variants (for 32-bit PowerPC ops)
    // ============================================
    
    // ADD Wd, Wn, Wm
    void ADD_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x0B000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ADDS Wd, Wn, Wm
    void ADDS_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x2B000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SUB Wd, Wn, Wm
    void SUB_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x4B000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SUBS Wd, Wn, Wm
    void SUBS_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x6B000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // MUL Wd, Wn, Wm
    void MUL_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x1B007C00 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SDIV Wd, Wn, Wm
    void SDIV_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x1AC00C00 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // UDIV Wd, Wn, Wm
    void UDIV_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x1AC00800 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // AND Wd, Wn, Wm
    void AND_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x0A000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ORR Wd, Wn, Wm
    void ORR_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x2A000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // EOR Wd, Wn, Wm
    void EOR_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x4A000000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // LSL Wd, Wn, Wm
    void LSL_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x1AC02000 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // LSR Wd, Wn, Wm
    void LSR_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x1AC02400 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ASR Wd, Wn, Wm
    void ASR_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x1AC02800 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ROR Wd, Wn, Wm
    void ROR_W(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x1AC02C00 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // CMP Wn, Wm
    void CMP_W(GpReg rn, GpReg rm) {
        SUBS_W(GpReg::ZR, rn, rm);
    }
    
    // Zero-extend W to X
    void UXTW(GpReg rd, GpReg rn) {
        // MOV Wd, Wn (32-bit move zero-extends to 64-bit)
        buf_.Emit(0x2A0003E0 | (static_cast<uint32_t>(rn) << 16) | static_cast<uint32_t>(rd));
    }
    
    // Helper: try to encode bitmask immediate
    // Returns false if immediate cannot be encoded
    static bool EncodeBitmaskImm(uint64_t imm, uint32_t& n, uint32_t& immr, uint32_t& imms) {
        // ARM64 bitmask immediates are complex - simplified version
        // Only handles common cases
        if (imm == 0 || imm == ~0ULL) return false;
        
        // For now, return false for complex cases
        // A full implementation would analyze bit patterns
        n = 1;
        immr = 0;
        imms = 0;
        
        // Count trailing zeros and ones to determine encoding
        int tz = __builtin_ctzll(imm);
        int to = __builtin_ctzll(~(imm >> tz));
        
        if (tz + to == 64 || (imm >> (tz + to)) == 0) {
            // Simple run of 1s
            immr = (64 - tz) & 63;
            imms = to - 1;
            return true;
        }
        
        return false;
    }
    
public:
    // ============================================
    // Load/Store (additional for PowerPC)
    // ============================================
    
    // LDR Wt, [Xn, #offset] (32-bit)
    void LDR_W(GpReg rt, GpReg rn, int32_t offset = 0) {
        uint32_t imm12 = (offset >> 2) & 0xFFF;
        buf_.Emit(0xB9400000 | (imm12 << 10) | (static_cast<uint32_t>(rn) << 5) | 
                  static_cast<uint32_t>(rt));
    }
    
    // STR Wt, [Xn, #offset] (32-bit)
    void STR_W(GpReg rt, GpReg rn, int32_t offset = 0) {
        uint32_t imm12 = (offset >> 2) & 0xFFF;
        buf_.Emit(0xB9000000 | (imm12 << 10) | (static_cast<uint32_t>(rn) << 5) | 
                  static_cast<uint32_t>(rt));
    }
    
    // LDRH Wt, [Xn, #offset] (16-bit unsigned)
    void LDRH(GpReg rt, GpReg rn, int32_t offset = 0) {
        uint32_t imm12 = (offset >> 1) & 0xFFF;
        buf_.Emit(0x79400000 | (imm12 << 10) | (static_cast<uint32_t>(rn) << 5) | 
                  static_cast<uint32_t>(rt));
    }
    
    // STRH Wt, [Xn, #offset] (16-bit)
    void STRH(GpReg rt, GpReg rn, int32_t offset = 0) {
        uint32_t imm12 = (offset >> 1) & 0xFFF;
        buf_.Emit(0x79000000 | (imm12 << 10) | (static_cast<uint32_t>(rn) << 5) | 
                  static_cast<uint32_t>(rt));
    }
    
    // LDRB Wt, [Xn, #offset] (8-bit unsigned)
    void LDRB(GpReg rt, GpReg rn, int32_t offset = 0) {
        buf_.Emit(0x39400000 | ((offset & 0xFFF) << 10) | (static_cast<uint32_t>(rn) << 5) | 
                  static_cast<uint32_t>(rt));
    }
    
    // STRB Wt, [Xn, #offset] (8-bit)
    void STRB(GpReg rt, GpReg rn, int32_t offset = 0) {
        buf_.Emit(0x39000000 | ((offset & 0xFFF) << 10) | (static_cast<uint32_t>(rn) << 5) | 
                  static_cast<uint32_t>(rt));
    }
    
    // ============================================
    // Byte Reversal (for Big-Endian support)
    // ============================================
    
    // REV Xd, Xn (reverse bytes in 64-bit)
    void REV(GpReg rd, GpReg rn) {
        buf_.Emit(0xDAC00C00 | (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // REV Wd, Wn (reverse bytes in 32-bit)  
    void REV_W(GpReg rd, GpReg rn) {
        buf_.Emit(0x5AC00800 | (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // REV16 Xd, Xn (reverse bytes in each halfword)
    void REV16(GpReg rd, GpReg rn) {
        buf_.Emit(0xDAC00400 | (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ============================================
    // Sign Extension
    // ============================================
    
    // SXTB Xd, Wn (sign extend byte)
    void SXTB(GpReg rd, GpReg rn) {
        buf_.Emit(0x93401C00 | (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SXTH Xd, Wn (sign extend halfword)
    void SXTH(GpReg rd, GpReg rn) {
        buf_.Emit(0x93403C00 | (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // SXTW Xd, Wn (sign extend word)
    void SXTW(GpReg rd, GpReg rn) {
        buf_.Emit(0x93407C00 | (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ============================================
    // Rotate
    // ============================================
    
    // ROR Xd, Xn, Xm (rotate right variable)
    void ROR(GpReg rd, GpReg rn, GpReg rm) {
        buf_.Emit(0x9AC02C00 | (static_cast<uint32_t>(rm) << 16) | 
                  (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ROR Xd, Xn, #imm (rotate right immediate via EXTR)
    void ROR_IMM(GpReg rd, GpReg rn, uint8_t imm) {
        // EXTR Xd, Xn, Xn, #imm
        buf_.Emit(0x93C00000 | (static_cast<uint32_t>(rn) << 16) | 
                  (imm << 10) | (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd));
    }
    
    // ============================================
    // ASR immediate
    // ============================================
    
    // ASR Xd, Xn, #imm (arithmetic shift right immediate)
    void ASR_IMM(GpReg rd, GpReg rn, uint8_t imm) {
        // SBFM Xd, Xn, #imm, #63
        buf_.Emit(0x9340FC00 | (imm << 16) | (static_cast<uint32_t>(rn) << 5) | 
                  static_cast<uint32_t>(rd));
    }
    
    // ============================================
    // MOVN (move wide with NOT)
    // ============================================
    
    void MOVN(GpReg rd, uint16_t imm, uint8_t shift = 0) {
        uint8_t hw = shift / 16;
        buf_.Emit(0x92800000 | (hw << 21) | (imm << 5) | static_cast<uint32_t>(rd));
    }
    
    // ============================================
    // Branch
    // ============================================
    
    // B #offset (unconditional)
    void B(int32_t offset) {
        int32_t imm26 = (offset >> 2) & 0x3FFFFFF;
        buf_.Emit(0x14000000 | imm26);
    }
    
    // B.cond #offset (conditional)
    void B_COND(Cond cond, int32_t offset) {
        int32_t imm19 = (offset >> 2) & 0x7FFFF;
        buf_.Emit(0x54000000 | (imm19 << 5) | static_cast<uint32_t>(cond));
    }
    
    // BL #offset (call with link)
    void BL(int32_t offset) {
        int32_t imm26 = (offset >> 2) & 0x3FFFFFF;
        buf_.Emit(0x94000000 | imm26);
    }
    
    // BR Xn (indirect branch)
    void BR(GpReg rn) {
        buf_.Emit(0xD61F0000 | (static_cast<uint32_t>(rn) << 5));
    }
    
    // BLR Xn (indirect call with link)
    void BLR(GpReg rn) {
        buf_.Emit(0xD63F0000 | (static_cast<uint32_t>(rn) << 5));
    }
    
    // RET (RET X30)
    void RET() {
        buf_.Emit(0xD65F03C0);
    }
    
    // ============================================
    // Immediate variants for JIT compiler
    // ============================================
    
    // MOVi - move immediate (handles sign extension for negative)
    void MOVi(GpReg rd, int64_t imm) {
        if (imm >= 0 && imm <= 0xFFFF) {
            MOVZ(rd, static_cast<uint16_t>(imm), 0);
        } else if (imm < 0 && imm >= -0x10000) {
            // Use MOVN for negative
            buf_.Emit(0x92800000 | ((~imm & 0xFFFF) << 5) | static_cast<uint32_t>(rd));
        } else {
            MOV_IMM64(rd, static_cast<uint64_t>(imm));
        }
    }
    
    // ADDi - add immediate (signed)
    void ADDi(GpReg rd, GpReg rn, int64_t imm) {
        if (imm >= 0 && imm < 4096) {
            ADD_IMM(rd, rn, static_cast<uint16_t>(imm));
        } else if (imm < 0 && imm > -4096) {
            SUB_IMM(rd, rn, static_cast<uint16_t>(-imm));
        } else {
            // Large immediate - load into temp and add
            MOVi(REG_TMP1, imm);
            ADD(rd, rn, REG_TMP1);
        }
    }
    
    // ORRi - or immediate 
    void ORRi(GpReg rd, GpReg rn, uint64_t imm) {
        if (imm == 0) {
            if (rd != rn) MOV(rd, rn);
            return;
        }
        // Load imm to temp and ORR
        MOVi(REG_TMP1, static_cast<int64_t>(imm));
        ORR(rd, rn, REG_TMP1);
    }
    
    // LDRWi - load 32-bit word with signed offset
    void LDRWi(GpReg rt, GpReg rn, int32_t offset) {
        if (offset >= 0 && offset < 16380 && (offset & 3) == 0) {
            LDR_W(rt, rn, offset);
        } else if (offset >= -256 && offset < 256) {
            // LDUR for unscaled offset
            buf_.Emit(0xB8400000 | ((offset & 0x1FF) << 12) | 
                     (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rt));
        } else {
            ADDi(REG_TMP1, rn, offset);
            LDR_W(rt, REG_TMP1, 0);
        }
    }
    
    // STRWi - store 32-bit word with signed offset  
    void STRWi(GpReg rt, GpReg rn, int32_t offset) {
        if (offset >= 0 && offset < 16380 && (offset & 3) == 0) {
            STR_W(rt, rn, offset);
        } else if (offset >= -256 && offset < 256) {
            // STUR for unscaled offset
            buf_.Emit(0xB8000000 | ((offset & 0x1FF) << 12) | 
                     (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rt));
        } else {
            ADDi(REG_TMP1, rn, offset);
            STR_W(rt, REG_TMP1, 0);
        }
    }
    
    // LDRB register indexed
    void LDRB(GpReg rt, GpReg rn, GpReg rm) {
        buf_.Emit(0x38606800 | (static_cast<uint32_t>(rm) << 16) |
                 (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rt));
    }
    
    // STRW register indexed
    void STRW(GpReg rt, GpReg rn, GpReg rm) {
        buf_.Emit(0xB8206800 | (static_cast<uint32_t>(rm) << 16) |
                 (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rt));
    }
    
    // CMP Xn, Xm (SUBS XZR, Xn, Xm)
    void CMP(GpReg rn, GpReg rm) {
        SUBS(GpReg::ZR, rn, rm);
    }
    
    // ============================================
    // Branches for JIT
    // ============================================
    
    // B offset (relative branch)
    void B(int32_t offset) {
        int32_t imm26 = (offset >> 2) & 0x3FFFFFF;
        buf_.Emit(0x14000000 | imm26);
    }
    
    // Bcond - conditional branch
    void Bcond(Cond cond, int32_t offset) {
        int32_t imm19 = (offset >> 2) & 0x7FFFF;
        buf_.Emit(0x54000000 | (imm19 << 5) | static_cast<uint32_t>(cond));
    }
    
    // ============================================
    // System
    // ============================================
    
    // NOP
    void NOP() {
        buf_.Emit(0xD503201F);
    }
    
    // BRK #imm16 (software breakpoint)
    void BRK(uint16_t imm = 0) {
        buf_.Emit(0xD4200000 | (static_cast<uint32_t>(imm) << 5));
    }
    
    // SVC #imm16 (syscall)
    void SVC(uint16_t imm) {
        buf_.Emit(0xD4000001 | (static_cast<uint32_t>(imm) << 5));
    }
    
    // DMB (data memory barrier)
    void DMB() {
        buf_.Emit(0xD5033BBF);  // DMB ISH
    }
    
    // DSB (data synchronization barrier)
    void DSB() {
        buf_.Emit(0xD5033B9F);  // DSB ISH
    }
    
    // ISB (instruction synchronization barrier)
    void ISB() {
        buf_.Emit(0xD5033FDF);
    }
    
    // ============================================
    // SIMD/NEON (for SSE translation)
    // ============================================
    
    // MOV Vd.16B, Vn.16B
    void MOV_VEC(VecReg vd, VecReg vn) {
        // ORR Vd.16B, Vn.16B, Vn.16B
        buf_.Emit(0x4EA01C00 | (static_cast<uint32_t>(vn) << 16) | 
                  (static_cast<uint32_t>(vn) << 5) | static_cast<uint32_t>(vd));
    }
    
    // LDR Qd, [Xn, #offset]
    void LDR_VEC(VecReg vt, GpReg rn, int32_t offset = 0) {
        uint32_t imm12 = (offset >> 4) & 0xFFF;
        buf_.Emit(0x3DC00000 | (imm12 << 10) | (static_cast<uint32_t>(rn) << 5) | 
                  static_cast<uint32_t>(vt));
    }
    
    // STR Qt, [Xn, #offset]
    void STR_VEC(VecReg vt, GpReg rn, int32_t offset = 0) {
        uint32_t imm12 = (offset >> 4) & 0xFFF;
        buf_.Emit(0x3D800000 | (imm12 << 10) | (static_cast<uint32_t>(rn) << 5) | 
                  static_cast<uint32_t>(vt));
    }
    
    // FADD Vd.4S, Vn.4S, Vm.4S
    void FADD_4S(VecReg vd, VecReg vn, VecReg vm) {
        buf_.Emit(0x4E20D400 | (static_cast<uint32_t>(vm) << 16) | 
                  (static_cast<uint32_t>(vn) << 5) | static_cast<uint32_t>(vd));
    }
    
    // FSUB Vd.4S, Vn.4S, Vm.4S
    void FSUB_4S(VecReg vd, VecReg vn, VecReg vm) {
        buf_.Emit(0x4EA0D400 | (static_cast<uint32_t>(vm) << 16) | 
                  (static_cast<uint32_t>(vn) << 5) | static_cast<uint32_t>(vd));
    }
    
    // FMUL Vd.4S, Vn.4S, Vm.4S
    void FMUL_4S(VecReg vd, VecReg vn, VecReg vm) {
        buf_.Emit(0x6E20DC00 | (static_cast<uint32_t>(vm) << 16) | 
                  (static_cast<uint32_t>(vn) << 5) | static_cast<uint32_t>(vd));
    }
    
    // FDIV Vd.4S, Vn.4S, Vm.4S
    void FDIV_4S(VecReg vd, VecReg vn, VecReg vm) {
        buf_.Emit(0x6E20FC00 | (static_cast<uint32_t>(vm) << 16) | 
                  (static_cast<uint32_t>(vn) << 5) | static_cast<uint32_t>(vd));
    }
    
private:
    CodeBuffer& buf_;
};

/**
 * PowerPC → ARM64 Register Mapping
 * 
 * PS3 PPU має 32 GPR (r0-r31), 32 FPR (f0-f31), 32 VR (v0-v31)
 * ARM64 має 31 GPR (x0-x30), 32 SIMD (v0-v31)
 * 
 * Mapping Strategy:
 * - PPC r0-r27 → ARM64 x0-x27 (direct mapping)
 * - PPC r28 → x28 (frame pointer reservation on ARM64)
 * - PPC r29 → x19 (callee-saved)
 * - PPC r30 → x20 (callee-saved)  
 * - PPC r31 → x21 (callee-saved, stack pointer on PPC)
 * - PPC LR → x30 (LR on ARM64)
 * - PPC CTR → x22 (callee-saved)
 * - PPC XER → x23 (callee-saved)
 * - PPC CR → x24 (callee-saved)
 * - State pointer → x29 (frame pointer)
 */

// Map PowerPC GPR to ARM64 register
inline GpReg MapPPCGPR(ppc::GPR reg) {
    uint8_t idx = static_cast<uint8_t>(reg);
    
    // Direct mapping for r0-r27
    if (idx <= 27) {
        return static_cast<GpReg>(idx);
    }
    
    // Special mappings for r28-r31
    switch (idx) {
        case 28: return GpReg::X28;
        case 29: return GpReg::X19;
        case 30: return GpReg::X20;
        case 31: return GpReg::X21;
        default: return GpReg::X0;
    }
}

// Map PowerPC FPR to ARM64 SIMD register
inline VecReg MapPPCFPR(ppc::FPR reg) {
    return static_cast<VecReg>(static_cast<uint8_t>(reg));
}

// Map PowerPC VR (AltiVec/VMX) to ARM64 SIMD register
inline VecReg MapPPCVR(ppc::VR reg) {
    return static_cast<VecReg>(static_cast<uint8_t>(reg));
}

/**
 * PPU State structure layout (offsets)
 */
struct PPUStateOffsets {
    static constexpr int GPR = 0;           // 32 * 8 = 256 bytes
    static constexpr int FPR = 256;         // 32 * 8 = 256 bytes  
    static constexpr int VR = 512;          // 32 * 16 = 512 bytes
    static constexpr int CR = 1024;         // 8 bytes
    static constexpr int LR = 1032;         // 8 bytes
    static constexpr int CTR = 1040;        // 8 bytes
    static constexpr int XER = 1048;        // 8 bytes
    static constexpr int FPSCR = 1056;      // 8 bytes
    static constexpr int PC = 1064;         // 8 bytes
    static constexpr int VRSAVE = 1072;     // 4 bytes
    static constexpr int SIZE = 1088;       // Total size
};

/**
 * PowerPC → ARM64 Code Generator
 */
class PPCTranslator {
public:
    explicit PPCTranslator(Emitter& emit) : emit_(emit) {}
    
    // Translate single PowerPC instruction
    bool Translate(const ppc::DecodedInstr& instr);
    
    // Translate OP31 (extended ALU) instructions
    void TranslateOp31(const ppc::DecodedInstr& instr) {
        // OP31 extended opcode is in bits 21-30
        uint16_t xo = instr.xo;
        
        switch (xo) {
            case 266:  // ADD
            case 10:   // ADDC
            case 138:  // ADDE
                EmitAdd(instr);
                break;
            case 40:   // SUBF
            case 8:    // SUBFC
            case 136:  // SUBFE
                EmitSub(instr);
                break;
            case 235:  // MULLW
            case 233:  // MULLD
                EmitMul(instr);
                break;
            case 491:  // DIVW
            case 489:  // DIVD
                EmitDiv(instr);
                break;
            case 28:   // AND
            case 60:   // ANDC
            case 444:  // OR
            case 124:  // NOR
            case 316:  // XOR
            case 284:  // EQV
                EmitLogic(instr);
                break;
            case 24:   // SLW
            case 536:  // SRW
            case 792:  // SRAW
                EmitShift(instr);
                break;
            case 0:    // CMP
            case 32:   // CMPL
                EmitCompare(instr);
                break;
            case 339:  // MFSPR
            case 467:  // MTSPR
                EmitSPR(instr);
                break;
            case 19:   // MFCR
            case 144:  // MTCRF
                EmitCRLogic(instr);
                break;
            case 23:   // LWZX
            case 21:   // LDUX
            case 87:   // LBZX
            case 279:  // LHZX
                EmitLoad(instr);
                break;
            case 151:  // STWX
            case 149:  // STDX
            case 215:  // STBX
            case 407:  // STHX
                EmitStore(instr);
                break;
            default:
                // Unsupported extended opcode - emit breakpoint
                emit_.BRK(static_cast<uint16_t>(xo & 0x3FF));
                break;
        }
    }
    
private:
    Emitter& emit_;
    
    // Load/Store
    void EmitLoad(const ppc::DecodedInstr& instr);
    void EmitStore(const ppc::DecodedInstr& instr);
    void EmitLoadMultiple(const ppc::DecodedInstr& instr);
    void EmitStoreMultiple(const ppc::DecodedInstr& instr);
    
    // Arithmetic
    void EmitAdd(const ppc::DecodedInstr& instr);
    void EmitSub(const ppc::DecodedInstr& instr);
    void EmitMul(const ppc::DecodedInstr& instr);
    void EmitDiv(const ppc::DecodedInstr& instr);
    
    // Logic
    void EmitLogic(const ppc::DecodedInstr& instr);
    void EmitShift(const ppc::DecodedInstr& instr);
    void EmitRotate(const ppc::DecodedInstr& instr);
    
    // Compare
    void EmitCompare(const ppc::DecodedInstr& instr);
    
    // Branch  
    void EmitBranch(const ppc::DecodedInstr& instr);
    
    // CR operations
    void EmitCRLogic(const ppc::DecodedInstr& instr);
    void EmitCR0Update(GpReg result);
    
    // System
    void EmitSystem(const ppc::DecodedInstr& instr);
    void EmitSPR(const ppc::DecodedInstr& instr);
    void EmitTrap(const ppc::DecodedInstr& instr);
    
    // Floating Point
    void EmitFPLoad(const ppc::DecodedInstr& instr);
    void EmitFPStore(const ppc::DecodedInstr& instr);
    void EmitFPArith(const ppc::DecodedInstr& instr);
};

} // namespace rpcsx::nce::arm64

#endif // RPCSX_ARM64_EMITTER_H
