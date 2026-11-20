// test_main.cpp — Tests unitarios del simulador (mini-framework propio).
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <sstream>

#include "cpu.hpp"
#include "decode.hpp"
#include "disasm.hpp"
#include "elf.hpp"
#include "elf_builder.hpp"
#include "encoder.hpp"
#include "memory.hpp"
#include "syscalls.hpp"

using namespace enc;

// --------------------------------------------------------- mini framework ---
namespace {
int g_checks = 0, g_failures = 0;
std::vector<std::pair<const char*, std::function<void()>>>& registry() {
    static std::vector<std::pair<const char*, std::function<void()>>> r;
    return r;
}
struct Reg {
    Reg(const char* n, std::function<void()> f) { registry().emplace_back(n, std::move(f)); }
};
}  // namespace

#define TEST(name)                                     \
    static void name();                                \
    static Reg reg_##name(#name, name);                \
    static void name()

#define CHECK_EQ(a, b)                                                                     \
    do {                                                                                   \
        ++g_checks;                                                                        \
        const auto va_ = static_cast<unsigned long long>(a);                               \
        const auto vb_ = static_cast<unsigned long long>(b);                               \
        if (va_ != vb_) {                                                                  \
            ++g_failures;                                                                  \
            std::printf("  FALLO %s:%d: %s == %s  (0x%llx != 0x%llx)\n", __FILE__, __LINE__, \
                        #a, #b, va_, vb_);                                                 \
        }                                                                                  \
    } while (0)

#define CHECK(c) CHECK_EQ(!!(c), 1)

// Registros por número (para legibilidad).
enum { ZERO = 0, RA = 1, SP = 2, T0 = 5, T1 = 6, T2 = 7, S0 = 8, A0 = 10, A1 = 11, A2 = 12, A7 = 17 };

// Carga un programa en 0x0 y deja la CPU lista.
static Cpu make(const std::vector<uint32_t>& prog, uint32_t base = 0) {
    Cpu c;
    for (std::size_t k = 0; k < prog.size(); ++k)
        c.mem.write32(base + static_cast<uint32_t>(4 * k), prog[k]);
    c.reset(base);
    return c;
}

static void run(Cpu& c, int n) {
    for (int k = 0; k < n; ++k) CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Ok));
}

// ================================================================ memoria ===
TEST(memory_little_endian) {
    Memory m;
    m.write32(0x1000, 0x11223344);
    CHECK_EQ(m.read8(0x1000), 0x44);
    CHECK_EQ(m.read8(0x1001), 0x33);
    CHECK_EQ(m.read8(0x1002), 0x22);
    CHECK_EQ(m.read8(0x1003), 0x11);
    CHECK_EQ(m.read16(0x1000), 0x3344);
    CHECK_EQ(m.read16(0x1002), 0x1122);
    m.write16(0x2000, 0xBEEF);
    CHECK_EQ(m.read8(0x2000), 0xEF);
    CHECK_EQ(m.read8(0x2001), 0xBE);
}

TEST(memory_sparse_and_unwritten_reads_zero) {
    Memory m;
    CHECK_EQ(m.read32(0xDEADBEEC), 0);
    CHECK_EQ(m.pages_in_use(), 0);
    m.write8(0xFFFFFFFF, 0xAB);  // última dirección del espacio de 4 GiB
    CHECK_EQ(m.read8(0xFFFFFFFF), 0xAB);
    CHECK_EQ(m.pages_in_use(), 1);
    m.write8(0x5000, 0);  // escribir 0 en página vacía no reserva página
    CHECK_EQ(m.pages_in_use(), 1);
}

TEST(memory_misaligned_and_page_crossing) {
    Memory m;
    m.write32(0x0FFE, 0xCAFEBABE);  // cruza de la página 0 a la 1
    CHECK_EQ(m.read32(0x0FFE), 0xCAFEBABE);
    CHECK_EQ(m.read8(0x0FFF), 0xBA);
    CHECK_EQ(m.read8(0x1000), 0xFE);
    m.write32(0xFFFFFFFE, 0x01020304);  // envuelve en 2^32
    CHECK_EQ(m.read8(0x00000000), 0x02);
    CHECK_EQ(m.read32(0xFFFFFFFE), 0x01020304);
}

