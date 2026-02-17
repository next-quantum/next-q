#pragma once

#include <cstdint>

#include <vector>

#include "qc_types_v2.hpp"
#include "qc_simulator_v2.hpp"

#include "intrin/alignedallocator.hpp"

#if defined(NOINTRIN) || !defined(INTRIN)
#include "nointrin/kernels.hpp"
#else
#include "intrin/kernels.hpp"
#endif

namespace QC_V2 {

namespace SimulatorDefaultCPUSVV2TypeDef {

constexpr std::uint64_t BYTE_ALIGNMENGT = 64UL; // 512 bits = 8 doubles

using StateVector = std::vector<ComplexType, aligned_allocator<ComplexType, BYTE_ALIGNMENGT>>;

// compitable with Fusion::Matrix
using MatrixType = std::vector<StateVector>;

// Define the commonly used gate
static const MatrixType XGate = MatrixType{{0, 1}, {1, 0}};
static const MatrixType YGate = MatrixType{{0., -ComplexI}, {ComplexI, 0.}};
static const MatrixType ZGate = MatrixType{{1., 0.}, {0., -1.}};
static const MatrixType HGate = MatrixType{{M_SQRT1_2, M_SQRT1_2}, {M_SQRT1_2, -M_SQRT1_2}};
static const MatrixType SGate = MatrixType{{1, 0}, {0, ComplexI}};
static const MatrixType TGate = MatrixType{{1, 0}, {0, std::exp(ComplexI*M_PI_4)}};
static const MatrixType AdjSGate = MatrixType{{1, 0}, {0, -ComplexI}};
static const MatrixType AdjTGate = MatrixType{{1, 0}, {0, std::exp(-ComplexI*M_PI_4)}};

static inline const MatrixType PhGate(double angle) {
  // exp(-i*theta/2)
  auto d = std::exp(ComplexType(0, -angle*0.5));

  return MatrixType{{d, 0}, {0, d}};
}

static inline const MatrixType RxGate(double angle) {
  auto h = angle*0.5;
  auto c = std::cos(h);
  auto s = std::sin(h);

  MatrixType mat(2);
  mat[0] = StateVector(2);
  mat[1] = StateVector(2);
  
  mat[0][0] = ComplexType(c, 0);
  mat[0][1] = ComplexType(0, -s);
  mat[1][0] = ComplexType(0, -s);
  mat[1][1] = ComplexType(c, 0);
  
  return mat;
}

static inline const MatrixType RyGate(double angle) {
  auto h = angle*0.5;
  auto c = std::cos(h);
  auto s = std::sin(h);

  MatrixType mat(2);
  mat[0] = StateVector(2);
  mat[1] = StateVector(2);
  
  // [c -s]
  // [s c]
  mat[0][0] = ComplexType(c, 0);
  mat[0][1] = ComplexType(-s, 0);
  mat[1][0] = ComplexType(s, 0);
  mat[1][1] = ComplexType(c, 0);
  
  return mat;
}

static inline const MatrixType RzGate(double angle) {
  auto h = angle*0.5;
  auto c = std::cos(h);
  auto s = std::sin(h);
  // exp(-i*theta/2)
  auto d1 = ComplexType(c, -s);
  // exp(i*theta/2)
  auto d2 = ComplexType(c, s);

  MatrixType mat(2);
  mat[0] = StateVector(2);
  mat[1] = StateVector(2);
  
  mat[0][0] = d1;
  mat[0][1] = ComplexType(0, 0);
  mat[1][0] = ComplexType(0, 0);
  mat[1][1] = d2;
  
  return mat;
}

// Rotation R1 gate
static inline const MatrixType R1Gate(double angle) {
  // exp(i*theta)
  auto d = std::exp(ComplexType(0, angle));

  return MatrixType{{1, 0}, {0, d}};
}

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

  inline bool empty() const {
    return targets.empty();
  }
};

}

class SimulatorDefaultCPUSVV2 : public SimulatorV2 {
public:
  SimulatorDefaultCPUSVV2(std::uint64_t nQubits_ = 0UL);
  ~SimulatorDefaultCPUSVV2() override;

  void seed(std::uint64_t seed) override;

  std::uint64_t numberOfQubits() const override;

  // Reset a specific qubit to |0> state
  void resetQubit(std::uint64_t target) override;

  bool dumpToFile(std::vector<std::uint64_t> const& qubits, std::string filepath) override;

