#!/usr/bin/env python3
"""No hardware, windows, audio, or non-stdlib Python dependencies."""
import io
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import pocketScreenshot as shot


class ScreenshotTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.exe = str(Path(cls.temp.name) / 'screenshottest')
        subprocess.run(['c++', '-std=c++17', '-O1', '-Wall', '-Wextra',
                        '-I' + str(ROOT / 'src'), str(ROOT / 'host/screenshottest.cpp'),
                        '-o', cls.exe], check=True)
        cls.dump = subprocess.check_output([cls.exe, '320', '240'])

    def test_firmware_frames_both_boards(self):
        for w, h in ((320, 240), (480, 320)):
            dump = subprocess.check_output([self.exe, str(w), str(h)])
            a, b, pixels = shot.capture(io.BytesIO(b'BOOT ready\n' + dump))
            self.assertEqual((a, b), (w, h))
            expected = b''.join(struct.pack('>H', (x * 97 + y * 31) & 65535)
                                for y in range(h) for x in range(w))
            self.assertEqual(pixels, expected)

    def test_reject_bad_frames(self):
        first = self.dump.index(b'SHOT DATA')
        payload = self.dump.index(b'\n', first) + 1
        corrupt = bytearray(self.dump)
        corrupt[payload] ^= 1
        cases = [bytes(corrupt), self.dump[:payload + 10],
                 self.dump[:self.dump.index(b'SHOT END')],
                 self.dump.replace(b'SHOT DATA 0 64', b'SHOT DATA 2 64', 1),
                 self.dump.replace(b'SHOT DATA 0 64', b'SHOT DATA 0 66', 1),
                 self.dump.replace(b'SHOT DATA 0 64', b'SHOT DATA 0 63', 1),
                 self.dump.replace(b'1 320 240', b'1 9999 240', 1),
                 self.dump.replace(b'BEGIN 1', b'BEGIN 2', 1),
                 self.dump.replace(b'SHOT END 153600', b'SHOT END 153599', 1),
                 self.dump.replace(b'SHOT END', b'SHOT ERR', 1),
                 self.dump[:self.dump.index(b'SHOT END')] + b'SHOT END 153600 00000000\n',
                 b'SHOT ERR busy\n', b'x' * 256]
        for dump in cases:
            with self.subTest(prefix=dump[:70]), self.assertRaises(ValueError):
                shot.capture(io.BytesIO(dump))

    def test_png_pixels_and_chunk_crcs(self):
        pixels = bytes.fromhex('f800 07e0 001f ffff 0000 8410')
        image = shot.png(3, 2, pixels)
        self.assertEqual(image[:8], b'\x89PNG\r\n\x1a\n')
        chunks = []
        pos = 8
        while pos < len(image):
            size, = struct.unpack_from('>I', image, pos)
            kind = image[pos + 4:pos + 8]
            data = image[pos + 8:pos + 8 + size]
            checksum, = struct.unpack_from('>I', image, pos + 8 + size)
            self.assertEqual(checksum, shot.crc(kind + data))
            chunks.append((kind, data))
            pos += size + 12
        self.assertEqual([k for k, _ in chunks], [b'IHDR', b'IDAT', b'IEND'])
        self.assertEqual(struct.unpack('>IIBBBBB', chunks[0][1]), (3, 2, 8, 2, 0, 0, 0))
        self.assertEqual(zlib.decompress(chunks[1][1]),
                         bytes.fromhex('00 ff0000 00ff00 0000ff 00 ffffff 000000 848284'))
        with self.assertRaises(ValueError):
            shot.png(3, 3, pixels)


if __name__ == '__main__':
    unittest.main()
