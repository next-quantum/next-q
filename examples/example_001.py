import sys
import os

# Add the root directory to sys.path
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from quantum_framework import quantum_kernel, qvector, h, x, mz, sample, set_target, get_target, list_targets

set_target('default-cpu-sv')
# set_target('biren-gpu-sv') 
# set_target('mthreads-gpu-sv') 

@quantum_kernel
def bell():
    q = qvector(2)
    h(q[0])
    x.ctrl(q[0], q[1])
    mz(q)

# 生成并打印SSA汇编
print("\n=== SSA Assembly ===")
ssa_assembly = bell.generate_ssa_assembly()
print(ssa_assembly)

# 将SSA汇编保存到文件
with open(os.path.join(os.path.dirname(__file__), 'example_001.asm'), 'w') as f:
    f.write(ssa_assembly)
print("\n=== SSA Assembly Saved to example_001.asm ===")

result = sample(bell, shots_count=1000)
print(result)