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

The process must already be running in the held guest (run_guest_suite.py
--keep). Parsing needs the etl-parser package: it is taken from
build-perfetto/venv when the running Python lacks it.
"""
import argparse
import bisect
import collections
import json
import os
import pathlib
import re
import struct
import subprocess
import sys
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
VENV_PYTHON = ROOT / "build-perfetto" / "venv" / "bin" / "python"
try:
    from etl.etl import IEtlFileObserver, build_from_stream
except ImportError:
    if VENV_PYTHON.exists() and pathlib.Path(sys.executable) != VENV_PYTHON:
        os.execv(str(VENV_PYTHON), [str(VENV_PYTHON), __file__] + sys.argv[1:])
    raise SystemExit("needs etl-parser: pip install etl-parser (or make build-perfetto/venv with it)")

LOCAL_BINARIES = (ROOT / "build" / "guest", ROOT / "build" / "driver")
CACHE = ROOT / "build-perfetto" / "symbols"
SYMBOL_SERVER = "https://msdl.microsoft.com/download/symbols"
SAMPLED_PROFILE = 46          # PerfInfo SampledProfile: instruction pointer, thread
STACKWALK_GROUP = 24          # EVENT_TRACE_GROUP_STACKWALK
STACK = 32                    # its stack event: time, process, thread, then the frames leaf first
KERNEL_SPACE = 1 << 63
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
    """The process's modules and the trace of `seconds` of every CPU."""
    listing = guest(work, "ps", f"""
wpr -cancel 2>&1 | Out-Null
$p = Get-Process {process} -ErrorAction Stop | Select-Object -First 1
"p " + $p.Id + " " + [Environment]::ProcessorCount
$p.Modules | ForEach-Object {{ "m " + ("{{0:x}}" -f $_.BaseAddress.ToInt64()) + " " + $_.ModuleMemorySize + " " + $_.FileName }}
wpr -start CPU -filemode 2>&1 | Out-Null
Start-Sleep -Milliseconds {int(seconds * 1000)}
wpr -stop {REMOTE_TRACE} 2>&1 | Out-Null""", timeout=int(seconds) + 120)
    pid, cpus, modules = 0, 1, []
    for line in listing.splitlines():
        parts = line.strip().split(" ", 3)
        if parts[0] == "p":
            pid, cpus = int(parts[1]), int(parts[2])
        elif parts[0] == "m" and len(parts) == 4:
            modules.append((int(parts[1], 16), int(parts[2]), parts[3]))
    if pid == 0:
        raise SystemExit(f"no process {process} in the guest")
    trace = scratch / "cpu.etl"
    guest(work, "pull", REMOTE_TRACE, str(trace), timeout=300)
    return pid, cpus, modules, trace


class Trace(IEtlFileObserver):
    """The samples of a WPR trace and the kernel images it names."""

    def __init__(self, path):
        self.samples = []
        self.sampled_at = set()   # (stamp, thread) of each sample
        self.walked = {}          # (stamp, thread): frames, leaf first
        self.kernel_images = []
        build_from_stream(path.read_bytes()).parse(self)

    def on_event_record(self, event):
        pass

    def on_trace_record(self, event):
        pass

    def on_win_trace(self, event):
        pass

    def on_perfinfo_trace(self, event):
        payload = bytes(event.source.mof_data)
        header = event.source.header
        if header.type == SAMPLED_PROFILE and len(payload) >= 12:
            address, thread = struct.unpack_from("<QI", payload)
            self.samples.append((address, thread))
            self.sampled_at.add((event.source.timestamp, thread))
        elif int(header.group) == STACKWALK_GROUP and header.type == STACK and len(payload) > 16:
            stamp, _, thread = struct.unpack_from("<QII", payload)
            count = (len(payload) - 16) // 8
            # A kernel-mode sample's stack comes in two events of one stamp:
            # the kernel's frames, then the user's once the thread returns.
            self.walked.setdefault((stamp, thread), []).extend(struct.unpack_from(f"<{count}Q", payload, 16))

    def sample_stacks(self):
        """(thread, frames) of each sample's stack: ETW also walks the stack
        for context switches and readied threads, which are not CPU time."""
        return [(key[1], tuple(frames)) for key, frames in self.walked.items() if key in self.sampled_at]

    def on_system_trace(self, event):
        try:
            mof = event.get_mof()
        except Exception:  # the parser raises on event versions it does not know; those are not images
            return
        if type(mof).__name__ == "ImageLoad":
            self.kernel_images.append((mof.source.ImageBase, mof.source.ImageSize, mof.get_image_filename()))