  // [UPDATE 2026.1.10 10:01] clear the quantum state to |0>
  void resetToZeroState() override;

  void X(std::uint64_t const target) override;
  void MCX(std::uint64_t const target, std::vector<std::uint64_t> const& controls) override;
  void Y(std::uint64_t const target) override;
  void MCY(std::uint64_t const target, std::vector<std::uint64_t> const& controls) override;
  void Z(std::uint64_t const target) override;
  void MCZ(std::uint64_t const target, std::vector<std::uint64_t> const& controls) override;
  void H(std::uint64_t const target) override;
  void MCH(std::uint64_t const target, std::vector<std::uint64_t> const& controls) override;
  void S(std::uint64_t const target) override;
  void MCS(std::uint64_t const target, std::vector<std::uint64_t> const& controls) override;
  void T(std::uint64_t const target) override;
  void MCT(std::uint64_t const target, std::vector<std::uint64_t> const& controls) override;
  void AdjS(std::uint64_t const target) override;
  void MCAdjS(std::uint64_t const target, std::vector<std::uint64_t> const& controls) override;
  void AdjT(std::uint64_t const target) override;
  void MCAdjT(std::uint64_t const target, std::vector<std::uint64_t> const& controls) override;
  void CNOT(std::uint64_t const control, std::uint64_t const target) override;
  void MCCNOT(std::uint64_t const control, std::uint64_t const target, std::vector<std::uint64_t> const& controls) override;
  void SWAP(std::uint64_t const qubit1, std::uint64_t const qubit2) override;
  void MCSWAP(std::uint64_t const qubit1, std::uint64_t const qubit2, std::vector<std::uint64_t> const& controls) override;
  void R(std::uint64_t const target, Basis const basis, RealType const angle) override;
  void MCR(std::uint64_t const target, std::vector<std::uint64_t> const& controls, Basis const basis, RealType const angle) override;
  void R1(std::uint64_t const target, RealType const angle) override;
  void MCR1(std::uint64_t const target, std::vector<std::uint64_t> const& controls, RealType const angle) override;
  Result M(std::uint64_t const target, Basis const basis) override;
  // Pr(One||ψ⟩)
  double JointEnsembleProbability(Term const& term) override;
  void getStateVector(std::vector<std::complex<double>>& state) const override;

private:
  using StateVector = SimulatorDefaultCPUSVV2TypeDef::StateVector;
  using MatrixType = SimulatorDefaultCPUSVV2TypeDef::MatrixType;
  using FusionGate = SimulatorDefaultCPUSVV2TypeDef::FusionGate;

  // nQubits is the number of qubits
  std::uint64_t nQubits;
  // vec is the storage of state vector
  StateVector vec;
  // buffer is the copied buffer space for state vector
  StateVector buffer;

  // engine is the random number generation engine
  RndEngine engine;
  // random is the random generator for floating number between 0 and 1
  std::function<RealType()> random;

private:
  static std::uint64_t getControlMask(std::vector<std::uint64_t> const& ctrls);
  
  // apply will perform a fusion gate on the state vector
  void apply(const FusionGate& gate);

  // fetch classical value
  // c++ complex norm calcuate |re|*|re| + |im|*|im|, then tolerance should larger than 1E-24 for each component less than 1E-12
  ClassicalValue classicalValue(std::uint64_t pos, double tol = 1E-12) const;

  RealType findProbabilityOfOutcome(std::uint64_t target, Result outcome) const;

  // collapseToOutcome will collapse the state vector
  void collapseToOutcome(std::uint64_t target, RealType normFactor, Result outcome);

  // store will save the current state in buffer
  void store();

  // swap will exchange the storage of the vec and buffer
  void swap();

  // dot will get the dot product with buffer
  RealType dot() const;

  // dump will print the state vector to an output stream
  // ID is the the number of chunks before starting of this chunk
  void dump(std::ostream& os) const;

  void dump(std::vector<std::uint64_t> pos, std::ostream& os) const;

  RealType norm() const;

  void multiControlledGate(const MatrixType& matrix, std::uint64_t target, const std::vector<std::uint64_t>& controls);

  Result measureZWithStats(std::uint64_t const target, RealType& stateProb);
  Result measureWithStats(std::uint64_t const target, Basis const basis, RealType& stateProb);

  Result measure(std::uint64_t const target, Basis const basis);

  void applyTerm(Term const& term);

  RealType expectation(const Term& term);
};

}