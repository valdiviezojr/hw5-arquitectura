cmake_minimum_required(VERSION 3.16)
project(rv32sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Sanitizers opcionales: cmake -DRV32SIM_SANITIZE=ON
option(RV32SIM_SANITIZE "Compilar con -fsanitize=address,undefined" OFF)

add_compile_options(-Wall -Wextra -Wpedantic)
if(RV32SIM_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

# Núcleo del simulador (compartido por el ejecutable y los tests).
add_library(rv32core STATIC
  src/memory.cpp
  src/decode.cpp
  src/cpu.cpp
  src/loader.cpp
  src/disasm.cpp
  src/syscalls.cpp
  src/elf.cpp
  src/gdbstub.cpp
)
target_include_directories(rv32core PUBLIC src)

add_executable(rv32sim src/main.cpp)
target_link_libraries(rv32sim PRIVATE rv32core)

# Helper (no es parte del simulador): genera tests/bin/ecall_demo.{bin,elf}.
add_executable(mkdemo tools/mkdemo.cpp)
add_custom_target(demo-bins COMMAND mkdemo ${CMAKE_SOURCE_DIR}/tests/bin DEPENDS mkdemo)

# Tests unitarios (sin dependencias externas).
enable_testing()
add_executable(rv32sim_tests tests/unit/test_main.cpp)
target_link_libraries(rv32sim_tests PRIVATE rv32core)
add_test(NAME unit COMMAND rv32sim_tests)
add_test(NAME programs
  COMMAND ${CMAKE_SOURCE_DIR}/tests/run_programs.sh $<TARGET_FILE:rv32sim>
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
