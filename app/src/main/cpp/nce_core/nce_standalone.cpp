/**
 * NCE Standalone Implementation
 * ARM64 JIT Compiler for Android
 */

#include "nce_standalone.h"

#include <sys/mman.h>
#include <cstring>
#include <android/log.h>

#define NCE_LOG_TAG "NCE-JIT"
#define NCE_LOGI(...) __android_log_print(ANDROID_LOG_INFO, NCE_LOG_TAG, __VA_ARGS__)
#define NCE_LOGW(...) __android_log_print(ANDROID_LOG_WARN, NCE_LOG_TAG, __VA_ARGS__)
#define NCE_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, NCE_LOG_TAG, __VA_ARGS__)

namespace nce {

// ============================================================================
// NCECompiler Implementation
// ============================================================================

NCECompiler::NCECompiler() = default;

NCECompiler::~NCECompiler() {
    Shutdown();
}

bool NCECompiler::Initialize(const JitConfig& config) {
    if (m_initialized) {
        return true;
    }
    
    m_config = config;
    
    // Allocate executable code cache
    m_code_cache_size = config.code_cache_size;
    m_code_cache = mmap(nullptr, m_code_cache_size,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (m_code_cache == MAP_FAILED) {
        NCE_LOGE("Failed to allocate code cache: %s", strerror(errno));
        m_code_cache = nullptr;
        return false;
    }
    
    m_code_cache_used = 0;
    m_initialized = true;
    
    NCE_LOGI("NCE ARM64 JIT initialized with %zuMB code cache", 
             m_code_cache_size / (1024 * 1024));
    
    return true;
}

void NCECompiler::Shutdown() {
    if (!m_initialized) return;
    
    NCE_LOGI("NCE shutdown - blocks: %llu, executed: %llu, instructions: %llu",
             (unsigned long long)m_stats.blocks_compiled.load(),
             (unsigned long long)m_stats.blocks_executed.load(),
             (unsigned long long)m_stats.instructions_executed.load());
    
    {
        std::lock_guard lock(m_blocks_mutex);
        m_blocks.clear();
    }
    
    if (m_code_cache) {
        munmap(m_code_cache, m_code_cache_size);
        m_code_cache = nullptr;
    }
    
    m_initialized = false;
}

void* NCECompiler::AllocateCode(size_t size) {
    size = (size + 15) & ~15;
    
    if (m_code_cache_used + size > m_code_cache_size) {
        NCE_LOGW("Code cache full, clearing...");
        ClearCache();
    }
    
    void* ptr = static_cast<uint8_t*>(m_code_cache) + m_code_cache_used;
    m_code_cache_used += size;
    
    return ptr;
}

void NCECompiler::ClearCache() {
    std::lock_guard lock(m_blocks_mutex);
    m_blocks.clear();
    m_code_cache_used = 0;
}

CompiledBlock* NCECompiler::LookupBlock(uint32_t guest_addr) {
    std::lock_guard lock(m_blocks_mutex);
    
    auto it = m_blocks.find(guest_addr);
    if (it != m_blocks.end()) {
        m_stats.cache_hits++;
        return it->second.get();
    }
    
    m_stats.cache_misses++;
    return nullptr;
}

void NCECompiler::InvalidateRange(uint32_t start, uint32_t size) {
    std::lock_guard lock(m_blocks_mutex);
    
    for (auto it = m_blocks.begin(); it != m_blocks.end();) {
        uint32_t block_start = it->second->guest_addr;
        uint32_t block_end = block_start + it->second->guest_size;
        
        if (block_start < start + size && block_end > start) {
            it = m_blocks.erase(it);
        } else {
            ++it;
        }
    }
}

void NCECompiler::ResetStats() {
    m_stats.blocks_compiled = 0;
    m_stats.blocks_executed = 0;
    m_stats.instructions_executed = 0;
    m_stats.cache_hits = 0;
    m_stats.cache_misses = 0;
    m_stats.interpreter_fallbacks = 0;
}

CompiledBlock* NCECompiler::CompileBlock(void* memory_base, uint32_t guest_addr) {
    if (auto* block = LookupBlock(guest_addr)) {
        return block;
    }
    
    std::vector<uint32_t> code;
    code.reserve(2048);
    
    ARM64Emitter emit(code);
    PPUTranslator trans(emit);
    
    // ===== Prologue =====
    emit.STP_pre(arm64::X19, arm64::X20, 31, -16);
    emit.STP_pre(arm64::X21, arm64::X22, 31, -16);
    emit.STP_pre(arm64::X29, arm64::X30, 31, -16);
    
    // X0 = PPUState*
    emit.MOV(arm64::REG_STATE, arm64::X0);
    
    // Load memory base
    constexpr int MEM_BASE_OFFSET = offsetof(PPUState, memory_base);
    emit.LDR(arm64::REG_MEM, arm64::REG_STATE, MEM_BASE_OFFSET);
    
    // ===== Compile Instructions =====
    uint32_t addr = guest_addr;
    uint32_t block_size = 0;
    bool end_block = false;
    uint32_t branch_target = 0;
    
    uint8_t* mem = static_cast<uint8_t*>(memory_base);
    
    while (!end_block && block_size < m_config.max_block_size) {
        // Read big-endian instruction
        uint32_t opcode_raw = 
            (static_cast<uint32_t>(mem[addr + 0]) << 24) |
            (static_cast<uint32_t>(mem[addr + 1]) << 16) |
            (static_cast<uint32_t>(mem[addr + 2]) << 8) |
            (static_cast<uint32_t>(mem[addr + 3]));
        
        PPUOpcode op{opcode_raw};
        
        trans.UpdatePC(addr);
        
        uint32_t primary = op.primary();
        
        switch (primary) {
            case 16: // BC
                end_block = true;
                branch_target = op.aa() ? op.bd() : (addr + op.bd());
                break;
                
            case 18: // B/BL
                end_block = true;
                branch_target = op.aa() ? op.li() : (addr + op.li());
                if (op.lk()) {
                    emit.MOV64(arm64::REG_TMP0, addr + 4);
                    emit.STR(arm64::REG_TMP0, arm64::REG_STATE, PPUTranslator::LR_OFFSET);
                }
                break;
                
            case 19: // bclr, bcctr
            {
                uint32_t xo = op.xo_10();
                if (xo == 16 || xo == 528) {
                    end_block = true;
                }
                break;
            }
                
            case 14: trans.INSN_ADDI(op); break;
            case 15: trans.INSN_ADDIS(op); break;
                
            case 31:
            {
                uint32_t xo = op.xo_9();
                switch (xo) {
                    case 266: trans.INSN_ADD(op); break;
                    case 40: trans.INSN_SUBF(op); break;
                    case 235: trans.INSN_MULLW(op); break;
                    case 491: trans.INSN_DIVW(op); break;
                    case 28: trans.INSN_AND(op); break;
                    case 444: trans.INSN_OR(op); break;
                    case 316: trans.INSN_XOR(op); break;
                    case 24: trans.INSN_SLW(op); break;
                    case 536: trans.INSN_SRW(op); break;
                    case 339:
                    {
                        uint32_t spr = ((op.raw >> 16) & 0x1F) | ((op.raw >> 6) & 0x3E0);
                        if (spr == 8) trans.INSN_MFLR(op);
                        else if (spr == 9) trans.INSN_MFCTR(op);
                        break;
                    }
                    case 467:
                    {
                        uint32_t spr = ((op.raw >> 16) & 0x1F) | ((op.raw >> 6) & 0x3E0);
                        if (spr == 8) trans.INSN_MTLR(op);
                        else if (spr == 9) trans.INSN_MTCTR(op);
                        break;
                    }
                    default:
                        emit.NOP();
                        break;
                }
                break;
            }
                
            case 24: trans.INSN_ORI(op); break;
            case 10: trans.INSN_CMPI(op); break;
            case 11: trans.INSN_CMPI(op); break;
            case 32: trans.INSN_LWZ(op); break;
            case 33: trans.INSN_LWZ(op); break;
            case 34: trans.INSN_LBZ(op); break;
            case 36: trans.INSN_STW(op); break;
            case 38: trans.INSN_STB(op); break;
            case 58:
            {
                uint32_t sub = op.raw & 3;
                if (sub == 0) trans.INSN_LD(op);
                break;
            }
            case 62:
            {
                uint32_t sub = op.raw & 3;
                if (sub == 0) trans.INSN_STD(op);
                break;
            }
            case 21: trans.INSN_RLWINM(op); break;
                
            default:
                emit.NOP();
                break;
        }
        
        addr += 4;
        block_size++;
    }
    
    trans.UpdatePC(branch_target ? branch_target : addr);
    
    // ===== Epilogue =====
    emit.LDP_post(arm64::X29, arm64::X30, 31, 16);
    emit.LDP_post(arm64::X21, arm64::X22, 31, 16);
    emit.LDP_post(arm64::X19, arm64::X20, 31, 16);
    emit.RET();
    
    // Allocate and copy
    size_t code_size = code.size() * sizeof(uint32_t);
    void* code_ptr = AllocateCode(code_size);
    memcpy(code_ptr, code.data(), code_size);
    
    // Clear icache
    __builtin___clear_cache(static_cast<char*>(code_ptr), 
                            static_cast<char*>(code_ptr) + code_size);
    
    auto block = std::make_unique<CompiledBlock>();
    block->code = code_ptr;
    block->guest_addr = guest_addr;
    block->guest_size = block_size * 4;
    block->host_size = code_size;
    block->exec_count = 0;
    block->has_branch = end_block;
    block->branch_target = branch_target;
    
    CompiledBlock* result = block.get();
    
    {
        std::lock_guard lock(m_blocks_mutex);
        m_blocks[guest_addr] = std::move(block);
    }
    
    m_stats.blocks_compiled++;
    
    if ((m_stats.blocks_compiled % 1000) == 0) {
        NCE_LOGI("Compiled %llu blocks", (unsigned long long)m_stats.blocks_compiled.load());
    }
    
    return result;
}

void NCECompiler::ExecuteBlock(PPUState& state, CompiledBlock* block) {
    if (!block || !block->code) {
        return;
    }
    
    using JitFunc = void (*)(PPUState*);
    auto func = reinterpret_cast<JitFunc>(block->code);
    func(&state);
    
    block->exec_count++;
    m_stats.blocks_executed++;
    m_stats.instructions_executed += block->guest_size / 4;
}

} // namespace nce
