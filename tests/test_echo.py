#!/usr/bin/env python3
"""Check real-board SYS_UART echo at 115200 8N1 using pyserial.

Each byte is acknowledged before the next is sent. This verifies byte values
and both directions, not sustained full-rate throughput or FIFO capacity.
"""

import argparse

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="CP2102 serial device, e.g. /dev/ttyUSB0")
    args = parser.parse_args()
    payload = b"Hello echo test\r\n" + bytes(range(256))

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
        exclusive=True,
    ) as port:
        # Consume any startup greeting before checking byte-for-byte echo.
        startup = port.read(4096)
        if startup:
            print(f"Initial UART output: {startup!r}")

        for index, value in enumerate(payload):
            expected = bytes([value])
            if port.write(expected) != 1:
                raise SystemExit(f"Short serial write at byte {index}")
            received = port.read(1)
            if received != expected:
                raise SystemExit(
                    f"Echo mismatch at byte {index}: "
                    f"sent {expected.hex()}, received {received.hex() or '<timeout>'}"
                )

        extra = port.read(1)
        if extra:
            raise SystemExit(f"Unexpected byte after echo: {extra.hex()}")

    print(f"PASS: {len(payload)} bytes echoed exactly at 115200 8N1")
    print("Coverage: text, CR/LF, and every byte from 0x00 through 0xff")


if __name__ == "__main__":
    main()
