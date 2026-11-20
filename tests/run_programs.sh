#!/usr/bin/env bash
# run_programs.sh — Ejecuta los 3 programas del enunciado en modo no interactivo
# y verifica los resultados esperados.
#   Uso: tests/run_programs.sh [ruta/al/rv32sim]   (por defecto build/rv32sim)
set -u
cd "$(dirname "$0")/.."
SIM="${1:-build/rv32sim}"
BIN=tests/bin
pass=0; fail=0

if [[ ! -x "$SIM" ]]; then
    echo "No se encontró el simulador en '$SIM'. Compila primero (make o cmake)." >&2
    exit 2
fi

# Extrae los valores decimales de la salida de 'memw A n' ("0x...: 0x...  (v)").
words() { grep -E '^0x[0-9a-f]{8}: ' | sed -E 's/.*\((-?[0-9]+)\)$/\1/' | tr '\n' ' ' | sed 's/ $//'; }

check() {  # check <nombre> <obtenido> <esperado>
    if [[ "$2" == "$3" ]]; then
        printf '[ OK ] %-38s %s\n' "$1" "$2"; pass=$((pass + 1))
    else
        printf '[FALLO] %-37s obtenido="%s" esperado="%s"\n' "$1" "$2" "$3"; fail=$((fail + 1))
    fi
}

# Ningún programa debe agotar el límite de instrucciones de 'run'.
check_run_finished() {  # check_run_finished <nombre> <salida>
    if grep -q 'Límite de' <<<"$2"; then
        check "$1: run termina" "límite alcanzado" "termina"
    else
        check "$1: run termina" "termina" "termina"
    fi
}

# ---------------------------------------------------------------- quicksort
out=$(printf 'run\nmemw 0x1000 7\nexit\n' | "$SIM" "$BIN/quicksort.bin")
check_run_finished quicksort "$out"
check "quicksort: arreglo en 0x1000" "$(words <<<"$out")" "1 2 3 4 6 8 9"

# ---------------------------------------------------------------- simétrico
out=$(printf 'run\nmemw 0x1000 7\nregs a0\nexit\n' | "$SIM" "$BIN/simetrico.bin")
check_run_finished simetrico "$out"
check "simetrico: árbol intacto en 0x1000" "$(words <<<"$out")" "6 4 4 1 2 2 1"
check "simetrico: is_symmetric -> a0" "$(grep -oE '^a0 \(x10\) = 0x[0-9a-f]+' <<<"$out" | awk '{print $NF}')" "0x00000001"

# ---------------------------------------------------------------- riscvtest
out=$(printf 'run\nmemw 100\nmem 0x64 0x67\nexit\n' | "$SIM" "$BIN/riscvtest.bin")
check_run_finished riscvtest "$out"
check "riscvtest: [100] == 25" "$(words <<<"$out")" "25"
check "riscvtest: bytes 0x64-0x67" "$(grep -oE 'Memoria \(0x64-0x67\):.*' <<<"$out" | cut -d: -f2 | sed 's/^ //')" "19 00 00 00"

# ------------------------------------------- extras: ecall (crudo y ELF)
for f in ecall_demo.bin ecall_demo.elf; do
    if [[ -f "$BIN/$f" ]]; then
        out=$(printf 'run\nexit\n' | "$SIM" "$BIN/$f")
        check "$f: salida por ecall" "$(grep -oE '^Suma 1\.\.10 = [0-9]+' <<<"$out")" "Suma 1..10 = 55"
        check "$f: exit2 con código 0" "$(grep -oE 'código de salida [0-9]+' <<<"$out")" "código de salida 0"
    fi
done

echo
echo "$pass correctos, $fail fallidos"
[[ $fail -eq 0 ]]
