/**
 * PLT (Procedure Linkage Table) Hooking для Android
 * Перехоплює виклики функцій у завантажених бібліотеках без root
 * 
 * Працює через модифікацію GOT (Global Offset Table) записів
 */

#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace rpcsx::hook {

// Hook result
struct HookResult {
    bool success = false;
    void* original_func = nullptr;
    std::string error;
};

// Hook info structure
struct HookInfo {
    const char* library_name;      // e.g., "librpcsx.so"
    const char* function_name;     // e.g., "ppu_execute_block"
    void* replacement_func;        // Our hook function
    void** original_func_ptr;      // Where to store original
};

/**
 * Initialize PLT hooking system
 * Must be called before any hooks
 */
bool InitializePLTHook();

/**
 * Shutdown PLT hooking, restore all hooks
 */
void ShutdownPLTHook();

/**
 * Hook a function in a loaded library by modifying its GOT entry
 * 
 * @param library_path  Path or name of the library (e.g., "librpcsx.so")
 * @param symbol_name   Name of the function to hook
 * @param replacement   Pointer to replacement function
 * @param original_out  Output: pointer to original function
 * @return HookResult with success status
 */
HookResult HookFunction(
    const char* library_path,
    const char* symbol_name,
    void* replacement,
    void** original_out
);

/**
 * Unhook a previously hooked function
 */
bool UnhookFunction(const char* library_path, const char* symbol_name);

/**
 * Get base address of a loaded library
 */
uintptr_t GetLibraryBase(const char* library_name);

/**
 * Find symbol address in a library
 */
void* FindSymbol(const char* library_name, const char* symbol_name);

/**
 * Patch memory at address (makes page writable temporarily)
 */
bool PatchMemory(void* address, const void* data, size_t size);

} // namespace rpcsx::hook
