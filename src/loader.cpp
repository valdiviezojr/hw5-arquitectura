#include "loader.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>

#include "elf.hpp"

bool read_file(const std::string& path, std::vector<uint8_t>& out, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        err = "no se pudo abrir \"" + path + "\"";
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    if (f.bad()) {
        err = "error leyendo \"" + path + "\"";
        return false;
    }
    return true;
}

bool load_program(const std::string& path, Cpu& cpu, LoadInfo& info, std::string& err) {
    std::vector<uint8_t> data;
    if (!read_file(path, data, err)) return false;

    cpu.mem.clear();
    info = LoadInfo{};
    char buf[128];

    if (is_elf(data)) {
        unsigned segments = 0;
        if (!load_elf(data, cpu.mem, info.entry, info.bytes, segments, err)) {
            err = "\"" + path + "\": " + err;
            return false;
        }
        info.is_elf = true;
        std::snprintf(buf, sizeof buf, "ELF32, %u segmento(s) PT_LOAD, %zu bytes", segments,
                      info.bytes);
        info.description = buf;
        return true;
    }

    // Binario crudo: se copia tal cual a partir de 0x00000000 y PC = 0.
    cpu.mem.load(data, 0);
    info.entry = 0;
    info.bytes = data.size();
    std::snprintf(buf, sizeof buf, "binario crudo, %zu bytes en 0x00000000", data.size());
    info.description = buf;
    return true;
}
