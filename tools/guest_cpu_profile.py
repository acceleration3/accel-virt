#!/usr/bin/env python3
"""Where a guest process's CPU goes, function by function, kernel included.

Samples every guest CPU with WPR (the guest's own sampling profiler) while a
process runs, pulls the trace, and names the hottest functions of its busiest
threads: this repository's binaries through their PDBs in build/guest, every
Windows module - ntdll, d3d11, dxgkrnl, the kernel - through the PDB
Microsoft's symbol server has for that exact build. Scopes (profile_sample.sh)
say how long a call took; this says what the CPU did inside it, including the
parts no scope covers: the runtime, the kernel, interrupts that land on the
thread.

    tools/guest_cpu_profile.py rvkd3d11samples
    tools/guest_cpu_profile.py rvkd3d11samples --seconds 3 --threads 2 --top 30

The guest must be held (run_guest_suite.py --keep). The process is found in the
trace by name, so one that exits and is started over during it counts whole.
The trace is read with tools/etw (no dependencies); symbols as
tools/frame_pacing.py names them.
"""
import argparse
import collections
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from etw.etl_file import EtlFile  # noqa: E402
from etw.kernel import KERNEL_SPACE, Kernel  # noqa: E402
from etw.symbols import Symbols  # noqa: E402

LOCAL_BINARIES = (ROOT / "build" / "guest", ROOT / "build" / "driver")
CACHE = ROOT / "build-perfetto" / "symbols"
CALLER_DEPTH = 4
REMOTE_TRACE = "C:\\rvk\\cpu.etl"


def guest(work, *command, timeout=120):
    out = subprocess.run([sys.executable, "-B", str(ROOT / "tools" / "guest" / "guest_shell.py"), "--work", str(work),
                          "--agent-timeout", str(timeout - 10), *command],
                         capture_output=True, text=True, timeout=timeout)
    if out.returncode != 0:
        raise SystemExit(f"guest {command[0]} failed: {(out.stderr or out.stdout).strip()[-400:]}")
    return out.stdout


def record(work, process, seconds, scratch):
    """The process's id, the guest's CPU count and a trace of `seconds` of every CPU."""
    listing = guest(work, "ps", f"""
wpr -cancel 2>&1 | Out-Null
$p = Get-Process {process} -ErrorAction SilentlyContinue | Select-Object -First 1
"c " + [Environment]::ProcessorCount
if ($p) {{ "p " + $p.Id }}
wpr -start CPU -filemode 2>&1 | Out-Null
Start-Sleep -Milliseconds {int(seconds * 1000)}
wpr -stop {REMOTE_TRACE} 2>&1 | Out-Null""", timeout=int(seconds) + 120)
    pid, cpus = 0, 1
    for line in listing.splitlines():
        parts = line.strip().split(" ")
        if parts[0] == "c":
            cpus = int(parts[1])
        elif parts[0] == "p":
            pid = int(parts[1])
    trace = scratch / "cpu.etl"
    guest(work, "pull", REMOTE_TRACE, str(trace), timeout=300)
    return pid, cpus, trace


class Names:
    """Addresses of one process named `module!function` (tools/etw/symbols.py)."""

    def __init__(self, symbols, pid):
        self.symbols, self.pid = symbols, pid

    def resolve(self, addresses):
        self.symbols.resolve(self.pid, addresses)

    def module(self, address):
        return self[address].split("!")[0].split("+")[0]

    def __getitem__(self, address):
        return self.symbols.name(self.pid, address)


def share(count, total):
    return f"{100 * count / total:5.1f}%"


