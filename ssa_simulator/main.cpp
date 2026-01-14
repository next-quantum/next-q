#include "ssa_parser.h"
#include "ssa_executor.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

// 输出寄存器类型名称
std::string register_type_name(ssa::RegisterType type) {
    switch (type) {
        case ssa::RegisterType::QREG: return "qreg";
        case ssa::RegisterType::CREG: return "creg";
        case ssa::RegisterType::MREG: return "mreg";
        default: return "unknown";
    }
}

// 输出量子门类型名称
std::string quantum_gate_name(ssa::QuantumGateType type) {
    switch (type) {
        case ssa::QuantumGateType::X: return "X";
        case ssa::QuantumGateType::Y: return "Y";
        case ssa::QuantumGateType::Z: return "Z";
        case ssa::QuantumGateType::H: return "H";
        case ssa::QuantumGateType::S: return "S";
        case ssa::QuantumGateType::T: return "T";
        case ssa::QuantumGateType::ADJS: return "AdjS";
        case ssa::QuantumGateType::ADJT: return "AdjT";
        case ssa::QuantumGateType::CNOT: return "CNOT";
        case ssa::QuantumGateType::SWAP: return "SWAP";
        case ssa::QuantumGateType::TOFFOLI: return "TOFFOLI";
        default: return "UNKNOWN";
    }
}

// 输出指令类型名称
std::string instruction_type_name(ssa::InstructionType type) {
    switch (type) {
        case ssa::InstructionType::DECLARE: return "DECLARE";
        case ssa::InstructionType::QGATE: return "QGATE";
        case ssa::InstructionType::MEASURE: return "MEASURE";
        case ssa::InstructionType::MOV: return "MOV";
        case ssa::InstructionType::AND: return "AND";
        case ssa::InstructionType::OR: return "OR";
        case ssa::InstructionType::XOR: return "XOR";
        case ssa::InstructionType::ALL: return "ALL";
        case ssa::InstructionType::ANY: return "ANY";
        case ssa::InstructionType::BR_COND: return "BR_COND";
        case ssa::InstructionType::BR_UNCOND: return "BR_UNCOND";
        case ssa::InstructionType::LABEL: return "LABEL";
        default: return "UNKNOWN";
    }
}

// 输出条件操作符名称
std::string condition_op_name(ssa::ConditionOp op) {
    switch (op) {
        case ssa::ConditionOp::EQ: return "==";
        case ssa::ConditionOp::NE: return "!=";
        case ssa::ConditionOp::LT: return "<";
        case ssa::ConditionOp::LE: return "<=";
        case ssa::ConditionOp::GT: return ">";
        case ssa::ConditionOp::GE: return ">=";
        default: return "UNKNOWN";
    }
}

// 输出寄存器列表
void print_registers(const std::vector<ssa::Register>& registers, const std::string& title) {
    std::cout << "\n" << title << ":" << std::endl;
    std::cout << "  Name  | Index | Type" << std::endl;
    std::cout << "--------|-------|------" << std::endl;
    for (const auto& reg : registers) {
        std::cout << "  " << std::left << std::setw(6) << reg.name
                  << "| " << std::setw(5) << reg.index
                  << "| " << register_type_name(reg.type) << std::endl;
    }
}

