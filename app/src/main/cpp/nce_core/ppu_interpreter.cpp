// PPU Interpreter Implementation
// Повна реалізація PowerPC Cell PPU інструкцій для PS3

#include "ppu_interpreter.h"
#include <android/log.h>
#include <cmath>
#include <bit>
#include <algorithm>

#define LOG_TAG "PPU-Interpreter"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx {
namespace ppu {

// Byte Swap Helpers (PS3 is Big Endian, ARM is Little Endian)
static inline uint16_t bswap16(uint16_t x) {
    return __builtin_bswap16(x);
}

static inline uint32_t bswap32(uint32_t x) {
    return __builtin_bswap32(x);
}

static inline uint64_t bswap64(uint64_t x) {
    return __builtin_bswap64(x);
}

// Constructor
PPUInterpreter::PPUInterpreter() 
    : memory_base_(nullptr)
    , memory_size_(0) {
}

void PPUInterpreter::Initialize(void* memory_base, size_t memory_size) {
    memory_base_ = memory_base;
    memory_size_ = memory_size;
    LOGI("PPU Interpreter initialized with %zu MB memory", memory_size / (1024 * 1024));
}

// Memory Access (Big Endian)
uint8_t PPUInterpreter::ReadMemory8(PPUState& state, uint64_t addr) {
    if (addr >= memory_size_) {
        LOGE("Memory read out of bounds: 0x%llx", (unsigned long long)addr);
        return 0;
    }
    return static_cast<uint8_t*>(memory_base_)[addr];
}

uint16_t PPUInterpreter::ReadMemory16(PPUState& state, uint64_t addr) {
    if (addr + 1 >= memory_size_) {
        LOGE("Memory read out of bounds: 0x%llx", (unsigned long long)addr);
        return 0;
    }
    uint16_t value = *reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(memory_base_) + addr);
    return bswap16(value);  // PS3 is big endian
}

uint32_t PPUInterpreter::ReadMemory32(PPUState& state, uint64_t addr) {
    if (addr + 3 >= memory_size_) {
        LOGE("Memory read out of bounds: 0x%llx", (unsigned long long)addr);
        return 0;
    }
    uint32_t value = *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(memory_base_) + addr);
    return bswap32(value);
}

uint64_t PPUInterpreter::ReadMemory64(PPUState& state, uint64_t addr) {
    if (addr + 7 >= memory_size_) {
        LOGE("Memory read out of bounds: 0x%llx", (unsigned long long)addr);
        return 0;
    }
    uint64_t value = *reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(memory_base_) + addr);
    return bswap64(value);
}

void PPUInterpreter::WriteMemory8(PPUState& state, uint64_t addr, uint8_t value) {
    if (addr >= memory_size_) {
        LOGE("Memory write out of bounds: 0x%llx", (unsigned long long)addr);
        return;
    }
    static_cast<uint8_t*>(memory_base_)[addr] = value;
}

void PPUInterpreter::WriteMemory16(PPUState& state, uint64_t addr, uint16_t value) {
    if (addr + 1 >= memory_size_) {
        LOGE("Memory write out of bounds: 0x%llx", (unsigned long long)addr);
        return;
    }
    *reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(memory_base_) + addr) = bswap16(value);
}

void PPUInterpreter::WriteMemory32(PPUState& state, uint64_t addr, uint32_t value) {
    if (addr + 3 >= memory_size_) {
        LOGE("Memory write out of bounds: 0x%llx", (unsigned long long)addr);
        return;
    }
    *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(memory_base_) + addr) = bswap32(value);
}

void PPUInterpreter::WriteMemory64(PPUState& state, uint64_t addr, uint64_t value) {
    if (addr + 7 >= memory_size_) {
        LOGE("Memory write out of bounds: 0x%llx", (unsigned long long)addr);
        return;
    }
    *reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(memory_base_) + addr) = bswap64(value);
}

// Execution
void PPUInterpreter::Step(PPUState& state) {
    profiler_.Start("PPU_Step");
    uint32_t inst_raw = ReadMemory32(state, state.pc);
    PPUInstruction inst = { inst_raw };
    state.npc = state.pc + 4;
    ExecuteInstruction(state, inst);
    state.pc = state.npc;
    profiler_.Stop("PPU_Step");
}

void PPUInterpreter::Execute(PPUState& state, uint64_t count) {
    profiler_.Start("PPU_Execute");
    for (uint64_t i = 0; i < count && state.running; ++i) {
        Step(state);
    }
    profiler_.Stop("PPU_Execute");
}

void PPUInterpreter::Run(PPUState& state) {
    profiler_.Start("PPU_Run");
    state.running = true;
    while (state.running && !state.halted) {
        Step(state);
    }
    profiler_.Stop("PPU_Run");
}