// ============================================================ inmediatos ===
TEST(immediates_extremes) {
    CHECK_EQ(rv::imm_i(ADDI(1, 0, -2048)), -2048);
    CHECK_EQ(rv::imm_i(ADDI(1, 0, 2047)), 2047);
    CHECK_EQ(rv::imm_s(SW(1, 2, -2048)), -2048);
    CHECK_EQ(rv::imm_s(SW(1, 2, 2047)), 2047);
    CHECK_EQ(rv::imm_b(BEQ(1, 2, -4096)), -4096);
    CHECK_EQ(rv::imm_b(BEQ(1, 2, 4094)), 4094);
    CHECK_EQ(rv::imm_b(BEQ(1, 2, -2)), -2);
    CHECK_EQ(rv::imm_j(JAL(1, 1048574)), 1048574);
    CHECK_EQ(rv::imm_j(JAL(1, -1048576)), -1048576);
    CHECK_EQ(rv::imm_j(JAL(1, 2048)), 2048);  // bit 11 en inst[20]
    CHECK_EQ(static_cast<uint32_t>(rv::imm_u(LUI(1, 0xFFFFF))), 0xFFFFF000u);
    // Palabras reales de riscvtest.s
    CHECK_EQ(rv::imm_i(0xFF718393), -9);       // addi x7, x3, -9
    CHECK_EQ(rv::imm_s(0x0471AA23), 84);       // sw x7, 84(x3)
    CHECK_EQ(rv::imm_b(0x02728863), 0x48 - 0x18);  // beq x5, x7, end
    CHECK_EQ(rv::imm_j(0x008001EF), 8);        // jal x3, end
}

TEST(decode_rejects_invalid_encodings) {
    using rv::Op;
    CHECK(rv::decode(0x00000000).op == Op::INVALID);
    CHECK(rv::decode(0xFFFFFFFF).op == Op::INVALID);
    CHECK(rv::decode(0xAAAAAAAA).op == Op::INVALID);  // relleno de CPUlator
    CHECK(rv::decode(r(0x10, 1, 2, 5, 3, OPI)).op == Op::INVALID);  // srai con funct7 malo
    CHECK(rv::decode(r(0x01, 1, 2, 0, 3, OPR)).op == Op::INVALID);  // mul (RV32M) no soportado
    CHECK(rv::decode(r(0x20, 1, 2, 1, 3, OPR)).op == Op::INVALID);  // "sub" con funct3=001
    CHECK(rv::decode(i(0, 1, 3, 2, LD)).op == Op::INVALID);         // ld (RV64)
    CHECK(rv::decode(s(0, 1, 2, 3, ST)).op == Op::INVALID);         // sd (RV64)
    CHECK(rv::decode(b(8, 1, 2, 2)).op == Op::INVALID);             // branch funct3=010
    CHECK(rv::decode(i(0, 1, 1, 2, 0b1100111)).op == Op::INVALID);  // jalr funct3!=0
    CHECK(rv::decode(0x30200073).op == Op::INVALID);                // mret
}