def windows_path(name):
    """A path as the trace or the module list gives it, as the guest agent takes it."""
    name = re.sub(r"^\\SystemRoot", r"C:\\Windows", name, flags=re.IGNORECASE)
    name = re.sub(r"^\\\?\?\\", "", name)
    name = re.sub(r"^\\Device\\HarddiskVolume\d+", "C:", name, flags=re.IGNORECASE)
    if re.match(r"^\\Windows", name, flags=re.IGNORECASE):
        name = "C:" + name
    return name


class Module:
    """One image in the sampled address space, and the names of its code."""

    def __init__(self, base, size, path):
        self.base, self.size, self.path = base, size, path
        self.name = path.replace("/", "\\").split("\\")[-1]

    def name_all(self, work, offsets):
        """Each of `offsets` named: {offset: function}."""
        for folder in LOCAL_BINARIES:
            local = folder / self.name
            if local.exists() and local.with_suffix(".pdb").exists():
                return self.symbolize(local, offsets)
        publics = self.publics(work)
        if not publics:
            return {offset: f"+{offset:#x}" for offset in offsets}
        addresses = [address for address, _ in publics]
        named = {}
        for offset in offsets:
            index = bisect.bisect_right(addresses, offset) - 1
            named[offset] = publics[index][1] if index >= 0 else f"+{offset:#x}"
        return named

    def symbolize(self, local, offsets):
        headers = subprocess.run(["llvm-readobj", "--file-headers", str(local)], capture_output=True, text=True).stdout
        image_base = int(re.search(r"ImageBase: (0x[0-9A-Fa-f]+)", headers).group(1), 16)
        ordered = sorted(offsets)
        answers = subprocess.run(["llvm-symbolizer", "--obj", str(local), "--functions=short", "--no-inlines"] +
                                 [hex(image_base + offset) for offset in ordered],
                                 capture_output=True, text=True).stdout.split("\n\n")
        return {offset: answer.strip().split("\n")[0] for offset, answer in zip(ordered, answers)}

    def publics(self, work):
        """The module's public functions, (offset, name) sorted, from its PDB
        on the symbol server; its exports when there is none."""
        folder = CACHE / f"{self.name.lower()}-{self.size:x}"
        folder.mkdir(parents=True, exist_ok=True)
        image = folder / self.name
        if not image.exists():
            try:
                guest(work, "pull", windows_path(self.path), str(image))
            except SystemExit:
                return []
        sections = [int(value, 16) for value in re.findall(
            r"VirtualAddress: (0x[0-9A-Fa-f]+)",
            subprocess.run(["llvm-readobj", "--sections", str(image)], capture_output=True, text=True).stdout)]
        pdb = self.fetch_pdb(image, folder)
        found = []
        if pdb is not None:
            dump = subprocess.run(["llvm-pdbutil", "dump", "-publics", str(pdb)], capture_output=True,
                                  text=True).stdout
            for match in re.finditer(r"`([^`]+)`\n\s+flags = function[^,\n]*, addr = (\d+):(\d+)", dump):
                section, offset = int(match.group(2)), int(match.group(3))
                if 1 <= section <= len(sections):
                    found.append((sections[section - 1] + offset, match.group(1)))
        if not found:
            exports = subprocess.run(["llvm-readobj", "--coff-exports", str(image)], capture_output=True,
                                     text=True).stdout
            for match in re.finditer(r"Name: (\S+)\n\s+RVA: (0x[0-9A-Fa-f]+)", exports):
                found.append((int(match.group(2), 16), match.group(1)))
        return sorted(found)

    def fetch_pdb(self, image, folder):
        directory = subprocess.run(["llvm-readobj", "--coff-debug-directory", str(image)], capture_output=True,
                                   text=True).stdout
        guid = re.search(r"PDBGUID: \(?\{?([0-9A-Fa-f-]+)", directory)
        age = re.search(r"PDBAge: (\d+)", directory)
        name = re.search(r"PDBFileName: (\S+)", directory)
        if not (guid and age and name):
            return None
        pdb_name = name.group(1).replace("\\", "/").split("/")[-1]
        pdb = folder / pdb_name
        if pdb.exists():
            return pdb
        key = guid.group(1).replace("-", "").upper() + f"{int(age.group(1)):X}"
        try:
            with urllib.request.urlopen(f"{SYMBOL_SERVER}/{pdb_name}/{key}/{pdb_name}", timeout=120) as reply:
                pdb.write_bytes(reply.read())
        except OSError as error:
            print(f"no symbols for {self.name}: {error}", file=sys.stderr)
            return None
        return pdb


