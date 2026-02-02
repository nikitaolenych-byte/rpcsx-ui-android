# LLVM-HCJIT-PS3VEC

## LLVM Heterogeneous Compute JIT with PS3-Specific Auto-Vectorization and ARM SVE2/SME Intrinsic Mapping

This directory contains patches and configuration for a custom LLVM build optimized for PS3 emulation on ARM devices.

### Components

1. **Cell BE Pattern Recognition Patch** (`patches/0001-cell-be-pattern-recognition.patch`)
   - Modifies LLVM IR generator to recognize typical PowerPC/SPU code blocks
   - Marks blocks for specialized optimization passes

2. **ARM Auto-Vectorization Patch** (`patches/0002-arm-autovectorization.patch`)
   - Extends LLVM auto-vectorizer for ARM NEON/SVE2/SME intrinsics
   - CPU-specific intrinsic selection based on runtime detection

3. **Heterogeneous Pipeline Patch** (`patches/0003-heterogeneous-pipeline.patch`)
   - Enables dispatching compute blocks to GPU via SPIR-V
   - Supports Vulkan compute shaders for physics/post-processing

### Build Options

| Option | Description |
|--------|-------------|
| `LLVM_PS3_ENABLE` | Enable PS3-specific LLVM patches |
| `LLVM_PS3_AGGRESSIVE_VECTORIZE` | Enable aggressive vectorization |
| `LLVM_PS3_HETEROGENEOUS` | Enable heterogeneous compute dispatch |
| `LLVM_PS3_SPIRV_CODEGEN` | Enable SPIR-V code generation for GPU |

### Compiler Flags

```
-mllvm -ps3-vectorize-aggressive
-mllvm -enable-heterogeneous
-mllvm -ps3-pattern-match
-mllvm -arm-sve2-prefer
```

### Integration

Include `llvm-ps3-config.cmake` in your build:

```cmake
include(${CMAKE_SOURCE_DIR}/third_party/llvm-ps3/llvm-ps3-config.cmake)
```
