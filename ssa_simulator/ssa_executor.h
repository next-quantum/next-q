#pragma once

// 调试输出控制宏
#ifdef DEBUG_OUTPUT
    #define DEBUG_PRINT(x) std::cout << x << std::endl
#else
    #define DEBUG_PRINT(x) do { } while (0)
#endif

#include "ssa_parser.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace ssa {

// 前向声明
class IQuantumGateHandler;

// 经典寄存器值联合体，支持多种类型
union RegisterValue {
    int32 int32_value;    // 32位整数
    float32 float32_value;  // 32位浮点数
    
    // 构造函数
    RegisterValue() : int32_value(0) {}
    RegisterValue(int32 v) : int32_value(v) {}
    RegisterValue(float32 v) : float32_value(v) {}
    RegisterValue(bool v) : int32_value(v ? 1 : 0) {}
};

// 执行状态枚举
enum class ExecutionStatus {
    RUNNING,      // 正在运行
    FINISHED,     // 正常完成
    ERROR,        // 执行错误
    INTERRUPTED   // 被中断
};

// 执行上下文结构体
struct ExecutionContext {
    // 经典寄存器值
    std::map<std::string, RegisterValue> creg_values;
    
    // 测量寄存器值
    std::map<std::string, RegisterValue> mreg_values;
    
    // 量子寄存器状态（由量子门处理器管理）
    // void* quantum_state = nullptr;
    
    // 程序计数器
    int pc = 0;
    
    // 执行状态
    ExecutionStatus status = ExecutionStatus::RUNNING;
    
    // 错误信息
    std::string error_msg;
    
    // 重置上下文
    void reset() {
        creg_values.clear();
        mreg_values.clear();
        // quantum_state = nullptr;
        pc = 0;
        status = ExecutionStatus::RUNNING;
        error_msg.clear();
    }
};

// 量子门处理器接口
class IQuantumGateHandler {
public:
    virtual ~IQuantumGateHandler() = default;
    
    // 初始化量子状态
    virtual bool initialize_quantum_state(size_t num_qubits) = 0;
    
    // 释放量子状态
    virtual void release_quantum_state() = 0;

    // 重置量子状态
    virtual bool reset_quantum_state() = 0;
    
    // 执行量子门操作
    virtual bool execute_quantum_gate(
        const QGateInstruction& gate, 
        ExecutionContext& context) = 0;
    
    // 执行测量操作
    virtual bool execute_measurement(
        const MeasureInstruction& measure, 
        ExecutionContext& context) = 0;
};

// 后端类型枚举
enum class BackendType {
    CPU,
    BirenGPU
};

// SSA执行引擎类
class SSAExecutor {
public:
    SSAExecutor(BackendType backend = BackendType::CPU);
    ~SSAExecutor();
    
    // 设置量子门处理器
    void set_quantum_gate_handler(std::shared_ptr<IQuantumGateHandler> handler);
    
    // 加载SSA程序
    bool load_program(const SSAProgram& program);
    
    // 从字符串加载SSA汇编
    bool load_assembly(const std::string& assembly_code);
    
    // 从文件加载SSA汇编
    bool load_assembly_file(const std::string& file_path);
    
    // 运行SSA程序
    bool run();
    
    // 单步执行
    bool step();
    
    // 重置执行状态，但保留程序
    void reset_state();
    
    // 重置执行状态，包括清除程序
    void reset();
    
    // 获取当前执行上下文
    const ExecutionContext& get_context() const;
    
    // 获取经典寄存器值
    RegisterValue get_classical_reg_value(const std::string& reg_name) const;
    
    // 获取测量寄存器值
    RegisterValue get_measurement_reg_value(const std::string& reg_name) const;
    
    // 设置经典寄存器值
    void set_classical_reg_value(const std::string& reg_name, RegisterValue value);
    
    // 设置测量寄存器值
    void set_measurement_reg_value(const std::string& reg_name, RegisterValue value);
    
    // 获取执行错误信息
    const std::string& get_error() const;
    
    // 获取执行状态
    ExecutionStatus get_status() const;
    
    // 获取所有测量寄存器名称
    std::vector<std::string> get_measurement_reg_names() const;
    
private:
    // 执行单个指令
    bool execute_instruction(const Instruction& instr, ExecutionContext& context);
    
    // 处理声明指令
    bool handle_declare(const Instruction& instr, ExecutionContext& context);
    
    // 处理量子门指令
    bool handle_qgate(const Instruction& instr, ExecutionContext& context);
    
    // 处理测量指令
    bool handle_measure(const Instruction& instr, ExecutionContext& context);
    
    // 处理移动指令
    bool handle_mov(const Instruction& instr, ExecutionContext& context);
    
    // 处理AND操作
    bool handle_and(const Instruction& instr, ExecutionContext& context);
    
    // 处理OR操作
    bool handle_or(const Instruction& instr, ExecutionContext& context);
    
    // 处理XOR操作
    bool handle_xor(const Instruction& instr, ExecutionContext& context);
    
    // 处理ALL操作
    bool handle_all(const Instruction& instr, ExecutionContext& context);
    
    // 处理ANY操作
    bool handle_any(const Instruction& instr, ExecutionContext& context);
    
    // 处理条件分支
    bool handle_br_cond(const Instruction& instr, ExecutionContext& context);
    
    // 处理无条件分支
    bool handle_br_uncond(const Instruction& instr, ExecutionContext& context);
    
    // 比较两个寄存器值
    bool compare_values(RegisterValue left, ConditionOp op, RegisterValue right, bool is_float);
    
    // 解析指令类型后缀，判断是否为浮点数指令
    bool is_float_instruction(const std::string& original_line);
    
    // 获取寄存器名称
    std::string get_reg_name(int reg_index, const std::string& reg_type_prefix);
    
    // 私有辅助方法：释放量子状态
    void release_quantum_state();
    
private:
    // SSA程序
    SSAProgram program_;
    
    // 执行上下文
    ExecutionContext context_;
    
    // 量子门处理器
    std::shared_ptr<IQuantumGateHandler> quantum_gate_handler_;
    
    // 解析器
    SSAParser parser_;
    
    // 错误信息
    std::string error_;
    
    // 指令类型到处理函数的映射
    using InstructionHandler = bool (SSAExecutor::*)(const Instruction&, ExecutionContext&);
    std::map<InstructionType, InstructionHandler> instruction_handlers_;
};

// 默认量子门处理器声明
class DefaultQuantumGateHandler : public IQuantumGateHandler {
private:
    // static bool is_initialized_; // 静态初始化标志，防止多次初始化
    // static bool qubits_allocated_; // 静态标志，标记量子位是否已经分配
    // static size_t last_allocated_qubits_; // 静态变量，记录最后一次分配的量子位数量
    size_t num_qubits_allocated_{0}; // 记录当前分配的量子位数量
    
public:
    DefaultQuantumGateHandler();
    ~DefaultQuantumGateHandler();
    
    bool initialize_quantum_state(size_t num_qubits) override;
    void release_quantum_state() override;
    bool reset_quantum_state() override;
    
    bool execute_quantum_gate(
        const QGateInstruction& gate, 
        ExecutionContext& context) override;
    
    bool execute_measurement(
        const MeasureInstruction& measure, 
        ExecutionContext& context) override;
};

} // namespace ssa
