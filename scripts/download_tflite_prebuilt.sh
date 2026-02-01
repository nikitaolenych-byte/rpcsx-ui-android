#!/usr/bin/env bash
# Simple helper to download TensorFlow Lite C prebuilt library archive and extract libtensorflowlite_c.so into app/src/main/jniLibs
# Usage: ./scripts/download_tflite_prebuilt.sh <url-to-archive> [abi]
# Example: ./scripts/download_tflite_prebuilt.sh https://example.com/tflite-prebuilt.tar.gz arm64-v8a

set -euo pipefail
URL=${1:-}
ABI=${2:-arm64-v8a}
DEST_DIR="$(pwd)/app/src/main/jniLibs/$ABI"

if [ -z "$URL" ]; then
  echo "Usage: $0 <url-to-archive> [abi]"
  exit 1
fi

mkdir -p "$DEST_DIR"
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

echo "Downloading $URL..."
if command -v curl >/dev/null 2>&1; then
  curl -L "$URL" -o "$TMPDIR/archive"
elif command -v wget >/dev/null 2>&1; then
  wget -O "$TMPDIR/archive" "$URL"
else
  echo "curl or wget required to download the archive"
  exit 1
fi

# Try to extract libtensorflowlite_c.so from common archive formats
if file "$TMPDIR/archive" | grep -q "gzip"; then
  tar -xzf "$TMPDIR/archive" -C "$TMPDIR" || true
elif file "$TMPDIR/archive" | grep -q "Zip"; then
  unzip -q "$TMPDIR/archive" -d "$TMPDIR" || true
else
  echo "Unknown archive format; try extracting manually"
fi

# Search for libtensorflowlite_c.so
SO_PATH=$(find "$TMPDIR" -type f -name libtensorflowlite_c.so | head -n1 || true)
if [ -z "$SO_PATH" ]; then
  echo "libtensorflowlite_c.so not found in archive. Please provide an archive that contains the .so for Android ABI $ABI."
  exit 2
fi

echo "Found $SO_PATH — copying to $DEST_DIR"
cp "$SO_PATH" "$DEST_DIR/"
chmod 644 "$DEST_DIR/libtensorflowlite_c.so"

echo "Done. Inserted $DEST_DIR/libtensorflowlite_c.so"

exit 0
