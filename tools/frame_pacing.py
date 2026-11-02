#!/usr/bin/env python3
"""Why a guest frame came late: one recording of every CPU's scheduling, CPU
samples with stacks, DPCs and interrupts, dxgkrnl, DWM and the flip manager,
and an account of each DWM frame from it.

    tools/frame_pacing.py run                          # WGC monitor capture of a D3D11 window, 8 s
    tools/frame_pacing.py run --workload window --seconds 5
    tools/frame_pacing.py run --workload none --seconds 5    # whatever the guest is doing
    tools/frame_pacing.py record --name before
    tools/frame_pacing.py analyze before --perfetto before.json

`record` drives the held guest (the work directory's guest agent) with WPR
and a profile this tool writes (build-perfetto/frame-pacing/<name>/), runs the
workload - tools/guest/probes/wgc_probe.exe in the user's session unless
--workload none - and pulls the trace, the probe's report and the guest's own
description of the providers' events (etw_schema.exe, cached per Windows
build). `analyze` reads it all without the guest:

  - DWM's frames, each against the vblank it follows (VSyncDPC's QPC time);
  - every Windows.Graphics.Capture skip (DelayedRendering) against the rule
    DWM applies - a capture check less than MinUpdateInterval (16 ms) after
    the last captured one is skipped - and whether the rule explains it;
  - where DWM's render thread spent its time from the vblank to the capture
    check: running (less DPCs and ISRs on its CPU), waiting for a CPU, or
    blocked and why; its dxgkrnl calls; its CPU samples by module and by
    function, normal frames beside the frames a skip was measured from.

--perfetto writes the same trace for ui.perfetto.dev: every CPU's threads and
interrupts, the render thread's states, frames, stages and samples, and the
vblanks. Symbols: this repository's binaries from build/guest and
build/driver when their PDBs match; Windows' own from Microsoft's symbol
server by the PDB the trace names, cached in build-perfetto/symbols.
"""
import argparse
import json
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from etw.dwm import DwmFrames  # noqa: E402
from etw.perfetto import export  # noqa: E402
from etw.report import lines_for  # noqa: E402
from etw.symbols import Symbols  # noqa: E402
from etw.trace import Trace  # noqa: E402

OUTPUT = ROOT / "build-perfetto" / "frame-pacing"
PROBES = ROOT / "build" / "guest-probes"
GUEST_DIR = "C:\\av\\frame-pacing"
PROFILE_NAME = "FramePacing"
PROVIDERS = [
    ("Microsoft-Windows-DxgKrnl", 0x8000003, 5, True),
    ("Microsoft-Windows-Dwm-Core", 0xC8, 5, False),
    ("Microsoft-Windows-Win32k", 0x1000, 5, True),
]
KERNEL_KEYWORDS = ["ProcessThread", "Loader", "CSwitch", "ReadyThread", "SampledProfile", "DPC", "Interrupt"]
KERNEL_STACKS = ["SampledProfile", "CSwitch", "ReadyThread"]
WORKLOADS = ("monitor-d3d", "window-d3d", "monitor", "window", "none")


def profile_xml():
    keywords = "\n".join(f'        <Keyword Value="{keyword}"/>' for keyword in KERNEL_KEYWORDS)
    stacks = "\n".join(f'        <Stack Value="{stack}"/>' for stack in KERNEL_STACKS)
    providers = []
    ids = []
    for index, (name, keyword_mask, level, nonpaged) in enumerate(PROVIDERS):
        ids.append(f"            <EventProviderId Value=\"Provider{index}\"/>")
        providers.append(f'    <EventProvider Id="Provider{index}" Name="{name}" Level="{level}"'
                         f'{" NonPagedMemory=\"true\"" if nonpaged else ""}>\n'
                         f'      <Keywords><Keyword Value="{keyword_mask:#x}"/></Keywords>\n    </EventProvider>')
    return f"""<?xml version="1.0" encoding="utf-8"?>
<WindowsPerformanceRecorder Version="1.0" Author="accel-virt" Comments="tools/frame_pacing.py">
  <Profiles>
    <SystemCollector Id="Kernel" Name="NT Kernel Logger">
      <BufferSize Value="1024"/>
      <Buffers Value="512"/>
    </SystemCollector>
    <EventCollector Id="Events" Name="av frame pacing">
      <BufferSize Value="1024"/>
      <Buffers Value="256"/>
    </EventCollector>
    <SystemProvider Id="KernelEvents">
      <Keywords>
{keywords}
      </Keywords>
      <Stacks>
{stacks}
      </Stacks>
    </SystemProvider>
{chr(10).join(providers)}
    <Profile Id="{PROFILE_NAME}.Verbose.File" Name="{PROFILE_NAME}" Description="frame pacing" LoggingMode="File" DetailLevel="Verbose">
      <Collectors>
        <SystemCollectorId Value="Kernel">
          <SystemProviderId Value="KernelEvents"/>
        </SystemCollectorId>
        <EventCollectorId Value="Events">
          <EventProviders>
{chr(10).join(ids)}
          </EventProviders>
        </EventCollectorId>
      </Collectors>
    </Profile>
  </Profiles>
</WindowsPerformanceRecorder>
"""


