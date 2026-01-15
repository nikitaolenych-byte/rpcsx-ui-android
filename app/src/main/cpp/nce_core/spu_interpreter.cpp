// ============================================================================
// SPU Interpreter Implementation
// ============================================================================
// Cell SPU → ARM NEON execution
// ============================================================================

#include "spu_interpreter.h"

#include <android/log.h>
#include <cstring>
#include <sys/mman.h>
#include "simd_utils.h"

#define LOG_TAG "SPU-Interpreter"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx {
namespace spu {

// ============================================================================
// SPU Interpreter
// ============================================================================
SPUInterpreter::SPUInterpreter() 
    : main_memory_(nullptr)
    , memory_size_(0)
    , spu_id_(0) {
}

SPUInterpreter::~SPUInterpreter() {
}

void SPUInterpreter::Initialize(uint32_t spu_id, void* main_memory, size_t memory_size) {
    spu_id_ = spu_id;
    main_memory_ = main_memory;
    memory_size_ = memory_size;
    LOGI("SPU %d interpreter initialized", spu_id);
}

void SPUInterpreter::Shutdown() {
    LOGI("SPU %d interpreter shutdown", spu_id_);
}

// ============================================================================
// Execution
// ============================================================================
void SPUInterpreter::Step(SPUState& state) {
    profiler_.Start("SPU_Step");
    // Fetch instruction from Local Store (Big Endian)
    uint32_t* ls = reinterpret_cast<uint32_t*>(state.local_store + state.pc);
    uint32_t inst_raw = __builtin_bswap32(*ls);
    SPUInstruction inst = { inst_raw };
    state.npc = state.pc + 4;
    ExecuteInstruction(state, inst);
    state.pc = state.npc;
    profiler_.Stop("SPU_Step");
}

void SPUInterpreter::Execute(SPUState& state, uint64_t count) {
    profiler_.Start("SPU_Execute");
    for (uint64_t i = 0; i < count && state.running && !state.stop; ++i) {
        Step(state);
    }
    profiler_.Stop("SPU_Execute");
}

void SPUInterpreter::Run(SPUState& state) {
    profiler_.Start("SPU_Run");
    state.running = true;
    state.stop = false;
    LOGI("SPU %d starting execution at PC=0x%08x", state.spu_id, state.pc);
    while (state.running && !state.stop && !state.halted) {
        Step(state);
        ProcessMFCQueue(state);  // Process DMA
    }
    LOGI("SPU %d stopped at PC=0x%08x", state.spu_id, state.pc);
    profiler_.Stop("SPU_Run");
}

void SPUInterpreter::RunInThread(SPUState& state) {
    Run(state);
}

// ============================================================================
// Instruction Dispatcher
// ============================================================================
void SPUInterpreter::ExecuteInstruction(SPUState& state, SPUInstruction inst) {
    uint32_t op = inst.opcode();
    
    // SPU має складну систему опкодів
    // Головні категорії: RRR, RR, RI7, RI10, RI16, RI18
    
    switch (op >> 4) {
        case 0x00: // Memory Load/Store
            switch (op & 0xF) {
                case 0x1: LQD(state, inst); break;
                case 0x3: STQD(state, inst); break;
                default: break;
            }
            break;
            
        case 0x04: // Arithmetic
            switch (op & 0xF) {
                case 0x0: A(state, inst); break;      // Add Word
                case 0x1: SF(state, inst); break;     // Subtract From
                case 0x8: AH(state, inst); break;     // Add Halfword  
                case 0x9: SFH(state, inst); break;    // Subtract From Halfword
                default: break;
            }
            break;
            
        case 0x08: // Logical
            switch (op & 0xF) {
                case 0x0: AND(state, inst); break;
                case 0x1: OR(state, inst); break;
                case 0x2: XOR(state, inst); break;
                case 0x3: NAND(state, inst); break;
                case 0x4: NOR(state, inst); break;
                case 0x9: ANDC(state, inst); break;
                case 0xA: ORC(state, inst); break;
                default: break;
            }
            break;
            
        case 0x0C: // Compare
            switch (op & 0xF) {
                case 0x0: CEQ(state, inst); break;    // Compare Equal
                case 0x1: CGT(state, inst); break;    // Compare Greater Than
                case 0x2: CLGT(state, inst); break;   // Compare Logical Greater Than
                default: break;
            }
            break;
            
        case 0x10: // Shift/Rotate
            switch (op & 0xF) {
                case 0x0: SHL(state, inst); break;    // Shift Left
                case 0x4: ROT(state, inst); break;    // Rotate
                case 0x8: ROTM(state, inst); break;   // Rotate and Mask
                case 0xC: ROTMA(state, inst); break;  // Rotate and Mask Algebraic
                default: break;
            }
            break;
            
        case 0x18: // Floating Point
            switch (op & 0xF) {
                case 0x0: FA(state, inst); break;     // Float Add
                case 0x1: FS(state, inst); break;     // Float Sub
                case 0x2: FM(state, inst); break;     // Float Mul
                default: break;
            }
            break;
            
        case 0x1C: // FMA
            switch (op & 0xF) {
                case 0x0: FMA(state, inst); break;    // Float Multiply-Add
                case 0x1: FMS(state, inst); break;    // Float Multiply-Sub
                case 0x2: FNMS(state, inst); break;   // Float Negative Multiply-Sub
                default: break;
            }
            break;
            
        case 0x20: // Branch
            switch (op & 0xF) {
                case 0x0: BR(state, inst); break;     // Branch Relative
                case 0x2: BRZ(state, inst); break;    // Branch if Zero
                case 0x3: BRNZ(state, inst); break;   // Branch if Not Zero
                case 0x4: BI(state, inst); break;     // Branch Indirect
                default: break;
            }
            break;
            
        case 0x30: // Select
            SELB(state, inst);
            break;
            
        case 0x38: // Shuffle
            SHUFB(state, inst);
            break;
            
        case 0x40: // Channel
            switch (op & 0xF) {
                case 0x0: RDCH(state, inst); break;   // Read Channel
                case 0x1: RCHCNT(state, inst); break; // Read Channel Count
                case 0x2: WRCH(state, inst); break;   // Write Channel
                default: break;
            }
            break;
            
        case 0x00:
            if (op == 0) {
                STOP(state, inst);
            } else if (op == 1) {
                NOP(state, inst);
            }
            break;
            
        default:
            // LOGE("SPU %d: Unknown opcode 0x%03x at PC=0x%08x", 
            //      state.spu_id, op, state.pc);
            break;
    }
}

// ============================================================================
// Memory Load/Store (128-bit Quadword)
// ============================================================================
void SPUInterpreter::LQD(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    int16_t i10 = inst.i10();
    
    uint32_t addr = (state.gpr[ra].u32[3] + (i10 << 4)) & 0x3FFF0;  // 256KB, 16-byte aligned
    
    // Load 128-bit from Local Store (Big Endian)
    uint8_t* src = state.local_store + addr;
    // SIMD copy for 16 bytes
    rpcsx::simd::memcpy_simd(state.gpr[rt].u8, src, 16);
    // Reverse for Big Endian
    for (int i = 0; i < 8; i++) {
        std::swap(state.gpr[rt].u8[i], state.gpr[rt].u8[15 - i]);
    }
}

void SPUInterpreter::LQX(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    uint32_t addr = (state.gpr[ra].u32[3] + state.gpr[rb].u32[3]) & 0x3FFF0;
    
    uint8_t* src = state.local_store + addr;
    rpcsx::simd::memcpy_simd(state.gpr[rt].u8, src, 16);
    for (int i = 0; i < 8; i++) {
        std::swap(state.gpr[rt].u8[i], state.gpr[rt].u8[15 - i]);
    }
}

void SPUInterpreter::STQD(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    int16_t i10 = inst.i10();
    
    uint32_t addr = (state.gpr[ra].u32[3] + (i10 << 4)) & 0x3FFF0;
    
    uint8_t* dst = state.local_store + addr;
    // Reverse for Big Endian
    uint8_t tmp[16];
    for (int i = 0; i < 16; i++) tmp[i] = state.gpr[rt].u8[15 - i];
    rpcsx::simd::memcpy_simd(dst, tmp, 16);
}

void SPUInterpreter::STQX(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    uint32_t addr = (state.gpr[ra].u32[3] + state.gpr[rb].u32[3]) & 0x3FFF0;
    
    uint8_t* dst = state.local_store + addr;
    uint8_t tmp[16];
    for (int i = 0; i < 16; i++) tmp[i] = state.gpr[rt].u8[15 - i];
    rpcsx::simd::memcpy_simd(dst, tmp, 16);
}

// ============================================================================
// Integer Arithmetic (SIMD - 4x32-bit words)
// ============================================================================
void SPUInterpreter::A(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    // NEON: 4x32-bit add
    state.gpr[rt].neon_u32 = vaddq_u32(state.gpr[ra].neon_u32, state.gpr[rb].neon_u32);
}

void SPUInterpreter::AI(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    int16_t i10 = inst.i10();
    
    uint32x4_t imm = vdupq_n_u32(static_cast<uint32_t>(i10));
    state.gpr[rt].neon_u32 = vaddq_u32(state.gpr[ra].neon_u32, imm);
}

void SPUInterpreter::SF(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    // RT = RB - RA (subtract from)
    state.gpr[rt].neon_u32 = vsubq_u32(state.gpr[rb].neon_u32, state.gpr[ra].neon_u32);
}

void SPUInterpreter::SFI(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    int16_t i10 = inst.i10();
    
    uint32x4_t imm = vdupq_n_u32(static_cast<uint32_t>(i10));
    state.gpr[rt].neon_u32 = vsubq_u32(imm, state.gpr[ra].neon_u32);
}

void SPUInterpreter::AH(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    // NEON: 8x16-bit add
    state.gpr[rt].neon_u16 = vaddq_u16(state.gpr[ra].neon_u16, state.gpr[rb].neon_u16);
}

void SPUInterpreter::SFH(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_u16 = vsubq_u16(state.gpr[rb].neon_u16, state.gpr[ra].neon_u16);
}

// ============================================================================
// Logical Operations (128-bit)
// ============================================================================
void SPUInterpreter::AND(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_u32 = vandq_u32(state.gpr[ra].neon_u32, state.gpr[rb].neon_u32);
}

void SPUInterpreter::ANDC(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_u32 = vbicq_u32(state.gpr[ra].neon_u32, state.gpr[rb].neon_u32);
}

void SPUInterpreter::OR(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_u32 = vorrq_u32(state.gpr[ra].neon_u32, state.gpr[rb].neon_u32);
}

void SPUInterpreter::ORC(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_u32 = vornq_u32(state.gpr[ra].neon_u32, state.gpr[rb].neon_u32);
}

void SPUInterpreter::XOR(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_u32 = veorq_u32(state.gpr[ra].neon_u32, state.gpr[rb].neon_u32);
}

void SPUInterpreter::NAND(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    uint32x4_t tmp = vandq_u32(state.gpr[ra].neon_u32, state.gpr[rb].neon_u32);
    state.gpr[rt].neon_u32 = vmvnq_u32(tmp);
}

void SPUInterpreter::NOR(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    uint32x4_t tmp = vorrq_u32(state.gpr[ra].neon_u32, state.gpr[rb].neon_u32);
    state.gpr[rt].neon_u32 = vmvnq_u32(tmp);
}

// ============================================================================
// Compare Operations
// ============================================================================
void SPUInterpreter::CEQ(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_u32 = vceqq_u32(state.gpr[ra].neon_u32, state.gpr[rb].neon_u32);
}

void SPUInterpreter::CGT(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_u32 = vcgtq_s32(state.gpr[ra].neon_i32, state.gpr[rb].neon_i32);
}

void SPUInterpreter::CLGT(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_u32 = vcgtq_u32(state.gpr[ra].neon_u32, state.gpr[rb].neon_u32);
}

// ============================================================================
// Shift/Rotate
// ============================================================================
void SPUInterpreter::SHL(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    // Shift each word by corresponding shift amount
    for (int i = 0; i < 4; i++) {
        uint32_t shift = state.gpr[rb].u32[i] & 0x3F;
        state.gpr[rt].u32[i] = (shift < 32) ? (state.gpr[ra].u32[i] << shift) : 0;
    }
}

void SPUInterpreter::SHLI(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t shift = inst.i10() & 0x3F;
    
    if (shift < 32) {
        state.gpr[rt].neon_u32 = vshlq_u32(state.gpr[ra].neon_u32, vdupq_n_s32(static_cast<int32_t>(shift)));
    } else {
        state.gpr[rt].neon_u32 = vdupq_n_u32(0);
    }
}

void SPUInterpreter::ROT(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    for (int i = 0; i < 4; i++) {
        uint32_t shift = state.gpr[rb].u32[i] & 0x1F;
        uint32_t val = state.gpr[ra].u32[i];
        state.gpr[rt].u32[i] = (val << shift) | (val >> (32 - shift));
    }
}

void SPUInterpreter::ROTM(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    for (int i = 0; i < 4; i++) {
        uint32_t shift = (0 - state.gpr[rb].u32[i]) & 0x3F;
        state.gpr[rt].u32[i] = (shift < 32) ? (state.gpr[ra].u32[i] >> shift) : 0;
    }
}

void SPUInterpreter::ROTMA(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    for (int i = 0; i < 4; i++) {
        int32_t shift = (0 - state.gpr[rb].i32[i]) & 0x3F;
        int32_t val = state.gpr[ra].i32[i];
        state.gpr[rt].i32[i] = (shift < 32) ? (val >> shift) : (val >> 31);
    }
}

// ============================================================================
// Floating Point (NEON accelerated)
// ============================================================================
void SPUInterpreter::FA(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_f32 = vaddq_f32(state.gpr[ra].neon_f32, state.gpr[rb].neon_f32);
}

void SPUInterpreter::FS(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_f32 = vsubq_f32(state.gpr[ra].neon_f32, state.gpr[rb].neon_f32);
}

void SPUInterpreter::FM(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    
    state.gpr[rt].neon_f32 = vmulq_f32(state.gpr[ra].neon_f32, state.gpr[rb].neon_f32);
}

void SPUInterpreter::FMA(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    uint32_t rc = inst.rc();
    
    // FMA: rt = ra * rb + rc
    state.gpr[rt].neon_f32 = vfmaq_f32(state.gpr[rc].neon_f32, 
                                        state.gpr[ra].neon_f32, 
                                        state.gpr[rb].neon_f32);
}

void SPUInterpreter::FMS(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    uint32_t rc = inst.rc();
    
    // FMS: rt = ra * rb - rc
    float32x4_t mul = vmulq_f32(state.gpr[ra].neon_f32, state.gpr[rb].neon_f32);
    state.gpr[rt].neon_f32 = vsubq_f32(mul, state.gpr[rc].neon_f32);
}

void SPUInterpreter::FNMS(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    uint32_t rc = inst.rc();
    
    // FNMS: rt = rc - ra * rb
    float32x4_t mul = vmulq_f32(state.gpr[ra].neon_f32, state.gpr[rb].neon_f32);
    state.gpr[rt].neon_f32 = vsubq_f32(state.gpr[rc].neon_f32, mul);
}

// ============================================================================
// Branch
// ============================================================================
void SPUInterpreter::BR(SPUState& state, SPUInstruction inst) {
    int32_t offset = static_cast<int16_t>(inst.i16()) << 2;
    state.npc = state.pc + offset;
}

void SPUInterpreter::BRA(SPUState& state, SPUInstruction inst) {
    state.npc = (inst.i16() << 2) & 0x3FFFC;
}

void SPUInterpreter::BRSL(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    int32_t offset = static_cast<int16_t>(inst.i16()) << 2;
    
    // Save return address
    state.gpr[rt].u32[3] = state.pc + 4;
    state.gpr[rt].u32[2] = 0;
    state.gpr[rt].u32[1] = 0;
    state.gpr[rt].u32[0] = 0;
    
    state.npc = state.pc + offset;
}

void SPUInterpreter::BI(SPUState& state, SPUInstruction inst) {
    uint32_t ra = inst.ra();
    state.npc = state.gpr[ra].u32[3] & 0x3FFFC;
}

void SPUInterpreter::BISL(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    
    state.gpr[rt].u32[3] = state.pc + 4;
    state.gpr[rt].u32[2] = 0;
    state.gpr[rt].u32[1] = 0;
    state.gpr[rt].u32[0] = 0;
    
    state.npc = state.gpr[ra].u32[3] & 0x3FFFC;
}

void SPUInterpreter::BRZ(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    int32_t offset = static_cast<int16_t>(inst.i16()) << 2;
    
    if (state.gpr[rt].u32[3] == 0) {
        state.npc = state.pc + offset;
    }
}

void SPUInterpreter::BRNZ(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    int32_t offset = static_cast<int16_t>(inst.i16()) << 2;
    
    if (state.gpr[rt].u32[3] != 0) {
        state.npc = state.pc + offset;
    }
}

// ============================================================================
// Select/Shuffle
// ============================================================================
void SPUInterpreter::SELB(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    uint32_t rc = inst.rc();
    
    // SELB: rt = (ra & ~rc) | (rb & rc)
    state.gpr[rt].neon_u32 = vbslq_u32(state.gpr[rc].neon_u32, 
                                        state.gpr[rb].neon_u32, 
                                        state.gpr[ra].neon_u32);
}

void SPUInterpreter::SHUFB(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ra = inst.ra();
    uint32_t rb = inst.rb();
    uint32_t rc = inst.rc();
    
    // Concatenate RA and RB for shuffle source
    uint8_t concat[32];
    memcpy(concat, state.gpr[ra].u8, 16);
    memcpy(concat + 16, state.gpr[rb].u8, 16);
    
    // Shuffle based on RC
    for (int i = 0; i < 16; i++) {
        uint8_t idx = state.gpr[rc].u8[i];
        if (idx >= 0x80) {
            if (idx >= 0xE0) state.gpr[rt].u8[i] = 0xFF;
            else if (idx >= 0xC0) state.gpr[rt].u8[i] = 0x80;
            else state.gpr[rt].u8[i] = 0x00;
        } else {
            state.gpr[rt].u8[i] = concat[idx & 0x1F];
        }
    }
}

// ============================================================================
// Channel Operations (SPU ↔ PPU communication)
// ============================================================================
void SPUInterpreter::RDCH(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ca = inst.ra() & 0x7F;  // Channel address
    
    // Simplified channel read
    uint32_t value = 0;
    switch (ca) {
        case 0: value = state.ch_snr[0]; break;
        case 1: value = state.ch_snr[1]; break;
        case 3: value = state.ch_dec; break;
        case 28: value = state.ch_event_stat; break;
        default: break;
    }
    
    state.gpr[rt].u32[3] = value;
    state.gpr[rt].u32[2] = 0;
    state.gpr[rt].u32[1] = 0;
    state.gpr[rt].u32[0] = 0;
}

void SPUInterpreter::RCHCNT(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ca = inst.ra() & 0x7F;
    
    // Return channel count (simplified)
    state.gpr[rt].u32[3] = 1;  // Always has data
    state.gpr[rt].u32[2] = 0;
    state.gpr[rt].u32[1] = 0;
    state.gpr[rt].u32[0] = 0;
}

void SPUInterpreter::WRCH(SPUState& state, SPUInstruction inst) {
    uint32_t rt = inst.rt();
    uint32_t ca = inst.ra() & 0x7F;
    uint32_t value = state.gpr[rt].u32[3];
    
    switch (ca) {
        case 16: state.ch_mfc_cmd = value; break;   // MFC Command
        case 17: state.ch_mfc_size = value; break;  // MFC Size
        case 18: state.ch_mfc_tag = value; break;   // MFC Tag
        case 21: // MFC LSA
        case 22: // MFC EAH
        case 23: // MFC EAL
            // Queue MFC command
            break;
        default: break;
    }
}

// ============================================================================
// Control
// ============================================================================
void SPUInterpreter::STOP(SPUState& state, SPUInstruction inst) {
    state.halted = true;
    LOGI("SPU %d: STOP signal 0x%04x", state.spu_id, inst.i16());
}

void SPUInterpreter::NOP(SPUState& state, SPUInstruction inst) {
    // No operation
}

void SPUInterpreter::LNOP(SPUState& state, SPUInstruction inst) {
    // No operation (load)
}

// ============================================================================
// MFC/DMA Processing
// ============================================================================
void SPUInterpreter::IssueMFCCommand(SPUState& state, uint32_t cmd, uint32_t lsa, 
                                     uint64_t ea, uint32_t size, uint32_t tag) {
    SPUState::MFCCommand mfc_cmd;
    mfc_cmd.lsa = lsa;
    mfc_cmd.ea = ea;
    mfc_cmd.size = size;
    mfc_cmd.tag = tag;
    mfc_cmd.cmd = cmd;
    mfc_cmd.active = true;
    
    state.mfc_queue.push_back(mfc_cmd);
}

void SPUInterpreter::ProcessMFCQueue(SPUState& state) {
    if (!main_memory_) return;
    
    for (auto& cmd : state.mfc_queue) {
        if (!cmd.active) continue;
        
        uint8_t* ls = state.local_store + (cmd.lsa & 0x3FFFF);
        uint8_t* ea = static_cast<uint8_t*>(main_memory_) + (cmd.ea & (memory_size_ - 1));
        
        switch (cmd.cmd & 0xFF) {
            case 0x20: // GET - DMA from main memory to LS
                rpcsx::simd::memcpy_simd(ls, ea, cmd.size);
                break;
            case 0x24: // PUT - DMA from LS to main memory
                rpcsx::simd::memcpy_simd(ea, ls, cmd.size);
                break;
        }
        
        cmd.active = false;
    }
    
    // Clear completed commands
    state.mfc_queue.erase(
        std::remove_if(state.mfc_queue.begin(), state.mfc_queue.end(),
                      [](const SPUState::MFCCommand& c) { return !c.active; }),
        state.mfc_queue.end());
}

// ============================================================================
// SPU Thread Group
// ============================================================================
SPUThreadGroup::SPUThreadGroup() 
    : main_memory_(nullptr)
    , memory_size_(0) {
}

SPUThreadGroup::~SPUThreadGroup() {
    Shutdown();
}

void SPUThreadGroup::Initialize(void* main_memory, size_t memory_size) {
    main_memory_ = main_memory;
    memory_size_ = memory_size;
    
    for (uint32_t i = 0; i < NUM_SPUS; i++) {
        // Allocate 256KB Local Store for each SPU
        states_[i].local_store = static_cast<uint8_t*>(
            mmap(nullptr, 256 * 1024, PROT_READ | PROT_WRITE, 
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        states_[i].ls_size = 256 * 1024;
        states_[i].spu_id = i;
        states_[i].running = false;
        states_[i].halted = false;
        states_[i].stop = false;
        states_[i].pc = 0;
        
        rpcsx::simd::memset_simd(states_[i].gpr, 0, sizeof(states_[i].gpr));
        
        interpreters_[i].Initialize(i, main_memory, memory_size);
    }
    
    LOGI("SPU Thread Group initialized: %d SPUs, 256KB LS each", NUM_SPUS);
}

void SPUThreadGroup::Shutdown() {
    StopAll();
    JoinAll();
    
    for (uint32_t i = 0; i < NUM_SPUS; i++) {
        if (states_[i].local_store) {
            munmap(states_[i].local_store, states_[i].ls_size);
            states_[i].local_store = nullptr;
        }
        interpreters_[i].Shutdown();
    }
}

void SPUThreadGroup::StartAll() {
    for (uint32_t i = 0; i < NUM_SPUS; i++) {
        StartSPU(i);
    }
}

void SPUThreadGroup::StopAll() {
    for (uint32_t i = 0; i < NUM_SPUS; i++) {
        StopSPU(i);
    }
}

void SPUThreadGroup::JoinAll() {
    for (uint32_t i = 0; i < NUM_SPUS; i++) {
        if (threads_[i].joinable()) {
            threads_[i].join();
        }
    }
}

void SPUThreadGroup::StartSPU(uint32_t spu_id) {
    if (spu_id >= NUM_SPUS) return;
    
    states_[spu_id].stop = false;
    states_[spu_id].halted = false;
    
    threads_[spu_id] = std::thread([this, spu_id]() {
        interpreters_[spu_id].Run(states_[spu_id]);
    });
    
    LOGI("SPU %d thread started", spu_id);
}

void SPUThreadGroup::StopSPU(uint32_t spu_id) {
    if (spu_id >= NUM_SPUS) return;
    
    states_[spu_id].stop = true;
}

void SPUThreadGroup::LoadProgram(uint32_t spu_id, const void* program, size_t size, uint32_t entry) {
    if (spu_id >= NUM_SPUS || !states_[spu_id].local_store) return;
    
    size_t copy_size = std::min(size, static_cast<size_t>(256 * 1024));
    rpcsx::simd::memcpy_simd(states_[spu_id].local_store, program, copy_size);
    states_[spu_id].pc = entry & 0x3FFFC;
    
    LOGI("SPU %d: Loaded %zu bytes, entry=0x%08x", spu_id, copy_size, entry);
}

SPUState* SPUThreadGroup::GetSPUState(uint32_t spu_id) {
    return (spu_id < NUM_SPUS) ? &states_[spu_id] : nullptr;
}

} // namespace spu
} // namespace rpcsx
