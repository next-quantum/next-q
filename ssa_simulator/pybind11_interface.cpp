// pybind11模块定义，使用真正的SSAExecutor和qc_runtime
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "ssa_parser.h"
#include "ssa_executor.h"

namespace py = pybind11;

// 使用真正的SSAExecutor来执行量子程序
class SSASimulatorPython {
public:
    SSASimulatorPython() {
        // 创建解析器和执行引擎
        parser_ = new ssa::SSAParser();
        executor_ = new ssa::SSAExecutor();
    }
    
    ~SSASimulatorPython() {
        // 释放资源
        delete parser_;
        delete executor_;
    }
    
    bool load_ssa_assembly(const std::string& assembly_code) {
        // 解析SSA汇编代码
        ssa::SSAProgram program;
        if (!parser_->parse(assembly_code, program)) {
            error_message_ = parser_->get_error();
            return false;
        }
        
        // 加载程序到执行引擎
        if (!executor_->load_program(program)) {
            error_message_ = executor_->get_error();
            return false;
        }
        
        return true;
    }
    
    bool run() {
        // 执行程序
        if (!executor_->run()) {
            error_message_ = executor_->get_error();
            return false;
        }
        return true;
    }
    
    void reset() {
        // 重置执行引擎
        executor_->reset();
    }
    
    void reset_state() {
        // 重置量子态
        executor_->reset_state();
    }
    
    std::vector<std::string> get_measurement_reg_names() {
        // 获取测量寄存器名称
        return executor_->get_measurement_reg_names();
    }
    
    std::map<std::string, int> sample(int shots_count = 1000) {
        std::map<std::string, int> results;
        
        // 运行多次模拟
        for (int i = 0; i < shots_count; ++i) {
            // 重置量子状态
            executor_->reset_state();
            
            // 执行程序
            if (!executor_->run()) {
                error_message_ = executor_->get_error();
                return results;
            }
            
            // 获取测量结果
            std::string result;
            // 获取所有测量寄存器名称
            std::vector<std::string> mreg_names = executor_->get_measurement_reg_names();
            if (mreg_names.empty()) {
                error_message_ = "No measurement registers found in the program";
                return results;
            }
            
            // 按照字典序排序寄存器名称，确保结果顺序一致
            std::sort(mreg_names.begin(), mreg_names.end());
            
            // 收集测量结果
            for (const auto& reg_name : mreg_names) {
                int value = executor_->get_measurement_reg_value(reg_name).int32_value;
                result += std::to_string(value);
            }
            
            // 更新计数
            results[result]++;
        }
        
        return results;
    }
    
    std::string get_error() {
        // 返回错误信息
        return error_message_;
    }
    
private:
    ssa::SSAParser* parser_;
    ssa::SSAExecutor* executor_;
    std::string error_message_;
};

// 模块初始化
PYBIND11_MODULE(ssa_simulator_cpp, m) {
    py::class_<SSASimulatorPython>(m, "SSASimulator")
        .def(py::init<>())
        .def("load_ssa_assembly", &SSASimulatorPython::load_ssa_assembly)
        .def("run", &SSASimulatorPython::run)
        .def("reset", &SSASimulatorPython::reset)
        .def("reset_state", &SSASimulatorPython::reset_state)
        .def("get_measurement_reg_names", &SSASimulatorPython::get_measurement_reg_names)
        .def("sample", &SSASimulatorPython::sample, py::arg("shots_count") = 1000)
        .def("get_error", &SSASimulatorPython::get_error);
}