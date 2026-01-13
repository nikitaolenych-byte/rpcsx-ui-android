// ============================================================================
// PPU Interpreter - Full PowerPC Cell Implementation
// ============================================================================
// Повна реалізація всіх PPU інструкцій PS3 Cell Broadband Engine
// ~200 інструкцій PowerPC 64-bit
// ============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <unordered_map>

namespace rpcsx {
namespace ppu {

// ============================================================================
// PPU Register State
// ============================================================================
struct PPUState {
    // General Purpose Registers (64-bit)
    uint64_t gpr[32];
    
    // Floating Point Registers (64-bit double)
    double fpr[32];
    
    // Vector Registers (128-bit for VMX/AltiVec)
    union {
        uint8_t  u8[16];
        uint16_t u16[8];
        uint32_t u32[4];
        uint64_t u64[2];
        float    f32[4];
        double   f64[2];
    } vr[32];
    
    // Special Purpose Registers
    uint64_t lr;      // Link Register
    uint64_t ctr;     // Count Register
    uint32_t cr;      // Condition Register
    uint64_t xer;     // Fixed-point Exception Register
    uint32_t fpscr;   // FP Status and Control Register
    uint32_t vscr;    // Vector Status and Control Register
    
    // Program Counter
    uint64_t pc;
    uint64_t npc;     // Next PC (for branches)
    
    // MSR - Machine State Register
    uint64_t msr;
    
    // Segment Registers (for address translation)
    uint64_t sr[16];
    
    // Time Base
    uint64_t tb;
    
    // Reservation (for lwarx/stwcx)
    uint64_t reserve_addr;
    bool reserve_valid;
    
    // State flags
    bool running;
    bool halted;
    uint32_t interrupt_pending;
};

// ============================================================================
// Condition Register Bits
// ============================================================================
enum CR_BITS {
    CR_LT = 0x8,  // Less Than
    CR_GT = 0x4,  // Greater Than
    CR_EQ = 0x2,  // Equal
    CR_SO = 0x1   // Summary Overflow
};

// ============================================================================
// XER Bits
// ============================================================================
enum XER_BITS {
    XER_SO = 0x80000000,  // Summary Overflow
    XER_OV = 0x40000000,  // Overflow
    XER_CA = 0x20000000   // Carry
};

// ============================================================================
// PPU Instruction Decoder
// ============================================================================
struct PPUInstruction {
    uint32_t raw;
    
    // Primary opcode (bits 0-5)
    uint32_t opcode() const { return (raw >> 26) & 0x3F; }
    
    // Extended opcode for X-form (bits 21-30)
    uint32_t xo_x() const { return (raw >> 1) & 0x3FF; }
    
    // Extended opcode for XO-form (bits 22-30)
    uint32_t xo_xo() const { return (raw >> 1) & 0x1FF; }
    
    // Register fields
    uint32_t rd() const { return (raw >> 21) & 0x1F; }  // RT/RS/FRT/FRS
    uint32_t ra() const { return (raw >> 16) & 0x1F; }
    uint32_t rb() const { return (raw >> 11) & 0x1F; }
    uint32_t rc() const { return (raw >> 6) & 0x1F; }
    
    // Immediate fields
    int16_t simm16() const { return static_cast<int16_t>(raw & 0xFFFF); }
    uint16_t uimm16() const { return raw & 0xFFFF; }
    int32_t li() const { 
        int32_t v = (raw & 0x03FFFFFC);
        if (v & 0x02000000) v |= 0xFC000000;  // Sign extend
        return v;
    }
    int16_t bd() const {
        int16_t v = (raw & 0xFFFC);
        if (v & 0x8000) v |= 0xFFFF0000;  // Sign extend
        return v;
    }
    
    // Branch fields
    uint32_t bo() const { return (raw >> 21) & 0x1F; }
    uint32_t bi() const { return (raw >> 16) & 0x1F; }
    bool aa() const { return (raw >> 1) & 1; }  // Absolute Address
    bool lk() const { return raw & 1; }         // Link
    bool rc_bit() const { return raw & 1; }     // Record bit (Rc)
    bool oe() const { return (raw >> 10) & 1; } // OE bit for overflow
    