// 输出指令详情
void print_instruction(const ssa::Instruction& instr, int32 index) {
    std::cout << "  [" << std::setw(3) << index << "] " 
              << std::left << std::setw(10) << instruction_type_name(instr.type);
    
    switch (instr.type) {
        case ssa::InstructionType::QGATE: {
            const auto& gate = instr.qgate;
            std::cout << quantum_gate_name(gate.gate_type) 
                      << (gate.adjoint ? " (adj)" : "") 
                      << " targets: [";
            for (size_t i = 0; i < gate.target_qubits.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << "q" << gate.target_qubits[i];
            }
            std::cout << "]";
            
            if (!gate.control_qubits.empty()) {
                std::cout << " controls: [";
                for (size_t i = 0; i < gate.control_qubits.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << "q" << gate.control_qubits[i];
                }
                std::cout << "]";
            }
            break;
        }
        case ssa::InstructionType::MEASURE: {
            const auto& measure = instr.measure;
            std::cout << "basis=" << measure.basis 
                      << " q" << measure.qubit_index 
                      << " -> m" << measure.measure_reg_index;
            break;
        }
        case ssa::InstructionType::MOV: {
            const auto& mov = instr.mov;
            std::cout << "c" << mov.dest_reg_index << " <- " 
                      << "m" << mov.src_reg_index;
            break;
        }
        case ssa::InstructionType::AND: {
            const auto& and_op = instr.and_op;
            std::cout << "c" << and_op.left_reg << " and c" << and_op.right_reg 
                      << " -> c" << and_op.output_reg;
            break;
        }
        case ssa::InstructionType::OR: {
            const auto& or_op = instr.or_op;
            std::cout << "c" << or_op.left_reg << " or c" << or_op.right_reg 
                      << " -> c" << or_op.output_reg;
            break;
        }
        case ssa::InstructionType::XOR: {
            const auto& xor_op = instr.xor_op;
            std::cout << "c" << xor_op.left_reg << " xor c" << xor_op.right_reg 
                      << " -> c" << xor_op.output_reg;
            break;
        }
        case ssa::InstructionType::ALL: {
            const auto& all_op = instr.all_op;
            std::cout << "all([";
            for (size_t i = 0; i < all_op.input_regs.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << "c" << all_op.input_regs[i];
            }
            std::cout << "]) -> c" << all_op.output_reg;

            break;
        }
        case ssa::InstructionType::ANY: {
            const auto& any_op = instr.any_op;
            std::cout << "any([";
            for (size_t i = 0; i < any_op.input_regs.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << "c" << any_op.input_regs[i];
            }
            std::cout << "]) -> c" << any_op.output_reg;

            break;
        }
        case ssa::InstructionType::BR_COND: {
            const auto& br = instr.br_cond;
            std::cout << "c" << br.reg_index << " " 
                      << condition_op_name(br.op) << " " 
                      << br.value << " ? " 
                      << br.true_label << " : " << br.false_label;
            break;
        }
        case ssa::InstructionType::BR_UNCOND: {
            const auto& br = instr.br_uncond;
            std::cout << br.label;
            break;
        }
        case ssa::InstructionType::LABEL: {
            const auto& label = instr.label;
            std::cout << label.name;
            break;
        }
        default:
            break;
    }
    
    std::cout << "" << std::endl;
}

// 输出标签映射
void print_labels(const std::map<std::string, int32>& label_map) {
    std::cout << "\nLabels:" << std::endl;
    std::cout << "  Name        | Instruction Index" << std::endl;
    std::cout << "--------------|------------------" << std::endl;
    for (const auto& [name, index] : label_map) {
        std::cout << "  " << std::left << std::setw(12) << name 
                  << "| " << std::setw(16) << index << std::endl;
    }
}

// 读取文件内容
std::string read_file_content(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return buffer.str();
}

// 比较两个字符串，忽略末尾的换行符
bool compare_asm(const std::string& original, const std::string& generated) {
    // 移除末尾的换行符
    std::string original_stripped = original;
    std::string generated_stripped = generated;
    
    while (!original_stripped.empty() && original_stripped.back() == '\n') {
        original_stripped.pop_back();
    }
    
    while (!generated_stripped.empty() && generated_stripped.back() == '\n') {
        generated_stripped.pop_back();
    }
    
    return original_stripped == generated_stripped;
}

// 输出执行状态名称
std::string execution_status_name(ssa::ExecutionStatus status) {
    switch (status) {
        case ssa::ExecutionStatus::RUNNING: return "RUNNING";
        case ssa::ExecutionStatus::FINISHED: return "FINISHED";
        case ssa::ExecutionStatus::ERROR: return "ERROR";
        case ssa::ExecutionStatus::INTERRUPTED: return "INTERRUPTED";
        default: return "UNKNOWN";
    }
}

