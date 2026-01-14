# NEXT-Q 量子计算框架

NEXT-Q 是一个轻量级量子计算框架，专注于将 CUDA-Q 风格的量子内核编译为自研的 SSA（静态单赋值）汇编。该框架允许开发者使用 Python 编写 CUDA-Q 风格的量子内核函数，并将其编译为低级 SSA 汇编，同时提供高性能的 C++ 后端（ssa_simulator）来执行生成的 SSA 汇编，支持在各种量子计算平台上高效运行。

## 项目特点

- **兼容 CUDA-Q kernel**：支持 CUDA-Q 风格的量子内核函数定义
- **SSA 汇编生成**：将量子内核编译为低级 SSA 汇编，具有明确的变量生命周期和无副作用的指令序列
- **C++ 后端支持**：ssa_simulator 目录提供了高性能的 C++ 后端，实现了 SSA 汇编的解析和执行
- **轻量级设计**：核心功能集中在单个文件中，易于理解和扩展
- **Python 友好**：使用 Python 装饰器语法定义量子内核
- **MIT 许可证**：允许自由使用、修改和分发

## SSA 汇编特性

SSA（静态单赋值）汇编是 NEXT-Q 框架的核心输出，具有以下特性：

- **静态单赋值**：每个变量仅被赋值一次，便于优化和分析
- **明确的变量生命周期**：清晰的变量定义和使用范围
- **无副作用的指令序列**：每条指令执行独立的量子操作
- **低级抽象**：接近硬件的指令集，便于在不同量子计算平台上实现
- **易于解析**：简洁的语法结构，便于编写模拟器和编译器

## 核心文件

### quantum_framework.py

`quantum_framework.py` 是框架的核心文件，包含以下主要组件：

- **量子内核装饰器**：`@quantum_kernel` 装饰器用于定义量子内核函数
- **量子比特管理**：`Qubit` 和 `QVector` 类用于管理量子比特
- **量子门实现**：支持 Hadamard (h)、Pauli-X (x)、Pauli-Z (z) 等基本量子门
- **测量操作**：`mz` 函数用于执行量子比特测量
- **SSA 汇编生成器**：将量子电路转换为 SSA 低级汇编
- **AST 解析器**：解析 Python 量子内核函数，生成量子电路表示

### ssa_simulator

`ssa_simulator` 目录包含高性能的 C++ 后端实现：

- **SSA 解析器**：解析生成的 SSA 汇编代码
- **量子模拟器**：实现量子电路的模拟执行
- **Python 绑定**：通过 pybind11 提供 Python 接口
- **多架构支持**：提供带/不带 intrinsics 的实现，支持不同 CPU 架构

#### C++ 后端编译

使用 `Makefile` 编译 C++ 后端：

```bash
cd ssa_simulator
make  # 编译所有目标
make clean  # 清理编译产物
```

编译产物包括 Python 扩展模块和独立的 SSA 模拟执行器。

## 示例

`examples` 目录包含两个框架使用示例：

### example_001.py

一个简单的 Bell 态制备示例，演示了如何：
- 定义量子内核函数
- 创建 2 量子比特向量
- 应用 Hadamard 门和受控 X 门制备 Bell 态
- 执行量子测量
- 生成 SSA 汇编
- 使用 sample 函数进行多次采样

### example_002.py

一个可扩展的 GHZ 态制备示例，演示了如何：
- 定义带参数的量子内核函数
- 创建任意数量的量子比特向量
- 使用循环结构制备 GHZ 态
- 执行量子测量
- 生成 SSA 汇编
- 使用 sample 函数进行多次采样

## 快速开始

### 安装

```bash
git clone https://github.com/next-quantum/next-q.git
cd next-q
```

### 编译 C++ 后端（可选）

如果需要使用高性能的 C++ 后端，可以编译 ssa_simulator：

```bash
# 编译 C++ 后端
cd ssa_simulator
make
cd ..
```

### 基本用法

1. **编译C++模拟后端**：首先编译高性能C++后端，这是运行量子程序的必要步骤：

```bash
cd ssa_simulator
make
cd ..
```

2. **创建Python文件**：在项目根目录创建一个名为`my_quantum_script.py`的文件，内容如下：

```python
from quantum_framework import quantum_kernel, qvector, h, x, mz, sample

@quantum_kernel
def my_quantum_program():
    # 创建量子比特
    qubits = qvector(2)
    
    # 应用量子门
    h(qubits[0])
    x.ctrl(qubits[0], qubits[1])
    
    # 测量量子比特
    mz(qubits)

# 生成 SSA 汇编
ssa_assembly = my_quantum_program.generate_ssa_assembly()
print(ssa_assembly)

result = sample(my_quantum_program, shots_count=1000)
print(result)
```

3. **执行Python文件**：在项目根目录执行以下命令运行量子程序：

```bash
python my_quantum_script.py
```

执行后，你将看到生成的SSA汇编代码和量子测量结果。

## 支持的量子操作

- **基本量子门**：h, x, z
- **受控门**：x.ctrl, h.ctrl, z.ctrl
- **测量操作**：mz
- **量子比特创建**：qubit, qvector
- **条件执行**：支持基于测量结果的条件操作
- **循环结构**：支持 for 循环遍历量子比特

## 许可证

本项目采用 MIT 许可证，详情请查看 [LICENSE](LICENSE) 文件。

## 贡献

欢迎提交问题报告和拉取请求，帮助改进这个项目！

## 联系方式

如有任何问题或建议，请通过 GitHub Issues 与我们联系。