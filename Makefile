# Makefile de respaldo (equivalente al CMakeLists.txt) para entornos sin CMake.
#   make            -> build/rv32sim y build/rv32sim_tests
#   make test       -> tests unitarios + programas del enunciado
#   make SANITIZE=1 -> compila con -fsanitize=address,undefined (en build-san/)
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2
CXXFLAGS += -Wall -Wextra -Wpedantic -Isrc
BUILD    ?= build

ifeq ($(SANITIZE),1)
  BUILD     = build-san
  CXXFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1
  LDFLAGS  += -fsanitize=address,undefined
endif

CORE_SRC := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
CORE_OBJ := $(CORE_SRC:src/%.cpp=$(BUILD)/obj/%.o)
HEADERS  := $(wildcard src/*.hpp) $(wildcard tests/unit/*.hpp)

all: $(BUILD)/rv32sim $(BUILD)/rv32sim_tests $(BUILD)/mkdemo

$(BUILD)/obj/%.o: src/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/rv32sim: $(BUILD)/obj/main.o $(CORE_OBJ)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BUILD)/rv32sim_tests: tests/unit/test_main.cpp $(CORE_OBJ) $(HEADERS)
	$(CXX) $(CXXFLAGS) tests/unit/test_main.cpp $(CORE_OBJ) -o $@ $(LDFLAGS)

# Helper que genera tests/bin/ecall_demo.{bin,elf} (make demo-bins).
$(BUILD)/mkdemo: tools/mkdemo.cpp $(HEADERS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) tools/mkdemo.cpp -o $@ $(LDFLAGS)

demo-bins: $(BUILD)/mkdemo
	./$(BUILD)/mkdemo tests/bin

test: all
	./$(BUILD)/rv32sim_tests
	./tests/run_programs.sh ./$(BUILD)/rv32sim

clean:
	rm -rf build build-san

.PHONY: all test clean demo-bins
