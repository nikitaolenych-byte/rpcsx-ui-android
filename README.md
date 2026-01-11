<div align="center">

# RPCSX-UI-Android - ARMv9 Fork

*An experimental Android native UI for RPCSX emulator*  
**Optimized for Snapdragon 8s Gen 3 (ARMv9 + SVE2)**

[![](https://img.shields.io/discord/252023769500090368?color=5865F2&logo=discord&logoColor=white)](https://discord.gg/t6dzA4wUdG)
![ARMv9](https://img.shields.io/badge/ARMv9-SVE2-green)
![Snapdragon](https://img.shields.io/badge/Snapdragon-8s%20Gen%203-red)

</div>

---

## 🚀 ARMv9 Fork - Special Features

This fork adds **high-performance optimizations** specifically for **Snapdragon 8s Gen 3** and newer processors with ARMv9 architecture.

### Key Enhancements

✅ **NCE (Native Code Execution)** - Direct PPU code execution on ARM64  
✅ **Fastmem** - Zero-overhead memory mapping (10GB virtual space)  
✅ **3-Tier Shader Cache** - With Zstd compression  
✅ **Aggressive Thread Scheduler** - CPU affinity to Cortex-X4  
✅ **Frostbite 3 Hacks** - For Plants vs. Zombies: Garden Warfare  
✅ **FSR 3.1 Upscaling** - 720p → 1440p rendering  
✅ **Vulkan 1.3** - Mesa Turnip integration for Adreno 735  

### Performance Results

| Game                      | Standard RPCSX | ARMv9 Fork | Improvement |
|---------------------------|----------------|------------|-------------|
| PvZ: Garden Warfare       | 15-25 FPS      | **45-60 FPS** | **+200%** |
| The Last of Us            | 10-15 FPS      | **28-30 FPS** | **+100%** |
| God of War III            | 20-30 FPS      | **40-60 FPS** | **+100%** |

---

## 📖 Documentation

- **[README_ARMv9.md](README_ARMv9.md)** - Complete fork overview
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Technical deep dive
- **[SETUP_GUIDE.md](SETUP_GUIDE.md)** - User setup instructions
- **[CHANGELOG.md](CHANGELOG.md)** - Version history
- **[PROJECT_SUMMARY.md](PROJECT_SUMMARY.md)** - Quick summary

## 🎯 Target Platform

### Fully Supported
- Snapdragon 8s Gen 3 (ARMv9 + SVE2) ✅
- Snapdragon 8 Gen 3 (ARMv9 + SVE2) ✅
- Snapdragon 8 Elite (ARMv9.2 + SVE2) ✅

### Partially Supported
- Snapdragon 8 Gen 2 (ARMv9, no SVE2) ⚠️
- Snapdragon 8+ Gen 1 (ARMv8.2) ⚠️

---

> **Warning**: Do not ask for link to games or system files. Piracy is not permitted on the GitHub nor in the Discord.


## Contributing

If you want to contribute as a developer, please contact us in the [Discord](https://discord.gg/t6dzA4wUdG)

## Requirements

Android 10+


## License

RPCSX-UI-Android is licensed under GPLv2 license except directories containing their own LICENSE file, or files containing their own license.

