// decode.hpp — Extracción de campos, inmediatos y decodificación de RV32I.
//
// La decodificación produce un `Decoded` independiente de la ejecución, de modo
// que la CPU y el desensamblador comparten exactamente la misma lógica.
#pragma once

#include <cstdint>
#include <string>

namespace rv {

// Nombres ABI de x0..x31.
extern const char* const ABI_NAMES[32];
// Acepta "x5", "t0", "fp", "zero"... Devuelve -1 si el nombre no existe.
int parse_reg(const std::string& name);

// ---------------------------------------------------------------- campos ----
inline uint32_t bits(uint32_t v, unsigned hi, unsigned lo) {
    return (v >> lo) & ((hi - lo == 31) ? 0xFFFFFFFFu : ((1u << (hi - lo + 1)) - 1));
}

// Extensión de signo de los `n` bits menos significativos de `v` (portable:
// no depende de desplazamientos a la derecha de enteros negativos).
inline int32_t sext(uint32_t v, unsigned n) {
    const uint32_t m = 1u << (n - 1);
    v &= (n == 32) ? 0xFFFFFFFFu : ((1u << n) - 1);
    return static_cast<int32_t>((v ^ m) - m);
}

inline uint32_t opcode(uint32_t i) { return bits(i, 6, 0); }
inline uint32_t rd(uint32_t i) { return bits(i, 11, 7); }
inline uint32_t funct3(uint32_t i) { return bits(i, 14, 12); }
inline uint32_t rs1(uint32_t i) { return bits(i, 19, 15); }
inline uint32_t rs2(uint32_t i) { return bits(i, 24, 20); }
inline uint32_t funct7(uint32_t i) { return bits(i, 31, 25); }

// ------------------------------------------------------------ inmediatos ----
// I: inst[31:20]
inline int32_t imm_i(uint32_t i) { return sext(bits(i, 31, 20), 12); }
// S: inst[31:25] | inst[11:7]
inline int32_t imm_s(uint32_t i) { return sext((bits(i, 31, 25) << 5) | bits(i, 11, 7), 12); }
// B: bits del inmediato [12|10:5] en inst[31:25] y [4:1|11] en inst[11:7]; bit 0 = 0
inline int32_t imm_b(uint32_t i) {
    uint32_t v = (bits(i, 31, 31) << 12) | (bits(i, 7, 7) << 11) |
                 (bits(i, 30, 25) << 5) | (bits(i, 11, 8) << 1);
    return sext(v, 13);
}
// U: inst[31:12] << 12 (sin extensión adicional, ya ocupa 32 bits)
inline int32_t imm_u(uint32_t i) { return static_cast<int32_t>(i & 0xFFFFF000u); }
// J: bits [20|10:1|11|19:12] en inst[31:12]; bit 0 = 0
inline int32_t imm_j(uint32_t i) {
    uint32_t v = (bits(i, 31, 31) << 20) | (bits(i, 19, 12) << 12) |
                 (bits(i, 20, 20) << 11) | (bits(i, 30, 21) << 1);
    return sext(v, 21);
}

// ------------------------------------------------------------ opcodes ------
enum Opcode : uint32_t {
    OP_LUI = 0b0110111,
    OP_AUIPC = 0b0010111,
    OP_JAL = 0b1101111,
    OP_JALR = 0b1100111,
    OP_BRANCH = 0b1100011,
    OP_LOAD = 0b0000011,
    OP_STORE = 0b0100011,
    OP_IMM = 0b0010011,
    OP_REG = 0b0110011,
    OP_FENCE = 0b0001111,
    OP_SYSTEM = 0b1110011,
};

// Las 37 instrucciones de RV32I del enunciado + fence/ecall/ebreak.
enum class Op {
    INVALID,
    LUI, AUIPC, JAL, JALR,
    BEQ, BNE, BLT, BGE, BLTU, BGEU,
    LB, LH, LW, LBU, LHU,
    SB, SH, SW,
    ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI,
    ADD, SUB, SLL, SLT, SLTU, XOR, OR, AND, SRL, SRA,
    FENCE, ECALL, EBREAK,
};

enum class Format { R, I, S, B, U, J, NONE };

struct Decoded {
    Op op = Op::INVALID;
    Format fmt = Format::NONE;
    uint32_t raw = 0;
    uint32_t rd = 0, rs1 = 0, rs2 = 0;
    int32_t imm = 0;  // en shifts inmediatos contiene shamt (0..31)
};

Decoded decode(uint32_t inst);
const char* op_name(Op op);

}  // namespace rv