// Instruction Execution Dispatcher
void PPUInterpreter::ExecuteInstruction(PPUState& state, PPUInstruction inst) {
    uint32_t op = inst.opcode();
    
    switch (op) {
        // Branch Instructions
        case 18: B(state, inst); break;
        case 16: BC(state, inst); break;
        case 19: 
            switch (inst.xo_x()) {
                case 16: BCLR(state, inst); break;
                case 528: BCCTR(state, inst); break;
                case 0: MCRF(state, inst); break;
                case 33: CRNOR(state, inst); break;
                case 129: CRANDC(state, inst); break;
                case 193: CRXOR(state, inst); break;
                case 225: CRNAND(state, inst); break;
                case 257: CRAND(state, inst); break;
                case 289: CREQV(state, inst); break;
                case 417: CRORC(state, inst); break;
                case 449: CROR(state, inst); break;
                     /* ISYNC belongs to primary opcode 19 (ext group),
                         STWCX uses xo=150 in this group. Removed duplicate. */
                default: LOGE("Unknown opcode 19/%d", inst.xo_x()); break;
            }
            break;
            
        // Integer Load/Store
        case 34: LBZ(state, inst); break;
        case 35: LBZU(state, inst); break;
        case 40: LHZ(state, inst); break;
        case 41: LHZU(state, inst); break;
        case 42: LHA(state, inst); break;
        case 43: LHAU(state, inst); break;
        case 32: LWZ(state, inst); break;
        case 33: LWZU(state, inst); break;
        case 58: LD(state, inst); break;
        case 38: STB(state, inst); break;
        case 39: STBU(state, inst); break;
        case 44: STH(state, inst); break;
        case 45: STHU(state, inst); break;
        case 36: STW(state, inst); break;
        case 37: STWU(state, inst); break;
        case 62: STD(state, inst); break;
        
        // Integer Arithmetic
        case 14: ADDI(state, inst); break;
        case 15: ADDIS(state, inst); break;
        case 7: MULLI(state, inst); break;
        case 8: SUBFIC(state, inst); break;
        
        case 31:
            switch (inst.xo_xo()) {
                // Arithmetic
                case 266: ADD(state, inst); break;
                case 10: ADDC(state, inst); break;
                case 138: ADDE(state, inst); break;
                case 234: ADDME(state, inst); break;
                case 202: ADDZE(state, inst); break;
                case 40: SUBF(state, inst); break;
                case 8: SUBFC(state, inst); break;
                case 136: SUBFE(state, inst); break;
                case 232: SUBFME(state, inst); break;
                case 200: SUBFZE(state, inst); break;
                case 104: NEG(state, inst); break;
                case 235: MULLW(state, inst); break;
                case 75: MULHW(state, inst); break;
                case 11: MULHWU(state, inst); break;
                case 233: MULLD(state, inst); break;
                case 73: MULHD(state, inst); break;
                case 9: MULHDU(state, inst); break;
                case 491: DIVW(state, inst); break;
                case 459: DIVWU(state, inst); break;
                case 489: DIVD(state, inst); break;
                case 457: DIVDU(state, inst); break;
                
                // Compare
                case 0: CMP(state, inst); break;
                case 32: CMPL(state, inst); break;
                
                // Logical
                case 28: AND(state, inst); break;
                case 60: ANDC(state, inst); break;
                case 444: OR(state, inst); break;
                case 412: ORC(state, inst); break;
                case 316: XOR(state, inst); break;
                case 476: NAND(state, inst); break;
                case 124: NOR(state, inst); break;
                case 284: EQV(state, inst); break;
                case 954: EXTSB(state, inst); break;
                case 922: EXTSH(state, inst); break;
                case 986: EXTSW(state, inst); break;
                case 26: CNTLZW(state, inst); break;
                case 58: CNTLZD(state, inst); break;
                case 122: POPCNTB(state, inst); break;
                
                // Shift/Rotate
                case 24: SLW(state, inst); break;
                case 536: SRW(state, inst); break;
                case 792: SRAW(state, inst); break;
                case 824: SRAWI(state, inst); break;
                case 27: SLD(state, inst); break;
                case 539: SRD(state, inst); break;
                case 794: SRAD(state, inst); break;
                case 413: SRADI(state, inst); break;
                
                // Load/Store Indexed
                case 87: LBZX(state, inst); break;
                case 119: LBZUX(state, inst); break;
                case 279: LHZX(state, inst); break;
                case 311: LHZUX(state, inst); break;
                case 343: LHAX(state, inst); break;
                case 375: LHAUX(state, inst); break;
                case 23: LWZX(state, inst); break;
                case 55: LWZUX(state, inst); break;
                case 341: LWAX(state, inst); break;
                case 373: LWAUX(state, inst); break;
                case 21: LDX(state, inst); break;
                case 53: LDUX(state, inst); break;
                case 215: STBX(state, inst); break;
                case 247: STBUX(state, inst); break;
                case 407: STHX(state, inst); break;
                case 439: STHUX(state, inst); break;
                case 151: STWX(state, inst); break;
                case 183: STWUX(state, inst); break;
                case 149: STDX(state, inst); break;
                case 181: STDUX(state, inst); break;
                
                // Special Purpose Registers
                case 339: MFSPR(state, inst); break;
                case 467: MTSPR(state, inst); break;
                case 19: MFCR(state, inst); break;
                case 144: MTCRF(state, inst); break;
                
                // System
                case 598: SYNC(state, inst); break;
                case 854: EIEIO(state, inst); break;
                case 86: DCBF(state, inst); break;
                case 278: DCBT(state, inst); break;
                case 246: DCBTST(state, inst); break;
                case 1014: DCBZ(state, inst); break;
                case 982: ICBI(state, inst); break;
                
                // Load/Store with Reservation
                case 20: LWARX(state, inst); break;
                case 84: LDARX(state, inst); break;
                case 150: STWCX(state, inst); break;
                case 214: STDCX(state, inst); break;
                
                default: LOGE("Unknown opcode 31/%d", inst.xo_xo()); break;
            }
            break;
            
        // Integer Compare Immediate
        case 11: CMPI(state, inst); break;
        case 10: CMPLI(state, inst); break;
        
        // Integer Logical Immediate
        case 28: ANDI(state, inst); break;
        case 29: ANDIS(state, inst); break;
        case 24: ORI(state, inst); break;
        case 25: ORIS(state, inst); break;
        case 26: XORI(state, inst); break;
        case 27: XORIS(state, inst); break;
        
        // Integer Rotate/Shift
        case 21: RLWINM(state, inst); break;
        case 23: RLWNM(state, inst); break;
        case 20: RLWIMI(state, inst); break;
        case 30:
            switch ((inst.raw >> 2) & 7) {
                case 0: RLDICL(state, inst); break;
                case 1: RLDICR(state, inst); break;
                case 2: RLDIC(state, inst); break;
                case 3: RLDIMI(state, inst); break;
                case 4: RLDCL(state, inst); break;
                case 5: RLDCR(state, inst); break;
                default: LOGE("Unknown opcode 30/%d", (inst.raw >> 2) & 7); break;
            }
            break;
            
        // Floating Point Load/Store
        case 48: LFS(state, inst); break;
        case 49: LFSU(state, inst); break;
        case 50: LFD(state, inst); break;
        case 51: LFDU(state, inst); break;
        case 52: STFS(state, inst); break;
        case 53: STFSU(state, inst); break;
        case 54: STFD(state, inst); break;
        case 55: STFDU(state, inst); break;
        
        // Duplicate FP indexed handlers removed (handled in opcode 31 above)
            
        // Floating Point Arithmetic
        case 63:
        case 59:
            switch (inst.xo_x() & 0x1F) {
                case 18: FDIV(state, inst); break;
                case 20: FSUB(state, inst); break;
                case 21: FADD(state, inst); break;
                case 22: FSQRT(state, inst); break;
                case 24: FRES(state, inst); break;
                case 25: FMUL(state, inst); break;
                case 26: FRSQRTE(state, inst); break;
                case 28: FMSUB(state, inst); break;
                case 29: FMADD(state, inst); break;
                case 30: FNMSUB(state, inst); break;
                case 31: FNMADD(state, inst); break;
            }
            
            if (op == 63) {
                switch (inst.xo_x()) {
                    case 0: FCMPU(state, inst); break;
                    case 12: FRSP(state, inst); break;
                    case 14: FCTIW(state, inst); break;
                    case 15: FCTIWZ(state, inst); break;
                    case 32: FCMPO(state, inst); break;
                    case 38: MTFSB1(state, inst); break;
                    case 40: FNEG(state, inst); break;
                    case 64: MCRF(state, inst); break;
                    case 70: MTFSB0(state, inst); break;
                    case 72: FMR(state, inst); break;
                    case 134: MTFSFI(state, inst); break;
                    case 136: FNABS(state, inst); break;
                    case 264: FABS(state, inst); break;
                    case 583: MFFS(state, inst); break;
                    case 711: MTFSF(state, inst); break;
                    case 814: FCTID(state, inst); break;
                    case 815: FCTIDZ(state, inst); break;
                    case 846: FCFID(state, inst); break;
                }
            }
            break;
            
        // VMX/AltiVec (Vector) Instructions
        case 4:
            // VMX instructions use opcode 4 with extended opcode
            // Implementation in separate handlers
            LOGE("VMX instruction not yet implemented: 0x%08x", inst.raw);
            break;
            
        // System Call
        case 17: SC(state, inst); break;
        
        // Trap
        case 3: TWI(state, inst); break;
        case 2: TDI(state, inst); break;
        // Duplicate trap handlers for opcode 31 removed
            
        default:
            LOGE("Unknown opcode: %d (inst=0x%08x at PC=0x%llx)", 
                 op, inst.raw, (unsigned long long)state.pc);
            state.halted = true;
            break;
    }
}

