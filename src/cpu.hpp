// cpu.hpp — Estado arquitectónico (PC, x0..x31, memoria) y ejecución de RV32I.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "decode.hpp"
#include "memory.hpp"

// Valor inicial de sp (x2) para programas que usan pila sin inicializarla.
constexpr uint32_t DEFAULT_SP = 0x00100000;

enum class Status {
    Ok,      // la instrucción se ejecutó con normalidad
    Halted,  // fin del programa (ecall exit / ebreak)
    Error,   // instrucción inválida, PC desalineado, syscall desconocida...
};

class Cpu;

// Manejador de `ecall`. Devuelve Ok para continuar, Halted para terminar o
// Error (con mensaje en `msg`) para detener la simulación.
using EcallHandler = std::function<Status(Cpu&, std::string& msg)>;

class Cpu {
public:
    Memory mem;
    uint32_t pc = 0;
    uint64_t instret = 0;  // instrucciones ejecutadas con éxito
    int exit_code = 0;
    EcallHandler ecall_handler;  // sin handler, ecall es un error explícito

    Cpu() { reset(0, DEFAULT_SP); }

    // Reinicia registros y PC (no toca la memoria).
    void reset(uint32_t entry_pc, uint32_t sp = DEFAULT_SP);

    uint32_t reg(unsigned i) const { return i ? x_[i & 31] : 0; }
    void set_reg(unsigned i, uint32_t v) {
        if (i & 31) x_[i & 31] = v;  // x0 siempre vale 0: se ignora la escritura
    }

    // Ejecuta exactamente una instrucción en `pc`.
    Status step();

    bool halted() const { return halted_; }
    // Mensaje de la última detención (Halted o Error).
    const std::string& message() const { return message_; }

    // Marca el fin del programa (usado por los syscalls de salida).
    void halt(int code, const std::string& why) {
        halted_ = true;
        exit_code = code;
        message_ = why;
    }

private:
    uint32_t x_[32] = {};
    bool halted_ = false;
    std::string message_;

    Status fail(uint32_t inst, const std::string& what);
};
