#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

// 明确的类型别名，方便将来扩展
typedef int32_t int32;
typedef float float32;

namespace ssa {

// 寄存器类型枚举
enum class RegisterType {
    QREG,  // 量子寄存器
    CREG,  // 经典寄存器
    MREG   // 测量寄存器
};

// 量子门类型枚举
enum class QuantumGateType {
    X, Y, Z, H, S, T,
    ADJS, ADJT,
    RX, RY, RZ, R1,
    RESET,
    CNOT, SWAP,
    TOFFOLI,
    UNKNOWN
};

// 指令类型枚举
enum class InstructionType {
    DECLARE,        // 寄存器声明
    QGATE,          // 量子门
    MEASURE,        // 测量
    MOV,            // 移动操作
    AND,            // 经典与操作
    OR,             // 经典或操作
    XOR,            // 经典异或操作
    ALL,            // 所有测量结果为真
    ANY,            // 任一测量结果为真
    BR_COND,        // 条件分支
    BR_UNCOND,      // 无条件分支
    LABEL,          // 标签
    UNKNOWN         // 未知指令
};

// 寄存器信息结构体
struct Register {
    std::string name;        // 寄存器名称
    RegisterType type;       // 寄存器类型
    int32 index;               // 寄存器索引
    
    // 默认构造函数，用于map等容器
    Register() : type(RegisterType::QREG), index(-1) {}
    
    // 带参数的构造函数
    Register(const std::string& n, RegisterType t, int32 idx) 
        : name(n), type(t), index(idx) {}
};

// 条件分支操作枚举
enum class ConditionOp {
    EQ,  // ==
    NE,  // !=
    LT,  // <
    LE,  // <=
    GT,  // >
    GE,  // >=
    UNKNOWN
};

// 量子门指令结构体
struct QGateInstruction {
    QuantumGateType gate_type;               // 量子门类型
    std::vector<int> target_qubits;          // 目标量子比特
    std::vector<int> control_qubits;         // 控制量子比特
    bool adjoint;                            // 是否为共轭转置
    double angle;                            // 旋转门的角度参数
    
    QGateInstruction() : gate_type(QuantumGateType::UNKNOWN), adjoint(false), angle(0.0) {}
};

// 测量基枚举
enum class MeasureBasis {
    X,  // X基
    Y,  // Y基
    Z   // Z基
};

// 测量指令结构体
struct MeasureInstruction {
    MeasureBasis basis;                       // 测量基
    int32 qubit_index;                         // 被测量的量子比特索引
    int32 measure_reg_index;                   // 测量结果存储的寄存器索引
    
    MeasureInstruction() : basis(MeasureBasis::Z), qubit_index(-1), measure_reg_index(-1) {}
};

// 移动指令结构体
struct MovInstruction {
    int32 dest_reg_index;                      // 目标寄存器索引
    int32 src_reg_index;                       // 源寄存器索引
    int32 const_value;                         // 常量值，仅当src_reg_index为-1时有效
    
    MovInstruction() : dest_reg_index(-1), src_reg_index(-1), const_value(0) {}
};

// 条件分支指令结构体
struct BrCondInstruction {
    int32 reg_index;                           // 比较的寄存器索引
    ConditionOp op;                          // 比较操作
    int32 value;                               // 比较值
    std::string true_label;                  // 条件为真时跳转的标签
    std::string false_label;                 // 条件为假时跳转的标签
    
    BrCondInstruction() : reg_index(-1), op(ConditionOp::UNKNOWN), value(0) {}
};

// 无条件分支指令结构体
struct BrUncondInstruction {
    std::string label;                       // 跳转的标签
    
    BrUncondInstruction() {}
    BrUncondInstruction(const std::string& l) : label(l) {}
};

// 标签指令结构体
struct LabelInstruction {
    std::string name;                        // 标签名称
    
    LabelInstruction() {}
    LabelInstruction(const std::string& n) : name(n) {}
};

// 经典逻辑操作指令结构体
struct AndOperation {
    int32 left_reg;           // 左操作数寄存器索引
    int32 right_reg;          // 右操作数寄存器索引
    int32 output_reg;         // 输出寄存器索引
    
    AndOperation() : left_reg(-1), right_reg(-1), output_reg(-1) {}
};

struct OrOperation {
    int32 left_reg;           // 左操作数寄存器索引
    int32 right_reg;          // 右操作数寄存器索引
    int32 output_reg;         // 输出寄存器索引
    
    OrOperation() : left_reg(-1), right_reg(-1), output_reg(-1) {}
};

struct XorOperation {
    int32 left_reg;           // 左操作数寄存器索引
    int32 right_reg;          // 右操作数寄存器索引
    int32 output_reg;         // 输出寄存器索引
    
