# rv32sim — Simulador de RISC-V (RV32I)

HW#5 de CS3051 Arquitectura de Computadores 2026-II (UTEC).

`rv32sim` es un **simulador de software** del ISA RV32I escrito en C++17. Carga un programa ya
compilado (binario crudo o ELF32), lo ejecuta paso a paso y permite inspeccionar el PC, los
registros y la memoria en cualquier momento desde una consola interactiva (REPL).

## Compilar

Requisitos: un compilador C++17 (g++ ≥ 8 o clang++ ≥ 7). No hay dependencias externas.

Con CMake (≥ 3.16):

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure   # tests unitarios + programas
```

Sin CMake, el `Makefile` incluido hace lo mismo:

```bash
make            # build/rv32sim, build/rv32sim_tests, build/mkdemo
make test       # tests unitarios + tests/run_programs.sh
make SANITIZE=1 # versión con -fsanitize=address,undefined en build-san/
```

El código compila sin warnings con `-Wall -Wextra -Wpedantic`.

## Uso

```bash
./build/rv32sim [opciones] programa.bin|programa.elf
```

| Opción | Efecto |
|---|---|
| `--sp VALOR` | valor inicial de `sp` (por defecto `0x00100000`) |
| `--limit N` | límite de instrucciones de `run` (por defecto 10 000 000) |
| `--echo` | muestra el prompt y los comandos aunque stdin no sea una terminal (útil en scripts/demos) |
| `--gdb [PUERTO]` | en vez del REPL, espera a GDB por TCP (por defecto 1234) |

Ejemplo de sesión:

```
$ ./build/rv32sim tests/bin/riscvtest.bin
"tests/bin/riscvtest.bin" cargado a memoria. (binario crudo, 83 bytes en 0x00000000, pc = 0x00000000)
> pc
pc = 0x00000000
> step
Ejecutando instrucción. 0x00000000 : 0x00500113  addi sp, zero, 5
> regs x2 t2
x2 = 0x00000005
t2 (x7) = 0x00000000
> run
...
> mem 0x64 0x67
Memoria (0x64-0x67): 19 00 00 00
> exit
```

### Comandos

| Comando | Descripción |
|---|---|
| `step [n]` | ejecuta 1 (o n) instrucciones; muestra dirección, palabra y desensamblado de cada una |
| `run [max]` | ejecuta hasta: fin del programa (`ecall` exit / `ebreak`), error, breakpoint, bucle infinito trivial o el límite de instrucciones |
| `pc` | muestra el PC (hex, 8 dígitos) |
| `regs [r ...]` | sin argumentos, los 32 registros + pc; con argumentos, solo esos. Acepta `x5` y nombres ABI (`t0`, `a0`, `sp`, `ra`, `fp`, …) |
| `mem A [B]` | bytes de A a B inclusive en hex, en orden ascendente de direcciones (por defecto 16 bytes) |
| `memw A [n]` | n palabras de 32 bits (little-endian) desde A, en hex y decimal |
| `dis [A] [n]` | desensambla n instrucciones desde A (por defecto desde el PC, 10). `=>` marca el PC y `*` los breakpoints |
| `break A` / `delete A` / `delete all` / `breaks` | gestión de breakpoints (los usa `run`) |
| `reset` | recarga el programa y reinicia registros, PC y heap |
| `help` | lista de comandos |
| `exit` | sale (también `quit` o fin de stdin) |

Las direcciones y números aceptan hexadecimal (`0x…`) o decimal. Un comando desconocido o con
argumentos inválidos imprime un error y el REPL continúa. Las líneas vacías o que empiezan con
`#` se ignoran.

### Modo no interactivo

El REPL lee comandos de stdin línea a línea y no depende de una TTY ni usa colores:

```bash
printf 'run\nmem 0x1000 0x101b\nexit\n' | ./build/rv32sim tests/bin/quicksort.bin
```

## Decisiones de diseño

