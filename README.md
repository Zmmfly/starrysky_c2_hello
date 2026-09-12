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

Startup output:

```text
Hello World from xhive vendor: opencos / StarrySky C2!
```

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
| Startup and linker script | xhive default templates |

`CONFIG_STARRYSKY_C2_CLOCK_HZ` describes the actual oscillator frequency; changing
it does not reconfigure the hardware clock.

## Requirements

**Current host environment: Linux / Ubuntu 26.04.** Implementation, firmware
builds, and host-side software checks were performed in this environment.
The C2 firmware itself remains bare-metal; Ubuntu runs on the development host.

- An xhive SDK checkout with its RISC-V GCC toolchain and configuration tools
  available.
- Xmake and the SDK's Python configuration dependencies, including `kconfiglib`.
- Python 3 with `pyserial` for the serial monitor and echo test.
- Linux with `lsblk`, `cp`, and `sync` for the board-local programming task.
- Permission to access the mounted programmer volume and serial device.

### Operating System Scope

| Workflow | Constraint |
| --- | --- |
| Build | Verified on Ubuntu 26.04; examples use a POSIX shell and `realpath` |
| Programming helper | Explicitly restricted to Linux; depends on `lsblk`, `cp`, and `sync -f` |
| Serial monitor and echo test | Implemented for the current Linux setup; require serial-device access, and the test uses POSIX exclusive-open support |
| Other environments | Other Linux distributions, Windows, macOS, and WSL have not been validated for this project's complete workflow |

Ubuntu 26.04 is the current verification baseline, not a declared minimum
version. Linux host support does not mean HFP-LINK programming or hardware echo
has passed; see [Validation and Limitations](#validation-and-limitations).
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

## Program and Run

On the C2 Pi board, the physical mode switch selects HFP-LINK/program mode or
UART/run mode. The programmer volume and CP2102 serial interface are not
available simultaneously.

> Programming has not been validated as a successful update path on the tested
> setup. A completed copy or matching host-side file hash does not establish
> that the new firmware is present in Flash or running on the board.

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

The test opens the port exclusively at 115200 8N1 with flow control off, consumes
any initial output, then sends text, CR/LF, and every value from `0x00` through
`0xFF`. It waits for each echoed byte before sending the next. A mismatch,
timeout, or trailing byte causes failure.

Expected output on a successful test:

```text
PASS: 273 bytes echoed exactly at 115200 8N1
Coverage: text, CR/LF, and every byte from 0x00 through 0xff
```

This is a basic bidirectional test, not a sustained-throughput or FIFO-capacity
test. Data sent during the startup greeting is not covered.

## Validation and Limitations

| Check | Status |
| --- | --- |
| Host environment | Linux / Ubuntu 26.04 |
| Firmware build | Passed with xPack RISC-V GCC 15.2.0 |
| Host echo-test script | Pseudo-terminal checks passed for correct echo and rejection of corrupted, missing, and trailing bytes |
| Earlier Hello-only firmware | SYS_UART transmit output observed on a C2 board |
| Echo firmware on hardware | Not validated; earlier programming attempts left the old periodic-Hello firmware running |
| Independent Flash readback | Not performed |
| OpenOCD/GDB hardware debugging | No workflow verified for this project |

The current startup-greeting revision has been built, but not hardware-tested.
Earlier attempts included single-copy and two-copy programming, alternative
filenames, and an observed USB disconnect/reconnect after a requested power
cycle. These did not establish a successful firmware update. `STATE.TXT` retaining
its default instruction is not a positive result; host caching also limits
conclusions drawn from file reads. The cause remains unresolved.

SYS_UART has no documented software-visible TX-complete flag. A register write
does not prove that the last stop bit has left the wire. This example has no
software receive buffer or flow control, and makes no guarantee of lossless
continuous full-rate traffic. Neither simulated tests nor the older Hello-only
result constitute hardware validation of echo.

## References

- [Official C2 Pi board documentation](https://embedded.openecos.com/zh-cn/latest/page/brd/starry-sky-c/v2.0_pi/)
- [Official SDK quickstart](https://embedded.openecos.com/zh-cn/latest/page/sdk/common/quickstart/)
- [ECOS SDK programming script](https://github.com/openecos-projects/embedded-sdk/blob/2.0/bin/ecos-flash)
- [HFP-LINK design and operation for earlier StarrySky boards](https://ysyx.oscc.cc/chip/board/official/boards/board-1/)
- [PicoRV32 source and interfaces](https://github.com/YosysHQ/picorv32/blob/main/picorv32.v)

Board revisions and programmer firmware can differ. Instructions or firmware
images for an earlier StarrySky board are not automatically applicable to C2 Pi.
