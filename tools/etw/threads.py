"""What one thread was doing over time, from the kernel's context switches and
readied-thread events: running (and on which processor), ready (runnable,
waiting for a processor), or blocked (waiting, and for what) - and, while it
ran, how much of that time interrupts and DPCs took from it.

    timeline = ThreadTimeline(kernel, tid, trace.ms)
    timeline.account(start, end)      # Account: ms running, ready, blocked by reason, interrupted
    timeline.spans(start, end)        # [(start, end, state, detail)] for drawing
"""
import bisect
import collections
from dataclasses import dataclass, field

PREEMPTED_STATES = ("Ready", "Standby", "DeferredReady")
OPEN_END = 1 << 63     # the state after a thread's last switch lasts past the trace


@dataclass
class Account:
    running: float = 0.0            # on a processor, interrupts included
    interrupted: float = 0.0        # of which DPCs and ISRs had the processor
    ready: float = 0.0              # runnable, no processor given
    blocked: dict = field(default_factory=lambda: collections.defaultdict(float))   # wait reason -> ms
    unknown: float = 0.0            # before the trace saw the thread switch

    def add(self, other):
        self.running += other.running
        self.interrupted += other.interrupted
        self.ready += other.ready
        self.unknown += other.unknown
        for reason, ms in other.blocked.items():
            self.blocked[reason] += ms


class ThreadTimeline:
    def __init__(self, kernel, tid, to_ms):
        self.kernel = kernel
        self.tid = tid
        self.to_ms = to_ms
        self.interrupt_starts = {}
        self.spans_list = self.build()
        self.starts = [span[0] for span in self.spans_list]

    def build(self):
        """[(start, end, state, detail)]: 'running' with the processor, 'ready',
        or 'blocked' with the wait reason; covering from the thread's first
        switch to its last."""
        switches = []
        for cpu, per_cpu in self.kernel.switches.items():
            for switch in per_cpu:
                if switch.new_tid == self.tid:
                    switches.append((switch.time, "in", switch))
                elif switch.old_tid == self.tid:
                    switches.append((switch.time, "out", switch))
        readies = [(ready.time, "ready", ready) for ready in self.kernel.readies if ready.tid == self.tid]
        changes = sorted(switches + readies, key=lambda item: item[0])
        spans = []
        state, detail, since = None, None, None
        for time, kind, item in changes:
            if kind == "in":
                if state is not None:
                    spans.append((since, time, state, detail))
                state, detail, since = "running", item.cpu, time
            elif kind == "out":
                if state is not None:
                    spans.append((since, time, state, detail))
                if item.old_state in PREEMPTED_STATES:
                    state, detail = "ready", "preempted"
                else:
                    state, detail = "blocked", item.wait_reason
                since = time
            elif kind == "ready" and state == "blocked":
                spans.append((since, time, state, detail))
                state, detail, since = "ready", "woken", time
        if state is not None:
            spans.append((since, OPEN_END, state, detail))
        return spans

    def spans(self, start, end):
        index = max(bisect.bisect_right(self.starts, start) - 1, 0)
        chosen = []
        for span in self.spans_list[index:]:
            if span[0] >= end:
                break
            if span[1] > start:
                chosen.append((max(span[0], start), min(span[1], end), span[2], span[3]))
        return chosen

    def account(self, start, end):
        account = Account()
        covered = 0.0
        for span_start, span_end, state, detail in self.spans(start, end):
            ms = self.to_ms(span_end - span_start)
            covered += ms
            if state == "running":
                account.running += ms
                account.interrupted += self.interrupted(detail, span_start, span_end)
            elif state == "ready":
                account.ready += ms
            else:
                account.blocked[detail] += ms
        account.unknown = max(self.to_ms(end - start) - covered, 0.0)
        return account

    def interrupted(self, cpu, start, end):
        """ms of DPCs and ISRs on `cpu` within [start, end)."""
        interrupts = self.kernel.interrupts.get(cpu, [])
        if cpu not in self.interrupt_starts:
            self.interrupt_starts[cpu] = [interrupt.start for interrupt in interrupts]
        starts = self.interrupt_starts[cpu]
        index = max(bisect.bisect_right(starts, start) - 1, 0)
        total = 0
        for interrupt in interrupts[index:]:
            if interrupt.start >= end:
                break
            overlap = min(interrupt.end, end) - max(interrupt.start, start)
            if overlap > 0:
                total += overlap
        return self.to_ms(total)

