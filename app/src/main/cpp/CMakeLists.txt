cmake_minimum_required(VERSION 3.22)
project("rpcsx-android" CXX)

# Basic configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ARMv9-A optimization flags (simple and stable)
set(COMMON_FLAGS "-O3 -march=armv9-a -mtune=cortex-x4 -ffast-math")
set(OPT_FLAGS "${COMMON_FLAGS} -ftree-vectorize -funroll-loops -finline-functions")

# Platform-specific settings
if(CMAKE_SYSTEM_NAME STREQUAL "Android" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    message(STATUS "Building for Android ARMv9-A")
    set(OPT_FLAGS "${OPT_FLAGS} -flto=thin")
endif()

# Main library with minimal dependencies
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
    nce_jit/ppc_decoder.cpp
    nce_jit/arm64_emitter.cpp
    nce_core/nce_standalone.cpp
    nce_core/nce_native.cpp
    nce_core/thread_pool.cpp
    nce_core/game_mode_android.cpp
    nce_core/ppu_interpreter.cpp
    nce_core/ppu_jit_compiler.cpp
    nce_core/spu_interpreter.cpp
    nce_core/spu_jit_compiler.cpp
    nce_v8/nce_v8.cpp
    nce_v8/tiered_jit.cpp
    nce_v8/llvm_backend.cpp
    plt_hook.cpp
    ppu_interceptor.cpp
    nce_hook.cpp
)

# Apply optimization flags
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${OPT_FLAGS}>
)

# Link with system libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    android
    log
    z
)

# Include directories
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

message(STATUS "==================================================")
message(STATUS "RPCSX Android Build Configuration")
message(STATUS "==================================================")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Optimization: ${OPT_FLAGS}")
message(STATUS "==================================================")
