#include "memory.hpp"

uint8_t Memory::read8(uint32_t addr) const {
    auto it = pages_.find(addr >> PAGE_BITS);
    if (it == pages_.end()) return 0;  // página nunca escrita
    return it->second[addr & (PAGE_SIZE - 1)];
}

void Memory::write8(uint32_t addr, uint8_t value) {
    auto it = pages_.find(addr >> PAGE_BITS);
    if (it == pages_.end()) {
        if (value == 0) return;  // escribir 0 en página vacía no cambia nada
        it = pages_.emplace(addr >> PAGE_BITS, Page{}).first;  // Page{} = ceros
    }
    it->second[addr & (PAGE_SIZE - 1)] = value;
}

// Little-endian: el byte menos significativo va en la dirección más baja.
// Se compone byte a byte para soportar accesos desalineados y cruces de página;
// `addr + i` envuelve naturalmente en 2^32 al ser uint32_t.
uint16_t Memory::read16(uint32_t addr) const {
    return static_cast<uint16_t>(read8(addr) | (read8(addr + 1) << 8));
}

uint32_t Memory::read32(uint32_t addr) const {
    return static_cast<uint32_t>(read8(addr)) |
           (static_cast<uint32_t>(read8(addr + 1)) << 8) |
           (static_cast<uint32_t>(read8(addr + 2)) << 16) |
           (static_cast<uint32_t>(read8(addr + 3)) << 24);
}

void Memory::write16(uint32_t addr, uint16_t value) {
    write8(addr, static_cast<uint8_t>(value));
    write8(addr + 1, static_cast<uint8_t>(value >> 8));
}

void Memory::write32(uint32_t addr, uint32_t value) {
    for (uint32_t i = 0; i < 4; ++i) write8(addr + i, static_cast<uint8_t>(value >> (8 * i)));
}

void Memory::load(const uint8_t* data, std::size_t size, uint32_t base) {
    for (std::size_t i = 0; i < size; ++i)
        write8(base + static_cast<uint32_t>(i), data[i]);
}
