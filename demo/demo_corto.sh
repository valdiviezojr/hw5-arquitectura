#!/usr/bin/env bash
# demo_corto.sh — Ejecuta exactamente los comandos de demo/GUION_VIDEO.md (video ≤ 3 min).
# Una pausa con Enter antes de cada segmento; luego limpia la pantalla y muestra el ejemplo.
#   Uso: demo/demo_corto.sh
set -u
cd "$(dirname "$0")/.."
SIM=build/rv32sim

if [[ ! -x "$SIM" || ! -x build/rv32sim_tests ]]; then
    echo "Compilando..."; make -s || exit 1
fi

next() {  # next <título>
    if [[ -t 0 ]]; then read -r -s -p "[Enter: $1]" _; fi
    if [[ -t 1 ]]; then clear; fi
    printf '\033[1m== %s ==\033[0m\n' "$1"
}
sim() { local prog="$1"; shift; printf '%s\n' "$@" | "$SIM" --echo "$prog"; }

next "Intro"
echo '$ ./build/rv32sim_tests | tail -1'
./build/rv32sim_tests | tail -1

next "Ejemplo 1: paso a paso (riscvtest)"
sim tests/bin/riscvtest.bin \
    "pc" "step" "step" "step" "step" "regs x2 x3 x7 x4" "pc" "exit"

next "Ejemplo 2: riscvtest completo — little-endian"
sim tests/bin/riscvtest.bin \
    "mem 0x64 0x67" "run" "mem 0x64 0x67" "memw 100" "exit"

next "Ejemplo 3: quicksort"
sim tests/bin/quicksort.bin \
    "break 0x48" "run" "mem 0x1000 0x101B" "run" "mem 0x1000 0x101B" "exit"

next "Ejemplo 4: árbol simétrico"
sim tests/bin/simetrico.bin \
    "break 0x44" "run" "memw 0x1000 7" "regs a0 a1" "run" "regs a0" "exit"

if [[ -t 0 ]]; then read -r -s -p "[Enter: fin]" _; echo; fi
