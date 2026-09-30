"""Manifest events decoded field by field with the schema the guest's own TDH
gave (tools/guest/probes/etw_schema.c): names, types, counts and value maps.

    schema = Schema.load(path)
    fields = schema.decode(record)            # {"Reasons": 1, ...} or None
    schema.event_name(record)                 # "SCHEDULED_COMPOSITION_REASON"
    schema.map_text("CompositionReason", 1)   # "DelayedRendering"
"""
import json
import struct
import uuid

POINTER = 8   # every event this reads is a 64-bit one

# TDH_INTYPE: the size of each fixed-size type and how struct reads it.
FIXED = {
    3: "<b", 4: "<B", 5: "<h", 6: "<H", 7: "<i", 8: "<I", 9: "<q", 10: "<Q",
    11: "<f", 12: "<d", 13: "<I", 16: "<Q", 17: "<Q", 20: "<I", 21: "<Q",
}
UNICODE_STRING, ANSI_STRING, BINARY, GUID, SYSTEMTIME, SID = 1, 2, 14, 15, 18, 19
COUNTED_STRING, COUNTED_ANSI_STRING, COUNTED_BINARY = 22, 23, 25


class Truncated(Exception):
    pass


class Schema:
    def __init__(self, providers):
        # provider guid (lower case, no braces) -> {"name", "events": {(id, version): event}, "maps"}
        self.providers = providers

    @classmethod
    def load(cls, path):
        with open(path, encoding="utf-8") as source:
            document = json.load(source)
        return cls.from_document(document)

    @classmethod
    def from_document(cls, document):
        providers = {}
        for provider in document["providers"]:
            key = provider["guid"].strip("{}").lower()
            events = {(event["id"], event["version"]): event for event in provider["events"]}
            providers[key] = {"name": provider["name"], "events": events, "maps": provider.get("maps", {})}
        return cls(providers)

    def provider_guid(self, name):
        for key, provider in self.providers.items():
            if provider["name"].lower() == name.lower():
                return key
        return None

    def event(self, record):
        provider = self.providers.get(record.provider)
        if provider is None:
            return None
        return provider["events"].get((record.id, record.version))

    def event_name(self, record):
        event = self.event(record)
        if event is None:
            return f"{record.provider[:8]}/{record.id}"
        opcode = (event.get("opcode_name") or "").strip()
        name = (event.get("task_name") or "").strip() or str(record.id)
        return f"{name}/{opcode}" if opcode and opcode not in ("Info", "win:Info") else name

    def map_text(self, provider_guid, map_name, value):
        """A mapped value as the manifest spells it: a value map's one name,
        or a bitmap's set bits joined by '|'. The part after '::' only."""
        provider = self.providers.get(provider_guid)
        mapping = provider["maps"].get(map_name) if provider else None
        if mapping is None:
            return str(value)
        entries = {int(key): text.strip().split("::")[-1] for key, text in mapping["entries"].items()}
        if not mapping.get("bitmap"):
            return entries.get(value, str(value))
        if value == 0:
            return entries.get(0, "0")
        return "|".join(text for bit, text in sorted(entries.items()) if bit and value & bit == bit) or str(value)

    def decode(self, record):
        """{field name: value} of a manifest event, or None when the schema
        does not know the event. A payload shorter than its fields decodes as
        far as it goes."""
        event = self.event(record)
        if event is None:
            return None
        fields = event["fields"]
        reader = Reader(record.payload)
        values = {}
        order = []
        try:
            for index in range(event.get("top_level", len(fields))):
                name = fields[index]["name"]
                values[name] = self.read_field(fields, index, values, order, reader)
                order.append(name)
        except Truncated:
            pass
        return values

    def read_field(self, fields, index, values, order, reader):
        field = fields[index]
        count = self.count_of(field, values, order)
        if "struct" in field:
            start, members = field["struct"]
            items = []
            for _ in range(count if count is not None else 1):
                item = {}
                member_order = []
                for member in range(start, start + members):
                    item[fields[member]["name"]] = self.read_field(fields, member, item, member_order, reader)
                    member_order.append(fields[member]["name"])
                items.append(item)
            return items if count is not None and count != 1 else items[0]
        if count is not None and count != 1:
            return [self.read_value(field, values, order, reader) for _ in range(count)]
        return self.read_value(field, values, order, reader)

    def count_of(self, field, values, order):
        if "count_field" in field:
            return int(values.get(order[field["count_field"]], 0)) if field["count_field"] < len(order) else 0
        return field.get("count")

    def read_value(self, field, values, order, reader):
        in_type = field["in"]
        length = field.get("length", 0)
        if "length_field" in field and field["length_field"] < len(order):
            length = int(values.get(order[field["length_field"]], 0))
        if in_type in FIXED and not (in_type in (13, 20, 21) and length and length not in (4, 8)):
            code = FIXED[in_type]
            if in_type in (13, 20) and length == 8:
                code = "<Q"
            return reader.unpack(code)
        if in_type == UNICODE_STRING:
            return reader.utf16(length)
        if in_type == ANSI_STRING:
            return reader.ansi(length)
        if in_type == GUID:
            return str(uuid.UUID(bytes_le=reader.take(16)))
        if in_type == BINARY:
            return reader.take(length).hex()
        if in_type == SYSTEMTIME:
            return reader.take(16).hex()
        if in_type == SID:
            header = reader.peek(8)
            return reader.take(8 + 4 * header[1]).hex()
        if in_type in (COUNTED_STRING, COUNTED_ANSI_STRING, COUNTED_BINARY):
            size = reader.unpack("<H")
            raw = reader.take(size)
            return raw.decode("utf-16-le", "replace") if in_type == COUNTED_STRING else raw.hex()
        raise Truncated()


class Reader:
    def __init__(self, data):
        self.data = data
        self.offset = 0

    def take(self, size):
        if self.offset + size > len(self.data):
            raise Truncated()
        chunk = self.data[self.offset:self.offset + size]
        self.offset += size
        return bytes(chunk)

    def peek(self, size):
        if self.offset + size > len(self.data):
            raise Truncated()
        return self.data[self.offset:self.offset + size]

    def unpack(self, code):
        size = struct.calcsize(code)
        value, = struct.unpack(code, self.take(size))
        return value

    def utf16(self, length):
        if length:
            return self.take(2 * length).decode("utf-16-le", "replace").rstrip("\0")
        end = self.offset
        while end + 1 < len(self.data) and self.data[end:end + 2] != b"\0\0":
            end += 2
        text = bytes(self.data[self.offset:end]).decode("utf-16-le", "replace")
        self.offset = min(end + 2, len(self.data))
        return text

    def ansi(self, length):
        if length:
            return self.take(length).decode("latin-1").rstrip("\0")
        end = self.data.find(b"\0", self.offset)
        end = len(self.data) if end < 0 else end
        text = bytes(self.data[self.offset:end]).decode("latin-1")
        self.offset = min(end + 1, len(self.data))
        return text