    XorOperation() : left_reg(-1), right_reg(-1), output_reg(-1) {}
};

// 布尔函数指令结构体
struct AllOperation {
    std::vector<int32> input_regs;    // 输入寄存器索引列表
    int32 output_reg;                 // 输出寄存器索引
    
    AllOperation() : output_reg(-1) {}
};

struct AnyOperation {
    std::vector<int32> input_regs;    // 输入寄存器索引列表
    int32 output_reg;                 // 输出寄存器索引
    
    AnyOperation() : output_reg(-1) {}
};

// 指令结构体 - 使用variant-like设计，避免union的复制构造函数问题
struct Instruction {
    InstructionType type;                   // 指令类型
    std::string original_line;              // 原始指令行（用于调试）
    
    // 指令数据 - 根据type访问不同的数据
    QGateInstruction qgate;                 // 量子门指令数据
    MeasureInstruction measure;             // 测量指令数据
    MovInstruction mov;                     // 移动指令数据
    AndOperation and_op;                    // 经典与操作数据
    OrOperation or_op;                      // 经典或操作数据
    XorOperation xor_op;                    // 经典异或操作数据
    AllOperation all_op;                    // 所有测量结果为真数据
    AnyOperation any_op;                    // 任一测量结果为真数据
    BrCondInstruction br_cond;              // 条件分支指令数据
    BrUncondInstruction br_uncond;          // 无条件分支指令数据
    LabelInstruction label;                 // 标签指令数据
    
    Instruction() : type(InstructionType::UNKNOWN) {}
    Instruction(InstructionType t) : type(t) {}
    
    // 复制构造函数
    Instruction(const Instruction& other) {
        type = other.type;
        original_line = other.original_line;
        
        // 根据类型复制相应的数据
        switch (type) {
            case InstructionType::QGATE: qgate = other.qgate; break;
            case InstructionType::MEASURE: measure = other.measure; break;
            case InstructionType::MOV: mov = other.mov; break;
            case InstructionType::AND: and_op = other.and_op; break;
            case InstructionType::OR: or_op = other.or_op; break;
            case InstructionType::XOR: xor_op = other.xor_op; break;
            case InstructionType::ALL: all_op = other.all_op; break;
            case InstructionType::ANY: any_op = other.any_op; break;
            case InstructionType::BR_COND: br_cond = other.br_cond; break;
            case InstructionType::BR_UNCOND: br_uncond = other.br_uncond; break;
            case InstructionType::LABEL: label = other.label; break;
            default: break;
        }
    }
    
    // 移动构造函数
    Instruction(Instruction&& other) noexcept {
        type = other.type;
        original_line = std::move(other.original_line);
        
        // 根据类型移动相应的数据
        switch (type) {
            case InstructionType::QGATE: qgate = std::move(other.qgate); break;
            case InstructionType::MEASURE: measure = std::move(other.measure); break;
            case InstructionType::MOV: mov = std::move(other.mov); break;
            case InstructionType::AND: and_op = std::move(other.and_op); break;
            case InstructionType::OR: or_op = std::move(other.or_op); break;
            case InstructionType::XOR: xor_op = std::move(other.xor_op); break;
            case InstructionType::ALL: all_op = std::move(other.all_op); break;
            case InstructionType::ANY: any_op = std::move(other.any_op); break;
            case InstructionType::BR_COND: br_cond = std::move(other.br_cond); break;
            case InstructionType::BR_UNCOND: br_uncond = std::move(other.br_uncond); break;
            case InstructionType::LABEL: label = std::move(other.label); break;
            default: break;
        }
    }
    
    // 复制赋值运算符
    Instruction& operator=(const Instruction& other) {
        if (this != &other) {
            type = other.type;
            original_line = other.original_line;
            
            // 根据类型复制相应的数据
            switch (type) {
                case InstructionType::QGATE: qgate = other.qgate; break;
                case InstructionType::MEASURE: measure = other.measure; break;
                case InstructionType::MOV: mov = other.mov; break;
                case InstructionType::AND: and_op = other.and_op; break;
                case InstructionType::OR: or_op = other.or_op; break;
                case InstructionType::XOR: xor_op = other.xor_op; break;
                case InstructionType::ALL: all_op = other.all_op; break;
                case InstructionType::ANY: any_op = other.any_op; break;
                case InstructionType::BR_COND: br_cond = other.br_cond; break;
                case InstructionType::BR_UNCOND: br_uncond = other.br_uncond; break;
                case InstructionType::LABEL: label = other.label; break;
                default: break;
            }
        }
        return *this;
    }
    
