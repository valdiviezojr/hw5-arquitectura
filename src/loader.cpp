#include "loader.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>

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

    // Binario crudo: se copia tal cual a partir de 0x00000000 y PC = 0.
    cpu.mem.load(data, 0);
    info.entry = 0;
    info.bytes = data.size();
    char buf[96];
    std::snprintf(buf, sizeof buf, "binario crudo, %zu bytes en 0x00000000", data.size());
    info.description = buf;
    return true;
}
