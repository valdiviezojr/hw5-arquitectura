// memory.hpp — Memoria plana de 32 bits, byte-addressable, little-endian.
//
// Se implementa de forma dispersa: el espacio de 4 GiB se divide en páginas
// de 4 KiB que solo se reservan cuando se escriben por primera vez. Leer una
// página nunca escrita devuelve 0. No se modelan permisos ni segmentos.
//
// Política de alineamiento: los accesos desalineados de datos están
// PERMITIDOS (se componen byte a byte, incluso cruzando páginas).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

class Memory {
public:
    static constexpr uint32_t PAGE_BITS = 12;
    static constexpr uint32_t PAGE_SIZE = 1u << PAGE_BITS;  // 4096 bytes

    uint8_t read8(uint32_t addr) const;
    uint16_t read16(uint32_t addr) const;
    uint32_t read32(uint32_t addr) const;

    void write8(uint32_t addr, uint8_t value);
    void write16(uint32_t addr, uint16_t value);
    void write32(uint32_t addr, uint32_t value);

    // Copia un bloque de bytes a partir de `base` (la dirección envuelve en 2^32).
    void load(const uint8_t* data, std::size_t size, uint32_t base);
    void load(const std::vector<uint8_t>& data, uint32_t base) {
        load(data.data(), data.size(), base);
    }

    void clear() { pages_.clear(); }
    std::size_t pages_in_use() const { return pages_.size(); }

private:
    using Page = std::array<uint8_t, PAGE_SIZE>;
    std::unordered_map<uint32_t, Page> pages_;
};
