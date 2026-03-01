#pragma once

#include <cmath>
#include <cstdint>
#include <functional>

#include <array>
#include <vector>

#include <mc_runtime.h>

#include "qc_types_v2.hpp"
#include "qc_simulator_v2.hpp"

#include "maca_complex.hpp"

namespace QC_V2 {

namespace SimulatorMetaxGPUSVV2TypeDef {

// common unitary matricess

using Unitary1QType = std::array<ComplexType, 4>;

constexpr Unitary1QType XGate = Unitary1QType{0, 1, 1, 0};
constexpr Unitary1QType YGate = Unitary1QType{0, ComplexIMinus, ComplexI, 0};
constexpr Unitary1QType ZGate = Unitary1QType{1, 0, 0, -1};
constexpr Unitary1QType HGate = Unitary1QType{M_SQRT1_2, M_SQRT1_2, M_SQRT1_2, -M_SQRT1_2};
constexpr Unitary1QType SGate = Unitary1QType{1, 0, 0, ComplexI};
constexpr Unitary1QType TGate = Unitary1QType{1, 0, 0, ComplexType(M_SQRT1_2, M_SQRT1_2)};
constexpr Unitary1QType AdjSGate = Unitary1QType{1, 0, 0, ComplexIMinus};
constexpr Unitary1QType AdjTGate = Unitary1QType{1, 0, 0, ComplexType(M_SQRT1_2, -M_SQRT1_2)};

// global phase shift gate
static inline Unitary1QType PhGate(double const angle) {
  // exp(-i*theta/2)
  auto d = std::exp(ComplexType(0, -angle*0.5));

  return Unitary1QType{d, 0, 0, d};
}

static inline Unitary1QType RxGate(double const angle) {
  auto h = angle*0.5;
  auto c = std::cos(h);
  auto s = std::sin(h);

  return Unitary1QType{ComplexType(c, 0), ComplexType(0, -s), ComplexType(0, -s), ComplexType(c, 0)};
}

static inline Unitary1QType RyGate(double const angle) {
  auto h = angle*0.5;
  auto c = std::cos(h);
  auto s = std::sin(h);

  // [c -s]
  // [s c]
  // [Bug detected, no minus sign]
  return Unitary1QType{ComplexType(c, 0), ComplexType(-s, 0), ComplexType(s, 0), ComplexType(c, 0)};
}

static inline Unitary1QType RzGate(double const angle) {
    auto h = angle*0.5;
    auto c = std::cos(h);
    auto s = std::sin(h);
    // exp(-i*theta/2)
    auto d1 = ComplexType(c, -s);
    // exp(i*theta/2)
    auto d2 = ComplexType(c, s);

    return Unitary1QType{d1, 0, 0, d2};
  }

// Rotation R1 gate
static inline Unitary1QType R1Gate(double const angle) {
  // exp(i*theta)
  auto d = std::exp(ComplexType(0, angle));

  return Unitary1QType{1, 0, 0, d};
}

}

class SimulatorMetaxGPUSVV2 : public SimulatorV2 {
public:
  SimulatorMetaxGPUSVV2(std::uint64_t nQubits_ = 0UL);
  ~SimulatorMetaxGPUSVV2() override;

  void seed(std::uint64_t seed) override;

  std::uint64_t numberOfQubits() const override;

  void resetQubit(std::uint64_t target) override;

  bool dumpToFile(std::vector<std::uint64_t> const& qubits, std::string filepath) override;

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
  double JointEnsembleProbability(Term const& term) override;
  void getStateVector(std::vector<std::complex<double>>& state) const override;

private:
  using Unitary1QType = SimulatorMetaxGPUSVV2TypeDef::Unitary1QType;

  std::uint64_t nQubits;

  RndEngine engine;
  std::function<RealType()> random;

  // MACA data structures
  int _mDeviceID{0};
  mcDeviceProp_t _mDeviceProp;

  std::vector<mcFloatComplex> _mHostStateVector;
  std::vector<mcFloatComplex> _mHostStateVectorBuffer;
  float* _mDeviceOutput{nullptr};

  mcFloatComplex* _mDeviceStateVector{nullptr};
  mcFloatComplex* _mDeviceStateVectorBuffer{nullptr};

  mcEvent_t _mEventStart{nullptr};
  mcEvent_t _mEventStop{nullptr};

  mcFuncAttributes _mApplyGateOneFunctionAttrs;
  mcFuncAttributes _mFindProbabilityOfOutcomeZeroFunctionAttrs;
  mcFuncAttributes _mCollapseToOutcomeScaleFunctionAttrs;
  mcFuncAttributes _mStoreStateVectorFunctionAttrs;
  mcFuncAttributes _mDotStateVectorFunctionAttrs;
  mcFuncAttributes _mNormStateVectorFunctionAttrs;

private:
  static std::uint64_t getControlMask(std::vector<std::uint64_t> const& ctrls);
  
  void applyGateOne(
    mcFloatComplex matrix[4],
    uint32_t const target, 
    uint32_t const controlMask
  );

  // ClassicalValue classicalValue(std::uint64_t pos, double tol = 1E-12) const;

  RealType findProbabilityOfOutcome(std::uint64_t target, Result outcome) const;

  void collapseToOutcome(std::uint64_t target, RealType normFactor, Result outcome, bool noNorm=false);

  void store();

  void swap();

  RealType dot() const;

  void dump(std::ostream& os) const;

  void dump(std::vector<std::uint64_t> pos, std::ostream& os) const;

  RealType norm() const;

  void multiControlledGate(const Unitary1QType& matrix, std::uint64_t target, const std::vector<std::uint64_t>& controls);

  Result measureZWithStats(std::uint64_t const target, RealType& stateProb);
  Result measureWithStats(std::uint64_t const target, Basis const basis, RealType& stateProb);

  Result measure(std::uint64_t const target, Basis const basis);

  void applyTerm(Term const& term);

  RealType expectation(const Term& term);

  void calculateKernelLaunchSizeType2(uint64_t& blocksPerGrid, uint64_t& threadsPerBlock, const uint64_t maxThreadsPerBlock) const;
	
  void calculateKernelLaunchSizeType3(uint64_t& blocksPerGrid, uint64_t& threadsPerBlock, const uint64_t maxThreadsPerBlock) const;
};

}