// Stub libcuda.so.1 — satisfies dlopen so FFmpeg doesn't spam
// "Cannot load libcuda.so.1". Returns 0 devices, forcing software decode.
#include <stddef.h>
#include <stdint.h>

typedef uint32_t CUresult;
enum { CUDA_SUCCESS = 0 };
typedef void *CUdevice;
typedef void *CUcontext;
typedef void *CUmodule;
typedef void *CUfunction;
typedef void *CUstream;
typedef void *CUevent;

CUresult cuInit(unsigned int) { return CUDA_SUCCESS; }
CUresult cuDriverGetVersion(int *v) { *v = 12040; return CUDA_SUCCESS; }
CUresult cuDeviceGetCount(int *c) { *c = 0; return CUDA_SUCCESS; }
CUresult cuDeviceGet(CUdevice *, int) { return CUDA_SUCCESS; }
CUresult cuDeviceGetName(char *, int, CUdevice) { return CUDA_SUCCESS; }
CUresult cuDeviceComputeCapability(int *, int *, CUdevice) { return CUDA_SUCCESS; }
CUresult cuDevicePrimaryCtxRetain(CUcontext *, CUdevice) { return CUDA_SUCCESS; }
CUresult cuDevicePrimaryCtxRelease(CUdevice) { return CUDA_SUCCESS; }
CUresult cuCtxCreate(CUcontext *, uint32_t, CUdevice) { return CUDA_SUCCESS; }
CUresult cuCtxDestroy(CUcontext) { return CUDA_SUCCESS; }
CUresult cuCtxPushCurrent(CUcontext) { return CUDA_SUCCESS; }
CUresult cuCtxPopCurrent(CUcontext *) { return CUDA_SUCCESS; }
CUresult cuCtxSynchronize(void) { return CUDA_SUCCESS; }
CUresult cuModuleLoad(CUmodule *, const char *) { return CUDA_SUCCESS; }
CUresult cuModuleGetFunction(CUfunction *, CUmodule, const char *) { return CUDA_SUCCESS; }
CUresult cuMemAlloc(void *, size_t) { return CUDA_SUCCESS; }
CUresult cuMemFree(void *) { return CUDA_SUCCESS; }
CUresult cuMemcpyHtoD(void *, const void *, size_t) { return CUDA_SUCCESS; }
CUresult cuMemcpyDtoH(void *, const void *, size_t) { return CUDA_SUCCESS; }
CUresult cuMemcpyDtoD(void *, const void *, size_t) { return CUDA_SUCCESS; }
CUresult cuStreamCreate(CUstream *, uint32_t) { return CUDA_SUCCESS; }
CUresult cuStreamDestroy(CUstream) { return CUDA_SUCCESS; }
CUresult cuStreamSynchronize(CUstream) { return CUDA_SUCCESS; }
CUresult cuEventCreate(CUevent *, uint32_t) { return CUDA_SUCCESS; }
CUresult cuEventDestroy(CUevent) { return CUDA_SUCCESS; }
CUresult cuEventRecord(CUevent, CUstream) { return CUDA_SUCCESS; }
CUresult cuEventSynchronize(CUevent) { return CUDA_SUCCESS; }
CUresult cuEventElapsedTime(float *ms, CUevent, CUevent) { *ms = 0.0f; return CUDA_SUCCESS; }
CUresult cuLaunchKernel(CUfunction, uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,CUstream,void**,void**) { return CUDA_SUCCESS; }
CUresult cuParamSetSize(CUfunction, uint32_t) { return CUDA_SUCCESS; }
CUresult cuParamSeti(CUfunction, int, uint32_t) { return CUDA_SUCCESS; }
CUresult cuParamSetv(CUfunction, int, void*, uint32_t) { return CUDA_SUCCESS; }
CUresult cuFuncSetBlockShape(CUfunction, int, int, int) { return CUDA_SUCCESS; }
CUresult cuFuncSetSharedSize(CUfunction, uint32_t) { return CUDA_SUCCESS; }