    // Shift/Rotate fields
    uint32_t sh() const { return (raw >> 11) & 0x1F; }
    uint32_t mb() const { return (raw >> 6) & 0x1F; }
    uint32_t me() const { return (raw >> 1) & 0x1F; }
    
    // 64-bit shift amount (sh + sh[5])
    uint32_t sh64() const { return ((raw >> 11) & 0x1F) | ((raw & 2) << 4); }
    uint32_t mb64() const { return ((raw >> 6) & 0x1F) | ((raw & 0x20)); }
    uint32_t me64() const { return ((raw >> 6) & 0x1F) | ((raw >> 5) & 0x20); }
    
    // SPR field
    uint32_t spr() const { return ((raw >> 16) & 0x1F) | ((raw >> 6) & 0x3E0); }
    
    // CRM field for mtcrf
    uint32_t crm() const { return (raw >> 12) & 0xFF; }
    
    // CR field
    uint32_t crfd() const { return (raw >> 23) & 0x7; }
    uint32_t crfs() const { return (raw >> 18) & 0x7; }
    uint32_t crba() const { return (raw >> 16) & 0x1F; }
    uint32_t crbb() const { return (raw >> 11) & 0x1F; }
    uint32_t crbd() const { return (raw >> 21) & 0x1F; }
};

// ============================================================================
// PPU Interpreter Class
// ============================================================================
class PPUInterpreter {
public:
    PPUInterpreter();
    ~PPUInterpreter() = default;
    
    // Initialize with memory base
    void Initialize(void* memory_base, size_t memory_size);
    
    // Execute single instruction
    void Step(PPUState& state);
    
    // Execute N instructions
    void Execute(PPUState& state, uint64_t count);
    
    // Run until halt
    void Run(PPUState& state);
    
    // Memory access
    uint8_t  ReadMemory8(PPUState& state, uint64_t addr);
    uint16_t ReadMemory16(PPUState& state, uint64_t addr);
    uint32_t ReadMemory32(PPUState& state, uint64_t addr);
    uint64_t ReadMemory64(PPUState& state, uint64_t addr);
    
    void WriteMemory8(PPUState& state, uint64_t addr, uint8_t value);
    void WriteMemory16(PPUState& state, uint64_t addr, uint16_t value);
    void WriteMemory32(PPUState& state, uint64_t addr, uint32_t value);
    void WriteMemory64(PPUState& state, uint64_t addr, uint64_t value);
    
private:
    void* memory_base_;
    size_t memory_size_;
    
    // Instruction handlers
    void ExecuteInstruction(PPUState& state, PPUInstruction inst);
    
    // ========================================================================
    // Branch Instructions
    // ========================================================================
    void B(PPUState& state, PPUInstruction inst);      // Branch
    void BC(PPUState& state, PPUInstruction inst);     // Branch Conditional
    void BCLR(PPUState& state, PPUInstruction inst);   // Branch Conditional to LR
    void BCCTR(PPUState& state, PPUInstruction inst);  // Branch Conditional to CTR
    
    // ========================================================================
    // Load/Store Instructions
    // ========================================================================
    void LBZ(PPUState& state, PPUInstruction inst);    // Load Byte and Zero
    void LBZU(PPUState& state, PPUInstruction inst);   // Load Byte and Zero with Update
    void LBZX(PPUState& state, PPUInstruction inst);   // Load Byte and Zero Indexed
    void LHZ(PPUState& state, PPUInstruction inst);    // Load Halfword and Zero
    void LHZU(PPUState& state, PPUInstruction inst);
    void LHZX(PPUState& state, PPUInstruction inst);
    void LHA(PPUState& state, PPUInstruction inst);    // Load Halfword Algebraic
    void LHAU(PPUState& state, PPUInstruction inst);
    void LHAX(PPUState& state, PPUInstruction inst);
    void LWZ(PPUState& state, PPUInstruction inst);    // Load Word and Zero
    void LWZU(PPUState& state, PPUInstruction inst);
    void LWZX(PPUState& state, PPUInstruction inst);
    void LWA(PPUState& state, PPUInstruction inst);    // Load Word Algebraic
    void LWAX(PPUState& state, PPUInstruction inst);
    void LD(PPUState& state, PPUInstruction inst);     // Load Doubleword
    void LDU(PPUState& state, PPUInstruction inst);
    void LDX(PPUState& state, PPUInstruction inst);
    
