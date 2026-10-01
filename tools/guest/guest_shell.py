#!/usr/bin/env python3
"""A shell into a guest that tools/guest/run_guest_suite.py --hold left
running: programs, PowerShell and files over its guest agent socket.

  guest_shell.py [--work build-guest] ps 'Get-PnpDevice -Class Display'
  guest_shell.py [--work build-guest] exec 'C:\\av\\avtest.exe' [ARGUMENT ...]
  guest_shell.py [--work build-guest] push avgpu.sys 'C:\\av\\package\\avgpu.sys'
  guest_shell.py [--work build-guest] pull 'C:\\Windows\\MEMORY.DMP' MEMORY.DMP

exec and ps print the program's output and exit with its exit code.
"""
import argparse
import os
import pathlib
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from guest_agent import AgentError, wait_for_agent  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
POWERSHELL = "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"


def arguments():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--work", type=pathlib.Path, default=ROOT / "build-guest")
    parser.add_argument("--timeout", type=float, default=600.0, help="seconds a program may run")
    parser.add_argument("--agent-timeout", type=float, default=30.0)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("ps").add_argument("script")
    program = commands.add_parser("exec")
    program.add_argument("path")
    program.add_argument("arguments", nargs=argparse.REMAINDER)
    push = commands.add_parser("push")
    push.add_argument("local", type=pathlib.Path)
    push.add_argument("remote")
    pull = commands.add_parser("pull")
    pull.add_argument("remote")
    pull.add_argument("local", type=pathlib.Path)
    return parser.parse_args()


def run(args, agent):
    if args.command in ("ps", "exec"):
        path, program_arguments = (POWERSHELL, ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", args.script]) \
            if args.command == "ps" else (args.path, args.arguments)
        code, out, error = agent.run(path, program_arguments, args.timeout)
        sys.stdout.buffer.write(out)
        sys.stderr.buffer.write(error)
        return code
    if args.command == "push":
        agent.push(args.local.read_bytes(), args.remote)
        return 0
    print(f"{agent.pull(args.remote, args.local)} bytes")
    return 0


def main():
    args = arguments()
    try:
        agent = wait_for_agent(str(args.work / "agent.sock"), time.monotonic() + args.agent_timeout,
                               args.agent_timeout)
        try:
            code = run(args, agent)
        finally:
            agent.close()
    except AgentError as error:
        print(f"guest_shell: {error}", file=sys.stderr)
        sys.exit(1)
    sys.exit(code)


if __name__ == "__main__":
    main()