// Branch Instructions Implementation
void PPUInterpreter::B(PPUState& state, PPUInstruction inst) {
    int64_t offset = inst.li();
    if (inst.aa()) {
        state.npc = offset;  // Absolute
    } else {
        state.npc = state.pc + offset;  // Relative
    }
    if (inst.lk()) {
        state.lr = state.pc + 4;  // Save return address
    }
}

void PPUInterpreter::BC(PPUState& state, PPUInstruction inst) {
    uint32_t bo = inst.bo();
    uint32_t bi = inst.bi();
    int16_t bd = inst.bd();
    
    bool ctr_ok = (bo & 4) || ((--state.ctr != 0) ^ ((bo & 2) != 0));
    bool cond_ok = (bo & 16) || (GetCRBit(state, bi) == ((bo & 8) != 0));
    
    if (ctr_ok && cond_ok) {
        if (inst.aa()) {
            state.npc = bd;
        } else {
            state.npc = state.pc + bd;
        }
    }
    
    if (inst.lk()) {
        state.lr = state.pc + 4;
    }
}

void PPUInterpreter::BCLR(PPUState& state, PPUInstruction inst) {
    uint32_t bo = inst.bo();
    uint32_t bi = inst.bi();
    
    bool ctr_ok = (bo & 4) || ((--state.ctr != 0) ^ ((bo & 2) != 0));
    bool cond_ok = (bo & 16) || (GetCRBit(state, bi) == ((bo & 8) != 0));
    
    if (ctr_ok && cond_ok) {
        state.npc = state.lr & ~3ULL;
    }
    
    if (inst.lk()) {
        state.lr = state.pc + 4;
    }
}

