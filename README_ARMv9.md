# RPCSX ARMv9 Fork - Snapdragon 8s Gen 3 Optimization

Форк емулятора RPCSX для Android, оптимізований під архітектуру ARMv9 (Snapdragon 8s Gen 3) для запуску надважких PS3 ігор на рівні потужного ПК.

## 🎯 Цільова платформа

- **Процесор**: Qualcomm Snapdragon 8s Gen 3
- **Архітектура**: ARMv9-A з підтримкою SVE2
- **Ядра**: 
  - 1x Cortex-X4 @ 3.0 GHz (Prime)
  - 3x Cortex-A720 @ 2.8 GHz (Performance)
  - 4x Cortex-A720 @ 2.5 GHz (Efficiency)
- **GPU**: Adreno 735 (Vulkan 1.3, OpenGL ES 3.2)
- **RAM**: LPDDR5X (до 16GB)
- **Сторідж**: UFS 4.0

## 🚀 Ключові оптимізації

### 1. NCE (Native Code Execution)
Технологія прямого виконання PPU коду PS3 на ядрах ARM64 без інтерпретації:
- Прив'язка PPU потоків до Cortex-X4 (Prime core)
- JIT компіляція PowerPC → ARM64
- SVE2 векторні інструкції для SPU емуляції
- Zero-overhead memory translation

**Файли**: `nce_engine.cpp`, `nce_engine.h`

