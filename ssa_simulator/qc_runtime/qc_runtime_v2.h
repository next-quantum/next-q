#ifndef QC_RUNTIME_V2_H_
#define QC_RUNTIME_V2_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

enum PauliV2 {
  PauliIV2 = 0,
  PauliXV2 = 1,
  PauliYV2 = 2,
  PauliZV2 = 3
};

enum BackendTypeV2 {
  BackendCPU = 0,
  BackendBirenGPU = 1
};

// [BUG 2026.1.31 20:35] all C functions must not be duplicated, therefore add v2
// [UPDATE 2026.2.7 14:20] introduce measure with X/Y/Z basis

void setBackend_v2(enum BackendTypeV2 backend);
void initWithQubitSize_v2(unsigned int qubit_size);
void seed_v2(unsigned int seed);
int  num_qubits_v2();
bool reset_v2(unsigned int qubit);
void resetToZeroState_v2();
// [UPDATE 2026.2.4 9:31] release the qubits allocated by initWithQubitSize_v2
void release_v2();

void X_v2(unsigned int qubit);
void MCX_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit);
void Y_v2(unsigned int qubit);
void MCY_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit);
void Z_v2(unsigned int qubit);
void MCZ_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit);
void H_v2(unsigned int qubit);
void MCH_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit);
void S_v2(unsigned int qubit);
void MCS_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit);
void T_v2(unsigned int qubit);
void MCT_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit);
void AdjS_v2(unsigned int qubit);
void MCAdjS_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit);
void AdjT_v2(unsigned int qubit);
void MCAdjT_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit);
void SWAP_v2(unsigned int qubit1, unsigned int qubit2);
void MCSWAP_v2(unsigned int count, unsigned int ctrls[], unsigned int qubit1, unsigned int qubit2);
void R_v2(enum PauliV2 basis, double angle, unsigned int qubit);
void MCR_v2(enum PauliV2 basis, double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit);
void R1_v2(double angle, unsigned int qubit);
void MCR1_v2(double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit);
void RX_v2(double angle, unsigned int qubit);
void RY_v2(double angle, unsigned int qubit);
void RZ_v2(double angle, unsigned int qubit);
void MCRX_v2(double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit);
void MCRY_v2(double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit);
void MCRZ_v2(double angle, unsigned int count, unsigned int ctrls[], unsigned int qubit);
bool M_v2(unsigned int qubit, enum PauliV2 basis=PauliZV2);

// Pr(One||ψ⟩)
double JointEnsembleProbability_v2(long long n, enum PauliV2 b[], unsigned int q[]);

// 获取量子态向量
void getStateVector_v2(double* real_part, double* imag_part, int* size);

#ifdef __cplusplus
}
#endif

#endif /* QC_RUNTIME_V2_H_ */
