import bisect
import collections
import struct
import uuid
from dataclasses import dataclass, field

PROCESS, THREAD, IMAGE_GROUP, PERFINFO, STACKWALK = 3, 5, 20, 15, 24
PROCESS_EVENTS = (1, 2, 3, 4, 39)
THREAD_EVENTS = (1, 2, 3, 4)
IMAGE_EVENTS = (2, 3, 4, 10)
CSWITCH, READY_THREAD, THREAD_NAME = 36, 50, 72
SAMPLED_PROFILE, STACK = 46, 32
DPC_TYPES = {66: "threaded dpc", 68: "dpc", 69: "timer dpc"}
ISR_TYPES = {67: "isr", 50: "msi isr"}
IMAGE_ID = "b3e675d7-2554-4f18-830b-2762732560de"
IMAGE_NAME, DBGID_RSDS = 0, 36
KERNEL_SPACE = 1 << 63
SID_OFFSETS = (40, 36)
MAX_SUBAUTHORITIES = 15

WAIT_REASONS = [
    "Executive", "FreePage", "PageIn", "PoolAllocation", "DelayExecution", "Suspended", "UserRequest",
    "WrExecutive", "WrFreePage", "WrPageIn", "WrPoolAllocation", "WrDelayExecution", "WrSuspended",
    "WrUserRequest", "WrEventPair", "WrQueue", "WrLpcReceive", "WrLpcReply", "WrVirtualMemory", "WrPageOut",
    "WrRendezvous", "WrKeyedEvent", "WrTerminated", "WrProcessInSwap", "WrCpuRateControl",
    "WrCalloutStack", "WrKernel", "WrResource", "WrPushLock", "WrMutex", "WrQuantumEnd", "WrDispatchInt",
    "WrPreempted", "WrYieldExecution", "WrFastMutex", "WrGuardedMutex", "WrRundown", "WrAlertByThreadId",
    "WrDeferredPreempt", "WrPhysicalFault", "WrIoRing", "WrMdlCache",
]
THREAD_STATES = ["Initialized", "Ready", "Running", "Standby", "Terminated", "Waiting", "Transition",
                 "DeferredReady", "GateWaitObsolete", "WaitingForProcessInSwap"]


@dataclass
class Switch:
    time: int
    cpu: int
    new_tid: int
    old_tid: int
    old_state: str
    wait_reason: str
    new_priority: int


@dataclass
class Ready:
    time: int
    cpu: int
    tid: int
    readier: int


@dataclass
class Sample:
    time: int
    cpu: int
    tid: int
    address: int
    stack: tuple = ()


@dataclass
class Interrupt:
    start: int
    end: int
    cpu: int
    kind: str
    routine: int
    vector: int = -1


@dataclass
class Image:
    base: int
    size: int
    path: str


def utf16z(data, offset):
    end = offset
    while end + 1 < len(data) and data[end:end + 2] != b"\0\0":
        end += 2
    return data[offset:end].decode("utf-16-le", "replace"), end + 2


def ansiz(data, offset):
    end = data.find(b"\0", offset)
    end = len(data) if end < 0 else end
    return data[offset:end].decode("latin-1"), end + 1