def report_thread(names, tid, samples, stacks, top, callers):
    hits = collections.Counter(address for address, sampled in samples if sampled == tid)
    total = sum(hits.values())
    own = [stack for stack in stacks if stack[0] == tid]
    names.resolve(set(hits) | {frame for _, frames in own for frame in frames})
    kernel = sum(count for address, count in hits.items() if address >= KERNEL_SPACE)
    modules = collections.Counter()
    functions = collections.Counter()
    for address, count in hits.items():
        modules[names.module(address)] += count
        functions[names[address]] += count
    print(f"\nthread {tid}: {total} samples, {100 * kernel / total:.1f}% in kernel mode")
    print("  " + ", ".join(f"{name} {100 * count / total:.1f}%" for name, count in modules.most_common(8)))
    print("  own time (the function the sample landed in)")
    for name, count in functions.most_common(top):
        print(f"  {share(count, total)}  {name}")
    if not own:
        return
    # A stack counts once toward every function on it: what a call cost with
    # everything it called, as a share of the stacks the thread has.
    inclusive = collections.Counter()
    for _, frames in own:
        inclusive.update({names[frame] for frame in frames})
    print(f"  with callees ({len(own)} stacks)")
    for name, count in inclusive.most_common(top):
        print(f"  {share(count, len(own))}  {name}")
    for wanted in callers:
        chains = collections.Counter()
        for _, frames in own:
            named = [names[frame] for frame in frames]
            if wanted in named[0]:
                chains[" <- ".join(named[1:1 + CALLER_DEPTH])] += 1
        found = sum(chains.values())
        print(f"  who calls {wanted} ({found} stacks land in it)")
        for chain, count in chains.most_common(top // 2):
            print(f"  {share(count, found)}  {chain}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("process", nargs="?", help="the guest process's name, without .exe")
    parser.add_argument("--work", type=pathlib.Path, default=ROOT / "build-guest-render")
    parser.add_argument("--seconds", type=float, default=2.0)
    parser.add_argument("--threads", type=int, default=1, help="how many of the busiest threads to break down")
    parser.add_argument("--top", type=int, default=25, help="how many functions of each thread to list")
    parser.add_argument("--callers", action="append", default=[], metavar="NAME",
                        help="also list the call chains into functions whose name holds NAME (repeatable)")
    parser.add_argument("--again", action="store_true",
                        help="report the last recording again instead of making one (with other --callers, say)")
    args = parser.parse_args()
    scratch = ROOT / "build-perfetto"
    scratch.mkdir(exist_ok=True)
    recording = scratch / "cpu.json"
    if not args.again:
        if not args.process:
            parser.error("name the process to profile, or pass --again")
        pid, cpus, path = record(args.work, args.process, args.seconds, scratch)
        # A process that exits during the trace is found in it by name; one
        # still running is asked for its threads as well.
        threads = [int(tid) for tid in guest(args.work, "ps", f"(Get-Process -Id {pid} -ErrorAction SilentlyContinue)"
                                                               ".Threads | ForEach-Object { $_.Id }").split()] if pid else []
        recording.write_text(json.dumps({"cpus": cpus, "threads": threads, "trace": str(path),
                                         "process": args.process}))
    saved = json.loads(recording.read_text())
    kernel = Kernel()
    for item in EtlFile(saved["trace"]).records():
        kernel.add(item)
    kernel.finish()
    pids = kernel.pids_named(saved["process"])
    tids = kernel.threads_of(pids) | set(saved["threads"])
    symbols = Symbols(kernel, local=LOCAL_BINARIES, cache=CACHE)
    samples = [(sample.address, sample.tid) for sample in kernel.samples]
    per_thread = collections.Counter(tid for _, tid in samples)
    per_cpu = len(samples) / max(saved["cpus"], 1)
    busiest = [tid for tid, _ in per_thread.most_common() if tid in tids][:args.threads]
    if len(pids) > 1:
        print(f"{len(pids)} processes named {saved['process']} ran during the trace; their threads are merged")
    if not busiest:
        raise SystemExit("no samples on the process's threads: was it running?")
    print(f"{len(samples)} samples over {saved['cpus']} CPUs; the process's busiest threads:")
    for tid in busiest:
        print(f"  thread {tid:6}  {100 * per_thread[tid] / per_cpu:5.1f}% of a CPU")
    stacks = [(sample.tid, sample.stack) for sample in kernel.samples if sample.stack]
    for tid in busiest:
        names = Names(symbols, kernel.thread_process.get(tid, 0))
        report_thread(names, tid, samples, stacks, args.top, args.callers)


if __name__ == "__main__":
    main()
