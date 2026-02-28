# NEXT-Q 量子计算框架

NEXT-Q 是一款面向未来的量子计算框架，致力于构建量子计算与经典计算的桥梁，为量子算法开发提供高效、灵活的编程范式。框架创新性地将 CUDA-Q 风格的量子内核编译为自研的 SSA（静态单赋值）汇编，实现了量子计算的低级抽象与高性能执行的完美结合。

通过 NEXT-Q，开发者可以使用 Python 编写直观的量子内核函数，框架会自动将其转换为优化的 SSA 汇编，并通过高性能 C++ 后端（ssa_simulator）在多种计算平台上高效执行。

## 项目特点

- **前沿技术架构**：创新性地采用 SSA（静态单赋值）汇编作为量子计算的中间表示，实现了量子算法的高效编译和优化
- **多平台支持**：提供 CPU 和多种 GPU 后端，包括壁仞和摩尔线程 GPU，实现量子计算的高性能跨平台执行
- **直观编程范式**：兼容 CUDA-Q 风格的量子内核函数定义，使用 Python 装饰器语法，降低量子算法开发门槛
- **高性能执行**：基于 C++ 实现的后端模拟器，提供高效的量子电路模拟，支持大规模量子算法的测试和验证
- **模块化设计**：核心功能模块化，易于理解和扩展，为量子计算研究和教育提供理想平台
- **开放生态系统**：采用 MIT 许可证，鼓励社区贡献和技术创新，推动量子计算的普及和发展

## SSA 汇编特性

SSA（静态单赋值）汇编是 NEXT-Q 框架的核心创新，为量子计算提供了一种高效、可优化的中间表示形式：

- **静态单赋值**：每个变量仅被赋值一次，为量子算法的编译优化提供了理想基础
- **明确的变量生命周期**：清晰的变量定义和使用范围，便于编译器进行分析和优化
- **无副作用的指令序列**：每条指令执行独立的量子操作，确保计算结果的可预测性
- **硬件友好的低级抽象**：接近硬件的指令集设计，便于在不同量子计算平台上高效实现
- **简洁易解析**：清晰的语法结构，降低了模拟器和编译器的开发难度
- **可扩展性**：模块化的指令设计，便于添加新的量子操作和优化策略

有关 SSA 汇编的详细语法、指令集和最佳实践，请参阅 [SSA 汇编说明手册](SSA_ASSEMBLY_MANUAL.md)。

## 核心文件

### quantum_framework.py

`quantum_framework.py` 是框架的核心文件，实现了量子计算的高级抽象和编译系统：

- **量子内核装饰器**：`@quantum_kernel` 装饰器，提供简洁直观的量子算法定义方式
- **量子比特管理**：`Qubit` 和 `QVector` 类，实现量子比特的高效管理和操作
- **量子门系统**：全面支持各种量子门操作，包括基本门、受控门和旋转门
- **测量系统**：实现多基测量操作，支持量子态的读取和分析
- **SSA 汇编生成器**：将量子电路转换为优化的 SSA 低级汇编，是框架的核心技术之一
- **AST 解析器**：解析 Python 量子内核函数，生成量子电路表示，实现高级语言到低级汇编的转换

### ssa_simulator

`ssa_simulator` 目录包含高性能的 C++ 后端实现，是量子算法执行的核心引擎：

- **SSA 解析器**：高效解析生成的 SSA 汇编代码，确保指令的正确执行
- **量子模拟器**：实现高性能的量子电路模拟，支持大规模量子态的表示和操作
- **多平台适配**：提供 CPU 和多种 GPU 后端，实现量子计算的高性能跨平台执行
- **Python 绑定**：通过 pybind11 提供无缝的 Python 接口，实现高级语言与高性能执行引擎的连接
- **多架构优化**：提供针对不同 CPU 架构的优化实现，充分发挥硬件性能

#### C++ 后端编译

使用 `Makefile` 编译 C++ 后端：

```bash
cd ssa_simulator
make -j # 编译所有目标（仅CPU后端）
make clean  # 清理编译产物
```

如需启用壁仞GPU后端编译，请使用以下命令：