TEST(decode_all_37_instructions) {
    using rv::Op;
    const std::vector<std::pair<uint32_t, Op>> cases = {
        {LUI(1, 1), Op::LUI}, {AUIPC(1, 1), Op::AUIPC}, {JAL(1, 8), Op::JAL},
        {JALR(1, 2, 0), Op::JALR}, {BEQ(1, 2, 8), Op::BEQ}, {BNE(1, 2, 8), Op::BNE},
        {BLT(1, 2, 8), Op::BLT}, {BGE(1, 2, 8), Op::BGE}, {BLTU(1, 2, 8), Op::BLTU},
        {BGEU(1, 2, 8), Op::BGEU}, {LB(1, 2, 0), Op::LB}, {LH(1, 2, 0), Op::LH},
        {LW(1, 2, 0), Op::LW}, {LBU(1, 2, 0), Op::LBU}, {LHU(1, 2, 0), Op::LHU},
        {SB(1, 2, 0), Op::SB}, {SH(1, 2, 0), Op::SH}, {SW(1, 2, 0), Op::SW},
        {ADDI(1, 2, 3), Op::ADDI}, {SLTI(1, 2, 3), Op::SLTI}, {SLTIU(1, 2, 3), Op::SLTIU},
        {XORI(1, 2, 3), Op::XORI}, {ORI(1, 2, 3), Op::ORI}, {ANDI(1, 2, 3), Op::ANDI},
        {SLLI(1, 2, 3), Op::SLLI}, {SRLI(1, 2, 3), Op::SRLI}, {SRAI(1, 2, 3), Op::SRAI},
        {ADD(1, 2, 3), Op::ADD}, {SUB(1, 2, 3), Op::SUB}, {SLL(1, 2, 3), Op::SLL},
        {SLT(1, 2, 3), Op::SLT}, {SLTU(1, 2, 3), Op::SLTU}, {XOR(1, 2, 3), Op::XOR},
        {OR(1, 2, 3), Op::OR}, {AND(1, 2, 3), Op::AND}, {SRL(1, 2, 3), Op::SRL},
        {SRA(1, 2, 3), Op::SRA},
    };
    CHECK_EQ(cases.size(), 37);
    for (const auto& c : cases) CHECK_EQ(static_cast<int>(rv::decode(c.first).op), static_cast<int>(c.second));
}

// ================================================================== U / J ===
TEST(lui_high_bit) {
    Cpu c = make({LUI(T0, 0x80000), LUI(T1, 0xFFFFF), LUI(T2, 0x12345)});
    run(c, 3);
    CHECK_EQ(c.reg(T0), 0x80000000u);
    CHECK_EQ(c.reg(T1), 0xFFFFF000u);
    CHECK_EQ(c.reg(T2), 0x12345000u);
}

TEST(auipc_uses_own_pc) {
    Cpu c = make({ADDI(0, 0, 0), AUIPC(T0, 0x00001), AUIPC(T1, 0xFFFFF)});
    run(c, 3);
    CHECK_EQ(c.reg(T0), 0x4u + 0x1000u);
    CHECK_EQ(c.reg(T1), 0x8u + 0xFFFFF000u);  // equivale a pc - 4096
}

TEST(jal_link_and_target) {
    Cpu c = make({JAL(RA, 12)});
    run(c, 1);
    CHECK_EQ(c.pc, 12);
    CHECK_EQ(c.reg(RA), 4);
}

TEST(jal_max_min_offsets) {
    Cpu c = make({JAL(RA, 1048574)});
    run(c, 1);
    CHECK_EQ(c.pc, 1048574u);
    Cpu d = make({JAL(RA, -1048576)}, 0x200000);
    run(d, 1);
    CHECK_EQ(d.pc, 0x200000u - 1048576u);
    CHECK_EQ(d.reg(RA), 0x200004u);
}

TEST(jal_rd_x0_no_link) {
    Cpu c = make({JAL(ZERO, 8)});
    run(c, 1);
    CHECK_EQ(c.pc, 8);
    CHECK_EQ(c.reg(ZERO), 0);
}

TEST(jalr_rd_equals_rs1_and_clears_bit0) {
    // t0 = 0x101; jalr t0, 3(t0)  -> pc = (0x101 + 3) & ~1 = 0x104, t0 = pc+4 = 8
    Cpu c = make({ADDI(T0, 0, 0x101), JALR(T0, T0, 3)});
    run(c, 2);
    CHECK_EQ(c.pc, 0x104);
    CHECK_EQ(c.reg(T0), 8);
    // jalr con offset negativo (ret estilo)
    Cpu d = make({ADDI(RA, 0, 0x40), JALR(ZERO, RA, -8)});
    run(d, 2);
    CHECK_EQ(d.pc, 0x38);
}

