# MaxCut QAOA Example adapted from: https://nvidia.github.io/cuda-quantum/latest/applications/python/qaoa.html

import sys
import os

# Add the root directory to sys.path
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import numpy as np
from scipy.optimize import minimize
from typing import List

from quantum_framework import quantum_kernel, qubit, qvector, h, x, mx, my, mz, sample, spin, Hamiltonian, observe, rx, rz, set_target
import ssa_simulator_cpp

set_target('default-cpu-sv')
# set_target('biren-gpu-sv')

# We'll use the graph below to illustrate how QAOA can be used to
# solve a max cut problem

#       v1  0--------------0 v2
#           |              | \
#           |              |  \
#           |              |   \
#           |              |    \
#       v0  0--------------0 v3-- 0 v4
# The max cut solutions are 01011, 10100, 01010, 10101 .

# First we define the graph nodes (i.e., vertices) and edges as lists of integers so that they can be broadcast into
# a cudaq.kernel.
nodes: List[int] = [0, 1, 2, 3, 4]
edges: List[List[int]] = [[0, 1], [1, 2], [2, 3], [3, 0], [2, 4], [3, 4]]
edges_src: List[int] = [edges[i][0] for i in range(len(edges))]
edges_tgt: List[int] = [edges[i][1] for i in range(len(edges))]

# Problem parameters
# The number of qubits we'll need is the same as the number of vertices in our graph
qubit_count: int = len(nodes)

# We can set the layer count to be any positive integer.  Larger values will create deeper circuits
layer_count: int = 2 # 1

# Each layer of the QAOA kernel contains 2 parameters
parameter_count: int = 2 * layer_count

@quantum_kernel
def qaoa_problem(qubit_0: qubit, qubit_1: qubit, alpha: float):
    """Build the QAOA gate sequence between two qubits that represent an edge of the graph
    Parameters
    ----------
    qubit_0: cudaq.qubit
        Qubit representing the first vertex of an edge
    qubit_1: cudaq.qubit
        Qubit representing the second vertex of an edge
    thetas: List[float]
        Free variable

    Returns
    -------
    cudaq.Kernel
        Subcircuit of the problem kernel for Max-Cut of the graph with a given edge
    """
    x.ctrl(qubit_0, qubit_1)
    rz(2.0 * alpha, qubit_1)
    x.ctrl(qubit_0, qubit_1)

# We now define the kernel_qaoa function which will be the QAOA circuit for our graph
# Since the QAOA circuit for max cut depends on the structure of the graph,
# we'll feed in global concrete variable values into the kernel_qaoa function for the qubit_count, layer_count, edges_src, edges_tgt.
# The types for these variables are restricted to Quake Values (e.g. qubit, int, List[int], ...)
# The thetas plaeholder will be our free parameters
@quantum_kernel
def kernel_qaoa(qubit_count: int, layer_count: int, edges_src: List[int],
                edges_tgt: List[int], thetas: List[float]):
    """Build the QAOA circuit for max cut of the graph with given edges and nodes
    Parameters
    ----------
    qubit_count: int
        Number of qubits in the circuit, which is the same as the number of nodes in our graph
    layer_count : int
        Number of layers in the QAOA kernel
    edges_src: List[int]
        List of the first (source) node listed in each edge of the graph, when the edges of the graph are listed as pairs of nodes
    edges_tgt: List[int]
        List of the second (target) node listed in each edge of the graph, when the edges of the graph are listed as pairs of nodes
    thetas: List[float]
        Free variables to be optimized

    Returns
    -------
    cudaq.Kernel
        QAOA circuit for Max-Cut for max cut of the graph with given edges and nodes
    """
    # Let's allocate the qubits
    qreg = qvector(qubit_count)
    # And then place the qubits in superposition
    h(qreg)

    # Each layer has two components: the problem kernel and the mixer
    for i in range(layer_count):
        # Add the problem kernel to each layer
        for edge in range(len(edges_src)):
            qubitu = edges_src[edge]
            qubitv = edges_tgt[edge]
            qaoa_problem(qreg[qubitu], qreg[qubitv], thetas[i])
        # Add the mixer kernel to each layer
        for j in range(qubit_count):
            rx(2.0 * thetas[i + layer_count], qreg[j])