    void STB(PPUState& state, PPUInstruction inst);    // Store Byte
    void STBU(PPUState& state, PPUInstruction inst);
    void STBX(PPUState& state, PPUInstruction inst);
    void STH(PPUState& state, PPUInstruction inst);    // Store Halfword
    void STHU(PPUState& state, PPUInstruction inst);
    void STHX(PPUState& state, PPUInstruction inst);
    void STW(PPUState& state, PPUInstruction inst);    // Store Word
    void STWU(PPUState& state, PPUInstruction inst);
    void STWX(PPUState& state, PPUInstruction inst);
    void STD(PPUState& state, PPUInstruction inst);    // Store Doubleword
    void STDU(PPUState& state, PPUInstruction inst);
    void STDX(PPUState& state, PPUInstruction inst);
    
    // ========================================================================
    // Integer Arithmetic Instructions
    // ========================================================================
    void ADDI(PPUState& state, PPUInstruction inst);   // Add Immediate
    void ADDIS(PPUState& state, PPUInstruction inst);  // Add Immediate Shifted
    void ADD(PPUState& state, PPUInstruction inst);    // Add
    void ADDC(PPUState& state, PPUInstruction inst);   // Add Carrying
    void ADDE(PPUState& state, PPUInstruction inst);   // Add Extended
    void ADDME(PPUState& state, PPUInstruction inst);  // Add to Minus One Extended
    void ADDZE(PPUState& state, PPUInstruction inst);  // Add to Zero Extended
    void SUBF(PPUState& state, PPUInstruction inst);   // Subtract From
    void SUBFC(PPUState& state, PPUInstruction inst);  // Subtract From Carrying
    void SUBFE(PPUState& state, PPUInstruction inst);  // Subtract From Extended
    void SUBFME(PPUState& state, PPUInstruction inst);
    void SUBFZE(PPUState& state, PPUInstruction inst);
    void NEG(PPUState& state, PPUInstruction inst);    // Negate
    void MULLI(PPUState& state, PPUInstruction inst);  // Multiply Low Immediate
    void MULLW(PPUState& state, PPUInstruction inst);  // Multiply Low Word
    void MULHW(PPUState& state, PPUInstruction inst);  // Multiply High Word
    void MULHWU(PPUState& state, PPUInstruction inst); // Multiply High Word Unsigned
    void MULLD(PPUState& state, PPUInstruction inst);  // Multiply Low Doubleword
    void MULHD(PPUState& state, PPUInstruction inst);  // Multiply High Doubleword
    void MULHDU(PPUState& state, PPUInstruction inst);
    void DIVW(PPUState& state, PPUInstruction inst);   // Divide Word
    void DIVWU(PPUState& state, PPUInstruction inst);  // Divide Word Unsigned
    void DIVD(PPUState& state, PPUInstruction inst);   // Divide Doubleword
    void DIVDU(PPUState& state, PPUInstruction inst);
    
    // ========================================================================
    // Integer Compare Instructions
    // ========================================================================
    void CMPI(PPUState& state, PPUInstruction inst);   // Compare Immediate
    void CMP(PPUState& state, PPUInstruction inst);    // Compare
    void CMPLI(PPUState& state, PPUInstruction inst);  // Compare Logical Immediate
    void CMPL(PPUState& state, PPUInstruction inst);   // Compare Logical
    
