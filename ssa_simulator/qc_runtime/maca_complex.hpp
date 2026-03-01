#pragma once

#include <iostream>
#include <cmath>

#include <mc_runtime.h>

// Use float2 as complex type in MACA
typedef float2 mcFloatComplex;

__host__ __device__ inline mcFloatComplex operator*(mcFloatComplex const& c, float const s) {
  auto r = c;
  r.x *= s;
  r.y *= s;

  return r;
}

__host__ __device__ inline float mcFloatComplexNorm(mcFloatComplex const& c) {
  return c.x * c.x + c.y * c.y;
}

__host__ inline std::ostream& operator<<(std::ostream& os, mcFloatComplex const& c) {
  os << "(" << c.x << ", " << c.y << ")";
  return os;
}