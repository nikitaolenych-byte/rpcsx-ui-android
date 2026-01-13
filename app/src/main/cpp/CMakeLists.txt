cmake_minimum_required(VERSION 3.14) # Bump for FetchContent
project("rpcsx-android")

include(FetchContent)

set(RPCSX_COMMON_OPT_FLAGS "")
set(RPCSX_LTO_FLAGS "")

# Fetch Zstd for Shader Cache compression
FetchContent_Declare(
  zstd
  GIT_REPOSITORY https://github.com/facebook/zstd.git
  GIT_TAG v1.5.5
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR build/cmake 
)
# Disable Zstd legacy support and binaries to save space
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zstd)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE on)

# ARMv9 + SVE2 оптимізації для Snapdragon 8s Gen 3
if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(RPCSX_COMMON_OPT_FLAGS
        -Ofast
        -O3
        -ffast-math
        -ftree-vectorize
        -funroll-loops
        -finline-functions
        -fdata-sections
        -ffunction-sections
        -Wno-unused-command-line-argument
        -mcpu=cortex-x4
        -march=armv9-a+sve2
    )

    # LTO (force, fallback to thin if needed)
    include(CheckCCompilerFlag)
    include(CheckCXXCompilerFlag)
    check_c_compiler_flag("-flto" HAVE_C_FLTO)
    check_cxx_compiler_flag("-flto" HAVE_CXX_FLTO)
    if (HAVE_C_FLTO AND HAVE_CXX_FLTO)
        set(RPCSX_LTO_FLAGS -flto)
    else()
        set(RPCSX_LTO_FLAGS -flto=thin)
    endif()

    # Fetch libadrenotools
    FetchContent_Declare(
      libadrenotools
      GIT_REPOSITORY https://github.com/bylaws/libadrenotools.git
      GIT_TAG master
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(libadrenotools)
    
    if(TARGET libadrenotools AND NOT TARGET adrenotools)
        add_library(adrenotools ALIAS libadrenotools)
    endif()
else()
    add_library(adrenotools INTERFACE)
endif()

add_library(${CMAKE_PROJECT_NAME} SHARED 
    native-lib.cpp
    signal_handler.cpp
    nce_engine.cpp
    fastmem_mapper.cpp
    shader_cache_manager.cpp
    thread_scheduler.cpp
    frostbite_hacks.cpp
    realsteel_hacks.cpp
    game_patches.cpp
    vulkan_renderer.cpp
    fsr31/fsr31.cpp
    # NCE JIT PowerPC (Cell PPU) → ARM64 compiler
    nce_jit/ppc_decoder.cpp
    nce_jit/arm64_emitter.cpp
    # NCE Standalone JIT (ARM64 only)
    nce_core/nce_standalone.cpp
    # PLT Hooking + PPU Interceptor
    plt_hook.cpp
    ppu_interceptor.cpp
    nce_hook.cpp
)

# Apply aggressive optimisation flags to native library (Release + RelWithDebInfo)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${RPCSX_COMMON_OPT_FLAGS}>
)
if (RPCSX_LTO_FLAGS)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${RPCSX_LTO_FLAGS}>
    )
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${RPCSX_LTO_FLAGS}>
    )
endif()

# Zstd headers are under <zstd.h> in ${zstd_SOURCE_DIR}/lib
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${zstd_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}  # For nce_jit includes
)

# Підключення Mesa Turnip для Adreno 735
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # ${CMAKE_CURRENT_SOURCE_DIR}/mesa_turnip/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/fsr31/include
)

# Find Vulkan for vkGetDeviceProcAddr etc.
find_library(VULKAN_LIB vulkan)

target_link_libraries(${CMAKE_PROJECT_NAME}
    android
    log
    adrenotools
    z
    libzstd_static
    ${VULKAN_LIB}
)
