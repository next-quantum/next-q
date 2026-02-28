# Makefile for SSA Simulator Python Extension

# 检测操作系统
ifeq ($(OS),Windows_NT)
    OS_TYPE := Windows
else
    OS_TYPE := $(shell uname -s)
    ifeq ($(OS_TYPE),Darwin)
        OS_TYPE := macOS
    endif
endif

# 检测架构
ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
    ARCH_TYPE := X86
else ifeq ($(ARCH),amd64)
    ARCH_TYPE := X86
else ifeq ($(ARCH),aarch64)
    ARCH_TYPE := ARM
else ifeq ($(ARCH),arm64)
    ARCH_TYPE := ARM
else ifeq ($(ARCH),riscv64)
    ARCH_TYPE := RISCV
else ifeq ($(ARCH),riscv32)
    ARCH_TYPE := RISCV
else
    ARCH_TYPE := X86
endif

# 编译器设置
CXX ?= g++

# 壁仞GPU支持
ifdef enable-biren
    BIREN_SUPPORT := 1
else
    BIREN_SUPPORT := 0
endif

# 摩尔线程GPU支持
ifdef enable-moorethread
    MOORETHREAD_SUPPORT := 1
else
    MOORETHREAD_SUPPORT := 0
endif

# 基础编译选项
CXXFLAGS = -std=c++17 -O3 -Wall -fPIC
ifeq ($(BIREN_SUPPORT), 1)
    CXXFLAGS += -DENABLE_BIREN
endif
ifeq ($(MOORETHREAD_SUPPORT), 1)
    CXXFLAGS += -DENABLE_MOORETHREAD
endif

# 调试模式选项
DEBUG_FLAGS = -DDEBUG_OUTPUT -DDEBUG

# 平台特定编译选项
ifeq ($(OS_TYPE),Windows)
    CXX ?= g++
    CXXFLAGS += -fpermissive
    LIB_EXT := .dll
    EXE_EXT := .exe
    RM := del /f /q
else ifeq ($(OS_TYPE),macOS)
    CXX ?= g++
    CXXFLAGS += $(if $(filter ARM,$(ARCH_TYPE)),-arch arm64,-arch x86_64)
    LIB_EXT := .so
    EXE_EXT := 
    RM := rm -f
else ifeq ($(OS_TYPE),Linux)
    CXX ?= g++
    ifeq ($(ARCH_TYPE),RISCV)
        CXXFLAGS += -march=rv64gc
    endif
    LIB_EXT := .so
    EXE_EXT := 
    RM := rm -f
endif

# Python设置
PYTHON ?= python3
PYTHON_INCLUDE = $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_path('include'))")
PYTHON_LIB = $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_config_var('LIBDIR') or '')")
PYTHON_LD = $(shell $(PYTHON) -c "import sysconfig; print(' '.join(sysconfig.get_config_var('LIBS').split()) if sysconfig.get_config_var('LIBS') else '')")
PYTHON_LDFLAGS = $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_config_var('LDFLAGS') or '')")
PYBIND11_INCLUDE = $(shell $(PYTHON) -c "import pybind11; print(pybind11.get_include())")

# 壁仞GPU编译设置
BIREN_CC ?= brcc
# 从brcc命令自动获取SUPA安装路径
BIREN_PATH = $(shell $(BIREN_CC) -v -x supa -c /dev/null 2>&1 | grep "Found SUPA installation:" | awk '{print $$4}' | sed 's/,//')
# 构造SUPA include目录路径
BIREN_INCLUDE = $(BIREN_PATH)/include
BIREN_FLAGS = -std=c++17 -O3 -fPIC -x supa
BIREN_LINK_FLAGS = --supa-link -O2

# 摩尔线程GPU编译设置
MOORETHREAD_CC ?= mcc
# 从mcc命令自动获取MUSA安装路径
MUSA_PATH = $(shell $(MOORETHREAD_CC) -v -x c++ -c /dev/null 2>&1 | grep "Found MUSA installation:" | awk '{print $$4}' | sed 's/,//')
# 构造MUSA include目录路径
MOORETHREAD_INCLUDE = $(MUSA_PATH)/include
MOORETHREAD_FLAGS = -std=c++17 -O3 -fPIC -x musa
MOORETHREAD_LINK_FLAGS = -O2

# 自动检测BR110芯片
ifeq ($(BIREN_SUPPORT), 1)
    BIREN_GPU_NAME := $(shell brsmi gpu --query-gpu=name --format=csv,noheader 2>/dev/null)
    ifneq ($(findstring 110,$(BIREN_GPU_NAME)),)
        BIREN_FLAGS += --supa-gpu-arch=br110
        BIREN_LINK_FLAGS += --supa-gpu-arch=br110
    endif
