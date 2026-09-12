#!/usr/bin/env python3
"""Check real-board SYS_UART echo at 115200 8N1 using pyserial.

Check both stop-and-wait bytes and whole bursts, matching a serial tool's Send
button. Finite bursts do not prove lossless sustained full-rate traffic.
"""

import argparse
import os

import serial


BYTE_PAYLOAD = b"Hello echo test\r\n" + bytes(range(256))
BURSTS = (b"Hello", b"Hello\r\n", b"0123456789", bytes(range(256)))


def check_echo(port, payload, label):
    """Send one complete payload and require an exact, bounded-time reply."""
    if port.write(payload) != len(payload):
        raise SystemExit(f"Short serial write: {label}")
    received = port.read(len(payload))
    if received != payload:
        raise SystemExit(
            f"Echo mismatch: {label}; sent {len(payload)} bytes, "
            f"received {len(received)}\n"
            f"Expected: {payload!r}\nReceived: {received!r}"
        )


def check_no_extra(port):
    extra = port.read(1)
    if extra:
        raise SystemExit(f"Unexpected byte after echo: {extra.hex()}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="CP2102 device, e.g. /dev/ttyUSB0 or COM3")
    parser.add_argument("--mode", choices=("byte", "burst", "all"), default="all")
    parser.add_argument("--repeat", type=int, default=20,
                        help="Repetitions of each burst (default: 20)")
    args = parser.parse_args()
    if args.repeat < 1:
        parser.error("--repeat must be positive")

    # pyserial's exclusive option is POSIX-only; Windows uses COM device names.
    options = {"exclusive": True} if os.name == "posix" else {}
    with serial.Serial(
        args.port,
        baudrate=115200,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.5,
        write_timeout=1.0,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False,
        **options,
    ) as port:
        # Consume any startup greeting before checking byte-for-byte echo.
        startup = port.read(4096)
        if startup:
            print(f"Initial UART output: {startup!r}")

        if args.mode in ("byte", "all"):
            for index, value in enumerate(BYTE_PAYLOAD):
                check_echo(port, bytes([value]), f"byte {index}")
            check_no_extra(port)
            print(f"PASS: {len(BYTE_PAYLOAD)} stop-and-wait bytes at 115200 8N1",
                  flush=True)

        if args.mode in ("burst", "all"):
            for payload in BURSTS:
                for repeat in range(args.repeat):
                    check_echo(port, payload,
                               f"{len(payload)}-byte burst {repeat + 1}/{args.repeat}")
                check_no_extra(port)
            print(f"PASS: {len(BURSTS) * args.repeat} bursts "
                  f"(5, 7, 10, 256 bytes) at 115200 8N1", flush=True)

    print("Coverage: text, CR/LF, and every byte from 0x00 through 0xff")


if __name__ == "__main__":
    main()
