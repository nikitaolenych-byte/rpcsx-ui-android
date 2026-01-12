/**
 * ARM64 Code Emitter для NCE JIT
 * Генерує нативний ARM64 код з x86-64 інструкцій
 */

#ifndef RPCSX_ARM64_EMITTER_H
#define RPCSX_ARM64_EMITTER_H

#include <cstdint>
#include <cstddef>
#include <cstring>

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
    
    // CMP Xn, #imm12
    void CMP_IMM(GpReg rn, uint16_t imm12) {
        // SUBS XZR, Xn, #imm12
        buf_.Emit(0xF1000000 | ((imm12 & 0xFFF) << 10) | 
                  (static_cast<uint32_t>(rn) << 5) | 31);
    }
    
    // TST Xn, Xm (ANDS XZR, Xn, Xm)
    void TST(GpReg rn, GpReg rm) {
        ANDS(GpReg::ZR, rn, rm);
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

} // namespace rpcsx::nce::arm64

#endif // RPCSX_ARM64_EMITTER_H
