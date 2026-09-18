"""Whether a captured frame is the one committed (feature 048, US2).

What the runner captures is the device's scanout, so these are PPMs; what is
judged is every channel of every pixel within the step a GPU may round by, and
what a failure leaves behind is an image a person can look at.
"""
import pathlib
import sys
import tempfile
import unittest

from tool_run import ROOT  # noqa: F401

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from frame_reference import FrameReference, PpmImage  # noqa: E402


def ppm(width, height, pixels, maxval=255, magic=b"P6"):
    return magic + f"\n{width} {height}\n{maxval}\n".encode() + bytes(pixels)


class Frames(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)

    def image(self, name, raw):
        path = self.root / name
        path.write_bytes(raw)
        return path

    def solid(self, name, colour, width=4, height=2):
        return self.image(name, ppm(width, height, list(colour) * width * height))

    def test_a_frame_the_same_as_its_reference_differs_nowhere(self):
        captured = self.solid("captured.ppm", (64, 128, 191))
        reference = self.solid("reference.ppm", (64, 128, 191))
        frame = FrameReference(captured, reference, 0)
        self.assertIsNone(frame.unreadable())
        self.assertIsNone(frame.mismatched_size())
        self.assertEqual(frame.differs(), 0)

    def test_rounding_within_the_tolerance_is_the_same_frame(self):
        captured = self.solid("captured.ppm", (65, 127, 191))
        reference = self.solid("reference.ppm", (64, 128, 191))
        self.assertEqual(FrameReference(captured, reference, 1).differs(), 0)
        # And a step further is not.
        self.assertEqual(FrameReference(captured, reference, 0).differs(), 8)

    def test_one_pixel_out_is_one_pixel_counted_whatever_channel_moved(self):
        for channel in range(3):
            pixels = [64, 128, 191] * 8
            pixels[channel] = pixels[channel] + 40
            captured = self.image(f"captured{channel}.ppm", ppm(4, 2, pixels))
            reference = self.solid("reference.ppm", (64, 128, 191))
            self.assertEqual(FrameReference(captured, reference, 2).differs(), 1)

    def test_a_mismatch_leaves_an_image_a_person_can_look_at(self):
        pixels = [64, 128, 191] * 8
        pixels[0:3] = [255, 255, 255]
        captured = self.image("captured.ppm", ppm(4, 2, pixels))
        reference = self.solid("reference.ppm", (64, 128, 191))
        frame = FrameReference(captured, reference, 1)
        self.assertEqual(frame.differs(), 1)
        difference = frame.write_difference(self.root / "difference.ppm")
        picked = PpmImage(difference)
        self.assertEqual(picked.size(), (4, 2))
        # The pixel that moved is picked out; the rest is the frame, dimmed.
        self.assertEqual(tuple(picked.pixels[0:3]), (255, 0, 255))
        self.assertEqual(tuple(picked.pixels[3:6]), (64 // 3, 128 // 3, 191 // 3))

    def test_two_frames_of_different_sizes_are_named_rather_than_compared(self):
        captured = self.solid("captured.ppm", (64, 128, 191), width=4, height=2)
        reference = self.solid("reference.ppm", (64, 128, 191), width=2, height=2)
        frame = FrameReference(captured, reference, 0)
        self.assertIn("4x2", frame.mismatched_size())
        self.assertIn("2x2", frame.mismatched_size())

    def test_what_is_not_an_image_says_so_rather_than_raising(self):
        """These files come from a QEMU that may have died halfway through
        writing one."""
        cases = {
            "empty": b"",
            "not a ppm": b"not an image at all",
            "the wrong magic": ppm(2, 2, [0] * 12, magic=b"P3"),
            "a maxval no screendump writes": ppm(2, 2, [0] * 12, maxval=65535),
            "a header with no size": b"P6\n255\n",
            "cut off mid-pixel": ppm(4, 4, [0] * 12),
            "a size that is not a number": b"P6\nwide tall\n255\n" + bytes(12),
            "no pixels at all": b"P6\n2 2\n255\n",
        }
        good = self.solid("reference.ppm", (0, 0, 0), width=2, height=2)
        for name, raw in cases.items():
            with self.subTest(name):
                broken = self.image("broken.ppm", raw)
                self.assertFalse(PpmImage(broken).valid())
                frame = FrameReference(broken, good, 0)
                self.assertIn("is not a P6 PPM", frame.unreadable())
        missing = FrameReference(self.root / "nowhere.ppm", good, 0)
        self.assertIn("is not a P6 PPM", missing.unreadable())

    def test_a_dump_with_trailing_bytes_is_read_to_its_size(self):
        """QEMU writes exactly the frame; a file with more in it is read to the
        size its header gave rather than refused."""
        captured = self.image("captured.ppm", ppm(2, 2, [7] * 12) + b"trailing")
        picked = PpmImage(captured)
        self.assertTrue(picked.valid())
        self.assertEqual(len(picked.pixels), 12)


if __name__ == "__main__":
    unittest.main()