// 输出寄存器值
void print_register_values(const ssa::ExecutionContext& context) {
    std::cout << "\nRegister Values:" << std::endl;
    
    // 输出经典寄存器值
    std::cout << "  Classical Registers:" << std::endl;
    for (const auto& [name, value] : context.creg_values) {
        std::cout << "    " << name << ": " << value.int32_value << std::endl;
    }
    
    // 输出测量寄存器值
    std::cout << "  Measurement Registers:" << std::endl;
    for (const auto& [name, value] : context.mreg_values) {
        std::cout << "    " << name << ": " << value.int32_value << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <ssa_file> [--execute]" << std::endl;
        std::cout << "  --execute: Execute the SSA assembly code" << std::endl;
        return 1;
    }
    
    std::string ssa_file = argv[1];
    bool execute = false;
    
    // 检查是否有执行选项
    if (argc >= 3 && std::string(argv[2]) == "--execute") {
        execute = true;
    }
    
    // 读取原始ASM文件内容
    std::string original_asm = read_file_content(ssa_file);
    if (original_asm.empty()) {
        return 1;
    }
    
    // 创建解析器
    ssa::SSAParser parser;
    ssa::SSAProgram program;
    
    // 解析SSA文件
    if (!parser.parse_file(ssa_file, program)) {
        std::cerr << "Error parsing SSA file: " << parser.get_error() << std::endl;
        return 1;
    }
    
    // 生成ASM代码
    std::string generated_asm = parser.generate_asm(program);
    
    // 比对ASM代码
    bool is_same = compare_asm(original_asm, generated_asm);
    
    std::cout << "=== SSA Assembly Parser Result ===" << std::endl;
    
    // 输出寄存器信息
    print_registers(program.qregs, "Quantum Registers");
    print_registers(program.cregs, "Classical Registers");
    print_registers(program.mregs, "Measurement Registers");
    
    // 输出标签信息
    print_labels(program.label_map);
    
    // 输出指令列表
    std::cout << "\nInstructions:" << std::endl;
    for (size_t i = 0; i < program.instructions.size(); ++i) {
        print_instruction(program.instructions[i], i);
    }
    
    std::cout << "\n=== ASM Comparison Result ===" << std::endl;
    if (is_same) {
        std::cout << "✓ Generated ASM is identical to original ASM!" << std::endl;
    } else {
        std::cout << "✗ Generated ASM differs from original ASM!" << std::endl;
        
        // 输出原始ASM
        std::cout << "\n=== Original ASM ===" << std::endl;
        std::cout << original_asm << std::endl;
        
        // 输出生成的ASM
        std::cout << "=== Generated ASM ===" << std::endl;
        std::cout << generated_asm << std::endl;
    }
    
    std::cout << "\n=== Parse Successful ===" << std::endl;
    std::cout << "Total instructions: " << program.instructions.size() << std::endl;
    std::cout << "Total qubits: " << program.qregs.size() << std::endl;
    std::cout << "Total classical registers: " << program.cregs.size() << std::endl;
    std::cout << "Total measurement registers: " << program.mregs.size() << std::endl;
    std::cout << "Total labels: " << program.label_map.size() << std::endl;
    
    // 如果需要执行SSA汇编
    if (execute) {
        std::cout << "\n=== Executing SSA Assembly ===" << std::endl;
        
        // 创建执行引擎
        ssa::SSAExecutor executor;
        
        // 加载程序
        if (!executor.load_program(program)) {
            std::cerr << "Error loading program: " << executor.get_error() << std::endl;
            return 1;
        }
        
        // 执行程序
        bool success = executor.run();
        
        // 输出执行结果
        std::cout << "\n=== Execution Result ===" << std::endl;
        std::cout << "Status: " << execution_status_name(executor.get_status()) << std::endl;
        
        if (success) {
            std::cout << "✓ Execution completed successfully!" << std::endl;
        } else {
            std::cout << "✗ Execution failed!" << std::endl;
            std::cout << "Error: " << executor.get_error() << std::endl;
        }
        
        // 输出寄存器值
        print_register_values(executor.get_context());
    }
    
    return is_same ? 0 : 1;
}