### 2. Fastmem (Direct Memory Mapping)
Миттєвий обмін даними між CPU та LPDDR5X RAM:
- memfd-based direct mapping (10GB віртуальної пам'яті)
- Transparent hugepages для зменшення TLB miss
- Hardware prefetching інструкції
- Lock у RAM для уникнення swap

**Файли**: `fastmem_mapper.cpp`, `fastmem_mapper.h`

### 3. Трирівневий Shader Cache
Система кешування шейдерів з Zstd компресією:
- **L1**: In-memory cache (512MB) - найшвидший доступ
- **L2**: Persistent cache на UFS 4.0 - швидкий SSD
- **L3**: Compressed archive з Zstd - економія місця
- Async shader compilation для усунення статерів

**Файли**: `shader_cache_manager.cpp`, `shader_cache_manager.h`

### 4. Vulkan 1.3 + FSR 3.1
Графічний пайплайн з апскейлінгом:
- Mesa Turnip driver для Adreno 735
- Vulkan 1.3 з async compute
- FSR 3.1: рендеринг 720p → апскейл до 1440p
- Pipeline cache для швидкого старту

**Файли**: `vulkan_renderer.h`, `fsr31/fsr31.h`

### 5. Агресивний Thread Scheduler
Планувальник потоків з фіксацією на конкретних ядрах:
- PPU на Cortex-X4 (CPU 7)
- SPU на Cortex-A720 Performance (CPU 4-6)
- Renderer на Performance cores
- Вимкнення енергозбереження Android
- SCHED_FIFO з максимальним пріоритетом

**Файли**: `thread_scheduler.cpp`, `thread_scheduler.h`

### 6. Frostbite 3 Engine Hacks
Специфічні оптимізації для Plants vs. Zombies: Garden Warfare:
- Write Color Buffers (виправлення transparency bugs)
- MLLE mode (покращена SPU емуляція)
- Terrain LOD patching (усунення flickering)
- Shader complexity reduction
- Particle system optimization

**Файли**: `frostbite_hacks.cpp`, `frostbite_hacks.h`

## 📊 Очікувана продуктивність

### Plants vs. Zombies: Garden Warfare
- **Розширення**: 720p → 1440p (FSR 3.1)
- **Цільовий FPS**: 30-60 (стабільний)
- **Покращення**: Усунення графічних багів, плавна анімація

### The Last of Us
- **Розширення**: 720p → 1080p
- **Цільовий FPS**: 30 (стабільний)
- **Покращення**: Підвищена деталізація текстур

### God of War III
- **Розширення**: 720p → 1440p
- **Цільовий FPS**: 40-60
- **Покращення**: Плавні бої, швидке завантаження

## 🔧 Збірка проєкту

### Вимоги
- Android Studio Ladybug (2024.3.1+)
- Android NDK 29.0.13113456
- CMake 3.31.6
- Kotlin 2.1.10

### Інструкція зі збірки

1. **Клонування репозиторію**
```bash
git clone https://github.com/RPCSX/rpcsx-ui-android.git
cd rpcsx-ui-android
```

2. **Налаштування NDK**
Переконайтеся, що встановлено NDK 29+:
```bash
sdkmanager "ndk;29.0.13113456"
```

3. **Збірка через Gradle**
```bash
./gradlew assembleRelease
```

4. **Встановлення на пристрій**
```bash
adb install -r app/build/outputs/apk/release/rpcsx-release.apk
```

## 📱 Використання

### Ініціалізація оптимізацій

```kotlin
import net.rpcsx.RPCSX

// Увімкнення ARMv9 оптимізацій
val cacheDir = context.cacheDir.absolutePath
val titleId = "BLUS31270" // Plants vs. Zombies: Garden Warfare

RPCSX.initializeOptimizations(cacheDir, titleId)
```

### Вимкнення при виході

```kotlin
override fun onDestroy() {
    super.onDestroy()
    RPCSX.shutdownOptimizations()
}
```

## 🎮 Підтримувані ігри

### Повністю оптимізовані (з Frostbite хаками)
- ✅ Plants vs. Zombies: Garden Warfare
- ✅ Battlefield 3
- ✅ Battlefield 4

### Частково оптимізовані
- ⚠️ The Last of Us (може вимагати 30 FPS lock)
- ⚠️ God of War III (можливі просідання в складних сценах)
- ⚠️ Uncharted 2/3

### Не рекомендовано
- ❌ Gran Turismo 6 (проблеми з physics)
- ❌ Metal Gear Solid 4 (нестабільна емуляція)

## 🛠️ Технічні деталі

### Флаги компіляції
```cmake
-march=armv9-a+sve2     # ARMv9 з SVE2 підтримкою
-O3                      # Максимальна оптимізація
-ffast-math              # Швидка математика
-ftree-vectorize         # Автоматична векторизація
-funroll-loops           # Розгортання циклів
-flto=thin               # Link-time optimization
```

### CPU Affinity Map
```
CPU 0-3: Efficiency cores → Background tasks
CPU 4-6: Performance cores → SPU, Renderer
CPU 7: Prime core (X4) → PPU (main thread)
```

### Memory Layout
```
0x0000000000000000 - 0x00000001FFFFFFFF: Guest RAM (8GB)
0x0000000200000000 - 0x000000027FFFFFFF: VRAM (2GB)
```

## 📝 Конфігурація

### Налаштування в BuildConfig
```kotlin
BuildConfig.EnableARMv9Optimizations  // true
BuildConfig.OptimizationTarget        // "Snapdragon 8s Gen 3 (ARMv9+SVE2)"
```

### Runtime налаштування
```kotlin
// Зміна цільового FPS
frostbite::OptimizeFramePacing(30)  // 30 або 60

// Зміна якості FSR
fsr::InitializeFSR(1280, 720, 1920, 1080, FSRQuality::BALANCED)
```

## 🔍 Діагностика

### Логи

Всі модулі виводять детальні логи через Android Logcat:

```bash
adb logcat -s RPCSX-NCE RPCSX-Fastmem RPCSX-ShaderCache RPCSX-Scheduler RPCSX-Frostbite
```

### Статистика кешу

```cpp
// Виклик з C++
rpcsx::shaders::PrintCacheStats();

// Вивід:
// L1 hits: 12453
// L2 hits: 342
// L3 hits: 89
// Cache misses: 12
// Hit rate: 99.1%
```

## 🤝 Внесок у проєкт

Вітаємо pull requests! Якщо ви маєте ідеї щодо покращення:

1. Fork репозиторію
2. Створіть feature branch (`git checkout -b feature/amazing-optimization`)
3. Commit зміни (`git commit -m 'Add amazing optimization'`)
4. Push до branch (`git push origin feature/amazing-optimization`)
5. Відкрийте Pull Request

## 📄 Ліцензія

Цей проєкт є форком [RPCSX](https://github.com/RPCSX/rpcsx) з додатковими оптимізаціями.

Оригінальна ліцензія: MIT (див. [LICENSE](LICENSE))

## 🙏 Подяки

- **RPCSX Team** - за оригінальний емулятор
- **Mesa/Turnip** - за Vulkan драйвер для Adreno
- **AMD** - за FSR технологію
- **Qualcomm** - за Snapdragon 8s Gen 3 документацію

## 📞 Контакти

- **Issues**: https://github.com/RPCSX/rpcsx-ui-android/issues
- **Discord**: [RPCSX Community](https://discord.gg/rpcsx)

---

**Увага**: Цей емулятор призначений тільки для запуску легально придбаних ігор. Піратство не підтримується.

**Disclaimer**: Потрібні оригінальні файли прошивки PS3 (firmware) для роботи емулятора.
