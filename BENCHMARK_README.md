# RPCSX Performance Benchmark - AnTuTu Compatible

A comprehensive performance benchmark suite for RPCSX PS3 Emulation on Android, compatible with AnTuTu benchmark scoring methodology.

## Features

### Test Categories

1. **GPU Graphics Test** (30% weight)
   - RSX Graphics Engine throughput
   - Draw call performance (FPS)
   - Polygon rendering speed
   - GPU command queue efficiency

2. **Memory Bandwidth Test** (25% weight)
   - DDR5/LPDDR5X sequential access
   - L1/L2 cache performance
   - Main memory bandwidth (GB/s)
   - Cache-line efficiency

3. **CPU Performance Test** (25% weight)
   - Cortex-X4 floating point (GFLOPS)
   - Single-threaded sustained performance
   - Sine/Cosine computation (sincos workload)
   - ARMv9 vectorization

4. **NCE JIT Performance Test** (20% weight)
   - PowerPC → ARM64 translation
   - PS3 PPU emulation performance
   - Code block compilation speed
   - Native Code Execution throughput

## Scoring System

### Overall Score (0-100)

```
Overall Score = (GPU × 0.30) + (Memory × 0.25) + (CPU × 0.25) + (NCE × 0.20)
```

Each test is normalized to 0-100 scale based on reference hardware (Snapdragon 8 Gen 3).

### Reference Scores

| Device Class | Score Range | Examples |
|--------------|-------------|----------|
| **Flagship** | 80-100 | Snapdragon 8 Gen 3, Dimensity 9300 |
| **High-End** | 70-79 | Snapdragon 8 Gen 2, Dimensity 9200 |
| **Mid-Range** | 50-69 | Snapdragon 7 Gen 2, Dimensity 8200 |
| **Budget** | 0-49 | Snapdragon 6 Gen 1, Helio G99 |

## Usage

### Running the Benchmark

```java
// Create benchmark instance
RPCSXBenchmark benchmark = new RPCSXBenchmark(rpcsx);

// Set listener for progress updates
benchmark.setBenchmarkListener(new RPCSXBenchmark.BenchmarkListener() {
    @Override
    public void onBenchmarkProgress(String testName, int progress) {
        // Update UI with progress (testName, progress 0-100)
        updateProgressBar(testName, progress);
    }
    
    @Override
    public void onBenchmarkComplete(RPCSXBenchmark.BenchmarkResult result) {
        // Display results
        showResults(
            String.format("GPU: %.1f FPS", result.gpuScore),
            String.format("Memory: %.1f GB/s", result.memoryScore),
            String.format("CPU: %.1f GFLOPS", result.cpuScore),
            String.format("Overall: %.1f", result.overallScore)
        );
    }
    
    @Override
    public void onBenchmarkError(String error) {
        // Handle error
        showError(error);
    }
});

// Run benchmark
benchmark.runFullBenchmark();
```

### Individual Tests

```java
// Run only GPU test
float fps = rpcsx.runGraphicsPerformanceTest(5000);

// Run only memory test
float bandwidth = rpcsx.runMemoryPerformanceTest();

// Run only CPU test
float gflops = rpcsx.runCPUPerformanceTest();
```

## Benchmark Results Interpretation

### GPU Score (FPS)
- **≥ 60 FPS**: Excellent - Full PS3 graphics emulation possible
- **40-60 FPS**: Good - Most games playable
- **20-40 FPS**: Fair - Some slowdowns expected
- **< 20 FPS**: Poor - Significant performance issues

### Memory Bandwidth (GB/s)
- **≥ 100 GB/s**: Excellent - Good for texture streaming
- **80-100 GB/s**: Good - Acceptable for most games
- **60-80 GB/s**: Fair - May have memory bandwidth bottleneck
- **< 60 GB/s**: Poor - Likely memory-bound performance

### CPU Performance (GFLOPS)
- **≥ 10 GFLOPS**: Excellent - Good FPU performance
- **7-10 GFLOPS**: Good - Sufficient for emulation
- **5-7 GFLOPS**: Fair - Some computational overhead
- **< 5 GFLOPS**: Poor - May bottleneck emulation

### NCE JIT (Iterations/sec)
- **≥ 100 iter/sec**: Excellent - Very fast code translation
- **50-100 iter/sec**: Good - Good JIT performance
- **20-50 iter/sec**: Fair - Acceptable JIT speed
- **< 20 iter/sec**: Poor - Slow code translation

## Device Requirements

- **Minimum API Level**: Android 10 (API 29)
- **Minimum RAM**: 4 GB
- **CPU Architecture**: ARM64-v8a
- **Vulkan Version**: 1.3 or higher
- **Required Features**: NEON
- **Optional Features**: SVE2 (ARM Scalable Vector Extension)

## Compatibility with AnTuTu

This benchmark follows AnTuTu methodology:
- ✓ Multiple test categories
- ✓ Weighted scoring system
- ✓ Device classification
- ✓ Reference baselines
- ✓ Score normalization (0-100)
- ✓ Results comparison database

The scoring is compatible with standard AnTuTu benchmark results for performance comparison.

## Performance Tips for Better Scores

1. **GPU Optimization**
   - Enable Vulkan validation layers in debug mode
   - Use appropriate texture formats (SRGB for colors)
   - Enable pipeline caching for faster compilation

2. **Memory Optimization**
   - Ensure sufficient free RAM (at least 2GB for benchmark)
   - Close background applications
   - Enable memory fastmem optimization in RPCSX

3. **CPU Optimization**
   - Use Cortex-X4 cores (avoid Cortex-A55 efficiency cores)
   - Disable CPU frequency scaling during benchmark
   - Use high-performance CPU frequency governor

4. **NCE Optimization**
   - Keep shader cache warm
   - Pre-compile frequently used code blocks
   - Enable aggressive JIT optimizations

## Files

- `RPCSXBenchmark.java` - Main benchmark class
- `benchmark_config.xml` - Configuration and reference data
- `BENCHMARK_README.md` - This documentation
- Tests integrated in `native-lib.cpp` via JNI

## Future Enhancements

- [ ] Multi-threaded workload tests
- [ ] GPU texture compression benchmarks
- [ ] Memory alignment effects analysis
- [ ] Branch prediction analysis
- [ ] Cache miss profiling
- [ ] Power consumption measurements
- [ ] Thermal throttling analysis
- [ ] Comparison with other emulators

## References

- [AnTuTu Benchmark](https://www.antutu.com/)
- [Vulkan Benchmarking Best Practices](https://www.khronos.org/vulkan/)
- [ARM Performance Analysis Guide](https://developer.arm.com/documentation/)
- [RPCSX Project](https://github.com/RPCSX/rpcsx)

---

**RPCSX Performance Benchmark v1.0**  
PS3 Emulation Performance Measurement for Android ARMv9
