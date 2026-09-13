// Instruction-level CPU RTL simulation with simplified C2 memory / SYS_UART.
// The UART model is not the C2 RTL and cannot reproduce hardware RX races.
#include "Vpicorv32.h"
#include "verilated.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        std::cerr << "usage: picorv32_sim firmware.bin bus-wait-cycles [probe-irq|probe-noirq]\n";
        return 2;
    }
    const std::string mode = argc == 4 ? argv[3] : "nano";
    unsigned wait_cycles = std::stoul(argv[2]);
    unsigned wait_left = 0;
    std::ifstream bin(argv[1], std::ios::binary);
    if (!bin) return 2;
    std::vector<uint8_t> flash((std::istreambuf_iterator<char>(bin)), {});
    flash.resize(16 * 1024 * 1024);
    std::vector<uint8_t> ram(128 * 1024, 0xa5);
    Vpicorv32 cpu;
    cpu.resetn = 0;
    cpu.irq = 0;
    cpu.mem_ready = 0;
    cpu.pcpi_wr = cpu.pcpi_ready = cpu.pcpi_wait = 0;
    cpu.pcpi_rd = 0;
    const std::string input = "help\ruptime\rlist_thread\r";
    size_t input_pos = 0;
    uint64_t next_input = 18000000;
    uint64_t next_tx = 0;
    uint32_t divisor = 0;
    std::string output;
    for (uint64_t cycle = 0; cycle < 72000000; ++cycle) {
        cpu.clk = 0;
        cpu.eval();
        if (cycle == 16) cpu.resetn = 1;
        if (cpu.mem_ready) {
            cpu.mem_ready = 0;
            wait_left = wait_cycles;
        } else if (cpu.mem_valid && wait_left) {
            --wait_left;
        } else if (cpu.mem_valid) {
            uint32_t addr = cpu.mem_addr & ~3u;
            uint8_t *ptr = nullptr;
            if (addr < flash.size()) ptr = flash.data() + addr;
            if (addr >= 0x30000000 && addr < 0x30020000)
                ptr = ram.data() + addr - 0x30000000;
            cpu.mem_ready = 1;
            cpu.mem_rdata = 0;
            if (ptr) {
                for (int i = 0; i < 4; ++i) {
                    if (cpu.mem_wstrb & (1 << i)) {
                        if (addr < flash.size()) throw std::runtime_error("Flash write");
                        ptr[i] = cpu.mem_wdata >> (i * 8);
                    }
                    cpu.mem_rdata |= uint32_t(ptr[i]) << (i * 8);
                }
            } else if (addr == 0x10001000) {
                if (cpu.mem_wstrb) divisor = cpu.mem_wdata;
                cpu.mem_rdata = divisor;
            } else if (addr == 0x10001004) {
                if (cpu.mem_wstrb) {
                    if (cycle < next_tx) {
                        cpu.mem_ready = 0; // A blocked bus write until TX is ready.
                    } else {
                        char ch = cpu.mem_wdata;
                        output += ch;
                        std::cout << ch << std::flush;
                        next_tx = cycle + divisor * 10;
                    }
                } else {
                    cpu.mem_rdata = 0xffffffff;
                    if (cycle >= next_input && input_pos < input.size()) {
                        cpu.mem_rdata = uint8_t(input[input_pos++]);
                        next_input = cycle + 720000; // Human-paced console input.
                    }
                }
            } else {
                std::cerr << "unmapped access: " << std::hex << addr << '\n';
                return 1;
            }
        }
        cpu.clk = 1;
        cpu.eval();
        if (cpu.trap) {
            std::cerr << "CPU trap at cycle " << cycle << '\n';
            if (mode == "probe-noirq" &&
                output == "C2 IRQ probe: before maskirq\r\n") {
                std::cerr << "IRQ-disabled probe PASS (expected trap)\n";
                return 0;
            }
            return 1;
        }
        if (mode == "probe-irq" && output.find(
                "C2 IRQ probe: timer supported (IRQs remain masked)\r\n")
                != std::string::npos) {
            std::cerr << "IRQ-enabled probe PASS\n";
            return 0;
        }
    }
    bool ok = divisor == 625 && output.find("preempt: PASS") != std::string::npos
        && output.find("irq-mask: PASS") != std::string::npos
        && output.find("uptime") != std::string::npos
        && output.find("rate=1000Hz") != std::string::npos
        && output.find("tshell") != std::string::npos
        && output.find("idle") != std::string::npos
        && output.find("FAIL") == std::string::npos
        && output.find("assert") == std::string::npos;
    std::cerr << (ok ? "RTL smoke test PASS\n" : "RTL smoke test FAIL\n");
    return ok ? 0 : 1;
}
