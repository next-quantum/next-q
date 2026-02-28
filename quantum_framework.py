import ast
import enum
import inspect
import os
import sys
from typing import List, Dict, Any, Callable, Optional, TypeVar, Union, Tuple

import astor
import warnings

# 直接使用C++后端
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), 'ssa_simulator')))

# 类型定义和别名
Self = TypeVar('Self')  # 为了兼容Python 3.10，定义Self类型
int32 = int             # 明确的类型别名，方便将来扩展到其他类型
float32 = float         # 明确的类型别名，方便将来扩展到其他类型


class ComparisonOperator(enum.Enum):
    """比较操作符枚举"""
    EQ = '=='  # 等于
    NE = '!='  # 不等于
    LT = '<'   # 小于
    LE = '<='  # 小于等于
    GT = '>'   # 大于
    GE = '>='  # 大于等于


class QuantumGateAttribute(enum.Enum):
    """量子门属性枚举"""
    CTRL = 'ctrl'  # 受控门
    ADJ = 'adj'    # 共轭转置


class QuantumFunction(enum.Enum):
    """量子函数枚举"""
    MX = 'mx'      # 测量X操作
    MY = 'my'      # 测量Y操作
    MZ = 'mz'      # 测量操作
    QVECTOR = 'qvector'  # 创建量子向量
    QUBIT = 'qubit'      # 创建单个量子比特
    CONTROL = 'control'  # 受控量子操作
    H = 'h'        # Hadamard门
    X = 'x'        # Pauli-X门
    Y = 'y'        # Pauli-Y门
    Z = 'z'        # Pauli-Z门
    T = 't'        # T门
    S = 's'        # S门
    RX = 'rx'      # 绕X轴旋转门
    RY = 'ry'      # 绕Y轴旋转门
    RZ = 'rz'      # 绕Z轴旋转门
    R1 = 'r1'      # 绕Z轴旋转门（相位门，角度为λ/2）
    SWAP = 'swap'  # SWAP门


class Qubit:
    """量子比特类，代表量子电路中的单个量子比特"""
    
    def __init__(self, index: int32 = -1):
        self.index: int32 = index
        self.id: str = f'qubit_{index}' if index >= 0 else f'qubit_{id(self)}'

    def __repr__(self) -> str:
        return self.id


class QVector:
    """量子向量类，代表一组量子比特的集合"""
    
    def __init__(self, size: int, start_index: int):
        self.size: int = size
        self.qubits: List[Qubit] = [
            Qubit(i + start_index) for i in range(size)]

    def __getitem__(self, index: Union[int, slice]) -> Union[Qubit, 'QVector']:
        if isinstance(index, int):
            return self.qubits[index]
        elif isinstance(index, slice):
            # 处理切片操作，返回新的QVector对象
            sliced_qubits = self.qubits[index]
            new_qvector = QVector(len(sliced_qubits), 0)
            new_qvector.qubits = sliced_qubits
            return new_qvector
        return None

    def __len__(self) -> int:
        """返回量子向量的大小"""
        return self.size

    def front(self) -> Qubit:
        """返回量子向量的第一个量子比特"""
        return self.qubits[0] if self.size > 0 else None

    def back(self) -> Qubit:
        """返回量子向量的最后一个量子比特"""
        return self.qubits[-1] if self.size > 0 else None

    def size(self) -> int:
        """返回量子向量的大小"""
        return self.size

    def empty(self) -> bool:
        """检查量子向量是否为空"""
        return self.size == 0

    def __repr__(self) -> str:
        return f'qvector({self.size})'


class QuantumOperation:
    """量子操作类，代表量子电路中的一个操作"""
    
    def __init__(
            self,
            gate_name: str,
            qubits: tuple,
            controls: list,
            adjoint: bool,
            angle: Optional[float] = None):
        self.gate_name: str = gate_name
        self.qubits: tuple = qubits
        self.controls: List[Qubit] = controls
        self.adjoint: bool = adjoint
        self.angle: Optional[float] = angle

    def __repr__(self) -> str:
        adjoint_str: str = '.adj' if self.adjoint else ''
        ctrl_str: str = f'.ctrl({self.controls})' if self.controls else ''
        angle_str: str = f'({self.angle})' if self.angle is not None else ''
        qubits_repr = f'([{self.qubits[0]}])' if len(
            self.qubits) == 1 else f'{self.qubits}'
        return f'{self.gate_name}{angle_str}{ctrl_str}{adjoint_str}{qubits_repr}'


class ControlledGate:
    """受控门的辅助类，支持链式调用语法：x.ctrl([q0])([q1])"""

    def __init__(self, gate_name: str, controls: List[Qubit], adjoint: bool):
        self.gate_name: str = gate_name
        self.controls: List[Qubit] = controls
        self.adjoint: bool = adjoint

    def adj(self) -> 'ControlledGate':
        """返回一个带有adjoint标志的新受控门"""
        return ControlledGate(self.gate_name, self.controls, not self.adjoint)

    def __call__(self, *args, **kwargs) -> QuantumOperation:
        """处理目标比特调用，如 ([q1])"""
        # 检查是否有角度参数，用于旋转门
        angle = kwargs.get('angle', None)
        if angle is None and len(args) > 0 and isinstance(args[-1], (int, float)):
            # 角度可能作为最后一个位置参数传递
            angle = args[-1]
            # 移除角度参数，剩下的是量子比特参数
            args = args[:-1]
        
        # 对于旋转门的adjoint操作，角度取负值
        if self.adjoint and angle is not None:
            angle = -angle
            
        target_qubits = args[0] if len(args) > 0 else ()
        if isinstance(target_qubits, list):
            # 支持 [q1] 形式
            target_qubits = tuple(target_qubits)
        elif not isinstance(target_qubits, tuple):
            # 支持单个量子比特形式：q1
            target_qubits = (target_qubits,)

        return QuantumOperation(
            self.gate_name,
            target_qubits,
            self.controls,
            self.adjoint,
            angle)


class QuantumGate:
    """量子门类，代表量子电路中的一个量子门"""
    
    def __init__(self, name: str):
        self.name: str = name
        self.adjoint: bool = False
        self.controls: List[Qubit] = []

    def __call__(self, *args, **kwargs) -> QuantumOperation:
        # 检查是否有角度参数，用于旋转门
        angle = kwargs.get('angle', None)
        if angle is None and len(args) > 0 and isinstance(args[-1], (int, float)):
            # 角度可能作为最后一个位置参数传递
            angle = args[-1]
            # 移除角度参数，剩下的是量子比特参数
            args = args[:-1]
        
        # 对于旋转门的adjoint操作，角度取负值
        if self.adjoint and angle is not None:
            angle = -angle
            
        return QuantumOperation(self.name, args, self.controls, self.adjoint, angle)

    def ctrl(self, *args) -> Union['ControlledGate', 'QuantumOperation']:
        """
        处理受控门，支持五种调用方式：
        1. x.ctrl(control_qubit1, control_qubit_2, ..., target_qubit)
        2. x.ctrl([control_qubit1, control_qubit_2, ...], target_qubit)
        3. x.ctrl([q0])([q1])  # 列表控制位，列表目标位
        4. x.ctrl(q0)(q1)  # 单个控制位，单个目标位
        5. x.ctrl(q0, q1)(q2)  # 多个控制位，单个目标位
        """
        if len(args) == 2:
            if isinstance(args[0], list) and isinstance(args[1], Qubit):
                controls = args[0]
                target_qubit = args[1]
                return QuantumOperation(
                    self.name, (target_qubit,), controls, self.adjoint)

        if len(args) >= 2 and isinstance(args[-1], Qubit):
            target_qubit = args[-1]
            control_args = list(args[:-1])
            controls = _get_qubits_from_args(control_args)

            return QuantumOperation(
                self.name, (target_qubit,), controls, self.adjoint)

        controls = _get_qubits_from_args(args)
        if not controls:
            raise ValueError("控制比特必须都是Qubit类型")

        return ControlledGate(self.name, controls, self.adjoint)

    def adj(self) -> Self:
        gate: QuantumGate = QuantumGate(self.name)
        gate.adjoint: bool = not self.adjoint
        gate.controls: List[Qubit] = self.controls
        return gate


class Measurement:
    """测量类，代表量子电路中的一个测量操作"""
    
    def __init__(self, qubit: Qubit, measurement_reg: Optional[int] = None, measure_type: str = 'z'):
        self.qubit: Qubit = qubit
        self.measurement_reg: Optional[int] = measurement_reg  # 测量寄存器编号
        self.measure_type: str = measure_type  # 测量类型： 'x', 'y', 'z'
        self.result: Optional[int] = None

    def __repr__(self):
        measure_prefix = f'm{self.measure_type}'
        if self.measurement_reg is not None:
            return f'{measure_prefix}({self.qubit}) to measurement reg {self.measurement_reg}'
        else:
            return f'{measure_prefix}({self.qubit})'


class MeasureToClassicalOperation:
    """测量结果到经典寄存器的赋值操作类"""
    
    def __init__(
            self,
            measurement_reg: int,
            classical_reg: int,
            var_name: Optional[str] = None):
        self.measurement_reg: int = measurement_reg  # 测量寄存器编号
        self.classical_reg: int = classical_reg  # 经典寄存器编号
        self.var_name: Optional[str] = var_name  # 变量名

    def __repr__(self):
        name_str = f', name {self.var_name}' if self.var_name else ', unnamed'
        return f'assign measurement reg {self.measurement_reg} to classical reg {self.classical_reg}{name_str}'


class AllOperation:
    """All操作类，检查所有输入寄存器是否都为1"""
    
    def __init__(
            self,
            input_regs: List[int],
            output_reg: int,
            var_name: Optional[str] = None):
        self.input_regs: List[int] = input_regs  # 输入经典寄存器列表
        self.output_reg: int = output_reg  # 输出经典寄存器
        self.var_name: Optional[str] = var_name  # 变量名

    def __repr__(self):
        regs_str = ', '.join([f'c{reg}' for reg in self.input_regs])
        return f'all({self.var_name}) with regs [{regs_str}] to c{self.output_reg}'


class AnyOperation:
    """Any操作类，检查任意输入寄存器是否为1"""
    
    def __init__(
            self,
            input_regs: List[int],
            output_reg: int,
            var_name: Optional[str] = None):
        self.input_regs: List[int] = input_regs  # 输入经典寄存器列表
        self.output_reg: int = output_reg  # 输出经典寄存器
        self.var_name: Optional[str] = var_name  # 变量名

    def __repr__(self):
        regs_str = ', '.join([f'c{reg}' for reg in self.input_regs])
        return f'any({self.var_name}) with regs [{regs_str}] to c{self.output_reg}'


class AndOperation:
    """And操作类，执行逻辑与操作"""
    
    def __init__(self, left_reg: int, right_reg: int, output_reg: int):
        self.left_reg: int = left_reg  # 左操作数经典寄存器
        self.right_reg: int = right_reg  # 右操作数经典寄存器
        self.output_reg: int = output_reg  # 输出经典寄存器

    def __repr__(self):
        return f'c{self.left_reg} and c{self.right_reg} to c{self.output_reg}'


class OrOperation:
    """Or操作类，执行逻辑或操作"""
    
    def __init__(self, left_reg: int, right_reg: int, output_reg: int):
        self.left_reg: int = left_reg  # 左操作数经典寄存器
        self.right_reg: int = right_reg  # 右操作数经典寄存器
        self.output_reg: int = output_reg  # 输出经典寄存器

    def __repr__(self):
        return f'c{self.left_reg} or c{self.right_reg} to c{self.output_reg}'


class XorOperation:
    """Xor操作类，执行逻辑异或操作"""
    
    def __init__(self, left_reg: int, right_reg: int, output_reg: int):
        self.left_reg: int = left_reg  # 左操作数经典寄存器
        self.right_reg: int = right_reg  # 右操作数经典寄存器
        self.output_reg: int = output_reg  # 输出经典寄存器

    def __repr__(self):
        return f'c{self.left_reg} xor c{self.right_reg} to c{self.output_reg}'


class VarQubitOperation:
    """动态量子比特操作类，表示根据经典寄存器索引动态获取量子比特"""
    
    def __init__(self, var_name: str, index_reg: int, qvector_size: int):
        self.var_name: str = var_name  # 变量名
        self.index_reg: int = index_reg  # 存储量子比特索引的经典寄存器
        self.qvector_size: int = qvector_size  # 量子向量的大小

    def __repr__(self):
        return f'VarQubit: {self.var_name} = qvector[{self.index_reg}] (size: {self.qvector_size})'


class Condition:
    """条件类，代表量子电路中的条件分支"""
    
    def __init__(
            self,
            classical_reg: int,
            operator: ComparisonOperator,
            value: Any):
        self.classical_reg: int = classical_reg  # 用于条件判断的经典寄存器
        self.operator: ComparisonOperator = operator
        self.value: Any = value
        self.then_operations: List[Any] = []  # 存储then分支的操作
        self.else_operations: List[Any] = []  # 存储else分支的操作

    def __repr__(self):
        return f'Condition: if classical reg {self.classical_reg} {self.operator.value} {self.value}:'


class QuantumCircuit:
    """量子电路类，代表完整的量子电路"""
    
    def __init__(self):
        # 可以包含 QuantumOperation、Condition 或 Measurement
        self.operations: List[Any] = []
        self.qubits: List[Qubit] = []

    def add_operation(self, operation: Any):
        self.operations.append(operation)

    def add_qubit(self, qubit: Qubit):
        if qubit not in self.qubits:
            # 设置量子比特的索引为当前列表的长度，确保第n个量子比特的index为n
            qubit.index = len(self.qubits)
            qubit.id = f'qubit_{qubit.index}'
            self.qubits.append(qubit)

    def add_measurement(self, measurement: Measurement):
        # 测量寄存器编号总是等于量子比特的index
        measurement.measurement_reg = measurement.qubit.index

        # 将测量操作添加到operations列表中
        self.operations.append(measurement)

    def __repr__(self):
        return f"QuantumCircuit(qubits={len(self.qubits)}, operations={len(self.operations)})"


