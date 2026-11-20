// loader.hpp — Carga de programas en memoria.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cpu.hpp"

struct LoadInfo {
    bool is_elf = false;
    uint32_t entry = 0;       // PC inicial
    std::size_t bytes = 0;    // bytes copiados a memoria
    std::string description;  // resumen legible de lo cargado
};

// Lee el archivo completo. Devuelve false (con `err`) si no se pudo leer.
bool read_file(const std::string& path, std::vector<uint8_t>& out, std::string& err);

// Limpia la memoria de `cpu` y carga el programa: si empieza con el magic
// 0x7F 'E' 'L' 'F' se trata como ELF32; si no, como binario crudo en 0x0.
// No reinicia los registros (eso lo hace el llamador con cpu.reset()).
bool load_program(const std::string& path, Cpu& cpu, LoadInfo& info, std::string& err);
