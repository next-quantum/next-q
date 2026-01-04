# NEXT-Q 量子计算框架

NEXT-Q 是一个轻量级量子计算框架，专注于将 CUDA-Q 风格的量子内核编译为自研的 SSA（静态单赋值）汇编。该框架允许开发者使用 Python 编写 CUDA-Q 风格的量子内核函数，并将其编译为低级 SSA 汇编，以便在各种量子计算平台上执行。

## 项目特点

- **兼容 CUDA-Q kernel**：支持 CUDA-Q 风格的量子内核函数定义
- **SSA 汇编生成**：将量子内核编译为低级 SSA 汇编
- **轻量级设计**：核心功能集中在单个文件中，易于理解和扩展
- **Python 友好**：使用 Python 装饰器语法定义量子内核
- **MIT 许可证**：允许自由使用、修改和分发

## 核心文件

### quantum_framework.py

`quantum_framework.py` 是框架的核心文件，包含以下主要组件：

- **量子内核装饰器**：`@quantum_kernel` 装饰器用于定义量子内核函数
- **量子比特管理**：`Qubit` 和 `QVector` 类用于管理量子比特
- **量子门实现**：支持 Hadamard (h)、Pauli-X (x)、Pauli-Z (z) 等基本量子门
- **测量操作**：`mz` 函数用于执行量子比特测量
- **SSA 汇编生成器**：将量子电路转换为 SSA 低级汇编
- **AST 解析器**：解析 Python 量子内核函数，生成量子电路表示

## 示例

`examples` 目录包含框架的使用示例：

### example_001.py

一个简单的量子程序示例，演示了如何：
- 定义量子内核函数
- 创建量子向量
- 应用量子门操作（Hadamard、受控 X 门）
- 执行量子测量
- 生成 SSA 汇编

## 快速开始

### 安装

```bash
git clone https://github.com/next-quantum/next-q.git
cd next-q
```

### 运行示例

```bash
python examples/example_001.py
```

### 基本用法

```python
from quantum_framework import quantum_kernel, qubit, qvector, h, x, z, mz

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
```

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
