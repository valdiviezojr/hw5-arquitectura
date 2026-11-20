// disasm.hpp — Desensamblador RV32I.
//
// Formato: mnemónico + nombres ABI de registros, inmediatos en decimal.
// Los saltos y branches muestran la dirección destino absoluta en hex
// (p. ej. "beq t0, t2, 0x48"), calculada a partir del PC de la instrucción.
#pragma once

#include <cstdint>
#include <string>

std::string disasm(uint32_t inst, uint32_t pc);