    // ========================================================================
    // Integer Logical Instructions
    // ========================================================================
    void ANDI(PPUState& state, PPUInstruction inst);   // AND Immediate
    void ANDIS(PPUState& state, PPUInstruction inst);  // AND Immediate Shifted
    void ORI(PPUState& state, PPUInstruction inst);    // OR Immediate
    void ORIS(PPUState& state, PPUInstruction inst);   // OR Immediate Shifted
    void XORI(PPUState& state, PPUInstruction inst);   // XOR Immediate
    void XORIS(PPUState& state, PPUInstruction inst);  // XOR Immediate Shifted
    void AND(PPUState& state, PPUInstruction inst);    // AND
    void ANDC(PPUState& state, PPUInstruction inst);   // AND with Complement
    void OR(PPUState& state, PPUInstruction inst);     // OR
    void ORC(PPUState& state, PPUInstruction inst);    // OR with Complement
    void XOR(PPUState& state, PPUInstruction inst);    // XOR
    void NAND(PPUState& state, PPUInstruction inst);   // NAND
    void NOR(PPUState& state, PPUInstruction inst);    // NOR
    void EQV(PPUState& state, PPUInstruction inst);    // Equivalent
    void EXTSB(PPUState& state, PPUInstruction inst);  // Extend Sign Byte
    void EXTSH(PPUState& state, PPUInstruction inst);  // Extend Sign Halfword
    void EXTSW(PPUState& state, PPUInstruction inst);  // Extend Sign Word
    void CNTLZW(PPUState& state, PPUInstruction inst); // Count Leading Zeros Word
    void CNTLZD(PPUState& state, PPUInstruction inst); // Count Leading Zeros Doubleword
    void POPCNTB(PPUState& state, PPUInstruction inst);// Population Count Bytes
    
    // ========================================================================
    // Integer Rotate/Shift Instructions
    // ========================================================================
    void RLWINM(PPUState& state, PPUInstruction inst); // Rotate Left Word Immediate then AND with Mask
    void RLWNM(PPUState& state, PPUInstruction inst);  // Rotate Left Word then AND with Mask
    void RLWIMI(PPUState& state, PPUInstruction inst); // Rotate Left Word Immediate then Mask Insert
    void RLDICL(PPUState& state, PPUInstruction inst); // Rotate Left Doubleword Immediate then Clear Left
    void RLDICR(PPUState& state, PPUInstruction inst); // Rotate Left Doubleword Immediate then Clear Right
    void RLDIC(PPUState& state, PPUInstruction inst);  // Rotate Left Doubleword Immediate then Clear
    void RLDIMI(PPUState& state, PPUInstruction inst); // Rotate Left Doubleword Immediate then Mask Insert
    void RLDCL(PPUState& state, PPUInstruction inst);  // Rotate Left Doubleword then Clear Left
    void RLDCR(PPUState& state, PPUInstruction inst);  // Rotate Left Doubleword then Clear Right
    void SLW(PPUState& state, PPUInstruction inst);    // Shift Left Word
    void SRW(PPUState& state, PPUInstruction inst);    // Shift Right Word
    void SRAW(PPUState& state, PPUInstruction inst);   // Shift Right Algebraic Word
    void SRAWI(PPUState& state, PPUInstruction inst);  // Shift Right Algebraic Word Immediate
    void SLD(PPUState& state, PPUInstruction inst);    // Shift Left Doubleword
    void SRD(PPUState& state, PPUInstruction inst);    // Shift Right Doubleword
    void SRAD(PPUState& state, PPUInstruction inst);   // Shift Right Algebraic Doubleword
    void SRADI(PPUState& state, PPUInstruction inst);  // Shift Right Algebraic Doubleword Immediate
    
    // ========================================================================
    // Move To/From Special Purpose Registers
    // ========================================================================
    void MTSPR(PPUState& state, PPUInstruction inst);  // Move To SPR
    void MFSPR(PPUState& state, PPUInstruction inst);  // Move From SPR
    void MTCRF(PPUState& state, PPUInstruction inst);  // Move To CR Fields
    void MFCR(PPUState& state, PPUInstruction inst);   // Move From CR
    void MFOCRF(PPUState& state, PPUInstruction inst); // Move From One CR Field
    void MTOCRF(PPUState& state, PPUInstruction inst); // Move To One CR Field
    void MCRXR(PPUState& state, PPUInstruction inst);  // Move to CR from XER
    
