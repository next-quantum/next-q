#include <cassert>

#include <iomanip>
#include <iostream>

#include <musa_runtime.h>

#include "musa_utils.hpp"
#include "qc_simulator_moorethread_gpu_sv_v2.hpp"
#include "qc_kernels_musa.muh"

namespace QC_V2 {

using namespace SimulatorMooreThreadGPUSVV2TypeDef;

const RealType RealEPS = 1e-6;

SimulatorMooreThreadGPUSVV2::SimulatorMooreThreadGPUSVV2(std::uint64_t nQubits_): random(std::bind(std::uniform_real_distribution<RealType>(0., 1.), std::ref(engine))) {
  nQubits = nQubits_;

  // display GPU device information
  _mDeviceID = 0;
  musaError_t  musaStatus = musaSetDevice(_mDeviceID);
  if (musaStatus != musaSuccess) {
    std::cerr
      << "[musaSetDevice] failed for device id: " << _mDeviceID
      << ", with error: " << musaGetErrorString(musaStatus) << std::endl;
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to set device");
  }
  musaGetDeviceProperties(&_mDeviceProp, _mDeviceID);

  // allocate device memory
  _mHostStateVector.resize(1UL << nQubits_, muFloatComplex{0.0f, 0.0f});
  _mHostStateVectorBuffer.resize(1UL << nQubits_, muFloatComplex{0.0f, 0.0f});

  _mHostStateVector.assign(_mHostStateVector.size(), {0.0f, 0.0f});
  _mHostStateVectorBuffer.assign(_mHostStateVectorBuffer.size(), {0.0f, 0.0f});

  // initialize to |0> state
  _mHostStateVector[0] = muFloatComplex{1.0f, 0.0f};
  _mHostStateVectorBuffer[0] = muFloatComplex{1.0f, 0.0f};

  if (!allocateDeviceMemory((void**)&_mDeviceStateVector, _mHostStateVector.size() * sizeof(muFloatComplex), "SimulatorMooreThreadGPUSVV2::_mDeviceStateVector")) {
    _mDeviceStateVector = nullptr;
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to allocate device memory for _mDeviceStateVector");
  }
  if (!allocateDeviceMemory((void**)&_mDeviceStateVectorBuffer, _mHostStateVectorBuffer.size() * sizeof(muFloatComplex), "SimulatorMooreThreadGPUSVV2::_mDeviceStateVectorBuffer")) {
    _mDeviceStateVectorBuffer = nullptr;
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to allocate device memory for _mDeviceStateVectorBuffer");
  }
  if (!allocateDeviceMemory((void**)&_mDeviceOutput, sizeof(float), "SimulatorMooreThreadGPUSVV2::_mDeviceOutput")) {
    _mDeviceOutput = nullptr;
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to allocate device memory for _mDeviceOutput");
  }

  if (!resetDeviceMemory((void**)&_mDeviceOutput, sizeof(float), "SimulatorMooreThreadGPUSVV2::_mDeviceOutput")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to reset device memory for _mDeviceOutput");
  }

  if (!copyHostMemoryToDevice(_mDeviceStateVector, _mHostStateVector.data(), _mHostStateVector.size() * sizeof(muFloatComplex), "SimulatorMooreThreadGPUSVV2::_mDeviceStateVector")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to copy host memory to device memory for _mDeviceStateVector");
  }
  if (!copyHostMemoryToDevice(_mDeviceStateVectorBuffer, _mHostStateVectorBuffer.data(), _mHostStateVectorBuffer.size() * sizeof(muFloatComplex), "SimulatorMooreThreadGPUSVV2::_mDeviceStateVectorBuffer")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to copy host memory to device memory for _mDeviceStateVectorBuffer");
  }

  // kernel function attributes
  if (!setKernelFunctionAttributes(_mApplyGateOneFunctionAttrs, "apply_gate_one", (void*)MOORETHREAD_GPU::apply_gate_one)) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to set kernel function attributes for _mApplyGateOneFunctionAttrs");
  }
  if (!setKernelFunctionAttributes(_mFindProbabilityOfOutcomeZeroFunctionAttrs, "find_probability_of_outcome_zero", (void*)MOORETHREAD_GPU::find_probability_of_outcome_zero)) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to set kernel function attributes for _mFindProbabilityOfOutcomeZeroFunctionAttrs");
  }
  if (!setKernelFunctionAttributes(_mCollapseToOutcomeScaleFunctionAttrs, "collapse_to_outcome_scale", (void*)MOORETHREAD_GPU::collapse_to_outcome_scale)) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to set kernel function attributes for _mCollapseToOutcomeScaleFunctionAttrs");
  }
  if (!setKernelFunctionAttributes(_mStoreStateVectorFunctionAttrs, "store_state_vector", (void*)MOORETHREAD_GPU::store_state_vector)) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to set kernel function attributes for _mStoreStateVectorFunctionAttrs");
  }
  if (!setKernelFunctionAttributes(_mDotStateVectorFunctionAttrs, "dot_state_vector", (void*)MOORETHREAD_GPU::dot_state_vector)) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to set kernel function attributes for _mDotStateVectorFunctionAttrs");
  }
  if (!setKernelFunctionAttributes(_mNormStateVectorFunctionAttrs, "norm_state_vector", (void*)MOORETHREAD_GPU::norm_state_vector)) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to set kernel function attributes for _mNormStateVectorFunctionAttrs");
  }

  // create events
  musaEventCreate(&_mEventStart);
  musaEventCreate(&_mEventStop);
}

