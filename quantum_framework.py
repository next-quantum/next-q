import ast
import enum
import inspect
from typing import List, Dict, Any, Callable, Optional, TypeVar, Union, Tuple
import random
import astor
import warnings

# 为了兼容Python 3.10，定义Self类型
Self = TypeVar('Self')


# 比较操作符枚举
class ComparisonOperator(enum.Enum):
    EQ = '=='  # 等于
    NE = '!='  # 不等于
    LT = '<'   # 小于
    LE = '<='  # 小于等于
    GT = '>'   # 大于
    GE = '>='  # 大于等于


# 量子门属性枚举
class QuantumGateAttribute(enum.Enum):
    CTRL = 'ctrl'  # 受控门
    ADJ = 'adj'    # 共轭转置


# 量子函数枚举
class QuantumFunction(enum.Enum):
    MZ = 'mz'      # 测量操作
    QVECTOR = 'qvector'  # 创建量子向量
    QUBIT = 'qubit'      # 创建单个量子比特
    H = 'h'        # Hadamard门
    X = 'x'        # Pauli-X门
    Z = 'z'        # Pauli-Z门


class Qubit:
    def __init__(self, index: int = -1):
        self.index: int = index
        self.id: str = f'qubit_{index}' if index >= 0 else f'qubit_{id(self)}'

    def __repr__(self) -> str:
        return self.id


class QVector:
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

    def __repr__(self) -> str:
        return f'qvector({self.size})'


class QuantumOperation:
    def __init__(
            self,
            gate_name: str,
            qubits: tuple,
            controls: list,
            adjoint: bool):
        self.gate_name: str = gate_name
        self.qubits: tuple = qubits
        self.controls: List[Qubit] = controls
        self.adjoint: bool = adjoint

    def __repr__(self) -> str:
        adjoint_str: str = '.adj' if self.adjoint else ''
        ctrl_str: str = f'.ctrl({self.controls})' if self.controls else ''
        qubits_repr = f'([{self.qubits[0]}])' if len(
            self.qubits) == 1 else f'{self.qubits}'
        return f'{self.gate_name}{ctrl_str}{adjoint_str}{qubits_repr}'


class ControlledGate:
    """受控门的辅助类，支持链式调用语法：x.ctrl([q0])([q1])"""

    def __init__(self, gate_name: str, controls: List[Qubit], adjoint: bool):
        self.gate_name: str = gate_name
        self.controls: List[Qubit] = controls
        self.adjoint: bool = adjoint

    def __call__(self,
                 target_qubits: Union[Qubit,
                                      List[Qubit]]) -> QuantumOperation:
        """处理目标比特调用，如 ([q1])"""
        if isinstance(target_qubits, list):
            # 支持 [q1] 形式
            target_qubits = tuple(target_qubits)

        return QuantumOperation(
            self.gate_name,
            target_qubits,
            self.controls,
            self.adjoint)


class QuantumGate:
    def __init__(self, name: str):
        self.name: str = name
        self.adjoint: bool = False
        self.controls: List[Qubit] = []

    def __call__(self, *args, **kwargs) -> QuantumOperation:
        return QuantumOperation(self.name, args, self.controls, self.adjoint)

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
    def __init__(self, qubit: Qubit, measurement_reg: Optional[int] = None):
        self.qubit: Qubit = qubit
        self.measurement_reg: Optional[int] = measurement_reg  # 测量寄存器编号
        self.result: Optional[int] = None

    def __repr__(self):
        if self.measurement_reg is not None:
            return f'mz({self.qubit}) to measurement reg {self.measurement_reg}'
        else:
            return f'mz({self.qubit})'


class MeasureToClassicalOperation:
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
    def __init__(self, left_reg: int, right_reg: int, output_reg: int):
        self.left_reg: int = left_reg  # 左操作数经典寄存器
        self.right_reg: int = right_reg  # 右操作数经典寄存器
        self.output_reg: int = output_reg  # 输出经典寄存器

    def __repr__(self):
        return f'c{self.left_reg} and c{self.right_reg} to c{self.output_reg}'


