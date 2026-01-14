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

# 基础编译选项
CXXFLAGS = -std=c++17 -O3 -Wall -fPIC

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

# 链接选项
ifeq ($(OS_TYPE),Windows)
    LDFLAGS = -shared
else ifeq ($(OS_TYPE),macOS)
    LDFLAGS = -bundle -undefined dynamic_lookup $(if $(filter ARM,$(ARCH_TYPE)),-arch arm64,-arch x86_64)
else ifeq ($(OS_TYPE),Linux)
    LDFLAGS = -shared
endif

# Python设置
PYTHON ?= python3
PYTHON_INCLUDE = $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_path('include'))")
PYTHON_LIB = $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_config_var('LIBDIR') or '')")
PYTHON_LD = $(shell $(PYTHON) -c "import sysconfig; print(' '.join(sysconfig.get_config_var('LIBS').split()) if sysconfig.get_config_var('LIBS') else '')")
PYTHON_LDFLAGS = $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_config_var('LDFLAGS') or '')")
PYBIND11_INCLUDE = $(shell $(PYTHON) -c "import pybind11; print(pybind11.get_include())")

# 完整版本：包含所有必要的源文件，使用真正的qc_runtime
SRCS = pybind11_interface.cpp ssa_parser.cpp ssa_executor.cpp qc_runtime_cpu/qc_runtime.cpp
OBJS = $(SRCS:.cpp=.o)

# 确定Python扩展模块后缀
PY_SUFFIX := $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX') or '.so')")
TARGET = ssa_simulator_cpp$(PY_SUFFIX)

# 头文件包含路径
INCLUDES = -I$(PYTHON_INCLUDE) -I$(PYBIND11_INCLUDE) -I.

# 链接库路径
LIBS = $(if $(PYTHON_LIB),-L$(PYTHON_LIB),) $(PYTHON_LD) $(PYTHON_LDFLAGS)

# 默认目标
all: $(TARGET) ssa_simulation_runner$(EXE_EXT)

# 调试模式目标
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: all

# 编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

qc_runtime_cpu/%.o: qc_runtime_cpu/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 链接规则
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) $(LIBS) -o $(TARGET)

# 二进制目标
BIN_OBJS = ssa_parser.o ssa_executor.o qc_runtime_cpu/qc_runtime.o

# ssa_simulation_runner 目标
ssa_simulation_runner$(EXE_EXT): ssa_simulation_runner.cpp $(BIN_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) ssa_simulation_runner.cpp $(BIN_OBJS) -o $@



# 清理规则
clean:
	$(RM) $(OBJS) $(TARGET) $(BIN_OBJS) ssa_simulation_runner$(EXE_EXT)

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
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "PYTHON: $(PYTHON)"
	@echo "PYTHON_INCLUDE: $(PYTHON_INCLUDE)"
	@echo "PYBIND11_INCLUDE: $(PYBIND11_INCLUDE)"
	@echo "TARGET: $(TARGET)"
	@echo "SRCS: $(SRCS)"
	@echo "OBJS: $(OBJS)"
