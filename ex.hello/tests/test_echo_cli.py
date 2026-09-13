"""Host-only regression tests for the serial checker, not the C2 firmware."""

import contextlib
import io
import types
import unittest
from unittest.mock import patch

import test_echo


class EchoPort:
    def __init__(self, fault=None):
        self.fault = fault
        self.pending = b""
        self.writes = []

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def write(self, data):
        self.writes.append(data)
        reply = data
        if self.fault == "short_write":
            return len(data) - 1
        if self.fault == "corrupt":
            reply = bytes([data[0] ^ 1]) + data[1:]
        elif self.fault == "drop":
            reply = data[:-1]
        elif self.fault == "extra":
            reply = data + b"!"
        self.pending += reply
        return len(data)

    def read(self, size):
        data, self.pending = self.pending[:size], self.pending[size:]
        return data


class EchoCliTests(unittest.TestCase):
    def run_checker(self, port, *args, host="posix"):
        output = io.StringIO()
        with patch("sys.argv", ["test_echo.py", "TEST_PORT", *args]), \
             patch.object(test_echo.serial, "Serial", return_value=port) as factory, \
             patch.object(test_echo, "os", types.SimpleNamespace(name=host)), \
             contextlib.redirect_stdout(output):
            test_echo.main()
        return output.getvalue(), factory.call_args.kwargs

    def test_default_checks_bytes_and_complete_bursts(self):
        port = EchoPort()
        output, options = self.run_checker(port, "--repeat", "2")
        expected = [bytes([value]) for value in test_echo.BYTE_PAYLOAD]
        expected += [payload for payload in test_echo.BURSTS for _ in range(2)]
        self.assertEqual(port.writes, expected)
        self.assertIn("273 stop-and-wait bytes", output)
        self.assertIn("8 bursts", output)
        self.assertTrue(options["exclusive"])
        self.assertEqual(options["baudrate"], 115200)
        self.assertFalse(options["xonxoff"])
        self.assertFalse(options["rtscts"])
        self.assertFalse(options["dsrdtr"])

    def test_windows_omits_posix_exclusive_option(self):
        output, options = self.run_checker(EchoPort(), "--mode", "byte", host="nt")
        self.assertNotIn("exclusive", options)
        self.assertNotIn("bursts", output)

    def test_burst_mode_does_not_send_one_byte_at_a_time(self):
        port = EchoPort()
        self.run_checker(port, "--mode", "burst", "--repeat", "1")
        self.assertEqual(port.writes, list(test_echo.BURSTS))

    def test_faults_are_rejected_in_both_modes(self):
        for mode in ("byte", "burst"):
            for fault in ("short_write", "corrupt", "drop", "extra"):
                with self.subTest(mode=mode, fault=fault):
                    with self.assertRaises(SystemExit):
                        self.run_checker(EchoPort(fault), "--mode", mode, "--repeat", "1")

    def test_nonpositive_repeat_is_rejected_before_opening_port(self):
        for count in ("0", "-1"):
            with self.subTest(count=count), \
                 patch.object(test_echo.serial, "Serial") as factory, \
                 patch("sys.argv", ["test_echo.py", "TEST_PORT", "--repeat", count]), \
                 contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    test_echo.main()
                factory.assert_not_called()


if __name__ == "__main__":
    unittest.main()
