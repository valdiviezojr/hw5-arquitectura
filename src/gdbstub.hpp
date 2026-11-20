// gdbstub.hpp — Servidor GDB Remote Serial Protocol (RSP) sobre TCP.
//
// Permite depurar el programa simulado con GDB:
//   rv32sim --gdb 1234 programa.bin
//   gdb -ex "set architecture riscv:rv32" -ex "target remote :1234"
//
// Paquetes soportados: ?, g, G, p, P, m, M, s, c, Z0/z0 (y Z1/z1), qSupported,
// qXfer:features:read (target.xml con x0..x31 + pc), qAttached, H, k, D.
// Solo POSIX (Linux/macOS). Atiende a un único cliente y termina cuando se
// desconecta.
#pragma once

#include <cstdint>

#include "cpu.hpp"

// Devuelve 0 si la sesión terminó normalmente, distinto de 0 ante errores de red.
int run_gdb_server(Cpu& cpu, uint16_t port);
