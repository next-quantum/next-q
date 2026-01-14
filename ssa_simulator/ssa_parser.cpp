#include "ssa_parser.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>

namespace ssa {

SSAParser::SSAParser() : current_line_(0) {
}

SSAParser::~SSAParser() {
}

bool SSAParser::parse(const std::string& ssa_code, SSAProgram& program) {
    // 重置程序状态
    program.clear();
    error_.clear();
    current_line_ = 0;
    
    std::istringstream iss(ssa_code);
    std::string line;
    
    // 逐行解析
    while (std::getline(iss, line)) {
        current_line_++;
        if (!parse_line(line, program)) {
            return false;
        }
    }
    
    return true;
}

bool SSAParser::parse_file(const std::string& file_path, SSAProgram& program) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        error_ = "Failed to open file: " + file_path;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return parse(buffer.str(), program);
}

const std::string& SSAParser::get_error() const {
    return error_;
}

std::string SSAParser::generate_asm(const SSAProgram& program) {
    std::string generated_asm;
    
    // 遍历所有指令，使用original_line生成ASM代码
    for (const auto& instr : program.instructions) {
        // 直接添加原始行，包括空行
        generated_asm += instr.original_line + "\n";
    }
    
    return generated_asm;
}

bool SSAParser::parse_line(const std::string& line, SSAProgram& program) {
    // 处理空行
    std::string trimmed_line = trim(line);
    if (trimmed_line.empty()) {
        // 添加空行到指令列表
        Instruction instr(InstructionType::UNKNOWN);
        instr.original_line = line;
        program.instructions.push_back(instr);
        return true;
    }
    
    // 处理注释行
    if (trimmed_line.find(";;") == 0) {
        // 添加注释行到指令列表
        Instruction instr(InstructionType::UNKNOWN);
        instr.original_line = line;
        program.instructions.push_back(instr);
        return true;
    }
    
    // 移除注释
    std::string processed_line = remove_comment(line);
    
    // 移除行首行尾空白字符
    processed_line = trim(processed_line);
    
    // 跳过空行
    if (processed_line.empty()) {
        return true;
    }
    
    // 根据指令类型进行解析
    if (processed_line.find("declare ") == 0) {
        return parse_declare(line, program);
    } else if (processed_line.find("qgate.") == 0) {
        return parse_qgate(line, program);
    } else if (processed_line.find("measure.") == 0) {
        return parse_measure(line, program);
    } else if (processed_line.find("mov.int32 ") == 0 || 
               processed_line.find("mov.float32 ") == 0 || 
               processed_line.find("mov ") == 0) {
        return parse_mov(line, program);
    } else if (processed_line.find("const.int32 ") == 0 || 
               processed_line.find("const.float32 ") == 0 || 
               processed_line.find("const ") == 0) {
        return parse_const(line, program);
    } else if (processed_line.find("and ") == 0) {
        return parse_and(line, program);
    } else if (processed_line.find("or ") == 0) {
        return parse_or(line, program);
    } else if (processed_line.find("xor ") == 0) {
        return parse_xor(line, program);
    } else if (processed_line.find("all ") == 0) {
        return parse_all(line, program);
    } else if (processed_line.find("any ") == 0) {
        return parse_any(line, program);
    } else if (processed_line.find("br.cond.int32 ") == 0 || 
               processed_line.find("br.cond.float32 ") == 0 || 
               processed_line.find("br.cond ") == 0) {
        return parse_br_cond(line, program);
    } else if (processed_line.find("br ") == 0) {
        return parse_br_uncond(line, program);
    } else if (processed_line.back() == ':') {
        return parse_label(line, program);
    }
    
    // 未知指令类型
    error_ = "Unknown instruction at line " + std::to_string(current_line_) + ": " + line;
    return false;
}

