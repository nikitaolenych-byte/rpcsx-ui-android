/**
 * PLT Hooking Implementation для Android ARM64
 * 
 * Техніка: модифікація GOT (Global Offset Table) для перенаправлення
 * викликів функцій на наші обробники без потреби root
 */

#include "plt_hook.h"

#include <android/log.h>
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>

#define LOG_TAG "RPCSX-PLTHook"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace rpcsx::hook {

namespace {

// Track installed hooks for cleanup
struct InstalledHook {
    std::string library;
    std::string symbol;
    void* got_entry;
    void* original_value;
    void* replacement;
};

std::mutex g_hook_mutex;
std::vector<InstalledHook> g_installed_hooks;
bool g_initialized = false;

// Parse /proc/self/maps to find library base
struct MapEntry {
    uintptr_t start;
    uintptr_t end;
    std::string perms;
    std::string path;
};

std::vector<MapEntry> ParseProcMaps() {
    std::vector<MapEntry> entries;
    std::ifstream maps("/proc/self/maps");
    std::string line;
    
    while (std::getline(maps, line)) {
        MapEntry entry;
        char perms[5] = {0};
        char path[512] = {0};
        unsigned long start, end, offset, inode;
        unsigned int dev_major, dev_minor;
        
        int parsed = sscanf(line.c_str(), 
            "%lx-%lx %4s %lx %x:%x %lu %511s",
            &start, &end, perms, &offset, &dev_major, &dev_minor, &inode, path);
        
        if (parsed >= 7) {
            entry.start = start;
            entry.end = end;
            entry.perms = perms;
            entry.path = (parsed >= 8) ? path : "";
            entries.push_back(entry);
        }
    }
    
    return entries;
}

// Find library base from maps
uintptr_t FindLibraryBaseFromMaps(const char* library_name) {
    auto maps = ParseProcMaps();
    
    for (const auto& entry : maps) {
        if (entry.path.find(library_name) != std::string::npos) {
            if (entry.perms.find('x') != std::string::npos || 
                entry.perms.find('r') != std::string::npos) {
                return entry.start;
            }
        }
    }
    
    return 0;
}

// Get page size
size_t GetPageSize() {
    static size_t page_size = sysconf(_SC_PAGESIZE);
    return page_size;
}

// Align address to page boundary
uintptr_t PageAlign(uintptr_t addr) {
    size_t page_size = GetPageSize();
    return addr & ~(page_size - 1);
}

// Make memory writable, execute callback, restore protection
template<typename Func>
bool WithWritableMemory(void* address, size_t size, Func&& func) {
    size_t page_size = GetPageSize();
    uintptr_t page_start = PageAlign(reinterpret_cast<uintptr_t>(address));
    uintptr_t page_end = PageAlign(reinterpret_cast<uintptr_t>(address) + size + page_size - 1);
    size_t region_size = page_end - page_start;
    
    // Make writable
    if (mprotect(reinterpret_cast<void*>(page_start), region_size, 
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGE("mprotect failed to make memory writable: %s", strerror(errno));
        return false;
    }
    
    // Execute the modification
    bool result = func();
    
    // Restore protection (read + exec for code)
    mprotect(reinterpret_cast<void*>(page_start), region_size, 
             PROT_READ | PROT_EXEC);
    
    // Clear instruction cache
    __builtin___clear_cache(
        reinterpret_cast<char*>(page_start),
        reinterpret_cast<char*>(page_end)
    );
    
    return result;
}

// Find GOT entry for a symbol using ELF parsing
void* FindGOTEntry(uintptr_t lib_base, const char* symbol_name) {
    // Parse ELF header
    Elf64_Ehdr* ehdr = reinterpret_cast<Elf64_Ehdr*>(lib_base);
    
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        LOGE("Invalid ELF magic");
        return nullptr;
    }
    
    // Find section headers
    Elf64_Shdr* shdr = reinterpret_cast<Elf64_Shdr*>(lib_base + ehdr->e_shoff);
    
    // Find dynamic section
    Elf64_Dyn* dynamic = nullptr;
    size_t dynamic_size = 0;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_DYNAMIC) {
            dynamic = reinterpret_cast<Elf64_Dyn*>(lib_base + shdr[i].sh_offset);
            dynamic_size = shdr[i].sh_size;
            break;
        }
    }
    
    if (!dynamic) {
        LOGE("Could not find dynamic section");
        return nullptr;
    }
    
    // Parse dynamic entries to find:
    // - DT_SYMTAB (symbol table)
    // - DT_STRTAB (string table)
    // - DT_JMPREL (PLT relocations)
    // - DT_PLTRELSZ (PLT relocation size)
    
    Elf64_Sym* symtab = nullptr;
    const char* strtab = nullptr;
    Elf64_Rela* jmprel = nullptr;
    size_t pltrelsz = 0;
    void* pltgot = nullptr;
    
    for (Elf64_Dyn* d = dynamic; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
            case DT_SYMTAB:
                symtab = reinterpret_cast<Elf64_Sym*>(d->d_un.d_ptr);
                break;
            case DT_STRTAB:
                strtab = reinterpret_cast<const char*>(d->d_un.d_ptr);
                break;
            case DT_JMPREL:
                jmprel = reinterpret_cast<Elf64_Rela*>(d->d_un.d_ptr);
                break;
            case DT_PLTRELSZ:
                pltrelsz = d->d_un.d_val;
                break;
            case DT_PLTGOT:
                pltgot = reinterpret_cast<void*>(d->d_un.d_ptr);
                break;
        }
    }
    
    if (!symtab || !strtab || !jmprel || pltrelsz == 0) {
        LOGE("Missing required dynamic entries");
        return nullptr;
    }
    
    // Search PLT relocations for our symbol
    size_t num_rels = pltrelsz / sizeof(Elf64_Rela);
    
    for (size_t i = 0; i < num_rels; i++) {
        Elf64_Rela* rel = &jmprel[i];
        uint32_t sym_idx = ELF64_R_SYM(rel->r_info);
        
        if (sym_idx == 0) continue;
        
        const char* sym_name = strtab + symtab[sym_idx].st_name;
        
        if (strcmp(sym_name, symbol_name) == 0) {
            // Found it! Return GOT entry address
            void* got_entry = reinterpret_cast<void*>(lib_base + rel->r_offset);
            LOGD("Found GOT entry for %s at %p", symbol_name, got_entry);
            return got_entry;
        }
    }
    
    LOGE("Symbol %s not found in PLT relocations", symbol_name);
    return nullptr;
}

