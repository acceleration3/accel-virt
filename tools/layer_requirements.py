#!/usr/bin/env python3
"""What a pinned translation layer says it requires (feature 047, FR-001).

DXVK and vkd3d-proton each declare, in their own source, the Vulkan extensions
they cannot start without and the ones they merely prefer. FR-001 forbids
copying those lists into this repository by hand: they are read out of the
pinned release at test time, so a layer that starts requiring one more cannot
pass unnoticed.

Both declare them the same way in the end - a `VK_*_EXTENSION_NAME` macro
beside a word saying whether it is required - so one reader serves both:

  * a `VK_*_EXTENSION_NAME` on a line that also holds the word `Required`
    (DXVK's `DxvkExtMode::Required`) is required; `Optional` is optional;
  * otherwise the name belongs to the table it is inside, and a table whose
    own name holds `required` or `optional` says which (vkd3d-proton's
    `required_device_extensions[]` and `optional_device_extensions[]`);
  * a name in neither shape is **unclassified**, and reported as such rather
    than dropped. A list that quietly shrank would be worse than no list.

The macros themselves are resolved from the vendored Vulkan headers, which are
pinned too, so a name is only ever a name the registry defines.

  layer_requirements.py <layer directory> [--json]
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_HEADER = os.path.join(ROOT, "third_party", "khronos", "include", "vulkan", "vulkan_core.h")
SOURCE_SUFFIXES = (".h", ".hpp", ".c", ".cpp", ".cc")
NAME_MACRO = re.compile(r"\bVK_[A-Z0-9_]+_EXTENSION_NAME\b")
MACRO_DEFINITION = re.compile(r'^#define\s+(VK_[A-Z0-9_]+_EXTENSION_NAME)\s+"([^"]+)"')
EXTENSION_NAME = re.compile(r"^VK_[A-Z]+_[a-z0-9_]+$")
TABLE_OPENING = re.compile(r"\b(?P<name>\w+)\s*\[\s*\]\s*=\s*\{")
REQUIRED_WORD = re.compile(r"\bRequired\b")
OPTIONAL_WORD = re.compile(r"\bOptional\b")


class ExtensionNames:
    """The `VK_*_EXTENSION_NAME` macros the pinned registry headers define, and
    the string each one stands for."""

    def __init__(self, header=DEFAULT_HEADER):
        self.header = header
        self.strings = {}

    def read(self):
        with open(self.header, encoding="utf-8", errors="replace") as source:
            for line in source:
                found = MACRO_DEFINITION.match(line.strip())
                if found is not None:
                    self.strings[found.group(1)] = found.group(2)
        return self.strings

    def string_of(self, macro):
        return self.strings.get(macro)


class LayerRequirements:
    """One layer's own answer: which extensions it requires, which it prefers,
    and which its source names in a shape this cannot classify."""

    def __init__(self, directory, names):
        self.directory = directory
        self.names = names
        self.required_ = set()
        self.optional_ = set()
        self.unclassified_ = set()

    def read(self):
        """Every source file under the directory, in a stable order."""
        if not os.path.isdir(self.directory):
            raise FileNotFoundError(f"{self.directory} is not a directory")
        for base, _directories, files in sorted(os.walk(self.directory)):
            for name in sorted(files):
                if name.endswith(SOURCE_SUFFIXES):
                    self.read_file(os.path.join(base, name))
        return self

    def read_file(self, path):
        with open(path, encoding="utf-8", errors="replace") as source:
            table = None
            depth = 0
            for line in source:
                opening = TABLE_OPENING.search(line)
                if opening is not None and depth == 0:
                    table = opening.group("name").lower()
                    depth = 1
                elif depth != 0:
                    depth += line.count("{") - line.count("}")
                    if depth <= 0:
                        table = None
                        depth = 0
                for macro in NAME_MACRO.findall(line):
                    self.classify(macro, line, table)

    def classify(self, macro, line, table):
        name = self.names.string_of(macro)
        if name is None or not EXTENSION_NAME.match(name):
            # A macro the pinned registry does not define is not a name this
            # can vouch for, whatever the layer's source calls it.
            self.unclassified_.add(macro)
            return
        if REQUIRED_WORD.search(line):
            self.required_.add(name)
        elif OPTIONAL_WORD.search(line):
            self.optional_.add(name)
        elif table is not None and "required" in table:
            self.required_.add(name)
        elif table is not None and "optional" in table:
            self.optional_.add(name)
        else:
            self.unclassified_.add(name)

    def required(self):
        """Required wins over optional: a layer that names an extension in both
        shapes cannot start without it."""
        return sorted(self.required_)

    def optional(self):
        return sorted(self.optional_ - self.required_)

    def unclassified(self):
        return sorted(self.unclassified_)

    def invariants_hold(self):
        """What it answers is disjoint, and every name it answers is one the
        registry defines."""
        required = set(self.required())
        optional = set(self.optional())
        if required & optional:
            return False
        for name in required | optional:
            if not EXTENSION_NAME.match(name):
                return False
        return True


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", help="the unpacked layer source")
    parser.add_argument("--header", default=DEFAULT_HEADER, help="the pinned vulkan_core.h")
    parser.add_argument("--json", action="store_true", help="machine-readable output")
    arguments = parser.parse_args(argv[1:])
    names = ExtensionNames(arguments.header)
    names.read()
    layer = LayerRequirements(arguments.directory, names)
    layer.read()
    if not layer.invariants_hold():
        print("layer_requirements: what it read is not a list", file=sys.stderr)
        return 2
    if arguments.json:
        print(json.dumps({"required": layer.required(), "optional": layer.optional(),
                          "unclassified": layer.unclassified()}, indent=2))
        return 0
    for name in layer.required():
        print(f"required {name}")
    for name in layer.optional():
        print(f"optional {name}")
    for name in layer.unclassified():
        print(f"unclassified {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
