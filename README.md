# StarrySky C2 Hello World

Bare-metal SYS_UART example built with xhive. The board's CP2102 console uses
115200 baud, 8 data bits, no parity, one stop bit, and no flow control. The
application prints `Hello World from xhive Vendor StarrySky C2!` once at startup,
then echoes each received byte unchanged. There is no prefix, newline conversion,
periodic output, timer delay, or software receive buffer in the echo loop.

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

The ELF and BIN are generated as `dist/c2_hello.elf` and `dist/c2_hello.bin`.
Link-map generation is currently disabled in `xmake.lua`.

## Program and Run

The physical mode switch shares both USB and Flash between HFP-LINK and the
SoC. The storage volume and the UART cannot be used at the same time.

1. Select HFP-LINK/program mode and mount the `YSYX-HFPLnk` volume.
2. Run `xmake flash --dry-run` to check the build and destination without writing.
3. Run `xmake flash`. It builds the firmware and copies it as `retrosoc_fw.bin`
   twice with a 0.5-second pause, synchronizes each pass, and displays `STATE.TXT`.
   This follows the official `ecos-flash` two-copy sequence but does not suppress
   copy errors. Multiple connected programmers require
   `--mount=/path/to/YSYX-HFPLnk`. This board-local task currently supports Linux.
4. Wait for programmer activity to stop, then switch to UART/run mode and reset
   the board. The storage volume disappears and CP2102 enumerates as a serial port.
5. Run `xmake monitor`, or `xmake monitor --port=/dev/ttyUSB0`. Exit with Ctrl+].

The monitor needs Python 3 and `pyserial`. On this machine pyserial is already
installed. Device permissions must allow the current user to open the serial
port. Reset after opening the monitor to see the startup greeting; echo also
works when the monitor connects later. Keep the terminal's local echo disabled
to avoid displaying both locally typed and board-echoed characters.

## Echo Check

After programming the new firmware, switch to UART/run mode and press RST.
Close any serial monitor before running the byte-for-byte test:

```sh
python3 tests/test_echo.py /dev/ttyUSB0
```

The test uses pyserial with an exclusive Linux serial-port open and sends text,
CR/LF, and all 256 byte values. It waits for each echoed byte before sending the
next and fails on a mismatch, timeout, or trailing data. This is a basic
bidirectional test, not a sustained-throughput qualification. Data sent during
startup is not covered; wait for the greeting or allow startup to finish first.

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

### Hello-Only Baseline

The following results apply to the previously tested periodic-Hello firmware,
not to the new echo loop:

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

### Echo Firmware

- Current firmware built and linked with xPack RISC-V GCC 15.2.0; BIN size:
  768 bytes. The revised startup greeting has not been programmed or tested
  on the board.
- Checked the host test script against a pseudo-terminal: correct echo passes;
  corrupted, missing, and trailing bytes fail. This does not exercise the SoC.
- Earlier programming attempts used a 764-byte echo image with the previous
  startup greeting. Single-copy attempts left the old periodic-Hello firmware
  running, and the echo test correctly failed. The
  two-copy attempt using `FIRMWARE.BIN` also left the old firmware running.
  Using the official filename (`retrosoc_fw.bin`) and two-copy sequence also
  failed to activate echo. A requested power cycle produced an observed CP2102
  disconnect/reconnect, but the test still received the old Hello output.
- Host-side target-file size and SHA-256 matched that 764-byte test BIN, but
  this is not independent Flash readback. Programming activation remains
  unresolved; hardware echo has not passed.
- No sustained-throughput or receive-overrun guarantees are made for SYS_UART.

The SDK regression check can be run from the xhive repository:

```sh
xmake l tests/riscv_toolset.lua \
  ~/.xhive/tools/riscv-none-elf-gcc-linux-x86_64/bin/riscv-none-elf-gcc
```

It cross-compiles and links RV32IMC, RV32IMC with LTO, RV64GC with the LP64D ABI,
and RV32IM with Zicsr, then checks BIN conversion and invalid XLEN rejection.
These are software checks, not additional board validations.
