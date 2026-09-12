# StarrySky C2 Hello and Echo

English | [Chinese (zh_CN)](README.zh_CN.md)

A bare-metal SYS_UART example for StarrySky C2, built with xhive. It prints one
startup greeting, then echoes received bytes unchanged.

## Contents

- [Overview](#overview)
- [Requirements](#requirements)
- [Build](#build)
- [Program and Run](#program-and-run)
- [Echo Test](#echo-test)
- [Validation and Limitations](#validation-and-limitations)
- [References](#references)

## Overview

The board's CP2102 USB-to-UART bridge connects to SYS_UART. The application uses
polling, with no RTOS, interrupts, timer delays, or software receive queue.
After the startup greeting, echo data has no added prefix or newline conversion.
Binary values, including `0x00` and `0xFF`, are handled as bytes.

The echo loop runs in internal SRAM (`.ramfunc`), with LTO enabled to inline
the UART calls. This avoids Flash XIP fetches on the receive/transmit hot path
without adding interrupts or queues. It is a candidate mitigation for the
burst-loss issue described below, pending a new hardware test.

Startup output:

```text
Hello World [SRAM/LTO v2] from xhive vendor: opencos / StarrySky C2!
```

The `[SRAM/LTO v2]` marker distinguishes this diagnostic build from the earlier
firmware. Open the serial monitor before resetting to capture the one-time line.

The checked-in [.config](.config) selects:

| Setting | Value |
| --- | --- |
| Target | StarrySky C2, RV32IMC / ILP32 |
| UART | SYS_UART, 115200 baud, 8N1, no flow control |
| Board oscillator | 72 MHz |
| Flash | 16 MiB at `0x00000000` |
| Internal SRAM | 128 KiB at `0x30000000` |
| Stack | 1 KiB in internal SRAM, aligned to 16 bytes |
| External RAM | Disabled |
| Execution | `CONFIG_ENABLE_EXEC_IN_RAM=y`, `CONFIG_COMPILER_ENABLE_LTO=y` |
| Startup and linker script | xhive default templates |

`CONFIG_STARRYSKY_C2_CLOCK_HZ` describes the actual oscillator frequency; changing
it does not reconfigure the hardware clock.

## Requirements

**Current host environment: Linux / Ubuntu 26.04.** Implementation, firmware
builds, and host-side software checks were performed in this environment.
The C2 firmware itself remains bare-metal; Ubuntu runs on the development host.

- An [xhive SDK](https://github.com/Zmmfly/xhive) checkout with its RISC-V GCC
  toolchain and configuration tools available.
- Xmake and the SDK's Python configuration dependencies, including `kconfiglib`.
- Python 3 with `pyserial` for the serial monitor and echo test.
- Linux with `lsblk`, `cp`, and `sync` for the board-local programming task.
- Permission to access the mounted programmer volume and serial device.

### Operating System Scope

| Workflow | Constraint |
| --- | --- |
| Build | Verified on Ubuntu 26.04; examples use a POSIX shell and `realpath` |
| Programming helper | Explicitly restricted to Linux; depends on `lsblk`, `cp`, and `sync -f` |
| Serial monitor and echo test | Tested on Linux; the test also accepts Windows COM ports and only requests exclusive-open support on POSIX |
| Windows drag-and-drop | User confirmed a firmware update through HFP-LINK; this does not validate a Windows build or the Linux helper |
| Other environments | Other Linux distributions, Windows, macOS, and WSL have not been validated for this project's complete workflow |

Ubuntu 26.04 is the current verification baseline, not a declared minimum
version. Programming, byte echo, and burst echo have separate verification
results; see [Validation and Limitations](#validation-and-limitations).
Prepare the SDK and Python environment before building; no user-specific
installation directory is required.

## Build

Run commands from the project root. This example assumes the xhive checkout is
a sibling directory named `xhive`; adjust `XHIVE_SDK_PATH` for another layout.

```sh
export XHIVE_SDK_PATH="$(realpath ../xhive)"
xmake f -y
xmake
```

To build only the application, use `xmake build c2_hello`. To rebuild after
cleaning generated artifacts, run `xmake clean` followed by `xmake`.

| Path | Purpose |
| --- | --- |
| [src/main.c](src/main.c) | Startup greeting and echo loop |
| [xmake.lua](xmake.lua) | Build, programming, and serial-monitor tasks |
| [tests/test_echo.py](tests/test_echo.py) | Byte-for-byte serial test |
| `dist/c2_hello.elf` | Linked RISC-V executable |
| `dist/c2_hello.bin` | Raw firmware image for programming |
| `.vscode/compile_commands.json` | Automatically updated compilation database |

The BIN is generated directly from the ELF using the toolchain's
`objcopy -O binary`. No extra image header, checksum trailer, or fixed-size
padding is added. Link-map generation is currently disabled in `xmake.lua`.
`build/`, `dist/`, `.xmake/`, and `.vscode/` are generated directories excluded
from version control.

Use an SDK revision that retains `Reset_Handler` under LTO (`used` attribute in
`templates/startup_riscv.c`). Startup copies `.ram_text` from Flash to SRAM before
calling `main`. When changing the compiler or drivers, inspect the ELF's
`.ram_text` disassembly: the echo loop must not call back into Flash. Placing a
caller in `.ramfunc` does not automatically move its callees.

## Program and Run

On the C2 Pi board, the physical mode switch selects HFP-LINK/program mode or
UART/run mode. The programmer volume and CP2102 serial interface are not
available simultaneously.

The user successfully updated the earlier Flash-executed echo firmware by
dragging its BIN onto `YSYX-HFPLnk` in Windows. Use that confirmed method to
program the new `dist/c2_hello.bin`, then switch to UART/run mode and reset.
Wait until the startup greeting finishes before sending input.

The Linux helper below has **not** produced a confirmed firmware update on this
setup. Copy success or a matching host-side hash is not Flash readback evidence.

1. Select HFP-LINK/program mode and mount the volume labeled `YSYX-HFPLnk`.
2. Run `xmake flash --dry-run` to build and validate the destination without
   writing the device.
3. Run `xmake flash`. The task builds the application, copies the BIN as
   `retrosoc_fw.bin` twice with a 0.5-second pause, synchronizes each pass, and
   displays `STATE.TXT`. Copy or synchronization failures stop the task.
4. Wait for programmer activity to stop, then select UART/run mode and reset
   the board. Check switch positions against the board markings.
5. Run `xmake monitor`. It selects CP2102 automatically when exactly one is
   detected. Exit with Ctrl+].

For multiple programmers, set `HFP_MOUNT` to the selected volume's actual mount
root and run `xmake flash --mount="$HFP_MOUNT"`. The task checks the mounted label
and write access; an arbitrary directory is not accepted.

To choose a serial device explicitly:

```sh
xmake monitor --port=/dev/ttyUSB0
```

`/dev/ttyUSB0` is an example; use the device assigned by the operating system.
Keep local terminal echo disabled. Reset after opening the monitor to see the
startup greeting; the application does not repeat it periodically.

The two-copy behavior follows the referenced ECOS SDK script. It is not a
confirmed HFP-LINK protocol requirement or a verified fix for failed updates.
The monitor is a serial console, not a GDB server.

## Echo Test

After the new firmware has started, close other serial monitors and run:

```sh
python3 tests/test_echo.py /dev/ttyUSB0
```

The test uses 115200 8N1 with flow control off and consumes initial output.
By default it checks both modes:

- `byte`: 273 bytes, waiting for each echo before sending the next.
- `burst`: send `Hello`, `Hello\r\n`, `0123456789`, and all 256 byte values as
  whole writes, each repeated 20 times. Wait for the complete echo between
  bursts, not between their bytes.

A mismatch, timeout, short write, or trailing byte causes failure. Use
`--mode byte` or `--mode burst` to isolate a mode, and `--repeat 100` for more
burst repetitions. On Windows, use `python tests/test_echo.py COM3`, replacing
`COM3` with the actual device; the POSIX-only exclusive flag is omitted.

Expected output on a successful test:

```text
PASS: 273 stop-and-wait bytes at 115200 8N1
PASS: 80 bursts (5, 7, 10, 256 bytes) at 115200 8N1
Coverage: text, CR/LF, and every byte from 0x00 through 0xff
```

Finite bursts are not a sustained-throughput or FIFO-capacity qualification.
Data sent during the startup greeting is not covered. The checker itself can
be tested without hardware:

```sh
python3 -m unittest discover -s tests -p test_echo_cli.py -v
```

## Validation and Limitations

| Check | Status |
| --- | --- |
| Host environment | Linux / Ubuntu 26.04 |
| Firmware build | Passed with xPack RISC-V GCC 15.2.0 |
| Host echo-test script | Mock-port checks passed for byte/burst modes, fault rejection, and Windows option selection; not firmware simulation |
| Earlier Hello-only firmware | SYS_UART transmit output observed on a C2 board |
| Windows HFP-LINK update | User confirmed the Flash-executed echo firmware and its startup greeting |
| Flash-executed echo on hardware | 273 stop-and-wait bytes passed; burst loss reproduced on Linux as well as reported on Windows |
| SRAM/LTO candidate | Built and disassembly checked; reset after Ubuntu programming still showed the old boot greeting |
| Independent Flash readback | Not performed |
| OpenOCD/GDB hardware debugging | No workflow verified for this project |

With the Flash-executed firmware, a Linux hardware check received complete
`Hello` replies in 1/5 attempts and complete `Hello\r\n` replies in 0/5 attempts.
Adding a 1 ms interval between bytes gave 5/5 complete `Hello` replies. At
115200 8N1, back-to-back frames arrive about every 86.8 microseconds. Flash fetch
latency and possible TX bus stalls are suspected to delay RX servicing; the
current SRAM/LTO change needs a before/after hardware comparison to confirm.

In the latest Ubuntu 26.04 attempt, `xmake flash` completed both copies and
filesystem synchronization for the 476-byte diagnostic BIN. Its mounted-file
hash matched the local image, while `STATE.TXT` kept its default instruction.
With the serial port open before RST, the board still printed the old greeting
without `[SRAM/LTO v2]`. This confirms that the old firmware was still running;
the previously observed echo failures do not evaluate the SRAM/LTO candidate.

The programming failure remains unresolved. These observations identify a
failure in the tested Ubuntu/HFP-LINK update workflow, not its exact cause or a
general Ubuntu incompatibility. Host file hashes are not independent Flash
readback. Continue the comparison using Windows drag-and-drop, first confirm
the new boot marker, and then run the byte and burst tests. Update both xhive
and this project before rebuilding on the next host; a complete Windows build
and test workflow has not yet been validated.

SYS_UART has no documented software-visible TX-complete flag. A register write
does not prove that the last stop bit has left the wire. This example has no
software receive buffer or flow control, and makes no guarantee of lossless
continuous full-rate traffic. A successful byte test does not validate bursts,
and host checker tests do not validate the firmware.

## References

- [Official C2 Pi board documentation](https://embedded.openecos.com/zh-cn/latest/page/brd/starry-sky-c/v2.0_pi/)
- [Official SDK quickstart](https://embedded.openecos.com/zh-cn/latest/page/sdk/common/quickstart/)
- [ECOS SDK programming script](https://github.com/openecos-projects/embedded-sdk/blob/2.0/bin/ecos-flash)
- [HFP-LINK design and operation for earlier StarrySky boards](https://ysyx.oscc.cc/chip/board/official/boards/board-1/)
- [PicoRV32 source and interfaces](https://github.com/YosysHQ/picorv32/blob/main/picorv32.v)

Board revisions and programmer firmware can differ. Instructions or firmware
images for an earlier StarrySky board are not automatically applicable to C2 Pi.
