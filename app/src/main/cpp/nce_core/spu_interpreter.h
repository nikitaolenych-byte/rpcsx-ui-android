// ============================================================================
// SPU Interpreter - Cell Synergistic Processing Unit
// ============================================================================
// PS3 має 6 SPU процесорів для SIMD обчислень
// Кожен SPU має 256KB Local Store + 128 регістрів по 128 біт
// ============================================================================

#pragma once

#include <cstdint>
#include <thread>
#include <atomic>
#include <vector>
#include <arm_neon.h>  // ARM NEON для 128-bit SIMD

namespace rpcsx {
namespace spu {

// ============================================================================
// SPU Register State
// ============================================================================
struct SPUState {
    // 128 General Purpose Registers (128-bit кожен)
    union SPUReg {
        uint8_t  u8[16];
        uint16_t u16[8];
        uint32_t u32[4];
        uint64_t u64[2];
        int8_t   i8[16];
        int16_t  i16[8];
        int32_t  i32[4];
        int64_t  i64[2];
        float    f32[4];
        double   f64[2];
        
        // NEON types для швидкості
        uint8x16_t  neon_u8;
        uint16x8_t  neon_u16;
        uint32x4_t  neon_u32;
        uint64x2_t  neon_u64;
        int8x16_t   neon_i8;
        int16x8_t   neon_i16;
        int32x4_t   neon_i32;
        int64x2_t   neon_i64;
        float32x4_t neon_f32;
    } gpr[128];
    
    // Program Counter
    uint32_t pc;
    uint32_t npc;
    
    // Channel Registers
    uint32_t ch_mfc_cmd;      // MFC command queue
    uint32_t ch_mfc_size;     // MFC transfer size
    uint32_t ch_mfc_tag;      // MFC tag
    uint32_t ch_snr[2];       // Signal notification registers
    uint32_t ch_event_mask;   // Event mask
    uint32_t ch_event_stat;   // Event status
    uint32_t ch_stall_stat;   // Stall status
    uint32_t ch_dec;          // Decrementer
    
    // Local Store Address (256KB per SPU)
    uint8_t* local_store;
    uint32_t ls_size;
    
    // DMA/MFC state
    struct MFCCommand {
        uint32_t lsa;      // Local Store Address
        uint64_t ea;       // Effective Address (main memory)
        uint32_t size;     // Transfer size
        uint16_t tag;      // Tag ID
        uint16_t cmd;      // Command type
        bool active;
    };
    std::vector<MFCCommand> mfc_queue;
    
    // Execution state
    bool running;
    bool halted;
    std::atomic<bool> stop;
    uint32_t spu_id;  // 0-5 for 6 SPUs
};

// ============================================================================
// SPU Instruction Format
// ============================================================================
struct SPUInstruction {
    uint32_t raw;
    
    // Opcode fields
    uint32_t opcode() const { return (raw >> 21) & 0x7FF; }
    uint32_t op4() const { return (raw >> 0) & 0xF; }
    
    // Register fields
    uint32_t rt() const { return (raw >> 21) & 0x7F; }
    uint32_t ra() const { return (raw >> 14) & 0x7F; }
    uint32_t rb() const { return (raw >> 7) & 0x7F; }
    uint32_t rc() const { return (raw >> 0) & 0x7F; }
    
    // Immediate fields
    int16_t i10() const {
        int16_t v = (raw >> 14) & 0x3FF;
        if (v & 0x200) v |= 0xFC00;  // Sign extend
        return v;
    }
    
    uint16_t i16() const { return (raw >> 7) & 0xFFFF; }
    
    int16_t i16s() const {
        int16_t v = i16();
        return v;
    }
    
    uint32_t i18() const { return (raw >> 7) & 0x3FFFF; }
};

// ============================================================================
// SPU Interpreter
// ============================================================================
class SPUInterpreter {
public:
    SPUInterpreter();
    ~SPUInterpreter();
<<<<<<< HEAD
    
    // Initialize SPU with ID (0-5) and main memory pointer
    void Initialize(uint32_t spu_id, void* main_memory, size_t memory_size);
    void Shutdown();
    
=======

    util::Profiler profiler_;
    double GetLastStepTime() const { return profiler_.GetElapsed("SPU_Step"); }

    // Initialize SPU with ID (0-5) and main memory pointer
    void Initialize(uint32_t spu_id, void* main_memory, size_t memory_size);
    void Shutdown();

>>>>>>> c3fa6c4 (build: ARMv9 NCE, thread pool, SIMD, shader cache, UI NCE button)
    // Execute
    void Step(SPUState& state);
    void Execute(SPUState& state, uint64_t count);
    void Run(SPUState& state);
<<<<<<< HEAD
    
=======

>>>>>>> c3fa6c4 (build: ARMv9 NCE, thread pool, SIMD, shader cache, UI NCE button)
    // Thread execution (для паралельного виконання 6 SPU)
    void RunInThread(SPUState& state);
    
