CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread -finput-charset=utf-8 -fexec-charset=utf-8
LDFLAGS  ?= -pthread

SRC_DIR   := src
TEST_DIR  := test
BUILD_DIR := build
BIN_DIR   := bin
RUNTIME_DIR := runtime

SRCS = $(SRC_DIR)/log_crypto.cpp $(SRC_DIR)/metrics.cpp $(SRC_DIR)/log_writer.cpp $(SRC_DIR)/logger.cpp \
       $(TEST_DIR)/test_business.cpp benchmark.cpp main.cpp
OBJS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(wildcard $(SRC_DIR)/*.h) $(wildcard $(TEST_DIR)/*.h)
TARGET = $(BIN_DIR)/logsys

all: dirs $(TARGET)

dirs:
	mkdir -p $(BUILD_DIR) $(BIN_DIR) $(RUNTIME_DIR)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.cpp $(DEPS) | dirs
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I./$(SRC_DIR) -c $< -o $@

run: dirs $(TARGET)
	./$(TARGET) run

test: dirs $(TARGET)
	./$(TARGET) test

bench: dirs $(TARGET)
	./$(TARGET) bench

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all run test bench clean
