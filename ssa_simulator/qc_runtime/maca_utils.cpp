#include <cassert>
#include <cmath>

#include <iostream>
#include <string>

#include <mc_runtime.h>

mcDeviceProp_t getMacaDeviceProperties(int deviceId) {
  mcDeviceProp_t deviceProp;
  mcGetDeviceProperties(&deviceProp, deviceId);
  return deviceProp;
}

bool setKernelFunctionAttributes(
  mcFuncAttributes& attrs, 
  const std::string& name, 
  const void* func
) {
  mcError_t status = mcFuncGetAttributes(&attrs, func);
  if (status != mcSuccess) {
    std::cerr
      << "[mcFuncGetAttributes] failed for function: " << name
      << ", with error: " << mcGetErrorString(status) << std::endl;
    return false;
  }
  return true;
}

bool allocateDeviceMemory(void** mem, uint64_t size, const std::string& name) {
  auto status = mcMalloc(mem, size);
  if (status != mcSuccess) {
    std::cerr << "[" << name << "] mc malloc failed with error " << mcGetErrorString(status) << std::endl;

    goto error_entry_point;
  }

  // memset
  status = mcMemset(*mem, 0, size);
  if (status != mcSuccess) {
    std::cerr << "[" << name << "] mc memset failed with error " << mcGetErrorString(status) << std::endl;

    goto error_entry_point;
  }

  return true;

error_entry_point:
  *mem = nullptr;
  return false;
}

bool resetDeviceMemory(void** mem, uint64_t size, const std::string& name) {
  // memset
  auto status = mcMemset(*mem, 0, size);
  if (status != mcSuccess) {
    std::cerr << "[" << name << "] mc memset failed with error " << mcGetErrorString(status) << std::endl;

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

  auto status = mcFree(*mem);
  if (status != mcSuccess && !noError) {
    std::cerr
      << "[" << name << "] mc free failed at address: 0x" << *mem
      << ", with error: " << mcGetErrorString(status) << std::endl;
  }
  *mem = nullptr;
}

bool copyHostMemoryToDevice(void* dst, const void* src, uint64_t size, const std::string& name) {
  auto status = mcMemcpyHtoD(reinterpret_cast<mcDeviceptr_t>(dst), src, size);
  if (status != mcSuccess) {
    std::cerr
      << "[" << name << "] mc mem copy from host at address: 0x" << src
      << " to device at address: 0x" << dst
      << " of size: " << size
      << " failed, with error: " << mcGetErrorString(status) << std::endl;
    return false;
  }

  status = mcDeviceSynchronize();
  if (status != mcSuccess) {
    std::cerr 
      << "[mcDeviceSynchronize] device sync with error " 
      << mcGetErrorString(status) 
      << std::endl;

    return false;
  }

  return true;
}

bool copyDeviceMemoryToHost(void* dst, const void* src, uint64_t size, const std::string& name) {
  auto status = mcMemcpyDtoH(dst, reinterpret_cast<mcDeviceptr_t>(const_cast<void*>(src)), size);
  if (status != mcSuccess) {
    std::cerr
      << "[" << name << "] mc mem copy from device at address: 0x" << src
      << " to host at address: 0x" << dst
      << " of size: " << size
      << " failed, with error: " << mcGetErrorString(status) << std::endl;
    return false;
  }

  status = mcDeviceSynchronize();
  if (status != mcSuccess) {
    std::cerr 
      << "[mcDeviceSynchronize] device sync with error " 
      << mcGetErrorString(status) 
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