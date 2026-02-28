#include <cassert>
#include <cmath>

#include <iostream>
#include <string>

#include <musa_runtime.h>

musaDeviceProp getMusaDeviceProperties(int deviceId) {
  musaDeviceProp deviceProp;
  musaGetDeviceProperties(&deviceProp, deviceId);
  return deviceProp;
}

bool setKernelFunctionAttributes(
  musaFuncAttributes& attrs, 
  const std::string& name, 
  const void* func
) {
  musaError_t status = musaFuncGetAttributes(&attrs, func);
  if (status != musaSuccess) {
    std::cerr
      << "[musaFuncGetAttributes] failed for function: " << name
      << ", with error: " << musaGetErrorString(status) << std::endl;
    return false;
  }
  return true;
}

bool allocateDeviceMemory(void** mem, uint64_t size, const std::string& name) {
  auto status = musaMalloc(mem, size);
  if (status != musaSuccess) {
    std::cerr << "[" << name << "] musa malloc failed with error " << musaGetErrorString(status) << std::endl;

    goto error_entry_point;
  }

  // memset
  status = musaMemset(*mem, 0, size);
  if (status != musaSuccess) {
    std::cerr << "[" << name << "] musa memset failed with error " << musaGetErrorString(status) << std::endl;

    goto error_entry_point;
  }

  return true;

error_entry_point:
  *mem = nullptr;
  return false;
}

bool resetDeviceMemory(void** mem, uint64_t size, const std::string& name) {
  // memset
  auto status = musaMemset(*mem, 0, size);
  if (status != musaSuccess) {
    std::cerr << "[" << name << "] musa memset failed with error " << musaGetErrorString(status) << std::endl;

    goto error_entry_point;
  }

  return true;

error_entry_point:
  *mem = nullptr;
  return false;
}

void releaseDeviceMemory(void** mem, const std::string& name, bool noError) {
  if (*mem == nullptr) {
    return;
  }

  auto status = musaFree(*mem);
  if (status != musaSuccess && !noError) {
    std::cerr
      << "[" << name << "] musa free failed at address: 0x" << *mem
      << ", with error: " << musaGetErrorString(status) << std::endl;
  }
  *mem = nullptr;
}

bool copyHostMemoryToDevice(void* dst, const void* src, uint64_t size, const std::string& name) {
  auto status = musaMemcpy(dst, src, size, musaMemcpyHostToDevice);
  if (status != musaSuccess) {
    std::cerr
      << "[" << name << "] musa mem copy from host at address: 0x" << src
      << " to device at address: 0x" << dst
      << " of size: " << size
      << " failed, with error: " << musaGetErrorString(status) << std::endl;
    return false;
  }

  status = musaDeviceSynchronize();
  if (status != musaSuccess) {
    std::cerr 
      << "[musaDeviceSynchronize] device sync with error " 
      << musaGetErrorString(status) 
      << std::endl;

    return false;
  }

  return true;
}

bool copyDeviceMemoryToHost(void* dst, const void* src, uint64_t size, const std::string& name) {
  auto status = musaMemcpy(dst, src, size, musaMemcpyDeviceToHost);
  if (status != musaSuccess) {
    std::cerr
      << "[" << name << "] musa mem copy from device at address: 0x" << src
      << " to host at address: 0x" << dst
      << " of size: " << size
      << " failed, with error: " << musaGetErrorString(status) << std::endl;
    return false;
  }

  status = musaDeviceSynchronize();
  if (status != musaSuccess) {
    std::cerr 
      << "[musaDeviceSynchronize] device sync with error " 
      << musaGetErrorString(status) 
      << std::endl;

    return false;
  }

  return true;
}

uint64_t calculateBlocksPerGrid(
  uint64_t const threadsPerGrid, 
  uint64_t const threadsPerBlock
) {
  assert(threadsPerGrid >= threadsPerBlock);
  assert(threadsPerBlock > 0);

  uint64_t blocksPerGrid = (threadsPerGrid + threadsPerBlock - 1) / threadsPerBlock;

  return blocksPerGrid;
}
