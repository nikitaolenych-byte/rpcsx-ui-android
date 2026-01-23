/**
 * Adrenotools stub header for RPCSX Android build
 * Provides definitions for custom Adreno driver loading
 */

#ifndef ADRENOTOOLS_DRIVER_H
#define ADRENOTOOLS_DRIVER_H

#include <cstdint>
#include <dlfcn.h>
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

// Driver type flags
enum AdrenotoolsDriverFlags {
    ADRENOTOOLS_DRIVER_DEFAULT = 0,
    ADRENOTOOLS_DRIVER_CUSTOM = 1,
    ADRENOTOOLS_DRIVER_FILE = 2,
};

/**
 * Open a custom Vulkan driver library
 * 
 * @param mode RTLD flags (e.g., RTLD_NOW)
 * @param flags Adrenotools driver flags
 * @param reserved Reserved, pass nullptr
 * @param hook_dir Directory for hook libraries
 * @param driver_dir Directory containing driver files
 * @param driver_name Name of the driver library
 * @param custom_target Reserved, pass nullptr
 * @param custom_path Reserved, pass nullptr
 * @return Handle to loaded library, or nullptr on failure
 */
static inline void* adrenotools_open_libvulkan(
    int mode,
    int flags,
    void* reserved,
    const char* hook_dir,
    const char* driver_dir,
    const char* driver_name,
    void* custom_target,
    void* custom_path
) {
    (void)reserved;
    (void)hook_dir;
    (void)custom_target;
    (void)custom_path;
    (void)flags;
    
    // Construct full path to driver
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", driver_dir, driver_name);
    
    // Try to load the driver directly
    void* handle = dlopen(full_path, mode);
    if (handle) {
        return handle;
    }
    
    // Fallback: try system Vulkan
    return dlopen("libvulkan.so", mode);
}

#ifdef __cplusplus
}
#endif

#endif // ADRENOTOOLS_DRIVER_H