endif

# 确定Python扩展模块后缀
PY_SUFFIX := $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX') or '.so')")

# 目标文件定义
TARGET_CPU = ssa_simulator_cpp_default_cpu_sv$(PY_SUFFIX)
TARGET_BIREN_GPU = ssa_simulator_cpp_biren_gpu_sv$(PY_SUFFIX)
TARGET_MOORETHREAD_GPU = ssa_simulator_cpp_moorethread_gpu_sv$(PY_SUFFIX)

# 完整版本：包含所有必要的源文件，使用真正的qc_runtime_v2
# CPU后端源文件
CPP_SRCS_CPU = pybind11_interface.cpp ssa_parser.cpp ssa_executor.cpp qc_runtime/qc_runtime_v2.cpp qc_runtime/qc_simulator_default_cpu_sv_v2.cpp qc_runtime/qc_simulator_v2.cpp

# 壁仞GPU后端源文件
CPP_SRCS_BIREN_GPU = pybind11_interface.cpp ssa_parser.cpp ssa_executor.cpp qc_runtime/qc_runtime_v2.cpp qc_runtime/qc_simulator_default_cpu_sv_v2.cpp qc_runtime/qc_simulator_v2.cpp
SU_SRCS_BIREN_GPU = qc_runtime/qc_simulator_biren_gpu_sv_v2.su qc_runtime/supa_utils.su qc_runtime/qc_kernels_supa.su

# 摩尔线程GPU后端源文件
CPP_SRCS_MOORETHREAD_GPU = pybind11_interface.cpp ssa_parser.cpp ssa_executor.cpp qc_runtime/qc_runtime_v2.cpp qc_runtime/qc_simulator_default_cpu_sv_v2.cpp qc_runtime/qc_simulator_v2.cpp
MU_SRCS_MOORETHREAD_GPU = qc_runtime/qc_simulator_moorethread_gpu_sv_v2.mu qc_runtime/musa_utils.mu qc_runtime/qc_kernels_musa.mu

# 目标文件
CPP_OBJS_CPU = $(CPP_SRCS_CPU:.cpp=.cpu.o)
CPP_OBJS_BIREN_GPU = $(CPP_SRCS_BIREN_GPU:.cpp=.biren_gpu.o)
SU_OBJS_BIREN_GPU = $(SU_SRCS_BIREN_GPU:.su=.biren_gpu.o)
CPP_OBJS_MOORETHREAD_GPU = $(CPP_SRCS_MOORETHREAD_GPU:.cpp=.moorethread_gpu.o)
MU_OBJS_MOORETHREAD_GPU = $(MU_SRCS_MOORETHREAD_GPU:.mu=.moorethread_gpu.o)
OBJS_CPU = $(CPP_OBJS_CPU)
OBJS_BIREN_GPU = $(CPP_OBJS_BIREN_GPU) $(SU_OBJS_BIREN_GPU)
OBJS_MOORETHREAD_GPU = $(CPP_OBJS_MOORETHREAD_GPU) $(MU_OBJS_MOORETHREAD_GPU)

# 头文件包含路径
INCLUDES = -I$(PYTHON_INCLUDE) -I$(PYBIND11_INCLUDE) -I.
ifeq ($(BIREN_SUPPORT), 1)
    INCLUDES += -I$(BIREN_INCLUDE)
endif
ifeq ($(MOORETHREAD_SUPPORT), 1)
    INCLUDES += -I$(MOORETHREAD_INCLUDE)
endif

# 链接库路径
ifeq ($(OS_TYPE),macOS)
    # macOS 特殊处理：使用 -undefined dynamic_lookup 而不是显式链接 Python 库
    LIBS = 
    BIREN_LIBS = $(LIBS)
    MOORETHREAD_LIBS = $(LIBS) -lmusart
else
    # Linux/Windows 使用常规链接方式
    LIBS = $(if $(PYTHON_LIB),-L$(PYTHON_LIB),) $(PYTHON_LD) $(PYTHON_LDFLAGS)
    BIREN_LIBS = $(LIBS)
    MOORETHREAD_LIBS = $(LIBS) -lmusart
endif

# 默认目标
all: $(TARGET_CPU) ssa_simulation_runner$(EXE_EXT)

# 构建壁仞GPU后端（如果启用）
ifeq ($(BIREN_SUPPORT), 1)
all: $(TARGET_BIREN_GPU)
endif

# 构建摩尔线程GPU后端（如果启用）
ifeq ($(MOORETHREAD_SUPPORT), 1)
all: $(TARGET_MOORETHREAD_GPU)
endif

# 调试模式目标
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: all

