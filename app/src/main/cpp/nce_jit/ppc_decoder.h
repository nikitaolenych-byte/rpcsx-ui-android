/**
 * PowerPC Decoder для PS3 Cell PPU
 * Декодування PowerPC інструкцій для JIT компіляції
 */

#ifndef RPCSX_PPC_DECODER_H
#define RPCSX_PPC_DECODER_H

#include <cstdint>
#include <cstddef>

namespace rpcsx::nce::ppc {

// PowerPC регістри (Cell PPU)
enum class GPR : uint8_t {
    R0 = 0, R1, R2, R3, R4, R5, R6, R7,
    R8, R9, R10, R11, R12, R13, R14, R15,
    R16, R17, R18, R19, R20, R21, R22, R23,
    R24, R25, R26, R27, R28, R29, R30, R31
};

// Floating Point регістри
enum class FPR : uint8_t {
    F0 = 0, F1, F2, F3, F4, F5, F6, F7,
    F8, F9, F10, F11, F12, F13, F14, F15,
    F16, F17, F18, F19, F20, F21, F22, F23,
    F24, F25, F26, F27, F28, F29, F30, F31
};

// Vector регістри (AltiVec/VMX)
enum class VR : uint8_t {
    V0 = 0, V1, V2, V3, V4, V5, V6, V7,
    V8, V9, V10, V11, V12, V13, V14, V15,
    V16, V17, V18, V19, V20, V21, V22, V23,
    V24, V25, V26, V27, V28, V29, V30, V31
};

// PowerPC опкоди (primary opcode - біти 0-5)
enum class PrimaryOp : uint8_t {
    TWI = 3,        // Trap Word Immediate
    MULLI = 7,      // Multiply Low Immediate
    SUBFIC = 8,     // Subtract From Immediate Carrying
    CMPLI = 10,     // Compare Logical Immediate
    CMPI = 11,      // Compare Immediate
    ADDIC = 12,     // Add Immediate Carrying
    ADDIC_RC = 13,  // Add Immediate Carrying and Record
    ADDI = 14,      // Add Immediate
    ADDIS = 15,     // Add Immediate Shifted
    BC = 16,        // Branch Conditional
    SC = 17,        // System Call
    B = 18,         // Branch
    OP19 = 19,      // Extended opcodes (bclr, bcctr, etc)
    RLWIMI = 20,    // Rotate Left Word Immediate then Mask Insert
    RLWINM = 21,    // Rotate Left Word Immediate then AND with Mask
    RLWNM = 23,     // Rotate Left Word then AND with Mask
    ORI = 24,       // OR Immediate
    ORIS = 25,      // OR Immediate Shifted
    XORI = 26,      // XOR Immediate
    XORIS = 27,     // XOR Immediate Shifted
    ANDI_RC = 28,   // AND Immediate and Record
    ANDIS_RC = 29,  // AND Immediate Shifted and Record
    OP30 = 30,      // Extended 64-bit rotate/shift
    OP31 = 31,      // Extended ALU opcodes
    LWZ = 32,       // Load Word and Zero
    LWZU = 33,      // Load Word and Zero with Update
    LBZ = 34,       // Load Byte and Zero
    LBZU = 35,      // Load Byte and Zero with Update
    STW = 36,       // Store Word
    STWU = 37,      // Store Word with Update
    STB = 38,       // Store Byte
    STBU = 39,      // Store Byte with Update
    LHZ = 40,       // Load Half Word and Zero
    LHZU = 41,      // Load Half Word and Zero with Update
    LHA = 42,       // Load Half Word Algebraic
    LHAU = 43,      // Load Half Word Algebraic with Update
    STH = 44,       // Store Half Word
    STHU = 45,      // Store Half Word with Update
    LMW = 46,       // Load Multiple Word
    STMW = 47,      // Store Multiple Word
    LFS = 48,       // Load Floating-Point Single
    LFSU = 49,      // Load Floating-Point Single with Update
    LFD = 50,       // Load Floating-Point Double
    LFDU = 51,      // Load Floating-Point Double with Update
    STFS = 52,      // Store Floating-Point Single
    STFSU = 53,     // Store Floating-Point Single with Update
    STFD = 54,      // Store Floating-Point Double
    STFDU = 55,     // Store Floating-Point Double with Update
    OP58 = 58,      // LD/LDU/LWA (64-bit loads)
    OP59 = 59,      // FP single-precision
    OP62 = 62,      // STD/STDU (64-bit stores)
    OP63 = 63,      // FP double-precision
};

// Extended opcodes for OP31 (біти 21-30)
enum class ExtOp31 : uint16_t {
    CMP = 0,
    TW = 4,
    SUBFC = 8,
    ADDC = 10,
    MULHWU = 11,
    MFCR = 19,
    LWARX = 20,
    LWZX = 23,
    SLW = 24,
    CNTLZW = 26,
    AND = 28,
    CMPL = 32,
    SUBF = 40,
    DCBST = 54,
    LWZUX = 55,
    ANDC = 60,
    MULHW = 75,
    MFMSR = 83,
    DCBF = 86,
    LBZX = 87,
    NEG = 104,
    LBZUX = 119,
    NOR = 124,
    SUBFE = 136,
    ADDE = 138,
    MTCRF = 144,
    STDX = 149,
    STWCX = 150,
    STWX = 151,
    STDUX = 181,
    STWUX = 183,
    SUBFZE = 200,
    ADDZE = 202,
    STBX = 215,
    SUBFME = 232,
    ADDME = 234,
    MULLW = 235,
    DCBTST = 246,
    STBUX = 247,
    ADD = 266,
    DCBT = 278,
    LHZX = 279,
    EQV = 284,
    LHZUX = 311,
    XOR = 316,
    MFSPR = 339,
    LHAX = 343,
    LHAUX = 375,
    STHX = 407,
    ORC = 412,
    STHUX = 439,
    OR = 444,
    DIVWU = 459,
    MTSPR = 467,
    NAND = 476,
    DIVW = 491,
    LWAX = 341,
    LDUX = 53,
    DCBI = 470,
    LDX = 21,
    SRAW = 792,
    SRAWI = 824,
    EIEIO = 854,
    EXTSH = 922,
    EXTSB = 954,
    EXTSW = 986,
    ICBI = 982,
    DCBZ = 1014,
};

// Extended opcodes for OP19 (condition register ops)
enum class ExtOp19 : uint16_t {
    MCRF = 0,
    BCLR = 16,      // Branch Conditional to Link Register
    CRNOR = 33,
    RFI = 50,
    CRANDC = 129,
    ISYNC = 150,
    CRXOR = 193,
    CRNAND = 225,
    CRAND = 257,
    CREQV = 289,
    CRORC = 417,
    CROR = 449,
    BCCTR = 528,    // Branch Conditional to Count Register
};

// Тип інструкції
enum class InstrType {
    UNKNOWN,
    // Load/Store
    LOAD_WORD,
    LOAD_BYTE,
    LOAD_HALF,
    LOAD_DOUBLE,
    STORE_WORD,
    STORE_BYTE,
    STORE_HALF,
    STORE_DOUBLE,
    LOAD_MULTIPLE,
    STORE_MULTIPLE,
    // Arithmetic
    ADD,
    ADDI,
    ADDIS,
    ADDC,
    ADDIC,
    SUB,
    SUBF,
    SUBFIC,
    MULLI,
    MULLW,
    DIVW,
    DIVWU,
    NEG,
    // Logic
    AND,
    ANDI,
    ANDIS,
    OR,
    ORI,
    ORIS,
    XOR,
    XORI,
    XORIS,
    NAND,
    NOR,
    EQV,
    ANDC,
    ORC,
    // Shift/Rotate
    SLW,
    SRW,
    SRAW,
    SRAWI,
    RLWINM,
    RLWIMI,
    RLWNM,
    // Compare
    CMP,
    CMPI,
    CMPL,
    CMPLI,
    // Branch
    B,
    BL,
    BC,
    BCLR,
    BCCTR,
    // CR ops
    CRAND,
    CROR,
    CRXOR,
    CRNAND,
    CRNOR,
    CREQV,
    CRANDC,
    CRORC,
    MCRF,
    // Special
    MFSPR,
    MTSPR,
    MFCR,
    MTCRF,
    EXTSB,
    EXTSH,
    EXTSW,
    CNTLZW,
    // System
    SC,
    RFI,
    ISYNC,
    EIEIO,
    SYNC,
    // Cache
    DCBF,
    DCBI,
    DCBST,
    DCBT,
    DCBTST,
    DCBZ,
    ICBI,
    // Trap
    TW,
    TWI,
    // Float
    FP_LOAD,
    FP_STORE,
    FP_ARITH,
};

// Декодована інструкція
struct DecodedInstr {
    InstrType type = InstrType::UNKNOWN;
    uint32_t raw;           // Raw instruction
    
