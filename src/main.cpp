// main.cpp — REPL de terminal del simulador RV32I.
//
// Uso: rv32sim [opciones] programa.bin
// Los comandos se leen de stdin línea por línea, por lo que también funciona
// en modo no interactivo:  printf 'run\nregs a0\nexit\n' | rv32sim prog.bin
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "cpu.hpp"
#include "decode.hpp"
#include "loader.hpp"

namespace {

constexpr uint64_t DEFAULT_RUN_LIMIT = 10'000'000;  // límite de seguridad de `run`
constexpr uint32_t MAX_MEM_DUMP = 64 * 1024;          // bytes máx. por comando `mem`

std::string hex32(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08x", v);
    return b;
}

// Acepta "0x..." (hex) o decimal. Rechaza valores fuera de 32 bits.
bool parse_u32(const std::string& s, uint32_t& out) {
    if (s.empty()) return false;
    uint64_t v = 0;
    std::size_t i = 0;
    int base = 10;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        i = 2;
    }
    for (; i < s.size(); ++i) {
        const char c = s[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        v = v * base + d;
        if (v > 0xFFFFFFFFull) return false;
    }
    out = static_cast<uint32_t>(v);
    return true;
}

bool parse_u64(const std::string& s, uint64_t& out) {
    if (s.empty() || s.size() > 18) return false;
    uint64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + static_cast<uint64_t>(c - '0');
    }
    out = v;
    return true;
}

struct Options {
    std::string path;
    uint32_t sp = DEFAULT_SP;
    uint64_t limit = DEFAULT_RUN_LIMIT;
    bool echo = false;
};

class Repl {
public:
    explicit Repl(Options opt) : opt_(std::move(opt)) {}

    bool load() {
        LoadInfo info;
        std::string err;
        if (!load_program(opt_.path, cpu_, info, err)) {
            std::cout << "Error: " << err << "\n";
            return false;
        }
        cpu_.reset(info.entry, opt_.sp);
        std::cout << "\"" << opt_.path << "\" cargado a memoria. (" << info.description
                  << ", pc = " << hex32(info.entry) << ")\n";
        return true;
    }

    int loop() {
        const bool interactive = isatty(STDIN_FILENO);
        const bool show_prompt = interactive || opt_.echo;
        std::string line;
        while (true) {
            if (show_prompt) std::cout << "> " << std::flush;
            if (!std::getline(std::cin, line)) {
                if (show_prompt) std::cout << "\n";
                break;
            }
            if (opt_.echo && !interactive) std::cout << line << "\n";
            if (!execute(line)) break;
            std::cout << std::flush;
        }
        return 0;
    }

private:
    Cpu cpu_;
    Options opt_;
    std::set<uint32_t> breakpoints_;

    // Devuelve false cuando hay que salir.
    bool execute(const std::string& line) {
        std::istringstream ss(line);
        std::vector<std::string> args;
        for (std::string t; ss >> t;) args.push_back(t);
        if (args.empty() || args[0][0] == '#') return true;  // vacío o comentario

        const std::string cmd = args[0];
        args.erase(args.begin());

        if (cmd == "exit" || cmd == "quit" || cmd == "q") return false;
        if (cmd == "help" || cmd == "h" || cmd == "?") help();
        else if (cmd == "step" || cmd == "s") cmd_step(args);
        else if (cmd == "run" || cmd == "r" || cmd == "continue" || cmd == "c") cmd_run(args);
        else if (cmd == "pc") cmd_pc(args);
        else if (cmd == "regs" || cmd == "reg") cmd_regs(args);
        else if (cmd == "mem") cmd_mem(args);
        else if (cmd == "memw") cmd_memw(args);
        else if (cmd == "break" || cmd == "b") cmd_break(args);
        else if (cmd == "delete" || cmd == "d") cmd_delete(args);
        else if (cmd == "breaks") cmd_breaks();
        else if (cmd == "reset") load();
        else std::cout << "Error: comando desconocido \"" << cmd << "\". Escribe 'help'.\n";
        return true;
    }

    void help() {
        std::cout <<
            "Comandos:\n"
            "  step [n]          ejecuta 1 (o n) instrucciones\n"
            "  run [max]         ejecuta hasta fin, error, breakpoint o bucle infinito\n"
            "                    (límite por defecto: " << opt_.limit << " instrucciones)\n"
            "  pc                muestra el PC\n"
            "  regs [r ...]      muestra todos los registros o solo los pedidos (x5, t0, sp...)\n"
            "  mem A [B]         bytes de A a B inclusive (por defecto 16 bytes)\n"
            "  memw A [n]        n palabras de 32 bits desde A (por defecto 1)\n"
            "  break A           pone un breakpoint en A\n"
            "  delete A          quita el breakpoint en A ('delete all' quita todos)\n"
            "  breaks            lista los breakpoints\n"
            "  reset             recarga el programa y reinicia el estado\n"
            "  help              esta ayuda\n"
            "  exit              sale\n"
            "Las direcciones y números aceptan hex (0x...) o decimal.\n";
    }

