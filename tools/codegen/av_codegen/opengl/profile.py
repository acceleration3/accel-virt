from __future__ import annotations

import tomllib
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Profile:
    tiers: dict[str, list[str]] = field(default_factory=dict)
    extra: list[str] = field(default_factory=list)
    excluded: set[str] = field(default_factory=set)
    handwritten: set[str] = field(default_factory=set)
    answered_by_guest: set[str] = field(default_factory=set)
    driver_implemented: set[str] = field(default_factory=set)
    extension_requires: dict[str, str] = field(default_factory=dict)
    synchronous: set[str] = field(default_factory=set)
    read_into_blob: set[str] = field(default_factory=set)
    aliases: dict[str, str] = field(default_factory=dict)
    params: dict[str, dict] = field(default_factory=dict)
    pname_counts: dict[str, int] = field(default_factory=dict)
    ids: dict[str, int] = field(default_factory=dict)
    ids_path: Path | None = None
    first_id: int = 0x00030000

    def param_override(self, command: str, param: str) -> dict:
        return self.params.get(f"{command}.{param}", {})

    def command_id(self, command: str) -> int:
        if command in self.ids:
            return self.ids[command]
        nxt = max(self.ids.values()) + 1 if self.ids else self.first_id
        self.ids[command] = nxt
        return nxt

    def write_ids(self) -> None:
        if self.ids_path is None:
            return
        lines = [
            "# Stable command IDs for the OpenGL half of the AV wire.",
            "# Auto-maintained by codegen — do not hand-edit. New commands",
            "# get appended at the next free ID; existing IDs never move.",
            "",
            "[commands]",
        ]
        for name, cid in sorted(self.ids.items(), key=lambda kv: kv[1]):
            lines.append(f'"{name}" = {cid}')
        self.ids_path.write_text("\n".join(lines) + "\n")


def load(profile_path: Path, ids_path: Path) -> Profile:
    data = tomllib.loads(profile_path.read_text())
    ids: dict[str, int] = {}
    if ids_path.is_file():
        ids = dict(tomllib.loads(ids_path.read_text()).get("commands", {}))
    return Profile(
        tiers={k: list(v) for k, v in data.get("tiers", {}).items()},
        extra=list(data.get("extra", [])),
        excluded=set(data.get("excluded", [])),
        handwritten=set(data.get("handwritten", [])),
        answered_by_guest=set(data.get("answered_by_guest", [])),
        driver_implemented=set(data.get("driver_implemented", [])),
        extension_requires=dict(data.get("extension_requires", {})),
        synchronous=set(data.get("synchronous", [])),
        read_into_blob=set(data.get("read_into_blob", [])),
        aliases={str(k): str(v) for k, v in data.get("aliases", {}).items()},
        params=dict(data.get("params", {})),
        pname_counts={k: int(v) for k, v in data.get("pname_counts", {}).items()},
        ids=ids,
        ids_path=ids_path,
    )
