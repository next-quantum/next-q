#ifndef QC_RUNTIME_H_
#define QC_RUNTIME_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

  enum Pauli {
    PauliI = 0,
    PauliX = 1,
    PauliY = 2,
    PauliZ = 3
  };

  void init();
  void seed(unsigned int seed);

  long long random_choice(long long size, double p[]);

  // Pr(One||ψ⟩)
  double JointEnsembleProbability(long long n, enum Pauli b[], unsigned int q[]);

  void   allocateQubit(unsigned int qubit);
  bool   release(unsigned int qubit);
  int    num_qubits();

  bool reset(unsigned int qubit);

  void X(unsigned int qubit);
  void MCX(unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void Y(unsigned int qubit);
  void MCY(unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void Z(unsigned int qubit);
  void MCZ(unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void H(unsigned int qubit);
  void MCH(unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void S(unsigned int qubit);
  void MCS(unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void T(unsigned int qubit);
  void MCT(unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void AdjS(unsigned int qubit);
  void MCAdjS(unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void AdjT(unsigned int qubit);
  void MCAdjT(unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void SWAP(unsigned int qubit1, unsigned int qubit2);
  // Need to check implementation
  void MCSWAP(unsigned int count, unsigned int ctrls[], unsigned int qubit1, unsigned int qubit2);
  void R(enum Pauli basis, double angle, unsigned int qubit);
  void MCR(enum Pauli basis, double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void R1(double angle, unsigned int qubit);
  void MCR1(double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  // Rotation gates
  void RX(unsigned int qubit, double angle);
  void RY(unsigned int qubit, double angle);
  void RZ(unsigned int qubit, double angle);
  void MC_RX(unsigned int count, unsigned int ctrls[], unsigned int qubit, double angle);
  void MC_RY(unsigned int count, unsigned int ctrls[], unsigned int qubit, double angle);
  void MC_RZ(unsigned int count, unsigned int ctrls[], unsigned int qubit, double angle);
  void Exp(unsigned int n, enum Pauli paulis[], double angle, unsigned int ids[]);
  void MCExp(unsigned int n, enum Pauli paulis[], double angle, unsigned int nc, unsigned int ctrls[], unsigned int ids[]);
  void ExpFrac(unsigned int n, enum Pauli paulis[], int nom, int den, unsigned int ids[]);
  void MCExpFrac(unsigned int n, enum Pauli paulis[], int nom, int den, unsigned int nc, unsigned int ctrls[], unsigned int ids[]);
  bool M(unsigned int qubit);
  bool Measure(unsigned int n, enum Pauli b[], unsigned int q[]);


  // void RFrac(enum Pauli basis, int nom, int den, unsigned int id);
  // void MCRFrac(enum Pauli basis, int nom, int den, unsigned int nc, unsigned int ctrls[], unsigned int id);

  // Interface for a updated version with program id involved
  void sim_init();
  void sim_seed(unsigned int seed);
  long long sim_random_choice(long long size, double p[]);
  double sim_JointEnsembleProbability(unsigned int program, long long n, enum Pauli b[], unsigned int q[]);
  void   sim_allocateQubit(unsigned int program, unsigned int qubit);
  bool   sim_release(unsigned int program, unsigned int qubit);
  int    sim_num_qubits();

  // [UPDATE 2026.1.14 13:35] introduce reset qubit states
  bool sim_reset(unsigned int program, unsigned int qubit);
  
  void sim_X(unsigned int program, unsigned int qubit);
  void sim_MCX(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void sim_Y(unsigned int program, unsigned int qubit);
  void sim_MCY(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void sim_Z(unsigned int program, unsigned int qubit);
  void sim_MCZ(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void sim_H(unsigned int program, unsigned int qubit);
  void sim_MCH(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void sim_S(unsigned int program, unsigned int qubit);
  void sim_MCS(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void sim_T(unsigned int program, unsigned int qubit);
  void sim_MCT(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void sim_AdjS(unsigned int program, unsigned int qubit);
  void sim_MCAdjS(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void sim_AdjT(unsigned int program, unsigned int qubit);
  void sim_MCAdjT(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void sim_SWAP(unsigned int program, unsigned int qubit1, unsigned int qubit2);
  void sim_MCSWAP(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit1, unsigned int qubit2);
  void sim_R(unsigned int program, enum Pauli basis, double angle, unsigned int qubit);
  void sim_MCR(unsigned int program, enum Pauli basis, double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  void sim_R1(unsigned int program, double angle, unsigned int qubit);
  void sim_MCR1(unsigned int program, double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit);
  // Rotation gates - simulation version
  void sim_RX(unsigned int program, unsigned int qubit, double angle);
  void sim_RY(unsigned int program, unsigned int qubit, double angle);
  void sim_RZ(unsigned int program, unsigned int qubit, double angle);
  void sim_MC_RX(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit, double angle);
  void sim_MC_RY(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit, double angle);
  void sim_MC_RZ(unsigned int program, unsigned int count, unsigned int ctrls[], unsigned int qubit, double angle);
  void sim_Exp(unsigned int program, unsigned int n, enum Pauli paulis[], double angle, unsigned int ids[]);
  void sim_MCExp(unsigned int program, unsigned int n, enum Pauli paulis[], double angle, unsigned int nc, unsigned int ctrls[], unsigned int ids[]);
  void sim_ExpFrac(unsigned int program, unsigned int n, enum Pauli paulis[], int nom, int den, unsigned int ids[]);
  void sim_MCExpFrac(unsigned int program, unsigned int n, enum Pauli paulis[], int nom, int den, unsigned int nc, unsigned int ctrls[], unsigned int ids[]);
  bool sim_M(unsigned int program, unsigned int qubit);
  bool sim_Measure(unsigned int program, unsigned int n, enum Pauli b[], unsigned int q[]);

  bool sim_Dump(unsigned int program, unsigned int n, unsigned int ids[], const char* filepath);

  // Additioanal interfaces
  void sim_IBM_X90(unsigned int program, unsigned int qubit, double phase);
  void sim_IBM_CZ(unsigned int program, unsigned int qubit1, unsigned int qubit2);

  // reset to zero state, all program
  void resetToZeroState();

#ifdef __cplusplus
}
#endif


#endif /* QCRUNTIME_H_ */