void PPUInterpreter::BCCTR(PPUState& state, PPUInstruction inst) {
    uint32_t bo = inst.bo();
    uint32_t bi = inst.bi();
    
    bool cond_ok = (bo & 16) || (GetCRBit(state, bi) == ((bo & 8) != 0));
    
    if (cond_ok) {
        state.npc = state.ctr & ~3ULL;
    }
    
    if (inst.lk()) {
        state.lr = state.pc + 4;
    }
}

// Load/Store Instructions
void PPUInterpreter::LBZ(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += inst.simm16();
    state.gpr[inst.rd()] = ReadMemory8(state, addr);
}

void PPUInterpreter::LBZU(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + inst.simm16();
    state.gpr[inst.rd()] = ReadMemory8(state, addr);
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::LBZX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += state.gpr[inst.rb()];
    state.gpr[inst.rd()] = ReadMemory8(state, addr);
}

void PPUInterpreter::LHZ(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += inst.simm16();
    state.gpr[inst.rd()] = ReadMemory16(state, addr);
}

void PPUInterpreter::LHZU(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + inst.simm16();
    state.gpr[inst.rd()] = ReadMemory16(state, addr);
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::LHZX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += state.gpr[inst.rb()];
    state.gpr[inst.rd()] = ReadMemory16(state, addr);
}

void PPUInterpreter::LHA(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += inst.simm16();
    state.gpr[inst.rd()] = static_cast<int16_t>(ReadMemory16(state, addr));
}

void PPUInterpreter::LHAU(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + inst.simm16();
    state.gpr[inst.rd()] = static_cast<int16_t>(ReadMemory16(state, addr));
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::LHAX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += state.gpr[inst.rb()];
    state.gpr[inst.rd()] = static_cast<int16_t>(ReadMemory16(state, addr));
}

void PPUInterpreter::LWZ(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += inst.simm16();
    state.gpr[inst.rd()] = ReadMemory32(state, addr);
}

void PPUInterpreter::LWZU(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + inst.simm16();
    state.gpr[inst.rd()] = ReadMemory32(state, addr);
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::LWZX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += state.gpr[inst.rb()];
    state.gpr[inst.rd()] = ReadMemory32(state, addr);
}

void PPUInterpreter::LWA(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += (inst.raw & 0xFFFC);  // ds field
    state.gpr[inst.rd()] = static_cast<int32_t>(ReadMemory32(state, addr));
}

void PPUInterpreter::LWAX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += state.gpr[inst.rb()];
    state.gpr[inst.rd()] = static_cast<int32_t>(ReadMemory32(state, addr));
}

void PPUInterpreter::LD(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += (inst.raw & 0xFFFC);  // ds field
    state.gpr[inst.rd()] = ReadMemory64(state, addr);
}

void PPUInterpreter::LDU(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + (inst.raw & 0xFFFC);
    state.gpr[inst.rd()] = ReadMemory64(state, addr);
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::LDX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += state.gpr[inst.rb()];
    state.gpr[inst.rd()] = ReadMemory64(state, addr);
}

// UX variants: Indexed with Update (ra = ra + rb)
void PPUInterpreter::LBZUX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + state.gpr[inst.rb()];
    state.gpr[inst.rd()] = ReadMemory8(state, addr);
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::LHZUX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + state.gpr[inst.rb()];
    state.gpr[inst.rd()] = ReadMemory16(state, addr);
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::LHAUX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + state.gpr[inst.rb()];
    state.gpr[inst.rd()] = static_cast<int16_t>(ReadMemory16(state, addr));
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::LWZUX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + state.gpr[inst.rb()];
    state.gpr[inst.rd()] = ReadMemory32(state, addr);
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::LWAUX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + state.gpr[inst.rb()];
    state.gpr[inst.rd()] = static_cast<int32_t>(ReadMemory32(state, addr));
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::LDUX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + state.gpr[inst.rb()];
    state.gpr[inst.rd()] = ReadMemory64(state, addr);
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::STBUX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + state.gpr[inst.rb()];
    WriteMemory8(state, addr, static_cast<uint8_t>(state.gpr[inst.rd()] & 0xFF));
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::STHUX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + state.gpr[inst.rb()];
    WriteMemory16(state, addr, static_cast<uint16_t>(state.gpr[inst.rd()] & 0xFFFF));
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::STWUX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + state.gpr[inst.rb()];
    WriteMemory32(state, addr, static_cast<uint32_t>(state.gpr[inst.rd()] & 0xFFFFFFFF));
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::STDUX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + state.gpr[inst.rb()];
    WriteMemory64(state, addr, state.gpr[inst.rd()]);
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::STB(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += inst.simm16();
    WriteMemory8(state, addr, static_cast<uint8_t>(state.gpr[inst.rd()]));
}

