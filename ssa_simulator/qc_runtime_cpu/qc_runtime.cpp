#include "qc_runtime.h"
#include "qc_simulator.hpp"
#include "qc_strformat.hpp"
#include "qc_types.hpp"

#include <cstdint>
#include <cmath>
#include <memory>

namespace QC {
  // A unique pointer to simulator object
  std::unique_ptr<Simulator> simulator = nullptr;

  void seed(std::uint64_t seed) {
    simulator->seed(seed);
  }

  std::uint64_t randomChoice(std::uint64_t size, RealType p[]) {
    return simulator->randomChoice(size, p);
  }

  RealType jointEnsembleProbability(const Term& term) {
    // Measurement result: -1 for Zero, 1 for One
    auto value = simulator->expectation(term);
    // Pr(One||ψ⟩)
    return static_cast<RealType>(0.5) * (static_cast<RealType>(1.0) - value);
  }

  void allocateQubit(const Qubit& qubit) {
    simulator->allocate(qubit);
  }

  bool deallocateQubit(const Qubit& qubit) {
    return simulator->deallocate(qubit);
  }

  std::uint64_t numQubits() {
    return simulator->actives();
  }

  bool reset(const Qubit& qubit) {
    return simulator->reset(qubit);
  }

  void X(const Qubit& qubit) {
    simulator->X(qubit);
  }

  void MCX(const Qubit& target, const std::vector<Qubit>& controls) {
    simulator->MCX(target, controls);
  }

  void Y(const Qubit& qubit) {
    simulator->Y(qubit);
  }

  void MCY(const Qubit& target, const std::vector<Qubit>& controls) {
    simulator->MCY(target, controls);
  }

  void Z(const Qubit& qubit) {
    simulator->Z(qubit);
  }

  void MCZ(const Qubit& target, const std::vector<Qubit>& controls) {
    simulator->MCZ(target, controls);
  }

  void H(const Qubit& qubit) {
    simulator->H(qubit);
  }

  void MCH(const Qubit& target, const std::vector<Qubit>& controls) {
    simulator->MCH(target, controls);
  }

  void S(const Qubit& qubit) {
    simulator->S(qubit);
  }

  void MCS(const Qubit& target, const std::vector<Qubit>& controls) {
    simulator->MCS(target, controls);
  }

  void T(const Qubit& qubit) {
    simulator->T(qubit);
  }

  void RX(const Qubit& qubit, RealType angle) {
    simulator->R(qubit, Basis::PauliX, angle);
  }

  void RY(const Qubit& qubit, RealType angle) {
    simulator->R(qubit, Basis::PauliY, angle);
  }

  void RZ(const Qubit& qubit, RealType angle) {
    simulator->R(qubit, Basis::PauliZ, angle);
  }

  void MC_RX(const Qubit& target, const std::vector<Qubit>& controls, RealType angle) {
    simulator->MCR(target, controls, Basis::PauliX, angle);
  }

  void MC_RY(const Qubit& target, const std::vector<Qubit>& controls, RealType angle) {
    simulator->MCR(target, controls, Basis::PauliY, angle);
  }

  void MC_RZ(const Qubit& target, const std::vector<Qubit>& controls, RealType angle) {
    simulator->MCR(target, controls, Basis::PauliZ, angle);
  }

  void MCT(const Qubit& target, const std::vector<Qubit>& controls) {
    simulator->MCT(target, controls);
  }

  void AdjS(const Qubit& qubit) {
    simulator->AdjS(qubit);
  }

  void MCAdjS(const Qubit& target, const std::vector<Qubit>& controls) {
    simulator->MCAdjS(target, controls);
  }

  void AdjT(const Qubit& qubit) {
    simulator->AdjT(qubit);
  }

  void MCAdjT(const Qubit& target, const std::vector<Qubit>& controls) {
    simulator->MCAdjT(target, controls);
  }

  void CNOT(const Qubit& control, const Qubit& target) {
    MCX(target, std::vector<Qubit>{control});
  }

  void MCCNOT(const Qubit& control, const Qubit& target, const std::vector<Qubit>& controls) {
    auto controlsCombined = controls;
    controlsCombined.push_back(control);
    MCX(target, controlsCombined);
  }

