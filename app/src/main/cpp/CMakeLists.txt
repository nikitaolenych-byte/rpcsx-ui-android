cmake_minimum_required(VERSION 3.22)
project("rpcsx-android" CXX)

# Basic configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ARMv9-A optimization flags - as separate list items
set(OPT_FLAGS
    -O3
    -march=armv9-a
    -mtune=cortex-x4
    -ffast-math
    -ftree-vectorize
    -funroll-loops
    -finline-functions
)

# Platform-specific settings
if(CMAKE_SYSTEM_NAME STREQUAL "Android" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    message(STATUS "Building for Android ARMv9-A")
    list(APPEND OPT_FLAGS -flto=thin)
endif()

# Try to find libadrenotools (driver loading)
find_library(ADRENOTOOLS_LIB adrenotools)
if(ADRENOTOOLS_LIB)
    message(STATUS "Found libadrenotools: ${ADRENOTOOLS_LIB}")
    set(HAVE_ADRENOTOOLS TRUE)
else()
    message(WARNING "libadrenotools not found, driver loading disabled")
    set(HAVE_ADRENOTOOLS FALSE)
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
    nce_core/profiler.cpp
    nce_core/simd_utils.cpp
    nce_core/stubs.cpp
    nce_core/llvm_optimized_ppu.cpp
    nce_core/llvm_optimized_spu.cpp
    nce_v8/nce_v8.cpp
    nce_v8/tiered_jit.cpp
    nce_v8/llvm_backend.cpp
    plt_hook.cpp
    ppu_interceptor.cpp
    nce_hook.cpp
    rpcsx/android/src/cutscene_bridge.cpp
    rpcsx/android/src/nbtc_bridge.cpp
    nbtc_engine/nbtc_engine.cpp
)

# If NBTC_WITH_TFLITE requested but no system or jniLibs lib found, compile a small stub
if(NBTC_WITH_TFLITE)
    if(NOT DEFINED HAVE_TFLITE)
        message(STATUS "NBTC: No system TFLite libs found — compiling stub implementation to allow builds")
        target_sources(${CMAKE_PROJECT_NAME} PRIVATE tflite_stub/tflite_stub.c)
        target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tflite_stub)
        target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_TFLITE=1)
    endif()
endif()

# Apply optimization flags
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${OPT_FLAGS}>
)

# Find Vulkan library
find_library(VULKAN_LIB vulkan)
if(VULKAN_LIB)
    message(STATUS "Found Vulkan: ${VULKAN_LIB}")
else()
    message(WARNING "Vulkan not found, will try to link with system vulkan")
    set(VULKAN_LIB vulkan)
endif()

# Link with system libraries and optional libadrenotools
target_link_libraries(${CMAKE_PROJECT_NAME}
    android
    log
    z
    ${VULKAN_LIB}
)

if(HAVE_ADRENOTOOLS)
    target_link_libraries(${CMAKE_PROJECT_NAME} ${ADRENOTOOLS_LIB})
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_ADRENOTOOLS=1)
endif()

# Include directories
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Optional TensorFlow Lite integration for NBTC
option(NBTC_WITH_TFLITE "Enable TensorFlow Lite support for NBTC (requires TFLITE_DIR or jniLibs)" OFF)
if(NBTC_WITH_TFLITE)
    # Prefer explicit TFLITE_DIR if provided
    if(DEFINED TFLITE_DIR)
        message(STATUS "NBTC: TFLite enabled, using TFLITE_DIR=${TFLITE_DIR}")
        target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${TFLITE_DIR}/include)
        if(EXISTS "${TFLITE_DIR}/lib/libtensorflowlite_c.so")
            target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ${TFLITE_DIR}/lib/libtensorflowlite_c.so)
            target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_TFLITE=1)
        else()
            message(WARNING "NBTC: libtensorflowlite_c.so not found under TFLITE_DIR/lib; will try jniLibs fallback")
        endif()
    endif()

    # If not set via TFLITE_DIR, check for prebuilt .so in app's jniLibs for current ABI
    if(NOT DEFINED HAVE_TFLITE)
        if(DEFINED ANDROID_ABI)
            set(JNILIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}")
            set(JNILIB_PATH "${JNILIB_DIR}/libtensorflowlite_c.so")
            if(EXISTS "${JNILIB_PATH}")
                message(STATUS "NBTC: Found libtensorflowlite_c.so in jniLibs (${JNILIB_PATH}) — linking")
                target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ${JNILIB_PATH})
                target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_TFLITE=1)
                # Also add include hint if a common include exists next to jniLibs
                if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/include")
                    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/include)
                endif()
            else()
                message(WARNING "NBTC: libtensorflowlite_c.so not found in jniLibs for ABI ${ANDROID_ABI} (${JNILIB_PATH})")
            endif()
        else()
            message(WARNING "NBTC: ANDROID_ABI not defined; cannot probe jniLibs for prebuilt TFLite library")
        endif()
    endif()

    if(NOT DEFINED HAVE_TFLITE)
        message(WARNING "NBTC: TensorFlow Lite not found — NBTC will run in prototype mode")
    endif()
else()
    message(STATUS "NBTC: TensorFlow Lite integration disabled (NBTC_WITH_TFLITE=OFF)")
endif()

message(STATUS "==================================================")
message(STATUS "RPCSX Android Build Configuration")
message(STATUS "==================================================")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Optimization: ${OPT_FLAGS}")
message(STATUS "Adrenotools: ${HAVE_ADRENOTOOLS}")
message(STATUS "==================================================")
