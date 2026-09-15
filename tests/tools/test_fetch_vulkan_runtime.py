"""The Vulkan runtime arrives pinned and verified, or nothing of it arrives.

Driven against a local archive through `file://`: the tool takes its source as
data, so no test touches the network.
"""
import hashlib
import io
import pathlib
import shutil
import tempfile
import unittest
import zipfile

from tool_run import ROOT, run

FETCH = ROOT / "tools" / "fetch_vulkan_runtime.py"
TOP = "VulkanRT-X64-9.9.9.0-Components"
LOADER = b"x64 loader\n"
VULKANINFO = b"x64 vulkaninfo\n"


def sha256(raw):
    return hashlib.sha256(raw).hexdigest()


def bundle(members):
    """A zip laid out as LunarG's is: one top directory, x64 and x86 under it."""
    raw = io.BytesIO()
    with zipfile.ZipFile(raw, "w") as archive:
        archive.writestr(f"{TOP}/", b"")
        for name, content in members.items():
            archive.writestr(f"{TOP}/{name}", content)
    return raw.getvalue()


FULL = {"VulkanRT-License.txt": b"MIT\n", "x86/vulkan-1.dll": b"x86 loader\n",
        "x86/vulkaninfo.exe": b"x86 vulkaninfo\n", "x64/vulkan-1.dll": LOADER,
        "x64/vulkaninfo.exe": VULKANINFO}