    // DMA/MFC operations
    void IssueMFCCommand(SPUState& state, uint32_t cmd, uint32_t lsa, uint64_t ea, uint32_t size, uint32_t tag);
    void ProcessMFCQueue(SPUState& state);
    
private:
    void* main_memory_;
    size_t memory_size_;
    uint32_t spu_id_;
    
    void ExecuteInstruction(SPUState& state, SPUInstruction inst);
    
    // ========================================================================
    // Memory Load/Store
    // ========================================================================
    void LQD(SPUState& state, SPUInstruction inst);    // Load Quadword (128-bit)
    void LQX(SPUState& state, SPUInstruction inst);    // Load Quadword Indexed
    void LQA(SPUState& state, SPUInstruction inst);    // Load Quadword Absolute
    void LQR(SPUState& state, SPUInstruction inst);    // Load Quadword PC-Relative
    void STQD(SPUState& state, SPUInstruction inst);   // Store Quadword
    void STQX(SPUState& state, SPUInstruction inst);
    void STQA(SPUState& state, SPUInstruction inst);
    void STQR(SPUState& state, SPUInstruction inst);
    
    // ========================================================================
    // Integer Arithmetic (SIMD)
    // ========================================================================
    void AH(SPUState& state, SPUInstruction inst);     // Add Halfword
    void AHI(SPUState& state, SPUInstruction inst);    // Add Halfword Immediate
    void A(SPUState& state, SPUInstruction inst);      // Add Word
    void AI(SPUState& state, SPUInstruction inst);     // Add Word Immediate
    void SFH(SPUState& state, SPUInstruction inst);    // Subtract From Halfword
    void SFHI(SPUState& state, SPUInstruction inst);
    void SF(SPUState& state, SPUInstruction inst);     // Subtract From Word
    void SFI(SPUState& state, SPUInstruction inst);
    void ADDX(SPUState& state, SPUInstruction inst);   // Add Extended
    void CG(SPUState& state, SPUInstruction inst);     // Carry Generate
    void CGX(SPUState& state, SPUInstruction inst);    // Carry Generate Extended
    void SFX(SPUState& state, SPUInstruction inst);    // Subtract From Extended
    void BG(SPUState& state, SPUInstruction inst);     // Borrow Generate
    void BGX(SPUState& state, SPUInstruction inst);
    void MPY(SPUState& state, SPUInstruction inst);    // Multiply (16-bit → 32-bit)
    void MPYU(SPUState& state, SPUInstruction inst);   // Multiply Unsigned
    void MPYH(SPUState& state, SPUInstruction inst);   // Multiply High
    void MPYHH(SPUState& state, SPUInstruction inst);  // Multiply High High
    void MPYS(SPUState& state, SPUInstruction inst);   // Multiply and Shift Right
    void MPYHHA(SPUState& state, SPUInstruction inst); // Multiply High High and Add
    
    // ========================================================================
    // Integer Compare
    // ========================================================================
    void CEQB(SPUState& state, SPUInstruction inst);   // Compare Equal Byte
    void CEQH(SPUState& state, SPUInstruction inst);   // Compare Equal Halfword
    void CEQ(SPUState& state, SPUInstruction inst);    // Compare Equal Word
    void CEQBI(SPUState& state, SPUInstruction inst);  // Compare Equal Byte Immediate
    void CEQHI(SPUState& state, SPUInstruction inst);
    void CEQI(SPUState& state, SPUInstruction inst);
    void CGTB(SPUState& state, SPUInstruction inst);   // Compare Greater Than Byte
    void CGTH(SPUState& state, SPUInstruction inst);
    void CGT(SPUState& state, SPUInstruction inst);
    void CGTBI(SPUState& state, SPUInstruction inst);
    void CGTHI(SPUState& state, SPUInstruction inst);
    void CGTI(SPUState& state, SPUInstruction inst);
    void CLGTB(SPUState& state, SPUInstruction inst);  // Compare Logical Greater Than
    void CLGTH(SPUState& state, SPUInstruction inst);
    void CLGT(SPUState& state, SPUInstruction inst);
    void CLGTBI(SPUState& state, SPUInstruction inst);
    void CLGTHI(SPUState& state, SPUInstruction inst);
    void CLGTI(SPUState& state, SPUInstruction inst);
    