class QuantumProgram:
    """量子程序类，代表完整的量子程序"""
    
    def __init__(
            self,
            circuit: QuantumCircuit,
            source_code: str,
            original_func: Callable):
        self.circuit: QuantumCircuit = circuit
        self.source_code: str = source_code
        self.original_func: Callable = original_func
        self.ast_tree = None

    def __repr__(self) -> str:
        func_name: str = self.original_func.__name__
        qubit_count: int = len(self.circuit.qubits)
        operation_count: int = len(self.circuit.operations)

        # 获取函数签名
        signature: str = str(inspect.signature(self.original_func))

        # 详细列出所有操作
        operations_str = []
        for i, op in enumerate(self.circuit.operations):
            if isinstance(op, QuantumOperation):
                operations_str.append(f"  {i+1}. {op}")
            elif isinstance(op, Condition):
                operations_str.append(f"  {i+1}. {op}")
                operations_str.append("      - Then branch operations:")
                if op.then_operations:
                    for j, then_op in enumerate(op.then_operations):
                        operations_str.append(f"          {j+1}. {then_op}")
                else:
                    operations_str.append("          (no operations)")
                operations_str.append("      - Else branch operations:")
                if op.else_operations:
                    for j, else_op in enumerate(op.else_operations):
                        operations_str.append(f"          {j+1}. {else_op}")
                else:
                    operations_str.append("          (no operations)")
            elif isinstance(op, Measurement):
                # 显示测量操作，使用测量寄存器而非经典寄存器
                operations_str.append(f"  {i+1}. {op}")
            elif isinstance(op, MeasureToClassicalOperation):
                operations_str.append(f"  {i+1}. {op}")

        return (f"QuantumProgram(name='{func_name}'{signature}, "
                f"qubits={qubit_count}, operations={operation_count})\n"
                f"Source Code:\n'''\n{self.source_code}\n'''\n"
                f"Circuit Operations:\n{chr(10).join(operations_str)}")

    def generate_ssa_assembly(self, *args, **kwargs) -> str:
        """生成SSA低级汇编"""
        # 创建访问者并解析函数体
        visitor = QuantumProgramVisitor()
        # 将参数传递给访问者的变量字典
        # 从AST树中获取参数名（FunctionDef在Module的body中）
        function_def = self.ast_tree.body[0]  # 获取函数定义节点
        params = [arg.arg for arg in function_def.args.args]  # arg对象的arg属性是参数名
        for i, arg in enumerate(args):
            if i < len(params):
                visitor.variables[params[i]] = arg
        
        # 将当前函数的全局作用域传递给访问者，以便查找被调用的量子kernel
        visitor.global_env = self.original_func.__globals__
        # 更新evaluator的global_env
        visitor.evaluator.global_env = self.original_func.__globals__
        
        # 解析函数体
        visitor.visit(self.ast_tree.body[0])
        self.circuit = visitor.circuit
        
        assembler = SSAAssembler()
        return assembler.generate(self)


# 全局函数：从参数列表中提取所有量子比特对象
def _get_qubits_from_args(args: List[Any]) -> List[Qubit]:
    """从参数列表中提取所有量子比特对象"""
    qubits: List[Qubit] = []
    for arg in args:
        if isinstance(arg, Qubit):
            qubits.append(arg)
        elif isinstance(arg, QVector):
            qubits.extend(arg.qubits)
        elif isinstance(arg, list):
            qubits.extend([q for q in arg if isinstance(q, Qubit)])
    return qubits

# 全局函数：检查节点是否是mz函数调用


def _is_measure_call(node: ast.AST) -> bool:
    """检查节点是否是测量函数调用（mz、mx或my）"""
    return isinstance(node, ast.Call) and isinstance(
        node.func, ast.Name) and node.func.id in [QuantumFunction.MX.value, QuantumFunction.MY.value, QuantumFunction.MZ.value]


def _is_mz_call(node: ast.AST) -> bool:
    """检查节点是否是mz函数调用"""
    return isinstance(node, ast.Call) and isinstance(
        node.func, ast.Name) and node.func.id == QuantumFunction.MZ.value


def _get_measure_type(node: ast.AST) -> str:
    """获取测量函数调用的类型（z、x或y）"""
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
        func_id = node.func.id
        if func_id == QuantumFunction.MX.value:
            return 'x'
        elif func_id == QuantumFunction.MY.value:
            return 'y'
        elif func_id == QuantumFunction.MZ.value:
            return 'z'
    return 'z'

# 全局函数：从比较表达式中提取寄存器编号


def _extract_reg_from_compare(compare: ast.Compare, visitor) -> Optional[int]:
    """从比较表达式中提取寄存器编号"""
    left = compare.left

    # 处理测量比较：if mz(q0) == 1: 或 if mx(q0) == 1: 或 if my(q0) == 1:
    if _is_measure_call(left):
        qubit_arg = left.args[0]
        qubit_or_qvector = visitor.evaluator.evaluate(qubit_arg)
        if isinstance(qubit_or_qvector, Qubit):
            qubit = qubit_or_qvector
            # 获取测量类型
            measure_type = _get_measure_type(left)
            # 先添加测量操作
            measurement = Measurement(qubit, measure_type=measure_type)
            visitor.circuit.add_measurement(measurement)
            # 然后将测量结果分配到经典寄存器，确保测量结果被正确保存
            return visitor.measurement_handler.add_measure_to_classical(
                qubit.index)
    # 处理变量比较：if result == 1:
    elif isinstance(left, ast.Name):
        var_name = left.id
        if var_name in visitor.var_to_classical_reg:
            return visitor.var_to_classical_reg[var_name]
    # 处理布尔操作结果比较：if (a and b) == 1:
    elif isinstance(left, ast.BoolOp):
        return visitor.bool_op_handler.handle_bool_op(left)
    return None


class ExpressionEvaluator:
    """表达式求值器类，用于评估Python表达式"""
    
    # 类级别的表达式处理器字典，避免每次调用evaluate时重复创建
    expr_handlers = {
        # 常量和基本类型
        ast.Constant: '_eval_constant',
        ast.Name: '_eval_name',

        # 运算符表达式
        ast.BinOp: '_eval_binop',
        ast.UnaryOp: '_eval_unaryop',
        ast.Compare: '_eval_compare',
        ast.BoolOp: '_eval_boolop',
        ast.IfExp: '_eval_ifexp',

        # 容器类型
        ast.List: '_eval_list',
        ast.Tuple: '_eval_tuple',
        ast.Dict: '_eval_dict',
        ast.Set: '_eval_set',

        # 访问和调用
        ast.Attribute: '_eval_attribute',
        ast.Subscript: '_eval_subscript',
        ast.Call: '_eval_call',

        # 切片相关
        ast.Index: '_eval_index',
        ast.Slice: '_eval_slice',
        ast.ExtSlice: '_eval_ext_slice',
        ast.JoinedStr: '_eval_joined_str',
    }

    def __init__(self, variables, global_env=None):
        self.variables = variables
        self.global_env = global_env

    def evaluate(self, expr: ast.AST) -> Any:
        """评估任意Python表达式，返回其值"""
        # 如果表达式是None，直接返回None，不发出警告
        if expr is None:
            return None
        
        # 使用类型分发字典提高效率，确保覆盖所有主要AST节点类型
        handler_name = self.expr_handlers.get(type(expr))
        if handler_name:
            handler = getattr(self, handler_name)
            return handler(expr)
        
        # 其他情况下，检查是否是函数调用的返回值为None
        # 如果是None类型，直接返回None，不发出警告
        if isinstance(expr, ast.Constant) and expr.value is None:
            return None

        # 尝试处理未知节点类型，提供更好的错误信息
        warnings.warn(f"不支持的表达式类型: {type(expr).__name__}, 返回None")
        return None

    def _eval_constant(self, expr: ast.Constant) -> Any:
        """评估常量表达式"""
        return expr.value

    def _eval_name(self, expr: ast.Name) -> Any:
        """评估变量名表达式"""
        if expr.id in self.variables:
            return self.variables[expr.id]
        # 检查是否是全局变量（从调用者的全局作用域）
        elif self.global_env and expr.id in self.global_env:
            return self.global_env[expr.id]
        return None

    def _eval_binop(self, expr: ast.BinOp) -> Any:
        """评估二元操作表达式"""
        left = self.evaluate(expr.left)
        right = self.evaluate(expr.right)

        # 如果任一操作数为None，直接返回None，避免类型错误
        if left is None or right is None:
            return None

        op_handlers = {
            ast.Add: lambda x, y: x + y,
            ast.Sub: lambda x, y: x - y,
            ast.Mult: lambda x, y: x * y,
            ast.Div: lambda x, y: x / y,
            ast.Mod: lambda x, y: x % y,
            ast.Pow: lambda x, y: x ** y,
            ast.LShift: lambda x, y: x << y,
            ast.RShift: lambda x, y: x >> y,
            ast.BitOr: lambda x, y: x | y,
            ast.BitXor: lambda x, y: x ^ y,
            ast.BitAnd: lambda x, y: x & y,
            ast.FloorDiv: lambda x, y: x // y,
        }

        op_handler = op_handlers.get(type(expr.op))
        if op_handler:
            return op_handler(left, right)
        return None

    def _eval_compare(self, expr: ast.Compare) -> bool:
        """评估比较表达式"""
        left = self.evaluate(expr.left)
        results = []

        op_handlers = {
            ast.Eq: lambda x, y: x == y,
            ast.NotEq: lambda x, y: x != y,
            ast.Lt: lambda x, y: x < y,
            ast.LtE: lambda x, y: x <= y,
            ast.Gt: lambda x, y: x > y,
            ast.GtE: lambda x, y: x >= y,
            ast.Is: lambda x, y: x is y,
            ast.IsNot: lambda x, y: x is not y,
            ast.In: lambda x, y: x in y,
            ast.NotIn: lambda x, y: x not in y,
        }

        for op, comparator in zip(expr.ops, expr.comparators):
            right = self.evaluate(comparator)
            op_handler = op_handlers.get(type(op))
            if op_handler:
                results.append(op_handler(left, right))

        return all(results)

    def _eval_unaryop(self, expr: ast.UnaryOp) -> Any:
        """评估一元操作表达式"""
        operand = self.evaluate(expr.operand)

        op_handlers = {
            ast.UAdd: lambda x: +x,
            ast.USub: lambda x: -x,
            ast.Not: lambda x: not x,
            ast.Invert: lambda x: ~x,
        }

        op_handler = op_handlers.get(type(expr.op))
        if op_handler:
            return op_handler(operand)
        return None

    def _eval_call(self, expr: ast.Call) -> Any:
        """评估函数调用表达式"""
        if isinstance(expr.func, ast.Name):
            func_name = expr.func.id

            # 内置函数处理
            if func_name in ['range', 'enumerate', 'len']:
                args = [self.evaluate(arg) for arg in expr.args]
                # 统一处理：对于无效参数返回空范围/枚举，保持一致性
                try:
                    if func_name == 'range':
                        return range(*args)
                    elif func_name == 'enumerate':
                        return enumerate(*args)
                    elif func_name == 'len':
                        # 处理len()函数，返回对象的长度
                        return len(*args)
                except (TypeError, ValueError):
                    # 对于无效参数，返回空的可迭代对象或0
                    if func_name == 'range':
                        return range(0)
                    elif func_name == 'enumerate':
                        return enumerate([])
                    elif func_name == 'len':
                        return 0

            # 量子函数处理
            elif func_name in ['qvector', 'qubit', 'mz']:
                # 对于量子函数调用，返回None，因为它们在visit_Call中处理
                return None
        elif isinstance(expr.func, ast.Attribute):
            # 处理方法调用，如 qubits.front()
            obj = self.evaluate(expr.func.value)
            method_name = expr.func.attr
            args = [self.evaluate(arg) for arg in expr.args]
            if obj is not None:
                return getattr(obj, method_name)(*args)
        return None

    def _eval_subscript(self, expr: ast.Subscript) -> Any:
        """评估下标表达式"""
        base_value = self.evaluate(expr.value)
        slice_value = self.evaluate(expr.slice)
        if isinstance(base_value, (list, tuple)):
            return base_value[slice_value]
        elif hasattr(base_value, '__getitem__'):
            # 支持QVector对象和其他实现了__getitem__的对象
            return base_value[slice_value]
        return None

    def _eval_index(self, expr: ast.Index) -> Any:
        """评估索引表达式（兼容Python 3.7及更早版本）"""
        return self.evaluate(expr.value)

    def _eval_slice(self, expr: ast.Slice) -> slice:
        """评估切片表达式"""
        # 兼容不同Python版本的ast.Slice属性名称
        if hasattr(expr, 'start'):
            start = self.evaluate(expr.start)
            stop = self.evaluate(expr.stop)
        else:
            start = self.evaluate(expr.lower)
            stop = self.evaluate(expr.upper)
        step = self.evaluate(expr.step)
        return slice(start, stop, step)

    def _eval_list(self, expr: ast.List) -> list:
        """评估列表表达式"""
        return [self.evaluate(elt) for elt in expr.elts]

    def _eval_tuple(self, expr: ast.Tuple) -> tuple:
        """评估元组表达式"""
        return tuple(self.evaluate(elt) for elt in expr.elts)

    def _eval_dict(self, expr: ast.Dict) -> dict:
        """评估字典表达式"""
        keys = [self.evaluate(
            key) if key is not None else None for key in expr.keys]
        values = [self.evaluate(value) for value in expr.values]
        return dict(zip(keys, values))

    def _eval_set(self, expr: ast.Set) -> set:
        """评估集合表达式"""
        return {self.evaluate(elt) for elt in expr.elts}

    def _eval_ifexp(self, expr: ast.IfExp) -> Any:
        """评估条件表达式：a if b else c"""
        test = self.evaluate(expr.test)
        body = self.evaluate(expr.body)
        orelse = self.evaluate(expr.orelse)
        return body if test else orelse

    def _eval_boolop(self, expr: ast.BoolOp) -> Any:
        """评估布尔操作表达式"""
        values = [self.evaluate(value) for value in expr.values]
        if isinstance(expr.op, ast.And):
            return all(values)
        elif isinstance(expr.op, ast.Or):
            return any(values)
        return None

    def _eval_ext_slice(self, expr: ast.ExtSlice) -> Any:
        """评估扩展切片表达式"""
        return tuple(self.evaluate(dim) for dim in expr.dims)

    def _eval_joined_str(self, expr: ast.JoinedStr) -> str:
        """评估f-string表达式"""
        result = []
        for value in expr.values:
            if isinstance(value, ast.Constant):
                # 直接字符串部分
                result.append(str(value.value))
            elif isinstance(value, ast.FormattedValue):
                # 格式化表达式部分 {expr}
                fmt_value = self.evaluate(value.value)
                result.append(str(fmt_value))
            else:
                # 处理其他可能的节点类型
                eval_value = self.evaluate(value)
                if eval_value is not None:
                    result.append(str(eval_value))
        return ''.join(result)

    def _eval_attribute(self, expr: ast.Attribute) -> Any:
        """评估属性访问表达式"""
        value = self.evaluate(expr.value)
        if value is not None:
            return getattr(value, expr.attr, None)
        return None


