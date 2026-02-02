# LLVM-PS3 Configuration for RPCSX Android
# Heterogeneous Compute JIT with PS3-Specific Auto-Vectorization
# and ARM SVE2/SME Intrinsic Mapping (LLVM-HCJIT-PS3VEC)
#
# Include this file in your CMakeLists.txt:
#   include(${CMAKE_SOURCE_DIR}/third_party/llvm-ps3/llvm-ps3-config.cmake)

cmake_minimum_required(VERSION 3.18)

# =============================================================================
# Options
# =============================================================================

option(LLVM_PS3_ENABLE "Enable PS3-specific LLVM patches" ON)
option(LLVM_PS3_AGGRESSIVE_VECTORIZE "Enable aggressive vectorization for PS3 patterns" ON)
option(LLVM_PS3_HETEROGENEOUS "Enable heterogeneous CPU/GPU compute dispatch" OFF)
option(LLVM_PS3_SPIRV_CODEGEN "Enable SPIR-V code generation for GPU offload" OFF)
option(LLVM_PS3_USE_PREBUILT "Use prebuilt LLVM with patches applied" ON)
option(LLVM_PS3_ARM_SVE2 "Prefer ARM SVE2 intrinsics (ARMv9+)" ON)
option(LLVM_PS3_ARM_SME "Enable ARM SME for matrix operations (ARMv9.2+)" OFF)

# =============================================================================
# Paths
# =============================================================================

