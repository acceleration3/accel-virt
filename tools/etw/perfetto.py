import json

CPUS_PID, DWM_PID = 1, 2


def export(path, trace, frames, timeline, symbols):
    origin = trace.events[0].time if trace.events else 0
    kernel = trace.kernel

    def us(ticks):
        return trace.ms(ticks - origin) * 1000.0

    def name_of(tid):
        pid = kernel.thread_process.get(tid, -1)
        thread = kernel.thread_names.get(tid, "")
        return f"{kernel.process_name(pid)} {tid}{' ' + thread if thread else ''}"

    events = [meta(CPUS_PID, 0, "process_name", "CPUs"), meta(DWM_PID, 0, "process_name", "DWM render thread")]
    for cpu in sorted(kernel.switches):
        events.append(meta(CPUS_PID, 2 * cpu + 1, "thread_name", f"CPU {cpu}"))
        events.append(meta(CPUS_PID, 2 * cpu + 2, "thread_name", f"CPU {cpu} DPC/ISR"))
        switches = kernel.switches[cpu]
        for switch, following in zip(switches, switches[1:]):
            if kernel.idle(switch.new_tid):
                continue
            events.append(slice_(CPUS_PID, 2 * cpu + 1, name_of(switch.new_tid), us(switch.time),
                                 us(following.time) - us(switch.time)))
        for interrupt in kernel.interrupts.get(cpu, []):
            events.append(slice_(CPUS_PID, 2 * cpu + 2, f"{interrupt.kind} {symbols.name(0, interrupt.routine)}",
                                 us(interrupt.start), us(interrupt.end) - us(interrupt.start)))
    if frames.render_tid is not None:
        rows = {"state": 1, "frames": 2, "stages": 3, "samples": 4}
        for label, row in rows.items():
            events.append(meta(DWM_PID, row, "thread_name", label))
        last = trace.events[-1].time if trace.events else 0
        for start, end, state, detail in timeline.spans_list:
            end = min(end, last)
            label = state if state != "blocked" else f"blocked: {detail}"
            if state == "running":
                label = f"running on CPU {detail}"
            events.append(slice_(DWM_PID, rows["state"], label, us(start), us(end) - us(start)))
        for frame in frames.frames:
            outcome = "captured" if frame.captured else ("skipped" if frame.delayed else "no capture")
            events.append(slice_(DWM_PID, rows["frames"], f"frame {frame.index} {outcome}", us(frame.start),
                                 us(frame.stop) - us(frame.start),
                                 {"phase_ms": frame.phase_ms(trace.ms), "elapsed_ms": frame.elapsed_ms,
                                  "predicted_skip": frame.predicted_skip}))
            marks = [("vblank to frame", frame.vblank, frame.start), ("compose", frame.start, frame.render),
                     ("render", frame.render, frame.first_submit), ("submit", frame.first_submit, frame.present),
                     ("present to capture check", frame.present, frame.check)]
            for label, start, end in marks:
                if start is not None and start and end and end >= start:
                    events.append(slice_(DWM_PID, rows["stages"], label, us(start), us(end) - us(start)))
            if frame.check and (frame.captured or frame.delayed):
                events.append(instant(DWM_PID, rows["frames"], "capture check: " + outcome, us(frame.check)))
            for sample in frame.samples:
                stack = [symbols.name(frames.pid, address) for address in sample.stack[:16]]
                events.append(instant(DWM_PID, rows["samples"], symbols.name(frames.pid, sample.address),
                                      us(sample.time), {"stack": stack}))
    for vblank in frames.vblanks:
        events.append({"name": "vblank", "ph": "i", "s": "g", "ts": us(vblank), "pid": CPUS_PID, "tid": 0})
    with open(path, "w", encoding="utf-8") as sink:
        json.dump({"traceEvents": events, "displayTimeUnit": "ms"}, sink)
    return len(events)


def meta(pid, tid, kind, name):
    return {"name": kind, "ph": "M", "pid": pid, "tid": tid, "args": {"name": name}}


def slice_(pid, tid, name, ts, duration, args=None):
    event = {"name": name, "ph": "X", "pid": pid, "tid": tid, "ts": ts, "dur": max(duration, 0.0)}
    if args:
        event["args"] = args
    return event


def instant(pid, tid, name, ts, args=None):
    event = {"name": name, "ph": "i", "s": "t", "pid": pid, "tid": tid, "ts": ts}
    if args:
        event["args"] = args
    return event
