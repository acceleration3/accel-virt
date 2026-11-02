import pathlib
import struct
import sys
import tempfile
import unittest
import uuid

from tool_run import ROOT  # noqa: F401

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from etw.dwm import DwmFrames  # noqa: E402
from etw.etl_file import EtlFile  # noqa: E402
from etw.kernel import Kernel  # noqa: E402
from etw.manifest import Schema  # noqa: E402
from etw.symbols import PublicTable, short_name  # noqa: E402
from etw.threads import ThreadTimeline  # noqa: E402
from etw.trace import Event, Trace  # noqa: E402

DWM_GUID = "9e9bba3c-2e38-40cb-99f4-9e8281425164"
MS = 10_000


def aligned(record):
    return record + b"\0" * (-len(record) % 8)


def perfinfo(group, kind, time, payload):
    size = 16 + len(payload)
    return aligned(struct.pack("<HBBHBBQ", 2, 0x11, 0xC0, size, kind, group, time) + payload)


def event(provider, event_id, time, pid, tid, payload, extended=()):
    items = b""
    for index, (ext_type, data) in enumerate(extended):
        more = 1 if index + 1 < len(extended) else 0
        items += aligned(struct.pack("<HHHH", 0, ext_type, more, len(data)) + data)
    flags = 0x1 if extended else 0
    body = items + payload
    header = struct.pack("<HBBHHIIQ", 80 + len(body), 0x13, 0xC0, flags, 0, tid, pid, time)
    header += uuid.UUID(provider).bytes_le + struct.pack("<HBBBBHQ", event_id, 0, 0, 4, 0, 1, 0)
    header += b"\0" * (80 - len(header))
    return aligned(header + body)


def buffer(cpu, records):
    body = b"".join(records)
    size = 72 + len(body)
    header = struct.pack("<II", size, size) + b"\0" * 32 + struct.pack("<H", cpu) + b"\0" * 30
    return header + body


class Records(unittest.TestCase):
    def read(self, data):
        with tempfile.NamedTemporaryFile(suffix=".etl", delete=False) as file:
            file.write(data)
        self.addCleanup(pathlib.Path(file.name).unlink)
        return list(EtlFile(file.name).records())

    def test_each_record_carries_the_processor_of_its_buffer(self):
        switch = struct.pack("<IIbbBbbbbbII", 20, 10, 8, 8, 0, 0, 13, 0, 5, 0, 0, 0)
        records = self.read(buffer(3, [perfinfo(5, 36, 1000, switch)]) + buffer(7, [perfinfo(5, 36, 2000, switch)]))
        self.assertEqual([(record.cpu, record.time, record.group, record.type) for record in records],
                         [(3, 1000, 5, 36), (7, 2000, 5, 36)])
        self.assertEqual(records[0].payload, switch)

    def test_a_manifest_event_keeps_its_payload_apart_from_its_extended_data(self):
        stack = struct.pack("<QQQ", 7, 0x1000, 0x2000)
        records = self.read(buffer(0, [event(DWM_GUID, 385, 500, 44, 55, struct.pack("<I", 9), [(5, stack)])]))
        record = records[0]
        self.assertEqual((record.kind, record.provider, record.id, record.pid, record.tid), ("event", DWM_GUID, 385, 44, 55))
        self.assertEqual(record.payload, struct.pack("<I", 9))
        self.assertEqual(record.extended, [(5, stack)])

    def test_a_record_that_runs_past_its_buffer_ends_the_buffer(self):
        truncated = perfinfo(5, 36, 1000, b"\0" * 24)[:-8]
        good = perfinfo(5, 50, 900, struct.pack("<I", 1) + b"\0" * 4)
        self.assertEqual([record.type for record in self.read(buffer(0, [good, truncated]))], [50])


