"""The records of an ETL file, with the processor each was logged on.

An ETL file is a run of per-processor buffers; each holds records of five
kinds told apart by a marker byte, 8-byte aligned. This reads them with plain
struct unpacking - several times faster than the etl-parser package - and
keeps what analysis needs and a generic parser drops: the buffer's processor
(the only place a context switch's processor is recorded) and a manifest
event's extended data items (its stack, when the session asked for one).

    for record in EtlFile(path).records():
        record.kind, record.cpu, record.time, record.group, record.type, ...

Times are the trace's clock as logged: QPC ticks for WPR's traces, whose
frequency the header record gives (EtlFile.frequency).
"""
import struct
import uuid
from dataclasses import dataclass, field

BUFFER_HEADER = 72
ALIGNMENT = 8
EXTENDED_INFO = 0x0001      # EVENT_HEADER_FLAG_EXTENDED_INFO
HEADER_GROUP = 0            # EVENT_TRACE_GROUP_HEADER
HEADER_INFO = 0             # its first type: the trace's header, with the clock
PERF_FREQUENCY = 256        # TRACE_LOGFILE_HEADER.PerfFreq in a 64-bit header

SYSTEM_FULL = (0x01, 0x02)
SYSTEM_COMPACT = (0x03, 0x04)
PERFINFO = (0x10, 0x11)
EVENT_HEADER = (0x12, 0x13)
TRACE_FULL = (0x0A, 0x14)

KINDS = {}
for marker in SYSTEM_FULL + SYSTEM_COMPACT:
    KINDS[marker] = "system"
for marker in PERFINFO:
    KINDS[marker] = "perfinfo"
for marker in EVENT_HEADER:
    KINDS[marker] = "event"
for marker in TRACE_FULL:
    KINDS[marker] = "trace"


@dataclass
class Record:
    """One record. `group` and `type` name a kernel (system or perfinfo)
    record; `provider`, `id`, `version`, `opcode` and `task` a manifest event;
    `provider` and `type` (its class's event type) a classic trace record."""
    kind: str
    cpu: int
    time: int
    pid: int = 0
    tid: int = 0
    group: int = 0
    type: int = 0
    provider: str = ""
    id: int = 0
    version: int = 0
    opcode: int = 0
    task: int = 0
    keyword: int = 0
    payload: bytes = b""
    extended: list = field(default_factory=list)   # (ext_type, bytes)


def guid_text(raw, offset):
    return str(uuid.UUID(bytes_le=bytes(raw[offset:offset + 16])))


class EtlFile:
    def __init__(self, path):
        self.data = memoryview(open(path, "rb").read())
        self.frequency = 0
        self.lost_events = 0
        self.lost_buffers = 0

    def buffers(self):
        """(processor, flags, payload view) of each buffer, in file order."""
        offset = 0
        data = self.data
        while offset + BUFFER_HEADER <= len(data):
            size, saved = struct.unpack_from("<II", data, offset)
            if size == 0 or offset + size > len(data):
                break
            processor, = struct.unpack_from("<H", data, offset + 40)
            flags, = struct.unpack_from("<H", data, offset + 52)
            end = offset + min(saved, size)
            yield processor, flags, data[offset + BUFFER_HEADER:end]
            offset += size

    def records(self):
        for cpu, flags, payload in self.buffers():
            if flags & 0x2:
                self.lost_events += 1
            if flags & 0x4:
                self.lost_buffers += 1
            yield from self.parse_buffer(cpu, payload)

    def parse_buffer(self, cpu, payload):
        offset = 0
        end = len(payload)
        while offset + 4 <= end:
            size, marker, flags = struct.unpack_from("<HBB", payload, offset)
            if flags & 0xC0 != 0xC0 or marker not in KINDS:
                break
            kind = KINDS[marker]
            if kind == "system" or kind == "perfinfo":
                # The packet's size is the record's; the marker's is a version.
                size, = struct.unpack_from("<H", payload, offset + 4)
            if size < 4 or offset + size > end:
                break
            record = self.parse_record(kind, marker, cpu, payload, offset, size)
            if record is not None:
                yield record
            offset += (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1)

    def parse_record(self, kind, marker, cpu, payload, offset, size):
        if kind == "system":
            _, hook_type, group, tid, pid, time = struct.unpack_from("<HBBIIQ", payload, offset + 4)
            header = 32 if marker in SYSTEM_FULL else 24
            record = Record("system", cpu, time, pid, tid, group, hook_type,
                            payload=bytes(payload[offset + header:offset + size]))
            if group == HEADER_GROUP and hook_type == HEADER_INFO:
                self.read_header(record.payload)
            return record
        if kind == "perfinfo":
            _, hook_type, group, time = struct.unpack_from("<HBBQ", payload, offset + 4)
            return Record("perfinfo", cpu, time, group=group, type=hook_type,
                          payload=bytes(payload[offset + 16:offset + size]))
        if kind == "event":
            (event_flags, _, tid, pid, time) = struct.unpack_from("<HHIIQ", payload, offset + 4)
            provider = guid_text(payload, offset + 24)
            (event_id, version, _, _, opcode, task, keyword) = struct.unpack_from("<HBBBBHQ", payload, offset + 40)
            body = offset + 80
            extended = []
            if event_flags & EXTENDED_INFO:
                while body + 8 <= offset + size:
                    _, ext_type, more, data_size = struct.unpack_from("<HHHH", payload, body)
                    extended.append((ext_type, bytes(payload[body + 8:body + 8 + data_size])))
                    body += (8 + data_size + ALIGNMENT - 1) & ~(ALIGNMENT - 1)
                    if not more & 0x1:
                        break
            return Record("event", cpu, time, pid, tid, provider=provider, id=event_id, version=version,
                          opcode=opcode, task=task, keyword=keyword,
                          payload=bytes(payload[body:offset + size]), extended=extended)
        if kind == "trace":
            (class_type, _, version, tid, pid, time) = struct.unpack_from("<BBHIIQ", payload, offset + 4)
            provider = guid_text(payload, offset + 24)
            return Record("trace", cpu, time, pid, tid, type=class_type, version=version, provider=provider,
                          payload=bytes(payload[offset + 48:offset + size]))
        return None

    def read_header(self, payload):
        """TRACE_LOGFILE_HEADER's PerfFreq, the clock's ticks a second: after
        two 64-bit name pointers at 56 and the 172-byte time zone at 72, then
        BootTime at 248."""
        if len(payload) >= PERF_FREQUENCY + 8:
            self.frequency, = struct.unpack_from("<Q", payload, PERF_FREQUENCY)
