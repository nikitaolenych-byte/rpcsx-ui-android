/**
 * ARM64 Code Emitter Implementation
 * PowerPC → ARM64 Translation for PS3 Cell PPU
 * 
 * This implements the core translation from 64-bit PowerPC (Cell PPU)
 * instructions to ARM64 (AArch64) instructions.
 * 
 * Key considerations:
 * - PS3 is big-endian, ARM64 is little-endian -> byte swaps needed
 * - PowerPC has 32 GPR, ARM64 has 31 -> need register spilling
 * - PowerPC CR is 32-bit (8 fields), ARM64 uses NZCV flags
 * - PowerPC rotate-and-mask instructions need multiple ARM64 ops
 */

#include "arm64_emitter.h"
#include <android/log.h>

#define LOG_TAG "RPCSX-ARM64"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::nce::arm64 {

bool PPCTranslator::Translate(const ppc::DecodedInstr& instr) {
    switch (instr.type) {
    // ================== Load ==================
    case ppc::InstrType::LOAD_WORD:
    case ppc::InstrType::LOAD_HALF:
    case ppc::InstrType::LOAD_BYTE:
    case ppc::InstrType::LOAD_DOUBLE:
        EmitLoad(instr);
        return true;
    
    case ppc::InstrType::LOAD_MULTIPLE:
        EmitLoadMultiple(instr);
        return true;
        
    // ================== Store ==================
    case ppc::InstrType::STORE_WORD:
    case ppc::InstrType::STORE_HALF:
    case ppc::InstrType::STORE_BYTE:
    case ppc::InstrType::STORE_DOUBLE:
        EmitStore(instr);
        return true;
    
    case ppc::InstrType::STORE_MULTIPLE:
        EmitStoreMultiple(instr);
        return true;
        
    // ================== Arithmetic Immediate ==================
    case ppc::InstrType::ADDI:
    case ppc::InstrType::ADDIS:
    case ppc::InstrType::ADDIC:
        EmitAdd(instr);
        return true;
        
    case ppc::InstrType::SUBFIC:
        EmitSub(instr);
        return true;
        
    case ppc::InstrType::MULLI:
        EmitMul(instr);
        return true;
        
    // ================== Arithmetic Register ==================
    case ppc::InstrType::ADD:
    case ppc::InstrType::ADDC:
        EmitAdd(instr);
        return true;
        
    case ppc::InstrType::SUBF:
    case ppc::InstrType::SUB:
        EmitSub(instr);
        return true;
        
    case ppc::InstrType::MULLW:
        EmitMul(instr);
        return true;
        
    case ppc::InstrType::DIVW:
    case ppc::InstrType::DIVWU:
        EmitDiv(instr);
        return true;
        
    case ppc::InstrType::NEG:
        emit_.NEG(MapPPCGPR(instr.rD), MapPPCGPR(instr.rA));
        if (instr.rc) {
            EmitCR0Update(MapPPCGPR(instr.rD));
        }
        return true;
        
    // ================== Logic ==================
    case ppc::InstrType::AND:
    case ppc::InstrType::ANDI:
    case ppc::InstrType::ANDIS:
    case ppc::InstrType::ANDC:
    case ppc::InstrType::OR:
    case ppc::InstrType::ORI:
    case ppc::InstrType::ORIS:
    case ppc::InstrType::ORC:
    case ppc::InstrType::XOR:
    case ppc::InstrType::XORI:
    case ppc::InstrType::XORIS:
    case ppc::InstrType::NAND:
    case ppc::InstrType::NOR:
    case ppc::InstrType::EQV:
        EmitLogic(instr);
        return true;
        
    // ================== Shift ==================
    case ppc::InstrType::SLW:
    case ppc::InstrType::SRW:
    case ppc::InstrType::SRAW:
    case ppc::InstrType::SRAWI:
        EmitShift(instr);
        return true;
        
    // ================== Rotate ==================
    case ppc::InstrType::RLWINM:
    case ppc::InstrType::RLWIMI:
    case ppc::InstrType::RLWNM:
        EmitRotate(instr);
        return true;
        
    // ================== Compare ==================
    case ppc::InstrType::CMP:
    case ppc::InstrType::CMPI:
    case ppc::InstrType::CMPL:
    case ppc::InstrType::CMPLI:
        EmitCompare(instr);
        return true;
        
    // ================== Branch ==================
    case ppc::InstrType::B:
    case ppc::InstrType::BL:
    case ppc::InstrType::BC:
    case ppc::InstrType::BCLR:
    case ppc::InstrType::BCCTR:
        EmitBranch(instr);
        return true;
        
    // ================== CR Operations ==================
    case ppc::InstrType::CRAND:
    case ppc::InstrType::CROR:
    case ppc::InstrType::CRXOR:
    case ppc::InstrType::CRNAND:
    case ppc::InstrType::CRNOR:
    case ppc::InstrType::CREQV:
    case ppc::InstrType::CRANDC:
    case ppc::InstrType::CRORC:
    case ppc::InstrType::MCRF:
        EmitCRLogic(instr);
        return true;
        
    // ================== SPR ==================
    case ppc::InstrType::MFSPR:
    case ppc::InstrType::MTSPR:
    case ppc::InstrType::MFCR:
    case ppc::InstrType::MTCRF:
        EmitSPR(instr);
        return true;
        
    // ================== Sign Extend ==================
    case ppc::InstrType::EXTSB:
        emit_.SXTB(MapPPCGPR(instr.rA), MapPPCGPR(static_cast<ppc::GPR>(instr.rD)));
        if (instr.rc) EmitCR0Update(MapPPCGPR(instr.rA));
        return true;
        
    case ppc::InstrType::EXTSH:
        emit_.SXTH(MapPPCGPR(instr.rA), MapPPCGPR(static_cast<ppc::GPR>(instr.rD)));
        if (instr.rc) EmitCR0Update(MapPPCGPR(instr.rA));
        return true;
        
    case ppc::InstrType::EXTSW:
        emit_.SXTW(MapPPCGPR(instr.rA), MapPPCGPR(static_cast<ppc::GPR>(instr.rD)));
        if (instr.rc) EmitCR0Update(MapPPCGPR(instr.rA));
        return true;
    
    // ================== Count Leading Zeros ==================
    case ppc::InstrType::CNTLZW:
        emit_.CLZ_W(MapPPCGPR(instr.rA), MapPPCGPR(static_cast<ppc::GPR>(instr.rD)));
        if (instr.rc) EmitCR0Update(MapPPCGPR(instr.rA));
        return true;
        
    // ================== System ==================
    case ppc::InstrType::SC:
        EmitSystem(instr);
        return true;
        
    case ppc::InstrType::RFI:
        // Return from interrupt - complex, emit trap for now
        emit_.BRK(0x100);
        return true;
        
    case ppc::InstrType::ISYNC:
        emit_.ISB();
        return true;
        
    case ppc::InstrType::EIEIO:
    case ppc::InstrType::SYNC:
        emit_.DMB();
        return true;
        
    // ================== Cache ==================
    case ppc::InstrType::DCBF:
    case ppc::InstrType::DCBI:
    case ppc::InstrType::DCBST:
    case ppc::InstrType::DCBT:
    case ppc::InstrType::DCBTST:
    case ppc::InstrType::DCBZ:
    case ppc::InstrType::ICBI:
        // Cache ops - NOP on ARM64 for now (could use DC/IC instructions)
        emit_.NOP();
        return true;
    
    // ================== Trap ==================
    case ppc::InstrType::TW:
    case ppc::InstrType::TWI:
        EmitTrap(instr);
        return true;
        
    // ================== Floating Point ==================
    case ppc::InstrType::FP_LOAD:
        EmitFPLoad(instr);
        return true;
        
    case ppc::InstrType::FP_STORE:
        EmitFPStore(instr);
        return true;
        
    case ppc::InstrType::FP_ARITH:
        EmitFPArith(instr);
        return true;
        
    default:
        LOGW("Unhandled PPC instruction type: %d (%s)", 
             static_cast<int>(instr.type), 
             ppc::GetInstructionName(instr.type));
        return false;
    }
}