# 编译规则 - C++文件（用默认编译器）
%.biren_gpu.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

qc_runtime/%.biren_gpu.o: qc_runtime/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

%.moorethread_gpu.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

qc_runtime/%.moorethread_gpu.o: qc_runtime/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# SU文件编译规则（壁仞GPU）- 仅当启用壁仞支持时，用 brcc 编译
ifeq ($(BIREN_SUPPORT), 1)
qc_runtime/%.biren_gpu.o: qc_runtime/%.su
	$(BIREN_CC) $(BIREN_FLAGS) $(INCLUDES) -c $< -o $@
endif

# MU文件编译规则（摩尔线程GPU）- 仅当启用摩尔线程支持时，用 mcc 编译
ifeq ($(MOORETHREAD_SUPPORT), 1)
qc_runtime/%.moorethread_gpu.o: qc_runtime/%.mu
	$(MOORETHREAD_CC) $(MOORETHREAD_FLAGS) $(INCLUDES) -c $< -o $@
endif

# 编译规则 - CPU后端（不启用BIREN）
%.cpu.o: %.cpp
	$(CXX) -std=c++17 -O3 -Wall -fPIC $(INCLUDES) -c $< -o $@

qc_runtime/%.cpu.o: qc_runtime/%.cpp
	$(CXX) -std=c++17 -O3 -Wall -fPIC $(INCLUDES) -c $< -o $@

# 链接规则 - CPU后端
$(TARGET_CPU): $(OBJS_CPU)
ifeq ($(OS_TYPE),macOS)
	$(CXX) -shared $(OBJS_CPU) $(LIBS) -undefined dynamic_lookup -o $(TARGET_CPU)
else
	$(CXX) -shared $(OBJS_CPU) $(LIBS) -o $(TARGET_CPU)
endif

# 链接规则 - 壁仞GPU后端（仅当启用壁仞支持时）
ifeq ($(BIREN_SUPPORT), 1)
$(TARGET_BIREN_GPU): $(OBJS_BIREN_GPU)
ifeq ($(OS_TYPE),macOS)
	$(BIREN_CC) $(BIREN_LINK_FLAGS) -shared $(OBJS_BIREN_GPU) $(BIREN_LIBS) -undefined dynamic_lookup -o $(TARGET_BIREN_GPU)
else
	$(BIREN_CC) $(BIREN_LINK_FLAGS) -shared $(OBJS_BIREN_GPU) $(BIREN_LIBS) -o $(TARGET_BIREN_GPU)
endif
endif

# 链接规则 - 摩尔线程GPU后端（仅当启用摩尔线程支持时）
ifeq ($(MOORETHREAD_SUPPORT), 1)
$(TARGET_MOORETHREAD_GPU): $(OBJS_MOORETHREAD_GPU)
ifeq ($(OS_TYPE),macOS)
	$(MOORETHREAD_CC) $(MOORETHREAD_LINK_FLAGS) -shared $(OBJS_MOORETHREAD_GPU) $(MOORETHREAD_LIBS) -undefined dynamic_lookup -o $(TARGET_MOORETHREAD_GPU)
else
	$(MOORETHREAD_CC) $(MOORETHREAD_LINK_FLAGS) -shared $(OBJS_MOORETHREAD_GPU) $(MOORETHREAD_LIBS) -o $(TARGET_MOORETHREAD_GPU)
endif
endif

# 二进制目标
BIN_CPP_OBJS = ssa_parser.cpu.o ssa_executor.cpu.o qc_runtime/qc_runtime_v2.cpu.o qc_runtime/qc_simulator_default_cpu_sv_v2.cpu.o qc_runtime/qc_simulator_v2.cpu.o
BIN_OBJS = $(BIN_CPP_OBJS)

# ssa_simulation_runner 目标
ssa_simulation_runner.cpu.o: ssa_simulation_runner.cpp
	$(CXX) -std=c++17 -O3 -Wall -fPIC $(INCLUDES) -c $< -o $@

# 链接规则 - 统一使用默认编译器
ssa_simulation_runner$(EXE_EXT): ssa_simulation_runner.cpu.o $(BIN_OBJS)
	$(CXX) -std=c++17 -O3 -Wall -fPIC $(INCLUDES) ssa_simulation_runner.cpu.o $(BIN_OBJS) $(LIBS) -o $@