SimulatorMooreThreadGPUSVV2::~SimulatorMooreThreadGPUSVV2() {
  // release MUSA memory
  _mHostStateVector.clear();
  _mHostStateVectorBuffer.clear();

  if (_mDeviceStateVector != nullptr) {
    releaseDeviceMemory((void**)&_mDeviceStateVector, "SimulatorMooreThreadGPUSVV2::_mDeviceStateVector", true);
    _mDeviceStateVector = nullptr;
  }
  if (_mDeviceStateVectorBuffer != nullptr) {
    releaseDeviceMemory((void**)&_mDeviceStateVectorBuffer, "SimulatorMooreThreadGPUSVV2::_mDeviceStateVectorBuffer", true);
    _mDeviceStateVectorBuffer = nullptr;
  }
  if (_mDeviceOutput != nullptr) {
    releaseDeviceMemory((void**)&_mDeviceOutput, "SimulatorMooreThreadGPUSVV2::_mDeviceOutput", true);
    _mDeviceOutput = nullptr;
  }
}

std::uint64_t SimulatorMooreThreadGPUSVV2::getControlMask(std::vector<std::uint64_t> const& ctrls){
  std::uint64_t ctrlmask = 0;
  for (auto c : ctrls)
    ctrlmask |= (1UL << c);
  return ctrlmask;
}

void SimulatorMooreThreadGPUSVV2::applyGateOne(
  muFloatComplex matrix[4],
  uint32_t const target, 
  uint32_t const controlMask
) {
  uint64_t threadsPerBlock = 0, blocksPerGrid = 0;
  calculateKernelLaunchSizeType3(blocksPerGrid, threadsPerBlock, _mApplyGateOneFunctionAttrs.maxThreadsPerBlock);

  auto const m00 = matrix[0];
  auto const m01 = matrix[1];
  auto const m10 = matrix[2];
  auto const m11 = matrix[3];

  musaEventRecord(_mEventStart);

  MOORETHREAD_GPU::apply_gate_one<<<blocksPerGrid, threadsPerBlock>>>(
    _mDeviceStateVector,
    m00, m01, m10, m11,
    target, controlMask
  );

  musaEventRecord(_mEventStop);
  musaDeviceSynchronize();
  float milliseconds = 0.0f;
  musaEventElapsedTime(&milliseconds, _mEventStart, _mEventStop);

  #ifdef DEBUG
    printf(
      "[apply_gate_one] GPU time is %0.10g ns, per state vector is %0.10g ns\n",
      double(milliseconds*1E6),
      double(milliseconds*1E6)/((float)(1 << nQubits))
    );
  #endif

}

