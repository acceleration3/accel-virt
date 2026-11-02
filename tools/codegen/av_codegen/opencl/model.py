from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum

from ..ir import CType


class Role(str, Enum):
    SCALAR = "scalar"
    HANDLE = "handle"
    HANDLE_ARRAY = "handle_array"
    HANDLE_OUT = "handle_out"
    HANDLE_ARRAY_OUT = "handle_array_out"
    SCALAR_ARRAY = "scalar_array"
    SCALAR_OUT = "scalar_out"
    STRING = "string"
    STRING_ARRAY = "string_array"
    BYTES_IN = "bytes_in"
    BYTES_OUT = "bytes_out"
    INFO_VALUE = "info_value"
    STRUCT_IN = "struct_in"
    STRUCT_OUT = "struct_out"
    SVM_POINTER = "svm_pointer"
    PROPERTIES = "properties"
    PROPERTY_LIST = "property_list"
    IGNORED = "ignored"


@dataclass
class Param:
    name: str
    type: CType
    role: Role
    len_param: str | None = None
    lengths_param: str | None = None
    size_param: str | None = None
    size_ret_param: str | None = None
    optional: bool = True
    lent: bool = False
    element: str = ""

    @property
    def is_out(self) -> bool:
        return self.role in (Role.HANDLE_OUT, Role.HANDLE_ARRAY_OUT,
                             Role.SCALAR_OUT, Role.BYTES_OUT,
                             Role.INFO_VALUE, Role.STRUCT_OUT)


@dataclass
class Command:
    name: str
    return_type: CType
    params: list[Param] = field(default_factory=list)
    cmd_id: int = 0
    since: str = ""
    is_async: bool = False

    @property
    def returns_handle(self) -> bool:
        return self.return_type.pointer_depth == 0 and \
            self.return_type.base.startswith("cl_") and \
            self.return_type.base not in ("cl_int", "cl_uint", "cl_ulong",
                                          "cl_bool")

    @property
    def returns_address(self) -> bool:
        return self.return_type.base == "void" and self.return_type.pointer_depth == 1

    @property
    def returns_void(self) -> bool:
        return self.return_type.base == "void" and \
            self.return_type.pointer_depth == 0

    @property
    def out_params(self) -> list[Param]:
        return [p for p in self.params if p.is_out]


@dataclass
class Registry:
    commands: dict[str, Command] = field(default_factory=dict)
    handles: set[str] = field(default_factory=set)
    structs: dict[str, list[tuple[str, CType]]] = field(default_factory=dict)
    carried_extensions: list[str] = field(default_factory=list)
