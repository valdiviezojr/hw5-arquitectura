# Guion del video demostrativo (máx. 3:00 · objetivo 2:40)

Los comandos de cada segmento los ejecuta `demo/demo_corto.sh` de corrido: una pausa con
Enter antes de cada ejemplo, limpia la pantalla y muestra todo el ejemplo de una vez. Mientras
tanto, lee el texto de la columna derecha. También se pueden teclear a mano en el REPL
(`./build/rv32sim <programa>`).

## Tiempos

| Segmento | Duración | Acumulado |
|---|---|---|
| Intro | 0:20 | 0:20 |
| Ej. 1: paso a paso (riscvtest) | 0:40 | 1:00 |
| Ej. 2: riscvtest completo, little-endian | 0:30 | 1:30 |
| Ej. 3: quicksort | 0:35 | 2:05 |
| Ej. 4: árbol simétrico | 0:25 | 2:30 |
| Cierre | 0:10 | **2:40** |

Margen hasta el máximo: 20 s.

---

## Intro (0:00 – 0:20)

| Comandos | Texto a decir |
|---|---|
| `./build/rv32sim_tests \| tail -1`<br>→ `40 tests, 418 comprobaciones, 0 fallos` | Este es rv32sim, un simulador escrito en C++17. Carga un binario crudo en la dirección cero y ejecuta las treinta y siete instrucciones de RV32I, con memoria little-endian. El desensamblador usa nombres ABI: x2 es sp, x5 es t0. Pasa sus cuarenta tests unitarios. |

## Ejemplo 1: paso a paso (0:20 – 1:00)

| Comandos | Texto a decir |
|---|---|
| `./build/rv32sim tests/bin/riscvtest.bin`<br>`pc` → `0x00000000`<br>`step` → `addi sp, zero, 5`<br>`step` → `addi gp, zero, 12`<br>`step` → `addi t2, gp, -9`<br>`step` → `or tp, t2, sp`<br>`regs x2 x3 x7 x4` → `5, 0xc, 3, 7`<br>`pc` → `0x00000010` | Ejemplo uno: ejecución paso a paso. El PC empieza en cero. Cada step muestra la dirección, la instrucción en hexadecimal y desensamblada. Primero, cinco en x2, que es sp. Luego doce en x3. Después un inmediato negativo: doce menos nueve, tres en x7. Y un or: tres or cinco, siete en x4. Con regs compruebo los cuatro registros. El PC avanzó de cuatro en cuatro, hasta hexadecimal diez. |

## Ejemplo 2: riscvtest completo (1:00 – 1:30)

| Comandos | Texto a decir |
|---|---|
| `./build/rv32sim tests/bin/riscvtest.bin`<br>`mem 0x64 0x67` → `00 00 00 00`<br>`run` → bucle en `0x50`, 19 instrucciones<br>`mem 0x64 0x67` → `19 00 00 00`<br>`memw 100` → `0x00000019 (25)` | Ejemplo dos: el programa completo. Antes de correr, la dirección cien vale cero. Run ejecuta diecinueve instrucciones y se detiene en el bucle final. Ahora la memoria muestra uno-nueve, cero, cero, cero: el byte menos significativo va primero, es little-endian. Hexadecimal diecinueve es veinticinco, y memw lo confirma. |

## Ejemplo 3: quicksort (1:30 – 2:05)

| Comandos | Texto a decir |
|---|---|
| `./build/rv32sim tests/bin/quicksort.bin`<br>`break 0x48`<br>`run` → para en `0x48` (`call _qsort`)<br>`mem 0x1000 0x101B` → `06 … 04 … 03 … 02 … 01 … 08 … 09`<br>`run` → bucle en `0x4c`<br>`mem 0x1000 0x101B` → `01 … 02 … 03 … 04 … 06 … 08 … 09` | Ejemplo tres: quicksort recursivo. Pongo un breakpoint justo en la llamada a qsort y corro. En la dirección mil hexadecimal está el arreglo original: seis, cuatro, tres, dos, uno, ocho, nueve, cuatro bytes cada uno. Sigo con run hasta el bucle final. El arreglo quedó ordenado: uno, dos, tres, cuatro, seis, ocho, nueve. |

## Ejemplo 4: árbol simétrico (2:05 – 2:30)

| Comandos | Texto a decir |
|---|---|
| `./build/rv32sim tests/bin/simetrico.bin`<br>`break 0x44`<br>`run` → para en `0x44` (`call is_symmetric`)<br>`memw 0x1000 7` → `6 4 4 1 2 2 1`<br>`regs a0 a1` → `0x1000, 7`<br>`run` → bucle en `0x48`<br>`regs a0` → `1` | Ejemplo cuatro: árbol simétrico. Breakpoint en la llamada a is_symmetric. El árbol está en memoria; a0 apunta al arreglo y a1 es siete, el tamaño. Tras run, a0 vale uno: el árbol es simétrico. |

## Cierre (2:30 – 2:40)

| Comandos | Texto a decir |
|---|---|
| *(ninguno)* | Como extras: ecall estilo SPIM, carga de ELF y stub para GDB. El código y el informe están en el repositorio. Gracias. |

---

## Checklist antes de grabar

- [ ] Fuente de la terminal grande (≥ 18 pt) y ventana maximizada; en pantalla deben caber las
      ~20 líneas de cada ejemplo.
- [ ] Terminal limpia: `clear`, sin notificaciones ni otras ventanas.
- [ ] Repo compilado: `make` (o `cmake --build build`) y `make test` en verde.
- [ ] Hacer una pasada completa con `demo/demo_corto.sh` cronometrando; si pasa de 2:50, recortar
      texto, no comandos.
- [ ] Probar el micrófono y grabar 5 s de prueba.