class OrOperation:
    def __init__(self, left_reg: int, right_reg: int, output_reg: int):
        self.left_reg: int = left_reg  # 左操作数经典寄存器
        self.right_reg: int = right_reg  # 右操作数经典寄存器
        self.output_reg: int = output_reg  # 输出经典寄存器

    def __repr__(self):
        return f'c{self.left_reg} or c{self.right_reg} to c{self.output_reg}'


class XorOperation:
    def __init__(self, left_reg: int, right_reg: int, output_reg: int):
        self.left_reg: int = left_reg  # 左操作数经典寄存器
        self.right_reg: int = right_reg  # 右操作数经典寄存器
        self.output_reg: int = output_reg  # 输出经典寄存器

    def __repr__(self):
        return f'c{self.left_reg} xor c{self.right_reg} to c{self.output_reg}'


class VarQubitOperation:
    """表示根据经典寄存器索引动态获取量子比特的操作"""

    def __init__(self, var_name: str, index_reg: int, qvector_size: int):
        self.var_name: str = var_name  # 变量名
        self.index_reg: int = index_reg  # 存储量子比特索引的经典寄存器
        self.qvector_size: int = qvector_size  # 量子向量的大小

    def __repr__(self):
        return f'VarQubit: {self.var_name} = qvector[{self.index_reg}] (size: {self.qvector_size})'


class Condition:
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
    def __init__(
            self,
            circuit: QuantumCircuit,
            source_code: str,
            original_func: Callable):
        self.circuit: QuantumCircuit = circuit
        self.source_code: str = source_code
        self.original_func: Callable = original_func

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
                operations_str.append(f"      - Then branch operations:")
                if op.then_operations:
                    for j, then_op in enumerate(op.then_operations):
                        operations_str.append(f"          {j+1}. {then_op}")
                else:
                    operations_str.append(f"          (no operations)")
                operations_str.append(f"      - Else branch operations:")
                if op.else_operations:
                    for j, else_op in enumerate(op.else_operations):
                        operations_str.append(f"          {j+1}. {else_op}")
                else:
                    operations_str.append(f"          (no operations)")
            elif isinstance(op, Measurement):
                # 显示测量操作，使用测量寄存器而非经典寄存器
                operations_str.append(f"  {i+1}. {op}")
            elif isinstance(op, MeasureToClassicalOperation):
                operations_str.append(f"  {i+1}. {op}")

        return (f"QuantumProgram(name='{func_name}'{signature}, "
                f"qubits={qubit_count}, operations={operation_count})\n"
                f"Source Code:\n'''\n{self.source_code}\n'''\n"
                f"Circuit Operations:\n{chr(10).join(operations_str)}")

    def generate_ssa_assembly(self) -> str:
        """生成SSA低级汇编"""
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


def _is_mz_call(node: ast.AST) -> bool:
    """检查节点是否是mz函数调用"""
    return isinstance(node, ast.Call) and isinstance(
        node.func, ast.Name) and node.func.id == QuantumFunction.MZ.value

# 全局函数：从比较表达式中提取寄存器编号


