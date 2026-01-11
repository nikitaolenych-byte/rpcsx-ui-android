#!/usr/bin/env bash
set -euo pipefail

# Clean build outputs so the next build is guaranteed "from source".
# Safe to run repeatedly.

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "[clean] Removing Gradle/NDK build outputs…"
rm -rf \
  .gradle \
  app/.gradle \
  app/.cxx \
  app/.externalNativeBuild

# Common output dirs (Gradle, CMake, custom scripts)
# We avoid touching directories under .git.
find . -type d \( -name build -o -name bin -o -name obj \) -print -prune -exec rm -rf {} +

echo "[clean] Removing CMake cache files…"
find . -type f \( -name 'CMakeCache.txt' -o -name 'cmake_install.cmake' \) -print -delete

# Remove any committed/local prebuilt native binaries if present.
# (Repo policy: build everything from source.)
echo "[clean] Removing any local prebuilt .so/.a (if any)…"
find . -type f \( -name '*.so' -o -name '*.a' \) -print -delete || true

echo "[clean] Done. Next: ./gradlew assembleRelease"
