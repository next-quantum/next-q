#ifndef QC_TYPES_HPP_
#define QC_TYPES_HPP_

#include <complex>
#include <cstdint>
#include <random>
#include <vector>

#include "intrin/alignedallocator.hpp"

namespace QC {
  constexpr std::uint64_t BYTE_ALIGNMENGT = 64UL; // 512 bits = 8 doubles
  using RealType = double;
  using ComplexType = std::complex<RealType>;
  using StateVector = std::vector<ComplexType, aligned_allocator<ComplexType, BYTE_ALIGNMENGT>>;

  // compitable with Fusion::Matrix
  using MatrixType = std::vector<StateVector>;

  struct FusionGate {
    MatrixType matrix;
    std::vector<std::uint64_t> targets;
    std::vector<std::uint64_t> controls;

    FusionGate() {}
    FusionGate(
      const MatrixType& matrix_,
      std::vector<std::uint64_t> targets_,
      std::vector<std::uint64_t> controls_
    ) :matrix(matrix_), targets(targets_), controls(controls_) {}

    bool empty() const {
      return targets.empty();
    }
  };

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

  const RealType RealEPS = 1e-13;
}

#endif
