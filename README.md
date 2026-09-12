# StarrySky C2 Hello World

Bare-metal SYS_UART example built with xhive. The board's CP2102 console uses
115200 baud, 8 data bits, no parity, one stop bit, and no flow control. The
application prints `Hello World from xhive / StarrySky C2!` about once per second.

## Build

```sh
export XHIVE_SDK_PATH=/home/zmmfly/prjs/xhive
xmake f -y
xmake build c2_hello
```

The checked-in `.config` selects RV32IMC/ILP32, the xPack RISC-V GCC toolchain,
and xhive's default startup/linker templates. Flash starts at `0x00000000`
(16 MiB); internal SRAM starts at `0x30000000` (128 KiB). The 1 KiB stack stays
in SRAM and is aligned to 16 bytes. External RAM and interrupts are not enabled.
`CONFIG_STARRYSKY_C2_CLOCK_HZ=72000000` describes the fitted oscillator; changing
the configuration does not change the hardware clock.

The ELF and BIN are in the target directory reported by Xmake, currently
`build/cross/x86_64/release/`. The `x86_64` directory component comes from Xmake's
host configuration, not the firmware ISA. A link map is at `build/c2_hello.map`.

## Program and Run

The physical mode switch shares both USB and Flash between HFP-LINK and the
SoC. The storage volume and the UART cannot be used at the same time.

1. Select HFP-LINK/program mode and mount the `YSYX-HFPLnk` volume.
2. Run `xmake flash --dry-run` to check the build and destination without writing.
3. Run `xmake flash`. It builds the firmware, writes `FIRMWARE.BIN`, flushes the
   filesystem, and displays `STATE.TXT`. Multiple connected programmers require
   `--mount=/path/to/YSYX-HFPLnk`. This board-local task currently supports Linux.
4. Wait for programmer activity to stop, then switch to UART/run mode and reset
   the board. The storage volume disappears and CP2102 enumerates as a serial port.
5. Run `xmake monitor`, or `xmake monitor --port=/dev/ttyUSB0`. Exit with Ctrl+].

The monitor needs Python 3 and `pyserial`. On this machine pyserial is already
installed. Device permissions must allow the current user to open the serial
port. A late connection is fine because the greeting repeats.

Copy completion and `sync` confirm host-side writes, not independent Flash
readback. This HFP-LINK firmware initially exposes only a default instruction in
`STATE.TXT`. Successful serial output after reset is the runtime check.

## Debugging Findings

- The attached HFP-LINK (`1a86:5722`) exposes one USB Mass Storage / SCSI / Bulk-Only
  interface. It does not expose a CMSIS-DAP, J-Link, or USB serial debug interface
  in program mode. Switching to UART provides a console, not a GDB server.
- HFP means High-speed QSPI-Flash Programmer. Its CH32V103 firmware-update/debug
  interface belongs to the programmer MCU, not to the C2 application processor.
- The published C2 board documentation and SDK provide no C2 JTAG/OpenOCD target
  configuration or documented hardware-debug connection. PicoRV32 itself does
  not provide a standard RISC-V Debug Module/JTAG transport. These facts do not
  establish whether any unpublished SoC integration added one; they do mean that
  a usable hardware GDB connection cannot be assumed for this board.
- For now use SYS_UART logs and the ELF/map/disassembly for diagnosis. A UART GDB
  stub would require a separate implementation and a verified C2 trap/IRQ model;
  it would not automatically provide hardware breakpoints or unrestricted stepping.

References:

- [Official C2 Pi board documentation](https://embedded.openecos.com/zh-cn/latest/page/brd/starry-sky-c/v2.0_pi/)
- [Official SDK quickstart](https://embedded.openecos.com/zh-cn/latest/page/sdk/common/quickstart/)
- [HFP-LINK design and operation](https://ysyx.oscc.cc/chip/board/official/boards/board-1/)
- [PicoRV32 source and interfaces](https://github.com/YosysHQ/picorv32/blob/main/picorv32.v)

The C2 page contains inconsistent ISA descriptions (RV32IMC in the introduction,
RV32IMAC in a table). The PicoRV32 implementation has no A extension; this project
uses RV32IMC, without atomics. Official SDK examples use the RV32IM subset.

## Validation

- Built and linked with xPack RISC-V GCC 15.2.0; BIN size: 3216 bytes.
- ELF inspection: RV32IMC, soft-float ILP32, reset entry `0x00000000`, initial
  stack pointer `0x30000400`, and no A extension in the linked ISA attributes.
- `xmake flash --dry-run` identified the mounted programmer; an unrelated
  `--mount=/tmp` was rejected. The BIN was copied to the connected HFP-LINK and
  the filesystem was synchronized. `STATE.TXT` retained its default instruction.
- Hardware runtime verified after switching to UART/run mode and pressing RST:
  CP2102 (`10c4:ea60`) appeared as `/dev/ttyUSB0`. An 8-second capture at 115200
  8N1 received 320 bytes containing 8 complete greetings, each terminated by
  CRLF, with no unexpected bytes. The serial port was closed after verification.

The SDK regression check can be run from the xhive repository:

```sh
xmake l tests/riscv_toolset.lua \
  ~/.xhive/tools/riscv-none-elf-gcc-linux-x86_64/bin/riscv-none-elf-gcc
```

It cross-compiles and links RV32IMC, RV32IMC with LTO, RV64GC with the LP64D ABI,
and RV32IM with Zicsr, then checks BIN conversion and invalid XLEN rejection.
These are software checks, not additional board validations.
