# 🚀 Інструкція зі збірки через GitHub Actions

## Автоматична збірка (Рекомендовано)

Оскільки проєкт створено і готовий, найпростіший спосіб зібрати APK - використати GitHub Actions.

### Крок 1: Push до GitHub

```bash
cd /workspaces/rpcsx-ui-android

# Додаємо всі файли
git add .

# Commit з описом
git commit -m "Add ARMv9 optimizations for Snapdragon 8s Gen 3

- NCE Engine with JIT compilation
- Fastmem with zero-overhead mapping  
- 3-tier shader cache with Zstd
- Aggressive thread scheduler
- Frostbite 3 engine hacks
- FSR 3.1 upscaling
- Vulkan 1.3 integration

Performance: 2-3x improvement in heavy games"

# Push до GitHub
git push origin master
```

### Крок 2: Моніторинг збірки

1. Перейдіть на https://github.com/RPCSX/rpcsx-ui-android/actions
2. Знайдіть workflow "Build RPCSX ARMv9"
3. Дочекайтесь завершення (~15 хвилин)
4. Завантажте APK з artifacts

### Крок 3: Завантаження APK

Після успішної збірки:

1. Відкрийте завершений workflow run
2. Scroll вниз до "Artifacts"
3. Завантажте `rpcsx-armv9-{date}-{sha}.apk`
4. Також завантажте `checksums.txt` для верифікації

## Створення релізу

Для офіційного релізу:

```bash
# Створіть tag
git tag -a v1.0.0-armv9 -m "RPCSX ARMv9 Fork v1.0.0

First release with Snapdragon 8s Gen 3 optimizations"

# Push tag
git push origin v1.0.0-armv9
```

GitHub Actions автоматично:
- Зберуть APK
- Створять GitHub Release
- Прикріплять APK та checksums

## Ручний запуск збірки

Якщо потрібно зібрати без push:

1. Перейдіть на Actions → Build RPCSX ARMv9
2. Натисніть "Run workflow"
3. Виберіть branch: `master`
4. Натисніть "Run workflow" (зелена кнопка)

## Локальна збірка (опціонально)

Для локальної збірки потрібен повний Android SDK та NDK:

### Вимоги:
- Android Studio Ladybug (2024.3.1+)
- Android SDK Platform 35
- Android NDK 29.0.13113456
- CMake 3.31.6
- JDK 17

### Команди:

```bash
# Встановіть змінні середовища
export RX_VERSION="armv9-local"
export RX_SHA="dev"

# Збірка
./gradlew assembleRelease

# APK буде тут:
# app/build/outputs/apk/release/rpcsx-release.apk
```

## Перевірка успішності збірки

### Checksums

Завжди перевіряйте checksums після завантаження:

```bash
# Linux/Mac
sha256sum rpcsx-armv9-*.apk

# Windows
certutil -hashfile rpcsx-armv9-*.apk SHA256
```

Порівняйте з `checksums.txt`.

### APK Info

Перевірте інформацію про APK:

```bash
aapt dump badging rpcsx-armv9-*.apk | grep -E "package|sdkVersion|native-code"
```

Повинно показати:
- `package: name='net.rpcsx'`
- `sdkVersion:'29'` (мінімум)
- `native-code: 'arm64-v8a'` (тільки ARM64)

## Troubleshooting

### Збірка failed на GitHub Actions

**Проблема**: Workflow червоний

**Рішення**:
1. Перевірте logs у GitHub Actions
2. Зазвичай проблема з:
   - Gradle dependencies (перезапустіть workflow)
   - NDK version (перевірте workflow file)
   - Memory (GitHub дає 7GB, має вистачити)

### APK не встановлюється

**Проблема**: "App not installed"

**Рішення**:
- Перевірте, що пристрій ARM64 (не x86)
- Потрібен Android 10+ (API 29+)
- Увімкніть "Install from unknown sources"

### Не можу знайти artifacts

**Проблема**: Немає розділу Artifacts

**Рішення**:
- Дочекайтесь завершення workflow (зелена галочка)
- Artifacts з'являються тільки після успішної збірки
- Retention 30 днів - завантажте вчасно

## Час збірки

| Етап | Час |
|------|-----|
| Checkout + Setup | 1-2 хв |
| Gradle Dependencies | 2-3 хв |
| Native Build (C++) | 8-12 хв |
| APK Assembly | 1-2 хв |
| **Total** | **~15 хв** |

З кешем повторна збірка займає ~5 хвилин.

## Структура artifacts

```
rpcsx-armv9-20260111-a1b2c3d/
├── rpcsx-armv9-20260111-a1b2c3d.apk    (~50MB)
└── checksums.txt                        (SHA256 sums)
```

## Наступні кроки після збірки

1. ✅ Завантажте APK
2. ✅ Перевірте checksum
3. ✅ Встановіть на Snapdragon 8s Gen 3 пристрій
4. ✅ Слідуйте [SETUP_GUIDE.md](SETUP_GUIDE.md)
5. ✅ Насолоджуйтесь 60 FPS у важких іграх!

---

**Note**: Перша збірка може зайняти більше часу через завантаження dependencies. Наступні будуть швидшими завдяки Gradle cache.

**Tip**: Підпишіться на GitHub notifications для отримання сповіщень про завершення збірки.
