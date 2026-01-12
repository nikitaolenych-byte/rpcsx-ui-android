/**
 * x86-64 Instruction Decoder для NCE JIT
 * Декодує PS4 (x86-64) інструкції для трансляції в ARM64
 */

#ifndef RPCSX_X86_DECODER_H
#define RPCSX_X86_DECODER_H

#include <cstdint>
#include <cstddef>

namespace rpcsx::nce::x86 {

// x86-64 Register IDs
enum class Reg64 : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8 = 8, R9 = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    NONE = 255
};

// x86-64 Opcode categories
enum class OpcodeType : uint8_t {
    UNKNOWN = 0,
    // Data movement
    MOV_REG_REG,
    MOV_REG_IMM,
    MOV_REG_MEM,
    MOV_MEM_REG,
    PUSH,
    POP,
    LEA,
    // Arithmetic
    ADD_REG_REG,
    ADD_REG_IMM,
    SUB_REG_REG,
    SUB_REG_IMM,
    MUL,
    IMUL,
    DIV,
    IDIV,
    INC,
    DEC,
    NEG,
    // Logic
    AND_REG_REG,
    AND_REG_IMM,
    OR_REG_REG,
    OR_REG_IMM,
    XOR_REG_REG,
    XOR_REG_IMM,
    NOT,
    SHL,
    SHR,
    SAR,
    // Compare/Test
    CMP_REG_REG,
    CMP_REG_IMM,
    TEST_REG_REG,
    TEST_REG_IMM,
    // Control flow
    JMP_REL,
    JMP_ABS,
    JCC,  // Conditional jumps
    CALL_REL,
    CALL_ABS,
    RET,
    // SIMD (SSE/AVX)
    MOVAPS,
    MOVUPS,
    ADDPS,
    SUBPS,
    MULPS,
    DIVPS,
    // System
    SYSCALL,
    NOP,
    INT3,
    // Total count
    MAX_OPCODE
};

// Condition codes for JCC
enum class ConditionCode : uint8_t {
    O = 0,   // Overflow
    NO = 1,  // Not overflow
    B = 2,   // Below (CF=1)
    AE = 3,  // Above or equal (CF=0)
    E = 4,   // Equal (ZF=1)
    NE = 5,  // Not equal (ZF=0)
    BE = 6,  // Below or equal
    A = 7,   // Above
    S = 8,   // Sign
    NS = 9,  // Not sign
    P = 10,  // Parity
    NP = 11, // Not parity
    L = 12,  // Less
    GE = 13, // Greater or equal
    LE = 14, // Less or equal
    G = 15   // Greater
};

// Memory operand
struct MemOperand {
    Reg64 base;
    Reg64 index;
    uint8_t scale;  // 1, 2, 4, 8
    int32_t displacement;
    bool has_base;
    bool has_index;
};

// Decoded instruction
struct DecodedInstr {
    OpcodeType type;
    uint8_t length;  // Instruction length in bytes
    
    // Operands
    Reg64 dst_reg;
    Reg64 src_reg;
    int64_t immediate;
    MemOperand mem;
    ConditionCode cond;
    
    // Prefixes
    bool rex_w;      // 64-bit operand
    bool rex_r;      // ModRM.reg extension
    bool rex_x;      // SIB.index extension
    bool rex_b;      // ModRM.rm or SIB.base extension
    bool has_lock;
    bool has_rep;
    
    // Flags affected
    bool affects_flags;
};

/**
 * Decode single x86-64 instruction
 * @param code Pointer to instruction bytes
 * @param max_len Maximum bytes to read
 * @param out Output decoded instruction
 * @return Number of bytes consumed, 0 on error
 */
size_t DecodeInstruction(const uint8_t* code, size_t max_len, DecodedInstr* out);

/**
 * Get instruction mnemonic for debugging
 */
const char* GetOpcodeName(OpcodeType type);

/**
 * Get register name for debugging
 */
const char* GetRegName(Reg64 reg);

} // namespace rpcsx::nce::x86

#endif // RPCSX_X86_DECODER_H