@quantum_kernel
def kernel_qaoa_with_measure(qubit_count: int, layer_count: int, edges_src: List[int], 
                             edges_tgt: List[int], thetas: List[float]):
    # Allocate the qubits
    qreg = qvector(qubit_count)
    # Place qubits in superposition
    h(qreg)

    # Each layer has two components: problem kernel and mixer
    for i in range(layer_count):
        # Problem kernel for each layer
        for edge in range(len(edges_src)):
            qubitu = edges_src[edge]
            qubitv = edges_tgt[edge]
            qaoa_problem(qreg[qubitu], qreg[qubitv], thetas[i])
        # Mixer kernel for each layer
        for j in range(qubit_count):
            rx(2.0 * thetas[i + layer_count], qreg[j])
    
    # Add measurements
    for j in range(qubit_count):
        mz(qreg[j])
        
# The problem Hamiltonian
# Define a function to generate the Hamiltonian for a max cut problem using the graph
# with the given edges
def hamiltonian_max_cut(edges_src, edges_tgt):
    """Hamiltonian for finding the max cut for the graph with given edges and nodes

    Parameters
    ----------
    edges_src: List[int]
        List of the first (source) node listed in each edge of the graph, when the edges of the graph are listed as pairs of nodes
    edges_tgt: List[int]
        List of the second (target) node listed in each edge of the graph, when the edges of the graph are listed as pairs of nodes

    Returns
    -------
    Hamiltonian
        Hamiltonian for finding the max cut of the graph with given edges
    """

    hamiltonian = Hamiltonian()

    for edge in range(len(edges_src)):

        qubitu = edges_src[edge]
        qubitv = edges_tgt[edge]
        # Add a term to the Hamiltonian for the edge (u,v)
        hamiltonian += 0.5 * (spin.z(qubitu) * spin.z(qubitv) -
                              spin.i(qubitu) * spin.i(qubitv))

    return hamiltonian

# Generate the Hamiltonian for our graph
hamiltonian: Hamiltonian = hamiltonian_max_cut(edges_src, edges_tgt)
print(hamiltonian)

parameters = np.random.uniform(-np.pi / 8, np.pi / 8, parameter_count)

def objective_qaoa(parameters):
    return observe(kernel_qaoa, hamiltonian, qubit_count, layer_count, edges_src, edges_tgt, parameters).expectation()

# Perform the optimization
result = minimize(objective_qaoa, parameters, method='nelder-mead')

# Print the results
print('Status : %s' % result['message'])
print('Total Evaluations: %d' % result['nfev'])
optimal_parameters = result['x']
optimal_expectation = objective_qaoa(optimal_parameters)

print('optimal_expectation =', optimal_expectation)
print('Therefore, the max cut value is at least ', -1 * optimal_expectation)
print('optimal_parameters =', optimal_parameters)

# 生成并打印SSA汇编
print("\n=== SSA Assembly ===")
ssa_assembly = kernel_qaoa_with_measure.generate_ssa_assembly(qubit_count, layer_count, edges_src, edges_tgt, optimal_parameters)
print(ssa_assembly)

# 将SSA汇编保存到文件
with open(os.path.join(os.path.dirname(__file__), 'example_004.asm'), 'w') as f:
    f.write(ssa_assembly)
print("\n=== SSA Assembly Saved to example_004.asm ===")

# Sample the circuit using the optimized parameters
# Since our kernel has more than one argument, we need to list the values for each of these variables in order in the `sample` call.
counts = sample(kernel_qaoa_with_measure, qubit_count, layer_count, edges_src, edges_tgt, optimal_parameters)
print(counts)

# Identify the most likely outcome from the sample
max_cut = max(counts, key=lambda x: counts[x])
print('The max cut is given by the partition: ',
    max(counts, key=lambda x: counts[x]))

# Calculate the actual cut value based on the partition
def calculate_cut(partition, edges):
    cut = 0
    for u, v in edges:
        if partition[u] != partition[v]:
            cut += 1
    return cut

# Convert the max_cut string to a list of integers (0 or 1)
# Note: The partition string is in the format '01011' for 5 nodes
partition = [int(bit) for bit in max_cut]
actual_cut = calculate_cut(partition, edges)
print('The actual cut value is: ', actual_cut, ' (max cut value should be 5, at least ', -1 * optimal_expectation, ')')