// elf.hpp — Cargador mínimo de ejecutables ELF32 RISC-V little-endian.
//
// Copia cada segmento PT_LOAD a su p_vaddr (p_filesz bytes del archivo y el
// resto hasta p_memsz en cero, p. ej. .bss) y devuelve e_entry como PC inicial.
// No se usan secciones ni símbolos.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "memory.hpp"

bool is_elf(const std::vector<uint8_t>& data);

// Devuelve false con `err` si el ELF no es válido o no es RV32 LE.
bool load_elf(const std::vector<uint8_t>& data, Memory& mem, uint32_t& entry,
              std::size_t& bytes_loaded, unsigned& segments, std::string& err);