SCHEMA = {"providers": [{
    "name": "Microsoft-Windows-Dwm-Core", "guid": "{" + DWM_GUID.upper() + "}",
    "events": [
        {"id": 385, "version": 0, "opcode": 0, "task": 283, "level": 4, "keywords": "0x80",
         "task_name": "SCHEDULED_COMPOSITION_REASON", "opcode_name": "Info ", "top_level": 1,
         "fields": [{"name": "Reasons", "in": 8, "out": 8, "map": "CompositionReason", "length": 4}]},
        {"id": 900, "version": 0, "opcode": 1, "task": 7, "level": 4, "keywords": "0x1",
         "task_name": "SHAPES", "opcode_name": "Start ", "top_level": 4,
         "fields": [{"name": "Name", "in": 1, "out": 1},
                    {"name": "Count", "in": 6, "out": 6},
                    {"name": "Values", "in": 8, "out": 8, "count_field": 1},
                    {"name": "Rect", "struct": [4, 2], "count": 1},
                    {"name": "Left", "in": 7, "out": 7}, {"name": "Top", "in": 7, "out": 7}]}],
    "maps": {"CompositionReason": {"bitmap": True, "entries": {
        "0": "CompositionReason::None ", "1": "CompositionReason::DelayedRendering ",
        "128": "CompositionReason::TargetStats "}}}}]}


class Manifest(unittest.TestCase):
    def setUp(self):
        self.schema = Schema.from_document(SCHEMA)

    def record(self, event_id, payload):
        return type("Record", (), {"provider": DWM_GUID, "id": event_id, "version": 0, "payload": payload})()

    def test_a_bitmap_field_names_each_bit_that_is_set(self):
        record = self.record(385, struct.pack("<I", 129))
        self.assertEqual(self.schema.decode(record), {"Reasons": 129})
        self.assertEqual(self.schema.map_text(DWM_GUID, "CompositionReason", 129), "DelayedRendering|TargetStats")
        self.assertEqual(self.schema.map_text(DWM_GUID, "CompositionReason", 0), "None")
        self.assertEqual(self.schema.event_name(record), "SCHEDULED_COMPOSITION_REASON")

    def test_strings_counted_arrays_and_structs_decode_in_order(self):
        payload = "ab".encode("utf-16-le") + b"\0\0" + struct.pack("<H", 2) + struct.pack("<II", 5, 6)
        payload += struct.pack("<ii", -3, 4)
        fields = self.schema.decode(self.record(900, payload))
        self.assertEqual(fields, {"Name": "ab", "Count": 2, "Values": [5, 6], "Rect": {"Left": -3, "Top": 4}})
        self.assertEqual(self.schema.event_name(self.record(900, payload)), "SHAPES/Start")

    def test_a_short_payload_decodes_as_far_as_it_goes_and_an_unknown_event_not_at_all(self):
        fields = self.schema.decode(self.record(900, "a".encode("utf-16-le") + b"\0\0" + struct.pack("<H", 3)))
        self.assertEqual(fields, {"Name": "a", "Count": 3})
        self.assertIsNone(self.schema.decode(self.record(901, b"")))


def kernel_record(kind, group, event_type, time, payload, cpu=0, provider="", pid=0):
    return type("Record", (), {"kind": kind, "group": group, "type": event_type, "time": time, "payload": payload,
                               "cpu": cpu, "provider": provider, "pid": pid})()


def switch(time, cpu, new, old, state, reason=6):
    states = {"Waiting": 5, "Ready": 1}
    return kernel_record("perfinfo", 5, 36, time,
                         struct.pack("<IIbbBbbbbbII", new, old, 8, 8, 0, 0, reason, 0, states[state], 0, 0, 0), cpu)


def ready(time, cpu, tid):
    return kernel_record("perfinfo", 5, 50, time, struct.pack("<I", tid) + b"\0" * 4, cpu)


def dpc(start, end, cpu, routine=0xFFFFF800_00001000):
    return kernel_record("perfinfo", 15, 68, end, struct.pack("<QQ", start, routine), cpu)