| Tema | Decisión |
|---|---|
| Lenguaje | C++17, sin dependencias externas |
| Memoria | plana, byte-addressable, 32 bits, **little-endian**. Dispersa por páginas de 4 KiB (`unordered_map<uint32_t, array<uint8_t,4096>>`): cubre los 4 GiB sin reservar RAM; las páginas nunca escritas leen 0 |
| Permisos / segmentación | no se modelan: toda dirección es legible y escribible |
| Registros | `uint32_t x[32]`; `x0` siempre vale 0 (las escrituras se ignoran) |
| Carga | binario crudo en `0x00000000` con PC = 0. Si el archivo empieza con `0x7F 'E' 'L' 'F'` se carga como ELF32 |
| `sp` inicial | `DEFAULT_SP = 0x00100000` (configurable con `--sp`); el resto de registros en 0 |
| Decodificación | un único decodificador (`decode.cpp`) compartido por la CPU y el desensamblador; las codificaciones no válidas (funct3/funct7 reservados, RV32M, RV64, CSR…) se rechazan |
| Aritmética | todo en `uint32_t`; `int32_t` solo en comparaciones con signo. `sra`/`srai` se implementan de forma portable (sin depender de `>>` sobre negativos, que en C++17 es *implementation-defined*) |
| `fence` | se trata como NOP |
| Fin del programa | `ecall` exit (10) / exit2 (17), `ebreak`, o en `run` una instrucción que salta a sí misma (`j .`), que es como terminan los programas de CPUlator |
| Errores | instrucción inválida, PC no alineado a 4 o syscall desconocida detienen la ejecución con un mensaje que incluye el PC y la palabra de instrucción en hex. El simulador nunca aborta |

### Política de accesos desalineados

* **Datos (loads/stores): permitidos.** `lh/lhu/lw/sh/sw` en cualquier dirección funcionan
  byte a byte, incluso cruzando páginas o el final del espacio de direcciones (envuelve en 2³²).
  Está cubierto por los tests `memory_misaligned_and_page_crossing` y
  `load_negative_offset_and_misaligned`.
* **Instrucciones: error.** Sin la extensión C, el PC debe ser múltiplo de 4; un `jalr`/branch
  que deje el PC desalineado provoca un error explícito al intentar ejecutar la siguiente
  instrucción (test `misaligned_pc_is_error`).

## Extras implementados

1. **Desensamblador** (`src/disasm.cpp`): salida con nombres ABI (`addi t0, zero, 291`,
   `lw t0, -4(t0)`, `beq t0, t2, 0x48`). Los branches y `jal` muestran la dirección destino absoluta.
