from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class CType:
    base: str
    pointer_depth: int = 0
    is_const: bool = False
    fixed_dim: int | None = None
    api_constant: str | None = None
    inner_const: bool = False

    @property
    def is_fixed_array(self) -> bool:
        return self.fixed_dim is not None

    def cpp(self) -> str:
        s = self.base
        if self.pointer_depth and self.inner_const and self.pointer_depth >= 2:
            s += "*const" + "*" * (self.pointer_depth - 1)
        elif self.pointer_depth:
            s += "*" * self.pointer_depth
        if self.is_const:
            s = "const " + s
        return s


@dataclass(frozen=True)
class Handle:
    name: str
    dispatchable: bool = True
    parent: str | None = None


@dataclass
class Param:
    name: str
    type: CType
    optional_levels: tuple[bool, ...] = ()
    len_expr: str | None = None
    null_refused: bool = False
    inner_len: str | None = None
    direction: str = "in"


@dataclass
class Command:
    name: str
    return_type: CType
    params: list[Param]
    cmd_id: int
    alias_of: str | None = None
    extension_only: bool = False
    sync_mode: str = "sync"
    success_codes: tuple[str, ...] = ()
    error_codes: tuple[str, ...] = ()


@dataclass
class StructMember:
    name: str
    type: CType
    optional_levels: tuple[bool, ...] = ()
    len_expr: str | None = None
    values: tuple[str, ...] = ()
    required: bool = False
    handle_required: bool = False
    conditional: bool = False


@dataclass
class Struct:
    name: str
    members: list[StructMember]
    returned_only: bool = False
    extends: tuple[str, ...] = ()
    is_union: bool = False

    @property
    def has_stype(self) -> bool:
        return bool(self.members) and self.members[0].name == "sType"


@dataclass
class Registry:
    handles: dict[str, Handle] = field(default_factory=dict)
    commands: dict[str, Command] = field(default_factory=dict)
    structs: dict[str, Struct] = field(default_factory=dict)
    struct_order: list[str] = field(default_factory=list)
    declared_names: set[str] = field(default_factory=set)
    implemented_extensions: dict[str, str] = field(default_factory=dict)
    manual_commands: set[str] = field(default_factory=set)