void PPUInterpreter::STBU(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + inst.simm16();
    WriteMemory8(state, addr, static_cast<uint8_t>(state.gpr[inst.rd()]));
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::STBX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += state.gpr[inst.rb()];
    WriteMemory8(state, addr, static_cast<uint8_t>(state.gpr[inst.rd()]));
}

void PPUInterpreter::STH(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += inst.simm16();
    WriteMemory16(state, addr, static_cast<uint16_t>(state.gpr[inst.rd()]));
}

void PPUInterpreter::STHU(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + inst.simm16();
    WriteMemory16(state, addr, static_cast<uint16_t>(state.gpr[inst.rd()]));
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::STHX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += state.gpr[inst.rb()];
    WriteMemory16(state, addr, static_cast<uint16_t>(state.gpr[inst.rd()]));
}

void PPUInterpreter::STW(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += inst.simm16();
    WriteMemory32(state, addr, static_cast<uint32_t>(state.gpr[inst.rd()]));
}

void PPUInterpreter::STWU(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + inst.simm16();
    WriteMemory32(state, addr, static_cast<uint32_t>(state.gpr[inst.rd()]));
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::STWX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += state.gpr[inst.rb()];
    WriteMemory32(state, addr, static_cast<uint32_t>(state.gpr[inst.rd()]));
}

void PPUInterpreter::STD(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += (inst.raw & 0xFFFC);
    WriteMemory64(state, addr, state.gpr[inst.rd()]);
}

void PPUInterpreter::STDU(PPUState& state, PPUInstruction inst) {
    uint64_t addr = state.gpr[inst.ra()] + (inst.raw & 0xFFFC);
    WriteMemory64(state, addr, state.gpr[inst.rd()]);
    state.gpr[inst.ra()] = addr;
}

void PPUInterpreter::STDX(PPUState& state, PPUInstruction inst) {
    uint64_t addr = inst.ra() ? state.gpr[inst.ra()] : 0;
    addr += state.gpr[inst.rb()];
    WriteMemory64(state, addr, state.gpr[inst.rd()]);
}

// Integer Arithmetic Instructions
void PPUInterpreter::ADDI(PPUState& state, PPUInstruction inst) {
    uint64_t a = inst.ra() ? state.gpr[inst.ra()] : 0;
    state.gpr[inst.rd()] = a + inst.simm16();
}

void PPUInterpreter::ADDIS(PPUState& state, PPUInstruction inst) {
    uint64_t a = inst.ra() ? state.gpr[inst.ra()] : 0;
    state.gpr[inst.rd()] = a + (static_cast<int64_t>(inst.simm16()) << 16);
}

void PPUInterpreter::ADD(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.rd()] = state.gpr[inst.ra()] + state.gpr[inst.rb()];
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.rd()]);
}

void PPUInterpreter::ADDC(PPUState& state, PPUInstruction inst) {
    uint64_t a = state.gpr[inst.ra()];
    uint64_t b = state.gpr[inst.rb()];
    state.gpr[inst.rd()] = a + b;
    
    // Set CA if carry occurred
    if (state.gpr[inst.rd()] < a) {
        state.xer |= XER_CA;
    } else {
        state.xer &= ~XER_CA;
    }
    
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.rd()]);
}

void PPUInterpreter::ADDE(PPUState& state, PPUInstruction inst) {
    uint64_t a = state.gpr[inst.ra()];
    uint64_t b = state.gpr[inst.rb()];
    uint64_t carry = (state.xer & XER_CA) ? 1 : 0;
    
    state.gpr[inst.rd()] = a + b + carry;
    
    // Calculate new carry
    bool new_carry = (state.gpr[inst.rd()] < a) || 
                     (carry && state.gpr[inst.rd()] == a);
    if (new_carry) {
        state.xer |= XER_CA;
    } else {
        state.xer &= ~XER_CA;
    }
    
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.rd()]);
}

void PPUInterpreter::SUBF(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.rd()] = state.gpr[inst.rb()] - state.gpr[inst.ra()];
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.rd()]);
}

void PPUInterpreter::SUBFIC(PPUState& state, PPUInstruction inst) {
    uint64_t a = state.gpr[inst.ra()];
    int64_t imm = inst.simm16();
    state.gpr[inst.rd()] = imm - a;
    
    // Set carry
    if (imm >= static_cast<int64_t>(a)) {
        state.xer |= XER_CA;
    } else {
        state.xer &= ~XER_CA;
    }
}

void PPUInterpreter::NEG(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.rd()] = -state.gpr[inst.ra()];
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.rd()]);
}

void PPUInterpreter::MULLI(PPUState& state, PPUInstruction inst) {
    int64_t a = state.gpr[inst.ra()];
    int64_t imm = inst.simm16();
    state.gpr[inst.rd()] = a * imm;
}