// Alternative: find GOT by searching relocations more thoroughly
void* FindGOTEntryAlt(void* lib_handle, const char* symbol_name) {
    // Use dlsym to get actual function address
    void* func_addr = dlsym(lib_handle, symbol_name);
    if (!func_addr) {
        LOGE("dlsym failed for %s: %s", symbol_name, dlerror());
        return nullptr;
    }
    
    LOGD("Function %s found at %p", symbol_name, func_addr);
    
    // We can't easily get GOT entry from dlsym result
    // Return the function pointer directly for inline hooking
    return func_addr;
}

} // anonymous namespace

bool InitializePLTHook() {
    std::lock_guard<std::mutex> lock(g_hook_mutex);
    
    if (g_initialized) {
        return true;
    }
    
    LOGI("Initializing PLT Hook system");
    g_installed_hooks.clear();
    g_initialized = true;
    
    return true;
}

void ShutdownPLTHook() {
    std::lock_guard<std::mutex> lock(g_hook_mutex);
    
    LOGI("Shutting down PLT Hook, restoring %zu hooks", g_installed_hooks.size());
    
    // Restore all hooks
    for (auto& hook : g_installed_hooks) {
        if (hook.got_entry && hook.original_value) {
            WithWritableMemory(hook.got_entry, sizeof(void*), [&]() {
                *reinterpret_cast<void**>(hook.got_entry) = hook.original_value;
                return true;
            });
        }
    }
    
    g_installed_hooks.clear();
    g_initialized = false;
}

