"""The Vulkan report's picture verdict (tools/vulkan_report.py): a sample whose
two runs differ by its own doing (NOTES) is a warning when its pictures
differ, and every other sample is judged as d3d11_report judges it."""
import pathlib
import sys
import unittest

from tool_run import ROOT  # noqa: F401

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

import vulkan_report  # noqa: E402

DIFFERENT = {"max": 255, "mean": 40.0, "changed": 30.0, "psnr": 12.0}
CLOSE = {"max": 20, "mean": 0.5, "changed": 3.0, "psnr": 33.0}
SAME = {"max": 0, "mean": 0.0, "changed": 0.0, "psnr": float("inf")}


class PictureVerdict(unittest.TestCase):
    def test_a_noted_sample_that_differs_varies(self):
        for name in vulkan_report.NOTES:
            self.assertEqual(vulkan_report.picture_verdict({"name": name, "stats": DIFFERENT}), ("varies", "warn"))

    def test_a_noted_sample_is_otherwise_judged_as_any(self):
        self.assertEqual(vulkan_report.picture_verdict({"name": "computeparticles", "stats": SAME}),
                         ("identical", "good"))
        self.assertEqual(vulkan_report.picture_verdict({"name": "computeparticles", "stats": CLOSE}),
                         ("close", "warn"))

    def test_any_other_sample_that_differs_fails(self):
        self.assertEqual(vulkan_report.picture_verdict({"name": "texture", "stats": DIFFERENT}), ("differs", "bad"))

    def test_a_missing_picture_fails_even_for_a_noted_sample(self):
        self.assertEqual(vulkan_report.picture_verdict({"name": "computeparticles", "stats": None}),
                         ("missing", "bad"))


if __name__ == "__main__":
    unittest.main()
