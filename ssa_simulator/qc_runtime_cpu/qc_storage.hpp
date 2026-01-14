#ifndef QC_STORAGE_HPP_
#define QC_STORAGE_HPP_

#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>

#include "qc_types.hpp"

#if defined(NOINTRIN) || !defined(INTRIN)
#include "nointrin/kernels.hpp"
#else
#include "intrin/kernels.hpp"
#endif

// QC storage is responsible for handling the qubit state vector storage
namespace QC {

  class QubitStorage {
  public:
    QubitStorage(): nQubits(0UL) {}
    ~QubitStorage() {}

    // init will initialize the state vector with # of local qubits
    void init(std::uint64_t nQubits_) {
      vec = StateVector(1UL << nQubits_, 0.0); // Copy elision
      buffer = StateVector(1UL << nQubits_, 0.0); // Copy elision
      nQubits = nQubits_;

      // Normalization is forced
      vec[0] = 1.0;
      buffer[0] = 1.0;

      assert(!vec.empty());
    }

    // extend will extend the state vector by one more qubit
    // extend will not change the storage positions for already allocated qubits
    std::uint64_t extend() {
      assert(!vec.empty());
      auto newSize = vec.size() << 1UL;
      vec.resize(newSize, 0.0);
      buffer.resize(newSize, 0.0);
      assert(!vec.empty());
      nQubits++;
      return nQubits;
    }

    // apply will perform a fusion gate on the state vector
    void apply(const FusionGate& gate) {
      auto ctrlmask = getControlMask(gate.controls);

      // std::cout << "[Apply kernel " << gate.targets.size() << "]" << std::endl;

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

      // dump
      // dump(std::cout);
    }

    // fetch classical value
    // c++ complex norm calcuate |re|*|re| + |im|*|im|, then tolerance should larger than 1E-24 for each component less than 1E-12
    ClassicalValue classicalValue(std::uint64_t pos, double tol = 1E-12) const {
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

    RealType findProbabilityOfOutcome(std::uint64_t target, Result outcome) const {
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
    void collapseToOutcome(std::uint64_t target, RealType normFactor, Result outcome) {
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

    // store will save the current state in buffer
    void store() {
      assert(!vec.empty());
      std::copy(vec.begin(), vec.end(), buffer.begin());
    }

    // swap will exchange the storage of the vec and buffer
    void swap() {
      assert(!vec.empty());
      std::swap(vec, buffer);
    }

    // dot will get the dot product with buffer
    RealType dot() const {
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

    // dump will print the state vector to an output stream
    // ID is the the number of chunks before starting of this chunk
    void dump(std::ostream& os) const {
      os << "State vector (size " << vec.size() << "): \n";
      os << "Norm: " << norm() << "\n";
      assert(!vec.empty());
      for (std::size_t i=0; i != vec.size(); ++i) {
        // os << "  " << std::fixed << std::setprecision(8) << vec[i] << "|" << (vec.size() + i) << ">" << "\n";
        os << "  " << std::fixed << std::setprecision(8) << vec[i] << "|" << (i) << ">" << "\n";
      }
      os << std::endl;
    }

    void dump(std::vector<std::uint64_t> pos, std::ostream& os) const {
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

    RealType norm() const {
      double sum = 0;
      for (auto& data : vec) {
        sum += std::norm(data);
      }

      return sum;
    }

    // [UPDATE 2026.1.10 10:01] clear the quantum state to |0>
    void resetToZeroState() {
      std::fill(vec.begin(), vec.end(), 0.0);
      vec[0] = 1.0;
    }
    
    // Reset a specific qubit to |0> state
    void resetQubit(std::uint64_t target) {
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

  private:
    // nQubits is the number of qubits
    std::uint64_t nQubits;
    // vec is the storage of state vector
    StateVector vec;
    // buffer is the copied buffer space for state vector
    StateVector buffer;

    static std::uint64_t getControlMask(std::vector<std::uint64_t> const& ctrls){
      std::uint64_t ctrlmask = 0;
      for (auto c : ctrls)
        ctrlmask |= (1UL << c);
      return ctrlmask;
    }

  };

}

#endif