def _extract_reg_from_compare(compare: ast.Compare, visitor) -> Optional[int]:
    """从比较表达式中提取寄存器编号"""
    left = compare.left

    # 处理测量比较：if mz(q0) == 1:
    if _is_mz_call(left):
        qubit_arg = left.args[0]
        qubit_or_qvector = visitor.evaluator.evaluate(qubit_arg)
        if isinstance(qubit_or_qvector, Qubit):
            qubit = qubit_or_qvector
            # 先添加测量操作
            measurement = Measurement(qubit)
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

    def __init__(self, variables):
        self.variables = variables

    def evaluate(self, expr: ast.AST) -> Any:
        """评估任意Python表达式，返回其值"""
        # 使用类型分发字典提高效率，确保覆盖所有主要AST节点类型
        handler_name = self.expr_handlers.get(type(expr))
        if handler_name:
            handler = getattr(self, handler_name)
            return handler(expr)

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
        return None

    def _eval_binop(self, expr: ast.BinOp) -> Any:
        """评估二元操作表达式"""
        left = self.evaluate(expr.left)
        right = self.evaluate(expr.right)

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
            if func_name in ['range', 'enumerate']:
                args = [self.evaluate(arg) for arg in expr.args]
                # 统一处理：对于无效参数返回空范围/枚举，保持一致性
                try:
                    if func_name == 'range':
                        return range(*args)
                    elif func_name == 'enumerate':
                        return enumerate(*args)
                except (TypeError, ValueError):
                    # 对于无效参数，返回空的可迭代对象
                    return range(0) if func_name == 'range' else enumerate([])

            # 量子函数处理
            elif func_name in ['qvector', 'qubit', 'mz']:
                # 对于量子函数调用，返回None，因为它们在visit_Call中处理
                return None
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
    """处理循环语句相关的类

    该类负责处理量子框架中的for和while循环语句，包括：
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
        except (ValueError, TypeError, AttributeError) as e:
            # 无法评估条件表达式，发出警告并跳过
            import warnings
            warnings.warn(
                f"无法评估while循环条件表达式: {astor.to_source(node.test).strip()}, 跳过循环")
            return


class ConditionHandler:
    """处理条件语句相关的类"""

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
    """处理测量相关操作的类"""

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

    def is_mz_call(self, node) -> bool:
        """检查节点是否是mz函数调用"""
        return isinstance(
            node,
            ast.Call) and isinstance(
            node.func,
            ast.Name) and node.func.id == QuantumFunction.MZ.value

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
        target_qubits: List[Qubit] = self.visitor.qubit_handler.get_target_qubits(
            evaluated_args)

        # 对每个量子比特，先执行测量，再分配到经典寄存器
        # 这样可以确保测量操作和赋值操作交替执行，与预期的ASM顺序一致
        for qubit in target_qubits:
            # 在所有上下文中都生成测量操作，确保条件表达式中的测量也能被正确处理
            measurement: Measurement = Measurement(qubit)
            self.visitor.circuit.add_measurement(measurement)

            # 如果提供了var_name，将测量结果分配到经典寄存器
            # 支持将多个量子比特的测量结果映射到同一个变量名
            # 这允许后续使用all()/any()等函数处理多个测量结果
            if var_name:
                self.add_measure_to_classical(qubit.index, var_name)

        # 我们不执行真正的测量，只返回占位值
        return None if len(target_qubits) == 1 else []


class QubitHandler:
    """处理量子比特相关操作的类"""

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
            # 不处理条件表达式，因为它们会在_handle_call中被特殊处理
        return target_qubits

    def create_quantum_operation(
            self,
            gate_name: str,
            target_qubits: List[Qubit],
            controls: List[Qubit] = None,
            adjoint: bool = False) -> None:
        """统一创建量子操作，支持单个量子比特或量子向量"""
        controls = controls or []

        # 直接为每个目标量子比特创建量子操作
        for target_qubit in target_qubits:
            quantum_op = QuantumOperation(
                gate_name, (target_qubit,), controls, adjoint)
            self.visitor.circuit.add_operation(quantum_op)


class BoolOpHandler:
    """处理布尔操作相关的类"""

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
        """处理比较表达式操作数，如 result > 0 或 mz(q0) == 1"""
        # 处理测量操作，如 mz(q0) == 1
        if _is_mz_call(operand.left):
            return self._handle_mz_compare_operand(operand)
        # 处理普通比较表达式，如 result > 0
        elif isinstance(operand.left, ast.Name):
            return self._handle_var_compare_operand(operand)
        return None

    def _handle_mz_compare_operand(
            self, compare: ast.Compare) -> Optional[int]:
        """处理测量比较操作数，如 mz(q0) == 1"""
        # 提取测量操作的量子比特
        qubit_arg = compare.left.args[0]
        qubit_or_qvector = self.visitor.evaluator.evaluate(qubit_arg)
        if isinstance(qubit_or_qvector, Qubit):
            qubit = qubit_or_qvector
            # 生成测量操作
            measurement = Measurement(qubit)
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
    """处理函数调用相关的类"""

    def __init__(self, visitor):
        self.visitor = visitor

    def handle_call(self, node):
        """处理函数调用，如 h(q0), mz(q0) 等"""
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
                    if var_value == item:
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
                    if var_value == item:
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

        if func_name == QuantumFunction.MZ.value:
            # 处理测量操作，传递var_name参数
            return self._handle_mz_call(evaluated_args, return_value, var_name)
        elif func_name == QuantumFunction.QVECTOR.value:
            # 处理量子向量创建
            return self._handle_qvector_call(
                evaluated_args, var_name, return_value)
        elif func_name == QuantumFunction.QUBIT.value:
            # 处理单个量子比特创建
            return self._handle_qubit_call(var_name, return_value)
        else:
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

    def _handle_mz_call(
            self,
            evaluated_args: List[Any],
            return_value: bool = False,
            var_name: Optional[str] = None) -> Any:
        """处理mz（测量）调用"""
        return self.visitor.measurement_handler.handle_mz_call(
            evaluated_args, return_value, var_name)

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

    def _handle_gate_call(
            self,
            gate_name: str,
            evaluated_args: List[Any]) -> None:
        """处理量子门调用"""
        # 应用量子门到每个量子比特，支持表达式索引：qubits[0 + 1]
        target_qubits = self.visitor.qubit_handler.get_target_qubits(
            evaluated_args)
        self.visitor.qubit_handler.create_quantum_operation(
            gate_name, target_qubits)
        return None

    def _handle_chain_call(self,
                           node: ast.Call,
                           return_value: bool = False,
                           evaluated_args: Optional[List[Any]] = None) -> None:
        """处理链式调用，如 x.adj()(q0) 或 x.ctrl(q0)(q1) 或 x.ctrl(q0).adj()(q1)"""
        # 使用传递的评估后参数，避免重复评估
        if evaluated_args is None:
            target_args: List[Any] = [
                self.visitor.evaluator.evaluate(arg) for arg in node.args]
        else:
            target_args = evaluated_args

        target_qubits: List[Qubit] = self.visitor.qubit_handler.get_target_qubits(
            target_args)

        # 从链式调用中提取门名和属性信息
        gate_name = self._get_gate_name(node.func.func.value)
        adjoint = False
        controls = []

        # 处理共轭转置链
        if node.func.func.attr == QuantumGateAttribute.ADJ.value:
            adjoint = True
            # 检查是否是受控门的共轭转置，如 x.ctrl(q0).adj()(q1)
            if isinstance(
                    node.func.func.value,
                    ast.Call) and isinstance(
                    node.func.func.value.func,
                    ast.Attribute) and node.func.func.value.func.attr == QuantumGateAttribute.CTRL.value:
                # 提取控制位
                ctrl_call = node.func.func.value
                ctrl_args = [self.visitor.evaluator.evaluate(
                    arg) for arg in ctrl_call.args]
                controls = _get_qubits_from_args(ctrl_args)
        # 处理受控门链
        elif node.func.func.attr == QuantumGateAttribute.CTRL.value:
            # 提取控制位
            ctrl_call = node.func
            ctrl_args = [self.visitor.evaluator.evaluate(
                arg) for arg in ctrl_call.args]
            controls = _get_qubits_from_args(ctrl_args)

        # 创建量子操作
        self.visitor.qubit_handler.create_quantum_operation(
            gate_name, target_qubits, controls, adjoint)
        return None

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

        if attr_name == QuantumGateAttribute.CTRL.value:
            # 处理直接属性调用，如 x.ctrl(q0, q1) 或 x.ctrl(q0, sub_qubits)
            if len(evaluated_args) >= 2:
                # 最后一个参数是目标量子比特或量子向量
                target_args: List[Any] = [evaluated_args[-1]]
                # 前面的参数是控制量子比特
                control_args: List[Any] = evaluated_args[:-1]

                # 获取目标量子比特列表
                target_qubits: List[Qubit] = self.visitor.qubit_handler.get_target_qubits(
                    target_args)
                # 获取控制量子比特列表
                valid_controls: List[Qubit] = _get_qubits_from_args(
                    control_args)

                # 为每个目标量子比特创建量子操作
                self.visitor.qubit_handler.create_quantum_operation(
                    gate_name, target_qubits, valid_controls, False)
        elif attr_name == QuantumGateAttribute.ADJ.value:
            # 处理直接属性调用，如 x.adj(q0) 或 z.adj(sub_qubits)
            target_qubits: List[Qubit] = self.visitor.qubit_handler.get_target_qubits(
                evaluated_args)
            self.visitor.qubit_handler.create_quantum_operation(
                gate_name, target_qubits, adjoint=True)

        return None


class AssignmentHandler:
    """处理各种赋值操作的类"""

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
        if _is_mz_call(node.value):
            # 处理测量结果赋值
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
        """处理测量结果赋值：result = mz(q1)"""
        # 获取测量的量子比特或量子向量
        qubit_arg = mz_call.args[0]
        qubit_or_qvector: Any = self.visitor.evaluator.evaluate(qubit_arg)

        if isinstance(qubit_or_qvector, Qubit):
            # 测量单个量子比特：result = mz(q1)
            qubit: Qubit = qubit_or_qvector
            # 为单个量子比特添加测量操作
            measurement: Measurement = Measurement(qubit)
            self.visitor.circuit.add_measurement(measurement)
            # 直接使用量子比特索引作为测量寄存器编号
            measurement_reg: int = qubit.index
            # 将测量结果分配到经典寄存器
            self.visitor.measurement_handler.add_measure_to_classical(
                measurement_reg, var_name)
        elif isinstance(qubit_or_qvector, QVector):
            # 测量量子向量：result = mz(sub_qubits)
            qvector: QVector = qubit_or_qvector
            # 为每个量子比特添加测量操作
            for qubit in qvector.qubits:
                measurement: Measurement = Measurement(qubit)
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
    def __init__(self):
        self.circuit: QuantumCircuit = QuantumCircuit()
        self.variables: Dict[str, Any] = {}

        self.var_to_classical_reg: Dict[str, int] = {}  # 变量名到经典寄存器的映射
        self.next_classical_reg: int = 0  # 下一个可用的经典寄存器编号
        self.measurement_to_classical: Dict[int, int] = {}  # 测量寄存器到经典寄存器的映射

        # 创建表达式求值器实例
        self.evaluator = ExpressionEvaluator(self.variables)

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


# 全局变量
_current_target = 'cpu'


def set_target(target: str):
    """设置后端目标"""
    global _current_target
    _current_target = target


def quantum_kernel(func: Callable) -> QuantumProgram:
    """量子kernel装饰器，用于解析量子程序"""
    import textwrap

    # 获取函数源代码并移除缩进
    source: str = inspect.getsource(func)
    dedented_source: str = textwrap.dedent(source)

    # 解析AST
    tree: ast.Module = ast.parse(dedented_source)

    # 创建访问者并访问AST中的函数定义
    visitor = QuantumProgramVisitor()
    # 直接访问函数定义，无需遍历整个模块
    visitor.visit(tree.body[0])

    # 创建量子程序对象
    program: QuantumProgram = QuantumProgram(visitor.circuit, source, func)
    return program


def sample(program: QuantumProgram, shots_count: int = 1000) -> Dict[str, int]:
    """运行量子程序并返回测量结果"""
    # 这里是简化的sample实现，实际应该调用后端执行
    results = {}

    for _ in range(shots_count):
        # 模拟量子比特状态
        qubit_states = {
            qubit: random.randint(
                0, 1) for qubit in program.circuit.qubits}
        measurement_results = []
        classical_registers = {}

        # 遍历所有操作，包括条件分支和测量
        for op in program.circuit.operations:
            if isinstance(op, QuantumOperation):
                # 处理量子操作（这里简化处理，实际应该更新量子状态）
                pass
            elif isinstance(op, Condition):
                # 处理条件分支
                # 获取经典寄存器值
                if op.classical_reg in classical_registers:
                    reg_value = classical_registers[op.classical_reg]
                else:
                    # 模拟经典寄存器值
                    reg_value = random.choice([0, 1])
                    classical_registers[op.classical_reg] = reg_value

                # 评估条件
                condition_met = False
                if op.operator == ComparisonOperator.EQ:
                    condition_met = (reg_value == op.value)
                elif op.operator == ComparisonOperator.NE:
                    condition_met = (reg_value != op.value)
                elif op.operator == ComparisonOperator.LT:
                    condition_met = (reg_value < op.value)
                elif op.operator == ComparisonOperator.LE:
                    condition_met = (reg_value <= op.value)
                elif op.operator == ComparisonOperator.GT:
                    condition_met = (reg_value > op.value)
                elif op.operator == ComparisonOperator.GE:
                    condition_met = (reg_value >= op.value)

                # 记录条件结果
                measurement_results.append(int(condition_met))
            elif isinstance(op, Measurement):
                # 处理测量操作，记录测量结果
                result = qubit_states[op.qubit]
                measurement_results.append(result)
                classical_registers[op.qubit] = result

        # 生成测量结果字符串
        bits = ''.join(str(result) for result in measurement_results)
        results[bits] = results.get(bits, 0) + 1

    return results


# 创建量子门对象
h = QuantumGate('h')
x = QuantumGate('x')
z = QuantumGate('z')

# 测量函数


def mz(qubit: Qubit) -> Measurement:
    return Measurement(qubit)


# 创建小写别名，匹配导入语句
qubit = Qubit


class SSAAssembler:
    """生成SSA低级汇编的类"""

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

    def generate(self, program):
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

        if op.controls:
            controls = [self.get_qubit_reg(q) for q in op.controls]
            controls_str = ", ".join(controls)
            line = f"qgate.{op.gate_name} {qubits_str}, ctrl={controls_str}"
        else:
            line = f"qgate.{op.gate_name} {qubits_str}"

        if op.adjoint:
            line += ".adj"

        self.add_line(line)

    def _generate_measurement(self, op):
        """生成测量操作的汇编"""
        qubit_reg = self.get_qubit_reg(op.qubit)
        meas_reg = f"m{op.measurement_reg}"
        self.measure_regs.add(meas_reg)
        self.add_line(f"measure.z {qubit_reg}, {meas_reg}")

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
            self.add_line(f"const {output_reg}, 1")
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

    def _generate_condition(self, op):
        """生成条件操作的汇编"""
        # 生成条件标签
        else_label = self.generate_label()
        end_label = self.generate_label()

        # 获取寄存器和比较值
        reg = f"c{op.classical_reg}"
        value = op.value

        # 生成条件测试和跳转
        if op.operator == ComparisonOperator.EQ:
            self.add_line(f"cmp {reg}, {value}")
            self.add_line(f"jumpne {else_label}")
        elif op.operator == ComparisonOperator.NE:
            self.add_line(f"cmp {reg}, {value}")
            self.add_line(f"jumpe {else_label}")
        elif op.operator == ComparisonOperator.LT:
            self.add_line(f"cmp {reg}, {value}")
            self.add_line(f"jumpge {else_label}")
        elif op.operator == ComparisonOperator.LE:
            self.add_line(f"cmp {reg}, {value}")
            self.add_line(f"jumpg {else_label}")
        elif op.operator == ComparisonOperator.GT:
            self.add_line(f"cmp {reg}, {value}")
            self.add_line(f"jumple {else_label}")
        elif op.operator == ComparisonOperator.GE:
            self.add_line(f"cmp {reg}, {value}")
            self.add_line(f"jumpl {else_label}")

        # 生成then分支操作
        self.add_comment(f"Then branch ({len(op.then_operations)} operations)")
        for then_op in op.then_operations:
            if isinstance(then_op, QuantumOperation):
                self._generate_quantum_operation(then_op)
            elif isinstance(then_op, Measurement):
                self._generate_measurement(then_op)
            elif isinstance(then_op, MeasureToClassicalOperation):
                self._generate_assign(then_op)
            elif isinstance(then_op, AllOperation):
                self._generate_all_operation(then_op)
            elif isinstance(then_op, AndOperation):
                self._generate_and_operation(then_op)
            elif isinstance(then_op, OrOperation):
                self._generate_or_operation(then_op)
            elif isinstance(then_op, XorOperation):
                self._generate_xor_operation(then_op)

        # 跳转到结束标签
        self.add_line(f"jump {end_label}")

        # 生成else分支标签
        self.add_line(f"{else_label}:")

        # 生成else分支操作
        self.add_comment(f"Else branch ({len(op.else_operations)} operations)")
        for else_op in op.else_operations:
            if isinstance(else_op, QuantumOperation):
                self._generate_quantum_operation(else_op)
            elif isinstance(else_op, Measurement):
                self._generate_measurement(else_op)
            elif isinstance(else_op, MeasureToClassicalOperation):
                self._generate_assign(else_op)
            elif isinstance(else_op, AllOperation):
                self._generate_all_operation(else_op)
            elif isinstance(else_op, AndOperation):
                self._generate_and_operation(else_op)
            elif isinstance(else_op, OrOperation):
                self._generate_or_operation(else_op)
            elif isinstance(else_op, XorOperation):
                self._generate_xor_operation(else_op)

        # 生成结束标签
        self.add_line(f"{end_label}:")

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
        self.add_line(
            f"br.cond {classical_reg}, {op.operator.value}, {op.value}, {then_label}, {else_label}")

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
                # 生成常量赋值指令：mov <reg>, <value>
                reg = f"c{op['reg']}"
                value = op['value']
                self.classical_regs.add(reg)
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


qvector = QVector


class SSASimulator:
    """SSA模拟器执行后端框架，实现if和assign操作，为量子门和测量操作预留接口"""

    def __init__(self):
        self.quantum_registers = {}      # 量子寄存器: {name: qubit_object}
        self.classical_registers = {}    # 经典寄存器: {name: value}
        self.measurement_registers = {}  # 测量寄存器: {name: value}

        self.instructions = []           # 解析后的指令列表
        self.labels = {}                 # 标签到指令索引的映射
        self.pc = 0                      # 程序计数器
        self.running = False             # 运行状态

        self.instruction_handlers = {
            'declare': self._handle_declare,
            'assign': self._handle_assign,
            'br.cond': self._handle_br_cond,
            'br': self._handle_br,
            'qgate': self._handle_qgate,
            'measure': self._handle_measure,
            'and': self._handle_and,
            'or': self._handle_or
        }

    def load_assembly(self, assembly_code: str) -> None:
        """加载并解析SSA汇编代码"""
        self.__init__()

        lines = assembly_code.strip().split('\n')

        for line in lines:
            line = line.strip()

            if not line or line.startswith(';;'):
                continue

            if line.endswith(':'):
                label = line[:-1].strip()
                self.labels[label] = len(self.instructions)
                continue

            line = line.replace(',', ' ')
            parts = line.split()
            if not parts:
                continue

            opcode = parts[0]
            args = parts[1:] if len(parts) > 1 else []

            self.instructions.append((opcode, args))

    def run(self) -> None:
        """执行加载的SSA汇编代码"""
        self.running = True
        self.pc = 0

        while self.running and self.pc < len(self.instructions):
            opcode, args = self.instructions[self.pc]

            if opcode in self.instruction_handlers:
                self.instruction_handlers[opcode](args)
            else:
                self.pc += 1

        self.running = False

    def stop(self) -> None:
        """停止执行"""
        self.running = False

    def _handle_declare(self, args: List[str]) -> None:
        """处理寄存器声明指令"""
        if len(args) < 2:
            self.pc += 1
            return

        reg_type = args[0]
        reg_name = args[1]

        if reg_type == 'qreg':
            if reg_name not in self.quantum_registers:
                self.quantum_registers[reg_name] = None  # 量子寄存器值由用户实现
        elif reg_type == 'creg':
            if reg_name not in self.classical_registers:
                self.classical_registers[reg_name] = 0  # 经典寄存器初始化为0
        elif reg_type == 'mreg':
            if reg_name not in self.measurement_registers:
                self.measurement_registers[reg_name] = 0  # 测量寄存器初始化为0

        self.pc += 1

    def _handle_assign(self, args: List[str]) -> None:
        """处理赋值操作：assign mreg, creg"""
        if len(args) < 2:
            self.pc += 1
            return

        src_reg = args[0]
        dst_reg = args[1]

        if src_reg in self.measurement_registers and dst_reg in self.classical_registers:
            self.classical_registers[dst_reg] = self.measurement_registers[src_reg]

        self.pc += 1

    def _handle_br_cond(self, args: List[str]) -> None:
        """处理条件分支：br.cond creg, op, value, label_then, label_else"""
        if len(args) < 5:
            self.pc += 1
            return

        creg = args[0]
        op = args[1]
        value_str = args[2]
        # 根据字符串值自动转换类型
        if value_str.lower() == 'true':
            value = True
        elif value_str.lower() == 'false':
            value = False
        else:
            try:
                value = int(value_str)
            except ValueError:
                # 如果转换失败，保留为字符串
                value = value_str
        label_then = args[3]
        label_else = args[4]

        reg_value = self.classical_registers.get(creg, 0)

        condition_met = False
        if op == '==':
            condition_met = (reg_value == value)
        elif op == '!=':
            condition_met = (reg_value != value)
        elif op == '<':
            condition_met = (reg_value < value)
        elif op == '<=':
            condition_met = (reg_value <= value)
        elif op == '>':
            condition_met = (reg_value > value)
        elif op == '>=':
            condition_met = (reg_value >= value)

        if condition_met:
            if label_then in self.labels:
                self.pc = self.labels[label_then]
            else:
                self.pc += 1
        else:
            if label_else in self.labels:
                self.pc = self.labels[label_else]
            else:
                self.pc += 1

    def _handle_br(self, args: List[str]) -> None:
        """处理无条件跳转：br label"""
        if len(args) < 1:
            self.pc += 1
            return

        label = args[0]
        if label in self.labels:
            self.pc = self.labels[label]
        else:
            self.pc += 1

    def _handle_and(self, args: List[str]) -> None:
        """处理AND操作：and output_reg, left_reg, right_reg"""
        if len(args) < 3:
            self.pc += 1
            return

        output_reg = args[0]
        left_reg = args[1]
        right_reg = args[2]

        if left_reg in self.classical_registers and right_reg in self.classical_registers:
            left_val = self.classical_registers[left_reg]
            right_val = self.classical_registers[right_reg]
            self.classical_registers[output_reg] = left_val & right_val

        self.pc += 1

    def _handle_or(self, args: List[str]) -> None:
        """处理OR操作：or output_reg, left_reg, right_reg"""
        if len(args) < 3:
            self.pc += 1
            return

        output_reg = args[0]
        left_reg = args[1]
        right_reg = args[2]

        if left_reg in self.classical_registers and right_reg in self.classical_registers:
            left_val = self.classical_registers[left_reg]
            right_val = self.classical_registers[right_reg]
            self.classical_registers[output_reg] = left_val | right_val

        self.pc += 1

    def _handle_qgate(self, args: List[str]) -> None:
        """处理量子门操作，由用户实现"""
        print(f"[SSASimulator] 量子门操作（待实现）: qgate {' '.join(args)}")
        self.pc += 1

    def _handle_measure(self, args: List[str]) -> None:
        """处理测量操作，由用户实现"""
        print(f"[SSASimulator] 测量操作（待实现）: measure {' '.join(args)}")
        self.pc += 1

    # -------------------------------
    # 辅助方法
    # -------------------------------

    def get_classical_reg_value(self, reg_name: str) -> int:
        """获取经典寄存器的值"""
        return self.classical_registers.get(reg_name, 0)

    def set_classical_reg_value(self, reg_name: str, value: int) -> None:
        """设置经典寄存器的值"""
        self.classical_registers[reg_name] = value

    def get_measurement_reg_value(self, reg_name: str) -> int:
        """获取测量寄存器的值"""
        return self.measurement_registers.get(reg_name, 0)

    def set_measurement_reg_value(self, reg_name: str, value: int) -> None:
        """设置测量寄存器的值"""
        self.measurement_registers[reg_name] = value

    def print_registers(self) -> None:
        """打印所有寄存器的值"""
        print("\n=== 寄存器状态 ===")
        print("经典寄存器:")
        for reg, value in self.classical_registers.items():
            print(f"  {reg}: {value}")

        print("测量寄存器:")
        for reg, value in self.measurement_registers.items():
            print(f"  {reg}: {value}")

        print("量子寄存器:")
        for reg in self.quantum_registers:
            print(f"  {reg}: {self.quantum_registers[reg]}")
        print("==================")
