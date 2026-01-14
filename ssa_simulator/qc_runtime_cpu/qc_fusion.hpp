#ifndef QC_FUSION_HPP_
#define QC_FUSION_HPP_

#include <cstdint>
#include <vector>

#include "fusion.hpp"
#include "qc_types.hpp"

// The fusion class is designed to handle matrix fusion

namespace QC {

  class QubitFusion {
  public:
    QubitFusion(): minQubits(1UL), maxQubits(1UL) {} // min 4, max 5 for default
    ~QubitFusion() {}

    // applyControlledGate inserts a controlled gate into fusion
    // targets, ctrls in the low level qubit positions
    // combined is given as empty, if return is none empty, kernel application is necessary
    void applyControlledGate(const FusionGate& input, FusionGate& combined) {
      auto newFused = fused;
      newFused.insert(input.matrix, input.targets, input.controls);

      if (newFused.num_qubits() >= minQubits && newFused.num_qubits() <= maxQubits) {
        fused = newFused;
        run(combined);
      } else if (newFused.num_qubits() > maxQubits || (newFused.num_qubits() - input.targets.size()) > fused.num_qubits()) {
        run(combined);
        fused.insert(input.matrix, input.targets, input.controls);
      } else {
        fused = newFused;
      }
    }

    // run executes the fusion gate transform
    void run(FusionGate& gate) {
      if (fused.size() < 1) {
        // std::cout << "After run (no change)" << std::endl;
        return;
      }

      fused.perform_fusion(gate.matrix, gate.targets, gate.controls);
      fused = Fusion();
    }

  private:
    // fusion defines the gate fusion operator
    Fusion fused;

    // minQubits is the minimum number of qubits to perform the fusion
    std::uint64_t minQubits;
    // maxQubits is the maximum number of qubits to perform the fusion
    std::uint64_t maxQubits;
  };

}


#endif
