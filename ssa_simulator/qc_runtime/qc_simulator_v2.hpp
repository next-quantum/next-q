#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "qc_types_v2.hpp"

namespace QC_V2 {

class SimulatorV2 {
public:
  virtual ~SimulatorV2() = default;

  virtual void seed(std::uint64_t seed) = 0;

  virtual std::uint64_t numberOfQubits() const = 0;

  virtual void resetQubit(std::uint64_t target) = 0;

  virtual bool dumpToFile(std::vector<std::uint64_t> const& qubits, std::string filepath) = 0;

  virtual void resetToZeroState() = 0;

  virtual void X(std::uint64_t const target) = 0;
  virtual void MCX(std::uint64_t const target, std::vector<std::uint64_t> const& controls) = 0;
  virtual void Y(std::uint64_t const target) = 0;
  virtual void MCY(std::uint64_t const target, std::vector<std::uint64_t> const& controls) = 0;
  virtual void Z(std::uint64_t const target) = 0;
  virtual void MCZ(std::uint64_t const target, std::vector<std::uint64_t> const& controls) = 0;
  virtual void H(std::uint64_t const target) = 0;
  virtual void MCH(std::uint64_t const target, std::vector<std::uint64_t> const& controls) = 0;
  virtual void S(std::uint64_t const target) = 0;
  virtual void MCS(std::uint64_t const target, std::vector<std::uint64_t> const& controls) = 0;
  virtual void T(std::uint64_t const target) = 0;
  virtual void MCT(std::uint64_t const target, std::vector<std::uint64_t> const& controls) = 0;
  virtual void AdjS(std::uint64_t const target) = 0;
  virtual void MCAdjS(std::uint64_t const target, std::vector<std::uint64_t> const& controls) = 0;
  virtual void AdjT(std::uint64_t const target) = 0;
  virtual void MCAdjT(std::uint64_t const target, std::vector<std::uint64_t> const& controls) = 0;
  virtual void CNOT(std::uint64_t const control, std::uint64_t const target) = 0;
  virtual void MCCNOT(std::uint64_t const control, std::uint64_t const target, std::vector<std::uint64_t> const& controls) = 0;
  virtual void SWAP(std::uint64_t const qubit1, std::uint64_t const qubit2) = 0;
  virtual void MCSWAP(std::uint64_t const qubit1, std::uint64_t const qubit2, std::vector<std::uint64_t> const& controls) = 0;
  virtual void R(std::uint64_t const target, Basis const basis, RealType const angle) = 0;
  virtual void MCR(std::uint64_t const target, std::vector<std::uint64_t> const& controls, Basis const basis, RealType const angle) = 0;
  virtual void R1(std::uint64_t const target, RealType const angle) = 0;
  virtual void MCR1(std::uint64_t const target, std::vector<std::uint64_t> const& controls, RealType const angle) = 0;
  virtual Result M(std::uint64_t const target, Basis const basis) = 0;
  virtual double JointEnsembleProbability(Term const& term) = 0;
  virtual void getStateVector(std::vector<std::complex<double>>& state) const = 0;
};

}