    // ========================================================================
    // Floating Point Load/Store
    // ========================================================================
    void LFS(PPUState& state, PPUInstruction inst);    // Load Floating-Point Single
    void LFSU(PPUState& state, PPUInstruction inst);
    void LFSX(PPUState& state, PPUInstruction inst);
    void LFD(PPUState& state, PPUInstruction inst);    // Load Floating-Point Double
    void LFDU(PPUState& state, PPUInstruction inst);
    void LFDX(PPUState& state, PPUInstruction inst);
    void STFS(PPUState& state, PPUInstruction inst);   // Store Floating-Point Single
    void STFSU(PPUState& state, PPUInstruction inst);
    void STFSX(PPUState& state, PPUInstruction inst);
    void STFD(PPUState& state, PPUInstruction inst);   // Store Floating-Point Double
    void STFDU(PPUState& state, PPUInstruction inst);
    void STFDX(PPUState& state, PPUInstruction inst);
    
    // ========================================================================
    // Floating Point Arithmetic
    // ========================================================================
    void FADD(PPUState& state, PPUInstruction inst);   // FP Add
    void FADDS(PPUState& state, PPUInstruction inst);  // FP Add Single
    void FSUB(PPUState& state, PPUInstruction inst);   // FP Subtract
    void FSUBS(PPUState& state, PPUInstruction inst);
    void FMUL(PPUState& state, PPUInstruction inst);   // FP Multiply
    void FMULS(PPUState& state, PPUInstruction inst);
    void FDIV(PPUState& state, PPUInstruction inst);   // FP Divide
    void FDIVS(PPUState& state, PPUInstruction inst);
    void FSQRT(PPUState& state, PPUInstruction inst);  // FP Square Root
    void FSQRTS(PPUState& state, PPUInstruction inst);
    void FRES(PPUState& state, PPUInstruction inst);   // FP Reciprocal Estimate
    void FRSQRTE(PPUState& state, PPUInstruction inst);// FP Reciprocal Square Root Estimate
    void FMADD(PPUState& state, PPUInstruction inst);  // FP Multiply-Add
    void FMADDS(PPUState& state, PPUInstruction inst);
    void FMSUB(PPUState& state, PPUInstruction inst);  // FP Multiply-Subtract
    void FMSUBS(PPUState& state, PPUInstruction inst);
    void FNMADD(PPUState& state, PPUInstruction inst); // FP Negative Multiply-Add
    void FNMADDS(PPUState& state, PPUInstruction inst);
    void FNMSUB(PPUState& state, PPUInstruction inst); // FP Negative Multiply-Subtract
    void FNMSUBS(PPUState& state, PPUInstruction inst);
    void FSEL(PPUState& state, PPUInstruction inst);   // FP Select
    
    // ========================================================================
    // Floating Point Compare/Convert/Move
    // ========================================================================
    void FCMPU(PPUState& state, PPUInstruction inst);  // FP Compare Unordered
    void FCMPO(PPUState& state, PPUInstruction inst);  // FP Compare Ordered
    void FCTID(PPUState& state, PPUInstruction inst);  // FP Convert to Integer Doubleword
    void FCTIDZ(PPUState& state, PPUInstruction inst); // FP Convert to Integer Doubleword with Round toward Zero
    void FCTIW(PPUState& state, PPUInstruction inst);  // FP Convert to Integer Word
    void FCTIWZ(PPUState& state, PPUInstruction inst);
    void FCFID(PPUState& state, PPUInstruction inst);  // FP Convert from Integer Doubleword
    void FMR(PPUState& state, PPUInstruction inst);    // FP Move Register
    void FNEG(PPUState& state, PPUInstruction inst);   // FP Negate
    void FABS(PPUState& state, PPUInstruction inst);   // FP Absolute Value
    void FNABS(PPUState& state, PPUInstruction inst);  // FP Negative Absolute Value
    void FRSP(PPUState& state, PPUInstruction inst);   // FP Round to Single Precision
    void MFFS(PPUState& state, PPUInstruction inst);   // Move From FPSCR
    void MTFSF(PPUState& state, PPUInstruction inst);  // Move To FPSCR Fields
    void MTFSFI(PPUState& state, PPUInstruction inst); // Move To FPSCR Field Immediate
    void MTFSB0(PPUState& state, PPUInstruction inst); // Move To FPSCR Bit 0
    void MTFSB1(PPUState& state, PPUInstruction inst); // Move To FPSCR Bit 1
    
