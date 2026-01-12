#!/usr/bin/env bash
set -euo pipefail

# RPCSX Clean Build - повне видалення кешу та бібліотек
# Примусова компіляція з нуля для ARMv9/SVE2 оптимізацій

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  RPCSX Clean Build - Surgical Cache Purge                  ║"
echo "╚════════════════════════════════════════════════════════════╝"

echo "[1/7] Removing Gradle/NDK build outputs…"
rm -rf \
  .gradle \
  app/.gradle \
  app/.cxx \
  app/.externalNativeBuild \
  app/build \
  build

# Shader cache directories
echo "[2/7] Purging shader cache directories…"
find . -type d -name "shader_cache" -exec rm -rf {} + 2>/dev/null || true
find . -type d -name "pipeline_cache" -exec rm -rf {} + 2>/dev/null || true

# Common output dirs (Gradle, CMake, custom scripts)
echo "[3/7] Removing build/bin/obj/libs directories…"
find . -type d \( -name build -o -name bin -o -name obj -o -name libs \) ! -path "./.git/*" -print -prune -exec rm -rf {} + 2>/dev/null || true

echo "[4/7] Removing CMake cache files…"
find . -type f \( -name 'CMakeCache.txt' -o -name 'cmake_install.cmake' -o -name 'compile_commands.json' \) ! -path "./.git/*" -print -delete 2>/dev/null || true
find . -type d -name 'CMakeFiles' ! -path "./.git/*" -exec rm -rf {} + 2>/dev/null || true

# Remove any committed/local prebuilt native binaries if present.
echo "[5/7] Removing prebuilt .so/.a libraries…"
find . -type f \( -name '*.so' -o -name '*.a' \) ! -path "./.git/*" -print -delete 2>/dev/null || true

# Remove APK artifacts
echo "[6/7] Removing APK artifacts…"
find . -type f -name '*.apk' ! -path "./.git/*" -print -delete 2>/dev/null || true

# Remove JIT/NCE code caches
echo "[7/7] Removing JIT/NCE code caches…"
find . -type f \( -name '*.jit' -o -name '*.nce' -o -name '*.ppc_cache' \) ! -path "./.git/*" -print -delete 2>/dev/null || true

echo ""
echo "✅ Clean complete! Ready for fresh ARMv9/SVE2 build."
echo ""
echo "Next steps:"
echo "  ./gradlew assembleRelease"
echo "  # or via GitHub Actions"
