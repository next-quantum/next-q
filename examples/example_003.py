import sys
import os

# Add the root directory to sys.path
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from typing import Callable
import numpy as np
from quantum_framework import quantum_kernel, qubit, qvector, h, x, t, mz, swap, sample, control

@quantum_kernel
def iqft(qubits: qvector):
    n = len(qubits)
    for i in range(n//2):
        swap(qubits[i], qubits[n-1-i])

    for i in range(n-1):
        h(qubits[i])
        j = i + 1
        for y in range(i, -1, -1):
            r1.ctrl(-np.pi / 2**(j - y), qubits[j], qubits[y])
    
    h(qubits[n - 1])

# Define the U kernel
@quantum_kernel
def t_gate(qubit: qubit):
    t(qubit)

# Define the state preparation |psi> kernel
@quantum_kernel
def x_gate(qubit: qubit):
    x(qubit)

@quantum_kernel
def qpe(nc: int, nq: int, state_prep: Callable[[qubit], None], oracle: Callable[[qubit], None]):
    q = qvector(nc + nq)
    counting_qubits = q[:nc]
    state_register = q[-1]
    state_prep(state_register)
    h(counting_qubits)
    for i in range(nc):
        for j in range(2**i):
            control(oracle, [counting_qubits[i]], state_register)
    iqft(counting_qubits)
    mz(counting_qubits)
    
nc = 3
nq = 1

# 生成并打印SSA汇编
print("\n=== SSA Assembly ===")
ssa_assembly = qpe.generate_ssa_assembly(nc, nq, x_gate, t_gate)
print(ssa_assembly)

# 将SSA汇编保存到文件
with open(os.path.join(os.path.dirname(__file__), 'example_003.asm'), 'w') as f:
    f.write(ssa_assembly)
print("\n=== SSA Assembly Saved to example_003.asm ===")

result = sample(qpe, nc, nq, x_gate, t_gate, shots_count=1000)
print(result)