set(LLVM_PS3_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Path to LLVM-PS3 directory")
set(LLVM_PS3_PATCHES_DIR "${LLVM_PS3_DIR}/patches" CACHE PATH "Path to LLVM patches")
set(LLVM_PS3_PREBUILT_DIR "${LLVM_PS3_DIR}/prebuilt" CACHE PATH "Path to prebuilt LLVM")

# =============================================================================
# Compiler Flags
# =============================================================================

set(LLVM_PS3_CXX_FLAGS "")
set(LLVM_PS3_LINKER_FLAGS "")

if(LLVM_PS3_ENABLE)
    # Enable PS3 pattern matching
    list(APPEND LLVM_PS3_CXX_FLAGS "-mllvm" "-ps3-pattern-match")
    
    if(LLVM_PS3_AGGRESSIVE_VECTORIZE)
        list(APPEND LLVM_PS3_CXX_FLAGS "-mllvm" "-ps3-vectorize-aggressive")
    endif()
    
    if(LLVM_PS3_HETEROGENEOUS)
        list(APPEND LLVM_PS3_CXX_FLAGS "-mllvm" "-enable-heterogeneous")
    endif()
    
    if(LLVM_PS3_ARM_SVE2)
        list(APPEND LLVM_PS3_CXX_FLAGS "-mllvm" "-arm-ps3-enable-sve2")
        # Require ARMv9 for SVE2
        if(ANDROID_ABI STREQUAL "arm64-v8a")
            list(APPEND LLVM_PS3_CXX_FLAGS "-march=armv9-a+sve2")
        endif()
    endif()
    
    if(LLVM_PS3_ARM_SME)
        list(APPEND LLVM_PS3_CXX_FLAGS "-mllvm" "-arm-ps3-enable-sme")
        if(ANDROID_ABI STREQUAL "arm64-v8a")
            list(APPEND LLVM_PS3_CXX_FLAGS "-march=armv9.2-a+sme")
        endif()
    endif()
    
    if(LLVM_PS3_SPIRV_CODEGEN)
        list(APPEND LLVM_PS3_CXX_FLAGS "-mllvm" "-generate-spirv")
    endif()
endif()

# =============================================================================
# Definitions
# =============================================================================

set(LLVM_PS3_DEFINITIONS "")

if(LLVM_PS3_ENABLE)
    list(APPEND LLVM_PS3_DEFINITIONS "LLVM_PS3_ENABLED=1")
endif()

if(LLVM_PS3_AGGRESSIVE_VECTORIZE)
    list(APPEND LLVM_PS3_DEFINITIONS "LLVM_PS3_AGGRESSIVE_VECTORIZE=1")
endif()

if(LLVM_PS3_HETEROGENEOUS)
    list(APPEND LLVM_PS3_DEFINITIONS "LLVM_PS3_HETEROGENEOUS=1")
endif()

if(LLVM_PS3_ARM_SVE2)
    list(APPEND LLVM_PS3_DEFINITIONS "LLVM_PS3_ARM_SVE2=1")
endif()

if(LLVM_PS3_ARM_SME)
    list(APPEND LLVM_PS3_DEFINITIONS "LLVM_PS3_ARM_SME=1")
endif()

# =============================================================================
# Apply to Target
# =============================================================================

function(llvm_ps3_apply_to_target TARGET_NAME)
    if(NOT LLVM_PS3_ENABLE)
        return()
    endif()
    
    # Apply compile flags
    foreach(FLAG ${LLVM_PS3_CXX_FLAGS})
        target_compile_options(${TARGET_NAME} PRIVATE ${FLAG})
    endforeach()
    
    # Apply definitions
    foreach(DEF ${LLVM_PS3_DEFINITIONS})
        target_compile_definitions(${TARGET_NAME} PRIVATE ${DEF})
    endforeach()
    
    message(STATUS "LLVM-PS3: Applied to target ${TARGET_NAME}")
endfunction()

# =============================================================================
# Patch Application Helper
# =============================================================================

function(llvm_ps3_apply_patches LLVM_SOURCE_DIR)
    if(NOT EXISTS "${LLVM_SOURCE_DIR}")
        message(FATAL_ERROR "LLVM source directory not found: ${LLVM_SOURCE_DIR}")
    endif()
    
    set(PATCHES
        "${LLVM_PS3_PATCHES_DIR}/0001-cell-be-pattern-recognition.patch"
        "${LLVM_PS3_PATCHES_DIR}/0002-arm-autovectorization.patch"
        "${LLVM_PS3_PATCHES_DIR}/0003-heterogeneous-pipeline.patch"
    )
    
    foreach(PATCH ${PATCHES})
        if(EXISTS "${PATCH}")
            execute_process(
                COMMAND git apply --check "${PATCH}"
                WORKING_DIRECTORY "${LLVM_SOURCE_DIR}"
                RESULT_VARIABLE PATCH_CHECK_RESULT
                OUTPUT_QUIET
                ERROR_QUIET
            )
            
            if(PATCH_CHECK_RESULT EQUAL 0)
                execute_process(
                    COMMAND git apply "${PATCH}"
                    WORKING_DIRECTORY "${LLVM_SOURCE_DIR}"
                    RESULT_VARIABLE PATCH_APPLY_RESULT
                )
                
                if(PATCH_APPLY_RESULT EQUAL 0)
                    message(STATUS "LLVM-PS3: Applied patch ${PATCH}")
                else()
                    message(WARNING "LLVM-PS3: Failed to apply patch ${PATCH}")
                endif()
            else()
                message(STATUS "LLVM-PS3: Patch already applied or incompatible: ${PATCH}")
            endif()
        else()
            message(WARNING "LLVM-PS3: Patch not found: ${PATCH}")
        endif()
    endforeach()
endfunction()

# =============================================================================
# Prebuilt LLVM Detection
# =============================================================================

function(llvm_ps3_find_prebuilt OUT_LLVM_DIR)
    set(SEARCH_PATHS
        "${LLVM_PS3_PREBUILT_DIR}/${ANDROID_ABI}"
        "${LLVM_PS3_PREBUILT_DIR}"
        "$ENV{LLVM_PS3_PREBUILT}"
    )
    
    foreach(SEARCH_PATH ${SEARCH_PATHS})
        if(EXISTS "${SEARCH_PATH}/lib/cmake/llvm")
            set(${OUT_LLVM_DIR} "${SEARCH_PATH}" PARENT_SCOPE)
            message(STATUS "LLVM-PS3: Found prebuilt at ${SEARCH_PATH}")
            return()
        endif()
    endforeach()
    
    set(${OUT_LLVM_DIR} "" PARENT_SCOPE)
    message(STATUS "LLVM-PS3: No prebuilt found, will use system LLVM")
endfunction()

# =============================================================================
# Runtime Configuration
# =============================================================================

# Configuration structure for runtime settings
set(LLVM_PS3_RUNTIME_CONFIG_HEADER "
// Auto-generated LLVM-PS3 configuration
#pragma once

namespace llvm_ps3 {

struct Config {
    bool patternMatchEnabled = ${LLVM_PS3_ENABLE};
    bool aggressiveVectorize = ${LLVM_PS3_AGGRESSIVE_VECTORIZE};
    bool heterogeneousEnabled = ${LLVM_PS3_HETEROGENEOUS};
    bool spirvCodegen = ${LLVM_PS3_SPIRV_CODEGEN};
    bool armSve2Enabled = ${LLVM_PS3_ARM_SVE2};
    bool armSmeEnabled = ${LLVM_PS3_ARM_SME};
};

inline Config& getConfig() {
    static Config config;
    return config;
}

} // namespace llvm_ps3
")

# Generate config header if needed
function(llvm_ps3_generate_config_header OUTPUT_DIR)
    file(WRITE "${OUTPUT_DIR}/llvm_ps3_config.h" "${LLVM_PS3_RUNTIME_CONFIG_HEADER}")
    message(STATUS "LLVM-PS3: Generated config header at ${OUTPUT_DIR}/llvm_ps3_config.h")
endfunction()

# =============================================================================
# Status Output
# =============================================================================

message(STATUS "========================================")
message(STATUS "LLVM-PS3 Configuration")
message(STATUS "========================================")
message(STATUS "  LLVM_PS3_ENABLE:              ${LLVM_PS3_ENABLE}")
message(STATUS "  LLVM_PS3_AGGRESSIVE_VECTORIZE: ${LLVM_PS3_AGGRESSIVE_VECTORIZE}")
message(STATUS "  LLVM_PS3_HETEROGENEOUS:       ${LLVM_PS3_HETEROGENEOUS}")
message(STATUS "  LLVM_PS3_SPIRV_CODEGEN:       ${LLVM_PS3_SPIRV_CODEGEN}")
message(STATUS "  LLVM_PS3_ARM_SVE2:            ${LLVM_PS3_ARM_SVE2}")
message(STATUS "  LLVM_PS3_ARM_SME:             ${LLVM_PS3_ARM_SME}")
message(STATUS "  LLVM_PS3_USE_PREBUILT:        ${LLVM_PS3_USE_PREBUILT}")
message(STATUS "========================================")