// ============================================
// CR0 Update Helper
// ============================================

void PPCTranslator::EmitCR0Update(GpReg result) {
    // CR0 is set based on comparison with 0:
    // CR0[LT] = result < 0
    // CR0[GT] = result > 0
    // CR0[EQ] = result == 0
    // CR0[SO] = XER[SO] (copied)
    
    // Compare result with 0 (sets ARM64 flags)
    emit_.CMP(result, GpReg::ZR);
    
    // Build CR0 value based on flags
    // Use CSET to extract each flag
    emit_.CSET(REG_TMP1, Cond::LT);     // LT bit
    emit_.LSL_IMM(REG_TMP1, REG_TMP1, 3);
    
    emit_.CSET(REG_TMP2, Cond::GT);     // GT bit
    emit_.LSL_IMM(REG_TMP2, REG_TMP2, 2);
    emit_.ORR(REG_TMP1, REG_TMP1, REG_TMP2);
    
    emit_.CSET(REG_TMP2, Cond::EQ);     // EQ bit
    emit_.LSL_IMM(REG_TMP2, REG_TMP2, 1);
    emit_.ORR(REG_TMP1, REG_TMP1, REG_TMP2);
    
    // Copy SO from XER (bit 0 of CR0)
    // For simplicity, assume SO is stored in a known location
    // TODO: Properly track XER[SO]
    
    // Store CR0 field (bits 0-3 of CR)
    // CR is stored in REG_CR, CR0 is the highest 4 bits
    emit_.LSL_IMM(REG_TMP1, REG_TMP1, 28);  // Shift to CR0 position
    
    // Clear CR0 field in CR and OR in new value
    emit_.MOV_IMM64(REG_TMP2, 0x0FFFFFFF);  // Mask to clear CR0
    emit_.AND(REG_CR, REG_CR, REG_TMP2);
    emit_.ORR(REG_CR, REG_CR, REG_TMP1);
}

