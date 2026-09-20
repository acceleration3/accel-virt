"""What the registry does not say: which commands, and what counts what."""

from __future__ import annotations

import tomllib
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Profile:
    commands: list[str] = field(default_factory=list)
    # Commands the driver answers itself and never sends. They are not on
    # the wire and get no id, but an application does get a working entry
    # point for each, so they count towards an extension being carried.
    driver_implemented: set[str] = field(default_factory=set)
    async_commands: set[str] = field(default_factory=set)
    params: dict[str, dict] = field(default_factory=dict)
    ids: dict[str, int] = field(default_factory=dict)
    ids_path: Path | None = None
    first_id: int = 0x00020000

    def param_override(self, command: str, param: str) -> dict:
        return self.params.get(f"{command}.{param}", {})

    def command_id(self, command: str) -> int:
        if command in self.ids:
            return self.ids[command]
        # Appended at the next free id; ids never move once assigned.
        nxt = max(self.ids.values()) + 1 if self.ids else self.first_id
        self.ids[command] = nxt
        return nxt

    def write_ids(self) -> None:
        if self.ids_path is None:
            return
        lines = [
            "# Stable command IDs for the OpenCL half of the RVK wire.",
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
        commands=list(data.get("commands", [])),
        driver_implemented=set(data.get("driver_implemented", [])),
        async_commands=set(data.get("async", [])),
        params=dict(data.get("params", {})),
        ids=ids,
        ids_path=ids_path,
    )
