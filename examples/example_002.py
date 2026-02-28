import sys
import os

# Add the root directory to sys.path
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from quantum_framework import quantum_kernel, qvector, h, x, mz, sample, set_target

set_target('default-cpu-sv')
# set_target('biren-gpu-sv') 
# set_target('mthreads-gpu-sv') 

@quantum_kernel
def ghz(num_qubits: int):
    qubits = qvector(num_qubits)
    h(qubits[0])
    for i, qubit in enumerate(qubits[:num_qubits - 1]):
        x.ctrl(qubit, qubits[i + 1])
    mz(qubits)

num_qubits = 10

# 生成并打印SSA汇编
print("\n=== SSA Assembly ===")
ssa_assembly = ghz.generate_ssa_assembly(num_qubits)
print(ssa_assembly)

# 将SSA汇编保存到文件
with open(os.path.join(os.path.dirname(__file__), 'example_002.asm'), 'w') as f:
    f.write(ssa_assembly)
print("\n=== SSA Assembly Saved to example_002.asm ===")

result = sample(ghz, num_qubits, shots_count=1000)
print(result)