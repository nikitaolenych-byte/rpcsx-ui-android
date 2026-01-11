#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "[build] Updating submodules (if any)…"
git submodule update --init --recursive

echo "[build] Clean from-source build outputs…"
"$repo_root/scripts/clean_from_source.sh"

# Android Gradle uses CMake+Ninja under the hood for externalNativeBuild.
# We keep configuration consistent with CI (RelWithDebInfo for native).
echo "[build] Building native via CMake+Ninja (RelWithDebInfo)…"
./gradlew :app:externalNativeBuildRelWithDebInfo --no-daemon

echo "[build] Assembling release APK…"
./gradlew :app:assembleRelease --no-daemon

echo "[build] APK output: app/build/outputs/apk/release/"