void PPCTranslator::EmitLoad(const ppc::DecodedInstr& instr) {
    GpReg rd = MapPPCGPR(instr.rD);
    GpReg ra = MapPPCGPR(instr.rA);
    int32_t offset = instr.simm;
    
    // Handle ra=0 case (use 0 instead of r0)
    if (static_cast<uint8_t>(instr.rA) == 0) {
        // Load from absolute address
        emit_.MOV_IMM64(REG_TMP1, offset);
        ra = REG_TMP1;
        offset = 0;
    }
    
    // PS3 є big-endian, ARM64 є little-endian
    // Потрібно робити byte-swap після load
    
    switch (instr.type) {
    case ppc::InstrType::LOAD_DOUBLE:
        emit_.LDR(rd, ra, offset);
        emit_.REV(rd, rd);  // byte swap 64-bit
        break;
        
    case ppc::InstrType::LOAD_WORD:
        emit_.LDR_W(rd, ra, offset);
        emit_.REV_W(rd, rd);  // byte swap 32-bit
        break;
        
    case ppc::InstrType::LOAD_HALF:
        emit_.LDRH(rd, ra, offset);
        emit_.REV16(rd, rd);  // byte swap 16-bit
        break;
        
    case ppc::InstrType::LOAD_BYTE:
        emit_.LDRB(rd, ra, offset);
        // No swap needed for single byte
        break;
        
    default:
        break;
    }
}

void PPCTranslator::EmitStore(const ppc::DecodedInstr& instr) {
    GpReg rs = MapPPCGPR(static_cast<ppc::GPR>(instr.rD));  // Source register
    GpReg ra = MapPPCGPR(instr.rA);
    int32_t offset = instr.simm;
    
    // Handle ra=0 case
    if (static_cast<uint8_t>(instr.rA) == 0) {
        emit_.MOV_IMM64(REG_TMP1, offset);
        ra = REG_TMP1;
        offset = 0;
    }
    
    // Byte-swap before store (big-endian)
    switch (instr.type) {
    case ppc::InstrType::STORE_DOUBLE:
        emit_.REV(REG_TMP2, rs);  // byte swap to temp
        emit_.STR(REG_TMP2, ra, offset);
        break;
        
    case ppc::InstrType::STORE_WORD:
        emit_.REV_W(REG_TMP2, rs);
        emit_.STR_W(REG_TMP2, ra, offset);
        break;
        
    case ppc::InstrType::STORE_HALF:
        emit_.REV16(REG_TMP2, rs);
        emit_.STRH(REG_TMP2, ra, offset);
        break;
        
    case ppc::InstrType::STORE_BYTE:
        emit_.STRB(rs, ra, offset);
        break;
        
    default:
        break;
    }
}

void PPCTranslator::EmitLoadMultiple(const ppc::DecodedInstr& instr) {
    // lmw rD, d(rA) - Load Multiple Word
    // Loads (32-rD) consecutive words starting at EA
    GpReg ra = MapPPCGPR(instr.rA);
    int32_t offset = instr.simm;
    uint8_t start_reg = static_cast<uint8_t>(instr.rD);
    
    // Calculate effective address
    if (static_cast<uint8_t>(instr.rA) == 0) {
        emit_.MOV_IMM64(REG_TMP1, offset);
    } else {
        emit_.ADD_IMM(REG_TMP1, ra, offset);
    }
    
    // Load each register
    for (uint8_t r = start_reg; r < 32; r++) {
        int32_t reg_offset = (r - start_reg) * 4;
        GpReg rd = MapPPCGPR(static_cast<ppc::GPR>(r));
        
        emit_.LDR_W(rd, REG_TMP1, reg_offset);
        emit_.REV_W(rd, rd);  // Big-endian swap
    }
}

void PPCTranslator::EmitStoreMultiple(const ppc::DecodedInstr& instr) {
    // stmw rS, d(rA) - Store Multiple Word
    // Stores (32-rS) consecutive words starting at EA
    GpReg ra = MapPPCGPR(instr.rA);
    int32_t offset = instr.simm;
    uint8_t start_reg = static_cast<uint8_t>(instr.rD);
    
    // Calculate effective address
    if (static_cast<uint8_t>(instr.rA) == 0) {
        emit_.MOV_IMM64(REG_TMP1, offset);
    } else {
        emit_.ADD_IMM(REG_TMP1, ra, offset);
    }
    
    // Store each register
    for (uint8_t r = start_reg; r < 32; r++) {
        int32_t reg_offset = (r - start_reg) * 4;
        GpReg rs = MapPPCGPR(static_cast<ppc::GPR>(r));
        
        emit_.REV_W(REG_TMP2, rs);  // Big-endian swap
        emit_.STR_W(REG_TMP2, REG_TMP1, reg_offset);
    }
}