uintptr_t GetLibraryBase(const char* library_name) {
    return FindLibraryBaseFromMaps(library_name);
}

void* FindSymbol(const char* library_name, const char* symbol_name) {
    void* handle = dlopen(library_name, RTLD_NOLOAD);
    if (!handle) {
        LOGE("Library %s not loaded", library_name);
        return nullptr;
    }
    
    void* sym = dlsym(handle, symbol_name);
    dlclose(handle);
    
    return sym;
}

bool PatchMemory(void* address, const void* data, size_t size) {
    return WithWritableMemory(address, size, [&]() {
        memcpy(address, data, size);
        return true;
    });
}

HookResult HookFunction(
    const char* library_path,
    const char* symbol_name,
    void* replacement,
    void** original_out
) {
    std::lock_guard<std::mutex> lock(g_hook_mutex);
    
    HookResult result;
    
    if (!g_initialized) {
        result.error = "PLT Hook not initialized";
        return result;
    }
    
    LOGI("Attempting to hook %s in %s", symbol_name, library_path);
    
    // Find library base
    uintptr_t lib_base = GetLibraryBase(library_path);
    if (lib_base == 0) {
        result.error = "Could not find library base";
        LOGE("%s", result.error.c_str());
        return result;
    }
    
    LOGD("Library base: 0x%lx", (unsigned long)lib_base);
    
    // Find GOT entry
    void* got_entry = FindGOTEntry(lib_base, symbol_name);
    if (!got_entry) {
        // Try alternative method
        void* handle = dlopen(library_path, RTLD_NOLOAD);
        if (handle) {
            got_entry = FindGOTEntryAlt(handle, symbol_name);
            dlclose(handle);
        }
    }
    
    if (!got_entry) {
        result.error = "Could not find GOT entry for symbol";
        LOGE("%s", result.error.c_str());
        return result;
    }
    
    // Read original value
    void* original = *reinterpret_cast<void**>(got_entry);
    
    // Patch GOT entry
    bool patched = WithWritableMemory(got_entry, sizeof(void*), [&]() {
        *reinterpret_cast<void**>(got_entry) = replacement;
        return true;
    });
    
    if (!patched) {
        result.error = "Failed to patch GOT entry";
        LOGE("%s", result.error.c_str());
        return result;
    }
    
    // Save hook info
    InstalledHook hook;
    hook.library = library_path;
    hook.symbol = symbol_name;
    hook.got_entry = got_entry;
    hook.original_value = original;
    hook.replacement = replacement;
    g_installed_hooks.push_back(hook);
    
    // Return results
    if (original_out) {
        *original_out = original;
    }
    
    result.success = true;
    result.original_func = original;
    
    LOGI("Successfully hooked %s (original: %p, replacement: %p)", 
         symbol_name, original, replacement);
    
    return result;
}

bool UnhookFunction(const char* library_path, const char* symbol_name) {
    std::lock_guard<std::mutex> lock(g_hook_mutex);
    
    for (auto it = g_installed_hooks.begin(); it != g_installed_hooks.end(); ++it) {
        if (it->library == library_path && it->symbol == symbol_name) {
            if (it->got_entry && it->original_value) {
                bool restored = WithWritableMemory(it->got_entry, sizeof(void*), [&]() {
                    *reinterpret_cast<void**>(it->got_entry) = it->original_value;
                    return true;
                });
                
                if (restored) {
                    LOGI("Unhooked %s in %s", symbol_name, library_path);
                    g_installed_hooks.erase(it);
                    return true;
                }
            }
            break;
        }
    }
    
    return false;
}

} // namespace rpcsx::hook
