cmake_minimum_required(VERSION 3.14) # Bump for FetchContent
project("rpcsx-android")

include(FetchContent)

set(RPCSX_COMMON_OPT_FLAGS "")
set(RPCSX_LTO_FLAGS "")

# Disable FetchContent updates and warnings
set(FETCHCONTENT_QUIET ON)
set(FETCHCONTENT_FULLY_DISCONNECTED OFF)

# Fetch Zstd for Shader Cache compression
FetchContent_Declare(
  zstd
  GIT_REPOSITORY https://github.com/facebook/zstd.git
  GIT_TAG v1.5.5
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR build/cmake
  TIMEOUT 60
)
# Disable Zstd legacy support and binaries to save space
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)

message(STATUS "Fetching Zstd...")
FetchContent_MakeAvailable(zstd)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE on)

# ARMv9-A оптимізації для Snapdragon 8s Gen 3
# Simplified for better compatibility and build stability
if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(RPCSX_COMMON_OPT_FLAGS
        -O3
        -march=armv9-a
        -mtune=cortex-x4
        -ffast-math
        -ftree-vectorize
        -funroll-loops
        -finline-functions
        -fdata-sections
        -ffunction-sections
    )

    # LTO: Use thin LTO for faster builds, falls back gracefully
    include(CheckCCompilerFlag)
    include(CheckCXXCompilerFlag)
    check_c_compiler_flag("-flto=thin" HAVE_C_FLTO_THIN)
    check_cxx_compiler_flag("-flto=thin" HAVE_CXX_FLTO_THIN)
    if (HAVE_C_FLTO_THIN AND HAVE_CXX_FLTO_THIN)
        set(RPCSX_LTO_FLAGS -flto=thin)
    endif()

    # Fetch libadrenotools with timeout
    set(FETCHCONTENT_QUIET ON)
    set(FETCHCONTENT_UPDATES_DISCONNECTED_LIBADRENOTOOLS ON)
    
    FetchContent_Declare(
      libadrenotools
      GIT_REPOSITORY https://github.com/bylaws/libadrenotools.git
      GIT_TAG master
      GIT_SHALLOW TRUE
      TIMEOUT 60
    )
    
    # Try to make available, but continue if fails
    if(NOT libadrenotools_POPULATED)
        message(STATUS "Fetching libadrenotools...")
        FetchContent_Populate(libadrenotools)
        if(EXISTS "${libadrenotools_SOURCE_DIR}/CMakeLists.txt")
            add_subdirectory(${libadrenotools_SOURCE_DIR} ${libadrenotools_BINARY_DIR})
        endif()
    endif()
    
    if(TARGET libadrenotools AND NOT TARGET adrenotools)
        add_library(adrenotools ALIAS libadrenotools)
    else()
        message(WARNING "libadrenotools not available, using stub")
        add_library(adrenotools INTERFACE)
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
    # NCE Native - Real PS3 Execution (телефон = PS3!)
    nce_core/nce_native.cpp
    nce_core/thread_pool.cpp
    nce_core/game_mode_android.cpp
    nce_core/ppu_interpreter.cpp
    nce_core/ppu_jit_compiler.cpp
    nce_core/spu_interpreter.cpp
    nce_core/spu_jit_compiler.cpp
    # NCE v8 - Ultimate Edition (Multi-tier JIT)
    nce_v8/nce_v8.cpp
    nce_v8/tiered_jit.cpp
    # NCE v8 LLVM Backend (~100-200x speedup)
    nce_v8/llvm_backend.cpp
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
