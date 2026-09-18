#!/usr/bin/env python3
"""Gate for feature 047's FR-001: every extension a pinned translation layer
says it requires is one this ICD reports.

The layer's list is read out of its own pinned source
(tools/layer_requirements.py), never copied here, so a release that starts
requiring one more fails this rather than failing in a game. What the ICD
reports is read out of its policy table, the same table
tools/check_extension_policy.py holds to a test each - so the two halves of the
promise are checked against each other and neither against a list kept by hand.

A layer whose pins are not fetched cannot be checked, and this says so and
passes, the way the gate skips the guest when no image is named. What it never
does is pass quietly on an empty list: a layer directory that is there but holds
no requirements at all is a failure, because that is what a broken extractor
looks like.

  check_layer_requirements.py [repository root]
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LAYERS = ("dxvk", "vkd3d-proton")
POLICY_TABLE = os.path.join("src", "icd", "vulkan", "icd_root.cpp")
DEVICE_ROWS = re.compile(r"ExtensionPolicy::Row\s+reported_extensions\s*\[\s*\]\s*=\s*\{(?P<rows>.*?)\}\s*;", re.S)
INSTANCE_ROWS = re.compile(r"InstanceExtension\s+reported_instance_extensions\s*\[\s*\]\s*=\s*\{(?P<rows>.*?)\}\s*;",
                           re.S)
NAME = re.compile(r'\{\s*"(?P<name>[^"]+)"')
# What the ICD answers itself rather than reporting, and what a layer requires
# of the loader rather than of a driver: neither is a policy row, and neither
# is missing when it is absent from one.
NOT_A_DRIVER_S = ("VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_utils", "VK_EXT_debug_report",
                  "VK_KHR_display", "VK_KHR_xlib_surface", "VK_KHR_xcb_surface", "VK_KHR_wayland_surface",
                  "VK_KHR_get_surface_capabilities2", "VK_KHR_surface_protected_capabilities",
                  "VK_KHR_portability_enumeration")


class ReportedExtensions:
    """What the Vulkan ICD's policy tables report, by name."""

    def __init__(self, root):
        self.root = root

    def names(self):
        with open(os.path.join(self.root, POLICY_TABLE), encoding="utf-8") as source:
            text = source.read()
        found = set()
        for pattern in (DEVICE_ROWS, INSTANCE_ROWS):
            table = pattern.search(text)
            if table is None:
                continue
            found.update(NAME.findall(table.group("rows")))
        return found


class LayerVerdict:
    """One layer: what it requires, and what of that the ICD does not report."""

    def __init__(self, root, layer):
        self.root = root
        self.layer = layer
        self.notes_ = []

    def notes(self):
        return self.notes_

    def source(self):
        return os.path.join(self.root, "third_party", self.layer, "source")

    def required(self):
        """The layer's own required list, or None when its pins are not
        fetched."""
        source = self.source()
        if not os.path.isdir(source):
            return None
        tool = os.path.join(self.root, "tools", "layer_requirements.py")
        answered = subprocess.run([sys.executable, "-B", tool, source, "--json"], capture_output=True, text=True,
                                  check=False)
        if answered.returncode != 0:
            self.notes_.append(f"{self.layer}: its own list could not be read: {answered.stderr.strip()[-200:]}")
            return []
        return json.loads(answered.stdout).get("required", [])

    def violations(self, reported):
        required = self.required()
        if required is None:
            self.notes_.append(f"{self.layer}: skipped, {self.source()} is not fetched "
                               f"(tools/fetch_layers.py {self.layer})")
            return []
        if not required:
            return [f"{self.layer}: its source is fetched but names no required extension at all"]
        missing = [name for name in required if name not in reported and name not in NOT_A_DRIVER_S]
        self.notes_.append(f"{self.layer}: {len(required)} required, {len(missing)} not reported")
        return [f"{self.layer} requires {name}, which this ICD does not report" for name in missing]


def main(argv):
    root = argv[1] if len(argv) > 1 else ROOT
    reported = ReportedExtensions(root).names()
    if not reported:
        print("layer_requirements: no policy table in " + POLICY_TABLE, file=sys.stderr)
        return 2
    violations = []
    for layer in LAYERS:
        verdict = LayerVerdict(root, layer)
        violations += verdict.violations(reported)
        for note in verdict.notes():
            print(f"layer_requirements: {note}")
    for violation in violations:
        print(f"layer_requirements: {violation}", file=sys.stderr)
    if violations:
        return 1
    print("layer_requirements: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