    // Imprime el resultado de una detención (fin o error).
    void report_stop(Status s) {
        if (s == Status::Halted) {
            std::cout << "Programa terminado: " << cpu_.message()
                      << " (código de salida " << cpu_.exit_code << ")\n";
        } else if (s == Status::Error) {
            std::cout << cpu_.message() << "\n";
        }
    }

    bool check_halted() {
        if (cpu_.halted()) {
            std::cout << "El programa ya terminó (" << cpu_.message()
                      << "). Usa 'reset' para reiniciar.\n";
            return true;
        }
        return false;
    }

    void cmd_step(const std::vector<std::string>& a) {
        uint64_t n = 1;
        if (a.size() > 1 || (a.size() == 1 && (!parse_u64(a[0], n) || n == 0))) {
            std::cout << "Error: uso: step [n]  (n entero positivo)\n";
            return;
        }
        if (check_halted()) return;
        for (uint64_t i = 0; i < n; ++i) {
            const uint32_t pc = cpu_.pc;
            const uint32_t inst = cpu_.mem.read32(pc);
            const Status s = cpu_.step();
            if (s == Status::Error) {
                report_stop(s);
                return;
            }
            std::cout << "Ejecutando instrucción. " << hex32(pc) << " : " << hex32(inst) << "\n";
            if (s == Status::Halted) {
                report_stop(s);
                return;
            }
        }
    }

    void cmd_run(const std::vector<std::string>& a) {
        uint64_t limit = opt_.limit;
        if (a.size() > 1 || (a.size() == 1 && (!parse_u64(a[0], limit) || limit == 0))) {
            std::cout << "Error: uso: run [max_instrucciones]\n";
            return;
        }
        if (check_halted()) return;

        uint64_t count = 0;
        bool stopped = false;
        while (count < limit) {
            // Un breakpoint detiene ANTES de ejecutar la instrucción; si ya
            // estamos parados sobre él, la primera instrucción sí se ejecuta.
            if (count > 0 && breakpoints_.count(cpu_.pc)) {
                std::cout << "Breakpoint en " << hex32(cpu_.pc) << ".\n";
                stopped = true;
                break;
            }
            const uint32_t old_pc = cpu_.pc;
            const Status s = cpu_.step();
            if (s == Status::Error) {
                report_stop(s);
                stopped = true;
                break;
            }
            ++count;
            if (s == Status::Halted) {
                report_stop(s);
                stopped = true;
                break;
            }
            // Los programas de CPUlator terminan con "j ." (salto a sí mismo):
            // eso es un bucle infinito trivial y se trata como fin del programa.
            if (cpu_.pc == old_pc) {
                std::cout << "Bucle infinito detectado en " << hex32(old_pc)
                          << " (la instrucción salta a sí misma). Ejecución detenida.\n";
                stopped = true;
                break;
            }
        }
        if (!stopped)
            std::cout << "Límite de " << limit << " instrucciones alcanzado; pc = "
                      << hex32(cpu_.pc) << ". Usa 'run' de nuevo para continuar.\n";
        std::cout << "Instrucciones ejecutadas: " << count << "\n";
    }

    void cmd_pc(const std::vector<std::string>& a) {
        if (!a.empty()) {
            std::cout << "Error: uso: pc\n";
            return;
        }
        std::cout << "pc = " << hex32(cpu_.pc) << "\n";
    }