class Kernel:
    def __init__(self):
        self.process_names = {}
        self.thread_process = {}
        self.thread_names = {}
        self.images = collections.defaultdict(dict)
        self.pdbs = {}
        self.switches = collections.defaultdict(list)
        self.readies = []
        self.samples = []
        self.interrupts = collections.defaultdict(list)
        self._stacks = {}
        self._idle_threads = set()

    def add(self, record):
        if record.kind in ("system", "perfinfo"):
            self.add_kernel(record)
        elif record.kind == "trace" and record.provider == IMAGE_ID:
            self.add_image_id(record)

    def add_kernel(self, record):
        group, kind, data = record.group, record.type, record.payload
        if group == THREAD and kind == CSWITCH and len(data) >= 24:
            new_tid, old_tid, new_priority, _, _, _, wait_reason, _, old_state = struct.unpack_from("<IIbbBbbbb", data)
            self.switches[record.cpu].append(Switch(
                record.time, record.cpu, new_tid, old_tid, name_of(THREAD_STATES, old_state),
                name_of(WAIT_REASONS, wait_reason), new_priority))
        elif group == THREAD and kind == READY_THREAD and len(data) >= 4:
            tid, = struct.unpack_from("<I", data)
            self.readies.append(Ready(record.time, record.cpu, tid, 0))
        elif group == THREAD and kind in THREAD_EVENTS and len(data) >= 8:
            pid, tid = struct.unpack_from("<II", data)
            self.thread_process[tid] = pid
            if pid == 0:
                self._idle_threads.add(tid)
        elif group == THREAD and kind == THREAD_NAME and len(data) >= 8:
            pid, tid = struct.unpack_from("<II", data)
            self.thread_names[tid] = utf16z(data, 8)[0]
        elif group == PROCESS and kind in PROCESS_EVENTS:
            self.add_process(data)
        elif (group == PROCESS and kind == 10) or (group == IMAGE_GROUP and kind in IMAGE_EVENTS):
            self.add_image(data)
        elif group == PERFINFO and kind == SAMPLED_PROFILE and len(data) >= 12:
            address, tid = struct.unpack_from("<QI", data)
            self.samples.append(Sample(record.time, record.cpu, tid, address))
        elif group == STACKWALK and kind == STACK and len(data) > 16:
            stamp, _, tid = struct.unpack_from("<QII", data)
            count = (len(data) - 16) // 8
            self._stacks.setdefault((stamp, tid), []).extend(struct.unpack_from(f"<{count}Q", data, 16))
        elif group == PERFINFO and kind in DPC_TYPES and len(data) >= 16:
            start, routine = struct.unpack_from("<QQ", data)
            self.interrupts[record.cpu].append(Interrupt(start, record.time, record.cpu, DPC_TYPES[kind], routine))
        elif group == PERFINFO and kind in ISR_TYPES and len(data) >= 19:
            start, routine, _, vector = struct.unpack_from("<QQBH", data)
            self.interrupts[record.cpu].append(Interrupt(start, record.time, record.cpu, ISR_TYPES[kind], routine,
                                                         vector))

    def add_process(self, data):
        if len(data) < 40:
            return
        pid, = struct.unpack_from("<I", data, 8)
        for token in SID_OFFSETS:
            offset = token + 16
            if offset + 8 <= len(data) and data[offset] == 1 and data[offset + 1] <= MAX_SUBAUTHORITIES:
                break
        else:
            return
        offset += 8 + 4 * data[offset + 1]
        name, _ = ansiz(data, offset)
        if name:
            self.process_names[pid] = name

    def add_image(self, data):
        if len(data) < 58:
            return
        base, size, pid = struct.unpack_from("<QQI", data)
        path, _ = utf16z(data, 56)
        if base and size:
            self.images[pid if base < KERNEL_SPACE else 0][base] = Image(base, size, path)

    def add_image_id(self, record):
        data = record.payload
        if record.type == DBGID_RSDS and len(data) >= 32:
            base, pid = struct.unpack_from("<QI", data)
            guid = uuid.UUID(bytes_le=bytes(data[12:28])).hex.upper()
            age, = struct.unpack_from("<I", data, 28)
            name, _ = ansiz(data, 32)
            self.pdbs[(pid if base < KERNEL_SPACE else 0, base)] = (name, guid, age)

    def finish(self):
        for sample in self.samples:
            sample.stack = tuple(self._stacks.get((sample.time, sample.tid), ()))
        self._stacks = {}
        self.samples.sort(key=lambda sample: sample.time)
        for switches in self.switches.values():
            switches.sort(key=lambda switch: switch.time)
        times = {cpu: [switch.time for switch in switches] for cpu, switches in self.switches.items()}
        for ready in self.readies:
            index = bisect.bisect_right(times.get(ready.cpu, []), ready.time) - 1
            ready.readier = self.switches[ready.cpu][index].new_tid if index >= 0 else 0
        self.readies.sort(key=lambda ready: ready.time)
        for interrupts in self.interrupts.values():
            interrupts.sort(key=lambda interrupt: interrupt.start)

    def process_name(self, pid):
        return self.process_names.get(pid, f"pid {pid}")

    def pids_named(self, name):
        wanted = name.lower()
        wanted = {wanted, wanted + ".exe"}
        return {pid for pid, image in self.process_names.items() if image.lower() in wanted}

    def threads_of(self, pids):
        return {tid for tid, pid in self.thread_process.items() if pid in pids}

    def idle(self, tid):
        return tid == 0 or tid in self._idle_threads

    def image_at(self, pid, address):
        table = self.images.get(0 if address >= KERNEL_SPACE else pid, {})
        bases = sorted(table)
        index = bisect.bisect_right(bases, address) - 1
        if index >= 0:
            image = table[bases[index]]
            if address < image.base + image.size:
                return image
        return None


def name_of(names, index):
    return names[index] if 0 <= index < len(names) else str(index)
