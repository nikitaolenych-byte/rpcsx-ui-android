// Lightweight NBTC prototype implementation (optional TFLite integration)
#include "nbtc_engine.h"
#include <android/log.h>
#include <fstream>

#define LOG_TAG "rpcsx-nbtc"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#if defined(HAVE_TFLITE)
#include "tensorflow/lite/c/c_api.h"
// File-scope TFLite pointers for simple lifecycle management
static TfLiteModel* s_model = nullptr;
static TfLiteInterpreterOptions* s_opts = nullptr;
static TfLiteInterpreter* s_interp = nullptr;
#endif

namespace nbtc {

bool Initialize(const std::string& model_path) {
    LOGI("NBTC: Initialize called, model_path=%s", model_path.c_str());
#if defined(HAVE_TFLITE)
    // If built with TFLite support, attempt to initialize interpreter here.
    LOGI("NBTC: Built with TFLite support. Attempting to load model: %s", model_path.c_str());
    if (model_path.empty()) {
        LOGI("NBTC: No model path provided; skipping model load.");
        return true;
    }
    // c_api.h functions
    TfLiteModel* tmodel = nullptr;
    TfLiteInterpreterOptions* opts = nullptr;
    TfLiteInterpreter* interp = nullptr;
    tmodel = TfLiteModelCreateFromFile(model_path.c_str());
    if (!tmodel) {
        LOGE("NBTC: TfLiteModelCreateFromFile failed for %s", model_path.c_str());
        return false;
    }
    opts = TfLiteInterpreterOptionsCreate();
    interp = TfLiteInterpreterCreate(tmodel, opts);
    if (!interp) {
        LOGE("NBTC: TfLiteInterpreterCreate failed");
        TfLiteInterpreterOptionsDelete(opts);
        TfLiteModelDelete(tmodel);
        return false;
    }
    if (TfLiteInterpreterAllocateTensors(interp) != kTfLiteOk) {
        LOGE("NBTC: TfLiteInterpreterAllocateTensors failed");
        TfLiteInterpreterDelete(interp);
        TfLiteInterpreterOptionsDelete(opts);
        TfLiteModelDelete(tmodel);
        return false;
    }
    // Store pointers in file-scope variables so Shutdown can cleanup if needed.
    s_model = tmodel;
    s_opts = opts;
    s_interp = interp;
    LOGI("NBTC: TFLite model loaded and interpreter allocated");
    return true;
#else
    // No TFLite available; remain in prototype fallback mode.
    LOGI("NBTC: TFLite not available, running prototype stub.");
    (void)model_path;
    return true;
#endif
}

bool AnalyzeAndCache(const std::string& game_path) {
    LOGI("NBTC: AnalyzeAndCache called for %s", game_path.c_str());
#if defined(HAVE_TFLITE)
    // If interpreter available, run a minimal inference pass (prototype: zero input)
    if (s_interp) {
        LOGI("NBTC: Running TFLite inference for %s", game_path.c_str());
        TfLiteTensor* input = TfLiteInterpreterGetInputTensor(s_interp, 0);
        if (!input) {
            LOGE("NBTC: failed to get input tensor");
        } else {
            size_t in_bytes = TfLiteTensorByteSize(input);
            std::vector<uint8_t> in_buf(in_bytes);
            // Prototype: fill zeros (real extractor should populate features)
            if (TfLiteTensorCopyFromBuffer(input, in_buf.data(), in_bytes) != kTfLiteOk) {
                LOGE("NBTC: TfLiteTensorCopyFromBuffer failed");
            } else if (TfLiteInterpreterInvoke(s_interp) != kTfLiteOk) {
                LOGE("NBTC: TfLiteInterpreterInvoke failed");
            } else {
                TfLiteTensor* out = TfLiteInterpreterGetOutputTensor(s_interp, 0);
                if (out) {
                    size_t out_bytes = TfLiteTensorByteSize(out);
                    std::vector<uint8_t> out_buf(out_bytes);
                    if (TfLiteTensorCopyToBuffer(out, out_buf.data(), out_bytes) == kTfLiteOk) {
                        LOGI("NBTC: inference output size=%zu", out_bytes);
                    } else {
                        LOGE("NBTC: TfLiteTensorCopyToBuffer failed");
                    }
                } else {
                    LOGE("NBTC: no output tensor");
                }
            }
        }
    } else {
        LOGI("NBTC: interpreter not initialized, skipping TFLite inference");
    }
#endif
#endif
    try {
        std::string marker = game_path + ".nbtc.cache";
        std::ofstream f(marker, std::ios::binary);
        f << "nbtc-prototype";
        f.close();
        LOGI("NBTC: cache written %s", marker.c_str());
        return true;
    } catch (...) {
        LOGE("NBTC: failed to write cache for %s", game_path.c_str());
        return false;
    }
}

bool LoadCacheForGame(const std::string& game_id) {
    LOGI("NBTC: LoadCacheForGame called for %s", game_id.c_str());
    // Prototype: check for marker file
    std::string marker = game_id + ".nbtc.cache";
    std::ifstream f(marker, std::ios::binary);
    bool ok = f.good();
    LOGI("NBTC: cache %s for %s", ok ? "found" : "not found", game_id.c_str());
    return ok;
}

void Shutdown() {
    LOGI("NBTC: Shutdown");
#if defined(HAVE_TFLITE)
    if (s_interp) {
        TfLiteInterpreterDelete(s_interp);
        s_interp = nullptr;
    }
    if (s_opts) {
        TfLiteInterpreterOptionsDelete(s_opts);
        s_opts = nullptr;
    }
    if (s_model) {
        TfLiteModelDelete(s_model);
        s_model = nullptr;
    }
#endif
}

} // namespace nbtc