RealType SimulatorMooreThreadGPUSVV2::findProbabilityOfOutcome(std::uint64_t target, Result outcome) const {
  if (!resetDeviceMemory((void**)&_mDeviceOutput, sizeof(float), "SimulatorMooreThreadGPUSVV2::_mDeviceOutput")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to reset device memory for _mDeviceOutput");
  }

  uint64_t threadsPerBlock = 0, blocksPerGrid = 0;
  calculateKernelLaunchSizeType3(blocksPerGrid, threadsPerBlock, _mFindProbabilityOfOutcomeZeroFunctionAttrs.maxThreadsPerBlock);

  musaEventRecord(_mEventStart);

  MOORETHREAD_GPU::find_probability_of_outcome_zero<<<blocksPerGrid, threadsPerBlock>>>(
    _mDeviceStateVector,
    _mDeviceOutput,
    target
  );

  musaEventRecord(_mEventStop);
  musaDeviceSynchronize();
  float milliseconds = 0.0f;
  musaEventElapsedTime(&milliseconds, _mEventStart, _mEventStop);

  #ifdef DEBUG
    printf(
      "[find_probability_of_outcome_zero] GPU time is %0.10g ns, per state vector is %0.10g ns\n",
      double(milliseconds*1E6),
      double(milliseconds*1E6)/((float)(1 << nQubits))
    );
  #endif

  float totalProbability = 0.0f;
  if (!copyDeviceMemoryToHost(&totalProbability, _mDeviceOutput, sizeof(float), "_mDeviceOutput")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to copy device memory to host memory for _mDeviceOutput");
  }

  if (outcome == Result::One) {
    return 1.0f - totalProbability;
  }

  return (RealType)totalProbability;
}

void SimulatorMooreThreadGPUSVV2::collapseToOutcome(std::uint64_t target, RealType normFactor, Result outcome, bool noNorm) {
  uint64_t threadsPerBlock = 0, blocksPerGrid = 0;
  calculateKernelLaunchSizeType3(blocksPerGrid, threadsPerBlock, _mCollapseToOutcomeScaleFunctionAttrs.maxThreadsPerBlock);

  musaEventRecord(_mEventStart);

  MOORETHREAD_GPU::collapse_to_outcome_scale<<<blocksPerGrid, threadsPerBlock>>>(
    _mDeviceStateVector,
    target,
    (float)normFactor,
    outcome == Result::One,
    noNorm
  );

  musaEventRecord(_mEventStop);
  musaDeviceSynchronize();
  float milliseconds = 0.0f;
  musaEventElapsedTime(&milliseconds, _mEventStart, _mEventStop);

  #ifdef DEBUG
    printf(
      "[collapse_to_outcome_scale] GPU time is %0.10g ns, per state vector is %0.10g ns\n",
      double(milliseconds*1E6),
      double(milliseconds*1E6)/((float)(1 << nQubits))
    );
  #endif

}

void SimulatorMooreThreadGPUSVV2::store() {
  uint64_t threadsPerBlock = 0, blocksPerGrid = 0;
  calculateKernelLaunchSizeType2(blocksPerGrid, threadsPerBlock, _mStoreStateVectorFunctionAttrs.maxThreadsPerBlock);

  musaEventRecord(_mEventStart);

  MOORETHREAD_GPU::store_state_vector<<<blocksPerGrid, threadsPerBlock>>>(
    _mDeviceStateVectorBuffer,
    _mDeviceStateVector
  );

  musaEventRecord(_mEventStop);
  musaDeviceSynchronize();
  float milliseconds = 0.0f;
  musaEventElapsedTime(&milliseconds, _mEventStart, _mEventStop);

  #ifdef DEBUG
    printf(
      "[store_state_vector] GPU time is %0.10g ns, per state vector is %0.10g ns\n",
      double(milliseconds*1E6),
      double(milliseconds*1E6)/((float)(1 << nQubits))
    );
  #endif
}

void SimulatorMooreThreadGPUSVV2::swap() {
  std::swap(_mDeviceStateVector, _mDeviceStateVectorBuffer);
  std::swap(_mHostStateVector, _mHostStateVectorBuffer);
}

