#!/usr/bin/env python3
"""Compile actual board file/upload routines against bounded filesystem/serial mocks.
Not a firmware build, LittleFS power-loss test, or physical card/USB qualification.
"""
from pathlib import Path
import argparse
import hashlib
import os
import shlex
import subprocess
import tempfile

host = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("case", choices=["cleanup", "read", "crc", "all"], default="all", nargs="?")
parser.add_argument("--sanitize", action="store_true")
args = parser.parse_args()
source = (host.parent / "src/main.cpp").read_text()

def routine(signature):
    start = source.index(signature)
    body = source.index("{", start)
    # Selected routines contain no braces in string literals/comments.
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"

routines = ""
for signature in ["static bool smallReadAt(", "static bool packOpen(", "static bool smallOpen(",
                  "static uint8_t* readFileIn(", "static bool diskLine(", "static void diskDrain("]:
    routines += routine(signature)
# Optional during the test-first baseline: absent cleanup must fail behavior, not compilation.
if "static bool diskCleanupTemps(" in source:
    routines += routine("static bool diskCleanupTemps(")
routines += routine("static void diskPut(")
start = source.index("  if (!LittleFS.begin(false))", source.index("void setup()"))
end = source.index("  loadGameList();", start) + len("  loadGameList();")
routines += "static void bootMount() {\n" + source[start:end] + "\n}\n"
program = (host / "uploadtest.cpp").read_text().replace("// PRODUCTION_ROUTINES", routines)
flags = ["-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fno-omit-frame-pointer"]
if args.sanitize:
    flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
with tempfile.TemporaryDirectory(prefix="pocket-upload-", dir=os.environ.get("TMPDIR")) as work:
    cpp = Path(work) / "test.cpp"
    exe = Path(work) / "test"
    cpp.write_text(program)
    command = [os.environ.get("CXX", "c++"), *flags, "-I" + str(host.parent / "src"), str(cpp), "-o", str(exe)]
    print("COMPILE", shlex.join(command), flush=True)
    print("SOURCE", hashlib.sha256(source.encode()).hexdigest(), "HARNESS", hashlib.sha256(program.encode()).hexdigest(), flush=True)
    subprocess.run(command, check=True)
    print("BINARY", hashlib.sha256(exe.read_bytes()).hexdigest(), flush=True)
    if args.sanitize:
        symbols = subprocess.check_output(["nm", "-u", str(exe)], text=True)
        for required in ["__asan_init", "__ubsan_handle_"]:
            assert required in symbols, required + " missing from compiled binary"
        libraries = subprocess.check_output(["ldd", str(exe)], text=True)
        assert "libasan" in libraries and "libubsan" in libraries
        print("INSTRUMENTATION", "\n".join(line for line in libraries.splitlines() if "libasan" in line or "libubsan" in line), flush=True)
    subprocess.run([str(exe), args.case], check=True, timeout=15)
