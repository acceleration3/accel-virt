"""One recorded trace, read once: the kernel's model of it and every event of
the manifest providers the schema describes, decoded and in time order.

    trace = Trace.read(etl_path, schema_path)
    trace.kernel                      # tools/etw/kernel.Kernel
    trace.events                      # [Event], by time
    trace.named("Microsoft-Windows-Dwm-Core", "SCHEDULE_PROCESS_FRAME/Start")
    trace.ms(ticks)                   # ticks of the trace's clock in milliseconds
"""
import bisect
from dataclasses import dataclass, field

from etw.etl_file import EtlFile
from etw.kernel import Kernel
from etw.manifest import Schema


@dataclass
class Event:
    time: int
    cpu: int
    pid: int
    tid: int
    provider: str
    name: str
    fields: dict = field(default_factory=dict)


class Trace:
    def __init__(self, kernel, events, frequency, schema, lost=0):
        self.kernel = kernel
        self.events = events
        self.frequency = frequency or 10_000_000
        self.schema = schema
        self.lost = lost
        self.times = [event.time for event in events]
        self.by_name = {}
        for event in events:
            self.by_name.setdefault((event.provider, event.name), []).append(event)

    @classmethod
    def read(cls, etl_path, schema_path):
        schema = Schema.load(schema_path)
        kernel = Kernel()
        events = []
        etl = EtlFile(etl_path)
        for record in etl.records():
            if record.kind != "event":
                kernel.add(record)
                continue
            provider = schema.providers.get(record.provider)
            if provider is None:
                continue
            fields = schema.decode(record)
            if fields is None:
                continue
            fields = mapped(schema, record, fields)
            events.append(Event(record.time, record.cpu, record.pid, record.tid, provider["name"],
                                schema.event_name(record), fields))
        kernel.finish()
        events.sort(key=lambda event: event.time)
        return cls(kernel, events, etl.frequency, schema, etl.lost_events + etl.lost_buffers)

    def ms(self, ticks):
        return 1000.0 * ticks / self.frequency

    def ticks(self, ms):
        return int(ms * self.frequency / 1000.0)

    def named(self, provider, name):
        return self.by_name.get((provider, name), [])

    def between(self, start, end):
        return self.events[bisect.bisect_left(self.times, start):bisect.bisect_left(self.times, end)]


def mapped(schema, record, fields):
    """Fields with a value map carry the map's text beside the number."""
    event = schema.event(record)
    for index in range(event.get("top_level", len(event["fields"]))):
        spec = event["fields"][index]
        name = spec["name"]
        if "map" in spec and isinstance(fields.get(name), int):
            fields[name + "_text"] = schema.map_text(record.provider, spec["map"], fields[name])
    return fields
