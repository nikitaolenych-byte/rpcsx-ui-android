// ============================================================================
// PPU JIT Compiler (PowerPC64 → ARM64)
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <string>

namespace rpcsx {
namespace ppu {

class PPUJITCompiler {
public:
    PPUJITCompiler();
    ~PPUJITCompiler();

    // Initialize JIT (allocate code cache, setup backend)
    bool Initialize(size_t cache_size = 256 * 1024 * 1024);
    void Shutdown();

    // Compile a block of PPU code at given address
    void* CompileBlock(uint64_t ppu_addr);

    // Execute compiled block (returns next PC)
    uint64_t ExecuteBlock(void* code, uint64_t pc, uint64_t* gpr, double* fpr);

    // Invalidate code cache
    void InvalidateAll();

    // Statistics
    size_t GetCompiledBlockCount() const;
    size_t GetCacheUsed() const;

private:
    struct BlockInfo {
        uint64_t ppu_addr;
        void* code_ptr;
        size_t code_size;
    };
    std::vector<BlockInfo> blocks_;
    void* code_cache_ = nullptr;
    size_t cache_size_ = 0;
    size_t cache_used_ = 0;
};

} // namespace ppu
} // namespace rpcsx
