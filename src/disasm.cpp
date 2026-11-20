#include "disasm.hpp"

#include <cstdio>

#include "decode.hpp"

using namespace rv;

std::string disasm(uint32_t inst, uint32_t pc) {
    const Decoded d = decode(inst);
    const char* n = op_name(d.op);
    const char* rd = ABI_NAMES[d.rd];
    const char* rs1 = ABI_NAMES[d.rs1];
    const char* rs2 = ABI_NAMES[d.rs2];
    char b[96];

    switch (d.op) {
    case Op::INVALID:
        std::snprintf(b, sizeof b, ".word 0x%08x  # inválida", inst);
        break;
    case Op::LUI:
    case Op::AUIPC:
        std::snprintf(b, sizeof b, "%s %s, 0x%x", n, rd, static_cast<uint32_t>(d.imm) >> 12);
        break;
    case Op::JAL:
        std::snprintf(b, sizeof b, "%s %s, 0x%x", n, rd, pc + static_cast<uint32_t>(d.imm));
        break;
    case Op::JALR:
    case Op::LB: case Op::LH: case Op::LW: case Op::LBU: case Op::LHU:
        std::snprintf(b, sizeof b, "%s %s, %d(%s)", n, rd, d.imm, rs1);
        break;
    case Op::SB: case Op::SH: case Op::SW:
        std::snprintf(b, sizeof b, "%s %s, %d(%s)", n, rs2, d.imm, rs1);
        break;
    case Op::BEQ: case Op::BNE: case Op::BLT: case Op::BGE: case Op::BLTU: case Op::BGEU:
        std::snprintf(b, sizeof b, "%s %s, %s, 0x%x", n, rs1, rs2, pc + static_cast<uint32_t>(d.imm));
        break;
    case Op::ADDI: case Op::SLTI: case Op::SLTIU: case Op::XORI: case Op::ORI: case Op::ANDI:
    case Op::SLLI: case Op::SRLI: case Op::SRAI:
        std::snprintf(b, sizeof b, "%s %s, %s, %d", n, rd, rs1, d.imm);
        break;
    case Op::ADD: case Op::SUB: case Op::SLL: case Op::SLT: case Op::SLTU:
    case Op::XOR: case Op::OR: case Op::AND: case Op::SRL: case Op::SRA:
        std::snprintf(b, sizeof b, "%s %s, %s, %s", n, rd, rs1, rs2);
        break;
    case Op::FENCE: case Op::ECALL: case Op::EBREAK:
        std::snprintf(b, sizeof b, "%s", n);
        break;
    }
    return b;
}
