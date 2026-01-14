#ifndef QC_MANAGER_HPP_
#define QC_MANAGER_HPP_

#include <cassert>
#include <cstdint>
#include <deque>
#include <map>
#include <vector>

namespace QC {

  struct Qubit {
    std::uint64_t program = 0;
    std::uint64_t id = 0;

    Qubit(std::uint64_t program_, std::uint64_t id_) :program(program_), id(id_) {}

    // Comparator of Qubit
    bool operator<(const Qubit &o) const {
      return (this->program < o.program) || ((this->program == o.program) && (this->id < o.id));
    }

    // Equal test
    bool operator==(const Qubit &o) const {
      return (this->program == o.program) && (this->id == o.id);
    }

    // Not equal test
    bool operator!=(const Qubit &o) const {
      return (this->program != o.program) || (this->id != o.id);
    }

  };

  const Qubit InvalidQubit = Qubit(static_cast<std::uint64_t>(-1), static_cast<std::uint64_t>(-1));

  using QubitMap = std::map<Qubit, std::uint64_t>;

  class QubitManager {
  public:
    QubitManager() {}
    ~QubitManager() {}

    bool setCapacity(std::size_t capacity) {
      // No capcaity change
      if (capacity == qubits.size()) {
        return true;
      }

      // Extend capacity
      if (capacity > qubits.size()) {
        auto oldSize = qubits.size();
        qubits.resize(capacity, InvalidQubit);

        for (std::size_t i=oldSize; i < capacity; i++) {
          availablePositions.push_back(i);
        }

        return true;
      }

      // Check no occupied qubits after certain position
      for (std::size_t i=capacity; i < qubits.size(); i++) {
        if (qubits.at(i) != InvalidQubit) {
          return false;
        }
      }

      // Shrink capacity, no default constructor invoked
      qubits.resize(capacity, InvalidQubit);

      for (auto iter = availablePositions.begin(); iter != availablePositions.end(); ) {
        if ((*iter) >= capacity) {
          iter = availablePositions.erase(iter);
        } else {
          ++iter;
        }
      }

      return true;
    }

    void allocate(const Qubit& qubit)  {
      assert(positions.count(qubit) == 0);
      assert(!availablePositions.empty());

      auto pos = availablePositions.front();
      availablePositions.pop_front();
      positions[qubit] = pos;
      qubits[pos] = qubit;

      // printQubitMap();
    }

    void deallocate(const Qubit& qubit) {
      auto pos = positions.at(qubit);
      assert(pos < qubits.size());

      availablePositions.push_back(pos);
      qubits[pos] = InvalidQubit;
      positions.erase(qubit);
    }

    bool available() const {
      return !availablePositions.empty();
    }

    std::size_t actives() const {
      return qubits.size() - availablePositions.size();
    }

    Qubit firstActive() const {
      return qubits.front();
    }

    bool exist(const Qubit& qubit) const {
      return positions.count(qubit) != 0;
    }

    std::uint64_t get(const Qubit& qubit) const {
      return positions.at(qubit);
    }

    std::vector<std::uint64_t> multiGet(const std::vector<Qubit>& qubits) const {
      auto pos = std::vector<std::uint64_t>(qubits.size());
      for (std::uint64_t i=0; i<qubits.size(); i++) {
        pos[i] = positions.at(qubits[i]);
      }

      return pos;
    }

  private:
    // availablePositions is the queue of available positions
    std::deque<std::uint64_t> availablePositions;
    // positions store the map from qubit to internal vector position
    QubitMap positions;
    // ids store the continuous qubits at the vector qubit positions
    std::vector<Qubit> qubits;

    void printQubitMap() const {
      for (auto& p : positions) {
        std::cout << "Program " << p.first.program << ", ID " << p.first.id << " => Qubit " << p.second << std::endl;
      }
    }
  };

}

#endif
