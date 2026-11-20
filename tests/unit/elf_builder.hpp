// elf_builder.hpp — Construye un ELF32 RISC-V mínimo (un solo PT_LOAD). SOLO para tests/tools.
#pragma once

#include <cstdint>
#include <vector>

namespace elfb {

inline void put16(std::vector<uint8_t>& v, std::size_t off, uint32_t x) {
    v[off] = static_cast<uint8_t>(x);
    v[off + 1] = static_cast<uint8_t>(x >> 8);
}
inline void put32(std::vector<uint8_t>& v, std::size_t off, uint32_t x) {
    for (int i = 0; i < 4; ++i) v[off + static_cast<std::size_t>(i)] = static_cast<uint8_t>(x >> (8 * i));
}

// `image` se carga en `vaddr`; memsz = image.size() + bss_extra (relleno a cero).
inline std::vector<uint8_t> build(const std::vector<uint8_t>& image, uint32_t vaddr, uint32_t entry,
                                  uint32_t bss_extra = 0) {
    constexpr uint32_t EHDR = 52, PHDR = 32, DATA = 0x100;  // datos alineados en el archivo
    std::vector<uint8_t> f(DATA + image.size(), 0);
    f[0] = 0x7F; f[1] = 'E'; f[2] = 'L'; f[3] = 'F';
    f[4] = 1;  // ELFCLASS32
    f[5] = 1;  // ELFDATA2LSB
    f[6] = 1;  // EV_CURRENT
    put16(f, 16, 2);       // ET_EXEC
    put16(f, 18, 243);     // EM_RISCV
    put32(f, 20, 1);       // e_version
    put32(f, 24, entry);   // e_entry
    put32(f, 28, EHDR);    // e_phoff
    put32(f, 32, 0);       // e_shoff
    put32(f, 36, 0);       // e_flags
    put16(f, 40, EHDR);    // e_ehsize
    put16(f, 42, PHDR);    // e_phentsize
    put16(f, 44, 1);       // e_phnum
    // Program header PT_LOAD
    put32(f, EHDR + 0, 1);
    put32(f, EHDR + 4, DATA);
    put32(f, EHDR + 8, vaddr);
    put32(f, EHDR + 12, vaddr);
    put32(f, EHDR + 16, static_cast<uint32_t>(image.size()));
    put32(f, EHDR + 20, static_cast<uint32_t>(image.size()) + bss_extra);
    put32(f, EHDR + 24, 7);  // RWX
    put32(f, EHDR + 28, 4);
    for (std::size_t i = 0; i < image.size(); ++i) f[DATA + i] = image[i];
    return f;
}

}  // namespace elfb
