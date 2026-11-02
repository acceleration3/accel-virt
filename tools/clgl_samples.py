#!/usr/bin/env python3
"""The OpenCL-OpenGL sharing samples (samples/clgl) in the running guest."""
import argparse
import pathlib
import re
import sys

from d3d11_report import guest_shell

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUEST_DIR = "C:\\av\\clglsamples"
GUEST_EXE = GUEST_DIR + "\\avclglsamples.exe"
NO_CONTEXT = 100


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--work", type=pathlib.Path, default=ROOT / "build-guest-render",
                        help="the running guest's work directory")
    parser.add_argument("--build", type=pathlib.Path, default=ROOT / "build" / "guest",
                        help="where avclglsamples.exe and OpenCL.dll were built")
    parser.add_argument("--guest-user", default="win11\\winuser", help="the guest's logged-on user")
    args = parser.parse_args()
    work = args.work
    guest_shell(work, "ps", f"Remove-Item -Recurse -Force {GUEST_DIR} -ErrorAction SilentlyContinue; "
                            f"New-Item -ItemType Directory -Force {GUEST_DIR} | Out-Null")
    guest_shell(work, "push", str(args.build / "OpenCL.dll"), GUEST_DIR + "\\OpenCL.dll")
    guest_shell(work, "push", str(args.build / "avclglsamples.exe"), GUEST_EXE)
    output = guest_shell(work, "ps", f"""
Unregister-ScheduledTask -TaskName avclglsamples -Confirm:$false -ErrorAction SilentlyContinue
$a = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument '/c cd /d {GUEST_DIR} && {GUEST_EXE} > {GUEST_DIR}\\run.txt 2>&1'
$p = New-ScheduledTaskPrincipal -UserId '{args.guest_user}' -LogonType Interactive -RunLevel Highest
$s = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 5)
Register-ScheduledTask -TaskName avclglsamples -Action $a -Principal $p -Settings $s | Out-Null
Start-ScheduledTask -TaskName avclglsamples
Start-Sleep 1
$waited = 0
while ((Get-ScheduledTask avclglsamples).State -eq 'Running' -and $waited -lt 300) {{ Start-Sleep 1; $waited++ }}
'exit: ' + (Get-ScheduledTaskInfo avclglsamples).LastTaskResult
Get-Content {GUEST_DIR}\\run.txt -ErrorAction SilentlyContinue""", timeout=420)
    print(output, end="", flush=True)
    code = re.search(r"^exit: (-?\d+)", output, re.MULTILINE)
    if code is None:
        sys.exit("the samples did not run to an exit")
    sys.exit(int(code.group(1)) if int(code.group(1)) <= NO_CONTEXT else NO_CONTEXT)


if __name__ == "__main__":
    main()
