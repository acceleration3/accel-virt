#!/usr/bin/env python3
"""The resolutions of a running VM's display, set from the host."""
import argparse
import json
import pathlib
import re
import socket
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MODE = re.compile(r"^[0-9]{1,5}[xX][0-9]{1,5}$")


class Qmp:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX)
        self.sock.settimeout(10)
        self.sock.connect(str(path))
        self.reader = self.sock.makefile("r")
        json.loads(self.reader.readline())
        self.execute("qmp_capabilities")

    def execute(self, command, **arguments):
        message = {"execute": command}
        if arguments:
            message["arguments"] = arguments
        self.sock.sendall((json.dumps(message) + "\n").encode())
        while True:
            answer = json.loads(self.reader.readline())
            if "return" in answer:
                return answer["return"]
            if "error" in answer:
                raise RuntimeError(answer["error"].get("desc", "QMP error"))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("modes", nargs="*", help="WxH sizes, the first preferred; or 'host'")
    parser.add_argument("--qmp", type=pathlib.Path, default=ROOT / "build-guest-render" / "qmp.sock",
                        help="the VM's QMP socket")
    parser.add_argument("--device", default="gpu", help="the display device's id on QEMU's command line")
    args = parser.parse_args()
    path = f"/machine/peripheral/{args.device}"
    if args.modes == ["host"]:
        text = ""
    else:
        bad = [mode for mode in args.modes if not MODE.match(mode)]
        if bad:
            sys.exit(f"not a WxH size: {', '.join(bad)}")
        text = ",".join(mode.lower() for mode in args.modes)
    qmp = Qmp(args.qmp)
    if args.modes:
        qmp.execute("qom-set", path=path, property="modes", value=text)
    current = qmp.execute("qom-get", path=path, property="modes")
    print(current if current else "the host monitors' modes")


if __name__ == "__main__":
    main()
