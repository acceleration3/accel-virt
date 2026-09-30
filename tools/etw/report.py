"""The frame-pacing report: what the trace's DWM frames did, whether the
capture rule explains every skipped capture, and where the render thread's
time went in the frames a skip was measured against, beside normal frames."""
import collections

from etw.dwm import median, percentile


def lines_for(trace, frames, symbols, sample_ms, top=15, probe=None):
    out = []
    ms = trace.ms
    kernel = trace.kernel
    period = median([b - a for a, b in zip(frames.vblanks, frames.vblanks[1:])])
    span = ms(trace.events[-1].time - trace.events[0].time) if trace.events else 0.0
    out.append(f"trace: {span / 1000:.2f} s, {len(kernel.switches)} CPUs, {len(frames.vblanks)} vblanks every "
               f"{ms(period):.3f} ms ({1000 / ms(period) if period else 0:.2f} Hz), lost buffers {trace.lost}")
    if probe:
        out.append(f"probe: {probe}")
    if frames.render_tid is None:
        out.append("no DWM frames in the trace (Dwm-Core's Scheduling keyword off?)")
        return out
    wanted = [frame for frame in frames.frames if frame.captured or frame.delayed]
    skipped = [frame for frame in wanted if frame.delayed and not frame.captured]
    out.append(f"DWM pid {frames.pid}, render thread {frames.render_tid}: {len(frames.frames)} frames, "
               f"capture flip manager {frames.capture_manager}: {sum(frame.captured for frame in frames.frames)} "
               f"captured, {len(skipped)} skipped (DelayedRendering)")
    agree, unexplained, false_alarms, quiet = frames.rule_agreement()
    out.append(f"rule: a capture check less than {frames.interval_ms:.1f} ms after the last captured one is skipped "
               f"- explains {agree} of {agree + unexplained} skips; {false_alarms} predicted skips were captured")
    before, skipped_frames, normal = frames.groups()
    out.append("")
    out.append("vblank -> capture check (ms)      median     p90     max")
    for label, group in (("normal frames", normal), ("frames a skip was measured from", before),
                         ("skipped frames", skipped_frames)):
        phases = [frame.phase_ms(ms) for frame in group]
        if phases:
            out.append(f"  {label:32s} {median(phases):6.2f}  {percentile(phases, 0.9):6.2f}  {max(phases):6.2f}"
                       f"   ({len(group)} frames)")
    if not before:
        return out
    out.append("")
    out.append("the render thread, vblank -> capture check (ms a frame)   normal   before a skip")
    rows = [("running", lambda frame: frame.account.running - frame.account.interrupted),
            ("  interrupted by DPCs and ISRs", lambda frame: frame.account.interrupted),
            ("ready, waiting for a CPU", lambda frame: frame.account.ready),
            ("not yet seen by the trace", lambda frame: frame.account.unknown)]
    reasons = sorted(set().union(*[frame.account.blocked for frame in normal + before]))
    for reason in reasons:
        rows.append((f"blocked: {reason}", lambda frame, reason=reason: frame.account.blocked.get(reason, 0.0)))
    for label, value in rows:
        normal_ms = mean([value(frame) for frame in normal])
        before_ms = mean([value(frame) for frame in before])
        if normal_ms >= 0.005 or before_ms >= 0.005:
            out.append(f"  {label:44s} {normal_ms:7.3f}  {before_ms:7.3f}")
    out.extend(call_rows(normal, before, top))
    out.extend(tracer_rows(frames.pid, normal, before, symbols, sample_ms))
    out.extend(sample_rows(frames.pid, normal, before, symbols, sample_ms, top))
    out.append("")
    out.append("each skip: the frame it was measured from, and the skipped one")
    out.append("  skipped at (s)  elapsed   phase before  phase skipped   running before")
    last_captured = None
    for frame in frames.frames:
        if frame.delayed and not frame.captured and last_captured and frame.account and last_captured.account:
            out.append(f"  {ms(frame.check - trace.events[0].time) / 1000:13.3f}  {frame.elapsed_ms:6.2f}   "
                       f"{last_captured.phase_ms(ms):11.2f}  {frame.phase_ms(ms):12.2f}   "
                       f"{last_captured.account.running:13.2f}")
        if frame.captured:
            last_captured = frame
    return out


