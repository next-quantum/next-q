#pragma once

#include <iostream>

#include <musa_runtime.h>

// forward define some utility functions
extern musaDeviceProp getMusaDeviceProperties(int deviceID);
extern bool setKernelFunctionAttributes(
  musaFuncAttributes& attrs, 
  const std::string& name, 
  const void* func
  // int const minThreadsPerBlock
);
extern bool allocateDeviceMemory(void** mem, uint64_t size, const std::string& name);
extern bool allocateDeviceNumaMemory(void** mem, uint64_t* sizePerRegionPitch, uint64_t numRegions, uint64_t sizePerRegion, const std::string& name);
// extern bool allocateManagedMemory(void** mem, uint64_t size, const std::string& name);
extern bool resetDeviceMemory(void** mem, uint64_t size, const std::string& name);
extern void releaseDeviceMemory(void** mem, const std::string& name, bool noError = false);

extern bool copyHostMemoryToDevice(void* dst, const void* src, uint64_t size, const std::string& name);
extern bool copyDeviceMemoryToHost(void* dst, const void* src, uint64_t size, const std::string& name);

extern uint64_t calculateBlocksPerGrid(
  uint64_t const threadsPerGrid, 
  uint64_t const threadsPerBlock
);