class AddressSpace:
    """The images samples fall in: the process's own modules, then the kernel's."""

    def __init__(self, user, kernel):
        # The trace lists the user modules too, by device path; the process's
        # own list names them as the guest agent can open them.
        bases = {base for base, _, _ in user}
        kernel = [entry for entry in kernel if entry[0] not in bases]
        self.modules = sorted((Module(*entry) for entry in user + kernel), key=lambda module: module.base)
        self.bases = [module.base for module in self.modules]

    def locate(self, address):
        index = bisect.bisect_right(self.bases, address) - 1
        if index >= 0 and address < self.modules[index].base + self.modules[index].size:
            return self.modules[index], address - self.modules[index].base
        return None, address


class Names:
    """Addresses named `module!function`, a module's worth at a time."""

    def __init__(self, work, space):
        self.work, self.space = work, space
        self.named = {}

    def resolve(self, addresses):
        by_module = collections.defaultdict(set)
        for address in addresses:
            if address in self.named:
                continue
            module, offset = self.space.locate(address)
            if module is None:
                self.named[address] = "?"
            else:
                by_module[module].add(offset)
        for module, offsets in by_module.items():
            for offset, function in module.name_all(self.work, sorted(offsets)).items():
                self.named[module.base + offset] = f"{module.name}!{function}"

    def module(self, address):
        module, _ = self.space.locate(address)
        return module.name if module else "?"

    def __getitem__(self, address):
        return self.named[address]


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
        pid, cpus, modules, path = record(args.work, args.process, args.seconds, scratch)
        threads = [int(tid) for tid in guest(args.work, "ps", f"(Get-Process -Id {pid} -ErrorAction SilentlyContinue)"
                                                               ".Threads | ForEach-Object { $_.Id }").split()]
        recording.write_text(json.dumps({"cpus": cpus, "modules": modules, "threads": threads, "trace": str(path)}))
    saved = json.loads(recording.read_text())
    trace = Trace(pathlib.Path(saved["trace"]))
    names = Names(args.work, AddressSpace([tuple(entry) for entry in saved["modules"]], trace.kernel_images))
    per_thread = collections.Counter(tid for _, tid in trace.samples)
    per_cpu = len(trace.samples) / max(saved["cpus"], 1)
    busiest = [tid for tid, _ in per_thread.most_common() if tid in set(saved["threads"])][:args.threads]
    if not busiest:
        raise SystemExit("no samples on the process's threads: was it running?")
    print(f"{len(trace.samples)} samples over {saved['cpus']} CPUs; the process's busiest threads:")
    for tid in busiest:
        print(f"  thread {tid:6}  {100 * per_thread[tid] / per_cpu:5.1f}% of a CPU")
    stacks = trace.sample_stacks()
    for tid in busiest:
        report_thread(names, tid, trace.samples, stacks, args.top, args.callers)


if __name__ == "__main__":
    main()