    // Decoded fields
    uint8_t opcode;         // Primary opcode (bits 0-5)
    uint16_t xo;            // Extended opcode
    
    // Register operands
    GPR rd;                 // Destination register
    GPR ra;                 // Source register A
    GPR rb;                 // Source register B
    
    // Immediate values
    int16_t simm;           // Signed immediate (D-form)
    uint16_t uimm;          // Unsigned immediate
    int32_t li;             // Branch offset (I-form)
    
    // Branch fields
    uint8_t bo;             // Branch options
    uint8_t bi;             // Condition bit
    bool aa;                // Absolute address
    bool lk;                // Link (save to LR)
    bool rc;                // Record (update CR0)
    bool oe;                // OE bit (update XER)
    
    // Rotate/Mask fields
    uint8_t sh;             // Shift amount
    uint8_t mb;             // Mask begin
    uint8_t me;             // Mask end
    
    // Special register
    uint16_t spr;           // Special purpose register
    
    // Instruction length (always 4 for PPC)
    size_t length = 4;
};

/**
 * Декодувати PowerPC інструкцію
 * @param code Вказівник на код (big-endian!)
 * @param pc Поточний Program Counter
 * @return Декодована інструкція
 */
DecodedInstr DecodeInstruction(const uint8_t* code, uint64_t pc);

/**
 * Отримати ім'я інструкції для логування
 */
const char* GetInstructionName(InstrType type);

/**
 * Перевірка чи інструкція є branch
 */
inline bool IsBranch(InstrType type) {
    return type == InstrType::B || type == InstrType::BL ||
           type == InstrType::BC || type == InstrType::BCLR ||
           type == InstrType::BCCTR;
}

/**
 * Перевірка чи інструкція є терміналом блоку
 */
inline bool IsBlockTerminator(InstrType type) {
    return IsBranch(type) || type == InstrType::SC || 
           type == InstrType::RFI;
}

} // namespace rpcsx::nce::ppc

#endif // RPCSX_PPC_DECODER_H
