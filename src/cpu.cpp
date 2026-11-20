#include "cpu.hpp"

#include <cstdio>

using namespace rv;

namespace {

std::string hex32(uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08x", v);
    return buf;
}

inline int32_t as_signed(uint32_t v) { return static_cast<int32_t>(v); }

// Desplazamiento aritmético portable (en C++17, `>>` sobre negativos es
// "implementation-defined"; aquí se rellena explícitamente con el bit de signo).
inline uint32_t sra32(uint32_t v, uint32_t sh) {
    sh &= 31;
    return (v & 0x80000000u) ? ~(~v >> sh) : (v >> sh);
}

}  // namespace

void Cpu::reset(uint32_t entry_pc, uint32_t sp) {
    for (auto& r : x_) r = 0;
    x_[2] = sp;
    pc = entry_pc;
    instret = 0;
    exit_code = 0;
    halted_ = false;
    message_.clear();
}

Status Cpu::fail(uint32_t inst, const std::string& what) {
    message_ = "Error en pc=" + hex32(pc) + " (instrucción " + hex32(inst) + "): " + what;
    return Status::Error;
}

Status Cpu::step() {
    if (halted_) return Status::Halted;

    // Sin extensión C, las instrucciones deben estar alineadas a 4 bytes.
    if (pc & 3) return fail(mem.read32(pc), "PC desalineado (no múltiplo de 4)");

    const uint32_t inst = mem.read32(pc);
    const Decoded d = decode(inst);

    // Se leen los operandos ANTES de escribir rd (importa cuando rd == rs1/rs2).
    const uint32_t a = reg(d.rs1);
    const uint32_t b = reg(d.rs2);
    const uint32_t imm = static_cast<uint32_t>(d.imm);
    uint32_t next_pc = pc + 4;

    switch (d.op) {
    // ---- U / J -----------------------------------------------------------
    case Op::LUI:   set_reg(d.rd, imm); break;
    case Op::AUIPC: set_reg(d.rd, pc + imm); break;
    case Op::JAL:
        set_reg(d.rd, pc + 4);
        next_pc = pc + imm;  // relativo al PC de la propia instrucción
        break;
    case Op::JALR:
        next_pc = (a + imm) & ~1u;  // `a` ya se leyó, así rd == rs1 funciona
        set_reg(d.rd, pc + 4);
        break;

    // ---- Branches (offset relativo al PC de la instrucción) --------------
    case Op::BEQ:  if (a == b) next_pc = pc + imm; break;
    case Op::BNE:  if (a != b) next_pc = pc + imm; break;
    case Op::BLT:  if (as_signed(a) < as_signed(b)) next_pc = pc + imm; break;
    case Op::BGE:  if (as_signed(a) >= as_signed(b)) next_pc = pc + imm; break;
    case Op::BLTU: if (a < b) next_pc = pc + imm; break;
    case Op::BGEU: if (a >= b) next_pc = pc + imm; break;

    // ---- Loads: lb/lh extienden signo, lbu/lhu extienden con ceros -------
    case Op::LB:  set_reg(d.rd, static_cast<uint32_t>(sext(mem.read8(a + imm), 8))); break;
    case Op::LH:  set_reg(d.rd, static_cast<uint32_t>(sext(mem.read16(a + imm), 16))); break;
    case Op::LW:  set_reg(d.rd, mem.read32(a + imm)); break;
    case Op::LBU: set_reg(d.rd, mem.read8(a + imm)); break;
    case Op::LHU: set_reg(d.rd, mem.read16(a + imm)); break;

    // ---- Stores: truncan al tamaño --------------------------------------
    case Op::SB: mem.write8(a + imm, static_cast<uint8_t>(b)); break;
    case Op::SH: mem.write16(a + imm, static_cast<uint16_t>(b)); break;
    case Op::SW: mem.write32(a + imm, b); break;

    // ---- Aritmética con inmediato ---------------------------------------
    case Op::ADDI:  set_reg(d.rd, a + imm); break;
    case Op::SLTI:  set_reg(d.rd, as_signed(a) < d.imm ? 1 : 0); break;
    case Op::SLTIU: set_reg(d.rd, a < imm ? 1 : 0); break;  // imm ya con signo extendido
    case Op::XORI:  set_reg(d.rd, a ^ imm); break;
    case Op::ORI:   set_reg(d.rd, a | imm); break;
    case Op::ANDI:  set_reg(d.rd, a & imm); break;
    case Op::SLLI:  set_reg(d.rd, a << (imm & 31)); break;
    case Op::SRLI:  set_reg(d.rd, a >> (imm & 31)); break;
    case Op::SRAI:  set_reg(d.rd, sra32(a, imm)); break;

    // ---- Registro-registro ----------------------------------------------
    case Op::ADD:  set_reg(d.rd, a + b); break;
    case Op::SUB:  set_reg(d.rd, a - b); break;
    case Op::SLL:  set_reg(d.rd, a << (b & 31)); break;
    case Op::SLT:  set_reg(d.rd, as_signed(a) < as_signed(b) ? 1 : 0); break;
    case Op::SLTU: set_reg(d.rd, a < b ? 1 : 0); break;
    case Op::XOR:  set_reg(d.rd, a ^ b); break;
    case Op::OR:   set_reg(d.rd, a | b); break;
    case Op::AND:  set_reg(d.rd, a & b); break;
    case Op::SRL:  set_reg(d.rd, a >> (b & 31)); break;
    case Op::SRA:  set_reg(d.rd, sra32(a, b)); break;

    // ---- Sistema ---------------------------------------------------------
    case Op::FENCE: break;  // NOP

    case Op::EBREAK:
        halt(0, "ebreak en pc=" + hex32(pc));
        ++instret;
        return Status::Halted;

    case Op::ECALL: {
        if (!ecall_handler)
            return fail(inst, "ecall no soportado (a7=" + std::to_string(reg(17)) + ")");
        std::string msg;
        const Status s = ecall_handler(*this, msg);
        if (s == Status::Error) return fail(inst, msg);
        ++instret;
        pc = next_pc;  // tras un exit el PC queda en la instrucción siguiente
        if (s == Status::Halted) {
            halted_ = true;
            if (message_.empty()) message_ = msg;
        }
        return s;
    }

    case Op::INVALID:
        return fail(inst, "instrucción inválida o no soportada");
    }

    pc = next_pc;
    ++instret;
    return Status::Ok;
}
