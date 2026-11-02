#!/usr/bin/env python3
import json
import os
import sys

ROOTS = ("km", "wdk-shared", "sdk-shared", "um")


def entries(kit, relative):
    listed = []
    for name in sorted(os.listdir(os.path.join(kit, relative))):
        path = os.path.join(relative, name)
        if os.path.isdir(os.path.join(kit, path)):
            listed.append({"name": name, "type": "directory",
                           "contents": entries(kit, path)})
        else:
            listed.append({"name": name, "type": "file",
                           "external-contents": path})
    return listed


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: windows_kit_overlay.py <kit dir>")
    kit = sys.argv[1]
    roots = [{"name": f"/windows-kit/{root}", "type": "directory",
              "contents": entries(kit, os.path.join("include", root))}
             for root in ROOTS if os.path.isdir(os.path.join(kit, "include", root))]
    overlay = {"version": 0, "case-sensitive": "false", "overlay-relative": True,
               "use-external-names": False, "roots": roots}
    with open(os.path.join(kit, "overlay.yaml"), "w") as out:
        json.dump(overlay, out, indent=1)


if __name__ == "__main__":
    main()