class Guest:
    def __init__(self, work):
        self.work = work

    def run(self, *command, timeout=120):
        out = subprocess.run([sys.executable, "-B", str(ROOT / "tools" / "guest" / "guest_shell.py"),
                              "--work", str(self.work), "--agent-timeout", str(timeout - 10), *command],
                             capture_output=True, text=True, timeout=timeout)
        if out.returncode != 0:
            raise SystemExit(f"guest {command[0]} failed: {(out.stderr or out.stdout).strip()[-400:]}")
        return out.stdout

    def ps(self, script, timeout=120):
        return self.run("ps", script, timeout=timeout)

    def push(self, local, remote):
        self.run("push", str(local), remote, timeout=120)

    def pull(self, remote, local, timeout=300):
        self.run("pull", remote, str(local), timeout=timeout)


def ensure_probes():
    wanted = [PROBES / "wgc_probe.exe", PROBES / "etw_schema.exe"]
    if not all(path.exists() for path in wanted):
        subprocess.run([str(ROOT / "tools" / "guest" / "probes" / "build.sh"), str(PROBES)], check=True)
    return wanted


def schema_for(guest, build):
    cached = OUTPUT / f"schema-{build}.json"
    if cached.exists():
        return cached
    names = " ".join(name for name, _, _, _ in PROVIDERS)
    guest.ps(f"& {GUEST_DIR}\\etw_schema.exe {GUEST_DIR}\\schema.json {names}", timeout=300)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    guest.pull(f"{GUEST_DIR}\\schema.json", cached)
    return cached


def record(args):
    guest = Guest(args.work)
    folder = OUTPUT / args.name
    folder.mkdir(parents=True, exist_ok=True)
    probe, schema_tool = ensure_probes()
    guest.ps(f"New-Item -ItemType Directory -Force {GUEST_DIR} | Out-Null")
    profile = folder / "frame_pacing.wprp"
    profile.write_text(profile_xml())
    for local in (probe, schema_tool, profile):
        guest.push(local, f"{GUEST_DIR}\\{local.name}")
    build = guest.ps("$v = Get-ItemProperty 'HKLM:\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion'; "
                     "\"$($v.CurrentBuildNumber).$($v.UBR)\"").strip().splitlines()[-1]
    schema = schema_for(guest, build)
    interval = max(int(args.sample_us * 10), 1221)
    if args.workload == "none":
        workload = f"Start-Sleep -Milliseconds {int(args.seconds * 1000)}"
    else:
        command = (f"{GUEST_DIR}\\wgc_probe.exe {args.workload} --seconds {args.seconds} "
                   f"--out {GUEST_DIR}\\probe.txt {args.probe_args}").strip()
        workload = f"""
$user = (Get-CimInstance Win32_ComputerSystem).UserName
schtasks /create /tn avframepacing /tr '{command}' /sc once /st 23:59 /ru $user /it /f | Out-Null
schtasks /run /tn avframepacing | Out-Null
$waited = 0
do {{ Start-Sleep -Milliseconds 250; $waited++ }} while (-not ((Test-Path {GUEST_DIR}\\probe.txt) -and
    (Select-String -Path {GUEST_DIR}\\probe.txt -Pattern '^WGC ' -Quiet)) -and $waited -lt {int(args.seconds * 4) + 120})
schtasks /delete /tn avframepacing /f | Out-Null"""
    started = time.time()
    output = guest.ps(f"""
wpr -cancel 2>&1 | Out-Null
Remove-Item {GUEST_DIR}\\trace.etl, {GUEST_DIR}\\probe.txt -ErrorAction SilentlyContinue
wpr -setprofint {interval} 2>&1 | Out-Null
wpr -start {GUEST_DIR}\\frame_pacing.wprp!{PROFILE_NAME} -filemode 2>&1 | Select-Object -Last 1
{workload}
wpr -stop {GUEST_DIR}\\trace.etl 2>&1 | Select-Object -Last 1""", timeout=int(args.seconds) + 400)
    trace = folder / "trace.etl"
    guest.pull(f"{GUEST_DIR}\\trace.etl", trace, timeout=600)
    probe_report = ""
    if args.workload != "none":
        guest.pull(f"{GUEST_DIR}\\probe.txt", folder / "probe.txt")
        probe_report = (folder / "probe.txt").read_text(errors="replace")
    (folder / "meta.json").write_text(json.dumps({
        "schema": str(schema), "sample_ms": interval / 10000.0, "workload": args.workload,
        "seconds": args.seconds, "windows_build": build, "recorded": time.strftime("%Y-%m-%d %H:%M:%S"),
    }, indent=1))
    summary = [line for line in probe_report.splitlines() if line.startswith(("capture", "intervals", "WGC"))]
    print(f"recorded {trace} ({trace.stat().st_size / 1e6:.1f} MB) in {time.time() - started:.0f} s")
    print("\n".join(line.strip() for line in output.splitlines() if line.strip() and "wpr" not in line.lower()))
    for line in summary:
        print(f"probe: {line}")
    return folder


