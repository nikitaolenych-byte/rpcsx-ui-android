Place prebuilt TensorFlow Lite C library files here to enable native NBTC integration.

For each ABI you support, create a subdirectory and copy `libtensorflowlite_c.so` there. Example:

app/src/main/jniLibs/arm64-v8a/libtensorflowlite_c.so
app/src/main/jniLibs/armeabi-v7a/libtensorflowlite_c.so

When building with CMake, enable TFLite support by passing:


If you prefer to provide headers and a lib directory, you can use -DTFLITE_DIR=/path/to/tflite which expects:

/path/to/tflite/include/...
/path/to/tflite/lib/libtensorflowlite_c.so

Notes:
Quick helper:
Use `scripts/download_tflite_prebuilt.sh <url-to-archive> [abi]` to download and extract a prebuilt `libtensorflowlite_c.so` into `app/src/main/jniLibs/<abi>/`.
Example:

```bash
./scripts/download_tflite_prebuilt.sh https://<provider>/tflite-prebuilt.tar.gz arm64-v8a
```

Place real `.so` files (not placeholders) into the ABI folders before building with `-DNBTC_WITH_TFLITE=ON`.