void PPUInterpreter::MULLW(PPUState& state, PPUInstruction inst) {
    int32_t a = state.gpr[inst.ra()];
    int32_t b = state.gpr[inst.rb()];
    state.gpr[inst.rd()] = static_cast<int64_t>(a) * static_cast<int64_t>(b);
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.rd()]);
}

void PPUInterpreter::MULLD(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.rd()] = state.gpr[inst.ra()] * state.gpr[inst.rb()];
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.rd()]);
}

void PPUInterpreter::DIVW(PPUState& state, PPUInstruction inst) {
    int32_t a = state.gpr[inst.ra()];
    int32_t b = state.gpr[inst.rb()];
    if (b == 0) {
        state.gpr[inst.rd()] = 0;
    } else {
        state.gpr[inst.rd()] = static_cast<uint64_t>(a / b);
    }
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.rd()]);
}

void PPUInterpreter::DIVWU(PPUState& state, PPUInstruction inst) {
    uint32_t a = state.gpr[inst.ra()];
    uint32_t b = state.gpr[inst.rb()];
    if (b == 0) {
        state.gpr[inst.rd()] = 0;
    } else {
        state.gpr[inst.rd()] = a / b;
    }
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.rd()]);
}

void PPUInterpreter::DIVD(PPUState& state, PPUInstruction inst) {
    int64_t a = state.gpr[inst.ra()];
    int64_t b = state.gpr[inst.rb()];
    if (b == 0) {
        state.gpr[inst.rd()] = 0;
    } else {
        state.gpr[inst.rd()] = a / b;
    }
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.rd()]);
}

void PPUInterpreter::DIVDU(PPUState& state, PPUInstruction inst) {
    uint64_t a = state.gpr[inst.ra()];
    uint64_t b = state.gpr[inst.rb()];
    if (b == 0) {
        state.gpr[inst.rd()] = 0;
    } else {
        state.gpr[inst.rd()] = a / b;
    }
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.rd()]);
}

// Integer Compare Instructions
void PPUInterpreter::CMPI(PPUState& state, PPUInstruction inst) {
    int crfd = inst.crfd();
    int64_t a = state.gpr[inst.ra()];
    int64_t b = inst.simm16();
    UpdateCRn(state, crfd, a, b);
}

void PPUInterpreter::CMP(PPUState& state, PPUInstruction inst) {
    int crfd = inst.crfd();
    int64_t a = state.gpr[inst.ra()];
    int64_t b = state.gpr[inst.rb()];
    UpdateCRn(state, crfd, a, b);
}

void PPUInterpreter::CMPLI(PPUState& state, PPUInstruction inst) {
    int crfd = inst.crfd();
    uint64_t a = state.gpr[inst.ra()];
    uint64_t b = inst.uimm16();
    UpdateCRnU(state, crfd, a, b);
}

void PPUInterpreter::CMPL(PPUState& state, PPUInstruction inst) {
    int crfd = inst.crfd();
    uint64_t a = state.gpr[inst.ra()];
    uint64_t b = state.gpr[inst.rb()];
    UpdateCRnU(state, crfd, a, b);
}

// Integer Logical Instructions
void PPUInterpreter::ANDI(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.ra()] = state.gpr[inst.rd()] & inst.uimm16();
    UpdateCR0(state, state.gpr[inst.ra()]);
}

void PPUInterpreter::ANDIS(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.ra()] = state.gpr[inst.rd()] & (static_cast<uint64_t>(inst.uimm16()) << 16);
    UpdateCR0(state, state.gpr[inst.ra()]);
}

void PPUInterpreter::ORI(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.ra()] = state.gpr[inst.rd()] | inst.uimm16();
}

void PPUInterpreter::ORIS(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.ra()] = state.gpr[inst.rd()] | (static_cast<uint64_t>(inst.uimm16()) << 16);
}

void PPUInterpreter::XORI(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.ra()] = state.gpr[inst.rd()] ^ inst.uimm16();
}

void PPUInterpreter::XORIS(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.ra()] = state.gpr[inst.rd()] ^ (static_cast<uint64_t>(inst.uimm16()) << 16);
}

void PPUInterpreter::AND(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.ra()] = state.gpr[inst.rd()] & state.gpr[inst.rb()];
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.ra()]);
}

void PPUInterpreter::OR(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.ra()] = state.gpr[inst.rd()] | state.gpr[inst.rb()];
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.ra()]);
}

void PPUInterpreter::XOR(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.ra()] = state.gpr[inst.rd()] ^ state.gpr[inst.rb()];
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.ra()]);
}

void PPUInterpreter::NAND(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.ra()] = ~(state.gpr[inst.rd()] & state.gpr[inst.rb()]);
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.ra()]);
}

void PPUInterpreter::NOR(PPUState& state, PPUInstruction inst) {
    state.gpr[inst.ra()] = ~(state.gpr[inst.rd()] | state.gpr[inst.rb()]);
    if (inst.rc_bit()) UpdateCR0(state, state.gpr[inst.ra()]);
}

// System Call
void PPUInterpreter::SC(PPUState& state, PPUInstruction inst) {
    // System call - trigger syscall handler
    state.interrupt_pending |= 1;  // Mark syscall interrupt
    LOGI("Syscall at PC=0x%llx, R11=%llu", 
         (unsigned long long)state.pc, 
         (unsigned long long)state.gpr[11]);
}

