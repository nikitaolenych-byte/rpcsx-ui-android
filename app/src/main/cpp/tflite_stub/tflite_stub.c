#include "tensorflow/lite/c/c_api.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct TfLiteModel { int dummy; };
struct TfLiteInterpreterOptions { int dummy; };
struct TfLiteInterpreter { int dummy; };
struct TfLiteTensor { size_t byte_size; void* buf; };

TfLiteModel* TfLiteModelCreateFromFile(const char* filename) {
    (void)filename;
    TfLiteModel* m = (TfLiteModel*)malloc(sizeof(TfLiteModel));
    return m;
}
void TfLiteModelDelete(TfLiteModel* model) { free(model); }

TfLiteInterpreterOptions* TfLiteInterpreterOptionsCreate() {
    return (TfLiteInterpreterOptions*)malloc(sizeof(TfLiteInterpreterOptions));
}
void TfLiteInterpreterOptionsDelete(TfLiteInterpreterOptions* options) { free(options); }

TfLiteInterpreter* TfLiteInterpreterCreate(const TfLiteModel* model, TfLiteInterpreterOptions* options) {
    (void)model; (void)options;
    TfLiteInterpreter* it = (TfLiteInterpreter*)malloc(sizeof(TfLiteInterpreter));
    return it;
}
void TfLiteInterpreterDelete(TfLiteInterpreter* interpreter) { free(interpreter); }

TfLiteStatus TfLiteInterpreterAllocateTensors(TfLiteInterpreter* interpreter) {
    (void)interpreter; return kTfLiteOk; }
TfLiteStatus TfLiteInterpreterInvoke(TfLiteInterpreter* interpreter) { (void)interpreter; return kTfLiteOk; }

TfLiteTensor* TfLiteInterpreterGetInputTensor(TfLiteInterpreter* interpreter, int index) {
    (void)interpreter; (void)index;
    TfLiteTensor* t = (TfLiteTensor*)malloc(sizeof(TfLiteTensor));
    t->byte_size = 16;
    t->buf = malloc(t->byte_size);
    memset(t->buf, 0, t->byte_size);
    return t;
}
TfLiteTensor* TfLiteInterpreterGetOutputTensor(TfLiteInterpreter* interpreter, int index) {
    (void)interpreter; (void)index;
    TfLiteTensor* t = (TfLiteTensor*)malloc(sizeof(TfLiteTensor));
    t->byte_size = 8;
    t->buf = malloc(t->byte_size);
    memset(t->buf, 0xff, t->byte_size);
    return t;
}

size_t TfLiteTensorByteSize(const TfLiteTensor* t) { return t ? t->byte_size : 0; }
TfLiteStatus TfLiteTensorCopyFromBuffer(TfLiteTensor* t, const void* data, size_t bytes) {
    if (!t || !t->buf) return kTfLiteError;
    size_t to_copy = bytes < t->byte_size ? bytes : t->byte_size;
    memcpy(t->buf, data, to_copy);
    return kTfLiteOk;
}
TfLiteStatus TfLiteTensorCopyToBuffer(const TfLiteTensor* t, void* out_data, size_t bytes) {
    if (!t || !t->buf) return kTfLiteError;
    size_t to_copy = bytes < t->byte_size ? bytes : t->byte_size;
    memcpy(out_data, t->buf, to_copy);
    return kTfLiteOk;
}
