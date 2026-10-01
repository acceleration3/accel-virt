#!/usr/bin/env python3
"""A timeline of a QEMU process's device and executor scopes, for ui.perfetto.dev.

Run QEMU with AV_PERFETTO=1 in its environment (tools/guest/run_guest_suite.py
--perfetto does) and every AV_SCOPE on the host is also an event in
/dev/shm/av-events-<module>-<pid>, a ring per thread holding its newest
events. This reads those rings, keeps the last --seconds, names each scope the
way tools/mcp/av_inspect.py does, and writes Chrome trace-event JSON, which
Perfetto opens directly:

    tools/perfetto_trace.py <qemu pid> -o trace.json --seconds 2

With a .pftrace output it writes Perfetto's own format instead, each thread's
scopes on the track of the real host thread, and --merge appends them to a
system trace tracebox recorded over the same seconds, so the scheduler's view
of every QEMU thread and the scopes it ran sit on one timeline:

    sudo build-perfetto/tracebox -o sys.pftrace --time 5s sched/sched_switch sched/sched_wakeup
    tools/perfetto_trace.py <qemu pid> --merge sys.pftrace -o merged.pftrace --seconds 5
"""
import argparse
import io
import json
import re
import subprocess
import gzip
import mmap
import time as clock
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "mcp"))
import av_inspect  # noqa: E402

MAGIC = 0x45564B52
HEADER = 32
SLOT_HEADER = 16
EVENT = 16
BEGIN = 0x80000000
MODULES = ("device", "executor")
TICKS_PER_US = 10  # the trace host's clock counts 100 ns


def read_ring(path, since=0):
    """Per slot, its events at or after `since` oldest first, as (time, name, thread, begin).
    Walked from the newest back, so a full ring costs only its window."""
    with open(path, "rb") as handle:
        data = mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ)
    magic, version, slots, per_slot = struct.unpack_from("<IIII", data, 0)
    if magic != MAGIC or version != 1:
        raise SystemExit(f"{path}: not an event ring")
    stride = SLOT_HEADER + per_slot * EVENT
    for slot in range(slots):
        base = HEADER + slot * stride
        (written,) = struct.unpack_from("<Q", data, base)
        if written == 0:
            continue
        first = written - min(written, per_slot)
        events = []
        for index in range(written - 1, first - 1, -1):
            at = base + SLOT_HEADER + (index % per_slot) * EVENT
            time, name, word = struct.unpack_from("<QII", data, at)
            if time < since:
                break
            events.append((time, name, word & ~BEGIN, bool(word & BEGIN)))
        events.reverse()
        yield slot, events


def collect(pid, seconds, names, now):
    trace = []
    newest = 0
    rings = {}
    for module in MODULES:
        path = pathlib.Path(f"/dev/shm/av-events-{module}-{pid}")
        if path.exists():
            rings[module] = list(read_ring(path, now - int(seconds * 1_000_000 * TICKS_PER_US)))
            for _, events in rings[module]:
                if events:
                    newest = max(newest, events[-1][0])
    # A slot a thread is writing while this reads can hand back a torn event;
    # the window ends at the host's clock now, which no real event passes.
    newest = min(newest, now)
    if not rings:
        raise SystemExit(f"no /dev/shm/av-events-*-{pid}: was QEMU started with AV_PERFETTO=1?")
    since = newest - int(seconds * 1_000_000 * TICKS_PER_US)
    for index, (module, slots) in enumerate(sorted(rings.items())):
        trace.append({"ph": "M", "name": "process_name", "pid": index + 1, "args": {"name": f"qemu {module}"}})
        named_threads = set()
        for _, events in slots:
            # A ring may start inside a scope: an end with no begin is dropped,
            # and what is still open at the window's edge opens there.
            depth = {}
            for time, name, thread, begin in events:
                if time < since or time > newest:
                    continue
                if thread not in named_threads:
                    named_threads.add(thread)
                    trace.append({"ph": "M", "name": "thread_name", "pid": index + 1, "tid": thread,
                                  "args": {"name": f"{module} {thread}"}})
                if begin:
                    depth[thread] = depth.get(thread, 0) + 1
                    trace.append({"ph": "B", "pid": index + 1, "tid": thread, "ts": time / TICKS_PER_US,
                                  "name": names.get(name, f"#{name:08x}")})
                elif depth.get(thread, 0) > 0:
                    depth[thread] -= 1
                    trace.append({"ph": "E", "pid": index + 1, "tid": thread, "ts": time / TICKS_PER_US})
    return trace


