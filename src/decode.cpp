#include "decode.hpp"

namespace rv {

const char* const ABI_NAMES[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

int parse_reg(const std::string& name) {
    if (name.size() >= 2 && name[0] == 'x') {
        int n = 0;
        for (std::size_t i = 1; i < name.size(); ++i) {
            if (name[i] < '0' || name[i] > '9') return -1;
            n = n * 10 + (name[i] - '0');
            if (n > 31) return -1;
        }
        // Rechaza ceros a la izquierda raros como "x05".
        if (name.size() > 2 && name[1] == '0') return -1;
        return n;
    }
    if (name == "fp") return 8;
    for (int i = 0; i < 32; ++i)
        if (name == ABI_NAMES[i]) return i;
    return -1;
}

Decoded decode(uint32_t inst) {
    Decoded d;
    d.raw = inst;
    d.rd = rd(inst);
    d.rs1 = rs1(inst);
    d.rs2 = rs2(inst);
    const uint32_t f3 = funct3(inst);
    const uint32_t f7 = funct7(inst);

    switch (opcode(inst)) {
    case OP_LUI:   d.op = Op::LUI;   d.fmt = Format::U; d.imm = imm_u(inst); break;
    case OP_AUIPC: d.op = Op::AUIPC; d.fmt = Format::U; d.imm = imm_u(inst); break;
    case OP_JAL:   d.op = Op::JAL;   d.fmt = Format::J; d.imm = imm_j(inst); break;

    case OP_JALR:
        if (f3 == 0) { d.op = Op::JALR; d.fmt = Format::I; d.imm = imm_i(inst); }
        break;

    case OP_BRANCH: {
        static const Op tbl[8] = {Op::BEQ, Op::BNE, Op::INVALID, Op::INVALID,
                                  Op::BLT, Op::BGE, Op::BLTU, Op::BGEU};
        d.op = tbl[f3];
        d.fmt = Format::B;
        d.imm = imm_b(inst);
        break;
    }

    case OP_LOAD: {
        static const Op tbl[8] = {Op::LB, Op::LH, Op::LW, Op::INVALID,
                                  Op::LBU, Op::LHU, Op::INVALID, Op::INVALID};
        d.op = tbl[f3];
        d.fmt = Format::I;
        d.imm = imm_i(inst);
        break;
    }

    case OP_STORE: {
        static const Op tbl[8] = {Op::SB, Op::SH, Op::SW, Op::INVALID,
                                  Op::INVALID, Op::INVALID, Op::INVALID, Op::INVALID};
        d.op = tbl[f3];
        d.fmt = Format::S;
        d.imm = imm_s(inst);
        break;
    }

    case OP_IMM:
        d.fmt = Format::I;
        d.imm = imm_i(inst);
        switch (f3) {
        case 0b000: d.op = Op::ADDI; break;
        case 0b010: d.op = Op::SLTI; break;
        case 0b011: d.op = Op::SLTIU; break;
        case 0b100: d.op = Op::XORI; break;
        case 0b110: d.op = Op::ORI; break;
        case 0b111: d.op = Op::ANDI; break;
        // En los shifts, inst[24:20] es shamt e inst[31:25] distingue lógico/aritmético.
        case 0b001:
            if (f7 == 0b0000000) { d.op = Op::SLLI; d.imm = static_cast<int32_t>(d.rs2); }
            break;
        case 0b101:
            if (f7 == 0b0000000) { d.op = Op::SRLI; d.imm = static_cast<int32_t>(d.rs2); }
            else if (f7 == 0b0100000) { d.op = Op::SRAI; d.imm = static_cast<int32_t>(d.rs2); }
            break;
        }
        break;

    case OP_REG:
        d.fmt = Format::R;
        if (f7 == 0b0000000) {
            static const Op tbl[8] = {Op::ADD, Op::SLL, Op::SLT, Op::SLTU,
                                      Op::XOR, Op::SRL, Op::OR, Op::AND};
            d.op = tbl[f3];
        } else if (f7 == 0b0100000) {
            if (f3 == 0b000) d.op = Op::SUB;
            else if (f3 == 0b101) d.op = Op::SRA;
        }
        break;

    case OP_FENCE:
        // fence / fence.i: en un simulador secuencial sin caché son NOP.
        if (f3 == 0b000 || f3 == 0b001) { d.op = Op::FENCE; d.fmt = Format::NONE; }
        break;

    case OP_SYSTEM:
        // Solo se aceptan las codificaciones exactas de ecall y ebreak.
        if (inst == 0x00000073u) d.op = Op::ECALL;
        else if (inst == 0x00100073u) d.op = Op::EBREAK;
        break;

    default:
        break;
    }

    if (d.op == Op::INVALID) d.fmt = Format::NONE;
    return d;
}

const char* op_name(Op op) {
    switch (op) {
    case Op::LUI: return "lui";     case Op::AUIPC: return "auipc";
    case Op::JAL: return "jal";     case Op::JALR: return "jalr";
    case Op::BEQ: return "beq";     case Op::BNE: return "bne";
    case Op::BLT: return "blt";     case Op::BGE: return "bge";
    case Op::BLTU: return "bltu";   case Op::BGEU: return "bgeu";
    case Op::LB: return "lb";       case Op::LH: return "lh";
    case Op::LW: return "lw";       case Op::LBU: return "lbu";
    case Op::LHU: return "lhu";     case Op::SB: return "sb";
    case Op::SH: return "sh";       case Op::SW: return "sw";
    case Op::ADDI: return "addi";   case Op::SLTI: return "slti";
    case Op::SLTIU: return "sltiu"; case Op::XORI: return "xori";
    case Op::ORI: return "ori";     case Op::ANDI: return "andi";
    case Op::SLLI: return "slli";   case Op::SRLI: return "srli";
    case Op::SRAI: return "srai";   case Op::ADD: return "add";
    case Op::SUB: return "sub";     case Op::SLL: return "sll";
    case Op::SLT: return "slt";     case Op::SLTU: return "sltu";
    case Op::XOR: return "xor";     case Op::OR: return "or";
    case Op::AND: return "and";     case Op::SRL: return "srl";
    case Op::SRA: return "sra";     case Op::FENCE: return "fence";
    case Op::ECALL: return "ecall"; case Op::EBREAK: return "ebreak";
    case Op::INVALID: break;
    }
    return "invalid";
}

}  // namespace rv
