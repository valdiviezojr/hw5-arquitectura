#include "elf.hpp"

namespace {

constexpr uint16_t EM_RISCV = 243;
constexpr uint32_t PT_LOAD = 1;

// Lectura little-endian independiente del host y con control de límites.
struct Reader {
    const std::vector<uint8_t>& d;
    bool ok = true;
    uint32_t u16(std::size_t off) {
        if (off + 2 > d.size()) { ok = false; return 0; }
        return static_cast<uint32_t>(d[off] | (d[off + 1] << 8));
    }
    uint32_t u32(std::size_t off) {
        if (off + 4 > d.size()) { ok = false; return 0; }
        return static_cast<uint32_t>(d[off]) | (static_cast<uint32_t>(d[off + 1]) << 8) |
               (static_cast<uint32_t>(d[off + 2]) << 16) | (static_cast<uint32_t>(d[off + 3]) << 24);
    }
};

}  // namespace

bool is_elf(const std::vector<uint8_t>& data) {
    return data.size() >= 4 && data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F';
}

bool load_elf(const std::vector<uint8_t>& data, Memory& mem, uint32_t& entry,
              std::size_t& bytes_loaded, unsigned& segments, std::string& err) {
    bytes_loaded = 0;
    segments = 0;
    if (data.size() < 52) { err = "ELF truncado (cabecera incompleta)"; return false; }
    if (data[4] != 1) { err = "ELF no es de 32 bits (ELFCLASS32)"; return false; }
    if (data[5] != 1) { err = "ELF no es little-endian"; return false; }

    Reader r{data};
    const uint32_t machine = r.u16(18);
    if (machine != EM_RISCV) {
        err = "ELF no es RISC-V (e_machine=" + std::to_string(machine) + ")";
        return false;
    }
    entry = r.u32(24);
    const uint32_t phoff = r.u32(28);
    const uint32_t phentsize = r.u16(42);
    const uint32_t phnum = r.u16(44);
    if (phnum == 0) { err = "ELF sin program headers"; return false; }
    if (phentsize < 32) { err = "ELF con e_phentsize inválido"; return false; }

    for (uint32_t k = 0; k < phnum; ++k) {
        const std::size_t ph = static_cast<std::size_t>(phoff) + static_cast<std::size_t>(k) * phentsize;
        const uint32_t type = r.u32(ph + 0);
        const uint32_t offset = r.u32(ph + 4);
        const uint32_t vaddr = r.u32(ph + 8);
        const uint32_t filesz = r.u32(ph + 16);
        const uint32_t memsz = r.u32(ph + 20);
        if (!r.ok) { err = "ELF truncado (program header fuera del archivo)"; return false; }
        if (type != PT_LOAD) continue;
        if (static_cast<uint64_t>(offset) + filesz > data.size()) {
            err = "ELF truncado (segmento PT_LOAD fuera del archivo)";
            return false;
        }
        if (filesz > memsz) { err = "ELF inválido (p_filesz > p_memsz)"; return false; }
        mem.load(data.data() + offset, filesz, vaddr);
        for (uint32_t i = filesz; i < memsz; ++i) mem.write8(vaddr + i, 0);  // .bss
        bytes_loaded += memsz;
        ++segments;
    }
    if (segments == 0) { err = "ELF sin segmentos PT_LOAD"; return false; }
    return true;
}