def analyze(args):
    folder = pathlib.Path(args.name) if pathlib.Path(args.name).is_dir() else OUTPUT / args.name
    meta = json.loads((folder / "meta.json").read_text())
    started = time.time()
    trace = Trace.read(folder / "trace.etl", meta["schema"])
    frames = DwmFrames(trace, args.interval_ms)
    symbols = Symbols(trace.kernel, local=[ROOT / "build" / "guest", ROOT / "build" / "driver"],
                      cache=ROOT / "build-perfetto" / "symbols", offline=args.offline)
    timeline = frames.analyze(symbols)
    probe = ""
    probe_file = folder / "probe.txt"
    if probe_file.exists():
        probe = " ".join(line.strip() for line in probe_file.read_text(errors="replace").splitlines()
                         if line.startswith(("capture", "intervals")))
    for line in lines_for(trace, frames, symbols, meta["sample_ms"], top=args.top, probe=probe):
        print(line)
    if args.perfetto:
        count = export(args.perfetto, trace, frames, timeline, symbols)
        print(f"\n{count} events in {args.perfetto}: open it at https://ui.perfetto.dev")
    print(f"\n(analysed in {time.time() - started:.0f} s)", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("record", "run"):
        command = commands.add_parser(name)
        command.add_argument("--work", type=pathlib.Path, default=ROOT / "build-guest-render")
        command.add_argument("--name", default=time.strftime("fp-%H%M%S"))
        command.add_argument("--seconds", type=float, default=8.0)
        command.add_argument("--sample-us", type=float, default=250.0,
                             help="CPU sampling interval in microseconds (WPR's floor is 122)")
        command.add_argument("--workload", choices=WORKLOADS, default="monitor-d3d")
        command.add_argument("--probe-args", default="", help="more wgc_probe.exe arguments, e.g. '--render-ms 4'")
    for name in ("analyze", "run"):
        command = commands.choices.get(name) or commands.add_parser(name)
        if name == "analyze":
            command.add_argument("name", help="a recording's name under build-perfetto/frame-pacing, or its folder")
        command.add_argument("--interval-ms", type=float, default=16.0,
                             help="the capture's MinUpdateInterval (16 ms unless the session sets one)")
        command.add_argument("--top", type=int, default=15)
        command.add_argument("--perfetto", type=pathlib.Path, help="also write a trace for ui.perfetto.dev")
        command.add_argument("--offline", action="store_true", help="no symbol server: cached PDBs only")
    args = parser.parse_args()
    if args.command == "record":
        record(args)
    elif args.command == "analyze":
        analyze(args)
    else:
        folder = record(args)
        args.name = str(folder)
        analyze(args)


if __name__ == "__main__":
    main()
