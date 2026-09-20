"""The OpenCL ICD loader's source arrives pinned and verified, or none of it does.

Driven against a local archive through `file://`: the tool takes its source as
data, so no test touches the network.
"""
import hashlib
import io
import pathlib
import re
import shutil
import tarfile
import tempfile
import unittest

from tool_run import ROOT, run

FETCH = ROOT / "tools" / "fetch_opencl_loader.py"
PINS = ROOT / "tools" / "opencl_loader_pins.txt"
TOP = "OpenCL-ICD-Loader-9.9.9"
# The list the tool refuses an archive for not holding.
WANTED = (
    "loader/icd.c",
    "loader/icd.h",
    "loader/icd_dispatch.c",
    "loader/icd_dispatch.h",
    "loader/icd_dispatch_generated.c",
    "loader/icd_envvars.h",
    "loader/icd_platform.h",
    "loader/icd_version.h",
    "loader/windows/adapter.h",
    "loader/windows/icd_windows.c",
    "loader/windows/icd_windows.h",
    "loader/windows/icd_windows_apppackage.c",
    "loader/windows/icd_windows_apppackage.h",
    "loader/windows/icd_windows_dxgk.c",
    "loader/windows/icd_windows_dxgk.h",
    "loader/windows/icd_windows_envvars.c",
    "loader/windows/icd_windows_hkr.c",
    "loader/windows/icd_windows_hkr.h",
    "loader/windows/OpenCL.def",
)
# Two the archive carries and the tool leaves behind: a separate tool, and the
# platform half this does not build.
UNWANTED = ("loader/cllayerinfo.c", "loader/linux/icd_linux.c")


def sha256(raw):
    return hashlib.sha256(raw).hexdigest()


def bundle(names=WANTED, extra=UNWANTED):
    """A tar.gz laid out as GitHub's source archives are: one top directory."""
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w:gz") as archive:
        for name in tuple(names) + tuple(extra):
            content = f"/* {name} */\n".encode()
            info = tarfile.TarInfo(f"{TOP}/{name}")
            info.size = len(content)
            archive.addfile(info, io.BytesIO(content))
    return raw.getvalue()


class FetchOpenClLoader(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.source = self.scratch / "loader.tar.gz"
        self.tree = self.scratch / "third_party" / "opencl-loader"
        self.cache = self.scratch / "cache"
        self.use_archive(bundle())

    def use_archive(self, raw):
        self.source.write_bytes(raw)
        self.archive_digest = sha256(raw)

    def pins(self, digest=None, url=None, text=None):
        pins = self.scratch / "pins.txt"
        pins.write_text(text if text is not None else
                        f"# local\nloader {url or self.source.as_uri()} {digest or self.archive_digest}\n")
        return pins

    def fetch(self, *extra, pins=None):
        return run(FETCH, "--pins", pins or self.pins(), "--into", self.tree, "--cache", self.cache, *extra)

    def assert_nothing_written(self):
        self.assertFalse(self.tree.exists() and any(self.tree.rglob("*")),
                         list(self.tree.rglob("*")) if self.tree.exists() else None)

    def test_a_clean_fetch_writes_every_wanted_file_and_nothing_else(self):
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        for name in WANTED:
            self.assertEqual((self.tree / name).read_bytes(), f"/* {name} */\n".encode())
        for name in UNWANTED:
            self.assertFalse((self.tree / name).exists(), name)
        # The config the loader's own CMake would have written.
        self.assertIn("HAVE_SECURE_GETENV", (self.tree / "loader" / "icd_cmake_config.h").read_text())
        self.assertEqual((self.tree / "PINNED").read_text().strip(), self.archive_digest)

    def test_check_passes_on_a_fetched_tree_and_fails_on_an_empty_one(self):
        self.assertEqual(self.fetch("--check").returncode, 1)
        self.fetch()
        result = self.fetch("--check")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ok", result.stdout)

    def test_check_names_the_file_that_went_missing(self):
        self.fetch()
        (self.tree / "loader" / "icd.c").unlink()
        result = self.fetch("--check")
        self.assertEqual(result.returncode, 1)
        self.assertIn("loader/icd.c", result.stderr)
        self.assertIn("missing", result.stderr)

    def test_a_tree_written_from_another_pin_is_refetched(self):
        self.fetch()
        (self.tree / "PINNED").write_text("0" * 64 + "\n")
        check = self.fetch("--check")
        self.assertEqual(check.returncode, 1)
        self.assertIn("want " + self.archive_digest, check.stderr)
        self.assertEqual(self.fetch().returncode, 0)
        self.assertEqual((self.tree / "PINNED").read_text().strip(), self.archive_digest)

    def test_a_second_fetch_downloads_nothing(self):
        self.fetch()
        self.source.unlink()
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("already holds", result.stdout)

    def test_a_deleted_tree_is_rebuilt_from_the_cache(self):
        self.fetch()
        shutil.rmtree(self.tree)
        self.source.unlink()
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.tree / "loader" / "icd.c").is_file())

    def test_an_archive_whose_hash_disagrees_writes_nothing_and_empties_the_cache(self):
        result = self.fetch(pins=self.pins(digest="0" * 64))
        self.assertEqual(result.returncode, 1)
        self.assertIn("archive sha256", result.stderr)
        self.assert_nothing_written()
        self.assertFalse(any(self.cache.iterdir()) if self.cache.exists() else False)

    def test_an_archive_missing_one_file_writes_none_of_them(self):
        self.use_archive(bundle(names=[name for name in WANTED if name != "loader/icd.c"]))
        result = self.fetch()
        self.assertEqual(result.returncode, 1)
        self.assertIn("loader/icd.c", result.stderr)
        self.assert_nothing_written()

    def test_something_that_is_not_a_tar_is_refused(self):
        self.use_archive(b"not a tar at all")
        result = self.fetch()
        self.assertEqual(result.returncode, 1)
        self.assertIn("not a tar", result.stderr)
        self.assert_nothing_written()

    def test_a_source_that_is_not_there_is_refused(self):
        result = self.fetch(pins=self.pins(url=(self.scratch / "absent.tar.gz").as_uri()))
        self.assertEqual(result.returncode, 1)
        self.assert_nothing_written()

    def test_malformed_pins_are_each_named(self):
        for text, expected in (("loader one two three\n", "want '<name> <url> <sha256>'"),
                               ("registry file:///x 00\n", "is not a thing this fetches"),
                               ("# only a comment\n", "no loader pin"),
                               (f"loader {self.source.as_uri()} {self.archive_digest}\n"
                                f"loader {self.source.as_uri()} {self.archive_digest}\n",
                                "a second loader pin")):
            with self.subTest(text=text):
                result = self.fetch(pins=self.pins(text=text))
                self.assertEqual(result.returncode, 1)
                self.assertIn(expected, result.stderr)
                self.assert_nothing_written()

    def test_the_repository_s_pins_name_a_tag_and_a_real_digest(self):
        lines = [line.split() for line in PINS.read_text().splitlines()
                 if line.strip() and not line.startswith("#")]
        self.assertEqual(len(lines), 1, lines)
        name, url, digest = lines[0]
        self.assertEqual(name, "loader")
        self.assertTrue(url.startswith("https://github.com/KhronosGroup/OpenCL-ICD-Loader/archive/refs/tags/"), url)
        self.assertNotIn("main", url)
        self.assertRegex(url, r"/v\d{4}\.\d{2}\.\d{2}\.tar\.gz$")
        self.assertRegex(digest, r"^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
