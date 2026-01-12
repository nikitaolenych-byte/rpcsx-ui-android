/**
 * x86-64 Instruction Decoder Implementation
 * Парсинг PS4 machine code
 */

#include "x86_decoder.h"
#include <cstring>

namespace rpcsx::nce::x86 {

// Opcode names for debugging
static const char* kOpcodeNames[] = {
    "UNKNOWN",
    "MOV_REG_REG", "MOV_REG_IMM", "MOV_REG_MEM", "MOV_MEM_REG",
    "PUSH", "POP", "LEA",
    "ADD_REG_REG", "ADD_REG_IMM", "SUB_REG_REG", "SUB_REG_IMM",
    "MUL", "IMUL", "DIV", "IDIV", "INC", "DEC", "NEG",
    "AND_REG_REG", "AND_REG_IMM", "OR_REG_REG", "OR_REG_IMM",
    "XOR_REG_REG", "XOR_REG_IMM", "NOT", "SHL", "SHR", "SAR",
    "CMP_REG_REG", "CMP_REG_IMM", "TEST_REG_REG", "TEST_REG_IMM",
    "JMP_REL", "JMP_ABS", "JCC", "CALL_REL", "CALL_ABS", "RET",
    "MOVAPS", "MOVUPS", "ADDPS", "SUBPS", "MULPS", "DIVPS",
    "SYSCALL", "NOP", "INT3"
};

static const char* kRegNames[] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
};

const char* GetOpcodeName(OpcodeType type) {
    if (static_cast<int>(type) < static_cast<int>(OpcodeType::MAX_OPCODE)) {
        return kOpcodeNames[static_cast<int>(type)];
    }
    return "INVALID";
}

const char* GetRegName(Reg64 reg) {
    if (static_cast<int>(reg) < 16) {
        return kRegNames[static_cast<int>(reg)];
    }
    return "???";
}

// Parse REX prefix (0x40-0x4F)
static bool ParseRex(uint8_t byte, DecodedInstr* out) {
    if ((byte & 0xF0) != 0x40) return false;
    out->rex_w = (byte & 0x08) != 0;  // 64-bit operand
    out->rex_r = (byte & 0x04) != 0;  // ModRM.reg extension
    out->rex_x = (byte & 0x02) != 0;  // SIB.index extension
    out->rex_b = (byte & 0x01) != 0;  // ModRM.rm extension
    return true;
}

// Parse ModRM byte
static void ParseModRM(uint8_t modrm, const DecodedInstr* instr,
                       uint8_t* mod, uint8_t* reg, uint8_t* rm) {
    *mod = (modrm >> 6) & 0x3;
    *reg = (modrm >> 3) & 0x7;
    *rm = modrm & 0x7;
    
    if (instr->rex_r) *reg |= 0x8;
    if (instr->rex_b) *rm |= 0x8;
}

// Parse SIB byte
static void ParseSIB(uint8_t sib, const DecodedInstr* instr, MemOperand* mem) {
    uint8_t scale = (sib >> 6) & 0x3;
    uint8_t index = (sib >> 3) & 0x7;
    uint8_t base = sib & 0x7;
    
    if (instr->rex_x) index |= 0x8;
    if (instr->rex_b) base |= 0x8;
    
    mem->scale = 1 << scale;  // 1, 2, 4, 8
    mem->index = (index != 4) ? static_cast<Reg64>(index) : Reg64::NONE;
    mem->base = static_cast<Reg64>(base);
    mem->has_index = (index != 4);
    mem->has_base = true;
}

