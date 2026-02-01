#ifndef TFLITE_C_API_STUB_H
#define TFLITE_C_API_STUB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef enum TfLiteStatus {
  kTfLiteOk = 0,
  kTfLiteError = 1,
} TfLiteStatus;

typedef struct TfLiteModel TfLiteModel;
typedef struct TfLiteInterpreterOptions TfLiteInterpreterOptions;
typedef struct TfLiteInterpreter TfLiteInterpreter;
typedef struct TfLiteTensor TfLiteTensor;

// Model
TfLiteModel* TfLiteModelCreateFromFile(const char* filename);
void TfLiteModelDelete(TfLiteModel* model);

// Options
TfLiteInterpreterOptions* TfLiteInterpreterOptionsCreate();
void TfLiteInterpreterOptionsDelete(TfLiteInterpreterOptions* options);

// Interpreter
TfLiteInterpreter* TfLiteInterpreterCreate(const TfLiteModel* model, TfLiteInterpreterOptions* options);
void TfLiteInterpreterDelete(TfLiteInterpreter* interpreter);
TfLiteStatus TfLiteInterpreterAllocateTensors(TfLiteInterpreter* interpreter);
TfLiteStatus TfLiteInterpreterInvoke(TfLiteInterpreter* interpreter);

TfLiteTensor* TfLiteInterpreterGetInputTensor(TfLiteInterpreter* interpreter, int index);
TfLiteTensor* TfLiteInterpreterGetOutputTensor(TfLiteInterpreter* interpreter, int index);

// Tensor helpers
size_t TfLiteTensorByteSize(const TfLiteTensor* t);
TfLiteStatus TfLiteTensorCopyFromBuffer(TfLiteTensor* t, const void* data, size_t bytes);
TfLiteStatus TfLiteTensorCopyToBuffer(const TfLiteTensor* t, void* out_data, size_t bytes);

#ifdef __cplusplus
}
#endif

#endif // TFLITE_C_API_STUB_H
