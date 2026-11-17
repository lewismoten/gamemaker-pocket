"""Unit tests for release_fw.py: no network, no gh, no git; subprocess and the file tree are faked."""
import hashlib, importlib.util, json, pathlib, subprocess, sys, tempfile, unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("release_fw", pathlib.Path(__file__).with_name("release_fw.py"))
rf = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rf)
HEAD = "abc1234"


class Case(unittest.TestCase):
    def setUp(self):
        self.tmp = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(lambda: __import__("shutil").rmtree(self.tmp))
        for b in rf.BOARDS:
            (self.tmp / "docs" / "fw" / b).mkdir(parents=True)
            for n in rf.FILES:
                (self.tmp / "docs" / "fw" / b / n).write_bytes(f"{b}{n}".encode())
        self.stamp("abc1234 (2026-11-17)")
        self.dirty, self.diff_fails, self.calls = "", False, []
        p = mock.patch.object(rf, "ROOT", self.tmp); p.start(); self.addCleanup(p.stop)
        p = mock.patch.object(rf, "sh", self.fake_sh); p.start(); self.addCleanup(p.stop)
        p = mock.patch.object(rf.subprocess, "check_call", lambda c, **k: self.calls.append(c)); p.start(); self.addCleanup(p.stop)

    def stamp(self, v):
        (self.tmp / "docs" / "manifest-28.json").write_text(json.dumps({"version": v}))

    def fake_sh(self, *a):
        if a[:2] == ("git", "status"): return self.dirty
        if a[:2] == ("git", "diff"):
            if self.diff_fails or a[2:4] != ("--quiet", "abc1234"): raise subprocess.CalledProcessError(1, a)
            return ""
        if a[:3] == ("git", "rev-parse", "--short=7"): return HEAD
        if a[:2] == ("git", "rev-parse"): return HEAD * 5 + "xxxxx"
        raise AssertionError(a)

    def run_main(self, *args):
        with mock.patch.object(sys, "argv", ["release_fw.py", "--out", str(self.tmp / "out"), *args]):
            rf.main()

    def test_dirty_tree_refused(self):
        self.dirty = " M x"
        with self.assertRaises(SystemExit): self.run_main("--publish")
        self.assertEqual(self.calls, [])

    def test_stale_build_refused(self):
        self.stamp("old0000 (2026-01-01)"); self.diff_fails = True
        with self.assertRaises(SystemExit): self.run_main("--publish")
        self.assertEqual(self.calls, [])

    def test_dirty_build_stamp_refused(self):
        self.stamp("abc1234+ (2026-11-17)")
        with self.assertRaises(SystemExit): self.run_main("--publish")
        self.assertEqual(self.calls, [])

    def test_target_passed(self):
        self.run_main("--publish")
        cmd = self.calls[0]
        self.assertEqual(cmd[:3], ["gh", "release", "create"])
        self.assertEqual(cmd[cmd.index("--target") + 1], HEAD * 5 + "xxxxx")

    def test_hash_matches_uploaded_bytes(self):
        reads = iter(range(100))
        orig = pathlib.Path.read_bytes
        def flaky(path):  # a second read of the same file would return other bytes
            return orig(path) if str(path).startswith(str(self.tmp / "docs")) and next(reads) < 8 else b"changed"
        with mock.patch.object(pathlib.Path, "read_bytes", flaky):
            self.run_main("--publish")
        man = json.loads((self.tmp / "out" / "pocket-firmware.json").read_text())
        for e in man["files"]:
            self.assertEqual(hashlib.sha256((self.tmp / "out" / e["asset"]).read_bytes()).hexdigest(), e["sha256"])


if __name__ == "__main__":
    unittest.main()
