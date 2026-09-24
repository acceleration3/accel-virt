#!/usr/bin/env python3
"""Where a frame's time goes, across the guest and the host, from one capture.

Reads a trace tools/perfetto_trace.py wrote with --guest (the guest's scopes
and the host's on one clock) and cuts the busiest guest thread into frames at
a marker scope - D3d11Device::flush by default, which the samples' runner
calls once a frame. For each frame it adds up:

  guest   waiting   the driver waiting on the host: GetData polls and synchronous
                    calls (the scopes in WAITS)
          driver    every other top-level driver call
          outside   the rest of the frame: the application and the runtime
  host    busy      the busiest host worker running batches (RingConsumer::service)
          idle      the worker with nothing to run

and, per query the frame ends and waits for, the chain its result travels:

  sent      the guest's EndQuery call to the host running it
  gpu       the host running it to the host writing the result into the slot
  seen      the result written to the guest's GetData returning with it

The biggest average is where the frame goes; the chain says which leg of a
round trip is slow. Scopes cost a little each, so absolute numbers are a bit
high; the tracer's own share is printed from the scope count.

    tools/frame_breakdown.py build-perfetto/occ4.pftrace
    tools/frame_breakdown.py trace.pftrace --marker D3d11Device::flush --top 15
"""
import argparse
import bisect
import csv
import io
import pathlib
import statistics
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PROCESSOR = ROOT / "build-perfetto" / "trace_processor"
WAITS = ("D3d11Device::query_get_data", "KmtTransport::await", "KmtTransport::drain",
         "KmtTransport::room_within_spin")
TRACER_NS_PER_SCOPE = 60   # a scope's enter and leave events, measured on this host


def query(trace, sql):
    """Rows of `sql` over `trace`, as dicts."""
    out = subprocess.run([str(PROCESSOR), "-q", "/dev/stdin", str(trace)], input=sql, capture_output=True,
                         text=True, timeout=900)
    if out.returncode != 0:
        raise SystemExit(f"trace_processor failed: {out.stderr.strip()[-400:]}")
    return list(csv.DictReader(io.StringIO(out.stdout)))


THREADS = """
select t.utid, t.tid, coalesce(p.name, '') process, count(*) slices
from slice s join thread_track tt on s.track_id = tt.id join thread t using(utid) left join process p using(upid)
group by t.utid order by slices desc;
"""


def top_level(trace, utid):
    return query(trace, f"""
select s.ts, s.dur, s.name from slice s join thread_track tt on s.track_id = tt.id
where tt.utid = {utid} and s.depth = 0 order by s.ts;""")


def named(trace, utid, name):
    return [int(row["ts"]) for row in query(trace, f"""
select s.ts from slice s join thread_track tt on s.track_id = tt.id
where tt.utid = {utid} and s.name = '{name}' order by s.ts;""")]


def self_times(trace, utid, top):
    return query(trace, f"""
with own as (
  select s.id, s.name, s.dur - coalesce((select sum(c.dur) from slice c where c.parent_id = s.id), 0) self
  from slice s join thread_track tt on s.track_id = tt.id where tt.utid = {utid})
select name, count(*) n, round(sum(self) / 1e6, 2) self_ms from own group by name order by self_ms desc limit {top};""")


def scope_count(trace, utid):
    return int(query(trace, f"select count(*) n from slice s join thread_track tt on s.track_id = tt.id "
                            f"where tt.utid = {utid};")[0]["n"])


def pick_threads(trace):
    threads = query(trace, THREADS)
    guest = next((t for t in threads if t["process"].startswith("guest")), None)
    host = next((t for t in threads if not t["process"].startswith("guest")), None)
    if guest is None or host is None:
        raise SystemExit("the trace needs a guest thread and a host thread: capture with perfetto_trace.py --guest")
    return guest, host


def frames_of(markers):
    return list(zip(markers, markers[1:]))


class Slices:
    """A thread's top-level slices, sorted, for summing the parts of them that
    fall in a window without walking all of them."""

    def __init__(self, rows):
        self.ts = [int(row["ts"]) for row in rows]
        self.dur = [int(row["dur"]) for row in rows]
        self.name = [row["name"] for row in rows]
        self.longest = max(self.dur, default=0)

    def overlap(self, start, end, names=None, exclude=()):
        total = 0
        for index in range(bisect.bisect_left(self.ts, start - self.longest), bisect.bisect_left(self.ts, end)):
            ts, dur, name = self.ts[index], self.dur[index], self.name[index]
            if ts + dur <= start or name in exclude or (names is not None and name not in names):
                continue
            total += min(ts + dur, end) - max(ts, start)
        return total