```bash
cd ssa_simulator
make -j enable-biren=1  # 编译壁仞GPU后端
make clean enable-biren=1 # 清理编译产物
```

如需启用摩尔线程GPU后端编译，请使用以下命令：

```bash
cd ssa_simulator
make -j enable-moorethread=1  # 编译摩尔线程GPU后端
make clean enable-moorethread=1 # 清理编译产物
```

编译产物包括 Python 扩展模块和独立的 SSA 模拟执行器。

## 示例

`examples` 目录包含四个精心设计的框架使用示例，覆盖了从基础到高级的量子算法实现：

### example_001.py - Bell 态制备

**基础量子电路示例**，展示量子计算的基本概念：
- 定义量子内核函数，体验 NEXT-Q 的直观编程范式
- 创建量子比特向量，掌握量子资源的管理方法
- 应用 Hadamard 门和受控 X 门，构建基础量子电路
- 执行量子测量，获取量子态信息
- 生成 SSA 汇编，了解框架的编译过程
- 使用 sample 函数进行多次采样，验证量子态的统计特性
- 支持多后端切换，体验不同硬件平台的性能差异

### example_002.py - GHZ 态制备

**可扩展量子电路示例**，展示量子算法的可扩展性：
- 定义带参数的量子内核函数，实现算法的通用性
- 创建任意数量的量子比特向量，处理大规模量子系统
- 使用循环结构制备 GHZ 态，展示量子电路的程序化构建
- 执行量子测量，验证多量子比特纠缠态的特性
- 生成 SSA 汇编，了解框架对复杂电路的编译能力
- 支持多后端切换，体验大规模量子态在不同硬件上的模拟性能

### example_003.py - 量子相位估计（QPE）

**高级量子算法示例**，展示量子计算的强大能力：
- 定义多个量子内核函数，实现算法的模块化设计
- 实现逆量子傅里叶变换（iqft），掌握量子算法的核心组件
- 使用控制门实现量子相位估计，展示量子算法的典型应用
- 演示量子算法的组合使用，构建复杂量子计算流程
- 生成并保存 SSA 汇编到文件，便于分析和优化
- 支持多后端切换，体验高级量子算法在不同硬件上的执行效率

### example_004.py - MaxCut QAOA

**量子优化算法示例**，展示量子计算在实际问题中的应用：
- 定义QAOA问题和混合量子内核函数，实现量子-经典混合算法
- 构建QAOA电路用于图的Max-Cut问题，展示量子算法的实际应用
- 使用自旋哈密顿量和observe函数进行期望测量，掌握量子态的高级分析方法
- 结合经典优化器（scipy.optimize）优化QAOA参数，实现量子-经典混合优化
- 生成并保存SSA汇编到文件，便于算法分析和调试
- 支持多后端切换，体验量子优化算法在不同硬件上的性能表现

## 后端切换

NEXT-Q框架支持多种计算后端，为量子算法的执行提供灵活的硬件选择：

### 1. default-cpu-sv（CPU向量态模拟器）
- **通用计算后端**：默认后端，适用于各种环境
- **基于CPU实现**：采用优化的向量态表示，确保在通用硬件上的可靠执行
- **调试友好**：适合小规模量子电路的开发、测试和调试
- **跨平台兼容**：在任何支持C++的平台上均可运行

### 2. biren-gpu-sv（壁仞GPU向量态模拟器）
- **高性能计算后端**：利用壁仞GPU的并行计算能力
- **硬件加速**：针对壁仞GPU架构优化，提供显著的性能提升
- **大规模模拟**：支持更大规模的量子电路模拟，加速算法开发和验证
- **国产硬件支持**：为国产GPU提供量子计算应用场景

### 3. moore-threads-gpu-sv（摩尔线程GPU向量态模拟器）
- **高性能计算后端**：利用摩尔线程GPU的并行计算能力
- **硬件加速**：针对摩尔线程GPU架构优化，提供显著的性能提升
- **大规模模拟**：支持更大规模的量子电路模拟，加速算法开发和验证
- **多平台适配**：为不同GPU架构提供统一的量子计算接口

