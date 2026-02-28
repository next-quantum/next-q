#include <cctype>
#include <cstdlib>

#include <algorithm>
#include <sstream>
#include <iostream>

// Compile-time constant to control which quantum runtime interface to use
// Define as 1 to use qc_runtime_v2.h, 0 to use qc_runtime.h
#define USE_QC_RUNTIME_V2 1

#include "ssa_executor.h"

#if USE_QC_RUNTIME_V2
#include "qc_runtime/qc_runtime_v2.h"
#else
#include "qc_runtime/qc_runtime.h"
#endif

namespace ssa {

SSAExecutor::SSAExecutor() {
    DEBUG_PRINT("[DEBUG] SSAExecutor::SSAExecutor() called");
    
    // 初始化随机数种子
    srand(static_cast<unsigned int>(time(nullptr)));
    
    // 初始化指令处理函数映射
    instruction_handlers_ = {
        {InstructionType::DECLARE, &SSAExecutor::handle_declare},
        {InstructionType::QGATE, &SSAExecutor::handle_qgate},
        {InstructionType::MEASURE, &SSAExecutor::handle_measure},
        {InstructionType::MOV, &SSAExecutor::handle_mov},
        {InstructionType::AND, &SSAExecutor::handle_and},
        {InstructionType::OR, &SSAExecutor::handle_or},
        {InstructionType::XOR, &SSAExecutor::handle_xor},
        {InstructionType::ALL, &SSAExecutor::handle_all},
        {InstructionType::ANY, &SSAExecutor::handle_any},
        {InstructionType::BR_COND, &SSAExecutor::handle_br_cond},
        {InstructionType::BR_UNCOND, &SSAExecutor::handle_br_uncond}
    };
    
    // 设置默认量子门处理器
    quantum_gate_handler_ = std::make_shared<DefaultQuantumGateHandler>();
    DEBUG_PRINT("[DEBUG] SSAExecutor::SSAExecutor() completed");
}

SSAExecutor::~SSAExecutor() {
    DEBUG_PRINT("[DEBUG] SSAExecutor::~SSAExecutor() called");
    // 只调用reset()，因为reset()已经会调用release_quantum_state()
    reset();
    DEBUG_PRINT("[DEBUG] SSAExecutor::~SSAExecutor() completed");
}

void SSAExecutor::set_quantum_gate_handler(std::shared_ptr<IQuantumGateHandler> handler) {
    quantum_gate_handler_ = handler;
}

bool SSAExecutor::load_program(const SSAProgram& program) {
    DEBUG_PRINT("[DEBUG] SSAExecutor::load_program() called");
    
    // 重置状态
    reset();
    
    // 复制程序
    program_ = program;
    
    DEBUG_PRINT("[DEBUG] Loaded program with:");
    DEBUG_PRINT("[DEBUG]   - " << program_.instructions.size() << " instructions");
    DEBUG_PRINT("[DEBUG]   - " << program_.qregs.size() << " quantum registers");
    DEBUG_PRINT("[DEBUG]   - " << program_.cregs.size() << " classical registers");
    DEBUG_PRINT("[DEBUG]   - " << program_.mregs.size() << " measure registers");
    
    // 初始化寄存器值
    for (const auto& creg : program_.cregs) {
        context_.creg_values[creg.name] = RegisterValue(0);
        DEBUG_PRINT("[DEBUG]   Initialized classical register: " << creg.name);
    }
    
    for (const auto& mreg : program_.mregs) {
        context_.mreg_values[mreg.name] = RegisterValue(0);
        DEBUG_PRINT("[DEBUG]   Initialized measure register: " << mreg.name);
    }
    
    // 初始化量子状态
    if (!program_.qregs.empty()) {
        DEBUG_PRINT("[DEBUG]   Initializing quantum state for " << program_.qregs.size() << " qubits...");
        if (!quantum_gate_handler_->initialize_quantum_state(program_.qregs.size())) {
            error_ = "Failed to initialize quantum state";
            DEBUG_PRINT("[DEBUG]   Failed to initialize quantum state");
            return false;
        }
        DEBUG_PRINT("[DEBUG]   Quantum state initialized successfully");
    }
    
    DEBUG_PRINT("[DEBUG] SSAExecutor::load_program() completed successfully");
    return true;
}

bool SSAExecutor::load_assembly(const std::string& assembly_code) {
    // 解析SSA汇编代码
    SSAProgram program;
    if (!parser_.parse(assembly_code, program)) {
        error_ = parser_.get_error();
        return false;
    }
    
    // 加载解析后的程序
    return load_program(program);
}

bool SSAExecutor::load_assembly_file(const std::string& file_path) {
    // 从文件解析SSA汇编代码
    SSAProgram program;
    if (!parser_.parse_file(file_path, program)) {
        error_ = parser_.get_error();
        return false;
    }
    
    // 加载解析后的程序
    return load_program(program);
}

bool SSAExecutor::run() {
    // 检查程序是否已加载
    if (program_.instructions.empty()) {
        error_ = "No SSA program loaded";
        return false;
    }
    
    // 检查量子门处理器是否已设置
    if (!quantum_gate_handler_) {
        error_ = "No quantum gate handler set";
        return false;
    }
    
    // 手动重置执行上下文的部分字段，保留quantum_state指针
    // 不调用context_.reset()，因为它会将quantum_state置空
    context_.pc = 0;
    context_.status = ExecutionStatus::RUNNING;
    context_.error_msg.clear();
    context_.creg_values.clear();
    context_.mreg_values.clear();
    
    // 假设量子状态已经在load_program()方法中成功初始化
    // 不进行任何量子状态的重新初始化，避免重复初始化错误
    
    // 执行指令直到完成或出错
    while (context_.status == ExecutionStatus::RUNNING) {
        if (context_.pc >= static_cast<int>(program_.instructions.size())) {
            // 所有指令执行完毕
            context_.status = ExecutionStatus::FINISHED;
            break;
        }
        
        // 执行当前指令
        const Instruction& instr = program_.instructions[context_.pc];
        if (!execute_instruction(instr, context_)) {
            context_.status = ExecutionStatus::ERROR;
            error_ = context_.error_msg;
            return false;
        }
    }
    
    return context_.status == ExecutionStatus::FINISHED;
}

bool SSAExecutor::step() {
    // 检查程序是否已加载
    if (program_.instructions.empty()) {
        error_ = "No SSA program loaded";
        return false;
    }
    
    // 检查执行状态
    if (context_.status == ExecutionStatus::FINISHED) {
        error_ = "Execution already finished";
        return false;
    }
    
    if (context_.status == ExecutionStatus::ERROR) {
        error_ = context_.error_msg;
        return false;
    }
    
    // 检查PC是否越界
    if (context_.pc >= static_cast<int>(program_.instructions.size())) {
        context_.status = ExecutionStatus::FINISHED;
        return true;
    }
    
    // 执行当前指令
    const Instruction& instr = program_.instructions[context_.pc];
    bool result = execute_instruction(instr, context_);
    
    // 如果执行出错，更新状态
    if (!result) {
        context_.status = ExecutionStatus::ERROR;
        error_ = context_.error_msg;
    }
    
    // 检查是否执行完毕
    if (context_.pc >= static_cast<int>(program_.instructions.size())) {
        context_.status = ExecutionStatus::FINISHED;
    }
    
    return result;
}

void SSAExecutor::reset_state() {
    // 重置错误信息
    error_.clear();
    
    // 手动重置执行上下文的部分字段
    context_.reset();
    
    // 重新初始化量子状态（如果有量子寄存器）
    // 这样可以确保每次执行前都释放并重新分配量子比特
    // 符合用户要求：每执行完一遍量子电路，结束时release所有量子比特，开始时allocateQubit
    if (!program_.qregs.empty() && quantum_gate_handler_) {
        if (!quantum_gate_handler_->reset_quantum_state()) {
            error_ = "Failed to reset quantum state";
            return;
        }
    }
}

void SSAExecutor::reset() {
    // 释放量子状态
    release_quantum_state();
    
    // 重置执行上下文
    context_.reset();
    
    // 重置错误信息
    error_.clear();
    
    // 重置程序
    program_.clear();
}

// 私有辅助方法：释放量子状态
void SSAExecutor::release_quantum_state() {
    // 对于qc_runtime，不需要显式释放量子状态，因为它在内部管理
    // 直接将指针置空即可
    if (quantum_gate_handler_) {
        quantum_gate_handler_->release_quantum_state();
    }
}

const ExecutionContext& SSAExecutor::get_context() const {
    return context_;
}

RegisterValue SSAExecutor::get_classical_reg_value(const std::string& reg_name) const {
    auto it = context_.creg_values.find(reg_name);
    if (it != context_.creg_values.end()) {
        return it->second;
    }
    return RegisterValue(0);
}

RegisterValue SSAExecutor::get_measurement_reg_value(const std::string& reg_name) const {
    auto it = context_.mreg_values.find(reg_name);
    if (it != context_.mreg_values.end()) {
        return it->second;
    }
    return RegisterValue(0);
}

void SSAExecutor::set_classical_reg_value(const std::string& reg_name, RegisterValue value) {
    context_.creg_values[reg_name] = value;
}

void SSAExecutor::set_measurement_reg_value(const std::string& reg_name, RegisterValue value) {
    context_.mreg_values[reg_name] = value;
}

const std::string& SSAExecutor::get_error() const {
    return error_;
}

ExecutionStatus SSAExecutor::get_status() const {
    return context_.status;
}

std::vector<std::string> SSAExecutor::get_measurement_reg_names() const {
    std::vector<std::string> mreg_names;
    for (const auto& mreg : program_.mregs) {
        mreg_names.push_back(mreg.name);
    }
    return mreg_names;
}

bool SSAExecutor::execute_instruction(const Instruction& instr, ExecutionContext& context) {
    // 打印指令执行信息，用于调试
    DEBUG_PRINT("[DEBUG] Executing instruction at PC=" << context.pc << ": " << instr.original_line);
    
    // 检查指令类型是否有对应的处理函数
    auto handler_it = instruction_handlers_.find(instr.type);
    if (handler_it == instruction_handlers_.end()) {
        // 未知指令类型，跳过
        DEBUG_PRINT("[DEBUG] Unknown instruction type, skipping");
        context.pc++;
        return true;
    }
    
    // 调用对应的处理函数
    InstructionHandler handler = handler_it->second;
    bool result = (this->*handler)(instr, context);
    
    // 打印指令执行结果
    DEBUG_PRINT("[DEBUG] Instruction executed, result: " << (result ? "SUCCESS" : "FAILURE"));
    if (!result) {
        DEBUG_PRINT("[DEBUG] Error message: " << context.error_msg);
    }
    
    return result;
}

bool SSAExecutor::handle_declare([[maybe_unused]] const Instruction& instr, ExecutionContext& context) {
    // 声明指令在加载时已经处理，执行时跳过
    context.pc++;
    return true;
}

bool SSAExecutor::handle_qgate(const Instruction& instr, ExecutionContext& context) {
    // 检查量子门处理器是否已设置
    if (!quantum_gate_handler_) {
        context.error_msg = "No quantum gate handler set";
        return false;
    }
    
    // 复制量子门指令，以便可能修改目标量子比特
    QGateInstruction gate = instr.qgate;
    
    // 检查是否为动态门操作
    if (instr.original_line.find("dynamic=") != std::string::npos) {
        // 提取动态参数，例如：dynamic=c1
        size_t dynamic_pos = instr.original_line.find("dynamic=");
        std::string dynamic_part = instr.original_line.substr(dynamic_pos + 8);
        
        // 移除可能的空格和注释
        size_t space_pos = dynamic_part.find(" ");
        if (space_pos != std::string::npos) {
            dynamic_part = dynamic_part.substr(0, space_pos);
        }
        
        // 移除可能的逗号
        if (!dynamic_part.empty() && dynamic_part.back() == ',') {
            dynamic_part.pop_back();
        }
        
        // 获取经典寄存器中的动态值
        RegisterValue dynamic_value = context.creg_values[dynamic_part];
        
        // 将动态值作为目标量子比特索引
        gate.target_qubits.push_back(dynamic_value.int32_value);
    }
    
    // 调用量子门处理器执行量子门操作
    if (!quantum_gate_handler_->execute_quantum_gate(gate, context)) {
        context.error_msg = "Failed to execute quantum gate";
        return false;
    }
    
    // 继续执行下一条指令
    context.pc++;
    return true;
}

bool SSAExecutor::handle_measure(const Instruction& instr, ExecutionContext& context) {
    // 检查量子门处理器是否已设置
    if (!quantum_gate_handler_) {
        context.error_msg = "No quantum gate handler set";
        return false;
    }
    
    // 调用量子门处理器执行测量操作
    if (!quantum_gate_handler_->execute_measurement(instr.measure, context)) {
        context.error_msg = "Failed to execute measurement";
        return false;
    }
    
    // 继续执行下一条指令
    context.pc++;
    return true;
}

bool SSAExecutor::handle_mov(const Instruction& instr, ExecutionContext& context) {
    // 获取目标寄存器和源寄存器名称
    std::string dest_reg_name = get_reg_name(instr.mov.dest_reg_index, "c");
    
    // 检查是否为常量赋值（src_reg_index为-1）
    if (instr.mov.src_reg_index == -1) {
        // 常量赋值，使用保存的常量值
        context.creg_values[dest_reg_name] = RegisterValue(instr.mov.const_value);
    } else {
        std::string src_reg_name = get_reg_name(instr.mov.src_reg_index, "");
        
        // 检查源寄存器类型
        if (src_reg_name[0] == 'c') {
            // 从经典寄存器复制
            auto it = context.creg_values.find(src_reg_name);
            if (it != context.creg_values.end()) {
                context.creg_values[dest_reg_name] = it->second;
            }
        } else if (src_reg_name[0] == 'm') {
            // 从测量寄存器复制
            auto it = context.mreg_values.find(src_reg_name);
            if (it != context.mreg_values.end()) {
                context.creg_values[dest_reg_name] = it->second;
            }
        } else {
            // 未知寄存器类型
            context.error_msg = "Unknown register type in MOV instruction";
            return false;
        }
    }
    
    // 继续执行下一条指令
    context.pc++;
    return true;
}

bool SSAExecutor::handle_and(const Instruction& instr, ExecutionContext& context) {
    // 获取寄存器名称
    std::string output_reg = get_reg_name(instr.and_op.output_reg, "c");
    std::string left_reg = get_reg_name(instr.and_op.left_reg, "c");
    std::string right_reg = get_reg_name(instr.and_op.right_reg, "c");
    
    // 执行AND操作
    auto left_it = context.creg_values.find(left_reg);
    auto right_it = context.creg_values.find(right_reg);
    
    if (left_it != context.creg_values.end() && right_it != context.creg_values.end()) {
        // 执行AND操作，使用整数表示
        int32 result = left_it->second.int32_value & right_it->second.int32_value;
        context.creg_values[output_reg] = RegisterValue(result);
    }
    
    // 继续执行下一条指令
    context.pc++;
    return true;
}

bool SSAExecutor::handle_or(const Instruction& instr, ExecutionContext& context) {
    // 获取寄存器名称
    std::string output_reg = get_reg_name(instr.or_op.output_reg, "c");
    std::string left_reg = get_reg_name(instr.or_op.left_reg, "c");
    std::string right_reg = get_reg_name(instr.or_op.right_reg, "c");
    
    // 执行OR操作
    auto left_it = context.creg_values.find(left_reg);
    auto right_it = context.creg_values.find(right_reg);
    
    if (left_it != context.creg_values.end() && right_it != context.creg_values.end()) {
        // 执行OR操作，使用整数表示
        int32 result = left_it->second.int32_value | right_it->second.int32_value;
        context.creg_values[output_reg] = RegisterValue(result);
    }
    
    // 继续执行下一条指令
    context.pc++;
    return true;
}

bool SSAExecutor::handle_xor(const Instruction& instr, ExecutionContext& context) {
    // 获取寄存器名称
    std::string output_reg = get_reg_name(instr.xor_op.output_reg, "c");
    std::string left_reg = get_reg_name(instr.xor_op.left_reg, "c");
    std::string right_reg = get_reg_name(instr.xor_op.right_reg, "c");
    
    // 执行XOR操作
    auto left_it = context.creg_values.find(left_reg);
    auto right_it = context.creg_values.find(right_reg);
    
    if (left_it != context.creg_values.end() && right_it != context.creg_values.end()) {
        // 执行XOR操作，使用整数表示
        int32 result = left_it->second.int32_value ^ right_it->second.int32_value;
        context.creg_values[output_reg] = RegisterValue(result);
    }
    
    // 继续执行下一条指令
    context.pc++;
    return true;
}

bool SSAExecutor::handle_all(const Instruction& instr, ExecutionContext& context) {
    // 获取输出寄存器名称
    std::string output_reg = get_reg_name(instr.all_op.output_reg, "c");
    
    // 检查所有输入寄存器是否都为1
    bool all_true = true;
    for (int reg_index : instr.all_op.input_regs) {
        std::string reg_name = get_reg_name(reg_index, "c");
        auto it = context.creg_values.find(reg_name);
        
        if (it == context.creg_values.end() || it->second.int32_value != 1) {
            all_true = false;
            break;
        }
    }
    
    // 设置输出寄存器值
    context.creg_values[output_reg] = RegisterValue(all_true);
    
    // 继续执行下一条指令
    context.pc++;
    return true;
}

bool SSAExecutor::handle_any(const Instruction& instr, ExecutionContext& context) {
    // 获取输出寄存器名称
    std::string output_reg = get_reg_name(instr.any_op.output_reg, "c");
    
    // 检查是否有任何输入寄存器为1
    bool any_true = false;
    for (int reg_index : instr.any_op.input_regs) {
        std::string reg_name = get_reg_name(reg_index, "c");
        auto it = context.creg_values.find(reg_name);
        
        if (it != context.creg_values.end() && it->second.int32_value == 1) {
            any_true = true;
            break;
        }
    }
    
    // 设置输出寄存器值
    context.creg_values[output_reg] = RegisterValue(any_true);
    
    // 继续执行下一条指令
    context.pc++;
    return true;
}

bool SSAExecutor::handle_br_cond(const Instruction& instr, ExecutionContext& context) {
    // 获取寄存器名称
    std::string reg_name = get_reg_name(instr.br_cond.reg_index, "c");
    
    // 获取寄存器值
    auto reg_it = context.creg_values.find(reg_name);
    if (reg_it == context.creg_values.end()) {
        context.error_msg = "Unknown register in conditional branch";
        return false;
    }
    
    // 检查指令是否为浮点数指令
    bool is_float = is_float_instruction(instr.original_line);
    
    // 执行比较操作
    RegisterValue reg_value = reg_it->second;
    RegisterValue compare_value(instr.br_cond.value);
    
    bool condition_met = compare_values(reg_value, instr.br_cond.op, compare_value, is_float);
    
    // 根据比较结果跳转到相应标签
    std::string target_label = condition_met ? instr.br_cond.true_label : instr.br_cond.false_label;
    
    // 查找标签对应的指令索引
    auto label_it = program_.label_map.find(target_label);
    if (label_it == program_.label_map.end()) {
        context.error_msg = "Unknown label in branch instruction";
        return false;
    }
    
    // 更新程序计数器
    context.pc = label_it->second;
    return true;
}

bool SSAExecutor::handle_br_uncond(const Instruction& instr, ExecutionContext& context) {
    // 查找标签对应的指令索引
    auto label_it = program_.label_map.find(instr.br_uncond.label);
    if (label_it == program_.label_map.end()) {
        context.error_msg = "Unknown label in unconditional branch";
        return false;
    }
    
    // 更新程序计数器
    context.pc = label_it->second;
    return true;
}

bool SSAExecutor::compare_values(RegisterValue left, ConditionOp op, RegisterValue right, bool is_float) {
    if (is_float) {
        // 浮点数比较
        float32 left_val = left.float32_value;
        float32 right_val = right.float32_value;
        
        switch (op) {
            case ConditionOp::EQ: return left_val == right_val;
            case ConditionOp::NE: return left_val != right_val;
            case ConditionOp::LT: return left_val < right_val;
            case ConditionOp::LE: return left_val <= right_val;
            case ConditionOp::GT: return left_val > right_val;
            case ConditionOp::GE: return left_val >= right_val;
            default: return false;
        }
    } else {
        // 整数比较
        int32 left_val = left.int32_value;
        int32 right_val = right.int32_value;
        
        switch (op) {
            case ConditionOp::EQ: return left_val == right_val;
            case ConditionOp::NE: return left_val != right_val;
            case ConditionOp::LT: return left_val < right_val;
            case ConditionOp::LE: return left_val <= right_val;
            case ConditionOp::GT: return left_val > right_val;
            case ConditionOp::GE: return left_val >= right_val;
            default: return false;
        }
    }
}

bool SSAExecutor::is_float_instruction(const std::string& original_line) {
    // 检查指令是否包含.float32后缀
    std::string instr_line = original_line;
    // 转换为小写以便比较
    std::transform(instr_line.begin(), instr_line.end(), instr_line.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    return instr_line.find(".float32") != std::string::npos;
}

std::string SSAExecutor::get_reg_name(int reg_index, const std::string& reg_type_prefix) {
    // 如果提供了寄存器类型前缀，直接生成寄存器名称
    if (!reg_type_prefix.empty()) {
        return reg_type_prefix + std::to_string(reg_index);
    }
    
    // 否则，尝试从索引映射中查找
    auto it = program_.register_index_map.find(reg_index);
    if (it != program_.register_index_map.end()) {
        return it->second;
    }
    
    // 如果找不到，默认返回经典寄存器名称
    return "c" + std::to_string(reg_index);
}

// 默认量子门处理器实现，使用qc_runtime进行实际量子计算
DefaultQuantumGateHandler::DefaultQuantumGateHandler() {
    DEBUG_PRINT("DefaultQuantumGateHandler::DefaultQuantumGateHandler() called");
}    

DefaultQuantumGateHandler::~DefaultQuantumGateHandler() {
    DEBUG_PRINT("DefaultQuantumGateHandler::~DefaultQuantumGateHandler() called");
}    

bool DefaultQuantumGateHandler::initialize_quantum_state(size_t num_qubits) {
    DEBUG_PRINT("DefaultQuantumGateHandler::initialize_quantum_state() called, num_qubits: " << num_qubits);
    
    // 只初始化一次qc_runtime
    // if (!is_initialized_) {
    try {
        DEBUG_PRINT("  Initializing qc_runtime...");
        #if USE_QC_RUNTIME_V2
        DEBUG_PRINT("  [qc_runtime] Calling initWithQubitSize_v2()");
        initWithQubitSize_v2(num_qubits);
        #else
        DEBUG_PRINT("  [qc_runtime] Calling init()");
        init();
        #endif

        unsigned int rand_seed = static_cast<unsigned int>(rand());

        #if USE_QC_RUNTIME_V2
        DEBUG_PRINT("  [qc_runtime] Calling seed_v2(" << rand_seed << ")");
        seed_v2(rand_seed);
        #else
        DEBUG_PRINT("  [qc_runtime] Calling seed(" << rand_seed << ")");
        seed(rand_seed);
        #endif
        DEBUG_PRINT("  qc_runtime initialized successfully");
    } catch (...) {
        std::cerr << "Error: Failed to initialize qc_runtime" << std::endl;
        return false;
    }
    
    try {
        // 只有当量子位未分配时才尝试分配
        // 分配量子位，这会将它们重置到|0⟩状态

        #if USE_QC_RUNTIME_V2
        // nothing
        #else
        for (size_t i = 0; i < num_qubits; ++i) {
            DEBUG_PRINT("  [qc_runtime] Calling allocateQubit(" << i << ")");
            allocateQubit(static_cast<unsigned int>(i));
        }
        #endif
        num_qubits_allocated_ = num_qubits;
    } catch (...) {
        std::cerr << "Error: Failed to allocate qubits" << std::endl;
        return false;
    }
    
    // 设置一个非空指针，让SSAExecutor知道量子状态已经初始化
    DEBUG_PRINT("DefaultQuantumGateHandler::initialize_quantum_state() returning true");
    return true;
}

void DefaultQuantumGateHandler::release_quantum_state() {
    DEBUG_PRINT("DefaultQuantumGateHandler::release_quantum_state() called");
    
    // 如果没有分配量子位，直接返回
    if (num_qubits_allocated_ == 0) {
        DEBUG_PRINT("  No qubits allocated, returning");
        return;
    }
    
    // release all qubits
    try {
        #if USE_QC_RUNTIME_V2
        DEBUG_PRINT("  [qc_runtime] Calling release_v2()");
        // [BUG 2026.2.7 9:33] 忘记释放
        release_v2();
        #else
        for (size_t i = 0; i < num_qubits_allocated_; ++i) {
            DEBUG_PRINT("  [qc_runtime] Calling release(" << i << ")");
            release(static_cast<unsigned int>(i));
        }
        #endif
        // 重置状态以避免后续问题
        num_qubits_allocated_ = 0;
    } catch (...) {
        // 忽略释放错误
        DEBUG_PRINT("  [qc_runtime] Warning: Failed to release some qubits");
        // 重置状态以避免后续问题
        num_qubits_allocated_ = 0;
    }
}

bool DefaultQuantumGateHandler::reset_quantum_state() {
    DEBUG_PRINT("DefaultQuantumGateHandler::reset_quantum_state() called");
    
    try {
        // 重置量子状态
        #if USE_QC_RUNTIME_V2
        DEBUG_PRINT("  [qc_runtime] Calling resetToZeroState_v2()");
        resetToZeroState_v2();
        #else
        DEBUG_PRINT("  [qc_runtime] Calling resetToZeroState()");
        resetToZeroState();
        #endif
        DEBUG_PRINT("  qc_runtime quantum state reset successfully");
        return true;
    } catch (...) {
        std::cerr << "Error: Failed to reset qc_runtime quantum state" << std::endl;
        return false;
    }
}

bool DefaultQuantumGateHandler::execute_quantum_gate(
    const QGateInstruction& gate, ExecutionContext& context) {

    DEBUG_PRINT("DefaultQuantumGateHandler::execute_quantum_gate() called");
    
    // 确保有目标量子位
    if (gate.target_qubits.empty()) {
        std::cerr << "Error: No target qubits specified for quantum gate" << std::endl;
        context.error_msg = "No target qubits specified for quantum gate";
        return false;
    }
    
    DEBUG_PRINT("  Gate type: " << static_cast<int>(gate.gate_type));
    
    std::string target_qubits_str = "  Target qubits: ";
    for (auto q : gate.target_qubits) {
        target_qubits_str += std::to_string(q) + " ";
    }
    DEBUG_PRINT(target_qubits_str);
    
    std::string control_qubits_str = "  Control qubits: ";
    for (auto q : gate.control_qubits) {
        control_qubits_str += std::to_string(q) + " ";
    }
    DEBUG_PRINT(control_qubits_str);
    
    // 处理SWAP门（双量子比特门）
    if (gate.gate_type == QuantumGateType::SWAP) {
        if (gate.target_qubits.size() < 2) {
            std::cerr << "Error: SWAP gate requires at least 2 target qubits" << std::endl;
            context.error_msg = "SWAP gate requires at least 2 target qubits";
            return false;
        }
        
        unsigned int qubit1 = static_cast<unsigned int>(gate.target_qubits[0]);
        unsigned int qubit2 = static_cast<unsigned int>(gate.target_qubits[1]);
        
        if (gate.control_qubits.empty()) {
            // 普通SWAP门
            #if USE_QC_RUNTIME_V2
            DEBUG_PRINT("  [qc_runtime] Calling SWAP_v2(" << qubit1 << ", " << qubit2 << ")");
            SWAP_v2(qubit1, qubit2);
            #else
            DEBUG_PRINT("  [qc_runtime] Calling SWAP(" << qubit1 << ", " << qubit2 << ")");
            SWAP(qubit1, qubit2);
            #endif
        } else {
            // 受控SWAP门
            unsigned int num_controls = static_cast<unsigned int>(gate.control_qubits.size());
            unsigned int* controls = new unsigned int[num_controls];
            for (size_t i = 0; i < gate.control_qubits.size(); ++i) {
                controls[i] = static_cast<unsigned int>(gate.control_qubits[i]);
            }
            
            // 构建控制 qubit 字符串用于调试输出
            std::stringstream controls_str;
            controls_str << "[";
            for (size_t i = 0; i < gate.control_qubits.size(); ++i) {
                controls_str << gate.control_qubits[i];
                if (i < gate.control_qubits.size() - 1) {
                    controls_str << ", ";
                }
            }
            controls_str << "]";
            
            #if USE_QC_RUNTIME_V2
            DEBUG_PRINT("  [qc_runtime] Calling MCSWAP_v2(" << num_controls << ", " << controls_str.str() << ", " << qubit1 << ", " << qubit2 << ")");
            MCSWAP_v2(num_controls, controls, qubit1, qubit2);
            #else
            DEBUG_PRINT("  [qc_runtime] Calling MCSWAP(" << num_controls << ", " << controls_str.str() << ", " << qubit1 << ", " << qubit2 << ")");
            MCSWAP(num_controls, controls, qubit1, qubit2);
            #endif
            
            delete[] controls;
        }
        
        DEBUG_PRINT("DefaultQuantumGateHandler::execute_quantum_gate() returning true");
        return true;
    }
    
    // 实际执行量子门操作
    unsigned int target_qubit = static_cast<unsigned int>(gate.target_qubits[0]);
    
    if (gate.control_qubits.empty()) {
        // 单量子门
        switch (gate.gate_type) {
            case QuantumGateType::X:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling X_v2(" << target_qubit << ")");
                X_v2(target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling X(" << target_qubit << ")");
                X(target_qubit);
                #endif
                break;
            case QuantumGateType::Y:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling Y_v2(" << target_qubit << ")");
                Y_v2(target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling Y(" << target_qubit << ")");
                Y(target_qubit);
                #endif
                break;
            case QuantumGateType::Z:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling Z_v2(" << target_qubit << ")");
                Z_v2(target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling Z(" << target_qubit << ")");
                Z(target_qubit);
                #endif
                break;
            case QuantumGateType::H:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling H_v2(" << target_qubit << ")");
                H_v2(target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling H(" << target_qubit << ")");
                H(target_qubit);
                #endif
                break;
            case QuantumGateType::S:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling S_v2(" << target_qubit << ")");
                S_v2(target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling S(" << target_qubit << ")");
                S(target_qubit);
                #endif
                break;
            case QuantumGateType::T:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling T_v2(" << target_qubit << ")");
                T_v2(target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling T(" << target_qubit << ")");
                T(target_qubit);
                #endif
                break;
            case QuantumGateType::ADJS:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling AdjS_v2(" << target_qubit << ")");
                AdjS_v2(target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling AdjS(" << target_qubit << ")");
                AdjS(target_qubit);
                #endif
                break;
            case QuantumGateType::ADJT:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling AdjT_v2(" << target_qubit << ")");
                AdjT_v2(target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling AdjT(" << target_qubit << ")");
                AdjT(target_qubit);
                #endif
                break;
            case QuantumGateType::RX:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling RX_v2(" << target_qubit << ", " << gate.angle << ")");
                RX_v2(gate.angle, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling RX(" << target_qubit << ", " << gate.angle << ")");
                RX(target_qubit, gate.angle);
                #endif
                break;
            case QuantumGateType::RY:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling RY_v2(" << target_qubit << ", " << gate.angle << ")");
                RY_v2(gate.angle, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling RY(" << target_qubit << ", " << gate.angle << ")");
                RY(target_qubit, gate.angle);
                #endif
                break;
            case QuantumGateType::RZ:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling RZ_v2(" << target_qubit << ", " << gate.angle << ")");
                RZ_v2(gate.angle, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling RZ(" << target_qubit << ", " << gate.angle << ")");
                RZ(target_qubit, gate.angle);
                #endif
                break;
            case QuantumGateType::R1:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling R1_v2(" << target_qubit << ", " << gate.angle << ")");
                R1_v2(gate.angle, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling R1(" << target_qubit << ", " << gate.angle << ")");
                R1(gate.angle, target_qubit);
                #endif
                break;
            case QuantumGateType::RESET:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling reset_v2(" << target_qubit << ")");
                reset_v2(target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling reset(" << target_qubit << ")");
                reset(target_qubit);
                #endif
                break;
            default:
                std::cerr << "Error: Unknown single-qubit gate type" << std::endl;
                context.error_msg = "Unknown single-qubit gate type";
                return false;
        }
    } else {
        // 控制门
        unsigned int num_controls = static_cast<unsigned int>(gate.control_qubits.size());
        unsigned int* controls = new unsigned int[num_controls];
        for (size_t i = 0; i < gate.control_qubits.size(); ++i) {
            controls[i] = static_cast<unsigned int>(gate.control_qubits[i]);
        }
        
        // 构建控制 qubit 字符串用于调试输出
        std::stringstream controls_str;
        controls_str << "[";
        for (size_t i = 0; i < gate.control_qubits.size(); ++i) {
            controls_str << gate.control_qubits[i];
            if (i < gate.control_qubits.size() - 1) {
                controls_str << ", ";
            }
        }
        controls_str << "]";
        
        switch (gate.gate_type) {
            case QuantumGateType::X:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MCX_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCX_v2(num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MCX(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCX(num_controls, controls, target_qubit);
                #endif
                break;
            case QuantumGateType::Y:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MCY_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCY_v2(num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MCY(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCY(num_controls, controls, target_qubit);
                #endif
                break;
            case QuantumGateType::Z:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MCZ_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCZ_v2(num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MCZ(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCZ(num_controls, controls, target_qubit);
                #endif
                break;
            case QuantumGateType::H:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MCH_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCH_v2(num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MCH(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCH(num_controls, controls, target_qubit);
                #endif
                break;
            case QuantumGateType::S:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MCS_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCS_v2(num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MCS(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCS(num_controls, controls, target_qubit);
                #endif
                break;
            case QuantumGateType::T:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MCT_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCT_v2(num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MCT(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCT(num_controls, controls, target_qubit);
                #endif
                break;
            case QuantumGateType::ADJS:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MCAdjS_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCAdjS_v2(num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MCAdjS(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCAdjS(num_controls, controls, target_qubit);
                #endif
                break;
            case QuantumGateType::ADJT:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MCAdjT_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCAdjT_v2(num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MCAdjT(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ")");
                MCAdjT(num_controls, controls, target_qubit);
                #endif
                break;
            case QuantumGateType::RX:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MC_RX_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ", " << gate.angle << ")");
                MCRX_v2(gate.angle, num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MC_RX(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ", " << gate.angle << ")");
                MC_RX(num_controls, controls, target_qubit, gate.angle);
                #endif
                break;
            case QuantumGateType::RY:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MCRY_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ", " << gate.angle << ")");
                MCRY_v2(gate.angle, num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MC_RY(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ", " << gate.angle << ")");
                MC_RY(num_controls, controls, target_qubit, gate.angle);
                #endif
                break;
            case QuantumGateType::RZ:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MCRZ_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ", " << gate.angle << ")");
                MCRZ_v2(gate.angle, num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MC_RZ(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ", " << gate.angle << ")");
                MC_RZ(num_controls, controls, target_qubit, gate.angle);
                #endif
                break;
            case QuantumGateType::R1:
                #if USE_QC_RUNTIME_V2
                DEBUG_PRINT("  [qc_runtime] Calling MCR1_v2(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ", " << gate.angle << ")");
                MCR1_v2(gate.angle, num_controls, controls, target_qubit);
                #else
                DEBUG_PRINT("  [qc_runtime] Calling MCR1(" << num_controls << ", " << controls_str.str() << ", " << target_qubit << ", " << gate.angle << ")");
                MCR1(gate.angle, num_controls, controls, target_qubit);
                #endif
                break;
            default:
                std::cerr << "Error: Unknown controlled gate type" << std::endl;
                context.error_msg = "Unknown controlled gate type";
                delete[] controls;
                return false;
        }
        
        delete[] controls;
    }
    
    DEBUG_PRINT("DefaultQuantumGateHandler::execute_quantum_gate() returning true");
    return true;
}    
    
bool DefaultQuantumGateHandler::execute_measurement(
    const MeasureInstruction& measure, 
    ExecutionContext& context) {
    
    DEBUG_PRINT("DefaultQuantumGateHandler::execute_measurement() called");
    DEBUG_PRINT("  Qubit index: " << measure.qubit_index);
    DEBUG_PRINT("  Measure reg index: " << measure.measure_reg_index);
    DEBUG_PRINT("  Basis: " << static_cast<int>(measure.basis));
    
    // 调用实际的测量函数，如实处理所有测量
    unsigned int qubit = static_cast<unsigned int>(measure.qubit_index);
    #if USE_QC_RUNTIME_V2
    // 根据测量基确定 PauliV2 枚举值
    PauliV2 basis = PauliZV2; // 默认使用 Z 基
    switch (measure.basis)
    {
    case MeasureBasis::X:
        basis = PauliXV2;
        break;
    case MeasureBasis::Y:
        basis = PauliYV2;
        break;
    case MeasureBasis::Z:
        basis = PauliZV2;
        break;
    default:
        std::cerr << "Error: Unknown measure basis" << std::endl;
        context.error_msg = "Unknown measure basis";
        return false;
        break;
    }
    DEBUG_PRINT("  [qc_runtime] Calling M_v2(" << qubit << ", " << static_cast<int>(basis) << ")");
    bool result = M_v2(qubit, basis);
    #else
    DEBUG_PRINT("  [qc_runtime] Calling M(" << qubit << ")");
    bool result = M(qubit);
    #endif
    
    // 将测量结果保存到测量寄存器
    std::string reg_name = "m" + std::to_string(measure.measure_reg_index);
    context.mreg_values[reg_name] = RegisterValue(result);
    
    DEBUG_PRINT("  Measurement result: " << (result ? "1" : "0"));
    DEBUG_PRINT("DefaultQuantumGateHandler::execute_measurement() returning true");
    return true;
}

} // namespace ssa
