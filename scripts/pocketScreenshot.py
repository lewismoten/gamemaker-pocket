#!/usr/bin/env python3
"""Capture SHOT from a Pocket Player, using only Python stdlib (macOS/Linux).
Usage: python3 scripts/pocketScreenshot.py PORT OUT.png
       python3 scripts/pocketScreenshot.py --input dump.bin OUT.png
"""
import argparse
import binascii
import os
from pathlib import Path
import select
import struct
import time
import zlib


def crc(data):
    return binascii.crc32(data) & 0xffffffff


def exact(stream, n):
    out = bytearray()
    while len(out) < n:
        part = stream.read(n - len(out))
        if not part:
            raise ValueError('truncated capture or serial timeout')
        out.extend(part)
    return bytes(out)


def line(stream):
    out = bytearray()
    while len(out) < 256:
        c = exact(stream, 1)
        if c == b'\n':
            return bytes(out).rstrip(b'\r')
        out.extend(c)
    raise ValueError('overlong protocol line')


def capture(stream):
    # Other firmware logs may occur between packets, never within a packet.
    deadline = time.monotonic() + 10
    while True:
        header = line(stream)
        if header.startswith(b'SHOT ERR'):
            raise ValueError(header.decode('ascii'))
        if header.startswith(b'SHOT BEGIN '):
            break
        if time.monotonic() > deadline:
            raise ValueError('no SHOT BEGIN received')
    fields = header.split()
    if len(fields) != 6 or fields[2] != b'1' or fields[5] != b'RGB565BE':
        raise ValueError('unsupported SHOT header')
    w, h = map(int, fields[3:5])
    if (w, h) not in ((320, 240), (480, 320)):
        raise ValueError('unsupported screen dimensions')
    pixels = bytearray()
    deadline = time.monotonic() + 180
    while True:
        if time.monotonic() > deadline:
            raise ValueError('capture deadline exceeded')
        fields = line(stream).split()
        if not fields or fields[0] != b'SHOT':
            continue
        if len(fields) == 5 and fields[1] == b'DATA':
            offset, size = map(int, fields[2:4])
            if (offset != len(pixels) or not 0 < size <= 64 or size % 2
                    or offset + size > w * h * 2
                    or offset % (w * 2) + size > w * 2):
                raise ValueError('invalid chunk offset/size')
            data = exact(stream, size)
            if line(stream) != b'' or crc(data) != int(fields[4], 16):
                raise ValueError('chunk CRC or delimiter mismatch')
            pixels.extend(data)
        elif len(fields) == 4 and fields[1] == b'END':
            if (len(pixels) != w * h * 2 or int(fields[2]) != len(pixels)
                    or int(fields[3], 16) != crc(pixels)):
                raise ValueError('incomplete capture or frame CRC mismatch')
            return w, h, bytes(pixels)
        else:
            raise ValueError('unexpected SHOT reply: ' + b' '.join(fields).decode('ascii', 'replace'))


def png(w, h, pixels):
    """RGB565 big endian -> lossless 8-bit RGB PNG, filter 0 per row."""
    if w <= 0 or h <= 0 or len(pixels) != w * h * 2:
        raise ValueError('invalid pixel dimensions')
    rows = bytearray()
    for y in range(h):
        rows.append(0)
        for x in range(w):
            i = (y * w + x) * 2
            v = (pixels[i] << 8) | pixels[i + 1]
            r, g, b = v >> 11, (v >> 5) & 63, v & 31
            rows.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', crc(kind + data))

    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


class Port:
    """POSIX 115200 serial, raw bytes; leave DTR/RTS untouched to avoid reset."""
    def __init__(self, path):
        import termios
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        try:
            self.old = termios.tcgetattr(self.fd)
            attr = termios.tcgetattr(self.fd)
            attr[0] = attr[1] = attr[3] = 0
            attr[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
            attr[4] = attr[5] = termios.B115200
            attr[6][termios.VMIN] = attr[6][termios.VTIME] = 0
            termios.tcsetattr(self.fd, termios.TCSANOW, attr)
            termios.tcflush(self.fd, termios.TCIFLUSH)
        except BaseException:
            os.close(self.fd)
            raise

    def read(self, n):
        if not select.select([self.fd], [], [], 5)[0]:
            return b''
        return os.read(self.fd, n)

    def write(self, data):
        while data:
            if not select.select([], [self.fd], [], 5)[1]:
                raise ValueError('serial write timeout')
            data = data[os.write(self.fd, data):]

    def close(self):
        import termios
        try:
            termios.tcsetattr(self.fd, termios.TCSANOW, self.old)
        finally:
            os.close(self.fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', action='store_true', help='read a saved binary serial dump')
    parser.add_argument('source')
    parser.add_argument('output')
    args = parser.parse_args()
    port = None
    try:
        if args.input:
            with open(args.source, 'rb') as stream:
                w, h, pixels = capture(stream)
        else:
            port = Port(args.source)
            port.write(b'SHOT\n')
            w, h, pixels = capture(port)
        Path(args.output).write_bytes(png(w, h, pixels))
        print(f'Saved {w}x{h} to {args.output}')
    except (OSError, ValueError) as error:
        if port:
            port.write(b'SHOT ABORT\n')
        parser.exit(1, f'Capture failed: {error}\n')
    finally:
        if port:
            port.close()


if __name__ == '__main__':
    main()
