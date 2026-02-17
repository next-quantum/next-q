// pybind11模块定义，使用真正的SSAExecutor和qc_runtime
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/complex.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <complex>

#include "ssa_parser.h"
#include "ssa_executor.h"
#include "qc_runtime/qc_runtime_v2.h"

namespace py = pybind11;

// 使用真正的SSAExecutor来执行量子程序
class SSASimulatorPython {
public:
    SSASimulatorPython(const std::string& backend_type = "cpu") {
        // 创建解析器和执行引擎
        parser_ = new ssa::SSAParser();
        
        // 根据字符串选择后端
        ssa::BackendType backend = ssa::BackendType::CPU;
        if (backend_type == "biren-gpu-sv") {
            backend = ssa::BackendType::BirenGPU;
        }
        
        executor_ = new ssa::SSAExecutor(backend);
    }
    
    ~SSASimulatorPython() {
        // [BUG 2026.2.7 9:30] 无需释放，会放在析构函数里
        // 释放量子状态
        // executor_->reset();
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
    
    double expect(const std::vector<std::pair<double, std::vector<std::pair<std::string, int>>>>& hamiltonian_terms) {
        double total_expectation = 0.0;
        
        for (const auto& term : hamiltonian_terms) {
            double coefficient = term.first;
            const auto& ops = term.second;
            
            long long n = ops.size();
            if (n == 0) {
                // 处理单位算符项，期望值为1
                total_expectation += coefficient * 1.0;
                continue;
            }
            
            PauliV2* b = new PauliV2[n];
            unsigned int* q = new unsigned int[n];
            
            for (int i = 0; i < n; ++i) {
                const auto& op = ops[i];
                const std::string& op_type = op.first;
                int qubit_idx = op.second;
                
                if (op_type == "x") {
                    b[i] = PauliXV2;
                } else if (op_type == "y") {
                    b[i] = PauliYV2;
                } else if (op_type == "z") {
                    b[i] = PauliZV2;
                } else if (op_type == "i") {
                    b[i] = PauliIV2;
                } else {
                    b[i] = PauliZV2; // 默认使用Z算符
                }
                
                q[i] = qubit_idx;
            }
            
            // 从 qc_runtime_v2.cpp 中我们知道：
            //   Pr(One) = 0.5 * (1 - ⟨σ⟩)
            // 因此：
            //   ⟨σ⟩ = 1 - 2 * Pr(One)
            double pr_one = JointEnsembleProbability_v2(n, b, q);
            double expectation = 1.0 - 2.0 * pr_one;
            total_expectation += coefficient * expectation;
            
            delete[] b;
            delete[] q;
        }
        
        return total_expectation;
    }
    
    std::vector<std::complex<double>> get_state_vector() {
        int size = 0;
        getStateVector_v2(nullptr, nullptr, &size);
        
        std::vector<double> real_part(size);
        std::vector<double> imag_part(size);
        getStateVector_v2(real_part.data(), imag_part.data(), &size);
        
        std::vector<std::complex<double>> state(size);
        for (int i = 0; i < size; ++i) {
            state[i] = std::complex<double>(real_part[i], imag_part[i]);
        }
        
        return state;
    }
    
private:
    ssa::SSAParser* parser_;
    ssa::SSAExecutor* executor_;
    std::string error_message_;
};

// 模块初始化
PYBIND11_MODULE(ssa_simulator_cpp, m) {
    py::class_<SSASimulatorPython>(m, "SSASimulator")
        .def(py::init<const std::string&>(), py::arg("backend_type") = "cpu")
        .def("load_ssa_assembly", &SSASimulatorPython::load_ssa_assembly)
        .def("run", &SSASimulatorPython::run)
        .def("reset", &SSASimulatorPython::reset)
        .def("reset_state", &SSASimulatorPython::reset_state)
        .def("get_measurement_reg_names", &SSASimulatorPython::get_measurement_reg_names)
        .def("sample", &SSASimulatorPython::sample, py::arg("shots_count") = 1000)
        .def("get_error", &SSASimulatorPython::get_error)
        .def("expect", &SSASimulatorPython::expect)
        .def("get_state_vector", &SSASimulatorPython::get_state_vector);
}