  void SWAP(const Qubit& qubit1, const Qubit& qubit2) {
    // Implementation with CNOT
    CNOT(qubit1, qubit2);
    CNOT(qubit2, qubit1);
    CNOT(qubit1, qubit2);
  }

  void MCSWAP(const Qubit& qubit1, const Qubit& qubit2, const std::vector<Qubit>& controls) {
    // Implementation with CNOT
    MCCNOT(qubit1, qubit2, controls);
    MCCNOT(qubit2, qubit1, controls);
    MCCNOT(qubit1, qubit2, controls);
  }

  void R(const Qubit& target, Basis basis, RealType angle) {
    simulator->R(target, basis, angle);
  }

  void MCR(const Qubit& target, const std::vector<Qubit>& controls, Basis basis, RealType angle) {
    simulator->MCR(target, controls, basis, angle);
  }

  void R1(const Qubit& target, RealType angle) {
    simulator->R1(target, angle);
  }

  void MCR1(const Qubit& target, const std::vector<Qubit>& controls, RealType angle) {
    simulator->MCR1(target, controls, angle);
  }

  void Exp(const Term& term, RealType angle) {
    // For empty term, perform phase shift
    if (term.empty()) {
      if (simulator->actives() == 0) {
        return;
      }

      // Arbitrary qubit is fine
      // W.L.O.G. choose id = 0
      R(simulator->firstActive(), Basis::PauliI, static_cast<std::uint64_t>(-2.0) * angle);

      return;
    }

    // Forward transform
    for (auto const& op : term) {
      auto const& target = op.first;

      switch(op.second) {
      case Basis::PauliI:
        {
          // I = S Z S
          throw std::runtime_error("Should contain no PauliI gate");
          break;
        }
      case Basis::PauliX:
        {
          // X = H Z H
          H(target);
          break;
        }
      case Basis::PauliY:
        {
          // Y = S H Z H AdjS
          AdjS(target);
          H(target);
          break;
        }
      case Basis::PauliZ:
        {
          // Do Nothing
          break;
        }
      }
    }

    // Forward CNOT
    for (std::uint64_t idx = 0; idx + 1 != term.size(); idx++) {
      CNOT(term[idx].first, term[idx+1].first);
    }

    // Rotate last qubit
    R(term.back().first, Basis::PauliZ, static_cast<RealType>(-2.0) * angle);

    // Backward CNOT
    for (std::uint64_t idx = 0; idx + 1 != term.size(); idx++) {
      CNOT(term[term.size()-idx-2].first, term[term.size()-idx-1].first);
    }

    // Backward transform
    for (auto const& op : term) {
      auto const& target = op.first;

      switch(op.second) {
      case Basis::PauliI:
        {
          // I = S Z S
          throw std::runtime_error("Should contain no PauliI gate");
          break;
        }
      case Basis::PauliX:
        {
          // X = H Z H
          H(target);
          break;
        }
      case Basis::PauliY:
        {
          // Y = S H Z H AdjS
          H(target);
          S(target);
          break;
        }
      case Basis::PauliZ:
        {
          // Do Nothing
          break;
        }
      }
    }
  }

  void MCExp(const Term& term, RealType angle, const std::vector<Qubit>& controls) {
    // For empty term, perform phase shift
    if (term.empty()) {
      if (simulator->actives() == 0) {
        return;
      }

      // Arbitrary qubit is fine
      // W.L.O.G. choose id = 0
      MCR(simulator->firstActive(), controls, Basis::PauliI, static_cast<std::uint64_t>(-2.0) * angle);

      return;
    }

    // Forward transform
    for (auto const& op : term) {
      auto const& target = op.first;

      switch(op.second) {
      case Basis::PauliI:
        {
          // I = S Z S
          throw std::runtime_error("Should contain no PauliI gate");
          break;
        }
      case Basis::PauliX:
        {
          // X = H Z H
          MCH(target, controls);
          break;
        }
      case Basis::PauliY:
        {
          // Y = S H Z H AdjS
          MCAdjS(target, controls);
          MCH(target, controls);
          break;
        }
      case Basis::PauliZ:
        {
          // Do Nothing
          break;
        }
      }
    }

    // Forward CNOT
    for (std::uint64_t idx = 0; idx + 1 != term.size(); idx++) {
      MCCNOT(term[idx].first, term[idx+1].first, controls);
    }

    // Rotate last qubit
    MCR(term.back().first, controls, Basis::PauliZ, static_cast<RealType>(-2.0) * angle);

    // Backward CNOT
    for (std::uint64_t idx = 0; idx + 1 != term.size(); idx++) {
      MCCNOT(term[term.size()-idx-2].first, term[term.size()-idx-1].first, controls);
    }

    // Backward transform
    for (auto const& op : term) {
      auto const& target = op.first;

      switch(op.second) {
      case Basis::PauliI:
        {
          // I = S Z S
          throw std::runtime_error("Should contain no PauliI gate");
          break;
        }
      case Basis::PauliX:
        {
          // X = H Z H
          MCH(target, controls);
          break;
        }
      case Basis::PauliY:
        {
          // Y = S H Z H AdjS
          MCH(target, controls);
          MCS(target, controls);
          break;
        }
      case Basis::PauliZ:
        {
          // Do Nothing
          break;
        }
      }
    }

  }