CLOCK_MONOTONIC = 3  # Perfetto's BUILTIN_CLOCK_MONOTONIC
CLOCK_BOOTTIME = 6
SEQUENCE = 0x5256  # trusted_packet_sequence_id of every packet this writes
TYPE_BEGIN, TYPE_END = 1, 2
SEQ_INCREMENTAL_STATE_CLEARED = 1


def varint(value):
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def field_varint(number, value):
    return varint(number << 3) + varint(value)


def field_bytes(number, payload):
    return varint(number << 3 | 2) + varint(len(payload)) + payload


def packet(body):
    """One TracePacket as a Trace's repeated field 1."""
    return field_bytes(1, body + field_varint(10, SEQUENCE))


def protobuf(pid, trace):
    """The JSON events as TrackEvents on host-thread tracks, stamped with CLOCK_MONOTONIC."""
    out = bytearray()
    # Perfetto places a stamp only through a snapshot naming its clock: the
    # two clocks read side by side now, BOOTTIME being what tracebox keeps.
    boottime = clock.clock_gettime_ns(clock.CLOCK_BOOTTIME)
    monotonic = clock.clock_gettime_ns(clock.CLOCK_MONOTONIC)
    snapshot = field_bytes(1, field_varint(1, CLOCK_BOOTTIME) + field_varint(2, boottime)) + \
        field_bytes(1, field_varint(1, CLOCK_MONOTONIC) + field_varint(2, monotonic)) + \
        field_varint(2, CLOCK_BOOTTIME)
    out += packet(field_bytes(6, snapshot))
    tracks = {}
    first = True
    for event in trace:
        if event["ph"] == "M" and event["name"] == "process_name":
            owner = pid if event["pid"] < GUEST_PID_BASE else event["pid"]
            process = field_varint(1, owner) + field_bytes(6, event["args"]["name"].encode())
            out += packet(field_bytes(60, field_varint(1, owner << 32) + field_bytes(3, process)))
    for event in trace:
        if event["ph"] == "M":
            continue
        tid = event["tid"]
        owner = pid if event["pid"] < GUEST_PID_BASE else event["pid"]
        if (owner, tid) not in tracks:
            uuid = (owner << 32) | tid
            tracks[(owner, tid)] = uuid
            thread = field_varint(1, owner) + field_varint(2, tid)
            descriptor = field_varint(1, uuid) + field_bytes(4, thread)
            out += packet(field_bytes(60, descriptor))
        track_event = field_varint(9, TYPE_BEGIN if event["ph"] == "B" else TYPE_END) + field_varint(11, tracks[(owner, tid)])
        if event["ph"] == "B":
            track_event += field_bytes(23, event["name"].encode())
        body = field_varint(8, int(event["ts"] * 1000)) + field_varint(58, CLOCK_MONOTONIC) + field_bytes(11, track_event)
        if first:
            body += field_varint(13, SEQ_INCREMENTAL_STATE_CLEARED)
            first = False
        out += packet(body)
    return bytes(out)


GUEST_PID_BASE = 0x40000000   # guest processes' track pids, clear of the host's
CLOCK_PAIR = re.compile(r"av-clock host=(\d+) guest=(\d+)")