RealType SimulatorMooreThreadGPUSVV2::dot() const {
  uint64_t threadsPerBlock = 0, blocksPerGrid = 0;
  calculateKernelLaunchSizeType2(blocksPerGrid, threadsPerBlock, _mDotStateVectorFunctionAttrs.maxThreadsPerBlock);
  
  if (!resetDeviceMemory((void**)&_mDeviceOutput, sizeof(float), "SimulatorMooreThreadGPUSVV2::_mDeviceOutput")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to reset device memory for _mDeviceOutput");
  }

  musaEventRecord(_mEventStart);

  MOORETHREAD_GPU::dot_state_vector<<<blocksPerGrid, threadsPerBlock>>>(
    _mDeviceStateVector,
    _mDeviceStateVectorBuffer,
    _mDeviceOutput
  );

  musaEventRecord(_mEventStop);
  musaDeviceSynchronize();
  float milliseconds = 0.0f;
  musaEventElapsedTime(&milliseconds, _mEventStart, _mEventStop);

  #ifdef DEBUG
    printf(
      "[dot_state_vector] GPU time is %0.10g ns, per state vector is %0.10g ns\n",
      double(milliseconds*1E6),
      double(milliseconds*1E6)/((float)(1 << nQubits))
    );
  #endif

  float sum = 0.0f;
  if (!copyDeviceMemoryToHost(&sum, _mDeviceOutput, sizeof(float), "_mDeviceOutput")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to copy device memory to host memory for _mDeviceOutput");
  }
  
  return (RealType)sum;
}

void SimulatorMooreThreadGPUSVV2::dump(std::ostream& os) const {
  if (!copyDeviceMemoryToHost((void*)_mHostStateVector.data(), _mDeviceStateVector, _mHostStateVector.size() * sizeof(muFloatComplex), "_mDeviceStateVector")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to copy device memory to host memory for _mDeviceStateVector");
  }

  auto const& vec = _mHostStateVector;
  
  os << "State vector (size " << vec.size() << "): \n";
  os << "Norm: " << norm() << "\n";
  assert(!vec.empty());
  for (std::size_t i=0; i != vec.size(); ++i) {
    os << "  " << std::fixed << std::setprecision(8) << vec[i] << "|" << (i) << ">" << "\n";
  }
  os << std::endl;
}

void SimulatorMooreThreadGPUSVV2::dump(std::vector<std::uint64_t> pos, std::ostream& os) const {
  if (!copyDeviceMemoryToHost((void*)_mHostStateVector.data(), _mDeviceStateVector, _mHostStateVector.size() * sizeof(muFloatComplex), "_mDeviceStateVector")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to copy device memory to host memory for _mDeviceStateVector");
  }

  auto const& vec = _mHostStateVector;

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
        << vec[m].x * vec[m].x + vec[m].y * vec[m].y << " |";
    for (std::size_t i=0; i != pos.size(); i++) {
      os << bits[i];
    }
    os << ">" << "\n";
  }
  
  os << std::endl;
}

RealType SimulatorMooreThreadGPUSVV2::norm() const {
  uint64_t threadsPerBlock = 0, blocksPerGrid = 0;
  calculateKernelLaunchSizeType2(blocksPerGrid, threadsPerBlock, _mNormStateVectorFunctionAttrs.maxThreadsPerBlock);

  if (!resetDeviceMemory((void**)&_mDeviceOutput, sizeof(float), "SimulatorMooreThreadGPUSVV2::_mDeviceOutput")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to reset device memory for _mDeviceOutput");
  }

  musaEventRecord(_mEventStart);

  MOORETHREAD_GPU::norm_state_vector<<<blocksPerGrid, threadsPerBlock>>>(
    _mDeviceStateVector,
    _mDeviceOutput
  );

  musaEventRecord(_mEventStop);
  musaDeviceSynchronize();
  float milliseconds = 0.0f;
  musaEventElapsedTime(&milliseconds, _mEventStart, _mEventStop);

  #ifdef DEBUG
    printf(
      "[norm_state_vector] GPU time is %0.10g ns, per state vector is %0.10g ns\n",
      double(milliseconds*1E6),
      double(milliseconds*1E6)/((float)(1 << nQubits))
    );
  #endif

  float sum = 0.0f;
  if (!copyDeviceMemoryToHost(&sum, _mDeviceOutput, sizeof(float), "_mDeviceOutput")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to copy device memory to host memory for _mDeviceOutput");
  }
  
  return (RealType)sum;
}