    void cmd_regs(const std::vector<std::string>& a) {
        if (a.empty()) {
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 4; ++col) {
                    const int r = row + col * 8;
                    char b[48];
                    std::snprintf(b, sizeof b, "x%-2d %-4s = 0x%08x", r, rv::ABI_NAMES[r],
                                  cpu_.reg(static_cast<unsigned>(r)));
                    std::cout << b << (col < 3 ? "   " : "\n");
                }
            }
            std::cout << "pc       = " << hex32(cpu_.pc) << "\n";
            return;
        }
        for (const auto& name : a) {
            if (name == "pc") {
                std::cout << "pc = " << hex32(cpu_.pc) << "\n";
                continue;
            }
            const int r = rv::parse_reg(name);
            if (r < 0) {
                std::cout << "Error: registro desconocido \"" << name << "\"\n";
                continue;
            }
            const uint32_t v = cpu_.reg(static_cast<unsigned>(r));
            if (name[0] == 'x')
                std::cout << name << " = " << hex32(v) << "\n";
            else
                std::cout << name << " (x" << r << ") = " << hex32(v) << "\n";
        }
    }

    void cmd_mem(const std::vector<std::string>& a) {
        uint32_t from = 0, to = 0;
        if (a.empty() || a.size() > 2 || !parse_u32(a[0], from) ||
            (a.size() == 2 && !parse_u32(a[1], to))) {
            std::cout << "Error: uso: mem A [B]  (direcciones hex 0x... o decimal)\n";
            return;
        }
        if (a.size() == 1) to = (from > 0xFFFFFFFFu - 15) ? 0xFFFFFFFFu : from + 15;
        if (to < from) {
            std::cout << "Error: la dirección final es menor que la inicial\n";
            return;
        }
        const uint64_t len = static_cast<uint64_t>(to) - from + 1;
        if (len > MAX_MEM_DUMP) {
            std::cout << "Error: rango demasiado grande (máximo " << MAX_MEM_DUMP << " bytes)\n";
            return;
        }
        char b[64];
        std::snprintf(b, sizeof b, "Memoria (0x%x-0x%x):", from, to);
        std::cout << b;
        if (len <= 16) {  // una sola línea, como en el enunciado
            for (uint64_t i = 0; i < len; ++i) {
                std::snprintf(b, sizeof b, " %02X", cpu_.mem.read8(from + static_cast<uint32_t>(i)));
                std::cout << b;
            }
            std::cout << "\n";
            return;
        }
        // Rangos largos: filas de 16 bytes con su dirección.
        for (uint64_t i = 0; i < len; ++i) {
            const uint32_t addr = from + static_cast<uint32_t>(i);
            if (i % 16 == 0) {
                std::snprintf(b, sizeof b, "\n  0x%08x:", addr);
                std::cout << b;
            }
            std::snprintf(b, sizeof b, " %02X", cpu_.mem.read8(addr));
            std::cout << b;
        }
        std::cout << "\n";
    }

    void cmd_memw(const std::vector<std::string>& a) {
        uint32_t addr = 0, n = 1;
        if (a.empty() || a.size() > 2 || !parse_u32(a[0], addr) ||
            (a.size() == 2 && (!parse_u32(a[1], n) || n == 0 || n > 4096))) {
            std::cout << "Error: uso: memw A [n]  (1 <= n <= 4096)\n";
            return;
        }
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t at = addr + 4 * i;
            const uint32_t v = cpu_.mem.read32(at);
            char b[80];
            std::snprintf(b, sizeof b, "0x%08x: 0x%08x  (%d)", at, v, static_cast<int32_t>(v));
            std::cout << b << "\n";
        }
    }

    void cmd_break(const std::vector<std::string>& a) {
        uint32_t addr = 0;
        if (a.size() != 1 || !parse_u32(a[0], addr)) {
            std::cout << "Error: uso: break A\n";
            return;
        }
        breakpoints_.insert(addr);
        std::cout << "Breakpoint puesto en " << hex32(addr) << ".\n";
    }

    void cmd_delete(const std::vector<std::string>& a) {
        if (a.size() == 1 && a[0] == "all") {
            breakpoints_.clear();
            std::cout << "Todos los breakpoints eliminados.\n";
            return;
        }
        uint32_t addr = 0;
        if (a.size() != 1 || !parse_u32(a[0], addr)) {
            std::cout << "Error: uso: delete A | delete all\n";
            return;
        }
        if (breakpoints_.erase(addr))
            std::cout << "Breakpoint en " << hex32(addr) << " eliminado.\n";
        else
            std::cout << "Error: no hay breakpoint en " << hex32(addr) << ".\n";
    }

    void cmd_breaks() {
        if (breakpoints_.empty()) {
            std::cout << "No hay breakpoints.\n";
            return;
        }
        for (uint32_t b : breakpoints_) std::cout << "  " << hex32(b) << "\n";
    }
};

void usage(const char* argv0) {
    std::cerr << "Uso: " << argv0 << " [opciones] programa.bin\n"
              << "Opciones:\n"
              << "  --sp VALOR      valor inicial de sp (por defecto " << hex32(DEFAULT_SP) << ")\n"
              << "  --limit N       límite de instrucciones de 'run' (por defecto "
              << DEFAULT_RUN_LIMIT << ")\n"
              << "  --echo          muestra prompt y comandos aunque stdin no sea una terminal\n"
              << "  -h, --help      esta ayuda\n";
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need_value = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << flag << " necesita un valor\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--help") {
            usage(argv[0]);
            return 0;
        } else if (a == "--sp") {
            if (!parse_u32(need_value("--sp"), opt.sp)) {
                std::cerr << "Error: valor inválido para --sp\n";
                return 2;
            }
        } else if (a == "--limit") {
            if (!parse_u64(need_value("--limit"), opt.limit) || opt.limit == 0) {
                std::cerr << "Error: valor inválido para --limit\n";
                return 2;
            }
        } else if (a == "--echo") {
            opt.echo = true;
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "Error: opción desconocida " << a << "\n";
            usage(argv[0]);
            return 2;
        } else if (opt.path.empty()) {
            opt.path = a;
        } else {
            std::cerr << "Error: solo se acepta un programa\n";
            return 2;
        }
    }
    if (opt.path.empty()) {
        usage(argv[0]);
        return 2;
    }

    Repl repl(opt);
    if (!repl.load()) return 1;
    return repl.loop();
}