    // ========================================================================
    // VMX (AltiVec/Vector) Instructions
    // ========================================================================
    void LVX(PPUState& state, PPUInstruction inst);    // Load Vector Indexed
    void LVXL(PPUState& state, PPUInstruction inst);   // Load Vector Indexed LRU
    void LVEBX(PPUState& state, PPUInstruction inst);  // Load Vector Element Byte Indexed
    void LVEHX(PPUState& state, PPUInstruction inst);  // Load Vector Element Halfword Indexed
    void LVEWX(PPUState& state, PPUInstruction inst);  // Load Vector Element Word Indexed
    void LVSL(PPUState& state, PPUInstruction inst);   // Load Vector for Shift Left
    void LVSR(PPUState& state, PPUInstruction inst);   // Load Vector for Shift Right
    void STVX(PPUState& state, PPUInstruction inst);   // Store Vector Indexed
    void STVXL(PPUState& state, PPUInstruction inst);
    void STVEBX(PPUState& state, PPUInstruction inst);
    void STVEHX(PPUState& state, PPUInstruction inst);
    void STVEWX(PPUState& state, PPUInstruction inst);
    
    void VADDFP(PPUState& state, PPUInstruction inst);  // Vector Add Floating Point
    void VSUBFP(PPUState& state, PPUInstruction inst);  // Vector Subtract FP
    void VMADDFP(PPUState& state, PPUInstruction inst); // Vector Multiply-Add FP
    void VNMSUBFP(PPUState& state, PPUInstruction inst);// Vector Negative Multiply-Subtract FP
    void VMAXFP(PPUState& state, PPUInstruction inst);  // Vector Maximum FP
    void VMINFP(PPUState& state, PPUInstruction inst);  // Vector Minimum FP
    void VREFP(PPUState& state, PPUInstruction inst);   // Vector Reciprocal Estimate FP
    void VRSQRTEFP(PPUState& state, PPUInstruction inst);// Vector Reciprocal Square Root Estimate FP
    void VLOGEFP(PPUState& state, PPUInstruction inst); // Vector Log2 Estimate FP
    void VEXPTEFP(PPUState& state, PPUInstruction inst);// Vector 2^x Estimate FP
    
    void VADDUBM(PPUState& state, PPUInstruction inst); // Vector Add Unsigned Byte Modulo
    void VADDUHM(PPUState& state, PPUInstruction inst); // Vector Add Unsigned Halfword Modulo
    void VADDUWM(PPUState& state, PPUInstruction inst); // Vector Add Unsigned Word Modulo
    void VSUBUBM(PPUState& state, PPUInstruction inst);
    void VSUBUHM(PPUState& state, PPUInstruction inst);
    void VSUBUWM(PPUState& state, PPUInstruction inst);
    void VMULESH(PPUState& state, PPUInstruction inst); // Vector Multiply Even Signed Halfword
    void VMULEUH(PPUState& state, PPUInstruction inst); // Vector Multiply Even Unsigned Halfword
    void VMULOSH(PPUState& state, PPUInstruction inst); // Vector Multiply Odd Signed Halfword
    void VMULOUH(PPUState& state, PPUInstruction inst);
    
    void VAND(PPUState& state, PPUInstruction inst);    // Vector AND
    void VANDC(PPUState& state, PPUInstruction inst);   // Vector AND with Complement
    void VOR(PPUState& state, PPUInstruction inst);     // Vector OR
    void VORC(PPUState& state, PPUInstruction inst);    // Vector OR with Complement
    void VXOR(PPUState& state, PPUInstruction inst);    // Vector XOR
    void VNOR(PPUState& state, PPUInstruction inst);    // Vector NOR
    
    void VSLB(PPUState& state, PPUInstruction inst);    // Vector Shift Left Byte
    void VSLH(PPUState& state, PPUInstruction inst);    // Vector Shift Left Halfword
    void VSLW(PPUState& state, PPUInstruction inst);    // Vector Shift Left Word
    void VSRB(PPUState& state, PPUInstruction inst);    // Vector Shift Right Byte
    void VSRH(PPUState& state, PPUInstruction inst);
    void VSRW(PPUState& state, PPUInstruction inst);
    void VSRAB(PPUState& state, PPUInstruction inst);   // Vector Shift Right Algebraic Byte
    void VSRAH(PPUState& state, PPUInstruction inst);
    void VSRAW(PPUState& state, PPUInstruction inst);
    void VSL(PPUState& state, PPUInstruction inst);     // Vector Shift Left
    void VSR(PPUState& state, PPUInstruction inst);     // Vector Shift Right
    void VSLDOI(PPUState& state, PPUInstruction inst);  // Vector Shift Left Double by Octet Immediate
    
