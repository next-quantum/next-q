#include "qc_runtime_v2.h"
#include "qc_types_v2.hpp"
#include "qc_simulator_default_cpu_sv_v2.hpp"

#ifdef ENABLE_BIREN
#include "qc_simulator_biren_gpu_sv_v2.hpp"
#endif

#ifdef ENABLE_MTHREADS
#include "qc_simulator_mthreads_gpu_sv_v2.hpp"
#endif

#ifdef ENABLE_METAX
#include "qc_simulator_metax_gpu_sv_v2.hpp"
#endif

#include <cassert>
#include <cmath>
#include <cstdint>

#include <complex>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

namespace QC_V2 {

template<typename ... Args>
std::string string_format( const std::string& format, Args ... args ) {
  size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
  std::unique_ptr<char[]> buf( new char[ size ] );
  snprintf( buf.get(), size, format.c_str(), args ... );
  return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

// A unique pointer to simulator object
std::unique_ptr<SimulatorV2> simulator = nullptr;

static inline Basis convertIntToBasis(PauliV2 const pauli) {
  switch (pauli) {
  case PauliV2::PauliIV2:
    return Basis::PauliI;
  case PauliV2::PauliXV2:
    return Basis::PauliX;
  case PauliV2::PauliYV2:
    return Basis::PauliY;
  case PauliV2::PauliZV2:
    return Basis::PauliZ;
  default:
    throw std::runtime_error(string_format("Unknown pauli value %d", static_cast<int>(pauli)));
  }
}

static inline Term convertArrayToTerm(std::uint64_t const n, PauliV2 const b[], unsigned int const q[]) {
  Term term(n);
  for (std::uint64_t i = 0; i < n; ++i) {
    term[i] = std::make_pair(q[i], convertIntToBasis(b[i]));
  }
  return term;
}

} // namespace QC_V2

// Implementations of external interfaces
void initWithQubitSize_v2(unsigned int qubit_size) {
  assert(QC_V2::simulator == nullptr);

  #ifdef ENABLE_METAX
  // Use Metax GPU backend when enabled
  QC_V2::simulator = std::make_unique<QC_V2::SimulatorMetaxGPUSVV2>(
    static_cast<std::uint64_t>(qubit_size)
  );
  #elif ENABLE_MTHREADS
  // Use Moore Thread GPU backend when enabled
  QC_V2::simulator = std::make_unique<QC_V2::SimulatorMooreThreadGPUSVV2>(
    static_cast<std::uint64_t>(qubit_size)
  );
  #elif ENABLE_BIREN
  // Use Biren GPU backend when enabled
  QC_V2::simulator = std::make_unique<QC_V2::SimulatorBirenGPUSVV2>(
    static_cast<std::uint64_t>(qubit_size)
  );
  #else
  // Use CPU backend by default
  QC_V2::simulator = std::make_unique<QC_V2::SimulatorDefaultCPUSVV2>(
    static_cast<std::uint64_t>(qubit_size)
  );
  #endif

  assert(QC_V2::simulator != nullptr);
}

void seed_v2(unsigned int seed) {
  QC_V2::simulator->seed(seed);
}

int num_qubits_v2() {
  return static_cast<int>(QC_V2::simulator->numberOfQubits());
}

bool reset_v2(unsigned int qubit) {
  if (qubit >= QC_V2::simulator->numberOfQubits()) {
    return false;
  }
  QC_V2::simulator->resetQubit(qubit);
  return true;
}

void resetToZeroState_v2() {
  QC_V2::simulator->resetToZeroState();
}

void release_v2() {
  assert(QC_V2::simulator != nullptr);
  // [BUG 2026.2.4 9:35] can not use release, which release the ownership, to clear the memory, use reset
  QC_V2::simulator.reset();
  QC_V2::simulator = nullptr;
  assert(QC_V2::simulator == nullptr);
}

// private functions
static bool checkQubitRange(unsigned int const qubit) {
  return (std::uint64_t)qubit < QC_V2::simulator->numberOfQubits();
}

static bool checkQubitsRange(unsigned int const count, unsigned int const qubits[]) {
  for (unsigned int idx = 0; idx < count; idx++) {
    if (qubits[idx] >= QC_V2::simulator->numberOfQubits()) {
      return false;
    }
  }

  return true;
}

void X_v2(unsigned int qubit) {
  assert(checkQubitRange(qubit));

  QC_V2::simulator->X((std::uint64_t)qubit);
}

void MCX_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCX((std::uint64_t)qubit, std::vector<std::uint64_t>(ctrls, ctrls+count));
}

void Y_v2(unsigned int qubit) {
  assert(checkQubitRange(qubit));
  
  QC_V2::simulator->Y((std::uint64_t)qubit);
}

void MCY_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCY((std::uint64_t)qubit, std::vector<std::uint64_t>(ctrls, ctrls+count));
}

void Z_v2(unsigned int qubit) {
  assert(checkQubitRange(qubit));
  
  QC_V2::simulator->Z((std::uint64_t)qubit);
}

void MCZ_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCZ((std::uint64_t)qubit, std::vector<std::uint64_t>(ctrls, ctrls+count));
}

void H_v2(unsigned int qubit) {
  assert(checkQubitRange(qubit));
  
  QC_V2::simulator->H((std::uint64_t)qubit);
}

