// ============================================================================
// SPU JIT Compiler (Cell SPU → ARM64 NEON/SVE2)
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <string>

namespace rpcsx {
namespace spu {

class SPUJITCompiler {
public:
    SPUJITCompiler();
    ~SPUJITCompiler();

    // Initialize JIT (allocate code cache, setup backend)
    bool Initialize(size_t cache_size = 64 * 1024 * 1024);
    void Shutdown();

    // Compile a block of SPU code at given address
    void* CompileBlock(uint32_t spu_addr);

    // Execute compiled block (returns next PC)
    uint32_t ExecuteBlock(void* code, uint32_t pc, void* gpr128);

    // Invalidate code cache
    void InvalidateAll();

    // Statistics
    size_t GetCompiledBlockCount() const;
    size_t GetCacheUsed() const;

private:
    struct BlockInfo {
        uint32_t spu_addr;
        void* code_ptr;
        size_t code_size;
    };
    std::vector<BlockInfo> blocks_;
    void* code_cache_ = nullptr;
    size_t cache_size_ = 0;
    size_t cache_used_ = 0;
};

} // namespace spu
} // namespace rpcsx