    void VPERM(PPUState& state, PPUInstruction inst);   // Vector Permute
    void VSEL(PPUState& state, PPUInstruction inst);    // Vector Select
    void VSPLTB(PPUState& state, PPUInstruction inst);  // Vector Splat Byte
    void VSPLTH(PPUState& state, PPUInstruction inst);  // Vector Splat Halfword
    void VSPLTW(PPUState& state, PPUInstruction inst);  // Vector Splat Word
    void VSPLTISB(PPUState& state, PPUInstruction inst);// Vector Splat Immediate Signed Byte
    void VSPLTISH(PPUState& state, PPUInstruction inst);// Vector Splat Immediate Signed Halfword
    void VSPLTISW(PPUState& state, PPUInstruction inst);// Vector Splat Immediate Signed Word
    void VMRGHB(PPUState& state, PPUInstruction inst);  // Vector Merge High Byte
    void VMRGHH(PPUState& state, PPUInstruction inst);  // Vector Merge High Halfword
    void VMRGHW(PPUState& state, PPUInstruction inst);  // Vector Merge High Word
    void VMRGLB(PPUState& state, PPUInstruction inst);  // Vector Merge Low Byte
    void VMRGLH(PPUState& state, PPUInstruction inst);
    void VMRGLW(PPUState& state, PPUInstruction inst);
    void VPKUHUM(PPUState& state, PPUInstruction inst); // Vector Pack Unsigned Halfword Unsigned Modulo
    void VPKUWUM(PPUState& state, PPUInstruction inst);
    void VUPKHSB(PPUState& state, PPUInstruction inst); // Vector Unpack High Signed Byte
    void VUPKHSH(PPUState& state, PPUInstruction inst);
    void VUPKLSB(PPUState& state, PPUInstruction inst); // Vector Unpack Low Signed Byte
    void VUPKLSH(PPUState& state, PPUInstruction inst);
    
    void VCMPEQUB(PPUState& state, PPUInstruction inst);// Vector Compare Equal Unsigned Byte
    void VCMPEQUH(PPUState& state, PPUInstruction inst);
    void VCMPEQUW(PPUState& state, PPUInstruction inst);
    void VCMPGTSB(PPUState& state, PPUInstruction inst);// Vector Compare Greater Than Signed Byte
    void VCMPGTSH(PPUState& state, PPUInstruction inst);
    void VCMPGTSW(PPUState& state, PPUInstruction inst);
    void VCMPGTUB(PPUState& state, PPUInstruction inst);// Vector Compare Greater Than Unsigned Byte
    void VCMPGTUH(PPUState& state, PPUInstruction inst);
    void VCMPGTUW(PPUState& state, PPUInstruction inst);
    void VCMPEQFP(PPUState& state, PPUInstruction inst);// Vector Compare Equal FP
    void VCMPGEFP(PPUState& state, PPUInstruction inst);// Vector Compare Greater Than or Equal FP
    void VCMPGTFP(PPUState& state, PPUInstruction inst);// Vector Compare Greater Than FP
    void VCMPBFP(PPUState& state, PPUInstruction inst); // Vector Compare Bounds FP
    
    void VCTSXS(PPUState& state, PPUInstruction inst);  // Vector Convert to Signed Fixed-Point Word Saturate
    void VCTUXS(PPUState& state, PPUInstruction inst);  // Vector Convert to Unsigned Fixed-Point Word Saturate
    void VCFSX(PPUState& state, PPUInstruction inst);   // Vector Convert from Signed Fixed-Point Word
    void VCFUX(PPUState& state, PPUInstruction inst);   // Vector Convert from Unsigned Fixed-Point Word
    void VRFIN(PPUState& state, PPUInstruction inst);   // Vector Round to FP Integer Nearest
    void VRFIZ(PPUState& state, PPUInstruction inst);   // Vector Round to FP Integer toward Zero
    void VRFIP(PPUState& state, PPUInstruction inst);   // Vector Round to FP Integer toward Positive Infinity
    void VRFIM(PPUState& state, PPUInstruction inst);   // Vector Round to FP Integer toward Minus Infinity
    
