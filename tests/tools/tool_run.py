"""What every gate-script test needs: where the repository is, and how to run a
tool and capture everything it printed."""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]


def run(*args, cwd=None):
    command = [str(a) for a in args]
    if command[0].endswith(".py"):
        command = [sys.executable, "-B"] + command
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True)
