import sys
import os

# Add the root directory to sys.path
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from quantum_framework import quantum_kernel, qubit, qvector, h, x, z, mz, sample, set_target, SSASimulator

@quantum_kernel
def prog():
    qubit_count = 2
    qubits = qvector(qubit_count)
    h(qubits[0])
    for i in range(1, qubit_count):
        x.ctrl(qubits[0], qubits[i])
    mz(qubits)

# 生成并打印SSA汇编
print("\n=== SSA Assembly ===")
ssa_assembly = prog.generate_ssa_assembly()
print(ssa_assembly)
