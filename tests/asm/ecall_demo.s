# ecall_demo.s — Programa de demostración de syscalls estilo SPIM (CPUlator RV32 SPIM).
# NOTA: este fuente es documentación; los binarios tests/bin/ecall_demo.{bin,elf}
# los genera tools/mkdemo.cpp con el codificador de tests (no hay ensamblador en el repo).
# Salida esperada: "Suma 1..10 = 55" y fin con exit2(0).
.global _start
_start:
        la      a0, msg           # lui + addi
        li      a7, 4             # print_string
        ecall
        li      t0, 0             # suma
        li      t1, 10            # i
loop:   add     t0, t0, t1
        addi    t1, t1, -1
        bnez    t1, loop
        mv      a0, t0
        li      a7, 1             # print_int
        ecall
        li      a0, '\n'
        li      a7, 11            # print_char
        ecall
        li      a0, 0
        li      a7, 17            # exit2(a0)
        ecall
        .org    0x100
msg:    .asciz  "Suma 1..10 = "