// =============================================================== branches ===
TEST(branches_taken_and_not_taken) {
    struct Case { uint32_t inst; uint32_t a, b; bool taken; };
    const uint32_t NEG1 = 0xFFFFFFFF;
    const std::vector<Case> cases = {
        {BEQ(T0, T1, 16), 5, 5, true},   {BEQ(T0, T1, 16), 5, 6, false},
        {BNE(T0, T1, 16), 5, 6, true},   {BNE(T0, T1, 16), 5, 5, false},
        {BLT(T0, T1, 16), NEG1, 1, true}, {BLT(T0, T1, 16), 1, NEG1, false},
        {BGE(T0, T1, 16), 1, NEG1, true}, {BGE(T0, T1, 16), 5, 5, true},
        {BGE(T0, T1, 16), NEG1, 1, false},
        {BLTU(T0, T1, 16), 1, NEG1, true}, {BLTU(T0, T1, 16), NEG1, 1, false},
        {BGEU(T0, T1, 16), NEG1, 1, true}, {BGEU(T0, T1, 16), 1, NEG1, false},
        {BGEU(T0, T1, 16), 7, 7, true},
    };
    for (const auto& k : cases) {
        Cpu c = make({k.inst}, 0x100);
        c.set_reg(T0, k.a);
        c.set_reg(T1, k.b);
        run(c, 1);
        CHECK_EQ(c.pc, k.taken ? 0x110u : 0x104u);
    }
}

TEST(branch_backward_loop) {
    // t0 = 3; loop: addi t0, t0, -1; bne t0, x0, loop  -> 3 iteraciones
    Cpu c = make({ADDI(T0, 0, 3), ADDI(T0, T0, -1), BNE(T0, ZERO, -4), ADDI(T1, 0, 7)});
    run(c, 1 + 3 * 2 + 1);
    CHECK_EQ(c.reg(T0), 0);
    CHECK_EQ(c.reg(T1), 7);
    CHECK_EQ(c.pc, 16);
}

// ================================================================== loads ===
TEST(loads_sign_and_zero_extension) {
    Cpu c = make({LB(T0, S0, 0), LBU(T1, S0, 0), LH(T2, S0, 2), LHU(A0, S0, 2), LW(A1, S0, 0),
                  LB(A2, S0, 1)});
    c.set_reg(S0, 0x1000);
    c.mem.write32(0x1000, 0x80F17F80);  // bytes: 80 7F F1 80
    run(c, 6);
    CHECK_EQ(c.reg(T0), 0xFFFFFF80u);  // lb 0x80 -> negativo
    CHECK_EQ(c.reg(T1), 0x00000080u);  // lbu 0x80
    CHECK_EQ(c.reg(T2), 0xFFFF80F1u);  // lh 0x80F1
    CHECK_EQ(c.reg(A0), 0x000080F1u);  // lhu 0x80F1
    CHECK_EQ(c.reg(A1), 0x80F17F80u);
    CHECK_EQ(c.reg(A2), 0x0000007Fu);  // lb positivo
}

TEST(load_negative_offset_and_misaligned) {
    Cpu c = make({LW(T0, S0, -4), LW(T1, S0, -3)});
    c.set_reg(S0, 0x1004);
    c.mem.write32(0x1000, 0x44332211);
    c.mem.write8(0x1004, 0x55);
    run(c, 2);
    CHECK_EQ(c.reg(T0), 0x44332211u);
    CHECK_EQ(c.reg(T1), 0x55443322u);  // desalineado: permitido
}

// ================================================================= stores ===
TEST(stores_truncate) {
    Cpu c = make({SB(T0, S0, 0), SH(T0, S0, 4), SW(T0, S0, 8), SW(T0, S0, -4)});
    c.set_reg(S0, 0x2000);
    c.set_reg(T0, 0xA1B2C3D4);
    run(c, 4);
    CHECK_EQ(c.mem.read32(0x2000), 0x000000D4u);
    CHECK_EQ(c.mem.read32(0x2004), 0x0000C3D4u);
    CHECK_EQ(c.mem.read32(0x2008), 0xA1B2C3D4u);
    CHECK_EQ(c.mem.read32(0x1FFC), 0xA1B2C3D4u);
    CHECK_EQ(c.mem.read8(0x2008), 0xD4);  // little-endian
}

// ============================================================ inmediatas ===
TEST(addi_negative_and_wrap) {
    Cpu c = make({ADDI(T0, 0, -1), ADDI(T1, T0, 1), ADDI(T2, 0, -2048), ADDI(A0, 0, 2047)});
    run(c, 4);
    CHECK_EQ(c.reg(T0), 0xFFFFFFFFu);
    CHECK_EQ(c.reg(T1), 0);
    CHECK_EQ(c.reg(T2), static_cast<uint32_t>(-2048));
    CHECK_EQ(c.reg(A0), 2047);
}

