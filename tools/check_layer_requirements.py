#!/usr/bin/env python3
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
NOT_A_DRIVER_S = ("VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_utils", "VK_EXT_debug_report",
                  "VK_KHR_display", "VK_KHR_xlib_surface", "VK_KHR_xcb_surface", "VK_KHR_wayland_surface",
                  "VK_KHR_get_surface_capabilities2", "VK_KHR_surface_protected_capabilities",
                  "VK_KHR_portability_enumeration")


class ReportedExtensions:
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
    def __init__(self, root, layer):
        self.root = root
        self.layer = layer
        self.notes_ = []

    def notes(self):
        return self.notes_

    def source(self):
        return os.path.join(self.root, "third_party", self.layer, "source")

    def required(self):
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
