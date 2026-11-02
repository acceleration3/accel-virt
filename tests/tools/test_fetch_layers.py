import hashlib
import io
import pathlib
import tarfile
import tempfile
import textwrap
import unittest
import zipfile

from tool_run import ROOT, run

TOOL = ROOT / "tools" / "fetch_layers.py"
SOURCE = textwrap.dedent("""
    static const char * const required_device_extensions[] = {
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    };
""")


def digest_of(raw):
    return hashlib.sha256(raw).hexdigest()


def tar_gz(members):
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w:gz") as bundle:
        for name, content in members.items():
            info = tarfile.TarInfo(f"top/{name}")
            info.size = len(content)
            bundle.addfile(info, io.BytesIO(content))
    return raw.getvalue()


def zip_of(members):
    raw = io.BytesIO()
    with zipfile.ZipFile(raw, "w") as bundle:
        for name, content in members.items():
            bundle.writestr(f"top/{name}", content)
    return raw.getvalue()


class FetchLayers(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.scratch = pathlib.Path(self.directory.name)
        self.addCleanup(self.directory.cleanup)
        self.into = self.scratch / "into"
        self.cache = self.scratch / "cache"
        self.source_archive = self.scratch / "source.tar.gz"
        self.release_archive = self.scratch / "release.zip"
        self.source_raw = tar_gz({"libs/vkd3d/device.c": SOURCE.encode()})
        self.release_raw = zip_of({"x64/d3d12.dll": b"MZ the layer"})
        self.source_archive.write_bytes(self.source_raw)
        self.release_archive.write_bytes(self.release_raw)

    def pins(self, *, source_digest=None, release_digest=None, file_digest=None, extra=""):
        text = textwrap.dedent(f"""
            source {self.source_archive.as_uri()} {source_digest or digest_of(self.source_raw)}
            release {self.release_archive.as_uri()} {release_digest or digest_of(self.release_raw)}
            file d3d12.dll x64/d3d12.dll {file_digest or digest_of(b"MZ the layer")}
        """) + extra
        path = self.scratch / "pins.txt"
        path.write_text(text)
        return path

    def fetch(self, pins=None, *extra):
        return run(TOOL, "vkd3d-proton", "--pins", pins or self.pins(), "--into", self.into,
                   "--cache", self.cache, *extra)

    def test_the_source_tree_and_the_pinned_dll_land_where_they_are_read(self):
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.into / "d3d12.dll").read_bytes(), b"MZ the layer")
        self.assertIn("VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME",
                      (self.into / "source" / "libs" / "vkd3d" / "device.c").read_text())
        listing = run(ROOT / "tools" / "layer_requirements.py", self.into / "source", "--json")
        self.assertEqual(listing.returncode, 0, listing.stdout + listing.stderr)
        self.assertIn("VK_KHR_push_descriptor", listing.stdout)

    def test_check_says_what_is_missing_and_then_that_it_is_there(self):
        before = self.fetch(self.pins(), "--check")
        self.assertEqual(before.returncode, 1)
        self.assertIn("source: missing", before.stderr)
        self.assertIn("d3d12.dll: missing", before.stderr)
        self.assertEqual(self.fetch().returncode, 0)
        after = self.fetch(self.pins(), "--check")
        self.assertEqual(after.returncode, 0, after.stdout + after.stderr)
        self.assertIn("the pinned files are in", after.stdout)
        (self.into / "d3d12.dll").write_bytes(b"MZ something else")
        changed = self.fetch(self.pins(), "--check")
        self.assertEqual(changed.returncode, 1)
        self.assertIn("sha256", changed.stderr)

    def test_an_archive_whose_hash_disagrees_writes_nothing(self):
        wrong = "0" * 64
        result = self.fetch(self.pins(source_digest=wrong))
        self.assertEqual(result.returncode, 1)
        self.assertIn("want " + wrong, result.stderr)
        self.assertFalse((self.into / "source").exists())
        self.assertFalse((self.into / "d3d12.dll").exists())
        self.assertEqual(list(self.cache.glob("*source*")), [])

    def test_a_file_whose_hash_disagrees_writes_nothing_at_all(self):
        result = self.fetch(self.pins(file_digest="1" * 64))
        self.assertEqual(result.returncode, 1)
        self.assertIn("d3d12.dll sha256", result.stderr)
        self.assertFalse((self.into / "source").exists())

    def test_a_file_the_release_does_not_hold_is_named(self):
        pins = self.pins(extra="file dxgi.dll x64/dxgi.dll " + "2" * 64 + "\n")
        result = self.fetch(pins)
        self.assertEqual(result.returncode, 1)
        self.assertIn("x64/dxgi.dll is not in", result.stderr)

    def test_pins_that_are_not_pins_are_refused_by_line(self):
        path = self.scratch / "bad.txt"
        path.write_text("source only-a-url\n")
        result = self.fetch(path)
        self.assertEqual(result.returncode, 1)
        self.assertIn("bad.txt:1", result.stderr)
        path.write_text("nonsense a b\n")
        self.assertIn("nonsense is not a pin", self.fetch(path).stderr)
        path.write_text("# nothing but a comment\n")
        self.assertIn("no source pin", self.fetch(path).stderr)
        missing = self.scratch / "absent.txt"
        self.assertIn("no pins", self.fetch(missing).stderr)

    def test_a_url_that_is_not_an_archive_shape_is_refused(self):
        path = self.scratch / "shape.txt"
        path.write_text(f"source file:///nowhere/layer.7z {'3' * 64}\n")
        result = self.fetch(path)
        self.assertEqual(result.returncode, 1)
        self.assertIn("not an archive shape", result.stderr)

    def test_a_url_that_answers_nothing_is_refused_and_leaves_no_partial(self):
        path = self.scratch / "gone.txt"
        path.write_text(f"source {(self.scratch / 'absent.tar.gz').as_uri()} {'4' * 64}\n")
        result = self.fetch(path)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(list(self.cache.glob("*.part")), [])

    def test_a_second_fetch_uses_the_cached_archive(self):
        self.assertEqual(self.fetch().returncode, 0)
        cached = sorted(path.name for path in self.cache.iterdir())
        self.assertEqual(len(cached), 2, cached)
        self.source_archive.unlink()
        self.release_archive.unlink()
        again = self.fetch()
        self.assertEqual(again.returncode, 0, again.stdout + again.stderr)


if __name__ == "__main__":
    unittest.main()
