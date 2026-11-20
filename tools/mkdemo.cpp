// mkdemo.cpp — Genera el programa de demostración de ecall (helper de tests,
// NO es parte del simulador). Usa el codificador de tests/unit/encoder.hpp.
//
//   build/mkdemo <dir_salida>   -> <dir>/ecall_demo.bin  (crudo, en 0x0)
//                                  <dir>/ecall_demo.elf  (ELF32, en 0x10000)
//
// Programa equivalente en ensamblador (ver tests/asm/ecall_demo.s):
//         lui   a0, %hi(msg) ; addi a0, a0, %lo(msg)
//         li    a7, 4        ; ecall          # print_string
//         li    t0, 0        ; li t1, 10
//   loop: add   t0, t0, t1   ; addi t1, t1, -1 ; bnez t1, loop
//         mv    a0, t0       ; li a7, 1  ; ecall   # print_int (55)
//         li    a0, '\n'     ; li a7, 11 ; ecall   # print_char
//         li    a0, 0        ; li a7, 17 ; ecall   # exit2(0)
//   msg:  .asciz "Suma 1..10 = "   (en base + 0x100)
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "../tests/unit/elf_builder.hpp"
#include "../tests/unit/encoder.hpp"

using namespace enc;

namespace {

enum { ZERO = 0, T0 = 5, T1 = 6, A0 = 10, A7 = 17 };
constexpr uint32_t MSG_OFFSET = 0x100;

std::vector<uint8_t> image(uint32_t base) {
    const uint32_t msg = base + MSG_OFFSET;
    const std::vector<uint32_t> code = {
        LUI(A0, msg >> 12), ADDI(A0, A0, static_cast<int32_t>(msg & 0xFFF)),
        ADDI(A7, ZERO, 4), ECALL,
        ADDI(T0, ZERO, 0), ADDI(T1, ZERO, 10),
        ADD(T0, T0, T1), ADDI(T1, T1, -1), BNE(T1, ZERO, -8),
        ADDI(A0, T0, 0), ADDI(A7, ZERO, 1), ECALL,
        ADDI(A0, ZERO, '\n'), ADDI(A7, ZERO, 11), ECALL,
        ADDI(A0, ZERO, 0), ADDI(A7, ZERO, 17), ECALL,
    };
    std::vector<uint8_t> img(MSG_OFFSET, 0);
    for (std::size_t i = 0; i < code.size(); ++i)
        for (int b = 0; b < 4; ++b) img[4 * i + static_cast<std::size_t>(b)] = static_cast<uint8_t>(code[i] >> (8 * b));
    for (char c : std::string("Suma 1..10 = ")) img.push_back(static_cast<uint8_t>(c));
    img.push_back(0);
    return img;
}

bool write(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!f) {
        std::fprintf(stderr, "no se pudo escribir %s\n", path.c_str());
        return false;
    }
    std::printf("escrito %s (%zu bytes)\n", path.c_str(), data.size());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    constexpr uint32_t ELF_BASE = 0x10000;
    const bool ok = write(dir + "/ecall_demo.bin", image(0)) &&
                    write(dir + "/ecall_demo.elf", elfb::build(image(ELF_BASE), ELF_BASE, ELF_BASE, 64));
    return ok ? 0 : 1;
}
