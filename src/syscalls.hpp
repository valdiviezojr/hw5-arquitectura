// syscalls.hpp — ecall estilo SPIM, compatible con el sistema "RV32 SPIM" de CPUlator
// (https://cpulator.01xz.net/doc/#syscall): número en a7, argumentos en a0/a1,
// resultado en a0.
//
//  1 print_int       a0 = entero con signo
//  4 print_string    a0 = dirección de cadena terminada en '\0'
//  5 read_int        -> a0
//  8 read_string     a0 = buffer, a1 = tamaño máximo (incluye '\0')
//  9 sbrk            a0 = bytes -> a0 = dirección del bloque reservado
// 10 exit
// 11 print_char      a0 = carácter
// 12 read_char       -> a0
// 17 exit2           a0 = código de salida
// 30 time            -> a0 = ms bajos, a1 = ms altos (desde 1970)
// 32 sleep           a0 = ms
// 34 print_hex, 35 print_bin, 36 print_unsigned   a0 = entero
//
// Los syscalls de punto flotante (2, 3, 6, 7) no aplican a RV32I y, como
// cualquier número desconocido, detienen la simulación con un error explícito.
#pragma once

#include <cstdint>
#include <iosfwd>

#include "cpu.hpp"

// Inicio del heap para sbrk (lejos del código, datos de ejemplo y pila).
constexpr uint32_t HEAP_BASE = 0x10000000;

// Instala en `cpu` un manejador de ecall que usa `in`/`out` como consola.
void install_spim_syscalls(Cpu& cpu, std::istream& in, std::ostream& out);
