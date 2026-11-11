#!/usr/bin/env python3
"""Save Pocket Player RAM logs (stdlib only, macOS/Linux, 115200 baud).
Usage: python3 scripts/pocketLogs.py PORT OUT.log
       python3 scripts/pocketLogs.py --input dump.bin OUT.log
"""
import argparse
from pathlib import Path
import time
from pocketScreenshot import Port, crc, exact, line

CAPACITY = 3072


def checksum(field):
    if len(field) != 8 or any(c not in b'0123456789abcdefABCDEF' for c in field):
        raise ValueError('invalid CRC field')
    return int(field, 16)


def number(field):
    if not field or not field.isdigit():
        raise ValueError('invalid decimal field')
    return int(field)


def capture(stream):
    deadline = time.monotonic() + 10
    while True:
        header = line(stream)
        if header.startswith(b'LOGS ERR'):
            raise ValueError(header.decode('ascii', 'replace'))
        if header.startswith(b'LOGS'):
            break
        if time.monotonic() > deadline:
            raise ValueError('no LOGS BEGIN received')
    fields = header.split()
    if len(fields) != 4 or fields[:3] != [b'LOGS', b'BEGIN', b'1']:
        raise ValueError('unsupported LOGS header')
    size = number(fields[3])
    if size > CAPACITY:
        raise ValueError('log size exceeds ring capacity')
    result = bytearray()
    deadline = time.monotonic() + 15
    while True:
        if time.monotonic() > deadline:
            raise ValueError('log transfer deadline exceeded')
        fields = line(stream).split()
        if not fields or fields[0] != b'LOGS':
            continue
        if len(fields) == 5 and fields[1] == b'DATA':
            offset, count = map(number, fields[2:4])
            if offset != len(result) or not 0 < count <= 64 or offset + count > size:
                raise ValueError('invalid log chunk offset/size')
            data = exact(stream, count)
            if line(stream) != b'' or crc(data) != checksum(fields[4]):
                raise ValueError('log chunk CRC or delimiter mismatch')
            result.extend(data)
        elif len(fields) == 4 and fields[1] == b'END':
            if (len(result) != size or number(fields[2]) != size
                    or checksum(fields[3]) != crc(result)):
                raise ValueError('incomplete logs or whole CRC mismatch')
            return bytes(result)
        else:
            raise ValueError('unexpected LOGS reply')


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
                data = capture(stream)
        else:
            port = Port(args.source)
            port.write(b'LOGS\n')
            data = capture(port)
        Path(args.output).write_bytes(data)
        print(f'Saved {len(data)} log bytes to {args.output}')
    except (OSError, ValueError) as error:
        parser.exit(1, f'Log capture failed: {error}\n')
    finally:
        if port:
            port.close()


if __name__ == '__main__':
    main()