bool SimulatorMooreThreadGPUSVV2::dumpToFile(std::vector<std::uint64_t> const& qubits, std::string filepath) {
  std::cout << "Dump to filepath " << filepath << " (but use cout for now)" << std::endl;

  dump(qubits, std::cout);

  return true;
}

void SimulatorMooreThreadGPUSVV2::resetToZeroState() {
  _mHostStateVector.assign(_mHostStateVector.size(), {0.0f, 0.0f});
  _mHostStateVectorBuffer.assign(_mHostStateVectorBuffer.size(), {0.0f, 0.0f});

  if (!resetDeviceMemory((void**)&_mDeviceStateVector, _mHostStateVector.size() * sizeof(muFloatComplex), "SimulatorMooreThreadGPUSVV2::_mDeviceStateVector")) {
    _mDeviceStateVector = nullptr;
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to reset device memory for _mDeviceStateVector");
  }
  if (!resetDeviceMemory((void**)&_mDeviceStateVectorBuffer, _mHostStateVectorBuffer.size() * sizeof(muFloatComplex), "SimulatorMooreThreadGPUSVV2::_mDeviceStateVectorBuffer")) {
    _mDeviceStateVectorBuffer = nullptr;
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to reset device memory for _mDeviceStateVectorBuffer");
  }

  muFloatComplex const one = {1.0f, 0.0f};
  if (!copyHostMemoryToDevice(_mDeviceStateVector, &one, sizeof(muFloatComplex), "SimulatorMooreThreadGPUSVV2::_mDeviceStateVector")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to copy host memory to device memory for _mDeviceStateVector");
  }
  if (!copyHostMemoryToDevice(_mDeviceStateVectorBuffer, &one, sizeof(muFloatComplex), "SimulatorMooreThreadGPUSVV2::_mDeviceStateVectorBuffer")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to copy host memory to device memory for _mDeviceStateVectorBuffer");
  }

  _mHostStateVector[0] = one;
  _mHostStateVectorBuffer[0] = one;
}

void SimulatorMooreThreadGPUSVV2::resetQubit(std::uint64_t target) {
  auto const probabilityOfZero = findProbabilityOfOutcome(target, Result::Zero);
  bool const noNorm = (probabilityOfZero == 0.0f);
  collapseToOutcome(target, 1.0 / std::sqrt(probabilityOfZero), Result::Zero, noNorm);
}

void SimulatorMooreThreadGPUSVV2::seed(std::uint64_t seed) {
  engine.seed(seed);
}

std::uint64_t SimulatorMooreThreadGPUSVV2::numberOfQubits() const {
  return nQubits;
}

void SimulatorMooreThreadGPUSVV2::multiControlledGate(const Unitary1QType& matrix, std::uint64_t target, const std::vector<std::uint64_t>& controls) {
  for (auto control : controls) {
    assert(control != target);
  }

  #ifdef DEBUG
    std::cout << "[Before multi controlled gate]" << std::endl;
    dump(std::cout);
  #endif

  uint64_t const controlMask = getControlMask(controls);

  muFloatComplex matrixForBackend[4] = {
    muFloatComplex{static_cast<float>(matrix[0].real()), static_cast<float>(matrix[0].imag())},
    muFloatComplex{static_cast<float>(matrix[1].real()), static_cast<float>(matrix[1].imag())},
    muFloatComplex{static_cast<float>(matrix[2].real()), static_cast<float>(matrix[2].imag())},
    muFloatComplex{static_cast<float>(matrix[3].real()), static_cast<float>(matrix[3].imag())}
  };

  applyGateOne(
    matrixForBackend,
    static_cast<std::uint32_t>(target),
    static_cast<std::uint32_t>(controlMask)
  );

  #ifdef DEBUG
    std::cout << "[After multi controlled gate]" << std::endl;
    dump(std::cout);
  #endif
}