size_t DecodeInstruction(const uint8_t* code, size_t max_len, DecodedInstr* out) {
    if (!code || !out || max_len == 0) return 0;
    
    memset(out, 0, sizeof(*out));
    out->dst_reg = Reg64::NONE;
    out->src_reg = Reg64::NONE;
    out->mem.base = Reg64::NONE;
    out->mem.index = Reg64::NONE;
    
    size_t pos = 0;
    
    // Parse legacy prefixes
    while (pos < max_len) {
        uint8_t b = code[pos];
        if (b == 0xF0) { out->has_lock = true; pos++; }
        else if (b == 0xF2 || b == 0xF3) { out->has_rep = true; pos++; }
        else if (b == 0x66 || b == 0x67) { pos++; } // Operand/address size
        else if (b == 0x2E || b == 0x3E || b == 0x26 || b == 0x64 || 
                 b == 0x65 || b == 0x36) { pos++; } // Segment overrides
        else break;
    }
    
    if (pos >= max_len) return 0;
    
    // Parse REX prefix
    if (ParseRex(code[pos], out)) {
        pos++;
    }
    
    if (pos >= max_len) return 0;
    
    uint8_t opcode = code[pos++];
    uint8_t mod, reg, rm;
    
    // Decode based on opcode
    switch (opcode) {
        // NOP
        case 0x90:
            out->type = OpcodeType::NOP;
            break;
            
        // RET
        case 0xC3:
            out->type = OpcodeType::RET;
            break;
            
        // INT3 (breakpoint)
        case 0xCC:
            out->type = OpcodeType::INT3;
            break;
            
        // PUSH r64
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57:
            out->type = OpcodeType::PUSH;
            out->src_reg = static_cast<Reg64>((opcode - 0x50) | (out->rex_b ? 8 : 0));
            break;
            
        // POP r64
        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5E: case 0x5F:
            out->type = OpcodeType::POP;
            out->dst_reg = static_cast<Reg64>((opcode - 0x58) | (out->rex_b ? 8 : 0));
            break;
            
        // MOV r64, imm64 (REX.W + B8+rd)
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF:
            out->type = OpcodeType::MOV_REG_IMM;
            out->dst_reg = static_cast<Reg64>((opcode - 0xB8) | (out->rex_b ? 8 : 0));
            if (out->rex_w && pos + 8 <= max_len) {
                memcpy(&out->immediate, &code[pos], 8);
                pos += 8;
            } else if (pos + 4 <= max_len) {
                int32_t imm32;
                memcpy(&imm32, &code[pos], 4);
                out->immediate = imm32;  // Sign extend
                pos += 4;
            }
            break;
            
        // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m64, imm8/32
        case 0x83:
            if (pos >= max_len) return 0;
            ParseModRM(code[pos++], out, &mod, &reg, &rm);
            out->affects_flags = true;
            switch (reg) {
                case 0: out->type = OpcodeType::ADD_REG_IMM; break;
                case 1: out->type = OpcodeType::OR_REG_IMM; break;
                case 4: out->type = OpcodeType::AND_REG_IMM; break;
                case 5: out->type = OpcodeType::SUB_REG_IMM; break;
                case 6: out->type = OpcodeType::XOR_REG_IMM; break;
                case 7: out->type = OpcodeType::CMP_REG_IMM; break;
                default: out->type = OpcodeType::UNKNOWN; break;
            }
            if (mod == 3) {
                out->dst_reg = static_cast<Reg64>(rm);
            }
            if (pos < max_len) {
                out->immediate = static_cast<int8_t>(code[pos++]);  // Sign extend
            }
            break;
            
        // MOV r64, r/m64 (8B /r)
        case 0x8B:
            if (pos >= max_len) return 0;
            ParseModRM(code[pos++], out, &mod, &reg, &rm);
            out->dst_reg = static_cast<Reg64>(reg);
            if (mod == 3) {
                out->type = OpcodeType::MOV_REG_REG;
                out->src_reg = static_cast<Reg64>(rm);
            } else {
                out->type = OpcodeType::MOV_REG_MEM;
                // Parse memory operand...
            }
            break;
            
        // MOV r/m64, r64 (89 /r)
        case 0x89:
            if (pos >= max_len) return 0;
            ParseModRM(code[pos++], out, &mod, &reg, &rm);
            out->src_reg = static_cast<Reg64>(reg);
            if (mod == 3) {
                out->type = OpcodeType::MOV_REG_REG;
                out->dst_reg = static_cast<Reg64>(rm);
            } else {
                out->type = OpcodeType::MOV_MEM_REG;
            }
            break;
            
        // ADD r64, r/m64 (03 /r)
        case 0x03:
            if (pos >= max_len) return 0;
            ParseModRM(code[pos++], out, &mod, &reg, &rm);
            out->type = OpcodeType::ADD_REG_REG;
            out->dst_reg = static_cast<Reg64>(reg);
            out->src_reg = static_cast<Reg64>(rm);
            out->affects_flags = true;
            break;
            
        // SUB r64, r/m64 (2B /r)
        case 0x2B:
            if (pos >= max_len) return 0;
            ParseModRM(code[pos++], out, &mod, &reg, &rm);
            out->type = OpcodeType::SUB_REG_REG;
            out->dst_reg = static_cast<Reg64>(reg);
            out->src_reg = static_cast<Reg64>(rm);
            out->affects_flags = true;
            break;
            
        // XOR r64, r/m64 (33 /r)
        case 0x33:
            if (pos >= max_len) return 0;
            ParseModRM(code[pos++], out, &mod, &reg, &rm);
            out->type = OpcodeType::XOR_REG_REG;
            out->dst_reg = static_cast<Reg64>(reg);
            out->src_reg = static_cast<Reg64>(rm);
            out->affects_flags = true;
            break;
            
        // CMP r64, r/m64 (3B /r)
        case 0x3B:
            if (pos >= max_len) return 0;
            ParseModRM(code[pos++], out, &mod, &reg, &rm);
            out->type = OpcodeType::CMP_REG_REG;
            out->dst_reg = static_cast<Reg64>(reg);
            out->src_reg = static_cast<Reg64>(rm);
            out->affects_flags = true;
            break;
            
        // TEST r/m64, r64 (85 /r)
        case 0x85:
            if (pos >= max_len) return 0;
            ParseModRM(code[pos++], out, &mod, &reg, &rm);
            out->type = OpcodeType::TEST_REG_REG;
            out->dst_reg = static_cast<Reg64>(rm);
            out->src_reg = static_cast<Reg64>(reg);
            out->affects_flags = true;
            break;
            
        // LEA r64, m (8D /r)
        case 0x8D:
            if (pos >= max_len) return 0;
            ParseModRM(code[pos++], out, &mod, &reg, &rm);
            out->type = OpcodeType::LEA;
            out->dst_reg = static_cast<Reg64>(reg);
            // Memory operand in rm...
            break;
            
        // JMP rel8 (EB)
        case 0xEB:
            out->type = OpcodeType::JMP_REL;
            if (pos < max_len) {
                out->immediate = static_cast<int8_t>(code[pos++]);
            }
            break;
            
        // JMP rel32 (E9)
        case 0xE9:
            out->type = OpcodeType::JMP_REL;
            if (pos + 4 <= max_len) {
                int32_t rel32;
                memcpy(&rel32, &code[pos], 4);
                out->immediate = rel32;
                pos += 4;
            }
            break;
            
        // CALL rel32 (E8)
        case 0xE8:
            out->type = OpcodeType::CALL_REL;
            if (pos + 4 <= max_len) {
                int32_t rel32;
                memcpy(&rel32, &code[pos], 4);
                out->immediate = rel32;
                pos += 4;
            }
            break;
            
        // Jcc rel8 (70-7F)
        case 0x70: case 0x71: case 0x72: case 0x73:
        case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B:
        case 0x7C: case 0x7D: case 0x7E: case 0x7F:
            out->type = OpcodeType::JCC;
            out->cond = static_cast<ConditionCode>(opcode - 0x70);
            if (pos < max_len) {
                out->immediate = static_cast<int8_t>(code[pos++]);
            }
            break;
            
        // Two-byte opcodes (0F xx)
        case 0x0F:
            if (pos >= max_len) return 0;
            opcode = code[pos++];
            switch (opcode) {
                // Jcc rel32 (0F 80-8F)
                case 0x80: case 0x81: case 0x82: case 0x83:
                case 0x84: case 0x85: case 0x86: case 0x87:
                case 0x88: case 0x89: case 0x8A: case 0x8B:
                case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                    out->type = OpcodeType::JCC;
                    out->cond = static_cast<ConditionCode>(opcode - 0x80);
                    if (pos + 4 <= max_len) {
                        int32_t rel32;
                        memcpy(&rel32, &code[pos], 4);
                        out->immediate = rel32;
                        pos += 4;
                    }
                    break;
                    
                // SYSCALL (0F 05)
                case 0x05:
                    out->type = OpcodeType::SYSCALL;
                    break;
                    
                // MOVAPS xmm, xmm/m128 (0F 28)
                case 0x28:
                    out->type = OpcodeType::MOVAPS;
                    if (pos < max_len) pos++;  // Skip ModRM for now
                    break;
                    
                // MOVUPS xmm, xmm/m128 (0F 10)
                case 0x10:
                    out->type = OpcodeType::MOVUPS;
                    if (pos < max_len) pos++;
                    break;
                    
                // ADDPS xmm, xmm/m128 (0F 58)
                case 0x58:
                    out->type = OpcodeType::ADDPS;
                    if (pos < max_len) pos++;
                    break;
                    
                // MULPS xmm, xmm/m128 (0F 59)
                case 0x59:
                    out->type = OpcodeType::MULPS;
                    if (pos < max_len) pos++;
                    break;
                    
                default:
                    out->type = OpcodeType::UNKNOWN;
                    break;
            }
            break;
            
        default:
            out->type = OpcodeType::UNKNOWN;
            break;
    }
    
    out->length = static_cast<uint8_t>(pos);
    return pos;
}

} // namespace rpcsx::nce::x86