def first_at_or_after(values, at):
    index = bisect.bisect_left(values, at)
    return values[index] if index < len(values) else None


def chain(trace, guest_utid, host_utid, frames):
    """Per frame, the last query end's legs: guest end to host end, host end to
    the host writing the result, and the write to the guest's GetData seeing it."""
    ends = named(trace, guest_utid, "D3d11Device::query_end")
    host_ends = named(trace, host_utid, "D3dBackend::on_EndQuery")
    writes = named(trace, host_utid, "QueryResultPublisher::write")
    polls = query(trace, f"""
select s.ts + s.dur done from slice s join thread_track tt on s.track_id = tt.id
where tt.utid = {guest_utid} and s.name = 'D3d11Device::query_get_data' order by s.ts;""")
    polls = [int(row["done"]) for row in polls]
    legs = {"sent": [], "gpu": [], "seen": []}
    for start, end in frames:
        index = bisect.bisect_left(ends, end) - 1
        if index < 0 or ends[index] < start:
            continue
        guest_end = ends[index]
        host_end = first_at_or_after(host_ends, guest_end)
        if host_end is None or host_end > end:
            continue
        write = first_at_or_after(writes, host_end)
        if write is None or write > end:
            continue
        seen = first_at_or_after(polls, write)
        if seen is None or seen > end:
            continue
        legs["sent"].append(host_end - guest_end)
        legs["gpu"].append(write - host_end)
        legs["seen"].append(seen - write)
    return legs


def us(values):
    return f"{statistics.mean(values) / 1000:8.1f} us" if values else "       -   "


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("trace", type=pathlib.Path)
    parser.add_argument("--marker", default="D3d11Device::flush", help="the guest scope that starts each frame")
    parser.add_argument("--top", type=int, default=12, help="how many functions of each side's own time to list")
    args = parser.parse_args()
    guest, host = pick_threads(args.trace)
    guest_utid, host_utid = int(guest["utid"]), int(host["utid"])
    markers = named(args.trace, guest_utid, args.marker)
    frames = frames_of(markers)
    if not frames:
        raise SystemExit(f"no two {args.marker} on the guest thread {guest['tid']}: is it the right marker?")
    guest_slices = Slices(top_level(args.trace, guest_utid))
    host_slices = Slices(top_level(args.trace, host_utid))
    window = (frames[0][0], frames[-1][1])
    rows = {"guest waiting": [], "guest driver": [], "guest outside": [], "host busy": [], "frame": []}
    for start, end in frames:
        length = end - start
        waiting = guest_slices.overlap(start, end, names=WAITS)
        driver = guest_slices.overlap(start, end, exclude=WAITS)
        rows["frame"].append(length)
        rows["guest waiting"].append(waiting)
        rows["guest driver"].append(driver)
        rows["guest outside"].append(max(0, length - waiting - driver))
        rows["host busy"].append(host_slices.overlap(start, end, names=("RingConsumer::service",)))
    legs = chain(args.trace, guest_utid, host_utid, frames)

    frame = statistics.mean(rows["frame"])
    print(f"guest thread {guest['tid']} ({guest['process']}), host worker {host['tid']} ({host['process']})")
    print(f"{len(frames)} frames, {frame / 1000:.1f} us each ({1e9 / frame:.0f} fps traced)\n")
    print("per frame                     mean      share")
    for name in ("guest waiting", "guest driver", "guest outside", "host busy"):
        mean = statistics.mean(rows[name])
        print(f"  {name:22} {mean / 1000:8.1f} us  {100 * mean / frame:5.1f}%")
    print(f"  {'host idle':22} {(frame - statistics.mean(rows['host busy'])) / 1000:8.1f} us")
    if any(legs.values()):
        print(f"\nthe frame's last query ({len(legs['sent'])} frames with the whole chain)")
        print(f"  sent   guest EndQuery -> host runs it      {us(legs['sent'])}")
        print(f"  gpu    host runs it -> result in the slot  {us(legs['gpu'])}")
        print(f"  seen   result in the slot -> guest has it  {us(legs['seen'])}")
    span = window[1] - window[0]
    for label, utid in (("guest", guest_utid), ("host", host_utid)):
        count = scope_count(args.trace, utid)
        print(f"\n{label}: own time by function (tracer ~{100 * count * TRACER_NS_PER_SCOPE / max(span, 1):.0f}% "
              f"of the window)")
        for row in self_times(args.trace, utid, args.top):
            print(f"  {float(row['self_ms']):8.2f} ms  {int(row['n']):7d}x  {row['name']}")


if __name__ == "__main__":
    main()
