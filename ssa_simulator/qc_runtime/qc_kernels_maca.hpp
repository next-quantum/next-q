#pragma once

#include <cstdint>

#include <mc_runtime.h>

#include "maca_complex.hpp"

namespace QC_V2 {
namespace METAX_GPU {

__device__ inline uint32_t nth_cleared(uint32_t const index, uint32_t const target_power);

__global__ void apply_gate_one(
  mcFloatComplex* stateVector,

  mcFloatComplex const m00,
  mcFloatComplex const m01,
  mcFloatComplex const m10,
  mcFloatComplex const m11,
  
  uint32_t const target,
  uint32_t const controlMask
);

__global__ void find_probability_of_outcome_zero(
  mcFloatComplex* stateVector,
  float* output,
  uint32_t const target
);

__global__ void collapse_to_outcome_scale(
  mcFloatComplex* stateVector,
  uint32_t const target,
  float const scale,
  bool const one,
  bool const noNorm
);

__global__ void store_state_vector(
  mcFloatComplex* stateVectorBuffer,
  mcFloatComplex const* stateVector
);

__global__ void dot_state_vector(
  mcFloatComplex const* stateVector,
  mcFloatComplex const* stateVectorBuffer,
  float* output
);

__global__ void norm_state_vector(
  mcFloatComplex const* stateVector,
  float* output
);

}
}