Result SimulatorMooreThreadGPUSVV2::measureZWithStats(std::uint64_t const target, RealType& stateProb) {
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

  collapseToOutcome(target, 1.0 / std::sqrt(stateProb), outcome);

  return outcome;
}

Result SimulatorMooreThreadGPUSVV2::measureWithStats(std::uint64_t const target, Basis const basis, RealType& stateProb) {
  switch(basis) {
    case Basis::PauliI:
      S(target);
      break;
    case Basis::PauliX:
      H(target);
      break;
    case Basis::PauliY:
      AdjS(target);
      H(target);
      break;
    case Basis::PauliZ:
      break;
  }

  auto const res = measureZWithStats(target, stateProb);

  switch(basis) {
    case Basis::PauliI:
      S(target);
      break;
    case Basis::PauliX:
      H(target);
      break;
    case Basis::PauliY:
      H(target);
      S(target);
      break;
    case Basis::PauliZ:
      break;
  }

  return res;
}

Result SimulatorMooreThreadGPUSVV2::measure(std::uint64_t const target, Basis const basis) {
  RealType stateProb = 0.0;
  return measureWithStats(target, basis, stateProb);
}

void SimulatorMooreThreadGPUSVV2::applyTerm(Term const& term) {
  for (auto const& op : term) {
    auto const pos = op.first;

    switch (op.second) {
      case Basis::PauliI:
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

RealType SimulatorMooreThreadGPUSVV2::expectation(const Term& term) {
  store();
  applyTerm(term);
  auto const value = dot();
  swap();

  #ifdef DEBUG
    dump(std::cout);
  #endif

  return value;
}

void SimulatorMooreThreadGPUSVV2::X(std::uint64_t const target) {
  multiControlledGate(XGate, target, std::vector<std::uint64_t>{});
}

void SimulatorMooreThreadGPUSVV2::MCX(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(XGate, target, controls);
}

void SimulatorMooreThreadGPUSVV2::Y(std::uint64_t const target) {
  multiControlledGate(YGate, target, std::vector<std::uint64_t>{});
}

void SimulatorMooreThreadGPUSVV2::MCY(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(YGate, target, controls);
}

void SimulatorMooreThreadGPUSVV2::Z(std::uint64_t const target) {
  multiControlledGate(ZGate, target, std::vector<std::uint64_t>{});
}

void SimulatorMooreThreadGPUSVV2::MCZ(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(ZGate, target, controls);
}

void SimulatorMooreThreadGPUSVV2::H(std::uint64_t const target) {
  multiControlledGate(HGate, target, std::vector<std::uint64_t>{});
}

void SimulatorMooreThreadGPUSVV2::MCH(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(HGate, target, controls);
}

void SimulatorMooreThreadGPUSVV2::S(std::uint64_t const target) {
  multiControlledGate(SGate, target, std::vector<std::uint64_t>{});
}

void SimulatorMooreThreadGPUSVV2::MCS(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(SGate, target, controls);
}

void SimulatorMooreThreadGPUSVV2::T(std::uint64_t const target) {
  multiControlledGate(TGate, target, std::vector<std::uint64_t>{});
}

void SimulatorMooreThreadGPUSVV2::MCT(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(TGate, target, controls);
}

void SimulatorMooreThreadGPUSVV2::AdjS(std::uint64_t const target) {
  multiControlledGate(AdjSGate, target, std::vector<std::uint64_t>{});
}

void SimulatorMooreThreadGPUSVV2::MCAdjS(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(AdjSGate, target, controls);
}

void SimulatorMooreThreadGPUSVV2::AdjT(std::uint64_t const target) {
  multiControlledGate(AdjTGate, target, std::vector<std::uint64_t>{});
}

void SimulatorMooreThreadGPUSVV2::MCAdjT(std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  multiControlledGate(AdjTGate, target, controls);
}

void SimulatorMooreThreadGPUSVV2::CNOT(std::uint64_t const control, std::uint64_t const target) {
  MCX(target, std::vector<std::uint64_t>{control});
}

void SimulatorMooreThreadGPUSVV2::MCCNOT(std::uint64_t const control, std::uint64_t const target, std::vector<std::uint64_t> const& controls) {
  auto controlsCombined = controls;
  controlsCombined.push_back(control);
  MCX(target, controlsCombined);
}

void SimulatorMooreThreadGPUSVV2::SWAP(std::uint64_t const qubit1, std::uint64_t const qubit2) {
  CNOT(qubit1, qubit2);
  CNOT(qubit2, qubit1);
  CNOT(qubit1, qubit2);
}

void SimulatorMooreThreadGPUSVV2::MCSWAP(std::uint64_t const qubit1, std::uint64_t const qubit2, std::vector<std::uint64_t> const& controls) {
  MCCNOT(qubit1, qubit2, controls);
  MCCNOT(qubit2, qubit1, controls);
  MCCNOT(qubit1, qubit2, controls);
}

void SimulatorMooreThreadGPUSVV2::R(std::uint64_t const target, Basis const basis, RealType const angle) {
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

void SimulatorMooreThreadGPUSVV2::MCR(std::uint64_t const target, std::vector<std::uint64_t> const& controls, Basis const basis, RealType const angle) {
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

void SimulatorMooreThreadGPUSVV2::R1(std::uint64_t const target, RealType const angle) {
  multiControlledGate(R1Gate(angle), target, std::vector<std::uint64_t>{});
}

void SimulatorMooreThreadGPUSVV2::MCR1(std::uint64_t const target, std::vector<std::uint64_t> const& controls, RealType const angle) {
  multiControlledGate(R1Gate(angle), target, controls);
}

Result SimulatorMooreThreadGPUSVV2::M(std::uint64_t const target, Basis const basis) {
  return measure(target, basis);
}

double SimulatorMooreThreadGPUSVV2::JointEnsembleProbability(Term const& term) {
  auto const value = expectation(term);
  return static_cast<RealType>(0.5) * (static_cast<RealType>(1.0) - value);
}

void SimulatorMooreThreadGPUSVV2::calculateKernelLaunchSizeType2(uint64_t& blocksPerGrid, uint64_t& threadsPerBlock, const uint64_t maxThreadsPerBlock) const {
  uint64_t threadsPerGrid = (1 << nQubits);
  threadsPerBlock = maxThreadsPerBlock;
  if (threadsPerBlock > threadsPerGrid)
  {
    threadsPerBlock = threadsPerGrid;
  }
  blocksPerGrid = (threadsPerGrid + threadsPerBlock - 1) / threadsPerBlock;
}
	
void SimulatorMooreThreadGPUSVV2::calculateKernelLaunchSizeType3(uint64_t& blocksPerGrid, uint64_t& threadsPerBlock, const uint64_t maxThreadsPerBlock) const {
  uint64_t threadsPerGrid = (1 << (nQubits - 1));
  threadsPerBlock = maxThreadsPerBlock;
  if (threadsPerBlock > threadsPerGrid)
  {
    threadsPerBlock = threadsPerGrid;
  }
  blocksPerGrid = (threadsPerGrid + threadsPerBlock - 1) / threadsPerBlock;
}

void SimulatorMooreThreadGPUSVV2::getStateVector(std::vector<std::complex<double>>& state) const {
  if (!copyDeviceMemoryToHost((void*)_mHostStateVector.data(), _mDeviceStateVector, _mHostStateVector.size() * sizeof(muFloatComplex), "_mDeviceStateVector")) {
    throw std::runtime_error("[SimulatorMooreThreadGPUSVV2] failed to copy device memory to host memory for _mDeviceStateVector");
  }
  
  state.resize(_mHostStateVector.size());
  for (std::size_t i = 0; i < _mHostStateVector.size(); ++i) {
    state[i] = std::complex<double>(
      static_cast<double>(_mHostStateVector[i].x),
      static_cast<double>(_mHostStateVector[i].y)
    );
  }
}

}