    // ========================================================================
    // Branch
    // ========================================================================
    void BR(SPUState& state, SPUInstruction inst);     // Branch Relative
    void BRA(SPUState& state, SPUInstruction inst);    // Branch Absolute
    void BRSL(SPUState& state, SPUInstruction inst);   // Branch Relative and Set Link
    void BRASL(SPUState& state, SPUInstruction inst);  // Branch Absolute and Set Link
    void BI(SPUState& state, SPUInstruction inst);     // Branch Indirect
    void BISL(SPUState& state, SPUInstruction inst);   // Branch Indirect and Set Link
    void IRET(SPUState& state, SPUInstruction inst);   // Interrupt Return
    void BISLED(SPUState& state, SPUInstruction inst); // Branch Indirect and Set Link if External Data
    void BRZ(SPUState& state, SPUInstruction inst);    // Branch if Zero
    void BRNZ(SPUState& state, SPUInstruction inst);   // Branch if Not Zero
    void BRHZ(SPUState& state, SPUInstruction inst);   // Branch if Halfword Zero
    void BRHNZ(SPUState& state, SPUInstruction inst);  // Branch if Halfword Not Zero
    
    // ========================================================================
    // Logical
    // ========================================================================
    void AND(SPUState& state, SPUInstruction inst);    // AND
    void ANDC(SPUState& state, SPUInstruction inst);   // AND Complement
    void ANDBI(SPUState& state, SPUInstruction inst);  // AND Byte Immediate
    void ANDHI(SPUState& state, SPUInstruction inst);  // AND Halfword Immediate
    void ANDI(SPUState& state, SPUInstruction inst);   // AND Word Immediate
    void OR(SPUState& state, SPUInstruction inst);     // OR
    void ORC(SPUState& state, SPUInstruction inst);    // OR Complement
    void ORBI(SPUState& state, SPUInstruction inst);
    void ORHI(SPUState& state, SPUInstruction inst);
    void ORI(SPUState& state, SPUInstruction inst);
    void ORX(SPUState& state, SPUInstruction inst);    // OR Across
    void XOR(SPUState& state, SPUInstruction inst);
    void XORBI(SPUState& state, SPUInstruction inst);
    void XORHI(SPUState& state, SPUInstruction inst);
    void XORI(SPUState& state, SPUInstruction inst);
    void NAND(SPUState& state, SPUInstruction inst);
    void NOR(SPUState& state, SPUInstruction inst);
    void EQV(SPUState& state, SPUInstruction inst);
    
    // ========================================================================
    // Shift/Rotate
    // ========================================================================
    void SHLH(SPUState& state, SPUInstruction inst);   // Shift Left Halfword
    void SHLHI(SPUState& state, SPUInstruction inst);
    void SHL(SPUState& state, SPUInstruction inst);    // Shift Left Word
    void SHLI(SPUState& state, SPUInstruction inst);
    void SHLQBI(SPUState& state, SPUInstruction inst); // Shift Left Quadword by Bits
    void SHLQBII(SPUState& state, SPUInstruction inst);
    void SHLQBY(SPUState& state, SPUInstruction inst); // Shift Left Quadword by Bytes
    void SHLQBYI(SPUState& state, SPUInstruction inst);
    void ROTH(SPUState& state, SPUInstruction inst);   // Rotate Halfword
    void ROTHI(SPUState& state, SPUInstruction inst);
    void ROT(SPUState& state, SPUInstruction inst);    // Rotate Word
    void ROTI(SPUState& state, SPUInstruction inst);
    void ROTQBY(SPUState& state, SPUInstruction inst); // Rotate Quadword by Bytes
    void ROTQBYI(SPUState& state, SPUInstruction inst);
    void ROTQBI(SPUState& state, SPUInstruction inst); // Rotate Quadword by Bits
    void ROTQBII(SPUState& state, SPUInstruction inst);
    void ROTHM(SPUState& state, SPUInstruction inst);  // Rotate and Mask Halfword
    void ROTHMI(SPUState& state, SPUInstruction inst);
    void ROTM(SPUState& state, SPUInstruction inst);   // Rotate and Mask Word
    void ROTMI(SPUState& state, SPUInstruction inst);
    void ROTMAH(SPUState& state, SPUInstruction inst); // Rotate and Mask Algebraic Halfword
    void ROTMAHI(SPUState& state, SPUInstruction inst);
    void ROTMA(SPUState& state, SPUInstruction inst);  // Rotate and Mask Algebraic Word
    void ROTMAI(SPUState& state, SPUInstruction inst);
    
