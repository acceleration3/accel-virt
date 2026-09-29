"""What the registry does not say: which commands, in which tier, and
what a COMPSIZE stands for.

The tiers are the plan. OpenGL is three thousand commands and no
application uses more than a few hundred of them, so they are taken in
order: what a modern core-profile application calls first, then the
rest of 4.6, then the compatibility profile. A tier is a list of
feature names — the registry's own — and everything those features
require is in.
"""

from __future__ import annotations

import tomllib
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Profile:
    # Tier name -> the features whose commands it takes.
    tiers: dict[str, list[str]] = field(default_factory=dict)
    # Commands named one at a time, for the ones no feature list gets.
    extra: list[str] = field(default_factory=list)
    # Commands left out even though their feature asked for them.
    excluded: set[str] = field(default_factory=set)
    # Commands the guest and the server implement by hand.
    handwritten: set[str] = field(default_factory=set)
    answered_by_guest: set[str] = field(default_factory=set)
    # Commands the guest driver answers itself, which do not cross but
    # do exist. See the profile's own note.
    driver_implemented: set[str] = field(default_factory=set)
    # Extension -> the extension it is written on top of, where the
    # registry does not say. See [extension_requires] in the profile.
    extension_requires: dict[str, str] = field(default_factory=dict)
    # Commands that must not be sent one-way. See Registry.synchronous.
    synchronous: set[str] = field(default_factory=set)
    # Reads that may also be sent one-way into blob room the caller keeps.
    # See Registry.read_into_blob.
    read_into_blob: set[str] = field(default_factory=set)
    # Other spellings of a generated command, where the registry
    # declares no alias of its own.
    aliases: dict[str, str] = field(default_factory=dict)
    # "command.param" -> {role/len/...}, for what the rules cannot decide.
    params: dict[str, dict] = field(default_factory=dict)
    # Enum name -> how many values a query with it answers with.
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
            "# Stable command IDs for the OpenGL half of the RVK wire.",
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
