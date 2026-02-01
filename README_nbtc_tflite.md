NBTC TensorFlow Lite integration notes

This repository includes a prototype NBTC engine that can use TensorFlow Lite's C API
when `NBTC_WITH_TFLITE` is enabled in CMake.

To enable native TFLite integration:

1. Place `libtensorflowlite_c.so` for each target ABI into `app/src/main/jniLibs/<abi>/`.
   Example: `app/src/main/jniLibs/arm64-v8a/libtensorflowlite_c.so`.

2. Option A (recommended): Run the helper script to extract a prebuilt `.so` from an archive:

   ```bash
   ./scripts/download_tflite_prebuilt.sh <url-to-archive> arm64-v8a
   ```

3. Configure CMake for a native build with TFLite enabled (from project root):

   ```bash
   cmake -S app/src/main/cpp -B app/build/cmake -DNBTC_WITH_TFLITE=ON -DANDROID_ABI=arm64-v8a
   cmake --build app/build/cmake -- -j$(nproc)
   ```

4. Alternatively provide headers and a lib dir and use `-DTFLITE_DIR=/path/to/tflite`.

Notes:
- The repository includes `NbtcBridge.initializeFromAssets(context, assetPath)` which copies
  a bundled model from `assets/nbtc/model.tflite` into `filesDir` and calls native `initialize()`.
- Placeholder files in `app/src/main/jniLibs/*` exist to indicate where to place real `.so` files.
