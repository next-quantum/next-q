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

# 基础编译选项
CXXFLAGS = -std=c++17 -O3 -Wall -fPIC
ifeq ($(BIREN_SUPPORT), 1)
    CXXFLAGS += -DENABLE_BIREN
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
BIREN_FLAGS = -std=c++17 -O3 -fPIC -x supa
BIREN_LINK_FLAGS = --supa-link -O2

# 完整版本：包含所有必要的源文件，使用真正的qc_runtime_v2
CPP_SRCS = pybind11_interface.cpp ssa_parser.cpp ssa_executor.cpp qc_runtime/qc_runtime_v2.cpp qc_runtime/qc_simulator_default_cpu_sv_v2.cpp qc_runtime/qc_simulator_v2.cpp

# 壁仞GPU源文件
ifeq ($(BIREN_SUPPORT), 1)
    SU_SRCS = qc_runtime/qc_simulator_biren_gpu_sv_v2.su qc_runtime/supa_utils.su qc_runtime/qc_kernels_supa.su
else
    SU_SRCS =
endif

CPP_OBJS = $(CPP_SRCS:.cpp=.o)
SU_OBJS = $(SU_SRCS:.su=.o)
OBJS = $(CPP_OBJS) $(SU_OBJS)

# 确定Python扩展模块后缀
PY_SUFFIX := $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX') or '.so')")
TARGET = ssa_simulator_cpp$(PY_SUFFIX)

# 头文件包含路径
INCLUDES = -I$(PYTHON_INCLUDE) -I$(PYBIND11_INCLUDE) -I.

# 链接库路径
ifeq ($(OS_TYPE),macOS)
    # macOS 特殊处理：使用 -undefined dynamic_lookup 而不是显式链接 Python 库
    LIBS =
else
    # Linux/Windows 使用常规链接方式
    LIBS = $(if $(PYTHON_LIB),-L$(PYTHON_LIB),) $(PYTHON_LD) $(PYTHON_LDFLAGS)
endif

# 默认目标
all: $(TARGET) ssa_simulation_runner$(EXE_EXT)

# 调试模式目标
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: all

# 编译规则 - C++文件（用默认编译器）
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

qc_runtime/%.o: qc_runtime/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# SU文件编译规则（壁仞GPU）- 仅当启用壁仞支持时，用 brcc 编译
ifeq ($(BIREN_SUPPORT), 1)
qc_runtime/%.o: qc_runtime/%.su
	$(BIREN_CC) $(BIREN_FLAGS) $(INCLUDES) -c $< -o $@
endif

# 链接规则 - 根据是否启用壁仞支持选择链接器
ifeq ($(BIREN_SUPPORT), 1)
# 使用 brcc 和 --supa-link 进行链接（官方推荐方式）
$(TARGET): $(OBJS)
ifeq ($(OS_TYPE),macOS)
	$(BIREN_CC) $(BIREN_LINK_FLAGS) -shared $(OBJS) $(LIBS) -undefined dynamic_lookup -o $(TARGET)
else
	$(BIREN_CC) $(BIREN_LINK_FLAGS) -shared $(OBJS) $(LIBS) -o $(TARGET)
endif
else
# 使用默认编译器进行链接
$(TARGET): $(OBJS)
ifeq ($(OS_TYPE),macOS)
	$(CXX) -shared $(OBJS) $(LIBS) -undefined dynamic_lookup -o $(TARGET)
else
	$(CXX) -shared $(OBJS) $(LIBS) -o $(TARGET)
endif
endif

# 二进制目标
BIN_CPP_OBJS = ssa_parser.o ssa_executor.o qc_runtime/qc_runtime_v2.o qc_runtime/qc_simulator_default_cpu_sv_v2.o qc_runtime/qc_simulator_v2.o
ifeq ($(BIREN_SUPPORT), 1)
    BIN_OBJS = $(BIN_CPP_OBJS) qc_runtime/qc_simulator_biren_gpu_sv_v2.o qc_runtime/supa_utils.o qc_runtime/qc_kernels_supa.o
else
    BIN_OBJS = $(BIN_CPP_OBJS)
endif

# ssa_simulation_runner 目标
ifeq ($(BIREN_SUPPORT), 1)
# 使用 brcc 和 --supa-link 进行链接
ssa_simulation_runner$(EXE_EXT): ssa_simulation_runner.cpp $(BIN_OBJS)
	$(BIREN_CC) $(BIREN_LINK_FLAGS) $(CXXFLAGS) $(INCLUDES) ssa_simulation_runner.cpp $(BIN_OBJS) $(LIBS) -o $@
else
# 使用默认编译器进行链接
ssa_simulation_runner$(EXE_EXT): ssa_simulation_runner.cpp $(BIN_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) ssa_simulation_runner.cpp $(BIN_OBJS) -o $@
endif

# 清理规则
clean:
	$(RM) $(CPP_OBJS) $(SU_OBJS) $(TARGET) $(BIN_OBJS) ssa_simulation_runner$(EXE_EXT)

# 测试规则
test:
	$(PYTHON) -c "import sys; import os; sys.path.insert(0, os.path.dirname(os.path.abspath('.'))); import ssa_simulator_cpp; print('Module imported successfully!')"
	$(PYTHON) -c "import sys; import os; sys.path.insert(0, os.path.dirname(os.path.abspath('.'))); from ssa_simulator_cpp import SSASimulator; sim = SSASimulator(); print('SSASimulator created successfully!')"

# 显示变量
show:
	@echo "OS_TYPE: $(OS_TYPE)"
	@echo "ARCH_TYPE: $(ARCH_TYPE)"
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "BIREN_SUPPORT: $(BIREN_SUPPORT)"
	@echo "BIREN_CC: $(BIREN_CC)"
	@echo "PYTHON: $(PYTHON)"
	@echo "PYTHON_INCLUDE: $(PYTHON_INCLUDE)"
	@echo "PYBIND11_INCLUDE: $(PYBIND11_INCLUDE)"
	@echo "TARGET: $(TARGET)"
	@echo "CPP_SRCS: $(CPP_SRCS)"
	@echo "SU_SRCS: $(SU_SRCS)"
	@echo "OBJS: $(OBJS)"