void PPCTranslator::EmitAdd(const ppc::DecodedInstr& instr) {
    GpReg rd = MapPPCGPR(instr.rD);
    GpReg ra = MapPPCGPR(instr.rA);
    
    switch (instr.type) {
    case ppc::InstrType::ADDI:
        // addi rd, ra, simm (ra=0 means load immediate)
        if (static_cast<uint8_t>(instr.rA) == 0) {
            // li rd, simm
            if (instr.simm >= 0) {
                emit_.MOVZ(rd, instr.simm, 0);
            } else {
                emit_.MOVN(rd, ~instr.simm, 0);
            }
        } else {
            if (instr.simm >= 0) {
                emit_.ADD_IMM(rd, ra, instr.simm);
            } else {
                emit_.SUB_IMM(rd, ra, -instr.simm);
            }
        }
        break;
        
    case ppc::InstrType::ADDIS:
        // addis rd, ra, simm (simm shifted left 16)
        {
            int32_t shifted = static_cast<int32_t>(instr.simm) << 16;
            if (static_cast<uint8_t>(instr.rA) == 0) {
                // lis rd, simm
                emit_.MOV_IMM64(rd, shifted);
            } else {
                emit_.MOV_IMM64(REG_TMP1, shifted);
                emit_.ADD(rd, ra, REG_TMP1);
            }
        }
        break;
        
    case ppc::InstrType::ADD:
    case ppc::InstrType::ADDC:
        {
            GpReg rb = MapPPCGPR(instr.rB);
            if (instr.rc) {
                emit_.ADDS(rd, ra, rb);
                // Update CR0 based on result
            } else {
                emit_.ADD(rd, ra, rb);
            }
        }
        break;
        
    default:
        break;
    }
}

void PPCTranslator::EmitSub(const ppc::DecodedInstr& instr) {
    GpReg rd = MapPPCGPR(instr.rD);
    GpReg ra = MapPPCGPR(instr.rA);
    
    switch (instr.type) {
    case ppc::InstrType::SUBFIC:
        // subfic rd, ra, simm: rd = simm - ra
        emit_.MOV_IMM64(REG_TMP1, instr.simm);
        emit_.SUB(rd, REG_TMP1, ra);
        break;
        
    case ppc::InstrType::SUBF:
    case ppc::InstrType::SUB:
        {
            // subf rd, ra, rb: rd = rb - ra
            GpReg rb = MapPPCGPR(instr.rB);
            if (instr.rc) {
                emit_.SUBS(rd, rb, ra);
            } else {
                emit_.SUB(rd, rb, ra);
            }
        }
        break;
        
    default:
        break;
    }
}

void PPCTranslator::EmitMul(const ppc::DecodedInstr& instr) {
    GpReg rd = MapPPCGPR(instr.rD);
    GpReg ra = MapPPCGPR(instr.rA);
    
    switch (instr.type) {
    case ppc::InstrType::MULLI:
        emit_.MOV_IMM64(REG_TMP1, instr.simm);
        emit_.MUL(rd, ra, REG_TMP1);
        break;
        
    case ppc::InstrType::MULLW:
        {
            GpReg rb = MapPPCGPR(instr.rB);
            emit_.MUL(rd, ra, rb);
        }
        break;
        
    default:
        break;
    }
}

void PPCTranslator::EmitDiv(const ppc::DecodedInstr& instr) {
    GpReg rd = MapPPCGPR(instr.rD);
    GpReg ra = MapPPCGPR(instr.rA);
    GpReg rb = MapPPCGPR(instr.rB);
    
    if (instr.type == ppc::InstrType::DIVWU) {
        emit_.UDIV(rd, ra, rb);
    } else {
        emit_.SDIV(rd, ra, rb);
    }
}

