"""The Khronos registries arrive pinned and verified, or they do not arrive.

A registry that changed under a regeneration is a wire change, so the fetch is
held to the same standard as QEMU's and the Windows kit's: a revision, a hash,
and nothing unpacked when they disagree. The tests drive the tool against
local archives through `file://`, which is why the tool takes its sources as
data rather than baking `https://` into the download path.
"""
import hashlib
import io
import pathlib
import shutil
import tarfile
import tempfile
import unittest

from tool_run import ROOT, run

FETCH = ROOT / "tools" / "fetch_khronos_registries.py"


def archive(root, files):
    """A .tar.gz laid out the way GitHub's source archives are: one top
    directory, everything under it."""
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w:gz") as tar:
        for name, text in files.items():
            data = text.encode()
            info = tarfile.TarInfo(f"{root}/{name}")
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))
    return raw.getvalue()


class FetchKhronosRegistries(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.sources = self.scratch / "sources"
        self.sources.mkdir()
        self.tree = self.scratch / "khronos"
        self.archives = {}
        for name, root, files in (
            ("vulkan", "Vulkan-Headers-1.2.3",
             {"registry/vk.xml": "<registry><vulkan/></registry>\n",
              "include/vulkan/vulkan_core.h": "#define VK_HEADER 1\n",
              "include/vulkan/vk_platform.h": "#define VK_PLATFORM 1\n"}),
            ("opengl", "OpenGL-Registry-abc",
             {"xml/gl.xml": "<registry><gl/></registry>\n",
              # The guest is Windows, so the WGL entry points come too.
              "xml/wgl.xml": "<registry><wgl/></registry>\n"}),
            ("opencl", "OpenCL-Docs-3.0",
             {"xml/cl.xml": "<registry><cl/></registry>\n"}),
        ):
            body = archive(root, files)
            path = self.sources / f"{name}.tar.gz"
            path.write_bytes(body)
            self.archives[name] = (path, hashlib.sha256(body).hexdigest())

    def pins(self, **overrides):
        """The pin file the tool reads: one line per registry, as data, so a
        test can point it at a local archive."""
        lines = []
        for name, (path, digest) in self.archives.items():
            digest = overrides.get(name, digest)
            lines.append(f"{name} {path.as_uri()} {digest}")
        pins = self.scratch / "pins.txt"
        pins.write_text("\n".join(lines) + "\n")
        return pins

    def fetch(self, pins=None, **overrides):
        """Its own cache, so one test's download is not another's answer."""
        return run(FETCH, "--pins", pins or self.pins(**overrides), "--into", self.tree,
                   "--cache", self.scratch / "cache")

    def test_a_clean_tree_gets_every_registry_and_a_marker(self):
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.tree / "vk.xml").read_text(), "<registry><vulkan/></registry>\n")
        self.assertEqual((self.tree / "gl.xml").read_text(), "<registry><gl/></registry>\n")
        self.assertEqual((self.tree / "wgl.xml").read_text(), "<registry><wgl/></registry>\n")
        self.assertEqual((self.tree / "cl.xml").read_text(), "<registry><cl/></registry>\n")
        self.assertEqual((self.tree / "include" / "vulkan" / "vulkan_core.h").read_text(),
                         "#define VK_HEADER 1\n")
        self.assertTrue((self.tree / "VERSION").is_file())
        self.assertIn("vk.xml", result.stdout)

    def test_a_second_fetch_says_it_has_them_already(self):
        self.assertEqual(self.fetch().returncode, 0)
        marked = (self.tree / "VERSION").read_text()
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("already", result.stdout)
        self.assertEqual((self.tree / "VERSION").read_text(), marked)

    def test_a_hash_that_does_not_match_unpacks_nothing_and_names_the_registry(self):
        result = self.fetch(opengl="0" * 64)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("opengl", result.stderr)
        self.assertIn("sha256", result.stderr)
        self.assertFalse(self.tree.exists())

    def test_a_source_that_is_not_there_leaves_a_verified_tree_alone(self):
        self.assertEqual(self.fetch().returncode, 0)
        before = (self.tree / "vk.xml").read_text()
        missing = self.scratch / "pins-missing.txt"
        gone = (self.sources / "nowhere.tar.gz").as_uri()
        missing.write_text(f"vulkan {gone} {'1' * 64}\n")
        result = self.fetch(pins=missing)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("vulkan", result.stderr)
        self.assertEqual((self.tree / "vk.xml").read_text(), before)

    def test_an_archive_without_the_file_it_promised_is_refused(self):
        empty = archive("Vulkan-Headers-1.2.3", {"README": "nothing here\n"})
        path = self.sources / "vulkan.tar.gz"
        path.write_bytes(empty)
        result = self.fetch(vulkan=hashlib.sha256(empty).hexdigest())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("vk.xml", result.stderr)
        self.assertFalse(self.tree.exists())

    def test_the_repository_s_own_pins_name_a_revision_and_a_hash(self):
        """What ships: three registries and the headers one of them needs, each
        with a full-length hash and a URL that names a tag or a revision rather
        than a branch."""
        pins = (ROOT / "tools" / "khronos_pins.txt").read_text().split("\n")
        named = [line.split() for line in pins if line.strip() and not line.startswith("#")]
        self.assertEqual({entry[0] for entry in named},
                         {"vulkan", "opengl", "opencl", "opencl_headers"})
        for name, url, digest in named:
            self.assertEqual(len(digest), 64, name)
            self.assertTrue(url.startswith("https://"), name)
            self.assertNotIn("/heads/", url, f"{name} names a branch, which moves under a pin")
