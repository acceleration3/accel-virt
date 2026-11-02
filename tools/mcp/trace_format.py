import re
import struct

MAGIC = b"AVTRACE\0"
VERSION = 1
HEADER_BYTES = 96
SLOT_HEADER_BYTES = 56
FRAME_BYTES = 24
FRAMES_PER_SLOT = 32
SLOT_BYTES = SLOT_HEADER_BYTES + FRAMES_PER_SLOT * FRAME_BYTES
MODULE_BYTES = 16
MAX_SLOTS = 256
ACTIVE = 1 << 63
TICKS_PER_MS = 10_000
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619

SNAPSHOT_MAGIC = b"AVSNAP1\0"
SNAPSHOT_HEADER = struct.Struct("<8sQII")
SNAPSHOT_TABLE = 1
SNAPSHOT_STATE = 2
SNAPSHOT_ERROR = 3
SNAPSHOT_KINDS = {SNAPSHOT_TABLE: "table", SNAPSHOT_STATE: "state", SNAPSHOT_ERROR: "error"}

HEADER = struct.Struct("<8sIIIIII16sIIQQIIQQ")
SLOT = struct.Struct("<QIIQIIQQQ")
FRAME = struct.Struct("<IHHQQ")
STATE_FIELD = re.compile(r"(\S+?)=(\S*)")


def fnv1a(name):
    value = FNV_OFFSET
    for byte in name.encode():
        value = ((value ^ byte) * FNV_PRIME) & 0xFFFFFFFF
    return value


def parse_table(data):
    data = bytes(data)
    if len(data) < HEADER_BYTES:
        raise ValueError(f"header: {len(data)} bytes, a table needs {HEADER_BYTES}")
    (magic, version, header_bytes, slots, slot_bytes, frames, frame_bytes, module, process, crash_thread,
     created, ticks, crash_code, _reserved, crash_address, crashed_at) = HEADER.unpack_from(data)
    checks = (("magic", magic == MAGIC, magic),
              ("version", version == VERSION, version),
              ("header bytes", header_bytes == HEADER_BYTES, header_bytes),
              ("slot bytes", slot_bytes == SLOT_BYTES, slot_bytes),
              ("frames per slot", frames == FRAMES_PER_SLOT, frames),
              ("frame bytes", frame_bytes == FRAME_BYTES, frame_bytes),
              ("slot count", slots <= MAX_SLOTS, slots))
    for field, holds, value in checks:
        if not holds:
            raise ValueError(f"{field}: {value!r} is not what a version {VERSION} table holds")
    if HEADER_BYTES + slots * SLOT_BYTES > len(data):
        raise ValueError(f"slot count: {slots} slots do not fit in {len(data)} bytes")
    if b"\0" not in module:
        raise ValueError("module: the name is not terminated inside its 16 bytes")
    table = {"module": module.split(b"\0", 1)[0].decode(errors="replace"), "process": process,
             "created": created, "ticks_per_second": ticks, "slot_count": slots, "slots": []}
    if crashed_at:
        table["crash"] = {"code": crash_code, "address": crash_address, "thread": crash_thread, "at": crashed_at}
    for index in range(slots):
        start = HEADER_BYTES + index * SLOT_BYTES
        owner, depth, thread, entries, last_left, _reserved, left_at, waiting, holding = SLOT.unpack_from(data, start)
        if owner == 0:
            continue
        frames_read = []
        for level in range(min(depth, FRAMES_PER_SLOT)):
            name, irql, _pad, entered, text = FRAME.unpack_from(data, start + SLOT_HEADER_BYTES + level * FRAME_BYTES)
            frames_read.append({"name_id": name, "level": irql, "entered": entered, "text": text})
        table["slots"].append({"slot": index, "key": owner & ~ACTIVE, "active": bool(owner & ACTIVE),
                               "depth": depth, "thread": thread, "entries": entries, "last_left_id": last_left,
                               "left_at": left_at, "waiting_on": waiting, "holding": holding,
                               "frames": frames_read})
    return table


def parse_snapshot(data):
    data = bytes(data)
    if len(data) < SNAPSHOT_HEADER.size:
        raise ValueError(f"snapshot: {len(data)} bytes, shorter than its header")
    magic, clock, kind, length = SNAPSHOT_HEADER.unpack_from(data)
    if magic != SNAPSHOT_MAGIC:
        raise ValueError(f"snapshot magic: {magic!r}")
    if kind not in SNAPSHOT_KINDS:
        raise ValueError(f"snapshot kind: {kind}")
    if SNAPSHOT_HEADER.size + length > len(data):
        raise ValueError(f"snapshot length: {length} bytes promised, {len(data) - SNAPSHOT_HEADER.size} present")
    return SNAPSHOT_KINDS[kind], clock, data[SNAPSHOT_HEADER.size:SNAPSHOT_HEADER.size + length]


def parse_state(text):
    entries = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        path, _, rest = line.partition(" ")
        segments = path.split("/")
        kinds = segments[0::2]
        entries.append({"path": path, "kinds": kinds, "fields": dict(STATE_FIELD.findall(rest))})
    return entries


def describe_threads(table, names, now=None, stall_seconds=5.0):
    if now is None:
        stamps = [frame["entered"] for slot in table["slots"] for frame in slot["frames"]]
        stamps += [slot["left_at"] for slot in table["slots"]]
        now = max(stamps, default=0)
    holders = {}
    for slot in table["slots"]:
        if slot["active"] and slot["holding"]:
            holders.setdefault(slot["holding"], []).append(slot["thread"])
    threads = []
    for slot in table["slots"]:
        frames = [{"name": names.get(frame["name_id"], f"#{frame['name_id']:08x}"), "level": frame["level"],
                   "age_ms": age_ms(now, frame["entered"])} for frame in slot["frames"]]
        described = {"thread": slot["thread"], "active": slot["active"], "depth": slot["depth"],
                     "unstored_frames": max(0, slot["depth"] - len(frames)), "entries": slot["entries"],
                     "frames": frames}
        if slot["last_left_id"]:
            described["last_left"] = names.get(slot["last_left_id"], f"#{slot['last_left_id']:08x}")
            described["left_ms_ago"] = age_ms(now, slot["left_at"])
        if slot["active"] and slot["waiting_on"]:
            described["waiting_on"] = f"{slot['waiting_on']:#x}"
            described["lock_held_by"] = [thread for thread in holders.get(slot["waiting_on"], [])
                                         if thread != slot["thread"]]
        if slot["active"] and slot["holding"]:
            described["holding"] = f"{slot['holding']:#x}"
        old = bool(slot["active"] and frames and frames[-1]["age_ms"] is not None
                   and frames[-1]["age_ms"] >= stall_seconds * 1000)
        parked = bool(slot["active"] and slot["waiting_on"] and not described["lock_held_by"])
        described["stalled"] = old and not parked
        if not slot["active"]:
            described["state"] = "idle"
        elif slot["waiting_on"]:
            described["state"] = "blocked" if described["lock_held_by"] else "waiting"
        else:
            described["state"] = "stalled" if old else "running"
        threads.append(described)
    result = {"module": table["module"], "process": table["process"], "threads": threads}
    if "crash" in table:
        crash = table["crash"]
        result["crash"] = {"code": f"{crash['code']:#x}", "address": f"{crash['address']:#x}",
                           "thread": crash["thread"], "ms_ago": age_ms(now, crash["at"])}
    return result


def age_ms(now, stamp):
    if not stamp or stamp > now:
        return None
    return (now - stamp) // TICKS_PER_MS