# 清理规则
clean:
	$(RM) *.cpu.o *.biren_gpu.o *.moorethread_gpu.o qc_runtime/*.cpu.o qc_runtime/*.biren_gpu.o qc_runtime/*.moorethread_gpu.o $(SU_OBJS_BIREN_GPU) $(MU_OBJS_MOORETHREAD_GPU) $(TARGET_CPU) $(TARGET_BIREN_GPU) $(TARGET_MOORETHREAD_GPU) $(BIN_OBJS) ssa_simulation_runner.cpu.o ssa_simulation_runner$(EXE_EXT)

# 测试规则
test:
	$(PYTHON) -c "import sys; import os; sys.path.insert(0, os.path.dirname(os.path.abspath('.'))); import ssa_simulator_cpp_default_cpu_sv; print('CPU module imported successfully!')"
	$(PYTHON) -c "import sys; import os; sys.path.insert(0, os.path.dirname(os.path.abspath('.'))); from ssa_simulator_cpp_default_cpu_sv import DefaultCPUSVSSASimulator; sim = DefaultCPUSVSSASimulator(); print('CPU SSASimulator created successfully!')"
ifeq ($(BIREN_SUPPORT), 1)
	$(PYTHON) -c "import sys; import os; sys.path.insert(0, os.path.dirname(os.path.abspath('.'))); import ssa_simulator_cpp_biren_gpu_sv; print('Biren GPU module imported successfully!')"
	$(PYTHON) -c "import sys; import os; sys.path.insert(0, os.path.dirname(os.path.abspath('.'))); from ssa_simulator_cpp_biren_gpu_sv import BirenGPUSVSSASimulator; sim = BirenGPUSVSSASimulator(); print('Biren GPU SSASimulator created successfully!')"
endif
ifeq ($(MOORETHREAD_SUPPORT), 1)
	$(PYTHON) -c "import sys; import os; sys.path.insert(0, os.path.dirname(os.path.abspath('.'))); import ssa_simulator_cpp_moorethread_gpu_sv; print('Moore Thread GPU module imported successfully!')"
	$(PYTHON) -c "import sys; import os; sys.path.insert(0, os.path.dirname(os.path.abspath('.'))); from ssa_simulator_cpp_moorethread_gpu_sv import MooreThreadGPUSVSSASimulator; sim = MooreThreadGPUSVSSASimulator(); print('Moore Thread GPU SSASimulator created successfully!')"
endif

# 显示变量
show:
	@echo "OS_TYPE: $(OS_TYPE)"
	@echo "ARCH_TYPE: $(ARCH_TYPE)"
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "BIREN_SUPPORT: $(BIREN_SUPPORT)"
	@echo "BIREN_CC: $(BIREN_CC)"
	@echo "BIREN_FLAGS: $(BIREN_FLAGS)"
	@echo "BIREN_LINK_FLAGS: $(BIREN_LINK_FLAGS)"
	@echo "BIREN_LIBS: $(BIREN_LIBS)"
	@echo "MOORETHREAD_SUPPORT: $(MOORETHREAD_SUPPORT)"
	@echo "MOORETHREAD_CC: $(MOORETHREAD_CC)"
	@echo "MUSA_PATH: $(MUSA_PATH)"
	@echo "MOORETHREAD_INCLUDE: $(MOORETHREAD_INCLUDE)"
	@echo "MOORETHREAD_FLAGS: $(MOORETHREAD_FLAGS)"
	@echo "MOORETHREAD_LINK_FLAGS: $(MOORETHREAD_LINK_FLAGS)"
	@echo "MOORETHREAD_LIBS: $(MOORETHREAD_LIBS)"
	@echo "PYTHON: $(PYTHON)"
	@echo "PYTHON_INCLUDE: $(PYTHON_INCLUDE)"
	@echo "PYBIND11_INCLUDE: $(PYBIND11_INCLUDE)"
	@echo "TARGET_CPU: $(TARGET_CPU)"
	@echo "TARGET_BIREN_GPU: $(TARGET_BIREN_GPU)"
	@echo "TARGET_MOORETHREAD_GPU: $(TARGET_MOORETHREAD_GPU)"
	@echo "CPP_SRCS_CPU: $(CPP_SRCS_CPU)"
	@echo "CPP_SRCS_BIREN_GPU: $(CPP_SRCS_BIREN_GPU)"
	@echo "SU_SRCS_BIREN_GPU: $(SU_SRCS_BIREN_GPU)"
	@echo "CPP_SRCS_MOORETHREAD_GPU: $(CPP_SRCS_MOORETHREAD_GPU)"
	@echo "MU_SRCS_MOORETHREAD_GPU: $(MU_SRCS_MOORETHREAD_GPU)"
	@echo "OBJS_CPU: $(OBJS_CPU)"
	@echo "OBJS_BIREN_GPU: $(OBJS_BIREN_GPU)"
	@echo "OBJS_MOORETHREAD_GPU: $(OBJS_MOORETHREAD_GPU)"