void MCH_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCH((std::uint64_t)qubit, std::vector<std::uint64_t>(ctrls, ctrls+count));
}

void S_v2(unsigned int qubit) {
  assert(checkQubitRange(qubit));
  
  QC_V2::simulator->S((std::uint64_t)qubit);
}

void MCS_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCS((std::uint64_t)qubit, std::vector<std::uint64_t>(ctrls, ctrls+count));
}

void T_v2(unsigned int qubit) {
  assert(checkQubitRange(qubit));
  
  QC_V2::simulator->T((std::uint64_t)qubit);
}

void MCT_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCT((std::uint64_t)qubit, std::vector<std::uint64_t>(ctrls, ctrls+count));
}

void AdjS_v2(unsigned int qubit) {
  assert(checkQubitRange(qubit));
  
  QC_V2::simulator->AdjS((std::uint64_t)qubit);
}

void MCAdjS_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCAdjS((std::uint64_t)qubit, std::vector<std::uint64_t>(ctrls, ctrls+count));
}

void AdjT_v2(unsigned int qubit) {
  assert(checkQubitRange(qubit));
  
  QC_V2::simulator->AdjT((std::uint64_t)qubit);
}

void MCAdjT_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCAdjT((std::uint64_t)qubit, std::vector<std::uint64_t>(ctrls, ctrls+count));
}

void SWAP_v2(unsigned int qubit1, unsigned int qubit2) {
  assert(checkQubitRange(qubit1));
  assert(checkQubitRange(qubit2));

  QC_V2::simulator->SWAP(qubit1, qubit2);
}

void MCSWAP_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit1, unsigned int qubit2) {
  assert(checkQubitRange(qubit1));
  assert(checkQubitRange(qubit2));

  QC_V2::simulator->MCSWAP((std::uint64_t)qubit1, (std::uint64_t)qubit2, std::vector<std::uint64_t>(ctrls, ctrls+count));
}

void R_v2(enum PauliV2 basis, double angle, unsigned int qubit) {
  assert(checkQubitRange(qubit));

  QC_V2::simulator->R(qubit, QC_V2::convertIntToBasis(basis), angle);
}

void MCR_v2(enum PauliV2 basis, double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCR(qubit, std::vector<std::uint64_t>(ctrls, ctrls+count), QC_V2::convertIntToBasis(basis), angle);
}

void R1_v2(double angle, unsigned int qubit) {
  assert(checkQubitRange(qubit));

  QC_V2::simulator->R1(qubit, angle);
}

void MCR1_v2(double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));

  QC_V2::simulator->MCR1(qubit, std::vector<std::uint64_t>(ctrls, ctrls+count), angle);
}

void RX_v2(double angle, unsigned int qubit) {
  assert(checkQubitRange(qubit));

  QC_V2::simulator->R(qubit, QC_V2::Basis::PauliX, angle);
}

void RY_v2(double angle, unsigned int qubit) {
  assert(checkQubitRange(qubit));

  QC_V2::simulator->R(qubit, QC_V2::Basis::PauliY, angle);
}

void RZ_v2(double angle, unsigned int qubit) {
  assert(checkQubitRange(qubit));

  QC_V2::simulator->R(qubit, QC_V2::Basis::PauliZ, angle);
}

void MCRX_v2(double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCR(qubit, std::vector<std::uint64_t>(ctrls, ctrls+count), QC_V2::Basis::PauliX, angle);
}

void MCRY_v2(double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCR(qubit, std::vector<std::uint64_t>(ctrls, ctrls+count), QC_V2::Basis::PauliY, angle);
}

void MCRZ_v2(double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit) {
  assert(checkQubitRange(qubit));
  assert(checkQubitsRange(count, ctrls));

  QC_V2::simulator->MCR(qubit, std::vector<std::uint64_t>(ctrls, ctrls+count), QC_V2::Basis::PauliZ, angle);
}

bool M_v2(unsigned int qubit, enum PauliV2 basis) {
  return QC_V2::simulator->M(qubit, QC_V2::convertIntToBasis(basis)) == QC_V2::Result::One;
}

// bool Measure_v2(unsigned int n, enum PauliV2 b[], unsigned int q[]) {
//   assert(checkQubitsRange(n, q));

//   return QC_V2::simulator->Measure(n, std::vector<QC_V2::Basis>(b, b+n), std::vector<std::uint64_t>(q, q+n)) == QC_V2::Result::One;
// }

double JointEnsembleProbability_v2(long long n, enum PauliV2 b[], unsigned int q[]) {
  auto term = QC_V2::convertArrayToTerm(n, b, q);
  // Pr(One||ψ⟩)
  return QC_V2::simulator->JointEnsembleProbability(term);
}

void getStateVector_v2(double* real_part, double* imag_part, int* size) {
  assert(QC_V2::simulator != nullptr);
  
  std::vector<std::complex<double>> state;
  QC_V2::simulator->getStateVector(state);
  
  *size = static_cast<int>(state.size());
  
  if (real_part != nullptr && imag_part != nullptr) {
    for (std::size_t i = 0; i < state.size(); ++i) {
      real_part[i] = state[i].real();
      imag_part[i] = state[i].imag();
    }
  }
}
