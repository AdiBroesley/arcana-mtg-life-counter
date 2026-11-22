"""Verify bounded ring tests and cleanup without accessing a physical device."""
import contextlib
import importlib.util
import io
import pathlib
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("ring_diagnostics", pathlib.Path(__file__).parents[1] / "tools/ring_diagnostics.py")
diagnostics = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diagnostics)


class SerialCapture:
    def __init__(self, chunks):
        self.chunks = list(chunks)
        self.written = b""
        self.is_open = False

    def open(self):
        if self.dtr or self.rts:
            raise AssertionError("Diagnostics must not reset the device")
        self.is_open = True

    def reset_input_buffer(self):
        pass

    def write(self, data):
        self.written += data

    def flush(self):
        pass

    def read(self, count):
        return self.chunks.pop(0)

    def close(self):
        self.is_open = False


class RingTests(unittest.TestCase):
    def record(self, action, chunks, clock=None):
        connection = SerialCapture(chunks)
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "ring.log"
            args = ["ring_diagnostics", "--port", "TEST", "--action", action, "--output", str(output)]
            clock_patch = patch.object(diagnostics.time, "monotonic", side_effect=clock) if clock else contextlib.nullcontext()
            with patch.object(diagnostics.serial, "Serial", return_value=connection), patch("sys.argv", args), clock_patch, contextlib.redirect_stdout(io.StringIO()):
                result = diagnostics.main()
            data = output.read_bytes()
        self.assertFalse(connection.is_open)
        return result, connection.written, data

    def test_status_reads_battery_history_without_forcing_a_colour(self):
        chunks = [b"RING STATUS mode=DMA failed=0\nRING BATTERY mv=3800 usb=0 percent=50\nRING STATUS END\n"]
        result, sent, data = self.record("status", chunks)
        self.assertEqual((result, sent, data), (0, b"d\n", b"".join(chunks)))

    def test_each_backend_waits_for_automatic_restoration(self):
        for action, command in (("dma", b"t\n"), ("rmt", b"u\n")):
            chunks = [b"RING TEST START red=5/255\nRING STATUS failed=0\nRING STATUS END\n", b"RING TEST REST", b"ORED\n"]
            result, sent, data = self.record(action, chunks)
            self.assertEqual((result, sent, data), (0, command, b"".join(chunks)))

    def test_timeout_cancels_override_and_retains_partial_log(self):
        result, sent, data = self.record("dma", [b"RING TEST START\n"], clock=[0, 1, 26])
        self.assertEqual((result, sent, data), (1, b"t\nx\n", b"RING TEST START\n"))

    def test_driver_failure_is_not_reported_as_a_successful_test(self):
        result, _, _ = self.record("rmt", [b"RING STATUS failed=1\nRING TEST RESTORED\n"])
        self.assertEqual(result, 1)


if __name__ == "__main__":
    unittest.main()
