#include "syscalls.hpp"

#include <chrono>
#include <cstdio>
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <thread>

namespace {

constexpr unsigned A0 = 10, A1 = 11, A7 = 17;
constexpr uint32_t MAX_STRING = 1u << 20;  // límite de seguridad para print_string

bool parse_int(const std::string& s, int32_t& out) {
    try {
        std::size_t pos = 0;
        // Decimal (con signo) o hex con prefijo 0x; un "010" se lee como 10, no octal.
        const bool hex = s.find("0x") != std::string::npos || s.find("0X") != std::string::npos;
        const long long v = std::stoll(s, &pos, hex ? 16 : 10);
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r')) ++pos;
        if (pos != s.size()) return false;
        out = static_cast<int32_t>(static_cast<uint32_t>(v));
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

void install_spim_syscalls(Cpu& cpu, std::istream& in, std::ostream& out) {
    // El "break" del heap se guarda fuera del Cpu para no acoplarlo a los syscalls.
    auto brk = std::make_shared<uint32_t>(HEAP_BASE);

    cpu.ecall_handler = [&in, &out, brk](Cpu& c, std::string& msg) -> Status {
        const uint32_t a0 = c.reg(A0);
        const uint32_t num = c.reg(A7);
        switch (num) {
        case 1:  // print_int
            out << static_cast<int32_t>(a0) << std::flush;
            return Status::Ok;
        case 4: {  // print_string
            std::string s;
            for (uint32_t i = 0; i < MAX_STRING; ++i) {
                const uint8_t ch = c.mem.read8(a0 + i);
                if (ch == 0) break;
                s.push_back(static_cast<char>(ch));
            }
            out << s << std::flush;
            return Status::Ok;
        }
        case 5: {  // read_int
            std::string line;
            int32_t v = 0;
            if (!std::getline(in, line) || !parse_int(line, v)) {
                msg = "read_int: entrada no es un entero válido";
                return Status::Error;
            }
            c.set_reg(A0, static_cast<uint32_t>(v));
            return Status::Ok;
        }
        case 8: {  // read_string (como fgets: hasta a1-1 caracteres + '\0', incluye '\n')
            const uint32_t max = c.reg(A1);
            if (max == 0) return Status::Ok;
            std::string line;
            const bool got = static_cast<bool>(std::getline(in, line));
            if (got) line.push_back('\n');
            uint32_t k = 0;
            for (; k + 1 < max && k < line.size(); ++k)
                c.mem.write8(a0 + k, static_cast<uint8_t>(line[k]));
            c.mem.write8(a0 + k, 0);
            return Status::Ok;
        }
        case 9: {  // sbrk
            const uint32_t addr = *brk;
            *brk += (a0 + 3) & ~3u;  // mantiene alineación a palabra
            c.set_reg(A0, addr);
            return Status::Ok;
        }
        case 10:  // exit
            c.halt(0, "ecall exit (a7=10)");
            return Status::Halted;
        case 11:  // print_char
            out << static_cast<char>(a0 & 0xFF) << std::flush;
            return Status::Ok;
        case 12: {  // read_char
            const int ch = in.get();
            c.set_reg(A0, ch == EOF ? 0xFFFFFFFFu : static_cast<uint32_t>(ch & 0xFF));
            return Status::Ok;
        }
        case 17:  // exit2
            c.halt(static_cast<int32_t>(a0), "ecall exit2 (a7=17)");
            return Status::Halted;
        case 30: {  // time
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();
            const uint64_t u = static_cast<uint64_t>(ms);
            c.set_reg(A0, static_cast<uint32_t>(u));
            c.set_reg(A1, static_cast<uint32_t>(u >> 32));
            return Status::Ok;
        }
        case 32:  // sleep
            std::this_thread::sleep_for(std::chrono::milliseconds(a0));
            return Status::Ok;
        case 34: {  // print_hex
            char b[16];
            std::snprintf(b, sizeof b, "0x%08x", a0);
            out << b << std::flush;
            return Status::Ok;
        }
        case 35: {  // print_bin
            std::string s;
            for (int i = 31; i >= 0; --i) s.push_back(((a0 >> i) & 1) ? '1' : '0');
            out << s << std::flush;
            return Status::Ok;
        }
        case 36:  // print_unsigned
            out << a0 << std::flush;
            return Status::Ok;
        default:
            msg = "syscall desconocida o no soportada: a7=" + std::to_string(num);
            return Status::Error;
        }
    };
}
