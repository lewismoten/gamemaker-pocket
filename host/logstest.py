#!/usr/bin/env python3
"""Ring and real firmware framing, no board or third-party Python modules."""
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import pocketLogs as logs


class LogsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.exe = str(Path(cls.temp.name) / 'logringtest')
        subprocess.run(['c++', '-std=c++17', '-O1', '-Wall', '-Wextra',
                        '-I' + str(ROOT / 'src'), str(ROOT / 'host/logringtest.cpp'),
                        '-o', cls.exe], check=True)
        cls.dump = subprocess.check_output([cls.exe])

    def test_ring_and_frames(self):
        result = logs.capture(io.BytesIO(b'BOOT ready\n' + self.dump))
        expected = b''
        for i in range(4000):
            text = (str(i) + 'a' * (i % 150) + '\n').encode()
            while len(expected) + len(text) > 3072:
                expected = expected.split(b'\n', 1)[1]
            expected += text
        self.assertEqual(result, expected)
        self.assertEqual(logs.capture(io.BytesIO(subprocess.check_output([self.exe, 'empty']))), b'')

    def test_bad_frames(self):
        start = self.dump.index(b'LOGS DATA')
        payload = self.dump.index(b'\n', start) + 1
        end = self.dump.index(b'LOGS END')
        corrupt = bytearray(self.dump)
        corrupt[payload] ^= 1
        cases = [bytes(corrupt), self.dump[:payload + 2], self.dump[:end],
                 self.dump.replace(b'DATA 0 64', b'DATA 1 64', 1),
                 self.dump.replace(b'DATA 0 64', b'DATA 0 65', 1),
                 self.dump.replace(b'DATA 0 64', b'DATA 0 0', 1),
                 self.dump.replace(b'BEGIN 1', b'BEGIN 2', 1),
                 b'LOGS BEGIN 1 3073\n', b'LOGS ERR busy\n', b'x' * 256,
                 self.dump[:end] + b'LOGS END 1 00000000\n',
                 self.dump[:-10] + b'00000000\n',
                 self.dump[:payload + 64] + b'x\n' + self.dump[payload + 66:],
                 self.dump[:end] + b'LOGS DATA 0 1 00000000\nx\n',
                 b'LOGS BEGIN 1 -1\n', b'LOGS BEGIN 1 1\nLOGS WHAT\n']
        for dump in cases:
            with self.subTest(prefix=dump[:60]), self.assertRaises(ValueError):
                logs.capture(io.BytesIO(dump))

    def test_cli_saves_only_verified_data(self):
        source = Path(self.temp.name) / 'dump.bin'
        output = Path(self.temp.name) / 'saved.log'
        source.write_bytes(self.dump)
        subprocess.run([sys.executable, str(ROOT / 'scripts/pocketLogs.py'),
                        '--input', str(source), str(output)], check=True, capture_output=True)
        expected = output.read_bytes()
        self.assertEqual(expected, logs.capture(io.BytesIO(self.dump)))
        source.write_bytes(self.dump[:-20])
        failed = subprocess.run([sys.executable, str(ROOT / 'scripts/pocketLogs.py'),
                                 '--input', str(source), str(output)], capture_output=True)
        self.assertNotEqual(failed.returncode, 0)
        self.assertEqual(output.read_bytes(), expected)


if __name__ == '__main__':
    unittest.main()