    // 移动赋值运算符
    Instruction& operator=(Instruction&& other) noexcept {
        if (this != &other) {
            type = other.type;
            original_line = std::move(other.original_line);
            
            // 根据类型移动相应的数据
            switch (type) {
                case InstructionType::QGATE: qgate = std::move(other.qgate); break;
                case InstructionType::MEASURE: measure = std::move(other.measure); break;
                case InstructionType::MOV: mov = std::move(other.mov); break;
                case InstructionType::AND: and_op = std::move(other.and_op); break;
                case InstructionType::OR: or_op = std::move(other.or_op); break;
                case InstructionType::XOR: xor_op = std::move(other.xor_op); break;
                case InstructionType::ALL: all_op = std::move(other.all_op); break;
                case InstructionType::ANY: any_op = std::move(other.any_op); break;
                case InstructionType::BR_COND: br_cond = std::move(other.br_cond); break;
                case InstructionType::BR_UNCOND: br_uncond = std::move(other.br_uncond); break;
                case InstructionType::LABEL: label = std::move(other.label); break;
                default: break;
            }
        }
        return *this;
    }
};

// SSA程序结构体，包含所有解析后的信息
struct SSAProgram {
    // 寄存器映射：名称 -> 寄存器信息
    std::map<std::string, Register> registers;
    
    // 寄存器索引映射：索引 -> 寄存器名称
    std::map<int32, std::string> register_index_map;
    
    // 量子寄存器列表
    std::vector<Register> qregs;
    
    // 经典寄存器列表
    std::vector<Register> cregs;
    
    // 测量寄存器列表
    std::vector<Register> mregs;
    
    // 指令列表
    std::vector<Instruction> instructions;
    
    // 标签映射：标签名称 -> 指令索引
    std::map<std::string, int> label_map;
    
    // 重置程序状态
    void clear() {
        registers.clear();
        register_index_map.clear();
        qregs.clear();
        cregs.clear();
        mregs.clear();
        instructions.clear();
        label_map.clear();
    }
};

// SSA汇编解析器类
class SSAParser {
public:
    SSAParser();
    ~SSAParser();
    
    // 从字符串解析SSA汇编
    bool parse(const std::string& ssa_code, SSAProgram& program);
    
    // 从文件解析SSA汇编
    bool parse_file(const std::string& file_path, SSAProgram& program);
    
    // 将SSAProgram生成回ASM代码
    std::string generate_asm(const SSAProgram& program);
    
    // 获取解析错误信息
    const std::string& get_error() const;
    
private:
    // 解析单行指令
    bool parse_line(const std::string& line, SSAProgram& program);
    
    // 解析声明指令
    bool parse_declare(const std::string& line, SSAProgram& program);
    
    // 解析量子门指令
    bool parse_qgate(const std::string& line, SSAProgram& program);
    
    // 解析测量指令
    bool parse_measure(const std::string& line, SSAProgram& program);
    
    // 解析移动指令
    bool parse_mov(const std::string& line, SSAProgram& program);
    
    // 解析常量赋值指令
    bool parse_const(const std::string& line, SSAProgram& program);
    
    // 解析条件分支指令
    bool parse_br_cond(const std::string& line, SSAProgram& program);
    
    // 解析无条件分支指令
    bool parse_br_uncond(const std::string& line, SSAProgram& program);
    
    // 解析标签指令
    bool parse_label(const std::string& line, SSAProgram& program);
    
    // 解析经典与操作指令
    bool parse_and(const std::string& line, SSAProgram& program);
    
    // 解析经典或操作指令
    bool parse_or(const std::string& line, SSAProgram& program);
    
    // 解析经典异或操作指令
    bool parse_xor(const std::string& line, SSAProgram& program);
    
    // 解析all操作指令
    bool parse_all(const std::string& line, SSAProgram& program);
    
    // 解析any操作指令
    bool parse_any(const std::string& line, SSAProgram& program);
    
    // 从寄存器名称提取索引
    int32 parse_register_index(const std::string& reg_name);
    
    
    
    // 将量子门名称转换为枚举类型
    QuantumGateType gate_name_to_type(const std::string& gate_name);
    
    // 将条件操作符转换为枚举类型
    ConditionOp condition_op_to_type(const std::string& op);
    
    // 移除行首行尾的空白字符
    std::string trim(const std::string& str);
    
    // 移除注释
    std::string remove_comment(const std::string& line);
    
    // 错误信息
    std::string error_;
    
    // 当前行号（用于错误定位）
    int32 current_line_;
};

} // namespace ssa