  bool M(const Qubit& target) {
    return simulator->M(target) == Result::One;
  }

  bool Measure(const Term& term) {
    // Check empty qubits
    if (term.empty()) {
      throw std::runtime_error("Can not measure with no qubits given");
    }

    // Forward transform
    for (auto const& op : term) {
      auto const& target = op.first;

      switch (op.second) {
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
          // Do Nothing
          break;
      }
    }

    // Forward CNOT
    for (std::uint64_t idx = 0; idx + 1 != term.size(); idx++) {
      CNOT(term[idx].first, term[idx+1].first);
    }

    // Measure last qubit
    auto res = M(term.back().first);

    // Backward CNOT
    for (std::uint64_t idx = 0; idx + 1 != term.size(); idx++) {
      CNOT(term[idx].first, term[idx+1].first);
    }

    // Backward transform
    for (auto const& op : term) {
      auto const& target = op.first;

      switch (op.second) {
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
          // Do Nothing
          break;
      }
    }

    return res;
  }

  bool Dump(const std::vector<Qubit>& qubits, std::string filepath) {
    return simulator->dumpToFile(qubits, filepath);
  }

  static Basis convertIntToBasis(Pauli pauli) {
    switch (pauli) {
    case Pauli::PauliI:
      return Basis::PauliI;
    case Pauli::PauliX:
      return Basis::PauliX;
    case Pauli::PauliY:
      return Basis::PauliY;
    case Pauli::PauliZ:
      return Basis::PauliZ;
    default:
      throw std::runtime_error(string_format("Unknown pauli value %d", static_cast<int>(pauli)));
    }
  }

  static Term getTerm(unsigned int program, long long n, enum Pauli paulis[], unsigned int q[]) {
    Term term;
    for (std::uint64_t idx = 0; idx < static_cast<std::uint64_t>(n); idx ++) {
      auto basis = convertIntToBasis(paulis[idx]);
      if (basis == Basis::PauliI) {
        // Do nothing
      } else {
        term.emplace_back(Qubit(static_cast<std::uint64_t>(program), static_cast<std::uint64_t>(q[idx])), basis);
      }
    }

    return term;
  }

  static inline Qubit wrapQubit(unsigned int program, unsigned int id) {
    return Qubit(static_cast<std::uint64_t>(program), static_cast<std::uint64_t>(id));
  }

  static inline std::vector<Qubit> wrapQubits(unsigned int program, const std::vector<unsigned int>& ids) {
    std::vector<Qubit> qubits;
    qubits.resize(ids.size(), QC::Qubit(program, 0UL));
    for (std::uint64_t i=0; i<ids.size(); i++) {
      assert(qubits[i].program == program);
      qubits[i].id = static_cast<std::uint64_t>(ids[i]);
    }
    return qubits;
  }

  // Additional interfaces
  void IBMX90(const Qubit& qubit, double phase) {
    simulator->R(qubit, Basis::PauliZ, -phase);
    simulator->R(qubit, Basis::PauliX, M_PI_2);
    simulator->R(qubit, Basis::PauliZ, phase);
  }

  void IBMCZ(const Qubit& control, const Qubit& target) {
    MCZ(target, std::vector<Qubit>{control});
  }

  void resetToZeroState() {
    simulator->resetToZeroState();
  }
}

// Implementation of the qc runtime function
void init() {
  QC::simulator = std::make_unique<QC::Simulator>();
}

