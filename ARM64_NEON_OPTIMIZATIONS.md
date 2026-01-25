# ARM64 NEON Optimizations for RPCSX

## Version 1.5.0-neon

This release contains **real LLVM-level optimizations** for ARM64 platforms, specifically targeting Snapdragon 8s Gen 3 and similar ARMv9-A processors.

## Summary of Changes

### 1. SPULLVMRecompiler.cpp - ARM64 ROTQBY Implementation

**Location:** `app/src/main/cpp/rpcsx/rpcs3/Emu/Cell/SPULLVMRecompiler.cpp`

Added native ARM64 NEON implementation for the `exec_rotqby` function that was previously only implemented for x86:

```cpp
#elif defined(ARCH_ARM64)
	// ARM64 NEON implementation of quadword rotate by bytes
	static uint8x16_t exec_rotqby_neon(uint8x16_t a, u8 b)
	{
		const u8 shift = 16 - (b & 0xf);
		// Uses vextq_u8 for efficient byte rotation
		switch (shift) { ... }
	}
```

**Benefit:** Uses ARM64 NEON `vextq_u8` instruction which efficiently rotates 128-bit vectors by extracting bytes from concatenated registers.

### 2. CPUTranslator.h - New ARM64 NEON Intrinsics

**Location:** `app/src/main/cpp/rpcsx/rpcs3/Emu/CPU/CPUTranslator.h`

Added new LLVM intrinsic wrappers for ARM64:

| Function | LLVM Intrinsic | Purpose |
|----------|----------------|---------|
| `vabs()` | `llvm.abs.v4i32` | Vector absolute value (integer) |
| `vfma()` | `llvm.fma.v4f32` | Vector FMA → NEON FMLA instruction |
| `vrndn()` | `llvm.aarch64.neon.frintn.v4f32` | Vector round to nearest |
| `vrndz()` | `llvm.trunc.v4f32` | Vector truncate toward zero |
| `vcvtns()` | `llvm.aarch64.neon.fcvtns.v4i32.v4f32` | Float→signed int with rounding |
| `vcvtnu()` | `llvm.aarch64.neon.fcvtnu.v4i32.v4f32` | Float→unsigned int with rounding |
| `vpadd_f32()` | `llvm.aarch64.neon.faddp.v2f32` | Pairwise add (for reductions) |
| `vqadd()` | `llvm.sadd.sat.v4i32` | Saturating add (signed) |
| `vqsub()` | `llvm.ssub.sat.v4i32` | Saturating subtract (signed) |

### 3. CPUTranslator.h - ARM64 vperm2b Implementation

**Location:** `app/src/main/cpp/rpcsx/rpcs3/Emu/CPU/CPUTranslator.h`

Replaced x86 AVX-512 `VPERM2B` with ARM64 NEON `TBL2` instruction:

```cpp
#if defined(ARCH_ARM64)
	// Use ARM64 NEON TBL2 intrinsic for efficient 2-register table lookup
	result.value = m_ir->CreateCall(
		get_intrinsic<u8[16]>(llvm::Intrinsic::aarch64_neon_tbl2), 
		{data0, data1, index_masked});
```

**Benefit:** `TBL2` is the native ARM64 equivalent of x86's `VPERM2B`, selecting bytes from two 128-bit registers based on an index.

## Pre-existing ARM64 Support

The following NEON intrinsics were already implemented:

- `llvm.aarch64.neon.frecpe.v4f32` - Reciprocal estimate
- `llvm.aarch64.neon.frsqrte.v4f32` - Reciprocal square root estimate
- `llvm.aarch64.neon.fmax.v4f32` - Float max
- `llvm.aarch64.neon.fmin.v4f32` - Float min
- `llvm.aarch64.neon.tbl1` - Single-register table lookup (PSHUFB equivalent)
- `llvm.aarch64.neon.fcvtns/fcvtzs` - Float-to-int conversions

## Performance Impact

These optimizations target:

1. **SPU Vector Rotations** - Critical for Cell SPU emulation
2. **FMA Operations** - Used heavily in PS3 graphics and physics
3. **Byte Permutation** - Essential for texture decoding and memory operations
4. **SIMD Arithmetic** - Saturating add/sub for audio and video processing

## Configuration

No additional settings needed - these optimizations are enabled automatically when building for ARM64 targets.

## Build

```bash
export RX_VERSION="1.5.0-neon"
./gradlew assembleRelease
```

## Verification

To verify NEON optimizations are active:

1. Check build log for `ARCH_ARM64` defines
2. Look for `arm_neon.h` include in SPULLVMRecompiler.cpp
3. Inspect generated assembly for NEON instructions (FMLA, TBL, EXT)