class LoopHandler:
    """循环处理类，负责处理量子框架中的for和while循环语句
    
    包括：
    - 循环条件评估
    - 循环体执行
    - 循环变量管理
    - 循环结果处理
    """

    def __init__(self, visitor):
        """初始化LoopHandler实例

        Args:
            visitor: 访问者对象，用于访问和处理抽象语法树节点
        """
        self.visitor = visitor

    def handle_for(self, node: ast.For) -> None:
        """处理for循环语句"""
        # 评估迭代值
        iter_value = self.visitor.evaluator.evaluate(node.iter)

        # 处理循环体
        self._process_for_loop(node, iter_value)

    def _process_for_loop(self, node: ast.For, iter_value: Any) -> None:
        """处理for循环的核心逻辑"""
        # 即使iter_value是None，我们也需要处理循环体
        # 这是为了确保循环体中的量子门操作被正确生成
        if iter_value is None:
            self._visit_loop_body(node)
            return

        if isinstance(node.target, ast.Name):
            # 简单循环变量：for x in iter:
            self._handle_simple_for_loop(node, iter_value)
        elif isinstance(node.target, ast.Tuple):
            # 元组解包：for i, qubit in enumerate(iter):
            self._handle_tuple_for_loop(node, iter_value)

    def _visit_loop_body(self, node: Union[ast.For, ast.While]) -> None:
        """访问循环体中的所有语句，支持for和while循环"""
        for stmt in node.body:
            self.visitor.visit(stmt)
        for stmt in node.orelse:
            self.visitor.visit(stmt)

    def _handle_simple_for_loop(self, node: ast.For, iter_value: Any) -> None:
        """处理简单循环变量：for x in iter:"""
        loop_var_name = node.target.id
        # 保存原始变量值
        original_loop_var = self.visitor.variables.get(loop_var_name)

        # 执行循环
        for loop_var_value in iter_value:
            self.visitor.variables[loop_var_name] = loop_var_value
            self._visit_loop_body(node)

        # 恢复原始变量值
        self._restore_loop_vars({loop_var_name: original_loop_var})

    def _handle_tuple_for_loop(self, node: ast.For, iter_value: Any) -> None:
        """处理元组解包：for i, qubit in enumerate(iter):"""
        # 保存原始变量值
        original_vars = self._save_tuple_loop_vars(node.target)

        # 执行循环
        for loop_var_value in iter_value:
            # 解包元组并赋值给变量
            if isinstance(loop_var_value, tuple):
                self._assign_tuple_loop_vars(node.target, loop_var_value)
            self._visit_loop_body(node)

        # 恢复原始变量值
        self._restore_loop_vars(original_vars)

    def _save_tuple_loop_vars(self, target: ast.Tuple) -> Dict[str, Any]:
        """保存元组解包循环中的原始变量值"""
        original_vars = {}
        for elt in target.elts:
            if isinstance(elt, ast.Name):
                original_vars[elt.id] = self.visitor.variables.get(elt.id)
        return original_vars

    def _assign_tuple_loop_vars(
            self,
            target: ast.Tuple,
            loop_var_value: tuple) -> None:
        """将元组解包值赋值给变量"""
        for i, elt in enumerate(target.elts):
            if isinstance(elt, ast.Name) and i < len(loop_var_value):
                var_name = elt.id
                self.visitor.variables[var_name] = loop_var_value[i]

    def _restore_loop_vars(self, original_vars: Dict[str, Any]) -> None:
        """统一恢复循环变量的原始值，支持简单变量和元组解包变量"""
        for var_name, orig_val in original_vars.items():
            if orig_val is not None:
                self.visitor.variables[var_name] = orig_val
            elif var_name in self.visitor.variables:
                del self.visitor.variables[var_name]

    def handle_while(self, node: ast.While) -> None:
        """处理while循环语句"""
        # 尝试评估条件表达式（仅用于已知变量）
        self._process_while_loop(node)

    def _process_while_loop(self, node: ast.While) -> None:
        """处理while循环的核心逻辑"""
        try:
            condition_result = self.visitor.evaluator.evaluate(node.test)
            if isinstance(condition_result, bool):
                # 条件结果已知，执行一次循环体
                # 注意：在量子框架中，我们不能执行真正的while循环，因为这会导致生成无限多的量子门操作
                # 我们只能执行一次循环体，或者发出警告
                if condition_result:
                    self._visit_loop_body(node)
                return
        except (ValueError, TypeError, AttributeError):
            # 无法评估条件表达式，发出警告并跳过
            import warnings
            warnings.warn(
                f"无法评估while循环条件表达式: {astor.to_source(node.test).strip()}, 跳过循环")
            return


class ConditionHandler:
    """条件处理类，负责处理量子框架中的条件语句"""

    def __init__(self, visitor):
        self.visitor = visitor

    def handle_if(self, node):
        """处理条件语句，支持更复杂的条件表达式"""
        # 保存当前操作数量，用于后续插入条件操作
        start_op_count = len(self.visitor.circuit.operations)

        # 尝试直接评估已知条件，仅用于循环中的已知变量
        if self._try_evaluate_known_condition(node):
            return

        # 处理条件，获取条件信息
        condition_info = self._handle_condition_test(node.test)

        if condition_info is None:
            # 条件无法解析，直接执行then分支
            self._handle_unresolved_condition(node)
            return

        # 收集then和else分支的操作
        then_ops, else_ops = self._collect_condition_operations(node)

        # 创建并添加条件操作
        self._create_and_add_condition(
            *condition_info, then_ops, else_ops, start_op_count)

    def _try_evaluate_known_condition(self, node: ast.If) -> bool:
        """尝试直接评估已知条件，返回是否成功评估"""
        try:
            if isinstance(
                    node.test,
                    ast.Compare) and isinstance(
                    node.test.left,
                    ast.BinOp):
                # 检查是否是循环变量的算术表达式条件
                if isinstance(node.test.left.left, ast.Name):
                    var_name = node.test.left.left.id
                    if var_name in self.visitor.variables:
                        # 条件结果已知，直接执行相应分支
                        condition_result = self.visitor.evaluator.evaluate(
                            node.test)
                        if condition_result:
                            # 条件为真，执行then分支
                            for stmt in node.body:
                                self.visitor.visit(stmt)
                        else:
                            # 条件为假，执行else分支
                            for stmt in node.orelse:
                                self.visitor.visit(stmt)
                        return True
        except BaseException:
            # 无法评估条件表达式，继续使用原有逻辑
            pass
        return False

    def _handle_condition_test(
            self, test: ast.AST) -> Optional[Tuple[int, ComparisonOperator, Any, bool]]:
        """处理条件表达式，返回条件信息"""
        if isinstance(test, ast.Compare):
            return self._handle_compare_condition(test)
        elif isinstance(test, ast.BoolOp):
            return self._handle_bool_op_condition(test)
        elif isinstance(test, ast.Name):
            return self._handle_name_condition(test)
        return None

    def _handle_compare_condition(
            self, compare: ast.Compare) -> Optional[Tuple[int, ComparisonOperator, Any, bool]]:
        """处理比较条件，如 mz(q0) == 1 或 result == 1"""
        left: ast.AST = compare.left

        if isinstance(left, ast.BinOp):
            # 处理算术表达式条件，如 i % 2 == 0
            return self._handle_binop_compare_condition(left, compare)

        # 使用统一的方法提取寄存器，支持多种比较类型
        classical_reg = _extract_reg_from_compare(compare, self.visitor)
        if classical_reg is not None:
            operator, compare_value = self._get_comparison_info(compare)
            # 判断是否为测量条件（直接调用mz的情况）
            is_var_condition = not _is_mz_call(left)
            return classical_reg, operator, compare_value, is_var_condition

        return None

    def _handle_binop_compare_condition(self,
                                        binop: ast.BinOp,
                                        compare: ast.Compare) -> Optional[Tuple[int,
                                                                                ComparisonOperator,
                                                                                Any,
                                                                                bool]]:
        """处理算术表达式条件，如 i % 2 == 0"""
        # 算术表达式条件由_try_evaluate_known_condition方法处理，直接返回None
        return None

    def _handle_bool_op_condition(
            self, bool_op: ast.BoolOp) -> Optional[Tuple[int, ComparisonOperator, Any, bool]]:
        """处理布尔操作条件，如 result1 and all(result2) 或 result1 or result2"""
        classical_reg = self.visitor.bool_op_handler.handle_bool_op(bool_op)
        if classical_reg is not None:
            operator = ComparisonOperator.EQ
            compare_value = 1
            is_var_condition = True
            return classical_reg, operator, compare_value, is_var_condition
        return None

    def _handle_name_condition(
            self, name: ast.Name) -> Optional[Tuple[int, ComparisonOperator, Any, bool]]:
        """处理简单变量条件，如 if combined:"""
        var_name: str = name.id
        if var_name in self.visitor.var_to_classical_reg:
            classical_reg = self.visitor.var_to_classical_reg[var_name]
            operator = ComparisonOperator.EQ
            compare_value = 1
            is_var_condition = True
            return classical_reg, operator, compare_value, is_var_condition
        return None

    def _handle_unresolved_condition(self, node: ast.If) -> None:
        """处理无法解析的条件，直接执行then分支"""
        import warnings
        warnings.warn(
            f"无法解析条件表达式: {astor.to_source(node.test).strip()}, 直接执行then分支")

        for stmt in node.body:
            self.visitor.visit(stmt)

    def _collect_condition_operations(
            self, node: ast.If) -> Tuple[List[Any], List[Any]]:
        """收集条件分支的操作"""
        # 收集then分支的操作
        then_start = len(self.visitor.circuit.operations)
        for stmt in node.body:
            self.visitor.visit(stmt)
        then_ops = self.visitor.circuit.operations[then_start:]
        del self.visitor.circuit.operations[then_start:]

        # 收集else分支的操作
        else_ops = []
        if node.orelse:
            else_start = len(self.visitor.circuit.operations)
            for stmt in node.orelse:
                self.visitor.visit(stmt)
            else_ops = self.visitor.circuit.operations[else_start:]
            del self.visitor.circuit.operations[else_start:]

        return then_ops, else_ops

    def _create_and_add_condition(
            self,
            classical_reg,
            operator,
            compare_value,
            is_var_condition,
            then_ops,
            else_ops,
            start_op_count):
        """创建并添加条件操作"""
        # 创建条件操作
        condition = Condition(classical_reg, operator, compare_value)
        condition.then_operations = then_ops
        condition.else_operations = else_ops

        if is_var_condition:
            # 变量条件，直接添加到电路末尾
            self.visitor.circuit.add_operation(condition)
        else:
            # 直接测量条件，插入到测量操作之后
            self._insert_condition_after_measurement(condition, start_op_count)

    def _insert_condition_after_measurement(self, condition, start_op_count):
        """将条件操作插入到测量操作之后"""
        for i in reversed(
            range(
                start_op_count, len(
                self.visitor.circuit.operations))):
            op = self.visitor.circuit.operations[i]
            if isinstance(op, MeasureToClassicalOperation):
                self.visitor.circuit.operations.insert(i + 1, condition)
                break

    def _get_comparison_info(self, compare_node):
        """从比较节点中提取操作符和值"""
        op = compare_node.ops[0]
        operator = None
        value = self.visitor.evaluator.evaluate(compare_node.comparators[0])

        # 映射AST比较操作符到内部ComparisonOperator枚举
        if isinstance(op, ast.Eq):
            operator = ComparisonOperator.EQ
        elif isinstance(op, ast.NotEq):
            operator = ComparisonOperator.NE
        elif isinstance(op, ast.Lt):
            operator = ComparisonOperator.LT
        elif isinstance(op, ast.LtE):
            operator = ComparisonOperator.LE
        elif isinstance(op, ast.Gt):
            operator = ComparisonOperator.GT
        elif isinstance(op, ast.GtE):
            operator = ComparisonOperator.GE

        return operator, value


class MeasurementHandler:
    """测量处理类，负责处理量子框架中的测量操作"""

    def __init__(self, visitor):
        self.visitor = visitor

    def add_measure_to_classical(
            self,
            qubit_index: int,
            var_name: Optional[str] = None) -> int:
        """将测量结果分配到经典寄存器，返回经典寄存器编号"""
        classical_reg = self.visitor.next_classical_reg
        self.visitor.next_classical_reg += 1

        assign_op = MeasureToClassicalOperation(
            qubit_index, classical_reg, var_name)
        self.visitor.circuit.operations.append(assign_op)

        if var_name:
            # 允许多个量子比特的测量结果映射到同一个变量名
            # 这允许后续使用all()/any()等函数处理多个测量结果
            self.visitor.var_to_classical_reg[var_name] = classical_reg
        self.visitor.measurement_to_classical[qubit_index] = classical_reg

        return classical_reg

    def handle_measure_call(
            self,
            evaluated_args: List[Any],
            return_value: bool = False,
            var_name: Optional[str] = None,
            measure_type: str = 'z') -> Any:
        """处理测量调用（mz、mx或my）

        Args:
            evaluated_args: 已评估的参数列表，包含要测量的量子比特
            return_value: 是否在表达式上下文中调用
                          - False: 直接调用上下文，生成测量操作
                          - True: 表达式上下文，生成测量操作，但返回占位值
            var_name: 可选的变量名，用于将测量结果映射到经典寄存器
            measure_type: 测量类型：'x', 'y', 'z'

        Returns:
            None或空列表，因为我们不执行真正的测量，只记录测量操作
        """
        target_qubits: List[Qubit] = self.visitor.qubit_handler.get_target_qubits(
            evaluated_args)

        # 对每个量子比特，先执行测量，再分配到经典寄存器
        # 这样可以确保测量操作和赋值操作交替执行，与预期的ASM顺序一致
        for qubit in target_qubits:
            # 在所有上下文中都生成测量操作，确保条件表达式中的测量也能被正确处理
            measurement: Measurement = Measurement(qubit, measure_type=measure_type)
            self.visitor.circuit.add_measurement(measurement)

            # 如果提供了var_name，将测量结果分配到经典寄存器
            # 支持将多个量子比特的测量结果映射到同一个变量名
            # 这允许后续使用all()/any()等函数处理多个测量结果
            if var_name:
                self.add_measure_to_classical(qubit.index, var_name)

        # 我们不执行真正的测量，只返回占位值
        return None if len(target_qubits) == 1 else []

    def handle_mz_call(
            self,
            evaluated_args: List[Any],
            return_value: bool = False,
            var_name: Optional[str] = None) -> Any:
        """处理mz（测量）调用

        Args:
            evaluated_args: 已评估的参数列表，包含要测量的量子比特
            return_value: 是否在表达式上下文中调用
                          - False: 直接调用上下文，生成测量操作
                          - True: 表达式上下文，生成测量操作，但返回占位值
            var_name: 可选的变量名，用于将测量结果映射到经典寄存器

        Returns:
            None或空列表，因为我们不执行真正的测量，只记录测量操作
        """
        return self.handle_measure_call(evaluated_args, return_value, var_name, 'z')


