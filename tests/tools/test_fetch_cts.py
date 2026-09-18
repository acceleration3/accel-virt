"""The CTS build this repository runs, pinned by URL and hash (feature 048,
FR-001).

Driven over archives the test builds and serves through `file://`, because the
real one has never been downloaded here - which is the point of the pins being
empty, and the reason the tooling has to be provable without them.
"""
import hashlib
import io
import pathlib
import sys
import tarfile
import tempfile
import unittest
import zipfile

from tool_run import ROOT, run

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from fetch_cts import CtsFetch, CtsPins  # noqa: E402
from fetch_layers import Refused  # noqa: E402

FETCH = ROOT / "tools" / "fetch_cts.py"
DEQP = b"MZ" + b"deqp-vk build" * 40
DATA = b"# a mustpass list" * 20


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


class Fetching(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)
        self.into = self.root / "vulkan-cts"
        self.cache = self.root / "cache"

    def zip_archive(self, members, name="vulkan-cts-1.4.0.0.zip"):
        path = self.root / name
        raw = io.BytesIO()
        with zipfile.ZipFile(raw, "w") as bundle:
            for inside, content in members.items():
                bundle.writestr(f"vulkan-cts-1.4.0.0/{inside}", content)
        path.write_bytes(raw.getvalue())
        return path

    def tar_archive(self, members, name="vulkan-cts-1.4.0.0.tar.gz"):
        path = self.root / name
        raw = io.BytesIO()
        with tarfile.open(fileobj=raw, mode="w:gz") as bundle:
            for inside, content in members.items():
                info = tarfile.TarInfo(f"vulkan-cts-1.4.0.0/{inside}")
                info.size = len(content)
                bundle.addfile(info, io.BytesIO(content))
        path.write_bytes(raw.getvalue())
        return path

    def pins(self, archive, members, name="cts_pins.txt", archive_digest=None, file_digests=None):
        lines = [f"# a pinned build\narchive file://{archive} {archive_digest or digest(archive.read_bytes())}"]
        for inside, content in members.items():
            wanted = (file_digests or {}).get(inside, digest(content))
            lines.append(f"file {pathlib.PurePosixPath(inside).name} {inside} {wanted}")
        path = self.root / name
        path.write_text("\n".join(lines) + "\n")
        return path

    def test_a_pinned_archive_lands_as_the_files_it_names(self):
        members = {"deqp-vk": DEQP, "vulkan/mustpass/main.txt": DATA}
        archive = self.zip_archive(members)
        result = run(FETCH, "--pins", self.pins(archive, members), "--into", self.into, "--cache", self.cache)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.into / "deqp-vk").read_bytes(), DEQP)
        self.assertEqual((self.into / "main.txt").read_bytes(), DATA)
        self.assertIn("2 file(s)", result.stdout)
        # The build is runnable, not just present.
        self.assertTrue((self.into / "deqp-vk").stat().st_mode & 0o111)
        # And --check now says the set on disk is the pinned one.
        checked = run(FETCH, "--pins", self.pins(archive, members), "--into", self.into, "--cache", self.cache,
                      "--check")
        self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)
        self.assertIn("2 pinned file(s)", checked.stdout)

    def test_a_tar_build_is_read_too(self):
        members = {"deqp-vk": DEQP}
        archive = self.tar_archive(members)
        result = run(FETCH, "--pins", self.pins(archive, members), "--into", self.into, "--cache", self.cache)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.into / "deqp-vk").read_bytes(), DEQP)

    def test_a_hash_that_disagrees_writes_nothing(self):
        members = {"deqp-vk": DEQP, "vulkan/mustpass/main.txt": DATA}
        archive = self.zip_archive(members)
        wrong_file = self.pins(archive, members, name="wrong_file.txt",
                               file_digests={"deqp-vk": "0" * 64})
        result = run(FETCH, "--pins", wrong_file, "--into", self.into, "--cache", self.cache)
        self.assertEqual(result.returncode, 1)
        self.assertIn("deqp-vk sha256", result.stderr)
        self.assertFalse(self.into.exists())

        wrong_archive = self.pins(archive, members, name="wrong_archive.txt", archive_digest="1" * 64)
        refused = run(FETCH, "--pins", wrong_archive, "--into", self.into, "--cache", self.cache)
        self.assertEqual(refused.returncode, 1)
        self.assertIn("sha256", refused.stderr)
        self.assertFalse(self.into.exists())
        # The archive that disagreed is not left in the cache to be trusted
        # next time; the one that verified stays, which is what a cache is
        # for.
        self.assertEqual(list(self.cache.glob("*" + "1" * 64 + "*")), [])
        self.assertEqual(len(list(self.cache.glob("*.zip"))), 1)

    def test_a_file_the_archive_does_not_hold_is_refused(self):
        archive = self.zip_archive({"deqp-vk": DEQP})
        pins = self.root / "absent.txt"
        pins.write_text(f"archive file://{archive} {digest(archive.read_bytes())}\n"
                        f"file gone.txt vulkan/gone.txt {digest(DATA)}\n")
        result = run(FETCH, "--pins", pins, "--into", self.into, "--cache", self.cache)
        self.assertEqual(result.returncode, 1)
        self.assertIn("is not in the CTS archive", result.stderr)

    def test_check_says_what_is_missing_or_changed(self):
        members = {"deqp-vk": DEQP}
        archive = self.zip_archive(members)
        pins = self.pins(archive, members)
        missing = run(FETCH, "--pins", pins, "--into", self.into, "--cache", self.cache, "--check")
        self.assertEqual(missing.returncode, 1)
        self.assertIn("deqp-vk: missing", missing.stderr)
        self.into.mkdir(parents=True)
        (self.into / "deqp-vk").write_bytes(b"something else")
        changed = run(FETCH, "--pins", pins, "--into", self.into, "--cache", self.cache, "--check")
        self.assertEqual(changed.returncode, 1)
        self.assertIn("deqp-vk: sha256", changed.stderr)

    def test_pins_that_are_not_pins_are_refused_before_anything_is_fetched(self):
        cases = {
            "no archive": "file deqp-vk deqp-vk " + "0" * 64 + "\n",
            "no file": f"archive file:///nowhere.zip {'0' * 64}\n",
            "a line that is neither": "something else entirely\n",
            "a short hash": f"archive file:///nowhere.zip 00\nfile deqp-vk deqp-vk {'0' * 64}\n",
        }
        for name, text in cases.items():
            with self.subTest(name):
                pins = self.root / "bad.txt"
                pins.write_text(text)
                result = run(FETCH, "--pins", pins, "--into", self.into, "--cache", self.cache)
                self.assertEqual(result.returncode, 1)
                self.assertFalse(self.into.exists())
        no_file = run(FETCH, "--pins", self.root / "nowhere.txt", "--into", self.into, "--cache", self.cache)
        self.assertEqual(no_file.returncode, 1)
        self.assertIn("no pins", no_file.stderr)

    def test_an_unreachable_archive_is_refused_rather_than_cached(self):
        pins = self.root / "gone.txt"
        pins.write_text(f"archive file://{self.root}/nowhere.zip {'0' * 64}\n"
                        f"file deqp-vk deqp-vk {'0' * 64}\n")
        with self.assertRaises(Refused):
            CtsFetch(CtsPins(pins).read(), self.into, self.cache).fetch()
        self.assertEqual(list(self.cache.glob("*")) if self.cache.exists() else [], [])

    def test_this_repositorys_own_pins_are_empty_and_say_so(self):
        """The CTS has never been downloaded here, so --check refuses and names
        the file rather than reporting a build nobody has."""
        result = run(FETCH, "--check")
        self.assertEqual(result.returncode, 1)
        self.assertIn("no archive pin", result.stderr)
        text = (ROOT / "tools" / "cts_pins.txt").read_text()
        self.assertIn("EMPTY ON PURPOSE", text)
        self.assertIn("tools/fetch_cts.py", text)


if __name__ == "__main__":
    unittest.main()
