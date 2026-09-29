#!/usr/bin/env python3
"""The OpenCL-OpenGL sharing samples (samples/clgl) in the running guest.

Each sample checks the values it moves between an OpenGL context and the
OpenCL context made from it, so there is no host run to compare against: the
program's own "ok"/"FAIL" lines are the verdict, and this exits with the
number that failed (100 when no shared context could be made at all).

    cmake --build build/guest --target rvkclglsamples.exe
    tools/clgl_samples.py --work build-guest-render

The guest must already be running a build of this tree, with the rvkcl.dll and
rvkgl.dll these samples were written against.
"""
import argparse
import pathlib
import re
import sys

from d3d11_report import guest_shell

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUEST_DIR = "C:\\rvk\\clglsamples"
GUEST_EXE = GUEST_DIR + "\\rvkclglsamples.exe"
NO_CONTEXT = 100


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--work", type=pathlib.Path, default=ROOT / "build-guest-render",
                        help="the running guest's work directory")
    parser.add_argument("--build", type=pathlib.Path, default=ROOT / "build" / "guest",
                        help="where rvkclglsamples.exe and OpenCL.dll were built")
    parser.add_argument("--guest-user", default="win11\\winuser", help="the guest's logged-on user")
    args = parser.parse_args()
    work = args.work
    guest_shell(work, "ps", f"Remove-Item -Recurse -Force {GUEST_DIR} -ErrorAction SilentlyContinue; "
                            f"New-Item -ItemType Directory -Force {GUEST_DIR} | Out-Null")
    guest_shell(work, "push", str(args.build / "OpenCL.dll"), GUEST_DIR + "\\OpenCL.dll")
    guest_shell(work, "push", str(args.build / "rvkclglsamples.exe"), GUEST_EXE)
    # The user's interactive session: an OpenGL context needs a desktop, which
    # the agent's session does not have.
    output = guest_shell(work, "ps", f"""
Unregister-ScheduledTask -TaskName rvkclglsamples -Confirm:$false -ErrorAction SilentlyContinue
$a = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument '/c cd /d {GUEST_DIR} && {GUEST_EXE} > {GUEST_DIR}\\run.txt 2>&1'
$p = New-ScheduledTaskPrincipal -UserId '{args.guest_user}' -LogonType Interactive -RunLevel Highest
$s = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 5)
Register-ScheduledTask -TaskName rvkclglsamples -Action $a -Principal $p -Settings $s | Out-Null
Start-ScheduledTask -TaskName rvkclglsamples
Start-Sleep 1
$waited = 0
while ((Get-ScheduledTask rvkclglsamples).State -eq 'Running' -and $waited -lt 300) {{ Start-Sleep 1; $waited++ }}
'exit: ' + (Get-ScheduledTaskInfo rvkclglsamples).LastTaskResult
Get-Content {GUEST_DIR}\\run.txt -ErrorAction SilentlyContinue""", timeout=420)
    print(output, end="", flush=True)
    code = re.search(r"^exit: (-?\d+)", output, re.MULTILINE)
    if code is None:
        sys.exit("the samples did not run to an exit")
    sys.exit(int(code.group(1)) if int(code.group(1)) <= NO_CONTEXT else NO_CONTEXT)


if __name__ == "__main__":
    main()
