// encoder.hpp — Codificador mínimo de instrucciones RV32I, SOLO para tests.
// Genera palabras de instrucción directamente, sin necesidad de un ensamblador.
#pragma once

#include <cstdint>

namespace enc {

inline uint32_t u(int32_t v) { return static_cast<uint32_t>(v); }

inline uint32_t r(uint32_t f7, uint32_t rs2, uint32_t rs1, uint32_t f3, uint32_t rd, uint32_t op) {
    return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}
inline uint32_t i(int32_t imm, uint32_t rs1, uint32_t f3, uint32_t rd, uint32_t op) {
    return ((u(imm) & 0xFFF) << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}
inline uint32_t s(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t f3, uint32_t op) {
    const uint32_t m = u(imm);
    return (((m >> 5) & 0x7F) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) |
           ((m & 0x1F) << 7) | op;
}
inline uint32_t b(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t f3) {
    const uint32_t m = u(imm);
    return (((m >> 12) & 1) << 31) | (((m >> 5) & 0x3F) << 25) | (rs2 << 20) | (rs1 << 15) |
           (f3 << 12) | (((m >> 1) & 0xF) << 8) | (((m >> 11) & 1) << 7) | 0b1100011;
}
// `imm20` son los 20 bits altos (lo que se escribe en ensamblador: lui x1, imm20).
inline uint32_t uty(uint32_t imm20, uint32_t rd, uint32_t op) {
    return ((imm20 & 0xFFFFF) << 12) | (rd << 7) | op;
}
inline uint32_t j(int32_t imm, uint32_t rd) {
    const uint32_t m = u(imm);
    return (((m >> 20) & 1) << 31) | (((m >> 1) & 0x3FF) << 21) | (((m >> 11) & 1) << 20) |
           (((m >> 12) & 0xFF) << 12) | (rd << 7) | 0b1101111;
}

// ---- atajos por instrucción --------------------------------------------
constexpr uint32_t OPI = 0b0010011, OPR = 0b0110011, LD = 0b0000011, ST = 0b0100011;

inline uint32_t LUI(uint32_t rd, uint32_t imm20) { return uty(imm20, rd, 0b0110111); }
inline uint32_t AUIPC(uint32_t rd, uint32_t imm20) { return uty(imm20, rd, 0b0010111); }
inline uint32_t JAL(uint32_t rd, int32_t off) { return j(off, rd); }
inline uint32_t JALR(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 0, rd, 0b1100111); }

inline uint32_t BEQ(uint32_t a, uint32_t c, int32_t off) { return b(off, c, a, 0); }
inline uint32_t BNE(uint32_t a, uint32_t c, int32_t off) { return b(off, c, a, 1); }
inline uint32_t BLT(uint32_t a, uint32_t c, int32_t off) { return b(off, c, a, 4); }
inline uint32_t BGE(uint32_t a, uint32_t c, int32_t off) { return b(off, c, a, 5); }
inline uint32_t BLTU(uint32_t a, uint32_t c, int32_t off) { return b(off, c, a, 6); }
inline uint32_t BGEU(uint32_t a, uint32_t c, int32_t off) { return b(off, c, a, 7); }

inline uint32_t LB(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 0, rd, LD); }
inline uint32_t LH(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 1, rd, LD); }
inline uint32_t LW(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 2, rd, LD); }
inline uint32_t LBU(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 4, rd, LD); }
inline uint32_t LHU(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 5, rd, LD); }

inline uint32_t SB(uint32_t rs2, uint32_t rs1, int32_t imm) { return s(imm, rs2, rs1, 0, ST); }
inline uint32_t SH(uint32_t rs2, uint32_t rs1, int32_t imm) { return s(imm, rs2, rs1, 1, ST); }
inline uint32_t SW(uint32_t rs2, uint32_t rs1, int32_t imm) { return s(imm, rs2, rs1, 2, ST); }

inline uint32_t ADDI(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 0, rd, OPI); }
inline uint32_t SLTI(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 2, rd, OPI); }
inline uint32_t SLTIU(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 3, rd, OPI); }
inline uint32_t XORI(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 4, rd, OPI); }
inline uint32_t ORI(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 6, rd, OPI); }
inline uint32_t ANDI(uint32_t rd, uint32_t rs1, int32_t imm) { return i(imm, rs1, 7, rd, OPI); }
inline uint32_t SLLI(uint32_t rd, uint32_t rs1, uint32_t sh) { return r(0, sh, rs1, 1, rd, OPI); }
inline uint32_t SRLI(uint32_t rd, uint32_t rs1, uint32_t sh) { return r(0, sh, rs1, 5, rd, OPI); }
inline uint32_t SRAI(uint32_t rd, uint32_t rs1, uint32_t sh) { return r(0x20, sh, rs1, 5, rd, OPI); }

inline uint32_t ADD(uint32_t rd, uint32_t a, uint32_t c) { return r(0, c, a, 0, rd, OPR); }
inline uint32_t SUB(uint32_t rd, uint32_t a, uint32_t c) { return r(0x20, c, a, 0, rd, OPR); }
inline uint32_t SLL(uint32_t rd, uint32_t a, uint32_t c) { return r(0, c, a, 1, rd, OPR); }
inline uint32_t SLT(uint32_t rd, uint32_t a, uint32_t c) { return r(0, c, a, 2, rd, OPR); }
inline uint32_t SLTU(uint32_t rd, uint32_t a, uint32_t c) { return r(0, c, a, 3, rd, OPR); }
inline uint32_t XOR(uint32_t rd, uint32_t a, uint32_t c) { return r(0, c, a, 4, rd, OPR); }
inline uint32_t SRL(uint32_t rd, uint32_t a, uint32_t c) { return r(0, c, a, 5, rd, OPR); }
inline uint32_t SRA(uint32_t rd, uint32_t a, uint32_t c) { return r(0x20, c, a, 5, rd, OPR); }
inline uint32_t OR(uint32_t rd, uint32_t a, uint32_t c) { return r(0, c, a, 6, rd, OPR); }
inline uint32_t AND(uint32_t rd, uint32_t a, uint32_t c) { return r(0, c, a, 7, rd, OPR); }

constexpr uint32_t ECALL = 0x00000073, EBREAK = 0x00100073, FENCE = 0x0000000F;

}  // namespace enc
