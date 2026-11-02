from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum

from ..ir import CType


class Role(str, Enum):
    SCALAR = "scalar"
    ARRAY_IN = "array_in"
    ARRAY_OUT = "array_out"
    STRING_IN = "string_in"
    STRING_ARRAY_IN = "string_array_in"
    STRING_OUT = "string_out"
    PIXELS_IN = "pixels_in"
    PIXELS_OUT = "pixels_out"
    BUFFER_OFFSET = "buffer_offset"
    OFFSET_ARRAY = "offset_array"
    SYNC = "sync"
    IGNORED = "ignored"


class LengthKind(str, Enum):
    FIXED = "fixed"
    PARAM = "param"
    EXPR = "expr"
    STRING = "string"
    PNAME = "pname"
    PIXELS = "pixels"
    TYPED_COUNT = "typed"


@dataclass
class Length:
    kind: LengthKind
    text: str = ""
    count: int = 0
    reads: tuple[str, ...] = ()

    def is_known_at_compile_time(self) -> bool:
        return self.kind is LengthKind.FIXED


@dataclass
class Param:
    name: str
    type: CType
    role: Role
    length: Length | None = None
    element: str = ""
    size_param: str | None = None
    length_param: str | None = None
    has_lengths: bool = False

    @property
    def is_out(self) -> bool:
        return self.role in (Role.ARRAY_OUT, Role.STRING_OUT, Role.PIXELS_OUT)


@dataclass
class Command:
    name: str
    return_type: CType
    params: list[Param] = field(default_factory=list)
    command_id: int = 0
    since: str = ""
    api: str = "gl"
    tier: str = ""
    force_sync: bool = False

    @property
    def returns_void(self) -> bool:
        return self.return_type.base == "void" and self.return_type.pointer_depth == 0

    @property
    def has_out(self) -> bool:
        return any(p.is_out for p in self.params)

    @property
    def mints_handle(self) -> bool:
        return (self.return_type.base == "GLsync"
                and self.return_type.pointer_depth == 0)

    @property
    def is_async(self) -> bool:
        if self.force_sync:
            return False
        if self.mints_handle:
            return not self.has_out
        return self.returns_void and not self.has_out


@dataclass
class Registry:
    commands: dict[str, Command] = field(default_factory=dict)
    types: dict[str, str] = field(default_factory=dict)
    pname_counts: dict[str, int] = field(default_factory=dict)
    handwritten: set[str] = field(default_factory=set)
    answered_by_guest: set[str] = field(default_factory=set)
    driver_entries: dict[str, tuple[str, str]] = field(default_factory=dict)
    aliases: dict[str, str] = field(default_factory=dict)
    synchronous: set[str] = field(default_factory=set)
    read_into_blob: set[str] = field(default_factory=set)
    tier_major: int = 1
    tier_minor: int = 0
    carried_extensions: list[str] = field(default_factory=list)
