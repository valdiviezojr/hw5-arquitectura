#include "gdbstub.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <set>
#include <string>

namespace {

// Descripción de registros que se envía a GDB (qXfer:features:read).
// Orden: x0..x31 (regnum 0..31) y pc (regnum 32), igual que el paquete 'g'.
std::string target_xml() {
    static const char* names[32] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "fp", "s1", "a0",
        "a1",   "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5",
        "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
    std::string x =
        "<?xml version=\"1.0\"?>\n<!DOCTYPE target SYSTEM \"gdb-target.dtd\">\n"
        "<target version=\"1.0\">\n<architecture>riscv:rv32</architecture>\n"
        "<feature name=\"org.gnu.gdb.riscv.cpu\">\n";
    for (int i = 0; i < 32; ++i) {
        const char* type = (i == 1) ? "code_ptr" : (i == 2 || i == 8) ? "data_ptr" : "int";
        x += "<reg name=\"" + std::string(names[i]) + "\" bitsize=\"32\" type=\"" + type +
             "\" regnum=\"" + std::to_string(i) + "\"/>\n";
    }
    x += "<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\" regnum=\"32\"/>\n</feature>\n</target>\n";
    return x;
}

const char HEX[] = "0123456789abcdef";

int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Parsea un número hexadecimal desde s[pos]; avanza pos. Devuelve false si no hay dígitos.
bool parse_hex(const std::string& s, std::size_t& pos, uint32_t& out) {
    uint64_t v = 0;
    std::size_t start = pos;
    while (pos < s.size() && hexval(s[pos]) >= 0) {
        v = (v << 4) | static_cast<uint64_t>(hexval(s[pos]));
        if (v > 0xFFFFFFFFull) return false;
        ++pos;
    }
    out = static_cast<uint32_t>(v);
    return pos > start;
}

// Registro de 32 bits en orden de bytes del target (little-endian), como pide RSP.
std::string reg_hex(uint32_t v) {
    std::string s;
    for (int i = 0; i < 4; ++i) {
        const uint8_t b = static_cast<uint8_t>(v >> (8 * i));
        s.push_back(HEX[b >> 4]);
        s.push_back(HEX[b & 15]);
    }
    return s;
}

bool parse_reg_hex(const std::string& s, std::size_t pos, uint32_t& out) {
    if (pos + 8 > s.size()) return false;
    out = 0;
    for (int i = 0; i < 4; ++i) {
        const int hi = hexval(s[pos + 2 * i]), lo = hexval(s[pos + 2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out |= static_cast<uint32_t>((hi << 4) | lo) << (8 * i);
    }
    return true;
}

class Session {
public:
    Session(int fd, Cpu& cpu) : fd_(fd), cpu_(cpu) {}

    void serve() {
        std::string pkt;
        while (read_packet(pkt)) {
            if (!handle(pkt)) break;
        }
    }

private:
    int fd_;
    Cpu& cpu_;
    std::set<uint32_t> breakpoints_;
    std::string rxbuf_;

    // ---------------------------------------------------------- transporte --
    int read_byte(int timeout_ms = -1) {
        if (!rxbuf_.empty()) {
            const int c = static_cast<unsigned char>(rxbuf_[0]);
            rxbuf_.erase(0, 1);
            return c;
        }
        pollfd p{fd_, POLLIN, 0};
        const int r = ::poll(&p, 1, timeout_ms);
        if (r == 0) return -2;  // timeout
        if (r < 0) return -1;
        char buf[4096];
        const ssize_t n = ::recv(fd_, buf, sizeof buf, 0);
        if (n <= 0) return -1;
        rxbuf_.assign(buf + 1, static_cast<std::size_t>(n - 1));
        return static_cast<unsigned char>(buf[0]);
    }

    bool write_all(const std::string& s) {
        std::size_t off = 0;
        while (off < s.size()) {
            const ssize_t n = ::send(fd_, s.data() + off, s.size() - off, MSG_NOSIGNAL);
            if (n <= 0) return false;
            off += static_cast<std::size_t>(n);
        }
        return true;
    }

    // Formato: $<datos>#<checksum de 2 dígitos hex>. Se espera '+' como ack.
    bool send_packet(const std::string& data) {
        uint8_t sum = 0;
        for (char c : data) sum = static_cast<uint8_t>(sum + static_cast<uint8_t>(c));
        std::string frame = "$" + data + "#";
        frame.push_back(HEX[sum >> 4]);
        frame.push_back(HEX[sum & 15]);
        for (int tries = 0; tries < 5; ++tries) {
            if (!write_all(frame)) return false;
            const int c = read_byte(5000);
            if (c == '+') return true;
            if (c == -1) return false;
            // '-' o timeout: reenviar
        }
        return false;
    }

    bool read_packet(std::string& out) {
        while (true) {
            int c = read_byte();
            if (c < 0) return false;
            if (c != '$') continue;  // ignora '+', '-' y 0x03 fuera de ejecución
            std::string data;
            while (true) {
                c = read_byte();
                if (c < 0) return false;
                if (c == '#') break;
                data.push_back(static_cast<char>(c));
            }
            const int h = read_byte(), l = read_byte();
            if (h < 0 || l < 0) return false;
            uint8_t sum = 0;
            for (char ch : data) sum = static_cast<uint8_t>(sum + static_cast<uint8_t>(ch));
            if (hexval(static_cast<char>(h)) * 16 + hexval(static_cast<char>(l)) != sum) {
                write_all("-");
                continue;
            }
            write_all("+");
            out = data;
            return true;
        }
    }

    // ¿Llegó un Ctrl-C (0x03) de GDB mientras corremos?
    // Solo "mira" el siguiente byte: si no es 0x03 queda en el buffer.
    bool interrupted() {
        if (rxbuf_.empty()) {
            pollfd p{fd_, POLLIN, 0};
            if (::poll(&p, 1, 0) <= 0) return false;
            char buf[4096];
            const ssize_t n = ::recv(fd_, buf, sizeof buf, 0);
            if (n <= 0) return true;  // GDB se desconectó: detener
            rxbuf_.assign(buf, static_cast<std::size_t>(n));
        }
        if (rxbuf_[0] == 0x03) {
            rxbuf_.erase(0, 1);
            return true;
        }
        return false;
    }

    // ------------------------------------------------------------ comandos --
    // Respuesta de parada tras step/continue.
    std::string stop_reply(Status s) {
        if (s == Status::Halted) {
            std::cout << "[gdb] Programa terminado: " << cpu_.message() << "\n" << std::flush;
            char b[8];
            std::snprintf(b, sizeof b, "W%02x", static_cast<unsigned>(cpu_.exit_code) & 0xFF);
            return b;
        }
        if (s == Status::Error) {
            std::cout << "[gdb] " << cpu_.message() << "\n" << std::flush;
            return "S04";  // SIGILL
        }
        return "S05";  // SIGTRAP
    }

    Status do_step() { return cpu_.halted() ? Status::Halted : cpu_.step(); }

    std::string do_continue() {
        bool first = true;
        uint64_t count = 0;
        while (true) {
            if (!first && breakpoints_.count(cpu_.pc)) return "S05";
            first = false;
            const uint32_t old_pc = cpu_.pc;
            const Status s = do_step();
            if (s != Status::Ok) return stop_reply(s);
            if (cpu_.pc == old_pc) {
                std::cout << "[gdb] Bucle infinito detectado en 0x" << std::hex << old_pc
                          << std::dec << "; se detiene.\n" << std::flush;
                return "S05";
            }
            if ((++count & 0xFFF) == 0 && interrupted()) return "S02";  // SIGINT
        }
    }

    // Devuelve false para terminar la sesión.
    bool handle(const std::string& p) {
        if (p.empty()) return send_packet("");
        const char cmd = p[0];

        switch (cmd) {
        case '?':
            return send_packet(cpu_.halted() ? "W00" : "S05");

        case 'g': {
            std::string r;
            for (unsigned i = 0; i < 32; ++i) r += reg_hex(cpu_.reg(i));
            r += reg_hex(cpu_.pc);
            return send_packet(r);
        }
        case 'G': {
            if (p.size() < 1 + 33 * 8) return send_packet("E01");
            for (unsigned i = 0; i <= 32; ++i) {
                uint32_t v;
                if (!parse_reg_hex(p, 1 + 8 * i, v)) return send_packet("E01");
                if (i == 32) cpu_.pc = v; else cpu_.set_reg(i, v);
            }
            return send_packet("OK");
        }
        case 'p': {
            std::size_t pos = 1;
            uint32_t n;
            if (!parse_hex(p, pos, n) || n > 32) return send_packet("E01");
            return send_packet(reg_hex(n == 32 ? cpu_.pc : cpu_.reg(n)));
        }
        case 'P': {
            std::size_t pos = 1;
            uint32_t n, v;
            if (!parse_hex(p, pos, n) || n > 32 || pos >= p.size() || p[pos] != '=' ||
                !parse_reg_hex(p, pos + 1, v))
                return send_packet("E01");
            if (n == 32) cpu_.pc = v; else cpu_.set_reg(n, v);
            return send_packet("OK");
        }
        case 'm': {
            std::size_t pos = 1;
            uint32_t addr, len;
            if (!parse_hex(p, pos, addr) || pos >= p.size() || p[pos++] != ',' ||
                !parse_hex(p, pos, len))
                return send_packet("E01");
            if (len > 4096) len = 4096;
            std::string r;
            for (uint32_t i = 0; i < len; ++i) {
                const uint8_t b = cpu_.mem.read8(addr + i);
                r.push_back(HEX[b >> 4]);
                r.push_back(HEX[b & 15]);
            }
            return send_packet(r);
        }
        case 'M': {
            std::size_t pos = 1;
            uint32_t addr, len;
            if (!parse_hex(p, pos, addr) || pos >= p.size() || p[pos++] != ',' ||
                !parse_hex(p, pos, len) || pos >= p.size() || p[pos++] != ':' ||
                p.size() - pos < 2ull * len)
                return send_packet("E01");
            for (uint32_t i = 0; i < len; ++i) {
                const int hi = hexval(p[pos + 2 * i]), lo = hexval(p[pos + 2 * i + 1]);
                if (hi < 0 || lo < 0) return send_packet("E01");
                cpu_.mem.write8(addr + i, static_cast<uint8_t>((hi << 4) | lo));
            }
            return send_packet("OK");
        }
        case 's':
        case 'c': {
            if (p.size() > 1) {  // dirección opcional desde donde continuar
                std::size_t pos = 1;
                uint32_t addr;
                if (parse_hex(p, pos, addr)) cpu_.pc = addr;
            }
            if (cpu_.halted()) return send_packet("W00");
            return send_packet(cmd == 's' ? stop_reply(do_step()) : do_continue());
        }
        case 'Z':
        case 'z': {
            // Z0 = breakpoint software, Z1 = hardware: ambos se tratan igual.
            if (p.size() < 3 || (p[1] != '0' && p[1] != '1') || p[2] != ',')
                return send_packet("");  // watchpoints no soportados
            std::size_t pos = 3;
            uint32_t addr;
            if (!parse_hex(p, pos, addr)) return send_packet("E01");
            if (cmd == 'Z') breakpoints_.insert(addr); else breakpoints_.erase(addr);
            return send_packet("OK");
        }
        case 'H':
            return send_packet("OK");
        case 'k':
            return false;
        case 'D':
            send_packet("OK");
            return false;
        case 'q':
            return handle_query(p);
        default:
            return send_packet("");  // vacío = no soportado
        }
    }

    bool handle_query(const std::string& p) {
        if (p.rfind("qSupported", 0) == 0)
            return send_packet("PacketSize=4000;qXfer:features:read+;swbreak+");
        if (p == "qAttached") return send_packet("1");
        if (p == "qC") return send_packet("QC1");
        if (p == "qfThreadInfo") return send_packet("m1");
        if (p == "qsThreadInfo") return send_packet("l");
        const std::string xfer = "qXfer:features:read:target.xml:";
        if (p.rfind(xfer, 0) == 0) {
            std::size_t pos = xfer.size();
            uint32_t off, len;
            if (!parse_hex(p, pos, off) || pos >= p.size() || p[pos++] != ',' ||
                !parse_hex(p, pos, len))
                return send_packet("E01");
            const std::string xml = target_xml();
            if (off >= xml.size()) return send_packet("l");
            const std::string chunk = xml.substr(off, len);
            return send_packet((off + chunk.size() >= xml.size() ? "l" : "m") + chunk);
        }
        return send_packet("");
    }
};

}  // namespace

int run_gdb_server(Cpu& cpu, uint16_t port) {
    const int srv = ::socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        std::perror("socket");
        return 1;
    }
    int one = 1;
    ::setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // solo conexiones locales
    if (::bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0 || ::listen(srv, 1) < 0) {
        std::perror("bind/listen");
        ::close(srv);
        return 1;
    }
    std::cout << "Esperando a GDB en 127.0.0.1:" << port
              << "  (gdb -ex \"set architecture riscv:rv32\" -ex \"target remote :" << port
              << "\")\n" << std::flush;
    const int fd = ::accept(srv, nullptr, nullptr);
    ::close(srv);
    if (fd < 0) {
        std::perror("accept");
        return 1;
    }
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    std::cout << "GDB conectado.\n" << std::flush;
    Session(fd, cpu).serve();
    ::close(fd);
    std::cout << "GDB desconectado.\n";
    return 0;
}