void PPCTranslator::EmitLogic(const ppc::DecodedInstr& instr) {
    GpReg ra = MapPPCGPR(instr.rA);
    GpReg rs = MapPPCGPR(static_cast<ppc::GPR>(instr.rD));  // Source
    GpReg rb = MapPPCGPR(instr.rB);
    
    switch (instr.type) {
    case ppc::InstrType::AND:
        emit_.AND(ra, rs, rb);
        break;
        
    case ppc::InstrType::ANDI:
        emit_.MOV_IMM64(REG_TMP1, instr.uimm);
        emit_.AND(ra, rs, REG_TMP1);
        break;
        
    case ppc::InstrType::ANDIS:
        emit_.MOV_IMM64(REG_TMP1, static_cast<uint32_t>(instr.uimm) << 16);
        emit_.AND(ra, rs, REG_TMP1);
        break;
        
    case ppc::InstrType::ANDC:
        // andc ra, rs, rb: ra = rs & ~rb
        emit_.MVN(REG_TMP1, rb);
        emit_.AND(ra, rs, REG_TMP1);
        break;
        
    case ppc::InstrType::OR:
        emit_.ORR(ra, rs, rb);
        break;
        
    case ppc::InstrType::ORI:
        if (instr.uimm == 0) {
            // nop (ori 0,0,0)
            emit_.NOP();
        } else {
            emit_.MOV_IMM64(REG_TMP1, instr.uimm);
            emit_.ORR(ra, rs, REG_TMP1);
        }
        break;
        
    case ppc::InstrType::ORIS:
        emit_.MOV_IMM64(REG_TMP1, static_cast<uint32_t>(instr.uimm) << 16);
        emit_.ORR(ra, rs, REG_TMP1);
        break;
        
    case ppc::InstrType::ORC:
        // orc ra, rs, rb: ra = rs | ~rb
        emit_.MVN(REG_TMP1, rb);
        emit_.ORR(ra, rs, REG_TMP1);
        break;
        
    case ppc::InstrType::XOR:
        emit_.EOR(ra, rs, rb);
        break;
        
    case ppc::InstrType::XORI:
        emit_.MOV_IMM64(REG_TMP1, instr.uimm);
        emit_.EOR(ra, rs, REG_TMP1);
        break;
        
    case ppc::InstrType::XORIS:
        emit_.MOV_IMM64(REG_TMP1, static_cast<uint32_t>(instr.uimm) << 16);
        emit_.EOR(ra, rs, REG_TMP1);
        break;
        
    case ppc::InstrType::NAND:
        emit_.AND(REG_TMP1, rs, rb);
        emit_.MVN(ra, REG_TMP1);
        break;
        
    case ppc::InstrType::NOR:
        emit_.ORR(REG_TMP1, rs, rb);
        emit_.MVN(ra, REG_TMP1);
        break;
        
    case ppc::InstrType::EQV:
        // eqv ra, rs, rb: ra = ~(rs ^ rb)
        emit_.EOR(REG_TMP1, rs, rb);
        emit_.MVN(ra, REG_TMP1);
        break;
        
    default:
        break;
    }
}

void PPCTranslator::EmitShift(const ppc::DecodedInstr& instr) {
    GpReg ra = MapPPCGPR(instr.rA);
    GpReg rs = MapPPCGPR(static_cast<ppc::GPR>(instr.rD));
    GpReg rb = MapPPCGPR(instr.rB);
    
    switch (instr.type) {
    case ppc::InstrType::SLW:
        emit_.LSL(ra, rs, rb);
        break;
        
    case ppc::InstrType::SRW:
        emit_.LSR(ra, rs, rb);
        break;
        
    case ppc::InstrType::SRAW:
        emit_.ASR(ra, rs, rb);
        break;
        
    case ppc::InstrType::SRAWI:
        emit_.ASR_IMM(ra, rs, instr.sh);
        break;
        
    default:
        break;
    }
}

void PPCTranslator::EmitRotate(const ppc::DecodedInstr& instr) {
    GpReg ra = MapPPCGPR(instr.rA);
    GpReg rs = MapPPCGPR(static_cast<ppc::GPR>(instr.rD));
    
    // rlwinm ra, rs, sh, mb, me
    // Rotate left, then mask bits [mb:me]
    
    // ARM64 doesn't have direct rotate-and-mask
    // Use: ROR + AND with mask
    
    uint32_t sh = instr.sh;
    uint32_t mb = instr.mb;
    uint32_t me = instr.me;
    
    // Calculate mask
    uint32_t mask;
    if (mb <= me) {
        mask = ((1u << (me - mb + 1)) - 1) << (31 - me);
    } else {
        mask = ~(((1u << (mb - me - 1)) - 1) << (31 - mb + 1));
    }
    
    // Rotate (left by sh = right by 32-sh)
    uint32_t ror_amount = (32 - sh) & 31;
    
    emit_.ROR_IMM(REG_TMP1, rs, ror_amount);
    emit_.MOV_IMM64(REG_TMP2, mask);
    emit_.AND(ra, REG_TMP1, REG_TMP2);
}

void PPCTranslator::EmitCompare(const ppc::DecodedInstr& instr) {
    GpReg ra = MapPPCGPR(instr.rA);
    
    switch (instr.type) {
    case ppc::InstrType::CMPI:
        // cmpi crD, L, ra, simm
        emit_.CMP_IMM(ra, instr.simm);
        // TODO: Store flags to CR field
        break;
        
    case ppc::InstrType::CMPLI:
        // cmpli crD, L, ra, uimm
        emit_.MOV_IMM64(REG_TMP1, instr.uimm);
        emit_.CMP(ra, REG_TMP1);
        break;
        
    case ppc::InstrType::CMP:
        {
            GpReg rb = MapPPCGPR(instr.rB);
            emit_.CMP(ra, rb);
        }
        break;
        
    case ppc::InstrType::CMPL:
        {
            GpReg rb = MapPPCGPR(instr.rB);
            emit_.CMP(ra, rb);  // Same encoding, flags indicate unsigned
        }
        break;
        
    default:
        break;
    }
}