2. **`ecall` estilo SPIM** (`src/syscalls.cpp`), con la tabla del sistema *RV32 SPIM* de CPUlator
   (<https://cpulator.01xz.net/doc/#syscall>): número en `a7`, argumentos en `a0`/`a1`, resultado en `a0`.

   | a7 | Servicio | a7 | Servicio |
   |---|---|---|---|
   | 1 | print_int | 11 | print_char |
   | 4 | print_string | 12 | read_char |
   | 5 | read_int | 17 | exit2 (código en a0) |
   | 8 | read_string (a0 = buffer, a1 = máx.) | 30 | time (ms; a0 bajos, a1 altos) |
   | 9 | sbrk (heap desde `0x10000000`) | 32 | sleep (ms) |
   | 10 | exit | 34 / 35 / 36 | print_int en hex / binario / sin signo |

   Los syscalls de punto flotante (2, 3, 6, 7) no aplican a RV32I; cualquier número no listado
   detiene la ejecución con `syscall desconocida o no soportada: a7=N`. La entrada (`read_*`)
   se lee de stdin.
3. **Carga de ELF32** (`src/elf.cpp`): valida clase 32 bits, little-endian y `EM_RISCV`; copia
   cada `PT_LOAD` a `p_vaddr`, rellena con ceros hasta `p_memsz` (`.bss`) y fija PC = `e_entry`.
4. **Stub GDB** (`src/gdbstub.cpp`): protocolo Remote Serial sobre TCP (solo `127.0.0.1`).
   Soporta `?`, `g`, `G`, `p`, `P`, `m`, `M`, `s`, `c`, `Z0/z0` (y `Z1/z1`), `qSupported`,
   `qXfer:features:read` (envía un `target.xml` con x0–x31 + pc), `H`, `k`, `D` y Ctrl-C.
   `continue` también se detiene ante un bucle `j .`.

   ```bash
   ./build/rv32sim --gdb 1234 tests/bin/quicksort.bin
   # en otra terminal (gdb-multiarch en Debian/Ubuntu; en Arch el gdb normal ya incluye RISC-V):
   gdb -ex "set architecture riscv:rv32" -ex "target remote :1234"
   (gdb) x/5i $pc
   (gdb) break *0x4c
   (gdb) continue
   (gdb) x/7dw 0x1000
   ```

## Pruebas

* **Unitarias** (`tests/unit/test_main.cpp`, 40 tests): las 37 instrucciones con casos borde
  (inmediatos negativos y extremos, `lb` con byte ≥ 0x80, `slt` vs `sltu` con `0xFFFFFFFF`,
  `sra` con negativos, `jalr` con `rd == rs1`, branch hacia atrás, escrituras a `x0`, `lui`/`auipc`
  con bit alto, offsets máximo/mínimo de `jal`), little-endian, memoria dispersa, errores,
  desensamblador, syscalls y ELF. Las instrucciones se generan con un pequeño codificador
  (`tests/unit/encoder.hpp`), así que no hace falta un ensamblador.
* **Programas del enunciado** (`tests/run_programs.sh`):

  | Programa | Verificación | Resultado |
  |---|---|---|
  | `quicksort.bin` | `{6,4,3,2,1,8,9}` en `0x1000` queda `{1,2,3,4,6,8,9}` | ✔ (588 instrucciones) |
  | `simetrico.bin` | árbol `{6,4,4,1,2,2,1}` en `0x1000`, `is_symmetric` deja `a0 = 1` | ✔ (146 instrucciones) |
  | `riscvtest.bin` | escribe 25 (`0x19`) en la dirección 100 (`0x64`) | ✔ |
  | `ecall_demo.bin` / `.elf` | imprime `Suma 1..10 = 55` y termina con exit2(0) | ✔ |

* Se verificó también con `-fsanitize=address,undefined` (`make SANITIZE=1`): sin errores.

Los binarios del enunciado se exportaron desde CPUlator (*Memory → File format Raw → Save to
file*). `tests/bin/ecall_demo.{bin,elf}` se generan con `make demo-bins` (`tools/mkdemo.cpp`,
un helper de tests que usa el codificador; el fuente equivalente está en `tests/asm/ecall_demo.s`).

## Estructura

```
src/        main.cpp (REPL), cpu, memory, decode, disasm, syscalls, loader, elf, gdbstub
tests/      unit/ (tests + codificador), asm/ (fuentes), bin/ (binarios), run_programs.sh
tools/      mkdemo.cpp (genera el programa de demostración de ecall)
demo/       demo.sh y GUION_VIDEO.md
docs/       informe.tex
```

## Limitaciones

* Solo RV32I (+ `fence`, `ecall`, `ebreak`): no hay extensiones M, A, F, D, C ni CSR/Zicsr,
  ni modos de privilegio, interrupciones o excepciones al estilo hardware (los errores detienen
  la simulación).
* No se modelan permisos de memoria ni dispositivos de E/S mapeados en memoria.
* No incluye ensamblador (por diseño): los programas se compilan fuera, p. ej. en CPUlator.
* La detección de bucle infinito de `run` solo cubre el caso trivial de una instrucción que salta
  a sí misma; otros bucles se cortan con el límite de instrucciones.
* El stub GDB usa sockets POSIX (Linux/macOS), atiende a un único cliente y no soporta
  watchpoints.
* Si un binario crudo exportado no cubre todo el programa, el resto de la memoria se lee como 0 y
  la ejecución termina en un error de instrucción inválida al llegar ahí (p. ej. `0x00000000`).
