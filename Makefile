CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread -finput-charset=utf-8 -fexec-charset=utf-8
LDFLAGS  ?= -pthread

SRCS = log_crypto.cpp metrics.cpp log_writer.cpp logger.cpp \
       test_business.cpp benchmark.cpp main.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = $(wildcard *.h)
TARGET = logsys

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) run

test: $(TARGET)
	./$(TARGET) test

bench: $(TARGET)
	./$(TARGET) bench

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf logs bench_logs test_logs_*

.PHONY: all run test bench clean