void PPCTranslator::EmitBranch(const ppc::DecodedInstr& instr) {
    switch (instr.type) {
    case ppc::InstrType::B:
        // Unconditional branch
        emit_.B(instr.li);
        break;
        
    case ppc::InstrType::BL:
        // Branch and link (call)
        emit_.BL(instr.li);
        break;
        
    case ppc::InstrType::BC:
        // Conditional branch
        // BO field determines condition
        {
            Cond cond = Cond::AL;
            uint8_t bo = instr.bo;
            
            // Decode BO field
            if ((bo & 0x14) == 0x14) {
                // Always branch
                cond = Cond::AL;
            } else if (bo & 0x10) {
                // Branch if CTR != 0
                emit_.SUB_IMM(REG_CTR, REG_CTR, 1);
                cond = Cond::NE;
            } else {
                // Branch based on CR bit
                uint8_t bi = instr.bi;
                bool branch_true = (bo & 0x08) != 0;
                
                // Map CR bit to ARM64 condition
                switch (bi & 3) {
                case 0: cond = branch_true ? Cond::LT : Cond::GE; break;  // LT
                case 1: cond = branch_true ? Cond::GT : Cond::LE; break;  // GT
                case 2: cond = branch_true ? Cond::EQ : Cond::NE; break;  // EQ
                case 3: cond = branch_true ? Cond::VS : Cond::VC; break;  // SO/OV
                }
            }
            
            emit_.B_COND(cond, instr.li);
        }
        break;
        
    case ppc::InstrType::BCLR:
        // Branch to link register
        if (instr.lk) {
            emit_.BLR(REG_LR);
        } else {
            emit_.BR(REG_LR);
        }
        break;
        
    case ppc::InstrType::BCCTR:
        // Branch to count register
        if (instr.lk) {
            emit_.BLR(REG_CTR);
        } else {
            emit_.BR(REG_CTR);
        }
        break;
        
    default:
        break;
    }
}

void PPCTranslator::EmitSystem(const ppc::DecodedInstr& instr) {
    // sc - system call
    // Trap to hypervisor/kernel
    emit_.SVC(0);
}

void PPCTranslator::EmitSPR(const ppc::DecodedInstr& instr) {
    GpReg rd = MapPPCGPR(instr.rD);
    
    switch (instr.type) {
    case ppc::InstrType::MFSPR:
        // Move from special purpose register
        switch (instr.spr) {
        case 1:   // XER
            emit_.MOV(rd, REG_XER);
            break;
        case 8:   // LR
            emit_.MOV(rd, REG_LR);
            break;
        case 9:   // CTR
            emit_.MOV(rd, REG_CTR);
            break;
        case 268: // TB (Time Base) - lower
        case 269: // TBU (Time Base) - upper
            // Read system counter (CNTVCT_EL0)
            // For now, just return 0
            emit_.MOVZ(rd, 0, 0);
            break;
        default:
            // Load from state struct
            emit_.LDR(rd, REG_STATE, PPUStateOffsets::XER + (instr.spr - 1) * 8);
            break;
        }
        break;
        
    case ppc::InstrType::MTSPR:
        // Move to special purpose register
        switch (instr.spr) {
        case 1:   // XER
            emit_.MOV(REG_XER, rd);
            break;
        case 8:   // LR
            emit_.MOV(REG_LR, rd);
            break;
        case 9:   // CTR
            emit_.MOV(REG_CTR, rd);
            break;
        default:
            emit_.STR(rd, REG_STATE, PPUStateOffsets::XER + (instr.spr - 1) * 8);
            break;
        }
        break;
        
    case ppc::InstrType::MFCR:
        // Move from condition register
        emit_.MOV(rd, REG_CR);
        break;
        
    case ppc::InstrType::MTCRF:
        // Move to condition register field
        // The FXM field (bits 12-19) specifies which CR fields to update
        emit_.MOV(REG_CR, rd);
        break;
        
    default:
        break;
    }
}

// ============================================
// Trap Instructions
// ============================================

void PPCTranslator::EmitTrap(const ppc::DecodedInstr& instr) {
    // tw/twi - Trap Word
    // Compares values and traps if condition met
    
    GpReg ra = MapPPCGPR(instr.rA);
    uint8_t to = instr.bo;  // TO field stored in bo
    
    if (instr.type == ppc::InstrType::TWI) {
        // twi TO, rA, SIMM
        emit_.MOV_IMM64(REG_TMP1, instr.simm);
    } else {
        // tw TO, rA, rB
        emit_.MOV(REG_TMP1, MapPPCGPR(instr.rB));
    }
    
    // Compare rA with rB/SIMM
    emit_.CMP(ra, REG_TMP1);
    
    // Check each condition and trap if met
    // TO bits: 0=LT, 1=GT, 2=EQ, 3=LTU, 4=GTU
    
    // For simplicity, always trap if any TO bit is set
    // A full implementation would check each condition
    if (to != 0) {
        // Conditional trap based on comparison
        if (to & 0x10) {  // LT (signed)
            emit_.B_COND(Cond::LT, 8);
            emit_.B(12);  // Skip trap
            emit_.BRK(0x80);  // Trap
        }
        if (to & 0x08) {  // GT (signed)
            emit_.B_COND(Cond::GT, 8);
            emit_.B(12);
            emit_.BRK(0x80);
        }
        if (to & 0x04) {  // EQ
            emit_.B_COND(Cond::EQ, 8);
            emit_.B(12);
            emit_.BRK(0x80);
        }
        // Unsigned comparisons handled similarly
    }
}

