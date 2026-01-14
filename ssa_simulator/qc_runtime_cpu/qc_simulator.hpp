#ifndef QC_SIMULATOR_HPP_
#define QC_SIMULATOR_HPP_

#include "qc_fusion.hpp"
#include "qc_manager.hpp"
#include "qc_storage.hpp"
#include "qc_types.hpp"

#include <cmath>
#include <complex>
#include <cstdint>
#include <iostream>
#include <random>

// #define DEBUG

// The QC runtime class is designed to implement low level quantum gate simulator
// that can be used either embeded or inside a dynamic library

namespace QC {
  using Term = std::vector<std::pair<Qubit, Basis>>;

  // Define the commonly used gate

  // Common unitary matrices
  const ComplexType ComplexI = ComplexType(0, 1);
  const MatrixType XGate = MatrixType{{0, 1}, {1, 0}};
  const MatrixType YGate = MatrixType{{0., -ComplexI}, {ComplexI, 0.}};
  const MatrixType ZGate = MatrixType{{1., 0.}, {0., -1.}};
  const MatrixType HGate = MatrixType{{M_SQRT1_2, M_SQRT1_2}, {M_SQRT1_2, -M_SQRT1_2}};
  const MatrixType SGate = MatrixType{{1, 0}, {0, ComplexI}};
  const MatrixType TGate = MatrixType{{1, 0}, {0, std::exp(ComplexI*M_PI_4)}};
  const MatrixType AdjSGate = MatrixType{{1, 0}, {0, -ComplexI}};
  const MatrixType AdjTGate = MatrixType{{1, 0}, {0, std::exp(-ComplexI*M_PI_4)}};

  // Global phase shift gate
  const MatrixType PhGate(double angle) {
    // exp(-i*theta/2)
    auto d = std::exp(ComplexType(0, -angle*0.5));

    return MatrixType{{d, 0}, {0, d}};
  }

