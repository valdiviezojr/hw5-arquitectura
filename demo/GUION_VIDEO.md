# Guion del video demostrativo (≈ 5–7 min)

Preparación: terminal con fuente grande, en la raíz del repo, ya compilado (`make` o
`cmake --build build`). Cada ejemplo se puede correr solo con `demo/demo.sh N` (pausa con Enter
entre ejemplos) o tecleando los comandos a mano en `./build/rv32sim <programa>`.

---

## 0. Introducción (≈ 45 s)

- "Este es `rv32sim`, un simulador de software del ISA RV32I escrito en C++17."
- Mostrar rápidamente el árbol del repo: `src/` (cpu, memory, decode, disasm, syscalls, elf,
  gdbstub, main = REPL), `tests/`.
- Puntos clave: carga un binario crudo en `0x0` (exportado de CPUlator) o un ELF; memoria de
  32 bits little-endian dispersa por páginas de 4 KiB; las 37 instrucciones de RV32I; ejecución
  paso a paso; inspección de PC, registros y memoria.
- Mostrar `make test`: 40 tests unitarios y los programas del enunciado en verde.

## 1. Instrucciones aritméticas paso a paso (≈ 1 min) — `demo/demo.sh 1`

```
./build/rv32sim tests/bin/riscvtest.bin
> pc
> dis 0 6
> step          # addi x2, x0, 5
> regs x2
> pc            # avanzó a 0x4
> step          # addi x3, x0, 12
> regs x3
> step          # addi x7, x3, -9   -> inmediato negativo
> regs x7       # 3
> step          # or  x4, x7, x2    -> 3 | 5 = 7
> regs x4
> step          # and x5, x3, x4    -> 12 & 7 = 4
> step          # add x5, x5, x4    -> 11
> regs x5 x4 t0 tp
```

Qué decir: cada `step` muestra la dirección, la palabra en hex y el desensamblado; los registros
se pueden pedir como `x5` o por nombre ABI (`t0`); el PC avanza de 4 en 4.

## 2. `riscvtest.s` escribe 25 en la dirección 100 (≈ 45 s) — `demo/demo.sh 2`

```
./build/rv32sim tests/bin/riscvtest.bin
> mem 0x64 0x67      # 00 00 00 00
> run
> mem 0x64 0x67      # 19 00 00 00  -> 0x19 = 25, little-endian
> memw 100
> regs x2 x9
```

Qué decir: el programa de Harris & Harris prueba add, sub, and, or, slt, addi, lw, sw, beq y jal;
si todo funciona escribe 25 en la dirección 100. Señalar el orden de bytes little-endian.

## 3. Quicksort (≈ 1 min 15 s) — `demo/demo.sh 3`

```
./build/rv32sim tests/bin/quicksort.bin
> break 0x48          # justo antes de "call _qsort"
> run
> mem 0x1000 0x101B   # 6 4 3 2 1 8 9
> memw 0x1000 7
> run                 # se detiene en el "j _end" final
> mem 0x1000 0x101B   # 1 2 3 4 6 8 9
> memw 0x1000 7
```

Qué decir: el programa es recursivo (usa `call`/`ret` = `jal`/`jalr` y la pila con `sw`/`lw`);
`run` se detiene solo al detectar el bucle `j _end` (instrucción que salta a sí misma).

## 4. Árbol simétrico (≈ 1 min) — `demo/demo.sh 4`

```
./build/rv32sim tests/bin/simetrico.bin
> break 0x44          # "call is_symmetric"
> run
> memw 0x1000 7       # árbol {6,4,4,1,2,2,1}
> regs a0 a1          # a0 = 0x1000 (arreglo), a1 = 7 (n)
> run
> regs a0             # 1 -> el árbol es simétrico
```

## 5. Extras (opcional, ≈ 1 min) — `demo/demo.sh 5`

- `ecall` estilo SPIM desde un ELF32:
  `./build/rv32sim tests/bin/ecall_demo.elf` → `dis 0x10000 12`, `run` → imprime `Suma 1..10 = 55`.
- GDB: `./build/rv32sim --gdb 1234 tests/bin/quicksort.bin` y en otra terminal
  `gdb -ex "set architecture riscv:rv32" -ex "target remote :1234"`, luego `x/5i $pc`,
  `break *0x4c`, `continue`, `x/7dw 0x1000`.
- Manejo de errores: en el REPL, `foo`, `mem zz`, `regs x99` muestran errores sin cerrar el programa.

## Cierre (≈ 15 s)

- Resumen: 37 instrucciones con tests de casos borde, 3 programas del enunciado verificados,
  extras (desensamblador, ecall, ELF, GDB). Enlace al repositorio en el informe.