def guest_shell(work, *arguments):
    command = ["python3", "-B", str(ROOT / "tools" / "guest" / "guest_shell.py"), "--work", str(work), *arguments]
    return subprocess.run(command, capture_output=True, text=True, timeout=600).stdout


def guest_offset(work):
    """Host clock minus guest clock, both 100 ns: the smallest difference a
    batch's guest stamp and the host's clock as it ran it show."""
    pairs = CLOCK_PAIR.findall((pathlib.Path(work) / "qemu.log").read_text(errors="replace"))
    if not pairs:
        raise SystemExit(f"no clock pairs in {work}/qemu.log: was QEMU started with AV_PERFETTO=1?")
    return min(int(host) - int(guest) for host, guest in pairs[-2000:])


def freeze_guest(work, frozen=True):
    """Sets the freeze byte of every guest event file: its process stops
    recording, so what the files hold is the time before this. Cleared again
    once they are copied."""
    guest_shell(work, "ps", "Get-ChildItem C:\\ProgramData\\av\\events-*.bin | ForEach-Object { "
                            "$f = [IO.File]::Open($_.FullName, 'Open', 'ReadWrite', 'ReadWrite'); "
                            f"$f.Seek(24, 'Begin') | Out-Null; $f.WriteByte({1 if frozen else 0}); $f.Close() }}")


GUEST_EVENTS_KEPT = 1 << 20   # the newest events a guest thread's ring gives the capture


def guest_rings(work, scratch, kept=GUEST_EVENTS_KEPT):
    """Every guest process's recorded events, copied in the guest without the
    ring's unused slots and with only each slot's newest `kept` events, zipped
    and pulled: slot number, count, then the events oldest first."""
    listing = guest_shell(work, "ps", "Get-ChildItem C:\\ProgramData\\av\\events-*.bin | ForEach-Object { $_.Name }")
    for name in listing.split():
        if not name.endswith(".bin"):
            continue
        remote = f"C:\\ProgramData\\av\\{name}"
        script = (
            f"$in = [IO.File]::Open('{remote}', 'Open', 'Read', 'ReadWrite'); $r = New-Object IO.BinaryReader($in); "
            "$magic = $r.ReadUInt32(); $version = $r.ReadUInt32(); $slots = $r.ReadUInt32(); $per = $r.ReadUInt32(); "
            f"$out = [IO.File]::Create('{remote}.c.gz'); "
            "$zip = New-Object IO.Compression.GZipStream($out, [IO.Compression.CompressionMode]::Compress); "
            "$w = New-Object IO.BinaryWriter($zip); $w.Write([uint32]0x00435641); $w.Write([uint32]$slots); "
            "for ($slot = 0; $slot -lt $slots; $slot++) { "
            "  $base = 32 + [int64]$slot * (16 + [int64]$per * 16); $in.Seek($base, 'Begin') | Out-Null; "
            "  $written = $r.ReadUInt64(); if ($written -eq 0) { continue } "
            f"  $n = [Math]::Min([Math]::Min($written, [uint64]$per), [uint64]{kept}); $first = $written - $n; "
            "  $w.Write([uint32]$slot); $w.Write([uint32]$n); "
            "  $at = $first % $per; $head = [Math]::Min($n, [uint64]$per - $at); "
            "  $buffer = New-Object byte[] ([int]($head * 16)); $in.Seek($base + 16 + [int64]$at * 16, 'Begin') | Out-Null; "
            "  $in.Read($buffer, 0, $buffer.Length) | Out-Null; $w.Write($buffer); "
            "  if ($n -gt $head) { $rest = New-Object byte[] ([int](($n - $head) * 16)); "
            "    $in.Seek($base + 16, 'Begin') | Out-Null; $in.Read($rest, 0, $rest.Length) | Out-Null; $w.Write($rest) } } "
            "$w.Close(); $in.Close()")
        local = scratch / f"{name}.c.gz"
        # A copy can come back empty once and whole the next time; a process
        # that ended mid-copy, or a file from an older driver, never gives a
        # readable one, and the others are still worth having.
        whole = False
        for _attempt in range(2):
            guest_shell(work, "ps", script)
            guest_shell(work, "pull", f"{remote}.c.gz", str(local))
            try:
                whole = local.exists() and len(gzip.decompress(local.read_bytes())) >= 8
            except (OSError, EOFError):
                whole = False
            if whole:
                break
        if whole:
            yield name, local
        else:
            print(f"guest: {name} gave no readable copy; left out", file=sys.stderr)