bool SSAParser::parse_declare(const std::string& line, SSAProgram& program) {
    // 格式：declare qreg|creg|mreg reg_name
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    std::istringstream iss(no_comment_line.substr(8)); // 跳过 "declare "
    std::string reg_type_str, reg_name;
    
    if (!(iss >> reg_type_str >> reg_name)) {
        error_ = "Invalid declare syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    RegisterType type;
    if (reg_type_str == "qreg") {
        type = RegisterType::QREG;
    } else if (reg_type_str == "creg") {
        type = RegisterType::CREG;
    } else if (reg_type_str == "mreg") {
        type = RegisterType::MREG;
    } else {
        error_ = "Invalid register type at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    // 提取寄存器索引
    int32 index = parse_register_index(reg_name);
    
    // 创建寄存器
    Register reg(reg_name, type, index);
    
    // 添加到寄存器映射
    program.registers[reg_name] = reg;
    program.register_index_map[index] = reg_name;
    
    // 添加到相应的寄存器列表
    switch (type) {
        case RegisterType::QREG:
            program.qregs.push_back(reg);
            break;
        case RegisterType::CREG:
            program.cregs.push_back(reg);
            break;
        case RegisterType::MREG:
            program.mregs.push_back(reg);
            break;
    }
    
    // 创建DECLARE类型的Instruction对象
    Instruction instr(InstructionType::DECLARE);
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

bool SSAParser::parse_qgate(const std::string& line, SSAProgram& program) {
    // 格式：qgate.x q1, ctrl=q0
    // 或：qgate.h q0
    // 或：qgate.cnot q0, q1
    
    Instruction instr(InstructionType::QGATE);
    QGateInstruction& gate = instr.qgate;
    
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    
    // 提取门名称
    size_t first_dot_pos = no_comment_line.find('.'); // 查找第一个点
    size_t space_pos = no_comment_line.find(' ');
    
    if (first_dot_pos == std::string::npos || space_pos == std::string::npos) {
        error_ = "Invalid qgate syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    std::string gate_name = no_comment_line.substr(first_dot_pos + 1, space_pos - first_dot_pos - 1);
    
    // 检查是否为共轭转置
    if (gate_name.back() == 'j') {
        gate.adjoint = true;
        gate_name = gate_name.substr(0, gate_name.size() - 1);
    }
    
    // 转换门类型
    gate.gate_type = gate_name_to_type(gate_name);
    
    // 提取量子比特部分
    std::string qubits_part = no_comment_line.substr(space_pos + 1);
    
    // 处理动态参数
    if (qubits_part.find("dynamic=") != std::string::npos) {
        // 提取动态参数值，例如：dynamic=c1 -> c1
        size_t dynamic_pos = qubits_part.find("dynamic=");
        std::string dynamic_reg = qubits_part.substr(dynamic_pos + 8);
        dynamic_reg = trim(dynamic_reg);
        
        // 设置原始行
        instr.original_line = line;
        
        // 添加到指令列表
        program.instructions.push_back(instr);
        return true;
    }
    
    // 处理控制位和angle参数
    size_t ctrl_pos = qubits_part.find("ctrl=");
    size_t angle_pos = qubits_part.find("angle=");
    
    // 确定targets_part的结束位置：如果有ctrl=或angle=，则到它们之前结束
    size_t targets_end = qubits_part.length();
    if (ctrl_pos != std::string::npos) {
        targets_end = ctrl_pos;
    } else if (angle_pos != std::string::npos) {
        targets_end = angle_pos;
    }
    
    std::string targets_part = trim(qubits_part.substr(0, targets_end));
    std::string ctrls_part;
    
    if (ctrl_pos != std::string::npos) {
        // 有控制位，确定ctrls_part的结束位置
        size_t ctrls_end = qubits_part.length();
        if (angle_pos != std::string::npos && angle_pos > ctrl_pos) {
            ctrls_end = angle_pos;
        }
        ctrls_part = trim(qubits_part.substr(ctrl_pos + 5, ctrls_end - ctrl_pos - 5));
    }
    
    // 解析目标量子比特
    std::istringstream targets_iss(targets_part);
    std::string target;
    while (std::getline(targets_iss, target, ',')) {
        target = trim(target);
        if (!target.empty()) {
            // 检查是否有.adj后缀
            if (target.find(".adj") != std::string::npos) {
                gate.adjoint = true;
                target = target.substr(0, target.find(".adj"));
            }
            int32 qubit_index = parse_register_index(target);
            gate.target_qubits.push_back(qubit_index);
        }
    }
    
    // 解析控制量子比特
    if (ctrl_pos != std::string::npos) {
        std::istringstream ctrls_iss(ctrls_part);
        std::string ctrl;
        while (std::getline(ctrls_iss, ctrl, ',')) {
            ctrl = trim(ctrl);
            if (!ctrl.empty()) {
                int32 qubit_index = parse_register_index(ctrl);
                gate.control_qubits.push_back(qubit_index);
            }
        }
    }
    
    // 解析角度参数（如果有）
    size_t full_angle_pos = no_comment_line.find("angle=");
    if (full_angle_pos != std::string::npos) {
        std::string angle_str = no_comment_line.substr(full_angle_pos + 6);
        // 移除可能的空格、逗号和注释
        size_t space_pos = angle_str.find(" ");
        if (space_pos != std::string::npos) {
            angle_str = angle_str.substr(0, space_pos);
        }
        if (!angle_str.empty() && angle_str.back() == ',') {
            angle_str.pop_back();
        }
        // 移除可能的注释
        size_t comment_pos = angle_str.find(";;");
        if (comment_pos != std::string::npos) {
            angle_str = angle_str.substr(0, comment_pos);
        }
        // 转换为double
        gate.angle = std::stod(angle_str);
    }
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

bool SSAParser::parse_measure(const std::string& line, SSAProgram& program) {
    // 格式：measure.z q0, m0
    std::regex measure_regex(R"(measure\.([a-zA-Z]+)\s+([a-zA-Z0-9]+)\s*,\s*([a-zA-Z0-9]+))");
    std::smatch match;
    
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    
    if (!std::regex_search(no_comment_line, match, measure_regex)) {
        error_ = "Invalid measure syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    Instruction instr(InstructionType::MEASURE);
    MeasureInstruction& measure = instr.measure;
    
    measure.basis = match[1];
    measure.qubit_index = parse_register_index(match[2]);
    measure.measure_reg_index = parse_register_index(match[3]);
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

bool SSAParser::parse_mov(const std::string& line, SSAProgram& program) {
    // 格式：mov c0, m0 或 mov.int32 c0, 1 或 mov.float32 c0, 1.5
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    
    // 确定指令前缀长度
    size_t prefix_len = 4; // 默认 "mov "
    if (no_comment_line.find("mov.int32 ") == 0) {
        prefix_len = 10; // "mov.int32 "
    } else if (no_comment_line.find("mov.float32 ") == 0) {
        prefix_len = 12; // "mov.float32 "
    }
    
    std::istringstream iss(no_comment_line.substr(prefix_len));
    std::string dest, src;
    
    if (!(iss >> dest >> src)) {
        error_ = "Invalid mov syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    // 移除逗号
    if (dest.back() == ',') {
        dest.pop_back();
    }
    if (src.back() == ',') {
        src.pop_back();
    }
    
    Instruction instr(InstructionType::MOV);
    MovInstruction& mov = instr.mov;
    
    mov.dest_reg_index = parse_register_index(dest);
    
    // 解析源操作数，可以是寄存器或常量
    if (std::isdigit(src[0]) || src[0] == '-') {
        // 常量值
        mov.src_reg_index = -1;
        // 保存常量值
        mov.const_value = std::stoi(src);
    } else {
        // 寄存器
        mov.src_reg_index = parse_register_index(src);
    }
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

bool SSAParser::parse_const(const std::string& line, SSAProgram& program) {
    // 格式：const.int32 c0, 1 或 const.float32 c0, 1.5
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    
    // 确定指令前缀长度
    size_t prefix_len = 6; // 默认 "const "
    if (no_comment_line.find("const.int32 ") == 0) {
        prefix_len = 12; // "const.int32 "
    } else if (no_comment_line.find("const.float32 ") == 0) {
        prefix_len = 14; // "const.float32 "
    }
    
    std::istringstream iss(no_comment_line.substr(prefix_len));
    std::string dest, value_str;
    
    if (!(iss >> dest >> value_str)) {
        error_ = "Invalid const syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    // 移除逗号
    if (dest.back() == ',') {
        dest.pop_back();
    }
    if (value_str.back() == ',') {
        value_str.pop_back();
    }
    
    Instruction instr(InstructionType::MOV);
    MovInstruction& mov = instr.mov;
    
    mov.dest_reg_index = parse_register_index(dest);
    mov.src_reg_index = -1; // 常量值使用-1表示
    
    // 保存常量值
    mov.const_value = std::stoi(value_str);
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

bool SSAParser::parse_br_cond(const std::string& line, SSAProgram& program) {
    // 格式：br.cond c0, ==, 1, label_0, label_1 或 br.cond.int32 c0, ==, 1, label_0, label_1 或 br.cond.float32 c0, ==, 1.5, label_0, label_1
    std::regex br_cond_regex(R"(br\.cond(\.(int32|float32))?\s+([a-zA-Z0-9]+)\s*\,\s*([=!<>]=?)\s*\,\s*([0-9]+\.?[0-9]*|True|False|true|false)\s*\,\s*([a-zA-Z0-9_]+)\s*\,\s*([a-zA-Z0-9_]+))");
    std::smatch match;
    
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    
    if (!std::regex_search(no_comment_line, match, br_cond_regex)) {
        error_ = "Invalid br.cond syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    Instruction instr(InstructionType::BR_COND);
    BrCondInstruction& br_cond = instr.br_cond;
    
    br_cond.reg_index = parse_register_index(match[3]);
    br_cond.op = condition_op_to_type(match[4]);
    
    // 处理比较值，可以是数字或布尔值
    std::string value_str = match[5];
    if (value_str == "True" || value_str == "true") {
        br_cond.value = 1;
    } else if (value_str == "False" || value_str == "false") {
        br_cond.value = 0;
    } else if (value_str.find('.') != std::string::npos) {
        // 浮点数，这里我们暂时转换为整数，实际实现中可能需要扩展BrCondInstruction支持浮点数
        br_cond.value = static_cast<int32>(std::stof(value_str));
    } else {
        // 整数
        br_cond.value = static_cast<int32>(std::stoi(value_str));
    }
    
    br_cond.true_label = match[6];
    br_cond.false_label = match[7];
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

bool SSAParser::parse_br_uncond(const std::string& line, SSAProgram& program) {
    // 格式：br label_0
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    std::istringstream iss(no_comment_line.substr(3)); // 跳过 "br "
    std::string label;
    
    if (!(iss >> label)) {
        error_ = "Invalid br syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    Instruction instr(InstructionType::BR_UNCOND);
    instr.br_uncond = BrUncondInstruction(label);
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

bool SSAParser::parse_label(const std::string& line, SSAProgram& program) {
    // 格式：label_0:
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    std::string label_name = no_comment_line.substr(0, no_comment_line.size() - 1);
    
    Instruction instr(InstructionType::LABEL);
    instr.label = LabelInstruction(label_name);
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    // 记录标签位置
    program.label_map[label_name] = program.instructions.size() - 1;
    
    return true;
}

bool SSAParser::parse_and(const std::string& line, SSAProgram& program) {
    // 格式：and <output_reg>, <input_reg1>, <input_reg2>
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    std::istringstream iss(no_comment_line.substr(4)); // 跳过 "and "
    std::string output_reg, input_reg1, input_reg2;
    char comma1, comma2;
    
    if (!(iss >> output_reg >> comma1 >> input_reg1 >> comma2 >> input_reg2)) {
        error_ = "Invalid and syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    Instruction instr(InstructionType::AND);
    instr.and_op.left_reg = parse_register_index(input_reg1);
    instr.and_op.right_reg = parse_register_index(input_reg2);
    instr.and_op.output_reg = parse_register_index(output_reg);
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

bool SSAParser::parse_or(const std::string& line, SSAProgram& program) {
    // 格式：or <output_reg>, <input_reg1>, <input_reg2>
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    std::istringstream iss(no_comment_line.substr(3)); // 跳过 "or "
    std::string output_reg, input_reg1, input_reg2;
    char comma1, comma2;
    
    if (!(iss >> output_reg >> comma1 >> input_reg1 >> comma2 >> input_reg2)) {
        error_ = "Invalid or syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    Instruction instr(InstructionType::OR);
    instr.or_op.left_reg = parse_register_index(input_reg1);
    instr.or_op.right_reg = parse_register_index(input_reg2);
    instr.or_op.output_reg = parse_register_index(output_reg);
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

bool SSAParser::parse_xor(const std::string& line, SSAProgram& program) {
    // 格式：xor <output_reg>, <input_reg1>, <input_reg2>
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    std::istringstream iss(no_comment_line.substr(4)); // 跳过 "xor "
    std::string output_reg, input_reg1, input_reg2;
    char comma1, comma2;
    
    if (!(iss >> output_reg >> comma1 >> input_reg1 >> comma2 >> input_reg2)) {
        error_ = "Invalid xor syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    Instruction instr(InstructionType::XOR);
    instr.xor_op.left_reg = parse_register_index(input_reg1);
    instr.xor_op.right_reg = parse_register_index(input_reg2);
    instr.xor_op.output_reg = parse_register_index(output_reg);
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

bool SSAParser::parse_all(const std::string& line, SSAProgram& program) {
    // 格式：all <output_reg>, [<input_regs>]
    std::regex all_regex(R"(all\s+([a-zA-Z0-9]+)\s*,\s*\[([^\]]+)\])");
    std::smatch match;
    
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    
    if (!std::regex_search(no_comment_line, match, all_regex)) {
        error_ = "Invalid all syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    Instruction instr(InstructionType::ALL);
    instr.all_op.output_reg = parse_register_index(match[1]);
    
    // 解析输入寄存器列表
    std::string input_regs_str = match[2];
    std::istringstream iss(input_regs_str);
    std::string reg;
    while (std::getline(iss, reg, ',')) {
        reg = trim(reg);
        if (!reg.empty()) {
            instr.all_op.input_regs.push_back(parse_register_index(reg));
        }
    }
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

bool SSAParser::parse_any(const std::string& line, SSAProgram& program) {
    // 格式：any <output_reg>, [<input_regs>]
    std::regex any_regex(R"(any\s+([a-zA-Z0-9]+)\s*,\s*\[([^\]]+)\])");
    std::smatch match;
    
    // 先移除注释
    std::string no_comment_line = remove_comment(line);
    
    if (!std::regex_search(no_comment_line, match, any_regex)) {
        error_ = "Invalid any syntax at line " + std::to_string(current_line_) + ": " + line;
        return false;
    }
    
    Instruction instr(InstructionType::ANY);
    instr.any_op.output_reg = parse_register_index(match[1]);
    
    // 解析输入寄存器列表
    std::string input_regs_str = match[2];
    std::istringstream iss(input_regs_str);
    std::string reg;
    while (std::getline(iss, reg, ',')) {
        reg = trim(reg);
        if (!reg.empty()) {
            instr.any_op.input_regs.push_back(parse_register_index(reg));
        }
    }
    
    // 设置原始行
    instr.original_line = line;
    
    // 添加到指令列表
    program.instructions.push_back(instr);
    
    return true;
}

int32 SSAParser::parse_register_index(const std::string& reg_name) {
    // 格式：q0, c1, m2等
    // 提取数字部分
    std::string index_str;
    for (char c : reg_name) {
        if (std::isdigit(c)) {
            index_str += c;
        }
    }
    
    if (index_str.empty()) {
        return -1;
    }
    
    return static_cast<int32>(std::stoi(index_str));
}



QuantumGateType SSAParser::gate_name_to_type(const std::string& gate_name) {
    if (gate_name == "x") return QuantumGateType::X;
    if (gate_name == "y") return QuantumGateType::Y;
    if (gate_name == "z") return QuantumGateType::Z;
    if (gate_name == "h") return QuantumGateType::H;
    if (gate_name == "s") return QuantumGateType::S;
    if (gate_name == "t") return QuantumGateType::T;
    if (gate_name == "sadj" || gate_name == "sdg" || gate_name == "adjs") return QuantumGateType::ADJS;
    if (gate_name == "tadj" || gate_name == "tdg" || gate_name == "adjt") return QuantumGateType::ADJT;
    if (gate_name == "rx") return QuantumGateType::RX;
    if (gate_name == "ry") return QuantumGateType::RY;
    if (gate_name == "rz") return QuantumGateType::RZ;
    if (gate_name == "reset") return QuantumGateType::RESET;
    if (gate_name == "cnot") return QuantumGateType::CNOT;
    if (gate_name == "swap") return QuantumGateType::SWAP;
    if (gate_name == "toffoli" || gate_name == "ccnot") return QuantumGateType::TOFFOLI;
    return QuantumGateType::UNKNOWN;
}

ConditionOp SSAParser::condition_op_to_type(const std::string& op) {
    if (op == "==") return ConditionOp::EQ;
    if (op == "!=") return ConditionOp::NE;
    if (op == "<") return ConditionOp::LT;
    if (op == "<=") return ConditionOp::LE;
    if (op == ">") return ConditionOp::GT;
    if (op == ">=") return ConditionOp::GE;
    return ConditionOp::UNKNOWN;
}

std::string SSAParser::trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }
    
    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    
    return std::string(start, end + 1);
}

std::string SSAParser::remove_comment(const std::string& line) {
    size_t comment_pos = line.find(";;");
    if (comment_pos != std::string::npos) {
        return line.substr(0, comment_pos);
    }
    return line;
}

} // namespace ssa
