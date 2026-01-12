/**
 * PowerPC Decoder Implementation для PS3 Cell PPU
 * 
 * PS3 Cell PPU Architecture:
 * - PowerPC 64-bit with VMX (AltiVec) extensions
 * - Big-endian byte order
 * - Fixed 32-bit instruction width
 * - Two threads (PPE0, PPE1)
 */

#include "ppc_decoder.h"
#include <android/log.h>

#define LOG_TAG "RPCSX-PPC"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace rpcsx::nce::ppc {

// Читання big-endian 32-bit word (PS3 = big-endian)
static inline uint32_t ReadBE32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

// Витягнути біти [start:end] з інструкції (PPC bit numbering: MSB=0)
// PPC uses big-endian bit numbering: bit 0 is MSB
static inline uint32_t ExtractBits(uint32_t instr, int start, int end) {
    int shift = 31 - end;
    uint32_t mask = (1u << (end - start + 1)) - 1;
    return (instr >> shift) & mask;
}

// Sign extend from arbitrary bit width to 32-bit
static inline int32_t SignExtend(uint32_t val, int bits) {
    int shift = 32 - bits;
    return static_cast<int32_t>(val << shift) >> shift;
}

// Sign extend to 64-bit
static inline int64_t SignExtend64(uint64_t val, int bits) {
    int shift = 64 - bits;
    return static_cast<int64_t>(val << shift) >> shift;
}

// Extended opcode extraction for OP30 (64-bit rotate/shift)
enum class ExtOp30 : uint8_t {
    RLDICL = 0,   // Rotate Left Doubleword Immediate then Clear Left
    RLDICR = 1,   // Rotate Left Doubleword Immediate then Clear Right
    RLDIC = 2,    // Rotate Left Doubleword Immediate then Clear
    RLDIMI = 3,   // Rotate Left Doubleword Immediate then Mask Insert
    RLDCL = 8,    // Rotate Left Doubleword then Clear Left
    RLDCR = 9,    // Rotate Left Doubleword then Clear Right
};

// Extended opcodes for OP59 (single-precision FP)
enum class ExtOp59 : uint8_t {
    FDIVS = 18,
    FSUBS = 20,
    FADDS = 21,
    FSQRTS = 22,
    FRES = 24,
    FMULS = 25,
    FRSQRTES = 26,
    FMSUBS = 28,
    FMADDS = 29,
    FNMSUBS = 30,
    FNMADDS = 31,
};

// Extended opcodes for OP63 (double-precision FP)
enum class ExtOp63 : uint16_t {
    FCMPU = 0,
    FRSP = 12,
    FCTIW = 14,
    FCTIWZ = 15,
    FDIV = 18,
    FSUB = 20,
    FADD = 21,
    FSQRT = 22,
    FSEL = 23,
    FRE = 24,
    FMUL = 25,
    FRSQRTE = 26,
    FMSUB = 28,
    FMADD = 29,
    FNMSUB = 30,
    FNMADD = 31,
    FCMPO = 32,
    MTFSB1 = 38,
    FNEG = 40,
    MCRFS = 64,
    MTFSB0 = 70,
    FMR = 72,
    MTFSFI = 134,
    FNABS = 136,
    FABS = 264,
    MFFS = 583,
    MTFSF = 711,
    FCTID = 814,
    FCTIDZ = 815,
    FCFID = 846,
};