class FetchVulkanRuntime(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.source = self.scratch / "runtime.zip"
        self.tree = self.scratch / "third_party" / "vulkan-runtime"
        self.cache = self.scratch / "cache"
        self.use_archive(bundle(FULL))

    def use_archive(self, raw):
        self.source.write_bytes(raw)
        self.archive_digest = sha256(raw)

    def pins(self, archive=None, loader=None, vulkaninfo=None, url=None):
        pins = self.scratch / "pins.txt"
        pins.write_text(f"# local\narchive {url or self.source.as_uri()} {archive or self.archive_digest}\n"
                        f"vulkan-1.dll x64/vulkan-1.dll {loader or sha256(LOADER)}\n"
                        f"vulkaninfo.exe x64/vulkaninfo.exe {vulkaninfo or sha256(VULKANINFO)}\n")
        return pins

    def fetch(self, *extra, pins=None):
        return run(FETCH, "--pins", pins or self.pins(), "--into", self.tree, "--cache", self.cache, *extra)

    def assert_nothing_written(self):
        self.assertFalse(self.tree.exists() and any(self.tree.iterdir()),
                         list(self.tree.iterdir()) if self.tree.exists() else None)

    def test_a_clean_fetch_extracts_the_x64_loader_and_vulkaninfo(self):
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.tree / "vulkan-1.dll").read_bytes(), LOADER)
        self.assertEqual((self.tree / "vulkaninfo.exe").read_bytes(), VULKANINFO)
        self.assertEqual(sorted(path.name for path in self.tree.iterdir()),
                         ["vulkan-1.dll", "vulkaninfo.exe"])
        self.assertIn(str(self.tree / "vulkan-1.dll"), result.stdout)

    def test_check_reports_each_pinned_file_and_exits_by_whether_all_are_there(self):
        result = self.fetch("--check")
        self.assertEqual(result.returncode, 1)
        self.assertIn(f"{self.tree / 'vulkan-1.dll'}: missing", result.stderr)
        self.assertIn(f"{self.tree / 'vulkaninfo.exe'}: missing", result.stderr)
        self.assert_nothing_written()

        self.assertEqual(self.fetch().returncode, 0)
        result = self.fetch("--check")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(f"{self.tree / 'vulkan-1.dll'}: ok", result.stdout)
        self.assertIn(f"{self.tree / 'vulkaninfo.exe'}: ok", result.stdout)

    def test_check_refuses_a_file_that_is_not_the_pinned_one_naming_both_hashes(self):
        self.assertEqual(self.fetch().returncode, 0)
        tampered = b"tampered\n"
        (self.tree / "vulkaninfo.exe").write_bytes(tampered)
        result = self.fetch("--check")
        self.assertEqual(result.returncode, 1)
        self.assertIn(f"{self.tree / 'vulkaninfo.exe'}: sha256 {sha256(tampered)}, "
                      f"want {sha256(VULKANINFO)}", result.stderr)
        self.assertIn(f"{self.tree / 'vulkan-1.dll'}: ok", result.stdout)

    def test_a_second_fetch_has_them_already_and_does_not_download(self):
        self.assertEqual(self.fetch().returncode, 0)
        shutil.rmtree(self.cache)
        self.source.unlink()
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("already holds every pinned file", result.stdout)

    def test_a_fetch_after_the_tree_is_gone_takes_the_archive_from_the_cache(self):
        self.assertEqual(self.fetch().returncode, 0)
        shutil.rmtree(self.tree)
        self.source.unlink()
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.tree / "vulkaninfo.exe").read_bytes(), VULKANINFO)

    def test_a_tampered_file_is_fetched_again(self):
        self.assertEqual(self.fetch().returncode, 0)
        (self.tree / "vulkan-1.dll").write_bytes(b"tampered\n")
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("already", result.stdout)
        self.assertEqual((self.tree / "vulkan-1.dll").read_bytes(), LOADER)

    def test_an_archive_hash_mismatch_writes_nothing_and_drops_the_cached_archive(self):
        result = self.fetch(pins=self.pins(archive="0" * 64))
        self.assertEqual(result.returncode, 1)
        self.assertIn(f"archive sha256 {self.archive_digest}, want {'0' * 64}", result.stderr)
        self.assert_nothing_written()
        self.assertEqual(list(self.cache.iterdir()), [])

    def test_a_file_hash_mismatch_writes_nothing_not_even_the_files_that_matched(self):
        result = self.fetch(pins=self.pins(vulkaninfo="1" * 64))
        self.assertEqual(result.returncode, 1)
        self.assertIn(f"vulkaninfo.exe: sha256 {sha256(VULKANINFO)}, want {'1' * 64}", result.stderr)
        self.assert_nothing_written()

    def test_an_archive_without_a_pinned_member_writes_nothing(self):
        self.use_archive(bundle({name: content for name, content in FULL.items()
                                 if name != "x64/vulkaninfo.exe"}))
        result = self.fetch()
        self.assertEqual(result.returncode, 1)
        self.assertIn("the archive holds no x64/vulkaninfo.exe for vulkaninfo.exe", result.stderr)
        self.assert_nothing_written()

    def test_an_archive_that_is_not_a_zip_is_refused(self):
        self.use_archive(b"not a zip at all\n")
        result = self.fetch()
        self.assertEqual(result.returncode, 1)
        self.assertIn("the archive is not a zip this can read", result.stderr)
        self.assert_nothing_written()

    def test_a_source_that_is_not_there_is_refused_naming_it(self):
        gone = (self.scratch / "nowhere.zip").as_uri()
        result = self.fetch(pins=self.pins(url=gone))
        self.assertEqual(result.returncode, 1)
        self.assertIn(gone, result.stderr)
        self.assert_nothing_written()
        self.assertEqual(list(self.cache.iterdir()), [])

    def test_pins_that_leave_out_a_file_or_name_another_are_refused(self):
        pins = self.scratch / "short.txt"
        pins.write_text(f"archive {self.source.as_uri()} {self.archive_digest}\n"
                        f"vulkan-1.dll x64/vulkan-1.dll {sha256(LOADER)}\n")
        result = self.fetch(pins=pins)
        self.assertEqual(result.returncode, 1)
        self.assertIn("no pin for vulkaninfo.exe", result.stderr)
        pins.write_text(self.pins().read_text() + f"vulkan-1.pdb x64/vulkan-1.pdb {'2' * 64}\n")
        result = self.fetch(pins=pins)
        self.assertEqual(result.returncode, 1)
        self.assertIn("vulkan-1.pdb is not a file this fetches", result.stderr)
        pins.write_text(f"vulkan-1.dll x64/vulkan-1.dll {sha256(LOADER)}\n"
                        f"vulkaninfo.exe x64/vulkaninfo.exe {sha256(VULKANINFO)}\n")
        result = self.fetch(pins=pins)
        self.assertEqual(result.returncode, 1)
        self.assertIn("no archive pin", result.stderr)
        pins.write_text("archive only-two-fields\n")
        result = self.fetch(pins=pins)
        self.assertEqual(result.returncode, 1)
        self.assertIn("want '<name> <source> <sha256>'", result.stderr)

    def test_the_repository_s_pins_name_a_version_and_the_x64_members(self):
        text = (ROOT / "tools" / "vulkan_runtime_pins.txt").read_text()
        pins = {line.split()[0]: line.split()[1:] for line in text.splitlines()
                if line.strip() and not line.startswith("#")}
        self.assertEqual(set(pins), {"archive", "vulkan-1.dll", "vulkaninfo.exe"})
        url, _digest = pins["archive"]
        self.assertTrue(url.startswith("https://sdk.lunarg.com/sdk/download/"), url)
        self.assertRegex(url, r"/\d+\.\d+\.\d+\.\d+/windows/VulkanRT-X64-\d+\.\d+\.\d+\.\d+-Components\.zip$")
        self.assertNotIn("latest", url)
        self.assertEqual(pins["vulkan-1.dll"][0], "x64/vulkan-1.dll")
        self.assertEqual(pins["vulkaninfo.exe"][0], "x64/vulkaninfo.exe")
        for name, (_source, digest) in pins.items():
            self.assertRegex(digest, r"^[0-9a-f]{64}$", name)


if __name__ == "__main__":
    unittest.main()