// Helper Functions
void PPUInterpreter::UpdateCR0(PPUState& state, int64_t value) {
    uint32_t cr = 0;
    if (value < 0) cr |= CR_LT;
    else if (value > 0) cr |= CR_GT;
    else cr |= CR_EQ;
    
    if (state.xer & XER_SO) cr |= CR_SO;
    
    state.cr = (state.cr & 0x0FFFFFFF) | (cr << 28);
}

void PPUInterpreter::UpdateCRn(PPUState& state, int n, int64_t a, int64_t b) {
    uint32_t cr = 0;
    if (a < b) cr |= CR_LT;
    else if (a > b) cr |= CR_GT;
    else cr |= CR_EQ;
    
    if (state.xer & XER_SO) cr |= CR_SO;
    
    int shift = 28 - (n * 4);
    state.cr = (state.cr & ~(0xF << shift)) | (cr << shift);
}

void PPUInterpreter::UpdateCRnU(PPUState& state, int n, uint64_t a, uint64_t b) {
    uint32_t cr = 0;
    if (a < b) cr |= CR_LT;
    else if (a > b) cr |= CR_GT;
    else cr |= CR_EQ;
    
    if (state.xer & XER_SO) cr |= CR_SO;
    
    int shift = 28 - (n * 4);
    state.cr = (state.cr & ~(0xF << shift)) | (cr << shift);
}

bool PPUInterpreter::GetCRBit(const PPUState& state, int bit) {
    return (state.cr >> (31 - bit)) & 1;
}

void PPUInterpreter::SetCRBit(PPUState& state, int bit, bool value) {
    if (value) {
        state.cr |= (1U << (31 - bit));
    } else {
        state.cr &= ~(1U << (31 - bit));
    }
}

// Stub implementations for remaining instructions
// TODO: Implement full instruction set

