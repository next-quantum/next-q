#include <cassert>

#include <iomanip>
#include <iostream>

#include "qc_simulator_default_cpu_sv_v2.hpp"

namespace QC_V2 {

using namespace SimulatorDefaultCPUSVV2TypeDef;

const RealType RealEPS = 1e-13;

// Implementations of SimulatorV2
SimulatorDefaultCPUSVV2::SimulatorDefaultCPUSVV2(std::uint64_t nQubits_): random(std::bind(std::uniform_real_distribution<RealType>(0., 1.), std::ref(engine))) {
  vec = StateVector(1UL << nQubits_, 0.0); // Copy elision
  buffer = StateVector(1UL << nQubits_, 0.0); // Copy elision
  nQubits = nQubits_;

  // Normalization is forced
  vec[0] = 1.0;
  buffer[0] = 1.0;

  assert(!vec.empty());
}

SimulatorDefaultCPUSVV2::~SimulatorDefaultCPUSVV2() {}

std::uint64_t SimulatorDefaultCPUSVV2::getControlMask(std::vector<std::uint64_t> const& ctrls){
  std::uint64_t ctrlmask = 0;
  for (auto c : ctrls)
    ctrlmask |= (1UL << c);
  return ctrlmask;
}

void SimulatorDefaultCPUSVV2::apply(const FusionGate& gate) {
  auto const ctrlmask = getControlMask(gate.controls);

  switch (gate.targets.size()){
    case 1:
      #pragma omp parallel
      kernel(vec, gate.targets[0], gate.matrix, ctrlmask);
      break;
    case 2:
      #pragma omp parallel
      kernel(vec, gate.targets[1], gate.targets[0], gate.matrix, ctrlmask);
      break;
    case 3:
      #pragma omp parallel
      kernel(vec, gate.targets[2], gate.targets[1], gate.targets[0], gate.matrix, ctrlmask);
      break;
    case 4:
      #pragma omp parallel
      kernel(vec, gate.targets[3], gate.targets[2], gate.targets[1], gate.targets[0], gate.matrix, ctrlmask);
      break;
    case 5:
      #pragma omp parallel
      kernel(vec, gate.targets[4], gate.targets[3], gate.targets[2], gate.targets[1], gate.targets[0], gate.matrix, ctrlmask);
      break;
  }
}

ClassicalValue SimulatorDefaultCPUSVV2::classicalValue(std::uint64_t pos, double tol) const {
  std::uint64_t delta = (1UL << pos);
  short up = 0, down = 0;
  for (std::uint64_t i = 0; i < vec.size(); i += 2*delta){
    for (std::uint64_t j = 0; j != delta; ++j){
      up = up | ((std::norm(vec[i+j]) > tol)&1);
      down = down | ((std::norm(vec[i+j+delta]) > tol)&1);
    }
  }

  if (up == 0 && down == 0) {
    return ClassicalValue::ClassicalUnknown;
  } else if (up == 1 && down == 0) {
    return ClassicalValue::ClassicalZero;
  } else if (up == 0 && down == 1) {
    return ClassicalValue::ClassicalOne;
  } else {
    return ClassicalValue::ClassicalMixed;
  }
}

RealType SimulatorDefaultCPUSVV2::findProbabilityOfOutcome(std::uint64_t target, Result outcome) const {
  RealType totalProbability = 0.0;

  std::uint64_t delta = (1UL << target);
  for (std::uint64_t i = 0; i < vec.size(); i += 2*delta){
    for (std::uint64_t j = 0; j < delta; ++j){
      totalProbability += std::norm(vec[i+j]);
    }
  }

  if (outcome == Result::One) {
    return 1.0 - totalProbability;
  }

  return totalProbability;
}

// collapseToOutcome will collapse the state vector
void SimulatorDefaultCPUSVV2::collapseToOutcome(std::uint64_t target, RealType normFactor, Result outcome) {
  std::uint64_t delta = (1UL << target);
  for (std::uint64_t i = 0; i < vec.size(); i += 2*delta){
    for (std::uint64_t j = 0; j < delta; ++j){
      switch (outcome) {
      case Result::Zero:
        vec[i+j] *= normFactor;
        vec[i+j+delta] = 0.0;
        break;
      case Result::One:
        vec[i+j] = 0.0;
        vec[i+j+delta] *= normFactor;
        break;
      }
    }
  }
}

void SimulatorDefaultCPUSVV2::store() {
  assert(!vec.empty());
  std::copy(vec.begin(), vec.end(), buffer.begin());
}

void SimulatorDefaultCPUSVV2::swap() {
  assert(!vec.empty());
  std::swap(vec, buffer);
}

RealType SimulatorDefaultCPUSVV2::dot() const {
  RealType sum = 0.0;
  assert(!vec.empty());
  assert(vec.size() == buffer.size());

  for (std::size_t i=0; i < vec.size(); ++i) {
    auto const a1 = std::real(buffer[i]);
    auto const b1 = -std::imag(buffer[i]);
    auto const a2 = std::real(vec[i]);
    auto const b2 = std::imag(vec[i]);
    sum += a1 * a2 - b1 * b2;
  }

  return sum;
}

void SimulatorDefaultCPUSVV2::dump(std::ostream& os) const {
  os << "State vector (size " << vec.size() << "): \n";
  os << "Norm: " << norm() << "\n";
  assert(!vec.empty());
  for (std::size_t i=0; i != vec.size(); ++i) {
    // os << "  " << std::fixed << std::setprecision(8) << vec[i] << "|" << (vec.size() + i) << ">" << "\n";
    os << "  " << std::fixed << std::setprecision(8) << vec[i] << "|" << (i) << ">" << "\n";
  }
  os << std::endl;
}

void SimulatorDefaultCPUSVV2::dump(std::vector<std::uint64_t> pos, std::ostream& os) const {
  os << "State vector (size " << 1UL << pos.size() << "): \n";
  assert(!vec.empty());

  std::vector<bool> bits(pos.size(), false); 
  for (std::size_t n=0; n != (1UL << pos.size()); n++) {
    auto value = n;
    for (std::size_t i=0; i != pos.size(); i++) {
      bits[pos.size() - 1UL - i] = value % 2;
      value /= 2;
    }

    std::size_t m = 0;
    for (std::size_t i=0; i != pos.size(); i++) {
      if (bits[i]) {
        m += 1UL << pos[i];
      }
    }

    os << "  " << std::fixed << std::setprecision(8) 
        << vec[m] << " |"
        << std::norm(vec[m]) << " |";
    for (std::size_t i=0; i != pos.size(); i++) {
      os << bits[i];
    }
    os << ">" << "\n";
  }
  
  os << std::endl;
}

RealType SimulatorDefaultCPUSVV2::norm() const {
  double sum = 0;
  for (auto& data : vec) {
    sum += std::norm(data);
  }

  return sum;
}

bool SimulatorDefaultCPUSVV2::dumpToFile(std::vector<std::uint64_t> const& qubits, std::string filepath) {
  std::cout << "Dump to filepath " << filepath << " (but use cout for now)" << std::endl;

  dump(qubits, std::cout);

  return true;
}

void SimulatorDefaultCPUSVV2::resetToZeroState() {
  std::fill(vec.begin(), vec.end(), 0.0);
  vec[0] = 1.0;
}

void SimulatorDefaultCPUSVV2::resetQubit(std::uint64_t target) {
  std::uint64_t delta = (1UL << target);
  
  // First pass: calculate the norm of |0> state for the target qubit
  RealType norm_zero = 0.0;
  for (std::uint64_t i = 0; i < vec.size(); i += 2*delta) {
    for (std::uint64_t j = 0; j < delta; ++j) {
      norm_zero += std::norm(vec[i + j]);
    }
  }
  
  // Second pass: set |1> state amplitude to zero and normalize |0> state
  if (norm_zero > 0.0) {
    RealType inv_norm = 1.0 / std::sqrt(norm_zero);
    for (std::uint64_t i = 0; i < vec.size(); i += 2*delta) {
      for (std::uint64_t j = 0; j < delta; ++j) {
        // Normalize |0> state amplitude
        vec[i + j] *= inv_norm;
        // Set the |1> state amplitude to zero
        vec[i + j + delta] = 0.0;
      }
    }
  } else {
    // If |0> state has no amplitude, directly set to |0> state
    for (std::uint64_t i = 0; i < vec.size(); ++i) {
      vec[i] = 0.0;
    }
    // Set |0> state for all qubits to 1.0
    vec[0] = 1.0;
  }
}

void SimulatorDefaultCPUSVV2::seed(std::uint64_t seed) {
  engine.seed(seed);
}

std::uint64_t SimulatorDefaultCPUSVV2::numberOfQubits() const {
  return nQubits;
}

void SimulatorDefaultCPUSVV2::multiControlledGate(const MatrixType& matrix, std::uint64_t target, const std::vector<std::uint64_t>& controls) {
  for (auto control : controls) {
    assert(control != target);
  }

  #ifdef DEBUG
    std::cout << "[Before multi controlled gate]" << std::endl;
    dump(std::cout);
  #endif

  FusionGate gate(matrix, std::vector<std::uint64_t>{target}, controls);
  apply(gate);

  #ifdef DEBUG
    std::cout << "[After multi controlled gate]" << std::endl;
    dump(std::cout);
  #endif
}

Result SimulatorDefaultCPUSVV2::measureZWithStats(std::uint64_t const target, RealType& stateProb) {
  Result outcome = Result::Zero;

  auto const stateProbOne = findProbabilityOfOutcome(target, Result::One);
  if (stateProbOne < RealEPS) {
    outcome = Result::Zero;
  } else if (1.0 - stateProbOne < RealEPS) {
    outcome = Result::One;
  } else {
    if (random() <= stateProbOne) {
      outcome = Result::One;
    } else {
      outcome = Result::Zero;
    }
  }

  // Obtain sum probability for the outcome
  switch(outcome) {
    case Result::Zero:
    {
      stateProb = 1.0 - stateProbOne;
      break;
    }
    case Result::One:
    {
      stateProb = stateProbOne;
      break;
    }
  }

  // Collapse state vector
  collapseToOutcome(target, 1.0 / std::sqrt(stateProb), outcome);

  return outcome;
}

Result SimulatorDefaultCPUSVV2::measureWithStats(std::uint64_t const target, Basis const basis, RealType& stateProb) {
  switch(basis) {
    case Basis::PauliI:
      // I = S Z S
      S(target);
      break;
    case Basis::PauliX:
      // X = H Z H
      H(target);
      break;
    case Basis::PauliY:
      // Y = S H Z H AdjS
      AdjS(target);
      H(target);
      break;
    case Basis::PauliZ:
      // Z = Z
      break;
  }

  auto const res = measureZWithStats(target, stateProb);

  switch(basis) {
    case Basis::PauliI:
      // I = S Z S
      S(target);
      break;
    case Basis::PauliX:
      // X = H Z H
      H(target);
      break;
    case Basis::PauliY:
      // Y = S H Z H AdjS
      H(target);
      S(target);
      break;
    case Basis::PauliZ:
      // Z = Z
      break;
  }

  return res;
}

Result SimulatorDefaultCPUSVV2::measure(std::uint64_t const target, Basis const basis) {
  RealType stateProb = 0.0;
  return measureWithStats(target, basis, stateProb);
}

void SimulatorDefaultCPUSVV2::applyTerm(Term const& term) {
  for (auto const& op : term) {
    auto const pos = op.first;

    switch (op.second) {
      case Basis::PauliI:
        // Do nothing
        break;
      case Basis::PauliX:
        multiControlledGate(XGate, pos, std::vector<std::uint64_t>{});
        break;
      case Basis::PauliY:
        multiControlledGate(YGate, pos, std::vector<std::uint64_t>{});
        break;
      case Basis::PauliZ:
        multiControlledGate(ZGate, pos, std::vector<std::uint64_t>{});
        break;
    }
  }
}

RealType SimulatorDefaultCPUSVV2::expectation(const Term& term) {
  store();
  applyTerm(term);
  auto const value = dot();
  swap();

  #ifdef DEBUG
    dump(std::cout);
  #endif

  return value;
}

void SimulatorDefaultCPUSVV2::X(std::uint64_t const target) {
  multiControlledGate(XGate, target, std::vector<std::uint64_t>{});
}

void SimulatorDefaultCPUSVV2::MCX(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(XGate, target, controls);
}

void SimulatorDefaultCPUSVV2::Y(std::uint64_t const target) {
  multiControlledGate(YGate, target, std::vector<std::uint64_t>{});
}

void SimulatorDefaultCPUSVV2::MCY(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(YGate, target, controls);
}

void SimulatorDefaultCPUSVV2::Z(std::uint64_t const target) {
  multiControlledGate(ZGate, target, std::vector<std::uint64_t>{});
}

void SimulatorDefaultCPUSVV2::MCZ(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(ZGate, target, controls);
}

void SimulatorDefaultCPUSVV2::H(std::uint64_t const target) {
  multiControlledGate(HGate, target, std::vector<std::uint64_t>{});
}

void SimulatorDefaultCPUSVV2::MCH(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(HGate, target, controls);
}

void SimulatorDefaultCPUSVV2::S(std::uint64_t const target) {
  multiControlledGate(SGate, target, std::vector<std::uint64_t>{});
}

void SimulatorDefaultCPUSVV2::MCS(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(SGate, target, controls);
}

void SimulatorDefaultCPUSVV2::T(std::uint64_t const target) {
  multiControlledGate(TGate, target, std::vector<std::uint64_t>{});
}

void SimulatorDefaultCPUSVV2::MCT(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(TGate, target, controls);
}

void SimulatorDefaultCPUSVV2::AdjS(std::uint64_t const target) {
  multiControlledGate(AdjSGate, target, std::vector<std::uint64_t>{});
}

void SimulatorDefaultCPUSVV2::MCAdjS(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(AdjSGate, target, controls);
}

void SimulatorDefaultCPUSVV2::AdjT(std::uint64_t const target) {
  multiControlledGate(AdjTGate, target, std::vector<std::uint64_t>{});
}

void SimulatorDefaultCPUSVV2::MCAdjT(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(AdjTGate, target, controls);
}

void SimulatorDefaultCPUSVV2::CNOT(std::uint64_t const control, std::uint64_t const target) {
  MCX(target, std::vector<std::uint64_t>{control});
}

void SimulatorDefaultCPUSVV2::MCCNOT(std::uint64_t const control, std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  auto controlsCombined = controls;
  controlsCombined.push_back(control);
  MCX(target, controlsCombined);
}

void SimulatorDefaultCPUSVV2::SWAP(std::uint64_t const qubit1, std::uint64_t const qubit2) {
  // Implementation with CNOT
  CNOT(qubit1, qubit2);
  CNOT(qubit2, qubit1);
  CNOT(qubit1, qubit2);
}

void SimulatorDefaultCPUSVV2::MCSWAP(std::uint64_t const qubit1, std::uint64_t const qubit2, std::vector<std::uint64_t> const& controls) {
  // Implementation with CNOT
  MCCNOT(qubit1, qubit2, controls);
  MCCNOT(qubit2, qubit1, controls);
  MCCNOT(qubit1, qubit2, controls);
}

void SimulatorDefaultCPUSVV2::R(std::uint64_t const target, Basis const basis, RealType const angle) {
  switch (basis) {
  case Basis::PauliI:
    {
      multiControlledGate(PhGate(angle), target, std::vector<std::uint64_t>{});
      break;
    }
  case Basis::PauliX:
    {
      multiControlledGate(RxGate(angle), target, std::vector<std::uint64_t>{});
      break;
    }
  case Basis::PauliY:
    {
      multiControlledGate(RyGate(angle), target, std::vector<std::uint64_t>{});
      break;
    }
  case Basis::PauliZ:
    {
      multiControlledGate(RzGate(angle), target, std::vector<std::uint64_t>{});
      break;
    }
  }
}

void SimulatorDefaultCPUSVV2::MCR(std::uint64_t const target, std::vector<std::uint64_t> const& controls, Basis const basis, RealType const angle) {
  switch (basis) {
  case Basis::PauliI:
    {
      multiControlledGate(PhGate(angle), target, controls);
      break;
    }
  case Basis::PauliX:
    {
      multiControlledGate(RxGate(angle), target, controls);
      break;
    }
  case Basis::PauliY:
    {
      multiControlledGate(RyGate(angle), target, controls);
      break;
    }
  case Basis::PauliZ:
    {
      multiControlledGate(RzGate(angle), target, controls);
      break;
    }
  }
}

void SimulatorDefaultCPUSVV2::R1(std::uint64_t const target, RealType const angle) {
  multiControlledGate(R1Gate(angle), target, std::vector<std::uint64_t>{});
}

void SimulatorDefaultCPUSVV2::MCR1(std::uint64_t const target, std::vector<std::uint64_t> const& controls, RealType const angle) {
  multiControlledGate(R1Gate(angle), target, controls);
}

Result SimulatorDefaultCPUSVV2::M(std::uint64_t const target, Basis const basis) {
  return measure(target, basis);
}

double SimulatorDefaultCPUSVV2::JointEnsembleProbability(Term const& term) {
  // Measurement result: -1 for Zero, 1 for One
  auto const value = expectation(term);
  // Pr(One||ψ⟩)
  return static_cast<RealType>(0.5) * (static_cast<RealType>(1.0) - value);
}

void SimulatorDefaultCPUSVV2::getStateVector(std::vector<std::complex<double>>& state) const {
  state.resize(vec.size());
  for (std::size_t i = 0; i < vec.size(); ++i) {
    state[i] = vec[i];
  }
}

}