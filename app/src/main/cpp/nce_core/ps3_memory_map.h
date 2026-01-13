// ============================================================================
// PS3 Memory Map - Native Address Space Emulation
// ============================================================================
// Телефон думає що він PS3 - справжній memory layout!
// ============================================================================

#pragma once

#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>

namespace rpcsx {
namespace nce {

// ============================================================================
// PS3 Memory Layout (Cell Broadband Engine)
// ============================================================================
// PS3 має 256MB XDR RAM + 256MB GDDR3 VRAM
// Адресний простір: 0x00000000 - 0xFFFFFFFF (32-bit effective address)
// ============================================================================

namespace ps3 {

// Main Memory Regions
constexpr uint64_t MAIN_MEMORY_BASE     = 0x00000000ULL;
constexpr uint64_t MAIN_MEMORY_SIZE     = 256 * 1024 * 1024;  // 256MB XDR

// Video Memory (RSX)
constexpr uint64_t VIDEO_MEMORY_BASE    = 0xC0000000ULL;
constexpr uint64_t VIDEO_MEMORY_SIZE    = 256 * 1024 * 1024;  // 256MB GDDR3

// Local Store for SPUs (each SPU has 256KB)
constexpr uint64_t SPU_LS_BASE          = 0xE0000000ULL;
constexpr uint64_t SPU_LS_SIZE          = 256 * 1024;         // 256KB per SPU
constexpr int      SPU_COUNT            = 6;                   // 6 usable SPUs

// Memory-Mapped I/O
constexpr uint64_t MMIO_BASE            = 0xF0000000ULL;
constexpr uint64_t MMIO_SIZE            = 16 * 1024 * 1024;

// RSX Command Buffer
constexpr uint64_t RSX_CMD_BASE         = 0x40000000ULL;
constexpr uint64_t RSX_CMD_SIZE         = 16 * 1024 * 1024;

// Stack Region (per PPU thread)
constexpr uint64_t STACK_BASE           = 0xD0000000ULL;
constexpr uint64_t STACK_SIZE           = 1 * 1024 * 1024;    // 1MB per thread

// PRX/Module Load Region
constexpr uint64_t MODULE_BASE          = 0x10000000ULL;
constexpr uint64_t MODULE_SIZE          = 256 * 1024 * 1024;

// TLS (Thread Local Storage)
constexpr uint64_t TLS_BASE             = 0x00700000ULL;
constexpr uint64_t TLS_SIZE             = 1 * 1024 * 1024;

// ============================================================================
// PS3 Page Table Entry
// ============================================================================
struct PageTableEntry {
    uint64_t virtual_addr;
    uint64_t physical_addr;
    uint32_t size;
    uint32_t flags;
    
    enum Flags : uint32_t {
        READ      = 1 << 0,
        WRITE     = 1 << 1,
        EXECUTE   = 1 << 2,
        CACHED    = 1 << 3,
        GUARDED   = 1 << 4,  // No speculative access
        NO_CACHE  = 1 << 5,
    };
};

// ============================================================================
// PS3 Memory Manager
// ============================================================================
class PS3MemoryManager {
public:
    static PS3MemoryManager& Instance();
    
    // Initialize PS3 memory layout on Android
    bool Initialize();
    void Shutdown();
    
    // Memory allocation (PS3 style)
    void* AllocateMainMemory(uint64_t ps3_addr, size_t size, uint32_t flags);
    void* AllocateVideoMemory(uint64_t ps3_addr, size_t size);
    void* AllocateSPULocalStore(int spu_id);
    void FreeMemory(uint64_t ps3_addr);
    
    // Address translation: PS3 → Host
    void* PS3ToHost(uint64_t ps3_addr) const;
    uint64_t HostToPS3(void* host_addr) const;
    
    // Memory protection
    bool SetProtection(uint64_t ps3_addr, size_t size, uint32_t flags);
    
    // Memory info
    size_t GetFreeMainMemory() const;
    size_t GetFreeVideoMemory() const;
    
    // Direct memory access (for NCE)
    uint8_t  Read8(uint64_t ps3_addr) const;
    uint16_t Read16(uint64_t ps3_addr) const;
    uint32_t Read32(uint64_t ps3_addr) const;
    uint64_t Read64(uint64_t ps3_addr) const;
    
    void Write8(uint64_t ps3_addr, uint8_t value);
    void Write16(uint64_t ps3_addr, uint16_t value);
    void Write32(uint64_t ps3_addr, uint32_t value);
    void Write64(uint64_t ps3_addr, uint64_t value);
    
    // Big-endian support (PS3 is BE, ARM is LE)
    uint16_t Read16BE(uint64_t ps3_addr) const;
    uint32_t Read32BE(uint64_t ps3_addr) const;
    uint64_t Read64BE(uint64_t ps3_addr) const;
    
    void Write16BE(uint64_t ps3_addr, uint16_t value);
    void Write32BE(uint64_t ps3_addr, uint32_t value);
    void Write64BE(uint64_t ps3_addr, uint64_t value);
    
private:
    PS3MemoryManager() = default;
    ~PS3MemoryManager() = default;
    
    // Host memory regions
    void* main_memory_ = nullptr;
    void* video_memory_ = nullptr;
    void* spu_ls_[SPU_COUNT] = {};
    void* mmio_region_ = nullptr;
    void* rsx_cmd_buffer_ = nullptr;
    
    // Memory tracking
    size_t main_memory_used_ = 0;
    size_t video_memory_used_ = 0;
    
    bool initialized_ = false;
};

}  // namespace ps3
}  // namespace nce
}  // namespace rpcsx