class KernelModel(unittest.TestCase):
    def test_switches_readies_samples_and_interrupts_are_kept_per_processor_in_time_order(self):
        kernel = Kernel()
        kernel.add(switch(20, 1, 7, 0, "Waiting"))
        kernel.add(switch(10, 1, 0, 9, "Waiting"))
        kernel.add(ready(30, 1, 9))
        kernel.add(kernel_record("perfinfo", 15, 46, 40, struct.pack("<QI", 0x7000, 7) + b"\0" * 4, 1))
        kernel.add(kernel_record("perfinfo", 24, 32, 41, struct.pack("<QIIQQ", 40, 1, 7, 0xFFFF800000001000, 0x100)))
        kernel.add(kernel_record("perfinfo", 24, 32, 41, struct.pack("<QIIQ", 40, 1, 7, 0x7000)))
        kernel.add(dpc(50, 60, 1))
        kernel.finish()
        self.assertEqual([item.time for item in kernel.switches[1]], [10, 20])
        self.assertEqual((kernel.readies[0].tid, kernel.readies[0].readier), (9, 7))
        self.assertEqual(kernel.samples[0].stack, (0xFFFF800000001000, 0x100, 0x7000))
        self.assertEqual((kernel.interrupts[1][0].start, kernel.interrupts[1][0].end), (50, 60))

    def test_processes_images_and_their_pdbs_name_addresses(self):
        kernel = Kernel()
        sid = struct.pack("<QQ", 0xFFFF8000_12345678, 0) + bytes([1, 1, 0, 0, 0, 0, 0, 5]) + struct.pack("<I", 18)
        process = struct.pack("<QIIIiQI", 0, 44, 4, 1, 0, 0, 0) + b"\0" * 4 + sid + b"dwm.exe\0"
        kernel.add(kernel_record("system", 3, 1, 1, process))
        image = struct.pack("<QQIIIBBHQ", 0x7FF600000000, 0x10000, 44, 0, 0, 0, 0, 0, 0) + b"\0" * 16
        image += "C:\\dwmcore.dll".encode("utf-16-le") + b"\0\0"
        kernel.add(kernel_record("system", 3, 10, 2, image))
        guid = uuid.UUID("91c1b656-d422-08bb-f1cb-22a4d5a75db0")
        rsds = struct.pack("<QI", 0x7FF600000000, 44) + guid.bytes_le + struct.pack("<I", 1) + b"dwmcore.pdb\0"
        kernel.add(kernel_record("trace", 0, 36, 3, rsds, provider="b3e675d7-2554-4f18-830b-2762732560de"))
        self.assertEqual(kernel.process_name(44), "dwm.exe")
        self.assertEqual(kernel.pids_named("DWM"), {44})
        self.assertEqual(kernel.image_at(44, 0x7FF600000100).path, "C:\\dwmcore.dll")
        self.assertIsNone(kernel.image_at(44, 0x7FF600010000))
        self.assertEqual(kernel.pdbs[(44, 0x7FF600000000)], ("dwmcore.pdb", "91C1B656D42208BBF1CB22A4D5A75DB0", 1))


class Timeline(unittest.TestCase):
    def test_a_thread_s_time_splits_into_running_interrupted_ready_and_blocked(self):
        kernel = Kernel()
        for record in [switch(0, 2, 5, 0, "Waiting"),
                       dpc(2 * MS, 3 * MS, 2),
                       switch(4 * MS, 2, 0, 5, "Waiting", reason=6),
                       ready(6 * MS, 3, 5),
                       switch(7 * MS, 2, 5, 0, "Waiting"),
                       switch(8 * MS, 2, 0, 5, "Ready")]:
            kernel.add(record)
        kernel.finish()
        account = ThreadTimeline(kernel, 5, lambda ticks: ticks / MS).account(0, 10 * MS)
        self.assertAlmostEqual(account.running, 5.0)
        self.assertAlmostEqual(account.interrupted, 1.0)
        self.assertAlmostEqual(account.ready, 1.0 + 2.0)
        self.assertEqual(dict(account.blocked), {"UserRequest": 2.0})
        self.assertAlmostEqual(account.unknown, 0.0)