TEST(slti_sltiu) {
    Cpu c = make({SLTI(T0, A0, 0), SLTIU(T1, A0, 0), SLTIU(T2, ZERO, -1), SLTI(A1, ZERO, -1),
                  SLTIU(A2, A0, -1)});
    c.set_reg(A0, 0xFFFFFFFF);  // -1
    run(c, 5);
    CHECK_EQ(c.reg(T0), 1);  // -1 < 0 con signo
    CHECK_EQ(c.reg(T1), 0);  // 0xFFFFFFFF < 0 sin signo: falso
    CHECK_EQ(c.reg(T2), 1);  // 0 < 0xFFFFFFFF (imm -1 extendido y comparado sin signo)
    CHECK_EQ(c.reg(A1), 0);  // 0 < -1: falso
    CHECK_EQ(c.reg(A2), 0);  // 0xFFFFFFFF < 0xFFFFFFFF: falso
}

TEST(logic_immediates) {
    Cpu c = make({XORI(T0, A0, -1), ORI(T1, A0, 0x0F0), ANDI(T2, A0, 0x0FF), ANDI(A1, A0, -16)});
    c.set_reg(A0, 0x12345678);
    run(c, 4);
    CHECK_EQ(c.reg(T0), 0xEDCBA987u);  // xori -1 == not
    CHECK_EQ(c.reg(T1), 0x123456F8u);
    CHECK_EQ(c.reg(T2), 0x00000078u);
    CHECK_EQ(c.reg(A1), 0x12345670u);  // andi con imm negativo extendido
}

TEST(shift_immediates) {
    Cpu c = make({SLLI(T0, A0, 4), SRLI(T1, A0, 4), SRAI(T2, A0, 4), SRAI(A1, A0, 31),
                  SRAI(A2, A0, 0), SRAI(S0, A7, 31), SLLI(SP, A0, 31)});
    c.set_reg(A0, 0x80000010);
    c.set_reg(A7, 0x7FFFFFFF);
    run(c, 7);
    CHECK_EQ(c.reg(T0), 0x00000100u);
    CHECK_EQ(c.reg(T1), 0x08000001u);
    CHECK_EQ(c.reg(T2), 0xF8000001u);  // aritmético: replica el signo
    CHECK_EQ(c.reg(A1), 0xFFFFFFFFu);
    CHECK_EQ(c.reg(A2), 0x80000010u);
    CHECK_EQ(c.reg(S0), 0);            // positivo >> 31
    CHECK_EQ(c.reg(SP), 0);            // 0x80000010 << 31 (bit 0 es 0)
}

// ============================================================== registro ===
TEST(add_sub_wrap) {
    Cpu c = make({ADD(T0, A0, A1), SUB(T1, ZERO, A1), SUB(T2, A1, A0)});
    c.set_reg(A0, 0xFFFFFFFF);
    c.set_reg(A1, 2);
    run(c, 3);
    CHECK_EQ(c.reg(T0), 1);            // overflow envuelve
    CHECK_EQ(c.reg(T1), 0xFFFFFFFEu);  // -2
    CHECK_EQ(c.reg(T2), 3);
}

TEST(slt_vs_sltu_with_all_ones) {
    Cpu c = make({SLT(T0, A0, A1), SLTU(T1, A0, A1), SLT(T2, A1, A0), SLTU(A2, A1, A0)});
    c.set_reg(A0, 0xFFFFFFFF);
    c.set_reg(A1, 1);
    run(c, 4);
    CHECK_EQ(c.reg(T0), 1);  // -1 < 1
    CHECK_EQ(c.reg(T1), 0);  // 0xFFFFFFFF < 1 sin signo: falso
    CHECK_EQ(c.reg(T2), 0);
    CHECK_EQ(c.reg(A2), 1);
}

