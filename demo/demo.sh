#!/usr/bin/env bash
# demo.sh — Los 4 ejemplos de ejecución para el video demostrativo.
#   Uso: demo/demo.sh [--no-pause] [n]     (n = 1..5 para correr solo un ejemplo)
# Los comandos se envían al REPL por stdin con --echo, así se ven igual que en
# una sesión interactiva ("> comando" seguido de su salida).
set -u
cd "$(dirname "$0")/.."
SIM=build/rv32sim
PAUSE=1
ONLY=""
for a in "$@"; do
    case "$a" in
        --no-pause) PAUSE=0 ;;
        [1-5]) ONLY="$a" ;;
        *) echo "Uso: $0 [--no-pause] [1-5]"; exit 2 ;;
    esac
done
[[ -t 0 ]] || PAUSE=0

if [[ ! -x "$SIM" ]]; then
    echo "Compilando..."; make -s || exit 1
fi

title() { printf '\n\033[1m==================== %s ====================\033[0m\n' "$1"; }
pause() { if [[ $PAUSE -eq 1 ]]; then read -r -p "[Enter para continuar] " _; fi; }
sim()   { local prog="$1"; shift; printf '%s\n' "$@" | "$SIM" --echo "$prog"; }
want()  { [[ -z "$ONLY" || "$ONLY" == "$1" ]]; }

if want 1; then
    title "1) Instrucciones aritméticas paso a paso (riscvtest.bin)"
    sim tests/bin/riscvtest.bin \
        "pc" "dis 0 6" \
        "step" "regs x2" "pc" \
        "step" "regs x3" \
        "step" "regs x7" \
        "step" "regs x4" \
        "step" "regs x5" \
        "step" "regs x5 x4 t0 tp" "pc" "exit"
    pause
fi

if want 2; then
    title "2) riscvtest.s: escribe 25 en la dirección 100 (0x64)"
    sim tests/bin/riscvtest.bin \
        "mem 0x64 0x67" "run" "mem 0x64 0x67" "memw 100" "regs x2 x9" "exit"
    pause
fi

if want 3; then
    title "3) Quicksort: arreglo en 0x1000 antes y después"
    sim tests/bin/quicksort.bin \
        "break 0x48" "run" "# arreglo inicial, justo antes de 'call _qsort'" \
        "mem 0x1000 0x101B" "memw 0x1000 7" \
        "run" "# arreglo ordenado" "mem 0x1000 0x101B" "memw 0x1000 7" "exit"
    pause
fi

if want 4; then
    title "4) Árbol simétrico: is_symmetric devuelve a0 = 1"
    sim tests/bin/simetrico.bin \
        "break 0x44" "run" "memw 0x1000 7" "regs a0 a1" \
        "run" "regs a0" "pc" "exit"
    pause
fi

if want 5; then
    title "Extra) ecall estilo SPIM desde un ELF32"
    sim tests/bin/ecall_demo.elf "pc" "dis 0x10000 12" "run" "exit"
fi