def mean(values):
    return sum(values) / len(values) if values else 0.0


def call_rows(normal, before, top):
    out = []
    functions = collections.Counter()
    for frame in normal + before:
        for function, spent in frame.calls.items():
            functions[function] += spent
    if not functions:
        return out
    out.append("  in dxgkrnl (of running):")
    for function, _ in functions.most_common(top):
        normal_ms = mean([frame.calls.get(function, 0.0) for frame in normal])
        before_ms = mean([frame.calls.get(function, 0.0) for frame in before])
        out.append(f"    {function:42s} {normal_ms:7.3f}  {before_ms:7.3f}")
    return out


TRACER_FUNCTIONS = ("StackWalk", "RtlpUnwind", "RtlVirtualUnwind", "RtlWalkFrameChain", "EtwpStackTrace",
                    "EtwpLogKernelEvent", "EtwpTraceStackWalk")


def tracer_rows(pid, normal, before, symbols, sample_ms):
    """The trace's own cost on the thread: the kernel walking its stack for
    each sample and switch, which a run without the trace does not pay."""
    if symbols is None:
        return []

    def cost(group):
        count = sum(1 for frame in group for sample in frame.samples
                    if any(part in symbols.name(pid, sample.address) for part in TRACER_FUNCTIONS))
        return count * sample_ms / max(len(group), 1)

    return [f"  of which the trace's own stack walks                  {cost(normal):7.3f}  {cost(before):7.3f}"]


def sample_rows(pid, normal, before, symbols, sample_ms, top):
    """Where the CPU samples of the two groups landed, as ms a frame, and the
    functions the extra time of the slow frames was spent in."""
    if symbols is None:
        return []
    out = []

    def per_frame(group, key):
        totals = collections.Counter()
        for frame in group:
            for sample in frame.samples:
                for name in key(sample):
                    totals[name] += 1
        return {name: count * sample_ms / max(len(group), 1) for name, count in totals.items()}

    def own(sample):
        return [symbols.name(pid, sample.address)]

    def module(sample):
        return [symbols.name(pid, sample.address).split("!")[0]]

    def inclusive(sample):
        return {symbols.name(pid, frame) for frame in sample.stack} or {symbols.name(pid, sample.address)}

    for title, key in (("by module (own time)", module), ("by function (own time)", own),
                       ("by function, callees included", inclusive)):
        normal_ms = per_frame(normal, key)
        before_ms = per_frame(before, key)
        ranked = sorted(set(normal_ms) | set(before_ms),
                        key=lambda name: before_ms.get(name, 0.0) - normal_ms.get(name, 0.0), reverse=True)
        out.append(f"  CPU samples every {sample_ms:.3f} ms, {title}; most grown first:")
        for rank, name in enumerate(ranked[:top]):
            out.append(f"    {name[:70]:70s} {normal_ms.get(name, 0.0):7.3f}  {before_ms.get(name, 0.0):7.3f}")
            if key is own and rank < CALLED_FROM_TOP:
                out.extend(called_from(pid, name, normal + before, symbols))
    return out


CALLED_FROM_TOP = 5
CALLER_DEPTH = 4


def called_from(pid, name, group, symbols):
    """The commonest chains of callers into `name` among the group's samples."""
    chains = collections.Counter()
    for frame in group:
        for sample in frame.samples:
            if symbols.name(pid, sample.address) != name or len(sample.stack) < 2:
                continue
            callers = [symbols.name(pid, address) for address in sample.stack[1:]]
            callers = [caller for caller in callers if caller != name][:CALLER_DEPTH]
            chains[" <- ".join(callers)] += 1
    total = sum(chains.values())
    return [f"        {100 * count / total:3.0f}% <- {chain[:110]}" for chain, count in chains.most_common(2)
            if total and chain]