DecodedInstr DecodeInstruction(const uint8_t* code, uint64_t pc) {
    DecodedInstr instr = {};
    instr.raw = ReadBE32(code);
    instr.length = 4;
    
    uint32_t raw = instr.raw;
    
    // Primary opcode: bits 0-5
    instr.opcode = ExtractBits(raw, 0, 5);
    
    // Common fields
    instr.rd = static_cast<GPR>(ExtractBits(raw, 6, 10));
    instr.ra = static_cast<GPR>(ExtractBits(raw, 11, 15));
    instr.rb = static_cast<GPR>(ExtractBits(raw, 16, 20));
    instr.rc = (raw & 1) != 0;
    
    // D-form immediate (bits 16-31)
    instr.simm = static_cast<int16_t>(raw & 0xFFFF);
    instr.uimm = raw & 0xFFFF;
    
    PrimaryOp op = static_cast<PrimaryOp>(instr.opcode);
    
    switch (op) {
    // ================== Load/Store ==================
    case PrimaryOp::LWZ:
    case PrimaryOp::LWZU:
        instr.type = InstrType::LOAD_WORD;
        break;
        
    case PrimaryOp::LBZ:
    case PrimaryOp::LBZU:
        instr.type = InstrType::LOAD_BYTE;
        break;
        
    case PrimaryOp::LHZ:
    case PrimaryOp::LHZU:
    case PrimaryOp::LHA:
    case PrimaryOp::LHAU:
        instr.type = InstrType::LOAD_HALF;
        break;
        
    case PrimaryOp::STW:
    case PrimaryOp::STWU:
        instr.type = InstrType::STORE_WORD;
        break;
        
    case PrimaryOp::STB:
    case PrimaryOp::STBU:
        instr.type = InstrType::STORE_BYTE;
        break;
        
    case PrimaryOp::STH:
    case PrimaryOp::STHU:
        instr.type = InstrType::STORE_HALF;
        break;
        
    case PrimaryOp::LMW:
        instr.type = InstrType::LOAD_MULTIPLE;
        break;
        
    case PrimaryOp::STMW:
        instr.type = InstrType::STORE_MULTIPLE;
        break;
    
    // ================== 64-bit Load/Store ==================
    case PrimaryOp::OP58: {
        uint8_t ds = ExtractBits(raw, 30, 31);
        if (ds == 0) instr.type = InstrType::LOAD_DOUBLE;      // LD
        else if (ds == 1) instr.type = InstrType::LOAD_DOUBLE; // LDU
        else if (ds == 2) instr.type = InstrType::LOAD_WORD;   // LWA
        break;
    }
    
    case PrimaryOp::OP62: {
        uint8_t ds = ExtractBits(raw, 30, 31);
        if (ds == 0) instr.type = InstrType::STORE_DOUBLE;     // STD
        else if (ds == 1) instr.type = InstrType::STORE_DOUBLE; // STDU
        break;
    }
    
    // ================== Arithmetic Immediate ==================
    case PrimaryOp::ADDI:
        instr.type = InstrType::ADDI;
        break;
        
    case PrimaryOp::ADDIS:
        instr.type = InstrType::ADDIS;
        break;
        
    case PrimaryOp::ADDIC:
    case PrimaryOp::ADDIC_RC:
        instr.type = InstrType::ADDIC;
        break;
        
    case PrimaryOp::SUBFIC:
        instr.type = InstrType::SUBFIC;
        break;
        
    case PrimaryOp::MULLI:
        instr.type = InstrType::MULLI;
        break;
    
    // ================== Logic Immediate ==================
    case PrimaryOp::ORI:
        instr.type = InstrType::ORI;
        break;
        
    case PrimaryOp::ORIS:
        instr.type = InstrType::ORIS;
        break;
        
    case PrimaryOp::XORI:
        instr.type = InstrType::XORI;
        break;
        
    case PrimaryOp::XORIS:
        instr.type = InstrType::XORIS;
        break;
        
    case PrimaryOp::ANDI_RC:
        instr.type = InstrType::ANDI;
        instr.rc = true;
        break;
        
    case PrimaryOp::ANDIS_RC:
        instr.type = InstrType::ANDIS;
        instr.rc = true;
        break;
    
    // ================== Compare Immediate ==================
    case PrimaryOp::CMPI:
        instr.type = InstrType::CMPI;
        break;
        
    case PrimaryOp::CMPLI:
        instr.type = InstrType::CMPLI;
        break;
    
    // ================== Branch ==================
    case PrimaryOp::B: {
        // I-form: bits 6-29 = LI, bit 30 = AA, bit 31 = LK
        instr.li = SignExtend(ExtractBits(raw, 6, 29) << 2, 26);
        instr.aa = (raw & 2) != 0;
        instr.lk = (raw & 1) != 0;
        instr.type = instr.lk ? InstrType::BL : InstrType::B;
        break;
    }
    
    case PrimaryOp::BC: {
        // B-form: bits 6-10 = BO, bits 11-15 = BI, bits 16-29 = BD
        instr.bo = ExtractBits(raw, 6, 10);
        instr.bi = ExtractBits(raw, 11, 15);
        instr.li = SignExtend(ExtractBits(raw, 16, 29) << 2, 16);
        instr.aa = (raw & 2) != 0;
        instr.lk = (raw & 1) != 0;
        instr.type = InstrType::BC;
        break;
    }
    
    case PrimaryOp::SC:
        instr.type = InstrType::SC;
        break;
    
    // ================== Rotate/Shift ==================
    case PrimaryOp::RLWIMI:
        instr.type = InstrType::RLWIMI;
        instr.sh = ExtractBits(raw, 16, 20);
        instr.mb = ExtractBits(raw, 21, 25);
        instr.me = ExtractBits(raw, 26, 30);
        break;
        
    case PrimaryOp::RLWINM:
        instr.type = InstrType::RLWINM;
        instr.sh = ExtractBits(raw, 16, 20);
        instr.mb = ExtractBits(raw, 21, 25);
        instr.me = ExtractBits(raw, 26, 30);
        break;
        
    case PrimaryOp::RLWNM:
        instr.type = InstrType::RLWNM;
        instr.mb = ExtractBits(raw, 21, 25);
        instr.me = ExtractBits(raw, 26, 30);
        break;
    
    // ================== Extended Opcode 19 ==================
    case PrimaryOp::OP19: {
        instr.xo = ExtractBits(raw, 21, 30);
        ExtOp19 xop = static_cast<ExtOp19>(instr.xo);
        
        switch (xop) {
        case ExtOp19::BCLR:
            instr.bo = ExtractBits(raw, 6, 10);
            instr.bi = ExtractBits(raw, 11, 15);
            instr.lk = (raw & 1) != 0;
            instr.type = InstrType::BCLR;
            break;
            
        case ExtOp19::BCCTR:
            instr.bo = ExtractBits(raw, 6, 10);
            instr.bi = ExtractBits(raw, 11, 15);
            instr.lk = (raw & 1) != 0;
            instr.type = InstrType::BCCTR;
            break;
            
        case ExtOp19::MCRF:
            instr.type = InstrType::MCRF;
            break;
            
        case ExtOp19::CRAND:
            instr.type = InstrType::CRAND;
            break;
            
        case ExtOp19::CROR:
            instr.type = InstrType::CROR;
            break;
            
        case ExtOp19::CRXOR:
            instr.type = InstrType::CRXOR;
            break;
            
        case ExtOp19::CRNAND:
            instr.type = InstrType::CRNAND;
            break;
            
        case ExtOp19::CRNOR:
            instr.type = InstrType::CRNOR;
            break;
            
        case ExtOp19::CREQV:
            instr.type = InstrType::CREQV;
            break;
            
        case ExtOp19::CRANDC:
            instr.type = InstrType::CRANDC;
            break;
            
        case ExtOp19::CRORC:
            instr.type = InstrType::CRORC;
            break;
            
        case ExtOp19::RFI:
            instr.type = InstrType::RFI;
            break;
            
        case ExtOp19::ISYNC:
            instr.type = InstrType::ISYNC;
            break;
            
        default:
            instr.type = InstrType::UNKNOWN;
            break;
        }
        break;
    }
    
    // ================== Extended Opcode 31 ==================
    case PrimaryOp::OP31: {
        instr.xo = ExtractBits(raw, 21, 30);
        instr.oe = (raw & (1 << 10)) != 0;
        ExtOp31 xop = static_cast<ExtOp31>(instr.xo & 0x3FF);
        
        switch (xop) {
        // Arithmetic
        case ExtOp31::ADD:
            instr.type = InstrType::ADD;
            break;
        case ExtOp31::ADDC:
            instr.type = InstrType::ADDC;
            break;
        case ExtOp31::ADDE:
            instr.type = InstrType::ADD;
            break;
        case ExtOp31::ADDZE:
            instr.type = InstrType::ADD;
            break;
        case ExtOp31::ADDME:
            instr.type = InstrType::ADD;
            break;
        case ExtOp31::SUBF:
            instr.type = InstrType::SUBF;
            break;
        case ExtOp31::SUBFC:
            instr.type = InstrType::SUB;
            break;
        case ExtOp31::SUBFE:
            instr.type = InstrType::SUB;
            break;
        case ExtOp31::SUBFZE:
            instr.type = InstrType::SUB;
            break;
        case ExtOp31::SUBFME:
            instr.type = InstrType::SUB;
            break;
        case ExtOp31::MULLW:
            instr.type = InstrType::MULLW;
            break;
        case ExtOp31::MULHW:
        case ExtOp31::MULHWU:
            instr.type = InstrType::MULLW;
            break;
        case ExtOp31::DIVW:
            instr.type = InstrType::DIVW;
            break;
        case ExtOp31::DIVWU:
            instr.type = InstrType::DIVWU;
            break;
        case ExtOp31::NEG:
            instr.type = InstrType::NEG;
            break;
            
        // Logic
        case ExtOp31::AND:
            instr.type = InstrType::AND;
            break;
        case ExtOp31::ANDC:
            instr.type = InstrType::ANDC;
            break;
        case ExtOp31::OR:
            instr.type = InstrType::OR;
            break;
        case ExtOp31::ORC:
            instr.type = InstrType::ORC;
            break;
        case ExtOp31::XOR:
            instr.type = InstrType::XOR;
            break;
        case ExtOp31::NAND:
            instr.type = InstrType::NAND;
            break;
        case ExtOp31::NOR:
            instr.type = InstrType::NOR;
            break;
        case ExtOp31::EQV:
            instr.type = InstrType::EQV;
            break;
            
        // Shift
        case ExtOp31::SLW:
            instr.type = InstrType::SLW;
            break;
        case ExtOp31::SRW:
            instr.type = InstrType::SRW;
            break;
        case ExtOp31::SRAW:
            instr.type = InstrType::SRAW;
            break;
        case ExtOp31::SRAWI:
            instr.type = InstrType::SRAWI;
            instr.sh = ExtractBits(raw, 16, 20);
            break;
            
        // Compare
        case ExtOp31::CMP:
            instr.type = InstrType::CMP;
            break;
        case ExtOp31::CMPL:
            instr.type = InstrType::CMPL;
            break;
            
        // Load indexed
        case ExtOp31::LWZX:
        case ExtOp31::LWZUX:
        case ExtOp31::LWAX:
            instr.type = InstrType::LOAD_WORD;
            break;
        case ExtOp31::LBZX:
        case ExtOp31::LBZUX:
            instr.type = InstrType::LOAD_BYTE;
            break;
        case ExtOp31::LHZX:
        case ExtOp31::LHZUX:
        case ExtOp31::LHAX:
        case ExtOp31::LHAUX:
            instr.type = InstrType::LOAD_HALF;
            break;
        case ExtOp31::LDX:
        case ExtOp31::LDUX:
            instr.type = InstrType::LOAD_DOUBLE;
            break;
            
        // Store indexed
        case ExtOp31::STWX:
        case ExtOp31::STWUX:
            instr.type = InstrType::STORE_WORD;
            break;
        case ExtOp31::STBX:
        case ExtOp31::STBUX:
            instr.type = InstrType::STORE_BYTE;
            break;
        case ExtOp31::STHX:
        case ExtOp31::STHUX:
            instr.type = InstrType::STORE_HALF;
            break;
        case ExtOp31::STDX:
        case ExtOp31::STDUX:
            instr.type = InstrType::STORE_DOUBLE;
            break;
            
        // Special registers
        case ExtOp31::MFSPR:
            instr.type = InstrType::MFSPR;
            instr.spr = (ExtractBits(raw, 16, 20) << 5) | ExtractBits(raw, 11, 15);
            break;
        case ExtOp31::MTSPR:
            instr.type = InstrType::MTSPR;
            instr.spr = (ExtractBits(raw, 16, 20) << 5) | ExtractBits(raw, 11, 15);
            break;
        case ExtOp31::MFCR:
            instr.type = InstrType::MFCR;
            break;
        case ExtOp31::MTCRF:
            instr.type = InstrType::MTCRF;
            break;
            
        // Extend
        case ExtOp31::EXTSB:
            instr.type = InstrType::EXTSB;
            break;
        case ExtOp31::EXTSH:
            instr.type = InstrType::EXTSH;
            break;
        case ExtOp31::EXTSW:
            instr.type = InstrType::EXTSW;
            break;
        case ExtOp31::CNTLZW:
            instr.type = InstrType::CNTLZW;
            break;
            
        // Cache
        case ExtOp31::DCBF:
            instr.type = InstrType::DCBF;
            break;
        case ExtOp31::DCBI:
            instr.type = InstrType::DCBI;
            break;
        case ExtOp31::DCBST:
            instr.type = InstrType::DCBST;
            break;
        case ExtOp31::DCBT:
            instr.type = InstrType::DCBT;
            break;
        case ExtOp31::DCBTST:
            instr.type = InstrType::DCBTST;
            break;
        case ExtOp31::DCBZ:
            instr.type = InstrType::DCBZ;
            break;
        case ExtOp31::ICBI:
            instr.type = InstrType::ICBI;
            break;
            
        // Sync
        case ExtOp31::EIEIO:
            instr.type = InstrType::EIEIO;
            break;
            
        // Trap
        case ExtOp31::TW:
            instr.type = InstrType::TW;
            break;
            
        // Atomic
        case ExtOp31::LWARX:
            instr.type = InstrType::LOAD_WORD;
            break;
        case ExtOp31::STWCX:
            instr.type = InstrType::STORE_WORD;
            break;
            
        default:
            instr.type = InstrType::UNKNOWN;
            break;
        }
        break;
    }
    
    // ================== Trap Immediate ==================
    case PrimaryOp::TWI:
        instr.type = InstrType::TWI;
        break;
    
    // ================== Float Load/Store ==================
    case PrimaryOp::LFS:
    case PrimaryOp::LFSU:
    case PrimaryOp::LFD:
    case PrimaryOp::LFDU:
        instr.type = InstrType::FP_LOAD;
        break;
        
    case PrimaryOp::STFS:
    case PrimaryOp::STFSU:
    case PrimaryOp::STFD:
    case PrimaryOp::STFDU:
        instr.type = InstrType::FP_STORE;
        break;
    
    // ================== Float Arithmetic ==================
    case PrimaryOp::OP59:
    case PrimaryOp::OP63:
        instr.type = InstrType::FP_ARITH;
        break;
    
    default:
        instr.type = InstrType::UNKNOWN;
        break;
    }
    
    return instr;
}

