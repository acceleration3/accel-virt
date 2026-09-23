#!/usr/bin/env python3
"""A timeline of a QEMU process's device and executor scopes, for ui.perfetto.dev.

Run QEMU with RVK_PERFETTO=1 in its environment (tools/guest/run_guest_suite.py
--perfetto does) and every AV_SCOPE on the host is also an event in
/dev/shm/rvk-events-<module>-<pid>, a ring per thread holding its newest
events. This reads those rings, keeps the last --seconds, names each scope the
way tools/mcp/rvk_inspect.py does, and writes Chrome trace-event JSON, which
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
import json
import mmap
import time as clock
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "mcp"))
import rvk_inspect  # noqa: E402

MAGIC = 0x45564B52
HEADER = 32
SLOT_HEADER = 16
EVENT = 16
BEGIN = 0x80000000
MODULES = ("device", "executor")
TICKS_PER_US = 10  # the trace host's clock counts 100 ns


def read_ring(path):
    """Per slot, its events oldest first, as (time, name, thread, begin)."""
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
        kept = min(written, per_slot)
        first = written - kept
        events = []
        for index in range(first, written):
            at = base + SLOT_HEADER + (index % per_slot) * EVENT
            time, name, word = struct.unpack_from("<QII", data, at)
            events.append((time, name, word & ~BEGIN, bool(word & BEGIN)))
        yield slot, events


def collect(pid, seconds, names):
    trace = []
    newest = 0
    rings = {}
    for module in MODULES:
        path = pathlib.Path(f"/dev/shm/rvk-events-{module}-{pid}")
        if path.exists():
            rings[module] = list(read_ring(path))
            for _, events in rings[module]:
                if events:
                    newest = max(newest, events[-1][0])
    # A slot a thread is writing while this reads can hand back a torn event;
    # the window ends at the host's clock now, which no real event passes.
    now = clock.clock_gettime_ns(clock.CLOCK_MONOTONIC) // 100
    newest = min(newest, now)
    if not rings:
        raise SystemExit(f"no /dev/shm/rvk-events-*-{pid}: was QEMU started with RVK_PERFETTO=1?")
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
        if event["ph"] == "M":
            continue
        tid = event["tid"]
        if tid not in tracks:
            uuid = (pid << 32) | tid
            tracks[tid] = uuid
            thread = field_varint(1, pid) + field_varint(2, tid)
            descriptor = field_varint(1, uuid) + field_bytes(4, thread)
            out += packet(field_bytes(60, descriptor))
        track_event = field_varint(9, TYPE_BEGIN if event["ph"] == "B" else TYPE_END) + field_varint(11, tracks[tid])
        if event["ph"] == "B":
            track_event += field_bytes(23, event["name"].encode())
        body = field_varint(8, int(event["ts"] * 1000)) + field_varint(58, CLOCK_MONOTONIC) + field_bytes(11, track_event)
        if first:
            body += field_varint(13, SEQ_INCREMENTAL_STATE_CLEARED)
            first = False
        out += packet(body)
    return bytes(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("pid", type=int, help="the QEMU process")
    parser.add_argument("-o", "--output", type=pathlib.Path, default=pathlib.Path("rvk-trace.json"))
    parser.add_argument("--seconds", type=float, default=2.0, help="how much of the newest history to keep")
    parser.add_argument("--merge", type=pathlib.Path, help="a tracebox .pftrace to append the scopes to")
    args = parser.parse_args()
    names = rvk_inspect.ScopeNames().load()
    trace = collect(args.pid, args.seconds, names)
    if args.output.suffix == ".pftrace" or args.merge is not None:
        system = args.merge.read_bytes() if args.merge is not None else b""
        args.output.write_bytes(system + protobuf(args.pid, trace))
    else:
        args.output.write_text(json.dumps({"traceEvents": trace, "displayTimeUnit": "ms"}))
    print(f"{args.output}: {len(trace)} events; open it at https://ui.perfetto.dev")


if __name__ == "__main__":
    main()
