#include <cstdint>

#include <device_types.h>

#include "mu_complex.muh"

namespace QC_V2 {
namespace MOORETHREAD_GPU {

__device__ inline uint32_t nth_cleared(uint32_t const index, uint32_t const target_power) {
  uint32_t const mask = target_power - 1;
  uint32_t const not_mask = ~mask;

  return (index & mask) | ((index & not_mask) << 1);
}

__global__ void apply_gate_one(
  muFloatComplex* stateVector,

  muFloatComplex const m00,
  muFloatComplex const m01,
  muFloatComplex const m10,
  muFloatComplex const m11,
  
  uint32_t const target,
  uint32_t const controlMask
) {
  unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;

  unsigned int C = 1 << target;
  unsigned int X = nth_cleared(gid, C); // 0
  unsigned int Y = X + C;               // 1

  auto const v0 = stateVector[X];
  auto const v1 = stateVector[Y];

  if ((X & controlMask) == controlMask)
  {
    stateVector[X].x = (v0.x * m00.x - v0.y * m00.y) + (v1.x * m01.x - v1.y * m01.y);
    stateVector[X].y = (v0.x * m00.y + v0.y * m00.x) + (v1.x * m01.y + v1.y * m01.x);

    stateVector[Y].x = (v0.x * m10.x - v0.y * m10.y) + (v1.x * m11.x - v1.y * m11.y);
    stateVector[Y].y = (v0.x * m10.y + v0.y * m10.x) + (v1.x * m11.y + v1.y * m11.x);
  }
}

__global__ void find_probability_of_outcome_zero(
  muFloatComplex* stateVector,
  float* output,
  uint32_t const target
) {
  unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;

  unsigned int C = 1 << target;
  unsigned int X = nth_cleared(gid, C); // 0

  float const re = stateVector[X].x;
  float const im = stateVector[X].y;

  float value = re * re + im * im;

  // 使用基于块的归约
  __shared__ float shared_sum[1024];
  int tid = threadIdx.x;
  shared_sum[tid] = value;
  __syncthreads();

  for (int offset = blockDim.x / 2; offset > 0; offset >>= 1) {
    if (tid < offset) {
      shared_sum[tid] += shared_sum[tid + offset];
    }
    __syncthreads();
  }

  if (tid == 0) {
    atomicAdd(output, shared_sum[0]);
  }
}

__global__ void collapse_to_outcome_scale(
  muFloatComplex* stateVector,
  uint32_t const target,
  float const scale,
  bool const one, // whether set outcome one?
  bool const noNorm // whether skip normalization
) {
  unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;

  unsigned int C = 1 << target;
  unsigned int X = nth_cleared(gid, C); // 0
  unsigned int Y = X + C;               // 1

  // in case of noNorm, exchange zero and 1 states
  if (noNorm) {
    auto const xCopy = stateVector[X];
    auto const yCopy = stateVector[Y];
    
    stateVector[X].x = one ? 0.0f : yCopy.x;
    stateVector[X].y = one ? 0.0f : yCopy.y;

    stateVector[Y].x = one ? xCopy.x : 0.0f;
    stateVector[Y].y = one ? xCopy.y : 0.0f;
  }

  stateVector[X].x = one ? 0.0f : stateVector[X].x * scale;
  stateVector[X].y = one ? 0.0f : stateVector[X].y * scale;

  stateVector[Y].x = one ? stateVector[Y].x * scale : 0.0f;
  stateVector[Y].y = one ? stateVector[Y].y * scale : 0.0f;
}

__global__ void store_state_vector(
  muFloatComplex* stateVectorBuffer,
  muFloatComplex const* stateVector
) {
  unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;

  stateVectorBuffer[gid] = stateVector[gid];
}

__global__ void dot_state_vector(
  muFloatComplex const* stateVector,
  muFloatComplex const* stateVectorBuffer,
  float* output
) {
  unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;

  float const a1 = stateVectorBuffer[gid].x;
  float const b1 = -stateVectorBuffer[gid].y;
  float const a2 = stateVector[gid].x;
  float const b2 = stateVector[gid].y;
  float value = a1 * a2 - b1 * b2;

  // 使用基于块的归约
  __shared__ float shared_sum[1024];
  int tid = threadIdx.x;
  shared_sum[tid] = value;
  __syncthreads();

  for (int offset = blockDim.x / 2; offset > 0; offset >>= 1) {
    if (tid < offset) {
      shared_sum[tid] += shared_sum[tid + offset];
    }
    __syncthreads();
  }

  if (tid == 0) {
    atomicAdd(output, shared_sum[0]);
  }
}

__global__ void norm_state_vector(
  muFloatComplex const* stateVector,
  float* output
) {
  unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;

  float const re = stateVector[gid].x;
  float const im = stateVector[gid].y;

  float value = re * re + im * im;

  // 使用基于块的归约
  __shared__ float shared_sum[1024];
  int tid = threadIdx.x;
  shared_sum[tid] = value;
  __syncthreads();

  for (int offset = blockDim.x / 2; offset > 0; offset >>= 1) {
    if (tid < offset) {
      shared_sum[tid] += shared_sum[tid + offset];
    }
    __syncthreads();
  }

  if (tid == 0) {
    atomicAdd(output, shared_sum[0]);
  }
}

}
}