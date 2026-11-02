import bisect
import collections
import statistics
from dataclasses import dataclass, field

from etw.threads import Account, ThreadTimeline

DWM_CORE = "Microsoft-Windows-Dwm-Core"
DXGKRNL = "Microsoft-Windows-DxgKrnl"
WIN32K = "Microsoft-Windows-Win32k"
DELAYED_RENDERING = 0x1
DEFAULT_MIN_UPDATE_INTERVAL_MS = 16.0


@dataclass
class Frame:
    index: int
    start: int
    stop: int
    vblank: int = None
    render: int = 0
    first_submit: int = 0
    present: int = 0
    check: int = 0
    delayed: bool = False
    captured: bool = False
    elapsed_ms: float = -1.0
    predicted_skip: bool = False
    account: Account = None
    calls: dict = field(default_factory=dict)
    samples: list = field(default_factory=list)

    def phase_ms(self, to_ms):
        return to_ms(self.check - self.vblank) if self.check and self.vblank is not None else None


class DwmFrames:
    def __init__(self, trace, min_update_interval_ms=DEFAULT_MIN_UPDATE_INTERVAL_MS):
        self.trace = trace
        self.interval_ms = min_update_interval_ms
        self.vblanks = self.find_vblanks()
        self.pid, self.render_tid = self.find_render_thread()
        self.capture_manager = self.find_capture_manager()
        self.frames = self.find_frames()
        self.apply_rule()

    def find_vblanks(self):
        counts = collections.Counter()
        for event in self.trace.named(DXGKRNL, "VSyncDPC"):
            counts[(event.fields.get("pDxgAdapter"), event.fields.get("VidPnSourceId"))] += 1
        if not counts:
            return []
        busiest = counts.most_common(1)[0][0]
        times = {event.fields["FrameQPCTime"] for event in self.trace.named(DXGKRNL, "VSyncDPC")
                 if (event.fields.get("pDxgAdapter"), event.fields.get("VidPnSourceId")) == busiest}
        return sorted(times)

    def find_render_thread(self):
        starts = self.trace.named(DWM_CORE, "SCHEDULE_PROCESS_FRAME/Start")
        if not starts:
            return None, None
        pid, tid = collections.Counter((event.pid, event.tid) for event in starts).most_common(1)[0][0]
        return pid, tid

    def find_capture_manager(self):
        posted = [event for event in self.trace.named(WIN32K, "FlipManagerPresentPosted") if event.pid == self.pid]
        if not posted:
            return None
        return collections.Counter(event.fields.get("FlipManagerId") for event in posted).most_common(1)[0][0]

    def find_frames(self):
        if self.render_tid is None:
            return []
        starts = [event.time for event in self.trace.named(DWM_CORE, "SCHEDULE_PROCESS_FRAME/Start")
                  if event.tid == self.render_tid]
        stops = [event.time for event in self.trace.named(DWM_CORE, "SCHEDULE_PROCESS_FRAME/Stop")
                 if event.tid == self.render_tid]
        posted = sorted(event.time for event in self.trace.named(WIN32K, "FlipManagerPresentPosted")
                        if event.pid == self.pid and event.fields.get("FlipManagerId") == self.capture_manager)
        frames = []
        for index, start in enumerate(starts):
            stop_index = bisect.bisect_left(stops, start)
            if stop_index >= len(stops):
                break
            stop = stops[stop_index]
            frame = Frame(index, start, stop)
            vblank_index = bisect.bisect_right(self.vblanks, start) - 1
            frame.vblank = self.vblanks[vblank_index] if vblank_index >= 0 else None
            self.fill_stages(frame)
            following = starts[index + 1] if index + 1 < len(starts) else stop + (stop - start) + 1
            first_post = bisect.bisect_left(posted, start)
            frame.captured = first_post < len(posted) and posted[first_post] < following
            frames.append(frame)
        return frames

    def fill_stages(self, frame):
        for event in self.trace.between(frame.start, frame.stop + 1):
            same_thread = event.tid == self.render_tid
            if event.provider == DWM_CORE:
                if event.name == "SCHEDULE_RENDER/Start" and not frame.render:
                    frame.render = event.time
                elif event.name == "LEGACYSWAPCHAIN_BUFFERFLIP" and not frame.check:
                    frame.check = event.time
                elif event.name == "SCHEDULED_COMPOSITION_REASON" and event.fields.get("Reasons", 0) & DELAYED_RENDERING:
                    frame.delayed = True
            elif event.provider == DXGKRNL and same_thread and event.name == "Profiler/Start":
                function = event.fields.get("Function_text", str(event.fields.get("Function")))
                if function == "DxgkSubmitCommand" and not frame.first_submit:
                    frame.first_submit = event.time
                elif function == "DxgkPresent" and not frame.present:
                    frame.present = event.time

    def apply_rule(self):
        last = None
        for frame in self.frames:
            if not frame.check or not (frame.captured or frame.delayed):
                continue
            if last is not None:
                frame.elapsed_ms = self.trace.ms(frame.check - last)
                frame.predicted_skip = frame.elapsed_ms < self.interval_ms
            if frame.captured:
                last = frame.check

    def rule_agreement(self):
        tally = collections.Counter()
        for frame in self.frames:
            if frame.elapsed_ms < 0:
                continue
            tally[(frame.predicted_skip, frame.delayed and not frame.captured)] += 1
        return tally[(True, True)], tally[(False, True)], tally[(True, False)], tally[(False, False)]

    def analyze(self, symbols=None):
        timeline = ThreadTimeline(self.trace.kernel, self.render_tid, self.trace.ms)
        samples = [sample for sample in self.trace.kernel.samples if sample.tid == self.render_tid]
        sample_times = [sample.time for sample in samples]
        for frame in self.frames:
            if not frame.check or frame.vblank is None:
                continue
            frame.account = timeline.account(frame.vblank, frame.check)
            frame.calls = self.calls(frame.vblank, frame.check)
            frame.samples = samples[bisect.bisect_left(sample_times, frame.vblank):
                                    bisect.bisect_left(sample_times, frame.check)]
        if symbols is not None:
            addresses = set()
            for frame in self.frames:
                for sample in frame.samples:
                    addresses.add(sample.address)
                    addresses.update(sample.stack)
            symbols.resolve(self.pid, addresses)
        return timeline

    def calls(self, start, end):
        spent = collections.defaultdict(float)
        open_calls = {}
        for event in self.trace.between(start, end):
            if event.tid != self.render_tid or event.provider != DXGKRNL:
                continue
            function = event.fields.get("Function_text", str(event.fields.get("Function")))
            if event.name == "Profiler/Start":
                open_calls[function] = event.time
            elif event.name == "Profiler/Stop" and function in open_calls:
                spent[function] += self.trace.ms(event.time - open_calls.pop(function))
        return dict(spent)

    def groups(self):
        before, skipped = set(), set()
        last_captured = None
        for frame in self.frames:
            if frame.delayed and not frame.captured and last_captured is not None:
                skipped.add(frame.index)
                before.add(last_captured.index)
            if frame.captured:
                last_captured = frame
        normal = [frame for frame in self.frames
                  if frame.account and frame.index not in before and frame.index not in skipped
                  and (frame.captured or frame.delayed)]
        pick = {frame.index: frame for frame in self.frames}
        return ([pick[index] for index in sorted(before) if pick[index].account],
                [pick[index] for index in sorted(skipped) if pick[index].account], normal)


def median(values):
    values = [value for value in values if value is not None]
    return statistics.median(values) if values else 0.0


def percentile(values, share):
    values = sorted(value for value in values if value is not None)
    if not values:
        return 0.0
    return values[min(int(share * len(values)), len(values) - 1)]
