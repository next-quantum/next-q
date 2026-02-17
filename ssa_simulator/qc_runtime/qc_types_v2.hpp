#pragma once

#include <cstdint>

#include <complex>
#include <random>

namespace QC_V2 {

using RealType = double;
using ComplexType = std::complex<RealType>;

// constexpr ComplexType Complex0 = ComplexType(0, 0);
// constexpr ComplexType Complex1 = ComplexType(1, 0);
constexpr ComplexType ComplexI = ComplexType(0, 1);
constexpr ComplexType ComplexIMinus = ComplexType(0, -1);

enum class Result {
  Zero = false,
  One  = true
};

enum class Basis {
  PauliI = 0,
  PauliX = 1,
  PauliY = 2,
  PauliZ = 3
};

enum class ClassicalValue {
  ClassicalUnknown = 0,
  ClassicalZero = 1,
  ClassicalOne = 2,
  ClassicalMixed = 3
};

using RndEngine = std::mt19937;

// [UPDATE 2026.1.31 20:25] use integer instead of Qubit structure
using Term = std::vector<std::pair<uint64_t, Basis>>;


}