class QubitHandler:
    """量子比特处理类，负责处理量子比特相关操作"""

    def __init__(self, visitor):
        self.visitor = visitor

    def handle_subscript(
            self,
            node: ast.Subscript,
            var_name: Optional[str] = None) -> Any:
        """处理下标访问，如 qubits[0] 或 qubits[0:2]"""
        base_value: Any = self.visitor.evaluator.evaluate(node.value)

        if isinstance(base_value, QVector):
            qvector_obj: QVector = base_value

            # 评估切片对象，处理不同类型的索引
            slice_value = self.visitor.evaluator.evaluate(node.slice)

            if isinstance(slice_value, slice):
                # 处理切片操作：qubits[start:end:step]
                return self._handle_slice_operation(
                    qvector_obj, slice_value, var_name)
            elif isinstance(slice_value, int):
                # 处理整数索引：qubits[index]
                return self._handle_integer_index(
                    qvector_obj, slice_value, var_name)
        return None

    def _handle_slice_operation(
            self,
            qvector_obj: QVector,
            slice_obj: slice,
            var_name: Optional[str] = None) -> QVector:
        """处理量子向量的切片操作"""
        # 获取切片参数，处理默认值
        start = slice_obj.start if slice_obj.start is not None else 0
        stop = slice_obj.stop if slice_obj.stop is not None else qvector_obj.size
        step = slice_obj.step if slice_obj.step is not None else 1

        # 确保start和stop在有效范围内
        start = max(0, start)
        stop = min(qvector_obj.size, stop)

        # 计算切片后的大小
        if step > 0:
            if start >= stop:
                size = 0
            else:
                size = ((stop - start - 1) // step) + 1
        else:
            if start <= stop:
                size = 0
            else:
                size = ((start - stop - 1) // abs(step)) + 1

        if size == 0:
            return QVector(0, len(self.visitor.circuit.qubits))

        # 创建新的量子向量，复用原向量中的量子比特
        new_qubits = [qvector_obj.qubits[i] for i in range(
            start, stop, step) if 0 <= i < qvector_obj.size]

        # 创建空的 QVector 并直接设置 qubits 列表
        new_qvector = QVector(size, 0)
        new_qvector.qubits = new_qubits

        if var_name:
            self.visitor.variables[var_name] = new_qvector
        return new_qvector

    def _handle_integer_index(
            self,
            qvector_obj: QVector,
            index: int,
            var_name: Optional[str] = None) -> Qubit:
        """处理量子向量的整数索引访问"""
        if 0 <= index < qvector_obj.size:
            qubit: Qubit = qvector_obj[index]
            if var_name:
                self.visitor.variables[var_name] = qubit
            return qubit
        return None

    def get_target_qubits(self, args: List[Any]) -> List[Qubit]:
        """统一处理目标量子比特，返回 Qubit 列表"""
        target_qubits: List[Qubit] = []
        for arg in args:
            if isinstance(arg, Qubit):
                target_qubits.append(arg)
            elif isinstance(arg, QVector):
                # 支持切片后的量子向量：qubits[0:2]
                target_qubits.extend(arg.qubits)
            elif isinstance(arg, list):
                # 处理列表参数，如 [q1]
                target_qubits.extend(self.get_target_qubits(arg))
            # 不处理条件表达式，因为它们会在_handle_call中被特殊处理
        return target_qubits

    def create_quantum_operation(
            self,
            gate_name: str,
            target_qubits: List[Qubit],
            controls: List[Qubit] = None,
            adjoint: bool = False,
            angle: Optional[float] = None) -> None:
        """统一创建量子操作，支持单个量子比特或量子向量"""
        controls = controls or []

        # 处理双量子比特门（SWAP）
        if gate_name == QuantumFunction.SWAP.value and len(target_qubits) >= 2:
            # SWAP门需要两个目标量子比特，创建一个操作
            quantum_op = QuantumOperation(
                gate_name, tuple(target_qubits[:2]), controls, adjoint, angle)
            self.visitor.circuit.add_operation(quantum_op)
        else:
            # 单量子比特门：为每个目标量子比特创建量子操作
            for target_qubit in target_qubits:
                quantum_op = QuantumOperation(
                    gate_name, (target_qubit,), controls, adjoint, angle)
                self.visitor.circuit.add_operation(quantum_op)


class BoolOpHandler:
    """布尔操作处理类，负责处理量子框架中的布尔操作"""

    def __init__(self, visitor):
        self.visitor = visitor

    def handle_bool_op(self, bool_op: ast.BoolOp) -> Optional[int]:
        """统一处理布尔操作，返回结果寄存器

        支持所有布尔操作上下文：
        - 赋值：combined = m1 or m2
        - 条件：if m1 and m2:
        - 比较：if (m1 or m2) == 1:
        """
        input_registers = self._handle_collect_inputs(bool_op)

        if not input_registers:
            return None

        # 根据布尔操作类型处理
        if isinstance(bool_op.op, ast.And):
            return self._compute_bool_chain(input_registers, AndOperation)
        elif isinstance(bool_op.op, ast.Or):
            return self._compute_bool_chain(input_registers, OrOperation)
        else:
            # 不支持的布尔操作类型
            return None

    def _handle_collect_inputs(self, bool_op: ast.BoolOp) -> List[int]:
        """收集布尔操作的输入寄存器"""
        input_registers = []

        for operand in bool_op.values:
            operand_reg = self._handle_evaluate_operand(operand)
            if operand_reg is not None:
                input_registers.append(operand_reg)

        return input_registers

    def _handle_evaluate_operand(self, operand: ast.AST) -> Optional[int]:
        """评估布尔操作的单个操作数，返回寄存器编号"""
        # 处理函数调用，如 all(result2)
        if isinstance(operand, ast.Call):
            return self._handle_call_operand(operand)
        # 处理变量，如 result1
        elif isinstance(operand, ast.Name):
            return self._handle_var_operand(operand)
        # 处理比较表达式，如 result > 0 或 mz(q0) == 1
        elif isinstance(operand, ast.Compare):
            return self._handle_compare_operand(operand)
        return None

    def _handle_call_operand(self, operand: ast.Call) -> Optional[int]:
        """处理函数调用操作数，如 all(result2) 或 any(result2)"""
        func_name = operand.func.id if isinstance(
            operand.func, ast.Name) else None

        if func_name == 'all':
            return self._handle_quantum_bool_func(operand, AllOperation)
        elif func_name == 'any':
            return self._handle_quantum_bool_func(operand, AnyOperation)
        return None

    def _handle_quantum_bool_func(
            self,
            func_call: ast.Call,
            operation_class) -> Optional[int]:
        """处理 all() 或 any() 形式的函数调用"""
        arg = func_call.args[0]
        if isinstance(arg, ast.Name):
            var_name = arg.id
            # 查找所有以var_name命名的测量结果寄存器
            measurement_registers = []
            for op in self.visitor.circuit.operations:
                if isinstance(
                        op, MeasureToClassicalOperation) and op.var_name == var_name:
                    measurement_registers.append(op.classical_reg)

            if measurement_registers:
                # 创建一个新的经典寄存器来存储结果
                result_reg = self.visitor.next_classical_reg
                self.visitor.next_classical_reg += 1

                # 添加操作到电路
                op = operation_class(
                    measurement_registers, result_reg, var_name)
                self.visitor.circuit.add_operation(op)

                return result_reg
        return None

    def _handle_var_operand(self, operand: ast.Name) -> Optional[int]:
        """处理变量操作数，如 result1"""
        var_name = operand.id
        if var_name in self.visitor.var_to_classical_reg:
            return self.visitor.var_to_classical_reg[var_name]
        return None

    def _handle_compare_operand(self, operand: ast.Compare) -> Optional[int]:
        """处理比较表达式操作数，如 result > 0 或 mz(q0) == 1 或 mx(q0) == 1 或 my(q0) == 1"""
        # 处理测量操作，如 mz(q0) == 1、mx(q0) == 1 或 my(q0) == 1
        if _is_measure_call(operand.left):
            return self._handle_measure_compare_operand(operand)
        # 处理普通比较表达式，如 result > 0
        elif isinstance(operand.left, ast.Name):
            return self._handle_var_compare_operand(operand)
        return None

    def _handle_measure_compare_operand(
            self, compare: ast.Compare) -> Optional[int]:
        """处理测量比较操作数，如 mx(q0) == 1、my(q0) == 1 或 mz(q0) == 1"""
        # 提取测量操作的量子比特
        qubit_arg = compare.left.args[0]
        qubit_or_qvector = self.visitor.evaluator.evaluate(qubit_arg)
        if isinstance(qubit_or_qvector, Qubit):
            qubit = qubit_or_qvector
            # 获取测量类型
            measure_type = _get_measure_type(compare.left)
            # 生成测量操作
            measurement = Measurement(qubit, measure_type=measure_type)
            self.visitor.circuit.add_measurement(measurement)
            # 将测量结果分配到经典寄存器
            reg = self.visitor.measurement_handler.add_measure_to_classical(
                qubit.index)

            # 处理比较操作
            return self._handle_comparison(
                reg, compare.ops[0], compare.comparators[0])
        return None

    def _handle_var_compare_operand(
            self, compare: ast.Compare) -> Optional[int]:
        """处理变量比较操作数，如 result > 0"""
        var_name = compare.left.id
        if var_name in self.visitor.var_to_classical_reg:
            reg = self.visitor.var_to_classical_reg[var_name]

            # 处理比较操作
            return self._handle_comparison(
                reg, compare.ops[0], compare.comparators[0])
        return None

    def _handle_comparison(
            self,
            reg: int,
            op: ast.cmpop,
            comparator: ast.AST) -> Optional[int]:
        """处理比较操作，生成比较结果寄存器"""
        # 评估比较值
        comparator_value = self.visitor.evaluator.evaluate(comparator)

        # 目前只支持与常量比较，且常量为0或1
        if not isinstance(
                comparator_value,
                int) or comparator_value not in (
                0,
                1):
            return None

        # 对于 == 比较：如果比较值是1，直接返回原寄存器；如果是0，需要反转
        if isinstance(op, ast.Eq):
            if comparator_value == 0:
                # 创建反转寄存器
                inverted_reg = self.visitor.next_classical_reg
                self.visitor.next_classical_reg += 1
                # 创建并添加反转操作
                # 这里我们使用一个简单的方式来表示反转：将1与原寄存器进行异或
                xor_op = XorOperation(reg, inverted_reg, inverted_reg)
                self.visitor.circuit.add_operation(xor_op)
                return inverted_reg
            return reg
        # 对于 != 比较：如果比较值是1，需要反转；如果是0，直接返回原寄存器
        elif isinstance(op, ast.NotEq):
            if comparator_value == 1:
                # 创建反转寄存器
                inverted_reg = self.visitor.next_classical_reg
                self.visitor.next_classical_reg += 1
                # 创建并添加反转操作
                xor_op = XorOperation(reg, inverted_reg, inverted_reg)
                self.visitor.circuit.add_operation(xor_op)
                return inverted_reg
            return reg
        # 其他比较操作（>, <, >=, <=）暂不支持
        return None

    def _compute_bool_chain(
            self,
            input_registers: List[int],
            operation_class) -> int:
        """计算布尔操作链的结果，支持AND和OR操作

        Args:
            input_registers: 输入寄存器列表
            operation_class: 布尔操作类（AndOperation或OrOperation）

        Returns:
            结果寄存器编号
        """
        result_reg = input_registers[0]
        for i in range(1, len(input_registers)):
            next_reg = input_registers[i]
            temp_reg = self.visitor.next_classical_reg
            self.visitor.next_classical_reg += 1

            # 创建布尔操作
            bool_operation = operation_class(result_reg, next_reg, temp_reg)
            self.visitor.circuit.add_operation(bool_operation)
            result_reg = temp_reg

        return result_reg


class CallHandler:
    """函数调用处理类，负责处理量子框架中的函数调用"""

    def __init__(self, visitor):
        self.visitor = visitor

    def handle_call(self, node):
        """处理函数调用，如 h(q0), mz(q0) 等"""
        # 检查是否是内置函数调用，如 len()，这些是经典操作，不需要生成量子门
        if isinstance(node.func, ast.Name):
            func_name = node.func.id
            if func_name in ['len', 'range', 'enumerate']:
                # 这些是经典内置函数，不需要生成量子门，直接返回evaluator评估的结果
                return self.visitor.evaluator.evaluate(node)
        
        # 评估参数，直接内联_evaluate_call_args逻辑
        evaluated_args = [
            self.visitor.evaluator.evaluate(arg) for arg in node.args]

        # 检查是否有保存的变量名（来自赋值语句）
        var_name = getattr(node, '_var_name', None)

        # 检查是否有动态量子比特变量
        var_qubit_info, dynamic_var_name = self._check_var_qubit_args(
            evaluated_args)

        if var_qubit_info and dynamic_var_name:
            # 处理动态量子比特调用
            self._handle_dynamic_qubit_call(
                node, var_qubit_info, dynamic_var_name)
            return None
        else:
            # 特殊处理：检查是否是量子kernel调用（直接通过函数对象检查）
            if isinstance(node.func, ast.Name):
                func_name = node.func.id
                # 尝试获取函数对象：先从变量中找，再从全局中找
                func_obj = None
                
                # 从当前作用域变量中查找
                if func_name in self.visitor.variables:
                    func_obj = self.visitor.variables[func_name]
                # 从调用者的全局作用域查找
                elif hasattr(self.visitor, 'global_env'):
                    func_obj = self.visitor.global_env.get(func_name)
                # 尝试获取调用者的全局作用域
                elif hasattr(self.visitor, 'original_func'):
                    func_obj = getattr(self.visitor.original_func.__globals__, func_name, None)
                
                if isinstance(func_obj, QuantumProgram):
                    # 处理量子kernel调用，内联其操作
                    return self._handle_kernel_call(func_obj, evaluated_args)
            
            # 正常处理函数调用，传递保存的变量名和评估后的参数
            return self._dispatch_call(
                node, var_name=var_name, evaluated_args=evaluated_args)

    def _check_var_qubit_args(
            self, args: List[Any]) -> Tuple[Optional[Dict], Optional[str]]:
        """检查参数列表中是否有动态量子比特变量，支持嵌套结构"""
        def check_item(item):
            """递归检查单个项目是否为动态量子比特"""
            if isinstance(item, dict) and item.get('type') == 'var_qubit':
                # 内联_find_var_by_value逻辑
                var_name = None
                for name, var_value in self.visitor.variables.items():
                    # 确保类型匹配后再进行比较
                    if type(var_value) == type(item) and var_value == item:
                        var_name = name
                        break
                return item, var_name
            elif isinstance(item, list):
                for sub_item in item:
                    result = check_item(sub_item)
                    if result[0]:
                        return result
            elif isinstance(item, QVector):
                # 检查QVector是否直接等于动态量子比特变量
                qvector_var_name = None
                for name, var_value in self.visitor.variables.items():
                    # 只有当var_value也是QVector时才进行比较
                    if isinstance(var_value, QVector) and var_value == item:
                        qvector_var_name = name
                        break
                if qvector_var_name:
                    qvector_info = self.visitor.variables.get(qvector_var_name)
                    if isinstance(qvector_info, dict) and qvector_info.get(
                            'type') == 'var_qubit':
                        return qvector_info, qvector_var_name
            return None, None

        for arg in args:
            result = check_item(arg)
            if result[0]:
                return result

        return None, None

    def _handle_dynamic_qubit_call(
            self,
            node: ast.Call,
            var_qubit_info: Dict,
            dynamic_var_name: str) -> None:
        """处理动态量子比特调用，支持更多语法模式"""
        func_name = self._get_gate_name(node.func)
        index_reg = var_qubit_info['index_reg']

        # 创建动态量子比特操作，使用标准字典格式
        dynamic_qubit_op = {
            'type': 'dynamic_qubit_op',
            'gate_name': func_name,
            'index_reg': index_reg
        }

        # 保存为特殊操作，在SSA汇编生成时处理
        self.visitor.circuit.add_operation(dynamic_qubit_op)

    def _dispatch_call(self,
                       node: ast.Call,
                       var_name: Optional[str] = None,
                       return_value: bool = False,
                       evaluated_args: Optional[List[Any]] = None) -> Any:
        """处理函数调用，根据不同类型分发到不同处理函数"""
        if isinstance(node.func, ast.Name):
            return self._handle_name_call(
                node, var_name, return_value, evaluated_args)
        # 处理属性调用，如 x.adj, x.ctrl
        elif isinstance(node.func, ast.Attribute):
            return self._handle_attribute_call(
                node, var_name, return_value, evaluated_args)
        # 处理链式调用，如 x.adj()(q0) 或 x.ctrl(q0)(q1) 或 x.ctrl(q0).adj()(q1)
        elif isinstance(node.func, ast.Call):
            return self._handle_chain_call(node, return_value, evaluated_args)
        return None

    def _handle_name_call(self,
                          node: ast.Call,
                          var_name: Optional[str] = None,
                          return_value: bool = False,
                          evaluated_args: Optional[List[Any]] = None) -> Any:
        # 处理直接函数名调用，如 h(q0), x(q0), mz(q0) 等
        func_name: str = node.func.id
        # 使用传递的评估后参数，避免重复评估
        if evaluated_args is None:
            evaluated_args = [
                self.visitor.evaluator.evaluate(arg) for arg in node.args]

        if func_name in [QuantumFunction.MZ.value, QuantumFunction.MX.value, QuantumFunction.MY.value]:
            # 处理测量操作，传递var_name参数和测量类型
            measure_type = 'z'
            if func_name == QuantumFunction.MX.value:
                measure_type = 'x'
            elif func_name == QuantumFunction.MY.value:
                measure_type = 'y'
            elif func_name == QuantumFunction.MZ.value:
                measure_type = 'z'
            return self._handle_measure_call(evaluated_args, return_value, var_name, measure_type)
        elif func_name == QuantumFunction.QVECTOR.value:
            # 处理量子向量创建
            return self._handle_qvector_call(
                evaluated_args, var_name, return_value)
        elif func_name == QuantumFunction.QUBIT.value:
            # 处理单个量子比特创建
            return self._handle_qubit_call(var_name, return_value)
        elif func_name == QuantumFunction.CONTROL.value:
            # 处理受控量子操作: control(kernel, control_qubits, *target_qubits)
            if len(evaluated_args) < 2:
                raise ValueError("control() expects at least 2 arguments: kernel, control_qubits, *target_qubits")
            
            kernel = evaluated_args[0]
            control_qubits = evaluated_args[1]
            target_qubits = evaluated_args[2:]
            
            if not isinstance(kernel, QuantumProgram):
                raise ValueError("First argument to control() must be a quantum_kernel")
            
            # 将控制 qubits 转换为列表
            if isinstance(control_qubits, QVector):
                control_qubits_list = list(control_qubits.qubits)
            elif isinstance(control_qubits, Qubit):
                control_qubits_list = [control_qubits]
            else:
                control_qubits_list = control_qubits
            
            # 为被调用的kernel创建新的访问者，用于解析其函数体
            kernel_visitor = QuantumProgramVisitor()
            
            # 获取被调用kernel的函数签名参数名
            import inspect
            sig = inspect.signature(kernel.original_func)
            params = list(sig.parameters.keys())
            
            # 检查目标qubit数量是否与kernel参数数量匹配
            if len(target_qubits) != len(params):
                raise ValueError(f"Kernel expects {len(params)} qubits, but {len(target_qubits)} were provided")
            
            # 将目标qubits映射到kernel的参数变量
            for i, param in enumerate(params):
                kernel_visitor.variables[param] = target_qubits[i]
            
            # 解析kernel函数体，生成电路操作
            kernel_visitor.visit(kernel.ast_tree.body[0])
            
            # 内联kernel的操作到当前电路，并添加控制位
            for op in kernel_visitor.circuit.operations:
                # 创建新的量子操作，添加控制位
                controlled_op = QuantumOperation(
                    op.gate_name,
                    op.qubits,
                    control_qubits_list,
                    op.adjoint
                )
                self.visitor.circuit.add_operation(controlled_op)
            
            return None
        else:
            # 检查是否是量子kernel调用
            # 首先检查是否是变量，然后检查是否是全局函数
            called_func = None
            
            # 检查是否是变量中的量子kernel
            if func_name in self.visitor.variables:
                called_func = self.visitor.variables[func_name]
            # 检查是否是全局函数中的量子kernel
            elif func_name in self.visitor.global_env:
                called_func = self.visitor.global_env[func_name]
            
            if isinstance(called_func, QuantumProgram):
                # 处理量子kernel调用，内联其操作
                return self._handle_kernel_call(called_func, evaluated_args)
            
            # 检查是否是内置函数，如 len, range, enumerate 等
            if func_name in ['len', 'range', 'enumerate']:
                # 这些是经典内置函数，不需要生成量子门，直接返回evaluator评估的结果
                return self.visitor.evaluator.evaluate(node)
            
            # 处理所有量子门调用，包括h, x, z等
            return self._handle_gate_call(func_name, evaluated_args)

    def _get_gate_name(self, node: ast.AST) -> str:
        """从AST节点中提取门名"""
        if isinstance(node, ast.Name):
            # 直接函数名，如h, x, z等
            return node.id
        elif isinstance(node, ast.Attribute):
            # 属性访问，如x.adj, x.ctrl, qf.x等
            return self._get_gate_name(node.value)
        elif isinstance(node, ast.Call):
            # 调用表达式，如x.ctrl(q0), x.adj()等
            return self._get_gate_name(node.func)
        return ''

    def _handle_measure_call(
            self,
            evaluated_args: List[Any],
            return_value: bool = False,
            var_name: Optional[str] = None,
            measure_type: str = 'z') -> Any:
        """处理测量调用（mz、mx或my）"""
        return self.visitor.measurement_handler.handle_measure_call(
            evaluated_args, return_value, var_name, measure_type)

    def _handle_mz_call(
            self,
            evaluated_args: List[Any],
            return_value: bool = False,
            var_name: Optional[str] = None) -> Any:
        """处理mz（测量）调用"""
        return self._handle_measure_call(evaluated_args, return_value, var_name, 'z')

    def _handle_qvector_call(
            self,
            evaluated_args: List[Any],
            var_name: Optional[str] = None,
            return_value: bool = False) -> Any:
        """处理qvector（量子向量创建）调用"""
        size: int = int(evaluated_args[0])
        qvector: QVector = QVector(size, len(self.visitor.circuit.qubits))
        if var_name:
            self.visitor.variables[var_name] = qvector
        for qubit in qvector.qubits:
            self.visitor.circuit.add_qubit(qubit)
        if return_value:
            return qvector
        return None

    def _handle_qubit_call(
            self,
            var_name: Optional[str] = None,
            return_value: bool = False) -> Any:
        """处理qubit（单个量子比特创建）调用"""
        qubit: Qubit = Qubit(len(self.visitor.circuit.qubits))
        if var_name:
            self.visitor.variables[var_name] = qubit
        self.visitor.circuit.add_qubit(qubit)
        if return_value:
            return qubit
        return None

    def _handle_kernel_call(self, program: QuantumProgram, evaluated_args: List[Any]) -> None:
        """处理量子kernel调用，内联其操作"""
        # 为被调用的kernel创建新的访问者，用于解析其函数体
        kernel_visitor = QuantumProgramVisitor()
        
        # 将父级visitor的global_env传递给新的kernel_visitor，确保可以访问第三方库
        kernel_visitor.global_env = self.visitor.global_env
        # 更新kernel_visitor的evaluator的global_env
        kernel_visitor.evaluator.global_env = self.visitor.global_env
        
        # 获取被调用kernel的函数签名参数名
        import inspect
        sig = inspect.signature(program.original_func)
        params = list(sig.parameters.keys())
        
        # 将当前调用的参数映射到kernel的参数变量
        for i, arg in enumerate(evaluated_args):
            if i < len(params):
                kernel_visitor.variables[params[i]] = arg
        
        # 解析kernel函数体，生成电路操作
        kernel_visitor.visit(program.ast_tree.body[0])
        
        # 内联kernel的操作到当前电路
        for op in kernel_visitor.circuit.operations:
            # 直接添加操作到当前电路
            self.visitor.circuit.add_operation(op)
        
        return None

    def _handle_gate_call(
            self,
            gate_name: str,
            evaluated_args: List[Any]) -> None:
        """处理量子门调用"""
        # 提取旋转门参数
        angle, target_args = self._extract_rotation_params(gate_name, evaluated_args)
        
        # 应用量子门到每个量子比特，支持表达式索引：qubits[0 + 1]
        target_qubits = self.visitor.qubit_handler.get_target_qubits(
            target_args)
        self.visitor.qubit_handler.create_quantum_operation(
            gate_name, target_qubits, angle=angle)
        return None

    def _handle_chain_call(self,
                           node: ast.Call,
                           return_value: bool = False,
                           evaluated_args: Optional[List[Any]] = None) -> None:
        """处理链式调用，如 x.adj()(q0) 或 x.ctrl(q0)(q1) 或 x.ctrl([q0])([q1]) 或 x.ctrl(q0).adj()(q1)"""
        # 使用传递的评估后参数，避免重复评估
        if evaluated_args is None:
            evaluated_args_list: List[Any] = [
                self.visitor.evaluator.evaluate(arg) for arg in node.args]
        else:
            evaluated_args_list = evaluated_args

        # 解析链式结构
        current = node.func
        adjoint = False
        controls = []
        gate_name = None
        
        # 沿着函数调用链往上找
        while True:
            if isinstance(current, ast.Call):
                # 如果是Call节点，检查它的func
                if isinstance(current.func, ast.Attribute):
                    attr_name = current.func.attr
                    if attr_name == QuantumGateAttribute.CTRL.value:
                        # 提取控制位
                        ctrl_args = [self.visitor.evaluator.evaluate(arg) for arg in current.args]
                        controls = _get_qubits_from_args(ctrl_args)
                        # 继续往上找
                        current = current.func.value
                    elif attr_name == QuantumGateAttribute.ADJ.value:
                        adjoint = True
                        # 继续往上找
                        current = current.func.value
                    else:
                        # 未知属性，停止
                        gate_name = self._get_gate_name(current.func.value)
                        break
                else:
                    # 不是Attribute，停止
                    gate_name = self._get_gate_name(current)
                    break
            elif isinstance(current, ast.Attribute):
                # 是Attribute，直接获取门名
                gate_name = self._get_gate_name(current.value)
                break
            elif isinstance(current, ast.Name):
                # 是Name，直接获取门名
                gate_name = current.id
                break
            else:
                # 其他情况，用_get_gate_name
                gate_name = self._get_gate_name(current)
                break
        
        # 提取旋转门参数
        angle, target_args = self._extract_rotation_params(gate_name, evaluated_args_list)
        
        target_qubits: List[Qubit] = self.visitor.qubit_handler.get_target_qubits(
            target_args)

        # 创建量子操作
        self.visitor.qubit_handler.create_quantum_operation(
            gate_name, target_qubits, controls, adjoint, angle)
        return None

    def _extract_rotation_params(self, gate_name: str, evaluated_args: List[Any]) -> Tuple[Optional[float], List[Any]]:
        """提取旋转门的角度参数，统一处理旋转门和普通门"""
        angle = None
        target_args = evaluated_args.copy()
        
        # 检查是否为旋转门（rx, ry, rz, r1）
        is_rotation_gate = gate_name in [QuantumFunction.RX.value, QuantumFunction.RY.value, QuantumFunction.RZ.value, QuantumFunction.R1.value]
        
        # 对于旋转门，检查是否有角度参数
        if is_rotation_gate and len(evaluated_args) > 0:
            # 检查最后一个参数是否为角度
            if isinstance(evaluated_args[-1], (int, float)):
                # 旋转门：最后一个参数是角度，其余是目标量子比特
                angle = evaluated_args[-1]
                target_args = evaluated_args[:-1]
            # 否则尝试第一个参数是否为角度（兼容旧格式）
            elif isinstance(evaluated_args[0], (int, float)):
                angle = evaluated_args[0]
                target_args = evaluated_args[1:]
        
        return angle, target_args

    def _handle_attribute_call(self,
                               node: ast.Call,
                               var_name: Optional[str] = None,
                               return_value: bool = False,
                               evaluated_args: Optional[List[Any]] = None) -> Any:
        """处理属性调用，如 x.adj(q0), x.ctrl(q0, q1) 等"""
        gate_name: str = self._get_gate_name(node.func.value)
        attr_name: str = node.func.attr

        # 使用传递的评估后参数，避免重复评估
        if evaluated_args is None:
            evaluated_args: List[Any] = [
                self.visitor.evaluator.evaluate(arg) for arg in node.args]

        # 提取旋转门参数
        angle, target_args = self._extract_rotation_params(gate_name, evaluated_args)
        
        if attr_name == QuantumGateAttribute.CTRL.value:
            # 处理直接属性调用，如 x.ctrl(q0, q1) 或 x.ctrl(q0, sub_qubits)
            # 或旋转门的受控调用，如 r1.ctrl(angle, q0, q1)
            if len(target_args) >= 1:
                # 最后一个参数是目标量子比特或量子向量
                final_target_args: List[Any] = [target_args[-1]]
                # 前面的参数是控制量子比特
                control_args: List[Any] = target_args[:-1]

                # 获取目标量子比特列表
                target_qubits: List[Qubit] = self.visitor.qubit_handler.get_target_qubits(
                    final_target_args)
                # 获取控制量子比特列表
                valid_controls: List[Qubit] = _get_qubits_from_args(
                    control_args)

                # 为每个目标量子比特创建量子操作
                self.visitor.qubit_handler.create_quantum_operation(
                    gate_name, target_qubits, valid_controls, False, angle)
        elif attr_name == QuantumGateAttribute.ADJ.value:
            # 处理直接属性调用，如 x.adj(q0) 或 z.adj(sub_qubits)
            target_qubits: List[Qubit] = self.visitor.qubit_handler.get_target_qubits(
                target_args)
            self.visitor.qubit_handler.create_quantum_operation(
                gate_name, target_qubits, adjoint=True, angle=angle)

        return None


class AssignmentHandler:
    """赋值处理类，负责处理量子框架中的赋值操作"""

    def __init__(self, visitor):
        self.visitor = visitor

    def handle_assign(self, node: ast.Assign) -> None:
        """处理变量赋值，支持更复杂的表达式，包括元组赋值和条件表达式"""
        # 先处理函数调用情况
        if isinstance(node.value, ast.Call):
            # 对于Call节点，先保存变量名，然后调用visit_Call处理
            var_names = []
            for target in node.targets:
                if isinstance(target, ast.Name):
                    var_names.append(target.id)
                # 对于元组/列表目标，暂不处理，因为函数调用通常返回单个值

            # 如果有变量名，保存第一个到节点供visit_Call使用
            if var_names:
                setattr(node.value, '_var_name', var_names[0])

            # 调用visit_Call处理函数调用，传递return_value=True，确保返回函数结果
            result = self.visitor.call_handler._dispatch_call(
                node.value, var_name=var_names[0] if var_names else None, return_value=True)

            # 保存变量值
            for var_name in var_names:
                self._save_variable(var_name, result)
            return

        # 非函数调用情况，先评估右侧表达式
        value = self.visitor.evaluator.evaluate(node.value)

        # 处理赋值目标
        for target in node.targets:
            if isinstance(target, ast.Name):
                # 单个变量赋值：a = b
                var_name: str = target.id
                self._handle_single_var_assign(var_name, node, value)
            elif isinstance(target, (ast.Tuple, ast.List)):
                # 序列赋值：a, b = c, d 或 [a, b] = [c, d]
                self._handle_sequence_assign(target, value)

    def _handle_single_var_assign(
            self,
            var_name: str,
            node: ast.Assign,
            value: Any) -> None:
        """处理单个变量赋值"""
        if _is_measure_call(node.value):
            # 处理测量结果赋值（mz、mx或my）
            self._handle_mz_result_assign(var_name, node.value)
        elif isinstance(node.value, ast.IfExp):
            # 处理条件表达式赋值：target_qubit = qubits[1] if condition else qubits[2]
            self._handle_conditional_assign(var_name, node.value, value)
        elif isinstance(node.value, ast.BoolOp):
            # 处理布尔操作赋值：combined = m1 or m2 或 combined = m1 and m2
            self._handle_bool_op_assign(var_name, node.value)
        elif isinstance(node.value, ast.Call):
            # 处理函数调用赋值，如 qubits = qvector(6)
            # 直接使用传递过来的value，如果value为None，检查visitor.variables中是否已经有值
            if value is not None:
                self._save_variable(var_name, value)
            else:
                # 检查visitor.variables中是否已经有该变量的值（可能由visit_Call直接设置）
                if var_name not in self.visitor.variables:
                    self._save_variable(var_name, value)
        else:
            # 非测量结果赋值，正常保存变量值
            self._save_variable(var_name, value)

    def _handle_mz_result_assign(
            self,
            var_name: str,
            mz_call: ast.Call) -> None:
        """处理测量结果赋值：result = mz(q1) 或 result = mx(q1) 或 result = my(q1)"""
        # 获取测量类型
        measure_type = _get_measure_type(mz_call)
        # 获取测量的量子比特或量子向量
        qubit_arg = mz_call.args[0]
        qubit_or_qvector: Any = self.visitor.evaluator.evaluate(qubit_arg)

        if isinstance(qubit_or_qvector, Qubit):
            # 测量单个量子比特：result = mz(q1) 或 result = mx(q1) 或 result = my(q1)
            qubit: Qubit = qubit_or_qvector
            # 为单个量子比特添加测量操作
            measurement: Measurement = Measurement(qubit, measure_type=measure_type)
            self.visitor.circuit.add_measurement(measurement)
            # 直接使用量子比特索引作为测量寄存器编号
            measurement_reg: int = qubit.index
            # 将测量结果分配到经典寄存器
            self.visitor.measurement_handler.add_measure_to_classical(
                measurement_reg, var_name)
        elif isinstance(qubit_or_qvector, QVector):
            # 测量量子向量：result = mz(sub_qubits) 或 result = mx(sub_qubits) 或 result = my(sub_qubits)
            qvector: QVector = qubit_or_qvector
            # 为每个量子比特添加测量操作
            for qubit in qvector.qubits:
                measurement: Measurement = Measurement(qubit, measure_type=measure_type)
                self.visitor.circuit.add_measurement(measurement)
                # 为每个量子比特分配经典寄存器
                self.visitor.measurement_handler.add_measure_to_classical(
                    qubit.index, var_name)
        # 保存变量值
        self._save_variable(var_name, None)

    def _handle_conditional_assign(
            self,
            var_name: str,
            if_exp: ast.IfExp,
            value: Any) -> None:
        """处理条件表达式赋值：target_qubit = qubits[1] if condition else qubits[2]"""
        # 提取条件表达式的各个部分
        condition_expr = if_exp.test
        then_expr = if_exp.body
        else_expr = if_exp.orelse

        # 处理条件，获取经典寄存器
        cond_classical_reg = self._get_condition_reg(condition_expr)

        if cond_classical_reg is not None:
            # 直接获取then和else分支的量子比特，不触发完整表达式评估
            then_qubit = None
            else_qubit = None

            # 特殊处理Subscript节点，直接获取量子比特而不生成操作
            if isinstance(then_expr, ast.Subscript):
                then_qubit = self.visitor.qubit_handler.handle_subscript(
                    then_expr)
            elif isinstance(then_expr, ast.Name):
                then_qubit = self.visitor.evaluator.evaluate(then_expr)

            if isinstance(else_expr, ast.Subscript):
                else_qubit = self.visitor.qubit_handler.handle_subscript(
                    else_expr)
            elif isinstance(else_expr, ast.Name):
                else_qubit = self.visitor.evaluator.evaluate(else_expr)

            if isinstance(then_qubit, Qubit) and isinstance(else_qubit, Qubit):
                # 处理量子比特作为结果的情况
                self._create_conditional_qubit_assign(
                    var_name, cond_classical_reg, then_qubit, else_qubit)
                return

        # 如果条件无法处理，直接使用传递过来的value保存变量
        self._save_variable(var_name, value)

    def _get_condition_reg(self, condition_expr: ast.AST) -> Optional[int]:
        """统一从条件表达式中获取经典寄存器，支持所有条件类型"""
        if isinstance(condition_expr, ast.Name):
            # 处理变量条件：if combined: 或 target_qubit = qubits[1] if condition else
            # qubits[2]
            var_name = condition_expr.id
            if var_name in self.visitor.var_to_classical_reg:
                return self.visitor.var_to_classical_reg[var_name]
        elif _is_mz_call(condition_expr):
            # 处理测量条件：if mz(q0) 或 target_qubit = qubits[1] if mz(q0) else
            # qubits[2]
            qubit_arg = condition_expr.args[0]
            qubit_or_qvector = self.visitor.evaluator.evaluate(qubit_arg)

            if isinstance(qubit_or_qvector, Qubit):
                qubit = qubit_or_qvector
                measurement = Measurement(qubit)
                self.visitor.circuit.add_measurement(measurement)
                return self.visitor.measurement_handler.add_measure_to_classical(
                    qubit.index)
        elif isinstance(condition_expr, ast.Compare):
            # 处理比较条件：if result == 1: 或 target_qubit = qubits[1] if x > 5 else
            # qubits[2]
            return _extract_reg_from_compare(condition_expr, self.visitor)
        elif isinstance(condition_expr, ast.BoolOp):
            # 处理布尔操作条件：if (a and b) 或 target_qubit = qubits[1] if (a and b)
            # else qubits[2]
            return self.visitor.bool_op_handler.handle_bool_op(condition_expr)
        return None

    def _create_conditional_qubit_assign(
            self,
            var_name: str,
            cond_reg: int,
            then_qubit: Qubit,
            else_qubit: Qubit) -> None:
        """创建条件量子比特赋值操作"""
        then_index = then_qubit.index
        else_index = else_qubit.index

        # 创建一个经典寄存器来存储量子比特索引
        index_reg = self.visitor.next_classical_reg
        self.visitor.next_classical_reg += 1

        # 保存变量信息
        var_info = {
            'type': 'var_qubit',
            'index_reg': index_reg,
            'then_index': then_index,
            'else_index': else_index,
            'cond_reg': cond_reg
        }
        self._save_variable(var_name, var_info)

        # 生成条件分支，给index_reg赋值
        cond = Condition(cond_reg, ComparisonOperator.EQ, 1)

        # 添加常量赋值操作
        cond.then_operations.append({
            'type': 'constant_assign',
            'reg': index_reg,
            'value': then_index
        })
        cond.else_operations.append({
            'type': 'constant_assign',
            'reg': index_reg,
            'value': else_index
        })

        # 添加条件操作到电路
        self.visitor.circuit.add_operation(cond)

    def _handle_bool_op_assign(
            self,
            var_name: str,
            bool_op: ast.BoolOp) -> None:
        """处理布尔操作赋值：combined = m1 or m2 或 combined = m1 and m2"""
        result_reg = self.visitor.bool_op_handler.handle_bool_op(bool_op)
        if result_reg is not None:
            # 将结果寄存器与变量名关联
            self.visitor.var_to_classical_reg[var_name] = result_reg
            self._save_variable(var_name, None)

    def _handle_sequence_assign(
            self, target: Union[ast.Tuple, ast.List], value: Any) -> None:
        """处理序列赋值：a, b = c, d 或 [a, b] = [c, d] 或 a, b = (c, d)"""
        # 处理量子向量解包，如 q0, q1, q2 = qubits[0], qubits[1], qubits[2] 或 q0, q1, q2
        # = qubits
        if isinstance(value, QVector):
            # 直接从量子向量中获取量子比特，支持解包赋值
            qubits = value.qubits

            # 为每个目标变量分配对应的量子比特
            for i, elt in enumerate(target.elts):
                if isinstance(elt, ast.Name) and i < len(qubits):
                    var_name = elt.id
                    self._save_variable(var_name, qubits[i])
        elif isinstance(value, (tuple, list)):
            # 右侧是序列，逐个赋值
            for i, elt in enumerate(target.elts):
                if isinstance(elt, ast.Name) and i < len(value):
                    var_name = elt.id
                    self._save_variable(var_name, value[i])

    def _save_variable(self, var_name: str, value: Any):
        """保存变量值，处理特殊情况"""
        # 保存变量值
        self.visitor.variables[var_name] = value


class QuantumProgramVisitor(ast.NodeVisitor):
    """量子程序访问者类，用于解析量子程序的AST"""
    
    def __init__(self):
        self.circuit: QuantumCircuit = QuantumCircuit()
        self.variables: Dict[str, Any] = {}
        self.global_env: Dict[str, Any] = {}  # 保存全局作用域，用于查找被调用的量子kernel

        self.var_to_classical_reg: Dict[str, int] = {}  # 变量名到经典寄存器的映射
        self.next_classical_reg: int = 0  # 下一个可用的经典寄存器编号
        self.measurement_to_classical: Dict[int, int] = {}  # 测量寄存器到经典寄存器的映射

        # 创建表达式求值器实例
        self.evaluator = ExpressionEvaluator(self.variables, self.global_env)

        # 创建外部处理类实例
        self.assignment_handler = AssignmentHandler(self)
        self.condition_handler = ConditionHandler(self)
        self.call_handler = CallHandler(self)
        self.loop_handler = LoopHandler(self)
        self.bool_op_handler = BoolOpHandler(self)
        self.qubit_handler = QubitHandler(self)
        self.measurement_handler = MeasurementHandler(self)

    def visit_FunctionDef(self, node):
        """处理函数定义，遍历函数体"""
        for stmt in node.body:
            self.visit(stmt)

    def visit_Expr(self, node):
        """处理表达式语句，如直接的函数调用 h(q0)"""
        # 如果表达式是函数调用，直接调用visit_Call处理
        if isinstance(node.value, ast.Call):
            self.visit_Call(node.value)

    def visit_Assign(self, node):
        """处理变量赋值，支持更复杂的表达式，包括元组赋值和条件表达式"""
        self.assignment_handler.handle_assign(node)

    def visit_Call(self, node):
        """处理函数调用，如 h(q0), mz(q0) 等"""
        return self.call_handler.handle_call(node)

    def visit_If(self, node: ast.If) -> None:
        """处理条件语句，支持更复杂的条件表达式

        支持的条件类型：
        1. 直接测量条件：if mz(q0) == 1:
        2. 变量条件：if result == 1:
        3. 布尔操作条件：if result1 and result2:
        4. 简单变量条件：if combined:
        5. 算术表达式条件：if i % 2 == 0:

        Args:
            node: if语句的AST节点
        """
        self.condition_handler.handle_if(node)

    def visit_For(self, node):
        """处理for循环语句"""
        self.loop_handler.handle_for(node)

    def visit_While(self, node: ast.While) -> None:
        """处理while循环语句"""
        self.loop_handler.handle_while(node)


class BackendTarget(enum.Enum):
    """后端目标枚举，支持多种加速器类型"""
    DEFAULT_CPU_SV = 'default-cpu-sv'
    NVIDIA_GPU_SV = 'nvidia-gpu-sv'
    AMD_GPU_SV = 'amd-gpu-sv'
    BIREN_GPU_SV = 'biren-gpu-sv'
    METAX_GPU_SV = 'metax-gpu-sv'
    MOORE_THREADS_GPU_SV = 'moore-threads-gpu-sv'
    ILUVATAR_GPU_SV = 'iluvatar-gpu-sv'
    APPLE_GPU_SV = 'apple-gpu-sv'
    OPENCL_SV = 'opencl-sv'

    @classmethod
    def is_valid(cls, target: str) -> bool:
        """检查目标是否有效"""
        return target in [member.value for member in cls]

    @classmethod
    def get_supported_targets(cls) -> List[str]:
        """获取所有支持的目标列表"""
        return [member.value for member in cls]


# 全局配置变量
_current_target = BackendTarget.DEFAULT_CPU_SV.value  # 后端目标设置


def set_target(target: str):
    """设置后端目标"""
    global _current_target
    if not BackendTarget.is_valid(target):
        supported = BackendTarget.get_supported_targets()
        warnings.warn(f"Warning: Unknown backend target '{target}'. Using default 'default-cpu-sv'. Supported targets: {', '.join(supported)}")
        _current_target = BackendTarget.DEFAULT_CPU_SV.value
    else:
        _current_target = target


def get_target() -> str:
    """获取当前后端目标"""
    global _current_target
    return _current_target


def list_targets() -> List[str]:
    """列出所有支持的后端目标"""
    return BackendTarget.get_supported_targets()


class BackendManager:
    """后端管理器，负责管理不同后端的模拟器实例"""
    
    _instance = None
    
    def __new__(cls):
        """单例模式"""
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._simulators = {}
            cls._instance._default_simulator = None
        return cls._instance
    
    def __init__(self):
        """初始化后端管理器"""
        if not hasattr(self, '_simulators'):
            self._simulators = {}
            self._default_simulator = None
    
    def get_simulator(self, target: Optional[str] = None):
        """获取指定目标的模拟器实例"""
        if target is None:
            target = get_target()
        
        # 如果是 default-cpu-sv 或其他 CPU 后端，使用 SSA 模拟器
        if target in [BackendTarget.DEFAULT_CPU_SV.value]:
            if self._default_simulator is None:
                import ssa_simulator_cpp_default_cpu_sv
                self._default_simulator = ssa_simulator_cpp_default_cpu_sv.DefaultCPUSVSSASimulator()
            return self._default_simulator
        
        # biren-gpu-sv 后端
        if target == BackendTarget.BIREN_GPU_SV.value:
            if target not in self._simulators:
                print(f"[BackendManager] Initializing Biren GPU SV backend")
                try:
                    # 直接尝试使用GPU后端，不导入CPU模块
                    import importlib
                    # 动态导入GPU模块
                    gpu_module = importlib.import_module('ssa_simulator_cpp_biren_gpu_sv')
                    simulator = gpu_module.BirenGPUSVSSASimulator()
                    
                    # 测试模拟器是否能正常工作
                    test_ssa = ';; Test program\ndeclare qreg q0\ndeclare mreg m0\nqgate.h q0\nmeasure.z q0, m0'
                    if not simulator.load_ssa_assembly(test_ssa):
                        raise Exception(f"Backend initialization failed: {simulator.get_error()}")
                    
                    self._simulators[target] = simulator
                    print(f"[BackendManager] Biren GPU SV backend initialized successfully")
                except Exception as e:
                    warnings.warn(f"Failed to initialize Biren GPU SV backend: {e}. Falling back to 'default-cpu-sv'.")
                    # 只有在失败时才导入CPU模块
                    if self._default_simulator is None:
                        import ssa_simulator_cpp_default_cpu_sv
                        self._default_simulator = ssa_simulator_cpp_default_cpu_sv.DefaultCPUSVSSASimulator()
                    self._simulators[target] = self._default_simulator
            return self._simulators[target]
        
        # moore-threads-gpu-sv 后端
        if target == BackendTarget.MOORE_THREADS_GPU_SV.value:
            if target not in self._simulators:
                print(f"[BackendManager] Initializing Moore Threads GPU SV backend")
                try:
                    # 直接尝试使用GPU后端，不导入CPU模块
                    import importlib
                    # 动态导入GPU模块
                    gpu_module = importlib.import_module('ssa_simulator_cpp_moorethread_gpu_sv')
                    simulator = gpu_module.MooreThreadGPUSVSSASimulator()
                    
                    # 测试模拟器是否能正常工作
                    test_ssa = ';; Test program\ndeclare qreg q0\ndeclare mreg m0\nqgate.h q0\nmeasure.z q0, m0'
                    if not simulator.load_ssa_assembly(test_ssa):
                        raise Exception(f"Backend initialization failed: {simulator.get_error()}")
                    
                    self._simulators[target] = simulator
                    print(f"[BackendManager] Moore Threads GPU SV backend initialized successfully")
                except Exception as e:
                    warnings.warn(f"Failed to initialize Moore Threads GPU SV backend: {e}. Falling back to 'default-cpu-sv'.")
                    # 只有在失败时才导入CPU模块
                    if self._default_simulator is None:
                        import ssa_simulator_cpp_default_cpu_sv
                        self._default_simulator = ssa_simulator_cpp_default_cpu_sv.DefaultCPUSVSSASimulator()
                    self._simulators[target] = self._default_simulator
            return self._simulators[target]
        
        # 对于其他后端，预留接口，目前暂时返回默认模拟器
        if target not in self._simulators:
            warnings.warn(f"Backend '{target}' is not implemented yet. Falling back to 'default-cpu-sv'.")
            if self._default_simulator is None:
                import ssa_simulator_cpp_default_cpu_sv
                self._default_simulator = ssa_simulator_cpp_default_cpu_sv.DefaultCPUSVSSASimulator()
            self._simulators[target] = self._default_simulator
        
        return self._simulators[target]
    
    def register_simulator(self, target: str, simulator_class):
        """注册自定义模拟器"""
        self._simulators[target] = simulator_class()


# 全局后端管理器实例
_backend_manager = BackendManager()


def quantum_kernel(func: Callable) -> QuantumProgram:
    """量子kernel装饰器，用于解析量子程序"""
    import textwrap

    # 获取函数源代码并移除缩进
    source: str = inspect.getsource(func)
    dedented_source: str = textwrap.dedent(source)

    # 解析AST
    tree: ast.Module = ast.parse(dedented_source)

    # 创建量子程序对象，暂不解析函数体
    program: QuantumProgram = QuantumProgram(None, source, func)
    # 保存AST以便后续解析
    program.ast_tree = tree
    return program


def sample(program: QuantumProgram, *args, shots_count: int = 1000, **kwargs) -> Dict[str, int]:
    """运行量子程序并返回测量结果"""
    
    target = kwargs.get('target', get_target())

    # 获取模拟器实例
    simulator = _backend_manager.get_simulator(target)
    
    # 生成SSA汇编代码，传递args作为prog的参数
    ssa_assembly = program.generate_ssa_assembly(*args, **kwargs)
    
    # 加载SSA汇编代码到模拟器
    load_result = simulator.load_ssa_assembly(ssa_assembly)
    if not load_result:
        raise RuntimeError(f"Failed to load SSA assembly: {simulator.get_error()}")
    
    # 调用sample方法执行模拟
    results = simulator.sample(shots_count)
    
    return results


# 测量函数

def mx(qubit: Qubit) -> Measurement:
    """测量函数，创建一个x轴测量操作"""
    return Measurement(qubit, measure_type='x')

def my(qubit: Qubit) -> Measurement:
    """测量函数，创建一个y轴测量操作"""
    return Measurement(qubit, measure_type='y')

def mz(qubit: Qubit) -> Measurement:
    """测量函数，创建一个z轴测量操作"""
    return Measurement(qubit, measure_type='z')

# 创建控制函数
def control(kernel, control_qubits, target_qubit):
    """创建受控量子操作"""
    pass


# 类别名 - 小写形式，方便导入和使用
qubit = Qubit     # Qubit类的小写别名
qvector = QVector # QVector类的小写别名

# 量子门对象 - 预定义的常用量子门
h = QuantumGate(QuantumFunction.H.value)   # Hadamard门
x = QuantumGate(QuantumFunction.X.value)   # Pauli-X门
y = QuantumGate(QuantumFunction.Y.value)   # Pauli-Y门
z = QuantumGate(QuantumFunction.Z.value)   # Pauli-Z门
t = QuantumGate(QuantumFunction.T.value)   # T门
s = QuantumGate(QuantumFunction.S.value)   # S门
rx = QuantumGate(QuantumFunction.RX.value) # 绕X轴旋转门
ry = QuantumGate(QuantumFunction.RY.value) # 绕Y轴旋转门
rz = QuantumGate(QuantumFunction.RZ.value) # 绕Z轴旋转门
r1 = QuantumGate(QuantumFunction.R1.value) # 绕Z轴旋转门（相位门，角度为λ/2）
swap = QuantumGate(QuantumFunction.SWAP.value) # SWAP门


class SSAAssembler:
    """SSA汇编生成器类，用于生成SSA低级汇编代码"""

    def __init__(self):
        self.assembly = []
        self.label_counter = 0
        self.qubit_map = {}
        self.classical_regs = set()
        self.measure_regs = set()

    def generate_label(self):
        """生成唯一标签"""
        label = f"label_{self.label_counter}"
        self.label_counter += 1
        return label

    def add_line(self, line):
        """添加一行汇编代码"""
        self.assembly.append(line)

    def add_comment(self, comment):
        """添加注释"""
        self.assembly.append(f";; {comment}")

    def get_qubit_reg(self, qubit):
        """获取量子比特对应的寄存器名，使用量子比特的原始索引确保顺序正确"""
        if qubit not in self.qubit_map:
            # 使用量子比特的index属性作为寄存器名，确保顺序正确
            self.qubit_map[qubit] = f"q{qubit.index}"
        return self.qubit_map[qubit]

    def generate(self, program, *args, **kwargs):
        """生成完整的SSA汇编"""
        # 重置状态，确保每次生成都是全新的
        self.assembly = []
        self.label_counter = 0
        self.qubit_map = {}
        self.classical_regs = set()
        self.measure_regs = set()

        # 第一次遍历：收集所有需要的寄存器信息
        for op in program.circuit.operations:
            if isinstance(op, QuantumOperation):
                pass  # 量子操作不直接影响寄存器声明
            elif isinstance(op, Measurement):
                self._generate_measurement(op)  # 收集测量寄存器
            elif isinstance(op, MeasureToClassicalOperation):
                self._generate_assign(op)  # 收集经典寄存器
            elif isinstance(op, AllOperation):
                # 收集all操作的寄存器
                for reg in op.input_regs:
                    self.classical_regs.add(f"c{reg}")
                self.classical_regs.add(f"c{op.output_reg}")
            elif isinstance(op, AndOperation):
                # 收集and操作的寄存器
                self.classical_regs.add(f"c{op.left_reg}")
                self.classical_regs.add(f"c{op.right_reg}")
                self.classical_regs.add(f"c{op.output_reg}")
            elif isinstance(op, OrOperation):
                # 收集or操作的寄存器
                self.classical_regs.add(f"c{op.left_reg}")
                self.classical_regs.add(f"c{op.right_reg}")
                self.classical_regs.add(f"c{op.output_reg}")
            elif isinstance(op, XorOperation):
                # 收集xor操作的寄存器
                self.classical_regs.add(f"c{op.left_reg}")
                self.classical_regs.add(f"c{op.right_reg}")
                self.classical_regs.add(f"c{op.output_reg}")
            elif isinstance(op, Condition):
                # 递归收集条件分支中的寄存器
                self._collect_registers_from_condition(op)
            elif isinstance(op, VarQubitOperation):
                # 收集动态量子比特操作的寄存器
                self.classical_regs.add(f"c{op.index_reg}")
            elif isinstance(op, dict) and op.get('type') == 'constant_assign':
                # 收集常量赋值操作的寄存器
                self.classical_regs.add(f"c{op['reg']}")

        # 为所有量子比特声明测量寄存器，确保测量寄存器数量正确
        for i in range(len(program.circuit.qubits)):
            meas_reg = f"m{i}"
            self.measure_regs.add(meas_reg)

        # 重置assembly，准备生成最终汇编
        self.assembly = []

        self.add_comment(f"Program: {program.original_func.__name__}")
        self.add_comment("=" * 50)
        self.add_line("")

        self.add_comment("Register Declarations")
        # 声明量子寄存器
        for qubit in program.circuit.qubits:
            qreg = self.get_qubit_reg(qubit)
            self.add_line(f"declare qreg {qreg}")

        # 声明经典寄存器
        for classical_reg in sorted(self.classical_regs):
            self.add_line(f"declare creg {classical_reg}")

        # 声明测量寄存器
        for meas_reg in sorted(self.measure_regs):
            self.add_line(f"declare mreg {meas_reg}")

        self.add_line("")
        self.add_comment("Circuit Operations")

        # 第二次遍历：生成实际的电路操作
        for i, op in enumerate(program.circuit.operations):
            self.add_comment(f"Step {i+1}: {op}")

            if isinstance(op, QuantumOperation):
                self._generate_quantum_operation(op)
            elif isinstance(op, Measurement):
                self._generate_measurement(op)
            elif isinstance(op, MeasureToClassicalOperation):
                self._generate_assign(op)
            elif isinstance(op, AllOperation):
                self._generate_all_operation(op)
            elif isinstance(op, AndOperation):
                self._generate_and_operation(op)
            elif isinstance(op, OrOperation):
                self._generate_or_operation(op)
            elif isinstance(op, XorOperation):
                self._generate_xor_operation(op)
            elif isinstance(op, Condition):
                self._generate_condition(op)
            elif isinstance(op, VarQubitOperation):
                self._generate_var_qubit(op)
            elif isinstance(op, dict) and op.get('type') == 'constant_assign':
                # 生成常量赋值指令：根据数值类型生成不同指令
                reg = f"c{op['reg']}"
                value = op['value']
                self.classical_regs.add(reg)
                
                # 根据数值类型生成不同的赋值指令
                if isinstance(value, int):
                    self.add_line(f"mov.int32 {reg}, {value}")
                elif isinstance(value, float):
                    self.add_line(f"mov.float32 {reg}, {value}")
                else:
                    self.add_line(f"mov {reg}, {value}")
            elif isinstance(op, dict) and op.get('type') == 'dynamic_qubit_op':
                # 生成使用动态量子比特的量子门操作
                # 简化格式：qgate.<gate_name> dynamic=<index_reg>
                gate_name = op['gate_name']
                index_reg = f"c{op['index_reg']}"
                self.classical_regs.add(index_reg)
                self.add_line(f"qgate.{gate_name} dynamic={index_reg}")

        self.add_line("")
        self.add_comment("End of SSA Assembly")

        return "\n".join(self.assembly)

    def _generate_quantum_operation(self, op):
        """生成量子门操作的汇编"""
        qubits = [self.get_qubit_reg(q) for q in op.qubits]
        qubits_str = ", ".join(qubits)

        # 检查是否有角度参数（旋转门）
        angle_str = f", angle={op.angle}" if op.angle is not None else ""

        if op.controls:
            controls = [self.get_qubit_reg(q) for q in op.controls]
            controls_str = ", ".join(controls)
            line = f"qgate.{op.gate_name} {qubits_str}{angle_str}, ctrl={controls_str}"
        else:
            line = f"qgate.{op.gate_name} {qubits_str}{angle_str}"

        if op.adjoint:
            line += ".adj"

        self.add_line(line)

    def _generate_measurement(self, op):
        """生成测量操作的汇编"""
        qubit_reg = self.get_qubit_reg(op.qubit)
        meas_reg = f"m{op.measurement_reg}"
        self.measure_regs.add(meas_reg)
        self.add_line(f"measure.{op.measure_type} {qubit_reg}, {meas_reg}")

    def _generate_assign(self, op):
        """生成赋值操作的汇编"""
        meas_reg = f"m{op.measurement_reg}"
        classical_reg = f"c{op.classical_reg}"
        self.classical_regs.add(classical_reg)
        # 汇编语言惯例：mov 目标操作数, 源操作数
        self.add_line(f"mov {classical_reg}, {meas_reg}")

    def _generate_all_operation(self, op):
        """生成all操作的汇编"""
        # all操作：检查所有输入寄存器是否都为1
        # 生成逻辑：output_reg = 1 if all(input_regs == 1) else 0

        # 声明输出寄存器
        output_reg = f"c{op.output_reg}"
        self.classical_regs.add(output_reg)

        if len(op.input_regs) == 0:
            # 空列表的all结果为True，所以输出1
            self.add_line(f"const.int32 {output_reg}, 1")
            return

        if len(op.input_regs) == 1:
            # 只有一个寄存器，直接复制值
            first_reg = f"c{op.input_regs[0]}"
            self.classical_regs.add(first_reg)
            self.add_line(f"mov {output_reg}, {first_reg}")
        else:
            # 优化：直接生成 and output_reg, reg1, reg2 形式，避免冗余的mov指令
            first_reg = f"c{op.input_regs[0]}"
            second_reg = f"c{op.input_regs[1]}"
            self.classical_regs.add(first_reg)
            self.classical_regs.add(second_reg)

            # 直接生成 and output_reg, first_reg, second_reg
            self.add_line(f"and {output_reg}, {first_reg}, {second_reg}")

            # 对于更多寄存器，依次与输出寄存器进行AND操作
            for i in range(2, len(op.input_regs)):
                next_reg = f"c{op.input_regs[i]}"
                self.classical_regs.add(next_reg)
                self.add_line(f"and {output_reg}, {output_reg}, {next_reg}")

    def _generate_and_operation(self, op):
        """生成and操作的汇编"""
        # and操作：output_reg = left_reg & right_reg
        left_reg = f"c{op.left_reg}"
        right_reg = f"c{op.right_reg}"
        output_reg = f"c{op.output_reg}"

        # 声明所有寄存器
        self.classical_regs.add(left_reg)
        self.classical_regs.add(right_reg)
        self.classical_regs.add(output_reg)

        # 生成AND操作
        self.add_line(f"and {output_reg}, {left_reg}, {right_reg}")

    def _generate_or_operation(self, op):
        """生成or操作的汇编"""
        # or操作：output_reg = left_reg | right_reg
        left_reg = f"c{op.left_reg}"
        right_reg = f"c{op.right_reg}"
        output_reg = f"c{op.output_reg}"

        # 声明所有寄存器
        self.classical_regs.add(left_reg)
        self.classical_regs.add(right_reg)
        self.classical_regs.add(output_reg)

        # 生成OR操作
        self.add_line(f"or {output_reg}, {left_reg}, {right_reg}")

    def _generate_xor_operation(self, op):
        """生成xor操作的汇编"""
        # xor操作：output_reg = left_reg ^ right_reg
        left_reg = f"c{op.left_reg}"
        right_reg = f"c{op.right_reg}"
        output_reg = f"c{op.output_reg}"

        # 声明所有寄存器
        self.classical_regs.add(left_reg)
        self.classical_regs.add(right_reg)
        self.classical_regs.add(output_reg)

        # 生成XOR操作
        self.add_line(f"xor {output_reg}, {left_reg}, {right_reg}")

    def _collect_registers_from_condition(self, op):
        """递归收集条件分支中的寄存器信息"""
        classical_reg = f"c{op.classical_reg}"
        self.classical_regs.add(classical_reg)

        # 收集then分支中的寄存器
        for then_op in op.then_operations:
            if isinstance(then_op, QuantumOperation):
                pass  # 量子操作不直接影响寄存器声明
            elif isinstance(then_op, Measurement):
                self._generate_measurement(then_op)  # 收集测量寄存器
            elif isinstance(then_op, MeasureToClassicalOperation):
                self._generate_assign(then_op)  # 收集经典寄存器
            elif isinstance(then_op, AllOperation):
                # 收集all操作的寄存器
                for reg in then_op.input_regs:
                    self.classical_regs.add(f"c{reg}")
                self.classical_regs.add(f"c{then_op.output_reg}")
            elif isinstance(then_op, AndOperation):
                # 收集and操作的寄存器
                self.classical_regs.add(f"c{then_op.left_reg}")
                self.classical_regs.add(f"c{then_op.right_reg}")
                self.classical_regs.add(f"c{then_op.output_reg}")
            elif isinstance(then_op, OrOperation):
                # 收集or操作的寄存器
                self.classical_regs.add(f"c{then_op.left_reg}")
                self.classical_regs.add(f"c{then_op.right_reg}")
                self.classical_regs.add(f"c{then_op.output_reg}")
            elif isinstance(then_op, Condition):
                self._collect_registers_from_condition(then_op)
            elif isinstance(then_op, dict) and then_op.get('type') == 'constant_assign':
                # 收集常量赋值操作的寄存器
                reg = f"c{then_op['reg']}"
                self.classical_regs.add(reg)

        # 收集else分支中的寄存器
        for else_op in op.else_operations:
            if isinstance(else_op, QuantumOperation):
                pass  # 量子操作不直接影响寄存器声明
            elif isinstance(else_op, Measurement):
                self._generate_measurement(else_op)  # 收集测量寄存器
            elif isinstance(else_op, MeasureToClassicalOperation):
                self._generate_assign(else_op)  # 收集经典寄存器
            elif isinstance(else_op, AllOperation):
                # 收集all操作的寄存器
                for reg in else_op.input_regs:
                    self.classical_regs.add(f"c{reg}")
                self.classical_regs.add(f"c{else_op.output_reg}")
            elif isinstance(else_op, AndOperation):
                # 收集and操作的寄存器
                self.classical_regs.add(f"c{else_op.left_reg}")
                self.classical_regs.add(f"c{else_op.right_reg}")
                self.classical_regs.add(f"c{else_op.output_reg}")
            elif isinstance(else_op, Condition):
                self._collect_registers_from_condition(else_op)
            elif isinstance(else_op, dict) and else_op.get('type') == 'constant_assign':
                # 收集常量赋值操作的寄存器
                reg = f"c{else_op['reg']}"
                self.classical_regs.add(reg)

    def _generate_condition(self, op):
        """生成条件分支的汇编"""
        then_label = self.generate_label()
        else_label = self.generate_label()
        end_label = self.generate_label()

        classical_reg = f"c{op.classical_reg}"
        self.classical_regs.add(classical_reg)
        
        # 根据数值类型生成不同的条件分支指令
        value = op.value
        # 将布尔值统一转换为整数
        if isinstance(value, bool):
            value = 1 if value else 0
        
        if isinstance(value, int):
            self.add_line(
                f"br.cond.int32 {classical_reg}, {op.operator.value}, {value}, {then_label}, {else_label}")
        elif isinstance(value, float):
            self.add_line(
                f"br.cond.float32 {classical_reg}, {op.operator.value}, {value}, {then_label}, {else_label}")
        else:
            self.add_line(
                f"br.cond {classical_reg}, {op.operator.value}, {value}, {then_label}, {else_label}")

        self.add_line(f"{then_label}:")
        self._generate_operations(op.then_operations)
        self.add_line(f"br {end_label}")

        self.add_line(f"{else_label}:")
        self._generate_operations(op.else_operations)
        self.add_line(f"br {end_label}")

        self.add_line(f"{end_label}:")

    def _generate_operations(self, operations):
        """生成操作列表的汇编代码"""
        for op in operations:
            if isinstance(op, QuantumOperation):
                self._generate_quantum_operation(op)
            elif isinstance(op, Measurement):
                self._generate_measurement(op)
            elif isinstance(op, MeasureToClassicalOperation):
                self._generate_assign(op)
            elif isinstance(op, AllOperation):
                self._generate_all_operation(op)
            elif isinstance(op, AndOperation):
                self._generate_and_operation(op)
            elif isinstance(op, OrOperation):
                self._generate_or_operation(op)
            elif isinstance(op, Condition):
                self._generate_condition(op)
            elif isinstance(op, VarQubitOperation):
                self._generate_var_qubit(op)
            elif isinstance(op, dict) and op.get('type') == 'constant_assign':
                # 生成常量赋值指令：根据数值类型生成不同指令
                reg = f"c{op['reg']}"
                value = op['value']
                self.classical_regs.add(reg)
                
                # 根据数值类型生成不同的赋值指令
                if isinstance(value, int):
                    self.add_line(f"mov.int32 {reg}, {value}")
                elif isinstance(value, float):
                    self.add_line(f"mov.float32 {reg}, {value}")
                else:
                    self.add_line(f"mov {reg}, {value}")
            elif isinstance(op, dict) and op.get('type') == 'dynamic_qubit_op':
                # 生成使用动态量子比特的量子门操作
                gate_name = op['gate_name']
                index_reg = f"c{op['index_reg']}"
                self.classical_regs.add(index_reg)
                self.add_line(f"qgate.{gate_name} dynamic={index_reg}")

    def _generate_var_qubit(self, op):
        """生成动态量子比特操作的汇编"""
        # 生成动态量子比特指令，格式：qreg.dynamic <var_name>, <index_reg>, <qvector_size>
        # 用于表示根据经典寄存器索引获取量子比特
        index_reg = f"c{op.index_reg}"
        self.classical_regs.add(index_reg)
        self.add_line(
            f"qreg.dynamic {op.var_name}, {index_reg}, {op.qvector_size}")


class SpinOperator:
    """自旋算符类，用于表示量子系统中的自旋算符"""
    
    def __init__(self, op_type: str, qubit: Union[Qubit, int]):
        """初始化自旋算符
        
        Args:
            op_type: 算符类型，如 'z', 'i' 等
            qubit: 量子比特或量子比特索引
        """
        self.op_type = op_type
        self.qubit = qubit
    
    def __mul__(self, other):
        """实现算符乘法"""
        if isinstance(other, SpinOperator):
            # 创建一个乘积算符
            return ProductOperator(self, other)
        return NotImplemented
    
    def __add__(self, other):
        """实现算符加法"""
        if isinstance(other, (SpinOperator, ProductOperator, Hamiltonian)):
            return Hamiltonian([(1.0, self), (1.0, other)])
        return NotImplemented
    
    def __sub__(self, other):
        """实现算符减法"""
        if isinstance(other, (SpinOperator, ProductOperator, Hamiltonian)):
            return Hamiltonian([(1.0, self), (-1.0, other)])
        return NotImplemented
    
    def __rmul__(self, scalar):
        """实现标量乘法（右侧）"""
        if isinstance(scalar, (int, float)):
            return Hamiltonian([(scalar, self)])
        return NotImplemented
    
    def __repr__(self):
        """返回算符的字符串表示"""
        qubit_idx = self.qubit.index if isinstance(self.qubit, Qubit) else self.qubit
        return f"spin.{self.op_type}({qubit_idx})"


class ProductOperator:
    """乘积算符类，用于表示多个自旋算符的乘积"""
    
    def __init__(self, *operators):
        """初始化乘积算符
        
        Args:
            *operators: 要相乘的算符
        """
        self.operators = []
        for op in operators:
            if isinstance(op, ProductOperator):
                # 展开乘积算符，避免嵌套
                self.operators.extend(op.operators)
            else:
                self.operators.append(op)
    
    def __mul__(self, other):
        """实现算符乘法"""
        if isinstance(other, (SpinOperator, ProductOperator)):
            return ProductOperator(self, other)
        return NotImplemented
    
    def __add__(self, other):
        """实现算符加法"""
        if isinstance(other, (SpinOperator, ProductOperator, Hamiltonian)):
            return Hamiltonian([(1.0, self), (1.0, other)])
        return NotImplemented
    
    def __sub__(self, other):
        """实现算符减法"""
        if isinstance(other, (SpinOperator, ProductOperator, Hamiltonian)):
            return Hamiltonian([(1.0, self), (-1.0, other)])
        return NotImplemented
    
    def __rmul__(self, scalar):
        """实现标量乘法（右侧）"""
        if isinstance(scalar, (int, float)):
            return Hamiltonian([(scalar, self)])
        return NotImplemented
    
    def __repr__(self):
        """返回算符的字符串表示"""
        return " * ".join([repr(op) for op in self.operators])


class Hamiltonian:
    """哈密顿量类，用于表示量子系统的哈密顿量"""
    
    def __init__(self, terms=None):
        """初始化哈密顿量
        
        Args:
            terms: 哈密顿量的项，格式为 [(系数, 算符), ...]
        """
        self.terms = terms if terms is not None else []
    
    def __add__(self, other):
        """实现哈密顿量加法"""
        if isinstance(other, Hamiltonian):
            return Hamiltonian(self.terms + other.terms)
        elif isinstance(other, (SpinOperator, ProductOperator)):
            return Hamiltonian(self.terms + [(1.0, other)])
        return NotImplemented
    
    def __iadd__(self, other):
        """实现哈密顿量就地加法"""
        if isinstance(other, Hamiltonian):
            self.terms.extend(other.terms)
        elif isinstance(other, (SpinOperator, ProductOperator)):
            self.terms.append((1.0, other))
        elif isinstance(other, tuple) and len(other) == 2:
            # 处理 (系数, 算符) 形式的项
            self.terms.append(other)
        return self
    
    def __sub__(self, other):
        """实现哈密顿量减法"""
        if isinstance(other, Hamiltonian):
            negated_terms = [(-coef, op) for coef, op in other.terms]
            return Hamiltonian(self.terms + negated_terms)
        elif isinstance(other, (SpinOperator, ProductOperator)):
            return Hamiltonian(self.terms + [(-1.0, other)])
        return NotImplemented
    
    def __rmul__(self, scalar):
        """实现标量乘法（右侧）"""
        if isinstance(scalar, (int, float)):
            scaled_terms = [(scalar * coef, op) for coef, op in self.terms]
            return Hamiltonian(scaled_terms)
        return NotImplemented
    
    def __repr__(self):
        """返回哈密顿量的字符串表示"""
        if not self.terms:
            return "Hamiltonian()"
        
        terms_str = []
        for coef, op in self.terms:
            if coef == 1.0:
                terms_str.append(repr(op))
            elif coef == -1.0:
                terms_str.append(f"-{repr(op)}")
            else:
                terms_str.append(f"{coef} * {repr(op)}")
        
        return "Hamiltonian(" + " + ".join(terms_str) + ")"


class SpinNamespace:
    """自旋算符命名空间，提供创建各种自旋算符的方法"""
    
    def i(self, qubit):
        """创建单位算符"""
        return SpinOperator('i', qubit)
    
    def x(self, qubit):
        """创建X自旋算符"""
        return SpinOperator('x', qubit)
    
    def y(self, qubit):
        """创建Y自旋算符"""
        return SpinOperator('y', qubit)

    def z(self, qubit):
        """创建Z自旋算符"""
        return SpinOperator('z', qubit)


# 创建spin命名空间实例
spin = SpinNamespace()


class ExpectationResult:
    """期望值结果类"""
    
    def __init__(self, value: float):
        self.value = value
    
    def expectation(self):
        """返回期望值"""
        return self.value
    
    def __repr__(self):
        return f"ExpectationResult(value={self.value})"


class ObserveProgram:
    """观察程序类，用于处理量子态的观察操作"""
    
    def __init__(self, program: QuantumProgram, hamiltonian: Hamiltonian):
        self.program = program
        self.hamiltonian = hamiltonian
    
    def get_hamiltonian_terms(self):
        """获取哈密顿量的项，转换为C++后端可用的格式"""
        terms = []
        
        for coef, op in self.hamiltonian.terms:
            if isinstance(op, SpinOperator):
                # 单个自旋算符
                qubit_idx = op.qubit.index if isinstance(op.qubit, Qubit) else op.qubit
                terms.append((coef, [(op.op_type, qubit_idx)]))
            elif isinstance(op, ProductOperator):
                # 乘积算符
                product_ops = []
                for spin_op in op.operators:
                    qubit_idx = spin_op.qubit.index if isinstance(spin_op.qubit, Qubit) else spin_op.qubit
                    product_ops.append((spin_op.op_type, qubit_idx))
                terms.append((coef, product_ops))
        
        return terms


def observe(program: QuantumProgram, hamiltonian: Hamiltonian, *args, **kwargs) -> ExpectationResult:
    """观察量子程序的哈密顿量期望值"""
    
    target = kwargs.get('target', get_target())
    
    # 创建观察程序实例
    observe_program = ObserveProgram(program, hamiltonian)
    
    # 获取模拟器实例
    simulator = _backend_manager.get_simulator(target)
    
    # 生成SSA汇编代码，传递args作为prog的参数
    ssa_assembly = program.generate_ssa_assembly(*args, **kwargs)
    
    # 加载SSA汇编代码到模拟器
    load_result = simulator.load_ssa_assembly(ssa_assembly)
    if not load_result:
        raise RuntimeError(f"Failed to load SSA assembly: {simulator.get_error()}")
    
    # 运行模拟器，执行量子电路并准备量子态
    run_result = simulator.run()
    if not run_result:
        raise RuntimeError(f"Failed to run simulation: {simulator.get_error()}")
    
    # 获取哈密顿量项，转换为C++兼容格式
    hamiltonian_terms = observe_program.get_hamiltonian_terms()
    
    # 调用expect方法计算期望值
    expectation = simulator.expect(hamiltonian_terms)
    
    return ExpectationResult(expectation)