// ============================================
// CR Logical Operations
// ============================================

void PPCTranslator::EmitCRLogic(const ppc::DecodedInstr& instr) {
    // CR logical operations operate on individual bits
    // crand, cror, crxor, etc.
    
    // Extract bit positions from instruction
    uint8_t bt = static_cast<uint8_t>(instr.rD);  // Target bit
    uint8_t ba = static_cast<uint8_t>(instr.rA);  // Source bit A
    uint8_t bb = static_cast<uint8_t>(instr.rB);  // Source bit B
    
    // Extract CR bits to temp registers
    // CR bit N is at position (31-N) in the CR register
    
    // Get bit A
    emit_.LSR_IMM(REG_TMP1, REG_CR, 31 - ba);
    emit_.AND_IMM(REG_TMP1, REG_TMP1, 1);
    
    // Get bit B
    emit_.LSR_IMM(REG_TMP2, REG_CR, 31 - bb);
    emit_.AND_IMM(REG_TMP2, REG_TMP2, 1);
    
    // Perform operation
    switch (instr.type) {
    case ppc::InstrType::CRAND:
        emit_.AND(REG_TMP1, REG_TMP1, REG_TMP2);
        break;
    case ppc::InstrType::CROR:
        emit_.ORR(REG_TMP1, REG_TMP1, REG_TMP2);
        break;
    case ppc::InstrType::CRXOR:
        emit_.EOR(REG_TMP1, REG_TMP1, REG_TMP2);
        break;
    case ppc::InstrType::CRNAND:
        emit_.AND(REG_TMP1, REG_TMP1, REG_TMP2);
        emit_.MVN(REG_TMP1, REG_TMP1);
        emit_.AND_IMM(REG_TMP1, REG_TMP1, 1);
        break;
    case ppc::InstrType::CRNOR:
        emit_.ORR(REG_TMP1, REG_TMP1, REG_TMP2);
        emit_.MVN(REG_TMP1, REG_TMP1);
        emit_.AND_IMM(REG_TMP1, REG_TMP1, 1);
        break;
    case ppc::InstrType::CREQV:
        emit_.EOR(REG_TMP1, REG_TMP1, REG_TMP2);
        emit_.MVN(REG_TMP1, REG_TMP1);
        emit_.AND_IMM(REG_TMP1, REG_TMP1, 1);
        break;
    case ppc::InstrType::CRANDC:
        emit_.MVN(REG_TMP2, REG_TMP2);
        emit_.AND(REG_TMP1, REG_TMP1, REG_TMP2);
        emit_.AND_IMM(REG_TMP1, REG_TMP1, 1);
        break;
    case ppc::InstrType::CRORC:
        emit_.MVN(REG_TMP2, REG_TMP2);
        emit_.ORR(REG_TMP1, REG_TMP1, REG_TMP2);
        emit_.AND_IMM(REG_TMP1, REG_TMP1, 1);
        break;
    case ppc::InstrType::MCRF:
        // Move CR field
        // mcrf crfD, crfS - copy 4-bit field
        {
            uint8_t crfD = bt >> 2;
            uint8_t crfS = ba >> 2;
            
            // Extract source field
            emit_.LSR_IMM(REG_TMP1, REG_CR, 28 - crfS * 4);
            emit_.AND_IMM(REG_TMP1, REG_TMP1, 0xF);
            
            // Clear destination field and insert
            uint32_t mask = ~(0xF << (28 - crfD * 4));
            emit_.MOV_IMM64(REG_TMP2, mask);
            emit_.AND(REG_CR, REG_CR, REG_TMP2);
            emit_.LSL_IMM(REG_TMP1, REG_TMP1, 28 - crfD * 4);
            emit_.ORR(REG_CR, REG_CR, REG_TMP1);
        }
        return;
    default:
        break;
    }
    
    // Store result bit back to CR
    // Clear target bit
    uint32_t bit_mask = ~(1 << (31 - bt));
    emit_.MOV_IMM64(REG_TMP2, bit_mask);
    emit_.AND(REG_CR, REG_CR, REG_TMP2);
    
    // Set target bit if result is 1
    emit_.LSL_IMM(REG_TMP1, REG_TMP1, 31 - bt);
    emit_.ORR(REG_CR, REG_CR, REG_TMP1);
}

// ============================================
// Floating Point Load
// ============================================

