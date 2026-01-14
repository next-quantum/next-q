#include "ssa_parser.h"
#include "ssa_executor.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <map>
#include <string>

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

int main(int argc, char* argv[]) {
    // 解析命令行参数
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <ssa_file_path> <shots>" << std::endl;
        std::cerr << "  <ssa_file_path>: Path to the SSA file to simulate" << std::endl;
        std::cerr << "  <shots>: Number of simulation shots to run" << std::endl;
        return 1;
    }
    
    std::string ssa_file = argv[1];
    int num_shots = std::stoi(argv[2]);
    
    // 读取SSA文件
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
    
    // 获取测量寄存器数量
    int num_mregs = program.mregs.size();
    if (num_mregs == 0) {
        std::cerr << "Error: No measurement registers found in the program" << std::endl;
        return 1;
    }
    
    // 统计结果
    std::map<std::string, int> result_counts;
    
    std::cout << "Running SSA simulation with " << num_shots << " shots..." << std::endl;
    
    // 创建执行引擎
    ssa::SSAExecutor executor;
    
    // 加载程序
    if (!executor.load_program(program)) {
        std::cerr << "Error loading program: " << executor.get_error() << std::endl;
        return 1;
    }
    
    // 运行多次模拟
    for (int i = 0; i < num_shots; ++i) {
        // 重置量子状态，不重新创建执行器
        executor.reset_state();
        
        // 执行程序
        if (!executor.run()) {
            std::cerr << "Error executing program: " << executor.get_error() << std::endl;
            return 1;
        }
        
        // 获取测量结果
        std::string result;
        for (int j = 0; j < num_mregs; ++j) {
            std::string reg_name = "m" + std::to_string(j);
            int value = executor.get_measurement_reg_value(reg_name).int32_value;
            result += std::to_string(value);
        }
        
        // 更新计数
        result_counts[result]++;
    }
    
    // 输出统计结果
    std::cout << "\n=== Simulation Results (" << num_shots << " shots) ===" << std::endl;
    std::cout << std::setw(12) << "Quantum State" << " | " << std::setw(8) << "Count" << " | " << std::setw(12) << "Probability" << std::endl;
    std::cout << "--------------|----------|--------------" << std::endl;
    
    for (const auto& [state, count] : result_counts) {
        double probability = static_cast<double>(count) / num_shots;
        std::cout << std::setw(12) << state << " | " 
                  << std::setw(8) << count << " | " 
                  << std::setw(11) << std::fixed << std::setprecision(6) << probability << "" << std::endl;
    }
    
    // 输出总计数检查
    std::cout << "\n=== Total Count Check ===" << std::endl;
    int total = 0;
    for (const auto& [state, count] : result_counts) {
        total += count;
    }
    std::cout << "Total shots executed: " << total << " (expected: " << num_shots << ")" << std::endl;
    
    return 0;
}