    // ========================================================================
    // Floating Point (використовуємо NEON)
    // ========================================================================
    void FA(SPUState& state, SPUInstruction inst);     // Floating Add
    void FS(SPUState& state, SPUInstruction inst);     // Floating Subtract
    void FM(SPUState& state, SPUInstruction inst);     // Floating Multiply
    void FMA(SPUState& state, SPUInstruction inst);    // Floating Multiply-Add
    void FNMS(SPUState& state, SPUInstruction inst);   // Floating Negative Multiply-Subtract
    void FMS(SPUState& state, SPUInstruction inst);    // Floating Multiply-Subtract
    void FREST(SPUState& state, SPUInstruction inst);  // Floating Reciprocal Estimate
    void FRSQEST(SPUState& state, SPUInstruction inst);// Floating Reciprocal Square Root Estimate
    void FI(SPUState& state, SPUInstruction inst);     // Floating Interpolate
    void FCMEQ(SPUState& state, SPUInstruction inst);  // Floating Compare Equal
    void FCMGT(SPUState& state, SPUInstruction inst);  // Floating Compare Greater Than
    void FCGT(SPUState& state, SPUInstruction inst);
    void FESD(SPUState& state, SPUInstruction inst);   // Floating Extend Single to Double
    void FRDS(SPUState& state, SPUInstruction inst);   // Floating Round Double to Single
    void FCEQ(SPUState& state, SPUInstruction inst);
    void FSCRRD(SPUState& state, SPUInstruction inst); // Floating Status and Control Register Read
    void FSCRWR(SPUState& state, SPUInstruction inst); // Floating Status and Control Register Write
    
    // ========================================================================
    // Select/Shuffle/Permute
    // ========================================================================
    void SELB(SPUState& state, SPUInstruction inst);   // Select Bits
    void SHUFB(SPUState& state, SPUInstruction inst);  // Shuffle Bytes
    void MPYA(SPUState& state, SPUInstruction inst);   // Multiply and Add
    void CBD(SPUState& state, SPUInstruction inst);    // Generate Controls for Byte Insertion
    void CBX(SPUState& state, SPUInstruction inst);
    void CHD(SPUState& state, SPUInstruction inst);    // Generate Controls for Halfword Insertion
    void CHX(SPUState& state, SPUInstruction inst);
    void CWD(SPUState& state, SPUInstruction inst);    // Generate Controls for Word Insertion
    void CWX(SPUState& state, SPUInstruction inst);
    void CDD(SPUState& state, SPUInstruction inst);    // Generate Controls for Doubleword Insertion
    void CDX(SPUState& state, SPUInstruction inst);
    
    // ========================================================================
    // Channel Instructions (SPU-specific)
    // ========================================================================
    void RDCH(SPUState& state, SPUInstruction inst);   // Read Channel
    void RCHCNT(SPUState& state, SPUInstruction inst); // Read Channel Count
    void WRCH(SPUState& state, SPUInstruction inst);   // Write Channel
    
    // ========================================================================
    // Control
    // ========================================================================
    void STOP(SPUState& state, SPUInstruction inst);   // Stop and Signal
    void STOPD(SPUState& state, SPUInstruction inst);  // Stop and Signal with Dependencies
    void LNOP(SPUState& state, SPUInstruction inst);   // No Operation (Load)
    void NOP(SPUState& state, SPUInstruction inst);    // No Operation (Execute)
    void SYNC(SPUState& state, SPUInstruction inst);   // Synchronize
    void DSYNC(SPUState& state, SPUInstruction inst);  // Synchronize Data
    void MFSPR(SPUState& state, SPUInstruction inst);  // Move From Special Purpose Register
    void MTSPR(SPUState& state, SPUInstruction inst);  // Move To Special Purpose Register
    
    // Helper functions
    void UpdateQuadword(SPUState& state, uint32_t rt, const uint8_t* data);
    void GetQuadword(SPUState& state, uint32_t rt, uint8_t* data);
};

// ============================================================================
// SPU Thread Group - Manages 6 SPUs in parallel
// ============================================================================
class SPUThreadGroup {
public:
    SPUThreadGroup();
    ~SPUThreadGroup();
    
    // Initialize with main memory
    void Initialize(void* main_memory, size_t memory_size);
    void Shutdown();
    
    // Start/stop all SPUs
    void StartAll();
    void StopAll();
    void JoinAll();
    
    // Individual SPU control
    void StartSPU(uint32_t spu_id);
    void StopSPU(uint32_t spu_id);
    
    // Load program into SPU local store
    void LoadProgram(uint32_t spu_id, const void* program, size_t size, uint32_t entry);
    
    // Access SPU state
    SPUState* GetSPUState(uint32_t spu_id);
    
private:
    static constexpr uint32_t NUM_SPUS = 6;
    
    SPUInterpreter interpreters_[NUM_SPUS];
    SPUState states_[NUM_SPUS];
    std::thread threads_[NUM_SPUS];
    
    void* main_memory_;
    size_t memory_size_;
};

} // namespace spu
} // namespace rpcsx