### 切换方法

在Python代码中使用`set_target()`函数切换后端：

```python
from quantum_framework import set_target

# 使用CPU后端（默认）
set_target('default-cpu-sv')

# 或者，使用壁仞GPU后端
set_target('biren-gpu-sv')

# 或者，使用摩尔线程GPU后端
set_target('moore-threads-gpu-sv')
```

注意：使用GPU后端前，需要先编译对应的GPU后端。

## 快速开始

### 安装

NEXT-Q 框架采用简洁的安装方式，让您快速开始量子计算之旅：

```bash
git clone https://github.com/next-quantum/next-q.git
cd next-q
```

### 编译 C++ 后端

为了获得最佳性能，我们推荐编译高性能 C++ 后端：

```bash
# 编译 C++ 后端（仅CPU后端）
cd ssa_simulator
make
cd ..
```

如需启用壁仞GPU后端编译，请使用：

```bash
# 编译壁仞GPU后端
cd ssa_simulator
make -j enable-biren=1
cd ..
```

如需启用摩尔线程GPU后端编译，请使用：

```bash
# 编译摩尔线程GPU后端
cd ssa_simulator
make -j enable-moorethread=1
cd ..
```

### 基本用法

NEXT-Q 框架提供了简洁直观的编程接口，让您轻松上手量子计算：

1. **编译C++模拟后端**：首先编译高性能C++后端：

```bash
cd ssa_simulator
make  # 仅CPU后端
# 或
make -j enable-biren=1  # 启用壁仞GPU后端
# 或
make -j enable-moorethread=1  # 启用摩尔线程GPU后端
cd ..
```

2. **创建Python文件**：在项目根目录创建一个名为`my_quantum_script.py`的文件，内容如下：

```python
from quantum_framework import quantum_kernel, qvector, h, x, mz, sample, set_target

# 选择计算后端（根据硬件环境选择）
set_target('default-cpu-sv')  # 使用CPU后端（默认）
# set_target('biren-gpu-sv')     # 或使用壁仞GPU后端
# set_target('moore-threads-gpu-sv')  # 或使用摩尔线程GPU后端

@quantum_kernel
def my_quantum_program():
    """量子程序：制备Bell态"""
    # 创建量子比特
    qubits = qvector(2)
    
    # 应用量子门，构建量子电路
    h(qubits[0])        # 对第一个量子比特应用Hadamard门
    x.ctrl(qubits[0], qubits[1])  # 以第一个量子比特为控制位，对第二个量子比特应用X门
    
    # 测量量子比特，获取结果
    mz(qubits)

# 生成 SSA 汇编，了解量子电路的底层表示
ssa_assembly = my_quantum_program.generate_ssa_assembly()
print("SSA汇编代码：")
print(ssa_assembly)

# 执行量子程序，进行多次采样
result = sample(my_quantum_program, shots_count=1000)
print("\n测量结果：")
print(result)
```

3. **执行Python文件**：在项目根目录执行以下命令运行量子程序：

```bash
python my_quantum_script.py
```

执行后，你将看到生成的SSA汇编代码和量子测量结果。

## 支持的量子操作

- **基本量子门**：h, x, y, z, t, s, r1, rx, ry, rz, swap
- **受控门**：x.ctrl, h.ctrl, z.ctrl, r1.ctrl, rx.ctrl, ry.ctrl, rz.ctrl, swap.ctrl
- **测量操作**：mz, mx, my
- **共轭转置**：支持.adj操作，如h.adj, x.adj等
- **量子比特创建**：qubit, qvector
- **条件执行**：支持基于测量结果的条件操作
- **循环结构**：支持 for 循环遍历量子比特
- **量子门组合**：支持量子内核的嵌套调用和组合使用
- **控制函数**：control() 函数用于创建受控量子操作

## 许可证

本项目采用 MIT 许可证，详情请查看 [LICENSE](LICENSE) 文件。

## 贡献

欢迎提交问题报告和拉取请求，帮助改进这个项目！

## 联系方式

如有任何问题或建议，请通过 GitHub Issues 与我们联系。