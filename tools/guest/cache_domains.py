import os
import pathlib

CPUS = pathlib.Path("/sys/devices/system/cpu")
UNITS = {"K": 1 << 10, "M": 1 << 20, "G": 1 << 30}


class CacheDomains:
    def __init__(self, cpus=CPUS):
        found = {}
        for cache in cpus.glob("cpu[0-9]*/cache/index*"):
            try:
                if (cache / "level").read_text().strip() != "3":
                    continue
                members = cpu_list((cache / "shared_cpu_list").read_text().strip())
                found[members] = size_of((cache / "size").read_text().strip())
            except OSError:
                continue
        self.domains = sorted(found, key=lambda members: (-found[members], min(members)))

    def fitting(self, count):
        return next((set(members) for members in self.domains if len(members) >= count), None)

    def confine(self, threads):
        domain = self.fitting(len(threads))
        if domain is None:
            return None
        for thread in threads:
            os.sched_setaffinity(thread, domain)
        return domain


def cpu_list(text):
    members = set()
    for part in filter(None, text.split(",")):
        first, _, last = part.partition("-")
        members.update(range(int(first), int(last or first) + 1))
    return frozenset(members)


def size_of(text):
    unit = UNITS.get(text[-1:], 1)
    return int(text.rstrip("KMG")) * unit