void sim_init() {
  QC::simulator = std::make_unique<QC::Simulator>();
}

void seed(unsigned int seed) {
  QC::seed(static_cast<std::uint64_t>(seed));
}

void sim_seed(unsigned int seed) {
  QC::seed(static_cast<std::uint64_t>(seed));
}

long long random_choice(long long size, double p[]) {
  return static_cast<long long>(QC::randomChoice(static_cast<std::uint64_t>(size), p));
}

long long sim_random_choice(long long size, double p[]) {
  return static_cast<long long>(QC::randomChoice(static_cast<std::uint64_t>(size), p));
}

// Pr(One||ψ⟩)
double JointEnsembleProbability(long long n, enum Pauli b[], unsigned int q[]) {
  return static_cast<double>(QC::jointEnsembleProbability(QC::getTerm(0, n, b, q)));
}

double sim_JointEnsembleProbability(unsigned int program, long long n, enum Pauli b[], unsigned int q[]) {
  return static_cast<double>(QC::jointEnsembleProbability(QC::getTerm(program, n, b, q)));
}

void allocateQubit(unsigned int id) {
  QC::allocateQubit(QC::wrapQubit(0, id));
}

void sim_allocateQubit(unsigned int program, unsigned int id) {
  QC::allocateQubit(QC::wrapQubit(program, id));
}

bool release(unsigned int id) {
  return QC::deallocateQubit(QC::wrapQubit(0, id));
}

bool sim_release(unsigned int program, unsigned int id) {
  return QC::deallocateQubit(QC::wrapQubit(program, id));
}

int num_qubits() {
  return static_cast<int>(QC::numQubits());
}

bool reset(unsigned int qubit) {
  return QC::reset(QC::wrapQubit(0, qubit));
}

int sim_num_qubits() {
  return static_cast<int>(QC::numQubits());
}

bool reset(unsigned int program, unsigned int id) {
  return QC::reset(QC::wrapQubit(0, id));
}

bool sim_reset(unsigned int program, unsigned int id) {
  return QC::reset(QC::wrapQubit(program, id));
}

void X(unsigned int id) {
  QC::X(QC::wrapQubit(0, id));
}

void sim_X(unsigned int program, unsigned int id) {
  // std::cout << "PID: " << program << ", ID: " << id << std::endl;
  QC::X(QC::wrapQubit(program, id));
}

void MCX(unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCX(QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)));
}

void sim_MCX(unsigned int program, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCX(QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)));
}

void Y(unsigned int id) {
  QC::Y(QC::wrapQubit(0, id));
}

void sim_Y(unsigned int program, unsigned int id) {
  QC::Y(QC::wrapQubit(program, id));
}

void MCY(unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCY(QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)));
}

void sim_MCY(unsigned int program, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCY(QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)));
}

void Z(unsigned int id) {
  QC::Z(QC::wrapQubit(0, id));
}

void sim_Z(unsigned int program, unsigned int id) {
  QC::Z(QC::wrapQubit(program, id));
}

void MCZ(unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCZ(QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)));
}

void sim_MCZ(unsigned int program, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCZ(QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)));
}

void H(unsigned int id) {
  QC::H(QC::wrapQubit(0, id));
}

void sim_H(unsigned int program, unsigned int id) {
  QC::H(QC::wrapQubit(program, id));
}

void MCH(unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCH(QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)));
}

void sim_MCH(unsigned int program, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCH(QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)));
}

void S(unsigned int id) {
  QC::S(QC::wrapQubit(0, id));
}

void sim_S(unsigned int program, unsigned int id) {
  QC::S(QC::wrapQubit(program, id));
}

void MCS(unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCS(QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)));
}

void sim_MCS(unsigned int program, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCS(QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)));
}

void T(unsigned int id) {
  QC::T(QC::wrapQubit(0, id));
}

void sim_T(unsigned int program, unsigned int id) {
  QC::T(QC::wrapQubit(program, id));
}

void RX(unsigned int id, double angle) {
  QC::RX(QC::wrapQubit(0, id), static_cast<QC::RealType>(angle));
}

void sim_RX(unsigned int program, unsigned int id, double angle) {
  QC::RX(QC::wrapQubit(program, id), static_cast<QC::RealType>(angle));
}