#define STUB_IMPL(name) \
    void PPUInterpreter::name(PPUState& state, PPUInstruction inst) { \
        LOGE("Unimplemented: " #name); \
    }

// Generate stubs for all remaining instructions
STUB_IMPL(ADDME) STUB_IMPL(ADDZE) STUB_IMPL(SUBFC) STUB_IMPL(SUBFE)
STUB_IMPL(SUBFME) STUB_IMPL(SUBFZE) STUB_IMPL(MULHW) STUB_IMPL(MULHWU)
STUB_IMPL(MULHD) STUB_IMPL(MULHDU) STUB_IMPL(ANDC) STUB_IMPL(ORC)
STUB_IMPL(EQV) STUB_IMPL(EXTSB) STUB_IMPL(EXTSH) STUB_IMPL(EXTSW)
STUB_IMPL(CNTLZW) STUB_IMPL(CNTLZD) STUB_IMPL(POPCNTB)
STUB_IMPL(RLWINM) STUB_IMPL(RLWNM) STUB_IMPL(RLWIMI)
STUB_IMPL(RLDICL) STUB_IMPL(RLDICR) STUB_IMPL(RLDIC) STUB_IMPL(RLDIMI)
STUB_IMPL(RLDCL) STUB_IMPL(RLDCR)
STUB_IMPL(SLW) STUB_IMPL(SRW) STUB_IMPL(SRAW) STUB_IMPL(SRAWI)
STUB_IMPL(SLD) STUB_IMPL(SRD) STUB_IMPL(SRAD) STUB_IMPL(SRADI)
STUB_IMPL(MTSPR) STUB_IMPL(MFSPR) STUB_IMPL(MTCRF) STUB_IMPL(MFCR)
STUB_IMPL(MFOCRF) STUB_IMPL(MTOCRF) STUB_IMPL(MCRXR)
STUB_IMPL(LFS) STUB_IMPL(LFSU) STUB_IMPL(LFSX)
STUB_IMPL(LFD) STUB_IMPL(LFDU) STUB_IMPL(LFDX)
STUB_IMPL(STFS) STUB_IMPL(STFSU) STUB_IMPL(STFSX)
STUB_IMPL(STFD) STUB_IMPL(STFDU) STUB_IMPL(STFDX)
STUB_IMPL(FADD) STUB_IMPL(FADDS) STUB_IMPL(FSUB) STUB_IMPL(FSUBS)
STUB_IMPL(FMUL) STUB_IMPL(FMULS) STUB_IMPL(FDIV) STUB_IMPL(FDIVS)
STUB_IMPL(FSQRT) STUB_IMPL(FSQRTS) STUB_IMPL(FRES) STUB_IMPL(FRSQRTE)
STUB_IMPL(FMADD) STUB_IMPL(FMADDS) STUB_IMPL(FMSUB) STUB_IMPL(FMSUBS)
STUB_IMPL(FNMADD) STUB_IMPL(FNMADDS) STUB_IMPL(FNMSUB) STUB_IMPL(FNMSUBS)
STUB_IMPL(FSEL) STUB_IMPL(FCMPU) STUB_IMPL(FCMPO)
STUB_IMPL(FCTID) STUB_IMPL(FCTIDZ) STUB_IMPL(FCTIW) STUB_IMPL(FCTIWZ)
STUB_IMPL(FCFID) STUB_IMPL(FMR) STUB_IMPL(FNEG) STUB_IMPL(FABS)
STUB_IMPL(FNABS) STUB_IMPL(FRSP) STUB_IMPL(MFFS) STUB_IMPL(MTFSF)
STUB_IMPL(MTFSFI) STUB_IMPL(MTFSB0) STUB_IMPL(MTFSB1)
STUB_IMPL(TW) STUB_IMPL(TD) STUB_IMPL(TWI) STUB_IMPL(TDI)
STUB_IMPL(SYNC) STUB_IMPL(LWSYNC) STUB_IMPL(EIEIO) STUB_IMPL(ISYNC)
STUB_IMPL(DCBF) STUB_IMPL(DCBI) STUB_IMPL(DCBST) STUB_IMPL(DCBT)
STUB_IMPL(DCBTST) STUB_IMPL(DCBZ) STUB_IMPL(ICBI)
STUB_IMPL(LWARX) STUB_IMPL(LDARX) STUB_IMPL(STWCX) STUB_IMPL(STDCX)
STUB_IMPL(CRAND) STUB_IMPL(CROR) STUB_IMPL(CRXOR) STUB_IMPL(CRNAND)
STUB_IMPL(CRNOR) STUB_IMPL(CREQV) STUB_IMPL(CRANDC) STUB_IMPL(CRORC)
STUB_IMPL(MCRF)

// VMX stubs
STUB_IMPL(LVX) STUB_IMPL(LVXL) STUB_IMPL(LVEBX) STUB_IMPL(LVEHX)
STUB_IMPL(LVEWX) STUB_IMPL(LVSL) STUB_IMPL(LVSR)
STUB_IMPL(STVX) STUB_IMPL(STVXL) STUB_IMPL(STVEBX) STUB_IMPL(STVEHX)
STUB_IMPL(STVEWX)
STUB_IMPL(VADDFP) STUB_IMPL(VSUBFP) STUB_IMPL(VMADDFP) STUB_IMPL(VNMSUBFP)
STUB_IMPL(VMAXFP) STUB_IMPL(VMINFP) STUB_IMPL(VREFP) STUB_IMPL(VRSQRTEFP)
STUB_IMPL(VLOGEFP) STUB_IMPL(VEXPTEFP)
STUB_IMPL(VADDUBM) STUB_IMPL(VADDUHM) STUB_IMPL(VADDUWM)
STUB_IMPL(VSUBUBM) STUB_IMPL(VSUBUHM) STUB_IMPL(VSUBUWM)
STUB_IMPL(VMULESH) STUB_IMPL(VMULEUH) STUB_IMPL(VMULOSH) STUB_IMPL(VMULOUH)
STUB_IMPL(VAND) STUB_IMPL(VANDC) STUB_IMPL(VOR) STUB_IMPL(VORC)
STUB_IMPL(VXOR) STUB_IMPL(VNOR)
STUB_IMPL(VSLB) STUB_IMPL(VSLH) STUB_IMPL(VSLW)
STUB_IMPL(VSRB) STUB_IMPL(VSRH) STUB_IMPL(VSRW)
STUB_IMPL(VSRAB) STUB_IMPL(VSRAH) STUB_IMPL(VSRAW)
STUB_IMPL(VSL) STUB_IMPL(VSR) STUB_IMPL(VSLDOI)
STUB_IMPL(VPERM) STUB_IMPL(VSEL)
STUB_IMPL(VSPLTB) STUB_IMPL(VSPLTH) STUB_IMPL(VSPLTW)
STUB_IMPL(VSPLTISB) STUB_IMPL(VSPLTISH) STUB_IMPL(VSPLTISW)
STUB_IMPL(VMRGHB) STUB_IMPL(VMRGHH) STUB_IMPL(VMRGHW)
STUB_IMPL(VMRGLB) STUB_IMPL(VMRGLH) STUB_IMPL(VMRGLW)
STUB_IMPL(VPKUHUM) STUB_IMPL(VPKUWUM)
STUB_IMPL(VUPKHSB) STUB_IMPL(VUPKHSH) STUB_IMPL(VUPKLSB) STUB_IMPL(VUPKLSH)
STUB_IMPL(VCMPEQUB) STUB_IMPL(VCMPEQUH) STUB_IMPL(VCMPEQUW)
STUB_IMPL(VCMPGTSB) STUB_IMPL(VCMPGTSH) STUB_IMPL(VCMPGTSW)
STUB_IMPL(VCMPGTUB) STUB_IMPL(VCMPGTUH) STUB_IMPL(VCMPGTUW)
STUB_IMPL(VCMPEQFP) STUB_IMPL(VCMPGEFP) STUB_IMPL(VCMPGTFP) STUB_IMPL(VCMPBFP)
STUB_IMPL(VCTSXS) STUB_IMPL(VCTUXS) STUB_IMPL(VCFSX) STUB_IMPL(VCFUX)
STUB_IMPL(VRFIN) STUB_IMPL(VRFIZ) STUB_IMPL(VRFIP) STUB_IMPL(VRFIM)

} // namespace ppu
} // namespace rpcsx
