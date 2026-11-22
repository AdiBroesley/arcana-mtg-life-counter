"""Check recording commands and cleanup without opening a physical serial port."""
import contextlib
import importlib.util
import io
import pathlib
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("profile_device", pathlib.Path(__file__).parents[1] / "tools/profile_device.py")
profiler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profiler)


class SerialCapture:
    def __init__(self, chunks):
        self.chunks = list(chunks)
        self.written = b""
        self.closed = False

    def open(self):
        if self.dtr or self.rts:
            raise AssertionError("Recording must not assert reset/boot lines")

    def reset_input_buffer(self):
        pass

    def write(self, data):
        self.written += data

    def flush(self):
        pass

    def read(self, count):
        return self.chunks.pop(0)

    def close(self):
        self.closed = True


class ProfilerTests(unittest.TestCase):
    def record(self, chunks, clock=None, benchmark=False):
        connection = SerialCapture(chunks)
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "recording.log"
            args = ["profile_device", "--port", "TEST", "--output", str(output)]
            if benchmark:
                args.append("--benchmark-undo")
            clock_patch = patch.object(profiler.time, "monotonic", side_effect=clock) if clock else contextlib.nullcontext()
            with patch.object(profiler.serial, "Serial", return_value=connection), patch("sys.argv", args), clock_patch, contextlib.redirect_stdout(io.StringIO()):
                result = profiler.main()
            data = output.read_bytes()
        self.assertTrue(connection.closed)
        return result, connection.written, data

    def test_preserves_split_summary_and_new_undo_metrics(self):
        chunks = [b"PROFILE START 30 seconds match_saving=removed\n",
                  b"PROFILE undo_apply n=2 avg=20 p50=20 p95=25 max=25\n",
                  b"PROFILE wheel_to_frame n=3 avg=25000 p50=24000 p95=28000 max=28000\n",
                  b"PROFILE frame_pixels n=3 avg=16092 p50=16092 p95=16092 max=16092\n",
                  b"PROFILE match_saving=removed\nPROFILE E", b"ND\n"]
        result, sent, logged = self.record(chunks)
        self.assertEqual(result, 0)
        self.assertEqual(sent, b"p\n")
        self.assertEqual(logged, b"".join(chunks))

    def test_timeout_retains_partial_log_and_closes_port(self):
        result, sent, data = self.record([b"PROFILE START\n"], clock=[0, 1, 46])
        self.assertEqual((result, sent, data), (1, b"p\n", b"PROFILE START\n"))

    def test_removed_saving_option_cannot_enable_match_writes(self):
        with patch("sys.argv", ["profile_device", "--port", "TEST", "--saving", "on"]), patch.object(profiler.serial, "Serial") as factory, contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                profiler.main()
            self.assertEqual(error.exception.code, 2)
            factory.assert_not_called()

    def test_undo_comparison_requires_restored_completion(self):
        chunks = [b"RENDER_BENCH START\nRENDER_BENCH phase=0 caption=live n=24 avg=100000\n",
                  b"RENDER_BENCH phase=1 caption=frozen n=24 avg=10000\n",
                  b"RENDER_BENCH END resto", b"red=1\n"]
        result, sent, data = self.record(chunks, benchmark=True)
        self.assertEqual((result, sent, data), (0, b"b\n", b"".join(chunks)))

    def test_undo_comparison_busy_closes_the_port(self):
        result, sent, data = self.record([b"RENDER_BENCH START\nRENDER_BENCH BUSY open Single ARCANA\n"], benchmark=True)
        self.assertEqual((result, sent), (1, b"b\n"))
        self.assertIn(b"RENDER_BENCH BUSY", data)


if __name__ == "__main__":
    unittest.main()