    // ========================================================================
    // System Instructions
    // ========================================================================
    void SC(PPUState& state, PPUInstruction inst);     // System Call
    void TW(PPUState& state, PPUInstruction inst);     // Trap Word
    void TD(PPUState& state, PPUInstruction inst);     // Trap Doubleword
    void TWI(PPUState& state, PPUInstruction inst);    // Trap Word Immediate
    void TDI(PPUState& state, PPUInstruction inst);    // Trap Doubleword Immediate
    void SYNC(PPUState& state, PPUInstruction inst);   // Synchronize
    void LWSYNC(PPUState& state, PPUInstruction inst); // Lightweight Sync
    void EIEIO(PPUState& state, PPUInstruction inst);  // Enforce In-Order Execution of I/O
    void ISYNC(PPUState& state, PPUInstruction inst);  // Instruction Synchronize
    void DCBF(PPUState& state, PPUInstruction inst);   // Data Cache Block Flush
    void DCBI(PPUState& state, PPUInstruction inst);   // Data Cache Block Invalidate
    void DCBST(PPUState& state, PPUInstruction inst);  // Data Cache Block Store
    void DCBT(PPUState& state, PPUInstruction inst);   // Data Cache Block Touch
    void DCBTST(PPUState& state, PPUInstruction inst); // Data Cache Block Touch for Store
    void DCBZ(PPUState& state, PPUInstruction inst);   // Data Cache Block Clear to Zero
    void ICBI(PPUState& state, PPUInstruction inst);   // Instruction Cache Block Invalidate
    
    void LWARX(PPUState& state, PPUInstruction inst);  // Load Word and Reserve Indexed
    void LDARX(PPUState& state, PPUInstruction inst);  // Load Doubleword and Reserve Indexed
    void STWCX(PPUState& state, PPUInstruction inst);  // Store Word Conditional Indexed
    void STDCX(PPUState& state, PPUInstruction inst);  // Store Doubleword Conditional Indexed
    
    // ========================================================================
    // CR Logical Instructions
    // ========================================================================
    void CRAND(PPUState& state, PPUInstruction inst);  // CR AND
    void CROR(PPUState& state, PPUInstruction inst);   // CR OR
    void CRXOR(PPUState& state, PPUInstruction inst);  // CR XOR
    void CRNAND(PPUState& state, PPUInstruction inst); // CR NAND
    void CRNOR(PPUState& state, PPUInstruction inst);  // CR NOR
    void CREQV(PPUState& state, PPUInstruction inst);  // CR Equivalent
    void CRANDC(PPUState& state, PPUInstruction inst); // CR AND with Complement
    void CRORC(PPUState& state, PPUInstruction inst);  // CR OR with Complement
    void MCRF(PPUState& state, PPUInstruction inst);   // Move CR Field
    
    // ========================================================================
    // Helper Functions
    // ========================================================================
    void UpdateCR0(PPUState& state, int64_t value);
    void UpdateCR1(PPUState& state);
    void UpdateCRn(PPUState& state, int n, int64_t a, int64_t b);
    void UpdateCRnU(PPUState& state, int n, uint64_t a, uint64_t b);
    uint32_t GetCRField(const PPUState& state, int field);
    void SetCRField(PPUState& state, int field, uint32_t value);
    bool GetCRBit(const PPUState& state, int bit);
    void SetCRBit(PPUState& state, int bit, bool value);
    
    uint64_t RotateLeft64(uint64_t value, int amount);
    uint32_t RotateLeft32(uint32_t value, int amount);
    uint64_t MakeMask64(int mb, int me);
    uint32_t MakeMask32(int mb, int me);
    
    int CountLeadingZeros32(uint32_t value);
    int CountLeadingZeros64(uint64_t value);
};

} // namespace ppu
} // namespace rpcsx
