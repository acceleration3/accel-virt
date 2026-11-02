import hashlib
import io
import pathlib
import shutil
import tarfile
import tempfile
import unittest

from tool_run import ROOT, run

FETCH = ROOT / "tools" / "fetch_khronos_registries.py"

VIDEO_HEADERS = (
    "vulkan_video_codec_av1std.h", "vulkan_video_codec_av1std_decode.h",
    "vulkan_video_codec_av1std_encode.h", "vulkan_video_codec_h264std.h",
    "vulkan_video_codec_h264std_decode.h", "vulkan_video_codec_h264std_encode.h",
    "vulkan_video_codec_h265std.h", "vulkan_video_codec_h265std_decode.h",
    "vulkan_video_codec_h265std_encode.h", "vulkan_video_codecs_common.h")


def archive(root, files):
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
              "include/vulkan/vk_platform.h": "#define VK_PLATFORM 1\n",
              **{f"include/vk_video/{name}": f"// {name}\n" for name in VIDEO_HEADERS}}),
            ("opengl", "OpenGL-Registry-abc",
             {"xml/gl.xml": "<registry><gl/></registry>\n",
              "xml/wgl.xml": "<registry><wgl/></registry>\n"}),
            ("opencl", "OpenCL-Docs-3.0",
             {"xml/cl.xml": "<registry><cl/></registry>\n"}),
        ):
            body = archive(root, files)
            path = self.sources / f"{name}.tar.gz"
            path.write_bytes(body)
            self.archives[name] = (path, hashlib.sha256(body).hexdigest())

    def pins(self, **overrides):
        lines = []
        for name, (path, digest) in self.archives.items():
            digest = overrides.get(name, digest)
            lines.append(f"{name} {path.as_uri()} {digest}")
        pins = self.scratch / "pins.txt"
        pins.write_text("\n".join(lines) + "\n")
        return pins

    def fetch(self, pins=None, **overrides):
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

    def test_the_video_headers_vulkan_core_includes_come_with_it(self):
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        for name in VIDEO_HEADERS:
            self.assertEqual((self.tree / "include" / "vk_video" / name).read_text(),
                             f"// {name}\n")

    def test_a_vulkan_archive_without_a_video_header_is_refused(self):
        files = {"registry/vk.xml": "<registry/>\n",
                 "include/vulkan/vulkan_core.h": "\n", "include/vulkan/vk_platform.h": "\n",
                 **{f"include/vk_video/{name}": "\n" for name in VIDEO_HEADERS[1:]}}
        body = archive("Vulkan-Headers-1.2.3", files)
        (self.sources / "vulkan.tar.gz").write_bytes(body)
        result = self.fetch(vulkan=hashlib.sha256(body).hexdigest())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(VIDEO_HEADERS[0], result.stderr)
        self.assertFalse(self.tree.exists())

    def test_a_tree_fetched_before_a_file_was_wanted_is_fetched_again(self):
        self.assertEqual(self.fetch().returncode, 0)
        shutil.rmtree(self.tree / "include" / "vk_video")
        pins = [line.split() for line in self.pins().read_text().splitlines()]
        (self.tree / "VERSION").write_text("".join(f"{name} {digest}\n"
                                                   for name, _url, digest in sorted(pins)))
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("already", result.stdout)
        self.assertTrue((self.tree / "include" / "vk_video" / VIDEO_HEADERS[0]).is_file())

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
        pins = (ROOT / "tools" / "khronos_pins.txt").read_text().split("\n")
        named = [line.split() for line in pins if line.strip() and not line.startswith("#")]
        self.assertEqual({entry[0] for entry in named},
                         {"vulkan", "opengl", "opencl", "opencl_headers"})
        for name, url, digest in named:
            self.assertEqual(len(digest), 64, name)
            self.assertTrue(url.startswith("https://"), name)
            self.assertNotIn("/heads/", url, f"{name} names a branch, which moves under a pin")