void RY(unsigned int id, double angle) {
  QC::RY(QC::wrapQubit(0, id), static_cast<QC::RealType>(angle));
}

void sim_RY(unsigned int program, unsigned int id, double angle) {
  QC::RY(QC::wrapQubit(program, id), static_cast<QC::RealType>(angle));
}

void RZ(unsigned int id, double angle) {
  QC::RZ(QC::wrapQubit(0, id), static_cast<QC::RealType>(angle));
}

void sim_RZ(unsigned int program, unsigned int id, double angle) {
  QC::RZ(QC::wrapQubit(program, id), static_cast<QC::RealType>(angle));
}

void MCT(unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCT(QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)));
}

void sim_MCT(unsigned int program, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCT(QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)));
}

void MC_RX(unsigned int count, unsigned int controls[], unsigned int target, double angle) {
  QC::MC_RX(QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)), static_cast<QC::RealType>(angle));
}

void sim_MC_RX(unsigned int program, unsigned int count, unsigned int controls[], unsigned int target, double angle) {
  QC::MC_RX(QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)), static_cast<QC::RealType>(angle));
}

void MC_RY(unsigned int count, unsigned int controls[], unsigned int target, double angle) {
  QC::MC_RY(QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)), static_cast<QC::RealType>(angle));
}

void sim_MC_RY(unsigned int program, unsigned int count, unsigned int controls[], unsigned int target, double angle) {
  QC::MC_RY(QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)), static_cast<QC::RealType>(angle));
}

void MC_RZ(unsigned int count, unsigned int controls[], unsigned int target, double angle) {
  QC::MC_RZ(QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)), static_cast<QC::RealType>(angle));
}

void sim_MC_RZ(unsigned int program, unsigned int count, unsigned int controls[], unsigned int target, double angle) {
  QC::MC_RZ(QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)), static_cast<QC::RealType>(angle));
}

void AdjS(unsigned int id) {
  QC::AdjS(QC::wrapQubit(0, id));
}

void sim_AdjS(unsigned int program, unsigned int id) {
  QC::AdjS(QC::wrapQubit(program, id));
}

void MCAdjS(unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCAdjS(QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)));
}

void sim_MCAdjS(unsigned int program, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCAdjS(QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)));
}

void AdjT(unsigned int id) {
  QC::AdjT(QC::wrapQubit(0, id));
}

void sim_AdjT(unsigned int program, unsigned int id) {
  QC::AdjT(QC::wrapQubit(program, id));
}

void MCAdjT(unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCAdjT(QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)));
}

void sim_MCAdjT(unsigned int program, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCAdjT(QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)));
}

void SWAP(unsigned int id1, unsigned int id2) {
  QC::SWAP(QC::wrapQubit(0, id1), QC::wrapQubit(0, id2));
}

void sim_SWAP(unsigned int program, unsigned int id1, unsigned int id2) {
  QC::SWAP(QC::wrapQubit(program, id1), QC::wrapQubit(program, id2));
}

void MCSWAP(unsigned int count, unsigned int ctrls[], unsigned int qubit1, unsigned int qubit2) {
  QC::MCSWAP(QC::wrapQubit(0, qubit1), QC::wrapQubit(0, qubit2), QC::wrapQubits(0, std::vector<unsigned int>(ctrls, ctrls+count)));
}

void sim_MCSWAP(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit1, unsigned int qubit2) {
  QC::MCSWAP(QC::wrapQubit(program, qubit1), QC::wrapQubit(program, qubit2), QC::wrapQubits(program, std::vector<unsigned int>(ctrls, ctrls+count)));
}

void R(enum Pauli basis, double angle, unsigned int id) {
  QC::R(QC::wrapQubit(0, id), QC::convertIntToBasis(basis), static_cast<QC::RealType>(angle));
}

void sim_R(unsigned int program, enum Pauli basis, double angle, unsigned int id) {
  QC::R(QC::wrapQubit(program, id), QC::convertIntToBasis(basis), static_cast<QC::RealType>(angle));
}

void MCR(enum Pauli basis, double angle, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCR(
    QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)),
    QC::convertIntToBasis(basis), static_cast<QC::RealType>(angle)
  );
}

void sim_MCR(unsigned int program, enum Pauli basis, double angle, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCR(
    QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)),
    QC::convertIntToBasis(basis), static_cast<QC::RealType>(angle)
  );
}