  static const MatrixType RxGate(double angle) {
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

  static const MatrixType RyGate(double angle) {
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

  static const MatrixType RzGate(double angle) {
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
  static const MatrixType R1Gate(double angle) {
    // exp(i*theta)
    auto d = std::exp(ComplexType(0, angle));

    return MatrixType{{1, 0}, {0, d}};
  }

  class Simulator {
    static const std::uint64_t InitNumQubits = 3;

  public:
    Simulator(): random(std::bind(std::uniform_real_distribution<RealType>(0., 1.), std::ref(engine))) {
      // Call init
      init();
    }
    ~Simulator() {}

    void init() {
      storage.init(InitNumQubits);
      assert(manager.setCapacity(InitNumQubits));
      #ifdef DEBUG
        storage.dump(std::cout);
      #endif
    }

    void seed(std::uint64_t seed) {
      engine.seed(seed);
    }

    std::uint64_t randomChoice(std::uint64_t size, RealType p[]) {
      // Create discrete distribution
      std::discrete_distribution<std::uint64_t> distribution(p, p+size);
      return distribution(engine);
    }

    void allocate(const Qubit& qubit) {
      // capacity and availables will be update
      if (!manager.exist(qubit)) {
        // extend state vector if necessary
        if (!manager.available()) {
          auto nQubits = storage.extend();
          assert(manager.setCapacity(nQubits));
        }
        manager.allocate(qubit);
      } else {
        throw(std::runtime_error("Allocate: qubit already exists. Qubits should be unique."));
      }
      #ifdef DEBUG
        std::cout << "[After Allocate]" << std::endl;
        storage.dump(std::cout);
      #endif
    }

    bool deallocate(const Qubit& qubit) {
      // capacity and availables will be update
      if (!manager.exist(qubit)) {
        throw(std::runtime_error("Deallocate: qubit not exists."));
      }

      run();
      auto pos = manager.get(qubit);
      auto c = storage.classicalValue(pos);
      if (c == ClassicalValue::ClassicalUnknown) {
        storage.dump(std::cout);
      }
      assert(c != ClassicalValue::ClassicalUnknown);

      if (c == ClassicalValue::ClassicalMixed) {
        // In this case will not collapse
        return false;
      }

      manager.deallocate(qubit);
      return true;
    }

    bool reset(const Qubit& qubit) {
      // capacity and availables will be update
      if (!manager.exist(qubit)) {
        throw(std::runtime_error("Reset: qubit not exists."));
      }

      run();
      auto const pos = manager.get(qubit);
      
      // Use the new resetQubit method to reset the qubit to |0> state
      storage.resetQubit(pos);
      
      // Use a larger tolerance to handle numerical precision issues
      auto const c = storage.classicalValue(pos);
      
      // Debug output
      #ifdef DEBUG
        std::cout << "Reset debug:" << std::endl;
        std::cout << "  Qubit: " << qubit.id << std::endl;
        std::cout << "  ClassicalValue: " << static_cast<int>(c) << std::endl;
        storage.dump(std::cout);
      #endif
      
      // Adjust assertion to handle numerical precision
      assert(c == ClassicalValue::ClassicalZero);
      
      return true;
    }

    // Get expectation value for given Pauli term
    RealType expectation(const Term& term) {
      run();
      storage.store();
      applyTerm(term);
      run();
      auto value = storage.dot();
      storage.swap();
      #ifdef DEBUG
        storage.dump(std::cout);
      #endif

      return value;
    }

    std::size_t actives() const {
      return manager.actives();
    }

    Qubit firstActive() const {
      return manager.firstActive();
    }

    void X(const Qubit& target) {
      auto pos = manager.get(target);
      multiControlledGate(XGate, pos, std::vector<std::uint64_t>{});
    }

    void MCX(const Qubit& target, const std::vector<Qubit>& controls) {
      auto targetPos = manager.get(target);
      auto ctrlsPos = manager.multiGet(controls);
      multiControlledGate(XGate, targetPos, ctrlsPos);
    }

    void Y(const Qubit& target) {
      auto pos = manager.get(target);
      multiControlledGate(YGate, pos, std::vector<std::uint64_t>{});
    }

    void MCY(const Qubit& target, const std::vector<Qubit>& controls) {
      auto targetPos = manager.get(target);
      auto ctrlsPos = manager.multiGet(controls);
      multiControlledGate(YGate, targetPos, ctrlsPos);
    }

    void Z(const Qubit& target) {
      auto pos = manager.get(target);
      multiControlledGate(ZGate, pos, std::vector<std::uint64_t>{});
    }

    void MCZ(const Qubit& target, const std::vector<Qubit>& controls) {
      auto targetPos = manager.get(target);
      auto ctrlsPos = manager.multiGet(controls);
      multiControlledGate(ZGate, targetPos, ctrlsPos);
    }

    void H(const Qubit& target) {
      auto pos = manager.get(target);
      multiControlledGate(HGate, pos, std::vector<std::uint64_t>{});
    }

    void MCH(const Qubit& target, const std::vector<Qubit>& controls) {
      auto targetPos = manager.get(target);
      auto ctrlsPos = manager.multiGet(controls);
      multiControlledGate(HGate, targetPos, ctrlsPos);
    }

    void S(const Qubit& target) {
      auto pos = manager.get(target);
      multiControlledGate(SGate, pos, std::vector<std::uint64_t>{});
    }

    void MCS(const Qubit& target, const std::vector<Qubit>& controls) {
      auto targetPos = manager.get(target);
      auto ctrlsPos = manager.multiGet(controls);
      multiControlledGate(SGate, targetPos, ctrlsPos);
    }

    void T(const Qubit& target) {
      auto pos = manager.get(target);
      multiControlledGate(TGate, pos, std::vector<std::uint64_t>{});
    }

    void MCT(const Qubit& target, const std::vector<Qubit>& controls) {
      auto targetPos = manager.get(target);
      auto ctrlsPos = manager.multiGet(controls);
      multiControlledGate(TGate, targetPos, ctrlsPos);
    }

    void AdjS(const Qubit& target) {
      auto pos = manager.get(target);
      multiControlledGate(AdjSGate, pos, std::vector<std::uint64_t>{});
    }

    void MCAdjS(const Qubit& target, const std::vector<Qubit>& controls) {
      auto targetPos = manager.get(target);
      auto ctrlsPos = manager.multiGet(controls);
      multiControlledGate(AdjSGate, targetPos, ctrlsPos);
    }

    void AdjT(const Qubit& target) {
      auto pos = manager.get(target);
      multiControlledGate(AdjTGate, pos, std::vector<std::uint64_t>{});
    }

    void MCAdjT(const Qubit& target, const std::vector<Qubit>& controls) {
      auto targetPos = manager.get(target);
      auto ctrlsPos = manager.multiGet(controls);
      multiControlledGate(AdjTGate, targetPos, ctrlsPos);
    }

    void R(const Qubit& target, Basis basis, RealType angle) {
      auto pos = manager.get(target);

      switch (basis) {
      case Basis::PauliI:
        {
          multiControlledGate(PhGate(angle), pos, std::vector<std::uint64_t>{});
          break;
        }
      case Basis::PauliX:
        {
          multiControlledGate(RxGate(angle), pos, std::vector<std::uint64_t>{});
          break;
        }
      case Basis::PauliY:
        {
          multiControlledGate(RyGate(angle), pos, std::vector<std::uint64_t>{});
          break;
        }
      case Basis::PauliZ:
        {
          multiControlledGate(RzGate(angle), pos, std::vector<std::uint64_t>{});
          break;
        }
      }
    }

    void MCR(const Qubit& target, const std::vector<Qubit>& controls, Basis basis, RealType angle) {
      auto targetPos = manager.get(target);
      auto ctrlsPos = manager.multiGet(controls);

      switch (basis) {
      case Basis::PauliI:
        {
          multiControlledGate(PhGate(angle), targetPos, ctrlsPos);
          break;
        }
      case Basis::PauliX:
        {
          multiControlledGate(RxGate(angle), targetPos, ctrlsPos);
          break;
        }
      case Basis::PauliY:
        {
          multiControlledGate(RyGate(angle), targetPos, ctrlsPos);
          break;
        }
      case Basis::PauliZ:
        {
          multiControlledGate(RzGate(angle), targetPos, ctrlsPos);
          break;
        }
      }
    }

    void R1(const Qubit& target, RealType angle) {
      auto pos = manager.get(target);
      multiControlledGate(R1Gate(angle), pos, std::vector<std::uint64_t>{});

      // Use alternative implementation
      // this->RID(id, Basis::PauliZ, angle);
      // this->RID(id, Basis::PauliI, -angle);
    }

    void MCR1(const Qubit& target, const std::vector<Qubit>& controls, RealType angle) {
      auto targetPos = manager.get(target);
      auto ctrlsPos = manager.multiGet(controls);
      multiControlledGate(R1Gate(angle), targetPos, ctrlsPos);

      //this->MCRID(targetID, ctrlsID, Basis::PauliZ, angle);
      //this->MCRID(targetID, ctrlsID, Basis::PauliI, -angle);
    }

    Result M(const Qubit& target) {
      auto pos = manager.get(target);
      return measure(pos);
    }

    bool dumpToFile(const std::vector<Qubit>& qubits, std::string filepath) {
      run();

      std::cout << "Dump to filepath " << filepath << std::endl;

      auto pos = manager.multiGet(qubits);
      storage.dump(pos, std::cout);

      return true;
    }

    void resetToZeroState() {
      storage.resetToZeroState();
    }

  private:
    // manager is the manager for qubit
    QubitManager manager;
    // fusion is the gate fusion engine
    QubitFusion fusion;
    // storage is the qubit storage
    QubitStorage storage;
    // engine is the random number generation engine
    RndEngine engine;
    // random is the random generator for floating number between 0 and 1
    std::function<RealType()> random;

    void run() {
      #ifdef DEBUG
        std::cout << "[Before Run]" << std::endl;
        storage.dump(std::cout);
      #endif

      FusionGate gate;
      fusion.run(gate);
      if (!gate.empty()) {
        storage.apply(gate);
      }

      #ifdef DEBUG
        std::cout << "[After Run]" << std::endl;
        storage.dump(std::cout);
      #endif
    }

    void multiControlledGate(const MatrixType& matrix, std::uint64_t target, const std::vector<std::uint64_t>& controls) {
      for (auto control : controls) {
        assert(control != target);
      }

      #ifdef DEBUG
        std::cout << "[Before multi controlled gate]" << std::endl;
        storage.dump(std::cout);
      #endif

      FusionGate gate;
      fusion.applyControlledGate(
        FusionGate(matrix, std::vector<std::uint64_t>{target}, controls),
        gate
      );
      // [bug] may be here, no storage apply at this stage
      if (!gate.empty()) {
        // std::cout << "[gate not empty]" << std::endl;
        storage.apply(gate);
      } else {
        // std::cout << "[gate still empty]" << std::endl;
      }

      #ifdef DEBUG
        std::cout << "[After multi controlled gate]" << std::endl;
        storage.dump(std::cout);
      #endif
    }

    Result measureWithStats(std::uint64_t target, RealType& stateProb) {
      Result outcome = Result::Zero;

      // Determine outcome
      run();

      auto stateProbOne = storage.findProbabilityOfOutcome(target, Result::One);
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
      storage.collapseToOutcome(target, 1.0 / std::sqrt(stateProb), outcome);

      return outcome;
    }

    Result measure(std::uint64_t target) {
      RealType stateProb = 0.0;
      return measureWithStats(target, stateProb);
    }

    void applyTerm(const Term& term) {
      for (auto const& op : term) {
        auto pos = manager.get(op.first);

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
  };

}

#endif