def frames_trace(check_offsets, delayed=(), vblank_ms=16.667):
    events = []
    vblanks = [int(index * vblank_ms * MS) for index in range(len(check_offsets) + 1)]
    for index, vblank in enumerate(vblanks):
        events.append(Event(vblank, 0, 0, 0, "Microsoft-Windows-DxgKrnl", "VSyncDPC",
                            {"pDxgAdapter": 1, "VidPnSourceId": 0, "FrameQPCTime": vblank}))
    for index, offset in enumerate(check_offsets):
        start = vblanks[index] + 100
        check = vblanks[index] + int(offset * MS)
        dwm = [("SCHEDULE_PROCESS_FRAME/Start", start, {}),
               ("LEGACYSWAPCHAIN_BUFFERFLIP", check, {"NewBackBufferIndex": 0}),
               ("SCHEDULE_PROCESS_FRAME/Stop", check + 3 * MS, {})]
        if index in delayed:
            dwm.insert(2, ("SCHEDULED_COMPOSITION_REASON", check + 10, {"Reasons": 1}))
        for name, time, fields in dwm:
            events.append(Event(time, 0, 44, 55, "Microsoft-Windows-Dwm-Core", name, fields))
        if index not in delayed:
            events.append(Event(check + MS, 0, 44, 55, "Microsoft-Windows-Win32k", "FlipManagerPresentPosted",
                                {"FlipManagerId": 3}))
    events.sort(key=lambda item: item.time)
    return Trace(Kernel(), events, 10_000_000, None)


class Frames(unittest.TestCase):
    def test_a_check_earlier_after_its_vblank_than_the_last_captured_one_is_predicted_to_skip(self):
        trace = frames_trace([0.5, 0.6, 2.0, 1.0, 1.0, 1.1], delayed={3})
        frames = DwmFrames(trace)
        self.assertEqual((frames.pid, frames.render_tid, frames.capture_manager), (44, 55, 3))
        self.assertEqual([frame.captured for frame in frames.frames], [True, True, True, False, True, True])
        self.assertEqual([frame.predicted_skip for frame in frames.frames], [False, False, False, True, False, False])
        self.assertAlmostEqual(frames.frames[3].elapsed_ms, 16.667 - 1.0, places=2)
        self.assertAlmostEqual(frames.frames[4].elapsed_ms, 2 * 16.667 - 1.0, places=2)
        self.assertEqual(frames.rule_agreement(), (1, 0, 0, 4))

    def test_a_skip_the_rule_does_not_predict_and_a_prediction_that_captured_are_both_counted(self):
        trace = frames_trace([0.5, 0.5, 0.5, 2.0, 1.0], delayed={1})
        frames = DwmFrames(trace)
        agree, unexplained, false_alarms, _ = frames.rule_agreement()
        self.assertEqual((agree, unexplained, false_alarms), (0, 1, 1))

    def test_groups_pair_each_skip_with_the_captured_frame_it_was_measured_from(self):
        trace = frames_trace([0.5, 2.0, 1.0, 0.5, 2.5, 1.0, 0.6], delayed={2, 5})
        frames = DwmFrames(trace)
        frames.analyze()
        before, skipped, normal = frames.groups()
        self.assertEqual([frame.index for frame in before], [1, 4])
        self.assertEqual([frame.index for frame in skipped], [2, 5])
        self.assertEqual([frame.index for frame in normal], [0, 3, 6])
        self.assertAlmostEqual(frames.frames[1].phase_ms(trace.ms), 2.0)


class Names(unittest.TestCase):
    def test_a_demangled_signature_is_cut_to_its_qualified_name(self):
        self.assertEqual(short_name("protected: virtual bool __cdecl COffScreenRenderTarget::IsRenderTime(void)"),
                         "COffScreenRenderTarget::IsRenderTime")
        self.assertEqual(short_name("public: long __cdecl A<struct B(int)>::C(int)"), "A<struct B(int)>::C")
        self.assertEqual(short_name("memmove"), "memmove")

    def test_an_address_takes_the_public_at_or_below_it(self):
        table = PublicTable([(0x2000, "second"), (0x1000, "first")])
        self.assertEqual([table.name(rva) for rva in (0x0FFF, 0x1000, 0x1FFF, 0x2000, 0x9000)],
                         [None, "first", "first", "second", "second"])


if __name__ == "__main__":
    unittest.main()