TEST(logic_registers) {
    Cpu c = make({XOR(T0, A0, A1), OR(T1, A0, A1), AND(T2, A0, A1)});
    c.set_reg(A0, 0xF0F0F0F0);
    c.set_reg(A1, 0xFF00FF00);
    run(c, 3);
    CHECK_EQ(c.reg(T0), 0x0FF00FF0u);
    CHECK_EQ(c.reg(T1), 0xFFF0FFF0u);
    CHECK_EQ(c.reg(T2), 0xF000F000u);
}

TEST(shift_registers_use_low_5_bits) {
    Cpu c = make({SLL(T0, A0, A1), SRL(T1, A2, A1), SRA(T2, A2, A1), SRA(S0, A2, A7),
                  SRL(SP, A2, A7)});
    c.set_reg(A0, 1);
    c.set_reg(A1, 33);  // solo cuentan los 5 bits bajos -> 1
    c.set_reg(A2, 0x80000000);
    c.set_reg(A7, 31);
    run(c, 5);
    CHECK_EQ(c.reg(T0), 2);
    CHECK_EQ(c.reg(T1), 0x40000000u);
    CHECK_EQ(c.reg(T2), 0xC0000000u);  // sra con negativo
    CHECK_EQ(c.reg(S0), 0xFFFFFFFFu);
    CHECK_EQ(c.reg(SP), 1);
}

// ============================================================ x0 y otros ===
TEST(x0_is_hardwired_zero) {
    Cpu c = make({ADDI(ZERO, 0, 123), LUI(ZERO, 0xFFFFF), ADD(T0, ZERO, ZERO), JAL(ZERO, 4)});
    run(c, 4);
    CHECK_EQ(c.reg(ZERO), 0);
    CHECK_EQ(c.reg(T0), 0);
    c.set_reg(0, 99);
    CHECK_EQ(c.reg(0), 0);
}

TEST(rd_equals_rs_reads_before_write) {
    Cpu c = make({ADD(T0, T0, T0), SUB(T1, T1, T1), LW(S0, S0, 0)});
    c.set_reg(T0, 21);
    c.set_reg(T1, 5);
    c.set_reg(S0, 0x3000);
    c.mem.write32(0x3000, 0xABCD);
    run(c, 3);
    CHECK_EQ(c.reg(T0), 42);
    CHECK_EQ(c.reg(T1), 0);
    CHECK_EQ(c.reg(S0), 0xABCD);
}

TEST(initial_state) {
    Cpu c;
    CHECK_EQ(c.pc, 0);
    CHECK_EQ(c.reg(SP), DEFAULT_SP);
    for (unsigned r = 0; r < 32; ++r)
        if (r != SP) CHECK_EQ(c.reg(r), 0);
}

TEST(fence_is_nop) {
    Cpu c = make({FENCE, ADDI(T0, 0, 1)});
    run(c, 2);
    CHECK_EQ(c.reg(T0), 1);
    CHECK_EQ(c.pc, 8);
}

// ================================================================ errores ===
TEST(invalid_instruction_reports_pc_and_word) {
    Cpu c = make({ADDI(T0, 0, 1), 0xFFFFFFFF});
    run(c, 1);
    CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Error));
    CHECK(c.message().find("0x00000004") != std::string::npos);
    CHECK(c.message().find("0xffffffff") != std::string::npos);
    CHECK_EQ(c.pc, 4);  // no avanza
    CHECK_EQ(c.instret, 1);
}

TEST(misaligned_pc_is_error) {
    Cpu c = make({JALR(ZERO, T0, 0)});
    c.set_reg(T0, 0x102);  // bit 1 activo: no múltiplo de 4
    run(c, 1);
    CHECK_EQ(c.pc, 0x102);
    CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Error));
    CHECK(c.message().find("desalineado") != std::string::npos);
}

TEST(ebreak_halts) {
    Cpu c = make({ADDI(T0, 0, 1), EBREAK, ADDI(T0, 0, 2)});
    run(c, 1);
    CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Halted));
    CHECK(c.halted());
    CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Halted));
    CHECK_EQ(c.reg(T0), 1);
}

TEST(ecall_without_handler_is_error) {
    Cpu c = make({ECALL});
    c.set_reg(A7, 93);
    CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Error));
    CHECK(c.message().find("ecall") != std::string::npos);
}

