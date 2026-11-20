# Guion del video demostrativo (≈ 3 min)

Se usa `demo/demo.sh` tal cual. El script muestra una parte completa y se detiene con
`[Enter para continuar]`. Di el texto de esa parte **mientras está en pantalla** y luego
presiona **Enter** para pasar a la siguiente. Tras la parte 5 el script termina solo.

| Momento | Duración aprox. | Acumulado |
|---|---|---|
| Intro (antes de correr el script) | 0:20 | 0:20 |
| Parte 1: paso a paso | 0:45 | 1:05 |
| Parte 2: riscvtest, little-endian | 0:30 | 1:35 |
| Parte 3: quicksort | 0:35 | 2:10 |
| Parte 4: árbol simétrico | 0:25 | 2:35 |
| Parte 5 (extra) + cierre | 0:20 | 2:55 |

---

## Intro — antes de correr el script

En pantalla: la terminal en la raíz del repo. Opcional: `./build/rv32sim_tests | tail -1`
(muestra `40 tests, 418 comprobaciones, 0 fallos`).

> **Decir:** Este es rv32sim, un simulador del ISA RV32I escrito en C++17. Carga un binario
> crudo en la dirección cero, ejecuta las treinta y siete instrucciones de RV32I con memoria
> little-endian, y permite avanzar paso a paso e inspeccionar el PC, los registros y la
> memoria. Pasa sus cuarenta tests unitarios.

Teclear: `./demo/demo.sh`

---

## Parte 1 — Instrucciones aritméticas paso a paso (riscvtest.bin)

En pantalla: `pc`, `dis 0 6`, seis `step` con su `regs`, y el `pc` final (`0x00000018`).

> **Decir:** Primera parte: ejecución paso a paso. El PC empieza en cero. Con dis veo las
> primeras instrucciones desensambladas; uso nombres ABI, por ejemplo x2 es sp y x5 es t0.
> Cada step muestra la dirección, la instrucción en hexadecimal y su desensamblado. Primero
> cinco en x2 y doce en x3. Luego un inmediato negativo: doce menos nueve, tres en x7. Un or
> da siete en x4, un and da cuatro en x5, y el add deja once. Al final el PC avanzó de cuatro
> en cuatro hasta hexadecimal dieciocho.

**→ Enter**

## Parte 2 — riscvtest completo: escribe 25 en la dirección 100

En pantalla: `mem 0x64 0x67` → `00 00 00 00`, `run` (19 instrucciones, bucle en `0x50`),
`mem 0x64 0x67` → `19 00 00 00`, `memw 100` → `(25)`.

> **Decir:** Segunda parte: el mismo programa completo. Antes de correr, la dirección cien
> vale cero. Run ejecuta diecinueve instrucciones y se detiene al detectar el bucle final.
> Ahora la memoria muestra uno-nueve, cero, cero, cero: el byte menos significativo va
> primero, es little-endian. Hexadecimal diecinueve es veinticinco, y memw lo confirma.

**→ Enter**

## Parte 3 — Quicksort

En pantalla: `break 0x48`, `run` (para en el breakpoint), arreglo `6 4 3 2 1 8 9`, otro `run`
(bucle en `0x4c`) y arreglo `1 2 3 4 6 8 9`.

> **Decir:** Tercera parte: quicksort recursivo. Pongo un breakpoint en la llamada a qsort y
> corro hasta ahí. En la dirección mil hexadecimal está el arreglo original: seis, cuatro,
> tres, dos, uno, ocho, nueve. Con memw lo veo como palabras de treinta y dos bits. Sigo con
> run hasta el final, y el arreglo queda ordenado: uno, dos, tres, cuatro, seis, ocho, nueve.

**→ Enter**

## Parte 4 — Árbol simétrico

En pantalla: `break 0x44`, `run`, árbol `6 4 4 1 2 2 1`, `a0 = 0x1000`, `a1 = 7`, `run`,
`a0 = 1`.

> **Decir:** Cuarta parte: árbol simétrico. Breakpoint en la llamada a is_symmetric. El árbol
> está en memoria; a0 apunta al arreglo y a1 es siete, el tamaño. Tras run, a0 vale uno: el
> árbol es simétrico.

**→ Enter**

## Parte 5 (extra) + cierre

En pantalla: el ELF carga con `pc = 0x00010000`, el desensamblado y `Suma 1..10 = 55`.
El script termina aquí.

> **Decir:** Como extra, este programa se carga desde un archivo ELF e imprime con ecall
> estilo SPIM: suma uno a diez, cincuenta y cinco. El simulador también tiene un stub para
> GDB. El código y el informe están en el repositorio. Gracias.

---

## Checklist antes de grabar

- [ ] Fuente grande (≥ 18 pt) y terminal maximizada: la parte 1 ocupa ~40 líneas.
- [ ] Terminal limpia (`clear`), sin notificaciones ni otras ventanas.
- [ ] Repo compilado: `make` (o `cmake --build build`) y `make test` en verde.
- [ ] Una pasada completa cronometrada con `./demo/demo.sh`; si pasa de 3:00, acortar texto.
- [ ] Probar el micrófono con una grabación de 5 s.
