# GitHub Actions CI/CD

Цей проєкт використовує GitHub Actions для автоматичної збірки та перевірки коду.

## 📱 RPCSX ARMv9 Build Automation

Повна автоматизація збірки RPCSX Android Core для архітектури ARMv9 з максимальними оптимізаціями.

### Основні компоненти

- **Workflow**: `build-armv9.yml` - Основний workflow для автоматичної збірки
- **Скрипти**: 
  - `manage-llvm-and-patches.sh` - Управління LLVM та патчами
  - `build-armv9-local.sh` - Локальна збірка з тими ж параметрами

### Сценарії активації

1. **Щотижневі**: Кожного понеділка о 00:00 UTC
2. **На push**: При змінах в C++ коді та конфіграції
3. **Ручний запуск**: Через GitHub UI з вибором параметрів

### Оптимізації

- ✅ LLVM v19+ (автоматичне завантаження)
- ✅ ARMv9-A архітектура з SVE2
- ✅ Флаги: `-O3 -march=armv9-a -mtune=cortex-x4`
- ✅ LTO (Link Time Optimization)
- ✅ Fastmem, FSR 3.1, Game Mode

### Виходи

Кожна збірка генерує:
- **APK файл** (100-150 MB)
- **Native libraries** (.so файли)
- **Архів з усім** (150-200 MB)
- **Звіт про збірку** (build-info.txt)

### Використання

```bash
# Запуск локально
./scripts/manage-llvm-and-patches.sh full-setup
./scripts/build-armv9-local.sh

# Запуск через GitHub Actions
# Actions → Build RPCSX ARMv9 → Run workflow
```

Детальна документація: [GITHUB_ACTIONS_GUIDE.md](../GITHUB_ACTIONS_GUIDE.md)

## Workflows

### 1. Build RPCSX ARMv9 (`build-armv9.yml`)

Автоматична збірка оптимізованого APK для Snapdragon 8s Gen 3.

**Triggers**:
- Push до master/main
- Pull requests
- Manual dispatch

**Steps**:
1. Checkout коду з submodules
2. Налаштування JDK 17 та Android SDK
3. Встановлення NDK 29.0.13113456
4. Кешування Gradle залежностей
5. Збірка Release APK з ARMv9 оптимізаціями
6. Підпис APK (якщо є keystore)
7. Генерація checksums
8. Upload artifacts
9. Створення GitHub Release (при tag)

**Artifacts**:
- `rpcsx-armv9-{version}-{sha}.apk`
- `checksums.txt`

**Retention**: 30 днів

### 2. Code Quality Checks (`quality-check.yml`)

Перевірка якості коду.

**Checks**:
- Kotlin linter (ktlint)
- Code style (spotless)
- C++ formatting (clang-format)
- Lines of code statistics

## Використання

### Запуск збірки вручну

1. Перейдіть на вкладку "Actions"
2. Виберіть "Build RPCSX ARMv9"
3. Натисніть "Run workflow"
4. Виберіть branch
5. Натисніть "Run workflow"

### Створення релізу

1. Створіть та push tag:
```bash
git tag v1.0.0-armv9
git push origin v1.0.0-armv9
```

2. GitHub Actions автоматично:
   - Зберуть APK
   - Створять GitHub Release
   - Прикріплять APK та checksums

## Secrets Configuration

Для підпису APK налаштуйте наступні secrets у Settings → Secrets:

- `KEYSTORE_FILE` - Base64-encoded keystore файл
- `KEYSTORE_ALIAS` - Alias ключа
- `KEYSTORE_PASSWORD` - Пароль keystore

### Як закодувати keystore в Base64:

```bash
base64 -w 0 your-keystore.jks > keystore-base64.txt
```

Потім скопіюйте вміст `keystore-base64.txt` у GitHub Secret `KEYSTORE_FILE`.

## Build Configuration

Збірка використовує такі змінні оточення:

- `RX_VERSION` - Версія у форматі `armv9-YYYYMMDD`
- `RX_SHA` - Короткий git commit hash
- `ANDROID_NDK_HOME` - Шлях до NDK

## Compiler Flags

```
-march=armv9-a+sve2
-O3
-flto=thin
-ffast-math
-ftree-vectorize
-funroll-loops
```

## Cache Strategy

Кешуються:
- Gradle wrapper
- Gradle dependencies
- Maven artifacts

Це значно прискорює наступні збірки (з ~15 хв до ~5 хв).

## Artifact Naming

```
rpcsx-armv9-{YYYYMMDD}-{git-sha}.apk
```

Приклад: `rpcsx-armv9-20260111-a1b2c3d.apk`

## Troubleshooting

### Build fails з NDK помилкою

Перевірте, що NDK версія `29.0.13113456` встановлена коректно.

### Out of memory під час збірки

GitHub Actions надає 7GB RAM. Якщо недостатньо, можна:
1. Зменшити parallel builds у gradle.properties
2. Використати self-hosted runner з більше RAM

### Checksum verification fails

Перевірте, що APK не був змінений після збірки.

## Local Testing

Протестуйте workflow локально за допомогою [act](https://github.com/nektos/act):

```bash
# Встановіть act
brew install act  # macOS
# або
curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash

# Запустіть workflow
act push -j build
```

## Performance

Типовий час збірки:
- Cold build (без cache): ~15 хвилин
- Warm build (з cache): ~5 хвилин

## Notifications

Статус збірки можна моніторити через:
- GitHub Actions UI
- Email notifications (налаштовуються в Settings)
- Slack/Discord webhooks (опціонально)

---

**Note**: Для production релізів завжди використовуйте signed APK з proper keystore.
