#!/usr/bin/env bash
set -euo pipefail

APP="net.rpcsx"
OUTDIR="rpcsx_diagnostics_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTDIR"

echo "Collecting logcat (dlopen/UnsatisfiedLinkError/rpcsx)..."
adb logcat -d | grep -E "dlopen|UnsatisfiedLinkError|No implementation found|rpcsx|librpcsx" -i > "$OUTDIR/logcat.txt" || true

echo "Finding process PID for $APP..."
PID=$(adb shell pidof $APP 2>/dev/null | tr -d '\r')
if [ -z "$PID" ]; then
  PID=$(adb shell ps -A | grep $APP | awk '{print $2}' | tr -d '\r' || true)
fi
if [ -n "$PID" ]; then
  echo "PID=$PID" > "$OUTDIR/pid.txt"
  echo "Collecting /proc/$PID/maps..."
  adb shell cat /proc/$PID/maps > "$OUTDIR/maps.txt" || true
else
  echo "No running PID found for $APP; skipping maps." > "$OUTDIR/pid.txt"
fi

# APK path and list libs inside
echo "Getting APK path for $APP..."
APK_PATH=$(adb shell pm path $APP | sed -n 's/package://p' | tr -d '\r')
if [ -n "$APK_PATH" ]; then
  echo "APK: $APK_PATH" > "$OUTDIR/apk_path.txt"
  echo "Pulling APK..."
  adb pull "$APK_PATH" "$OUTDIR/app.apk" > /dev/null || true
  echo "Listing APK contents for librpcsx-android..." > "$OUTDIR/apk_libs.txt"
  unzip -l "$OUTDIR/app.apk" | grep -i 'librpcsx' >> "$OUTDIR/apk_libs.txt" || true
else
  echo "APK not found via pm path" > "$OUTDIR/apk_path.txt"
fi

# Search app data directories for .so files (requires device to allow reading /data; for rooted devices it's fine)
echo "Searching app files/ for .so files..."
DEVICE_SO_PATHS=$(adb shell find /data/data/$APP -type f -name '*.so' 2>/dev/null || true)
if [ -n "$DEVICE_SO_PATHS" ]; then
  echo "$DEVICE_SO_PATHS" > "$OUTDIR/device_so_paths.txt"
  mkdir -p "$OUTDIR/pulled_sos"
  while IFS= read -r p; do
    cleaned=$(echo "$p" | tr -d '\r')
    echo "Pulling $cleaned"
    adb pull "$cleaned" "$OUTDIR/pulled_sos/" || true
  done <<< "$DEVICE_SO_PATHS"
else
  echo "No .so files found under /data/data/$APP" > "$OUTDIR/device_so_paths.txt"
fi

# Also check external files dir (context.getExternalFilesDir(null)) and common tmp locations
echo "Searching external files and tmp dirs for .so files..."
EXT_SO_PATHS=$(adb shell find /sdcard/Android/data/$APP -type f -name '*.so' 2>/dev/null || true)
if [ -n "$EXT_SO_PATHS" ]; then
  echo "$EXT_SO_PATHS" >> "$OUTDIR/device_so_paths.txt"
  while IFS= read -r p; do
    cleaned=$(echo "$p" | tr -d '\r')
    adb pull "$cleaned" "$OUTDIR/pulled_sos/" || true
  done <<< "$EXT_SO_PATHS"
fi

# For each pulled .so, run readelf (host must have readelf)
if command -v readelf >/dev/null 2>&1; then
  echo "Analyzing pulled .so with readelf..."
  mkdir -p "$OUTDIR/readelf"
  for f in "$OUTDIR"/pulled_sos/*.so; do
    [ -e "$f" ] || continue
    base=$(basename "$f")
    readelf -h "$f" > "$OUTDIR/readelf/${base}.readelf_h.txt" || true
    readelf -d "$f" > "$OUTDIR/readelf/${base}.readelf_d.txt" || true
    readelf -s "$f" > "$OUTDIR/readelf/${base}.readelf_s.txt" || true
  done
else
  echo "readelf not found on host; skipping ELF analysis." > "$OUTDIR/readelf_warning.txt"
fi

# Package results
echo "Packaging results into $OUTDIR.zip"
zip -r "$OUTDIR.zip" "$OUTDIR" > /dev/null || true

echo "Done. Upload the $OUTDIR.zip archive or paste contents of $OUTDIR/logcat.txt and $OUTDIR/maps.txt here."

exit 0