void R1(double angle, unsigned int id) {
  QC::R1(QC::wrapQubit(0, id), static_cast<QC::RealType>(angle));
}

void sim_R1(unsigned int program, double angle, unsigned int id) {
  QC::R1(QC::wrapQubit(program, id), static_cast<QC::RealType>(angle));
}

void MCR1(double angle, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCR1(
    QC::wrapQubit(0, target), QC::wrapQubits(0, std::vector<unsigned int>(controls, controls+count)),
    static_cast<QC::RealType>(angle)
  );
}

void sim_MCR1(unsigned int program, double angle, unsigned int count, unsigned int controls[], unsigned int target) {
  QC::MCR1(
    QC::wrapQubit(program, target), QC::wrapQubits(program, std::vector<unsigned int>(controls, controls+count)),
    static_cast<QC::RealType>(angle)
  );
}

void Exp(unsigned int n, enum Pauli paulis[], double angle, unsigned int ids[]) {
  QC::Exp(QC::getTerm(0, n, paulis, ids), static_cast<QC::RealType>(angle));
}

void sim_Exp(unsigned int program, unsigned int n, enum Pauli paulis[], double angle, unsigned int ids[]) {
  QC::Exp(QC::getTerm(program, n, paulis, ids), static_cast<QC::RealType>(angle));
}

void MCExp(unsigned int n, enum Pauli paulis[], double angle, unsigned int nc, unsigned int ctrls[], unsigned int ids[]) {
  QC::MCExp(QC::getTerm(0, n, paulis, ids), static_cast<QC::RealType>(angle), QC::wrapQubits(0, std::vector<unsigned int>(ctrls, ctrls+nc)));
}

void sim_MCExp(unsigned int program, unsigned int n, enum Pauli paulis[], double angle, unsigned int nc, unsigned int ctrls[], unsigned int ids[]) {
  QC::MCExp(QC::getTerm(program, n, paulis, ids), static_cast<QC::RealType>(angle), QC::wrapQubits(program, std::vector<unsigned int>(ctrls, ctrls+nc)));
}

void ExpFrac(unsigned int n, enum Pauli paulis[], int nom, int den, unsigned int ids[]) {
  throw std::runtime_error("[ExpFrac] gate not implmented!");
}

void sim_ExpFrac(unsigned int program, unsigned int n, enum Pauli paulis[], int nom, int den, unsigned int ids[]) {
  throw std::runtime_error("[sim_ExpFrac] gate not implmented!");
}

void MCExpFrac(unsigned int n, enum Pauli paulis[], int nom, int den, unsigned int nc, unsigned int ctrls[], unsigned int ids[]) {
  throw std::runtime_error("[MCExpFrac] gate not implmented!");
}

void sim_MCExpFrac(unsigned int program, unsigned int n, enum Pauli paulis[], int nom, int den, unsigned int nc, unsigned int ctrls[], unsigned int ids[]) {
  throw std::runtime_error("[sim_MCExpFrac] gate not implmented!");
}

bool M(unsigned int id) {
  return QC::M(QC::wrapQubit(0, id));
}

bool sim_M(unsigned int program, unsigned int id) {
  return QC::M(QC::wrapQubit(program, id));
}

bool Measure(unsigned int n, enum Pauli paulis[], unsigned int targets[]) {
  return QC::Measure(QC::getTerm(0, n, paulis, targets));
}

bool sim_Measure(unsigned int program, unsigned int n, enum Pauli paulis[], unsigned int targets[]) {
  return QC::Measure(QC::getTerm(program, n, paulis, targets));
}

bool sim_Dump(unsigned int program, unsigned int n, unsigned int ids[], const char* filepath) {
  return QC::Dump(
    QC::wrapQubits(program, std::vector<unsigned int>(ids, ids+n)),
    std::string(filepath)
  );
}

// Implmetations of additional interfaces
void sim_IBM_X90(unsigned int program, unsigned int id, double phase) {
  QC::IBMX90(
    QC::wrapQubit(program, id),
    phase
  );
}

void sim_IBM_CZ(unsigned int program, unsigned int control, unsigned int target) {
  QC::IBMCZ(
    QC::wrapQubit(program, control),
    QC::wrapQubit(program, target)
  );
}

void resetToZeroState() {
  QC::resetToZeroState();
}