// ======================================================= mini-programas ===
TEST(riscvtest_program_from_listing) {
    // Palabras de máquina tomadas del listado de tests/asm/riscvtest.s.
    Cpu c = make({0x00500113, 0x00C00193, 0xFF718393, 0x0023E233, 0x0041F2B3, 0x004282B3,
                  0x02728863, 0x0041A233, 0x00020463, 0x00000293, 0x0023A233, 0x005203B3,
                  0x402383B3, 0x0471AA23, 0x06002103, 0x005104B3, 0x008001EF, 0x00100113,
                  0x00910133, 0x0221A023, 0x00210063});
    int steps = 0;
    while (steps < 100) {
        const uint32_t old = c.pc;
        CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Ok));
        ++steps;
        if (c.pc == old) break;  // done: beq x2, x2, done
    }
    CHECK_EQ(c.pc, 0x50);
    CHECK_EQ(c.mem.read32(100), 25);
    CHECK_EQ(c.reg(2), 25);
    CHECK_EQ(c.reg(3), 0x44);
}

// ========================================================= desensamblador ===
TEST(disasm_formats) {
    CHECK(disasm(0x00500113, 0) == "addi sp, zero, 5");
    CHECK(disasm(0xFF718393, 0) == "addi t2, gp, -9");
    CHECK(disasm(0x0471AA23, 0) == "sw t2, 84(gp)");
    CHECK(disasm(0x06002103, 0) == "lw sp, 96(zero)");
    CHECK(disasm(0x02728863, 0x18) == "beq t0, t2, 0x48");
    CHECK(disasm(0x008001EF, 0x40) == "jal gp, 0x48");
    CHECK(disasm(0x402383B3, 0) == "sub t2, t2, sp");
    CHECK(disasm(0x00008067, 0) == "jalr zero, 0(ra)");
    CHECK(disasm(0x00010137, 0) == "lui sp, 0x10");
    CHECK(disasm(SRAI(T0, A0, 3), 0) == "srai t0, a0, 3");
    CHECK(disasm(BNE(T1, ZERO, -8), 0x20) == "bne t1, zero, 0x18");
    CHECK(disasm(ECALL, 0) == "ecall");
    CHECK(disasm(0xAAAAAAAA, 0) == ".word 0xaaaaaaaa  # inválida");
}

// =============================================================== syscalls ===
TEST(syscalls_print_and_exit) {
    Cpu c = make({ADDI(A0, 0, -42), ADDI(A7, 0, 1), ECALL,              // print_int
                  ADDI(A0, 0, 0x100), ADDI(A7, 0, 4), ECALL,            // print_string
                  ADDI(A0, 0, 'Z'), ADDI(A7, 0, 11), ECALL,             // print_char
                  ADDI(A0, 0, -1), ADDI(A7, 0, 36), ECALL,              // print_unsigned
                  ADDI(A0, 0, 255), ADDI(A7, 0, 34), ECALL,             // print_hex
                  ADDI(A0, 0, 7), ADDI(A7, 0, 17), ECALL,               // exit2(7)
                  ADDI(T0, 0, 1)});
    const char* str = " hola ";
    for (int k = 0; str[k]; ++k) c.mem.write8(0x100 + static_cast<uint32_t>(k), static_cast<uint8_t>(str[k]));
    std::istringstream in;
    std::ostringstream out;
    install_spim_syscalls(c, in, out);
    run(c, 15);
    CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Ok));
    CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Ok));
    CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Halted));
    CHECK(out.str() == "-42 hola Z42949672950x000000ff");
    CHECK(c.halted());
    CHECK_EQ(c.exit_code, 7);
    CHECK_EQ(c.reg(T0), 0);  // no se ejecutó nada después del exit
}