def read_compact(path):
    """The compact copy guest_rings pulls: per slot, its events oldest first,
    as read_ring yields them."""
    data = gzip.decompress(path.read_bytes())
    magic, _slots = struct.unpack_from("<II", data, 0)
    if magic != 0x00435641:
        raise SystemExit(f"{path}: not a compact event copy")
    at = 8
    while at < len(data):
        slot, count = struct.unpack_from("<II", data, at)
        at += 8
        events = []
        for index in range(count):
            time, name, word = struct.unpack_from("<QII", data, at + index * EVENT)
            events.append((time, name, word & ~BEGIN, bool(word & BEGIN)))
        at += count * EVENT
        yield slot, events


def collect_guest(work, seconds, names, scratch, end=None):
    """The guest processes' events in the `seconds` before `end` (the guest
    clock; the newest of them when None), on the host's clock, and where that
    window ends."""
    offset = guest_offset(work)
    rings = []
    latest = []
    for name, path in guest_rings(work, scratch):
        slots = list(read_compact(path))
        latest.append(max((events[-1][0] for _, events in slots if events), default=0))
        rings.append((name, slots))
    # The freeze reaches the files one at a time, hundreds of milliseconds
    # apart: the window ends where the first of the live processes stopped.
    live_within = 2 * 1_000_000 * TICKS_PER_US
    if end is not None:
        newest = end
    else:
        newest = min(stamp for stamp in latest if stamp >= max(latest) - live_within)
    end = newest + offset
    since = newest - int(seconds * 1_000_000 * TICKS_PER_US)
    trace = []
    for name, slots in rings:
        stem = name[len("events-"):-len(".bin")]
        module, _, pid_text = stem.rpartition("-")
        pid = GUEST_PID_BASE | int(pid_text)
        trace.append({"ph": "M", "name": "process_name", "pid": pid, "args": {"name": f"guest {module} {pid_text}"}})
        named = set()
        for _, events in slots:
            depth = {}
            for time, scope, thread, begin in events:
                if time < since or time > newest:
                    continue
                at = time + offset
                if thread not in named:
                    named.add(thread)
                    trace.append({"ph": "M", "name": "thread_name", "pid": pid, "tid": thread,
                                  "args": {"name": f"{module} {thread}"}})
                if begin:
                    depth[thread] = depth.get(thread, 0) + 1
                    trace.append({"ph": "B", "pid": pid, "tid": thread, "ts": at / TICKS_PER_US,
                                  "name": names.get(scope, f"#{scope:08x}")})
                elif depth.get(thread, 0) > 0:
                    depth[thread] -= 1
                    trace.append({"ph": "E", "pid": pid, "tid": thread, "ts": at / TICKS_PER_US})
    return trace, end


def stop_host(pid, stop):
    """Every host ring of `pid` stops recording at `stop` (host clock, 100 ns;
    0 records again)."""
    for module in MODULES:
        path = pathlib.Path(f"/dev/shm/av-events-{module}-{pid}")
        if path.exists():
            with open(path, "r+b") as handle:
                handle.seek(24)
                handle.write(struct.pack("<Q", stop))


def stop_guest(work, stop):
    """Every guest event file stops recording at `stop` (the guest's clock)."""
    guest_shell(work, "ps", "Get-ChildItem C:\\ProgramData\\av\\events-*.bin | ForEach-Object { "
                            "$f = [IO.File]::Open($_.FullName, 'Open', 'ReadWrite', 'ReadWrite'); "
                            f"$f.Seek(24, 'Begin') | Out-Null; $f.Write([BitConverter]::GetBytes([uint64]{stop}), 0, 8); "
                            "$f.Close() }")