const char* GetInstructionName(InstrType type) {
    switch (type) {
    case InstrType::LOAD_WORD: return "lwz/lwzx";
    case InstrType::LOAD_BYTE: return "lbz/lbzx";
    case InstrType::LOAD_HALF: return "lhz/lhzx";
    case InstrType::LOAD_DOUBLE: return "ld/ldx";
    case InstrType::STORE_WORD: return "stw/stwx";
    case InstrType::STORE_BYTE: return "stb/stbx";
    case InstrType::STORE_HALF: return "sth/sthx";
    case InstrType::STORE_DOUBLE: return "std/stdx";
    case InstrType::LOAD_MULTIPLE: return "lmw";
    case InstrType::STORE_MULTIPLE: return "stmw";
    case InstrType::ADD: return "add";
    case InstrType::ADDI: return "addi";
    case InstrType::ADDIS: return "addis";
    case InstrType::ADDC: return "addc";
    case InstrType::ADDIC: return "addic";
    case InstrType::SUB: return "subf";
    case InstrType::SUBF: return "subf";
    case InstrType::SUBFIC: return "subfic";
    case InstrType::MULLI: return "mulli";
    case InstrType::MULLW: return "mullw";
    case InstrType::DIVW: return "divw";
    case InstrType::DIVWU: return "divwu";
    case InstrType::NEG: return "neg";
    case InstrType::AND: return "and";
    case InstrType::ANDI: return "andi.";
    case InstrType::ANDIS: return "andis.";
    case InstrType::OR: return "or";
    case InstrType::ORI: return "ori";
    case InstrType::ORIS: return "oris";
    case InstrType::XOR: return "xor";
    case InstrType::XORI: return "xori";
    case InstrType::XORIS: return "xoris";
    case InstrType::NAND: return "nand";
    case InstrType::NOR: return "nor";
    case InstrType::EQV: return "eqv";
    case InstrType::ANDC: return "andc";
    case InstrType::ORC: return "orc";
    case InstrType::SLW: return "slw";
    case InstrType::SRW: return "srw";
    case InstrType::SRAW: return "sraw";
    case InstrType::SRAWI: return "srawi";
    case InstrType::RLWINM: return "rlwinm";
    case InstrType::RLWIMI: return "rlwimi";
    case InstrType::RLWNM: return "rlwnm";
    case InstrType::CMP: return "cmp";
    case InstrType::CMPI: return "cmpi";
    case InstrType::CMPL: return "cmpl";
    case InstrType::CMPLI: return "cmpli";
    case InstrType::B: return "b";
    case InstrType::BL: return "bl";
    case InstrType::BC: return "bc";
    case InstrType::BCLR: return "bclr";
    case InstrType::BCCTR: return "bcctr";
    case InstrType::MFSPR: return "mfspr";
    case InstrType::MTSPR: return "mtspr";
    case InstrType::MFCR: return "mfcr";
    case InstrType::MTCRF: return "mtcrf";
    case InstrType::EXTSB: return "extsb";
    case InstrType::EXTSH: return "extsh";
    case InstrType::EXTSW: return "extsw";
    case InstrType::CNTLZW: return "cntlzw";
    case InstrType::SC: return "sc";
    case InstrType::RFI: return "rfi";
    case InstrType::ISYNC: return "isync";
    case InstrType::EIEIO: return "eieio";
    case InstrType::SYNC: return "sync";
    case InstrType::TW: return "tw";
    case InstrType::TWI: return "twi";
    case InstrType::FP_LOAD: return "lfs/lfd";
    case InstrType::FP_STORE: return "stfs/stfd";
    case InstrType::FP_ARITH: return "fp_arith";
    case InstrType::DCBF: return "dcbf";
    case InstrType::DCBI: return "dcbi";
    case InstrType::DCBST: return "dcbst";
    case InstrType::DCBT: return "dcbt";
    case InstrType::DCBTST: return "dcbtst";
    case InstrType::DCBZ: return "dcbz";
    case InstrType::ICBI: return "icbi";
    default: return "unknown";
    }
}

} // namespace rpcsx::nce::ppc