void PPCTranslator::EmitFPLoad(const ppc::DecodedInstr& instr) {
    // lfs/lfd - Load Floating Single/Double
    GpReg ra = MapPPCGPR(instr.rA);
    int32_t offset = instr.simm;
    VecReg frd = static_cast<VecReg>(static_cast<uint8_t>(instr.rD));
    
    // Calculate effective address
    if (static_cast<uint8_t>(instr.rA) == 0) {
        emit_.MOV_IMM64(REG_TMP1, offset);
    } else if (offset != 0) {
        emit_.ADD_IMM(REG_TMP1, ra, offset);
    } else {
        emit_.MOV(REG_TMP1, ra);
    }
    
    // Load value (need to handle endianness)
    // For FP, we load to GPR, swap bytes, then move to FP reg
    emit_.LDR(REG_TMP2, REG_TMP1, 0);
    emit_.REV(REG_TMP2, REG_TMP2);  // Byte swap
    
    // Move to vector register
    // Using FMOV to move from GPR to FP
    // FMOV Dd, Xn = 0x9E670000 | (Xn << 5) | Dd
    emit_.Emit(0x9E670000 | (static_cast<uint32_t>(REG_TMP2) << 5) | 
               static_cast<uint32_t>(frd));
}

// ============================================
// Floating Point Store
// ============================================

void PPCTranslator::EmitFPStore(const ppc::DecodedInstr& instr) {
    // stfs/stfd - Store Floating Single/Double
    GpReg ra = MapPPCGPR(instr.rA);
    int32_t offset = instr.simm;
    VecReg frs = static_cast<VecReg>(static_cast<uint8_t>(instr.rD));
    
    // Calculate effective address
    if (static_cast<uint8_t>(instr.rA) == 0) {
        emit_.MOV_IMM64(REG_TMP1, offset);
    } else if (offset != 0) {
        emit_.ADD_IMM(REG_TMP1, ra, offset);
    } else {
        emit_.MOV(REG_TMP1, ra);
    }
    
    // Move from FP to GPR
    // FMOV Xd, Dn = 0x9E660000 | (Dn << 5) | Xd
    emit_.Emit(0x9E660000 | (static_cast<uint32_t>(frs) << 5) | 
               static_cast<uint32_t>(REG_TMP2));
    
    // Byte swap for big-endian
    emit_.REV(REG_TMP2, REG_TMP2);
    
    // Store
    emit_.STR(REG_TMP2, REG_TMP1, 0);
}

// ============================================
// Floating Point Arithmetic
// ============================================

void PPCTranslator::EmitFPArith(const ppc::DecodedInstr& instr) {
    // FP arithmetic instructions (fadd, fsub, fmul, fdiv, etc.)
    // The specific operation is encoded in the extended opcode
    
    VecReg frd = static_cast<VecReg>(static_cast<uint8_t>(instr.rD));
    VecReg fra = static_cast<VecReg>(static_cast<uint8_t>(instr.rA));
    VecReg frb = static_cast<VecReg>(static_cast<uint8_t>(instr.rB));
    
    uint16_t xo = instr.xo & 0x1F;  // Lower 5 bits for A-form instructions
    
    switch (xo) {
    case 18: // fdiv
        // FDIV Dd, Dn, Dm
        emit_.Emit(0x1E601800 | (static_cast<uint32_t>(frb) << 16) |
                   (static_cast<uint32_t>(fra) << 5) | static_cast<uint32_t>(frd));
        break;
    case 20: // fsub
        // FSUB Dd, Dn, Dm
        emit_.Emit(0x1E603800 | (static_cast<uint32_t>(frb) << 16) |
                   (static_cast<uint32_t>(fra) << 5) | static_cast<uint32_t>(frd));
        break;
    case 21: // fadd
        // FADD Dd, Dn, Dm
        emit_.Emit(0x1E602800 | (static_cast<uint32_t>(frb) << 16) |
                   (static_cast<uint32_t>(fra) << 5) | static_cast<uint32_t>(frd));
        break;
    case 25: // fmul
        // FMUL Dd, Dn, Dm
        // Note: PPC fmul uses FRC field (bits 21-25) for second operand
        {
            VecReg frc = static_cast<VecReg>((instr.rAw >> 6) & 0x1F);
            emit_.Emit(0x1E600800 | (static_cast<uint32_t>(frc) << 16) |
                       (static_cast<uint32_t>(fra) << 5) | static_cast<uint32_t>(frd));
        }
        break;
    case 22: // fsqrt
        // FSQRT Dd, Dn
        emit_.Emit(0x1E61C000 | (static_cast<uint32_t>(frb) << 5) | 
                   static_cast<uint32_t>(frd));
        break;
    default:
        // Unhandled FP operation
        emit_.NOP();
        LOGW("Unhandled FP operation xo=%d", xo);
        break;
    }
    
    // Update CR1 if Rc bit is set (not commonly used for FP)
    if (instr.rc) {
        // Copy FPSCR exception bits to CR1
        emit_.NOP();  // Simplified for now
    }
}

} // namespace rpcsx::nce::arm64