def arm_guest(work, events_per_slot, depth, only=""):
    """Guest processes started from now on record events, `events_per_slot`
    per thread, of scopes at most `depth` deep (0: all) - or with `only`
    (comma-separated scope names), just those, at any depth."""
    guest_shell(work, "ps", "Set-Content -NoNewline -Path C:\\ProgramData\\av\\perfetto "
                            f"-Value '{events_per_slot} {depth} {only}'")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("pid", type=int, help="the QEMU process")
    parser.add_argument("-o", "--output", type=pathlib.Path, default=pathlib.Path("av-trace.json"))
    parser.add_argument("--seconds", type=float, default=2.0, help="how much of the newest history to keep")
    parser.add_argument("--merge", type=pathlib.Path, help="a tracebox .pftrace to append the scopes to")
    parser.add_argument("--lead", type=float, default=3.0,
                        help="with --guest: how far ahead both sides are told to stop, time enough for the guest "
                             "to be told")
    parser.add_argument("--arm", type=int, metavar="EVENTS", default=0,
                        help="with --guest: only make guest processes started from now on record, EVENTS per "
                             "thread, and exit")
    parser.add_argument("--depth", type=int, default=2,
                        help="with --arm: the deepest scope guest processes record (0: all; the host's is "
                             "AV_TRACE_DEPTH in QEMU's environment)")
    parser.add_argument("--only", default="", metavar="SCOPES",
                        help="with --arm: the only scopes guest processes record, comma-separated "
                             "'Class::method' names, at any depth (the host's is AV_TRACE_ONLY)")
    parser.add_argument("--guest", type=pathlib.Path, metavar="WORK",
                        help="the run's work directory: add the guest processes' timelines too (they record while "
                             "C:\\ProgramData\\av\\perfetto exists when they start)")
    args = parser.parse_args()
    names = av_inspect.ScopeNames().load()
    now = clock.clock_gettime_ns(clock.CLOCK_MONOTONIC) // 100
    guest = []
    if args.guest is not None and args.arm:
        arm_guest(args.guest, args.arm, args.depth, args.only)
        print(f"guest processes started from now record {args.arm} events per thread, {args.depth} deep")
        return
    if args.guest is not None:
        # Both sides stop at one instant, far enough ahead for the guest to be
        # told: a ring holds well under a second of a busy thread, so windows
        # cut at different moments would not overlap.
        offset = guest_offset(args.guest)
        stop = now + int(args.lead * 1_000_000 * TICKS_PER_US)
        stop_host(args.pid, stop)
        stop_guest(args.guest, stop - offset)
        wait = (stop - clock.clock_gettime_ns(clock.CLOCK_MONOTONIC) // 100) / (1_000_000 * TICKS_PER_US) + 0.2
        if wait > 0:
            clock.sleep(wait)
        scratch = args.output.parent / "guest-events"
        scratch.mkdir(parents=True, exist_ok=True)
        guest, now = collect_guest(args.guest, args.seconds, names, scratch, end=stop - offset)
        stop_guest(args.guest, 0)
        now = stop
    trace = collect(args.pid, args.seconds, names, now) + guest
    if args.guest is not None:
        stop_host(args.pid, 0)
    if args.output.suffix == ".pftrace" or args.merge is not None:
        system = args.merge.read_bytes() if args.merge is not None else b""
        args.output.write_bytes(system + protobuf(args.pid, trace))
    else:
        args.output.write_text(json.dumps({"traceEvents": trace, "displayTimeUnit": "ms"}))
    print(f"{args.output}: {len(trace)} events; open it at https://ui.perfetto.dev")


if __name__ == "__main__":
    main()