TEST(syscalls_read_and_sbrk) {
    Cpu c = make({ADDI(A7, 0, 5), ECALL, ADDI(S0, A0, 0),                // read_int
                  ADDI(A7, 0, 12), ECALL, ADDI(T0, A0, 0),               // read_char
                  ADDI(A0, 0, 0x200), ADDI(A1, 0, 4), ADDI(A7, 0, 8), ECALL,  // read_string
                  ADDI(A0, 0, 10), ADDI(A7, 0, 9), ECALL, ADDI(T1, A0, 0),    // sbrk(10)
                  ADDI(A0, 0, 4), ECALL, ADDI(T2, A0, 0),                // sbrk(4)
                  ADDI(A7, 0, 10), ECALL});                              // exit
    std::istringstream in("-17\nXabcdef\n");
    std::ostringstream out;
    install_spim_syscalls(c, in, out);
    run(c, 18);
    CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Halted));
    CHECK_EQ(c.reg(S0), static_cast<uint32_t>(-17));
    CHECK_EQ(c.reg(T0), 'X');
    CHECK_EQ(c.mem.read8(0x200), 'a');
    CHECK_EQ(c.mem.read8(0x202), 'c');
    CHECK_EQ(c.mem.read8(0x203), 0);  // a1 = 4 -> 3 caracteres + '\0'
    CHECK_EQ(c.reg(T1), HEAP_BASE);
    CHECK_EQ(c.reg(T2), HEAP_BASE + 12);  // 10 redondeado a 12
    CHECK_EQ(c.exit_code, 0);
}

TEST(syscall_unknown_or_bad_input_is_error) {
    Cpu c = make({ADDI(A7, 0, 2), ECALL});  // print_float: no soportado en RV32I
    std::istringstream in("no-es-numero\n");
    std::ostringstream out;
    install_spim_syscalls(c, in, out);
    run(c, 1);
    CHECK_EQ(static_cast<int>(c.step()), static_cast<int>(Status::Error));
    CHECK(c.message().find("a7=2") != std::string::npos);
    CHECK_EQ(c.pc, 4);

    Cpu d = make({ADDI(A7, 0, 5), ECALL});
    install_spim_syscalls(d, in, out);
    run(d, 1);
    CHECK_EQ(static_cast<int>(d.step()), static_cast<int>(Status::Error));
    CHECK(d.message().find("read_int") != std::string::npos);
}

// ==================================================================== ELF ===
TEST(elf_load_segment_entry_and_bss) {
    std::vector<uint8_t> img = {0x13, 0x05, 0xA0, 0x02, 0xFF, 0xEE};  // addi a0, zero, 42 + datos
    const auto f = elfb::build(img, 0x80000000, 0x80000000, 16);
    CHECK(is_elf(f));
    Memory m;
    m.write8(0x80000007, 0x99);  // basura que .bss debe poner en 0
    uint32_t entry = 0;
    std::size_t bytes = 0;
    unsigned segs = 0;
    std::string err;
    CHECK(load_elf(f, m, entry, bytes, segs, err));
    CHECK_EQ(entry, 0x80000000u);
    CHECK_EQ(segs, 1);
    CHECK_EQ(bytes, 6 + 16);
    CHECK_EQ(m.read32(0x80000000), 0x02A00513u);
    CHECK_EQ(m.read8(0x80000005), 0xEE);
    CHECK_EQ(m.read8(0x80000007), 0);
}

TEST(elf_rejects_bad_files) {
    const auto good = elfb::build({1, 2, 3, 4}, 0x1000, 0x1000);
    Memory m;
    uint32_t entry;
    std::size_t bytes;
    unsigned segs;
    std::string err;

    auto bad_machine = good;
    elfb::put16(bad_machine, 18, 62);  // x86-64
    CHECK(!load_elf(bad_machine, m, entry, bytes, segs, err));
    CHECK(err.find("RISC-V") != std::string::npos);

    auto bad_class = good;
    bad_class[4] = 2;  // ELF64
    CHECK(!load_elf(bad_class, m, entry, bytes, segs, err));

    std::vector<uint8_t> truncated(good.begin(), good.begin() + 0x101);  // segmento cortado
    CHECK(!load_elf(truncated, m, entry, bytes, segs, err));
    CHECK(err.find("truncado") != std::string::npos);

    CHECK(!is_elf({0x13, 0x01, 0x50, 0x00}));  // binario crudo
}

// ===================================================================== main ===
int main() {
    for (auto& t : registry()) {
        const int before = g_failures;
        t.second();
        std::printf("[%s] %s\n", g_failures == before ? " OK " : "FALLO", t.first);
    }
    std::printf("\n%zu tests, %d comprobaciones, %d fallos\n", registry().size(), g_checks,
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
