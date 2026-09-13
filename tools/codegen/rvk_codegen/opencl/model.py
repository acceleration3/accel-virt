"""What a lowered OpenCL command looks like.

The roles are the whole design. cl.xml says ``const cl_event*`` and
stops; the wire needs to know that the pointer is an array, that
``num_events_in_wait_list`` counts it, that it may be null, and that
each element is a handle rather than eight bytes of data. A role
carries exactly that, and every emitter is a switch over these.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum

from ..ir import CType


class Role(str, Enum):
    # Sent as written.
    SCALAR = "scalar"
    # An object of the API, as the server's own handle.
    HANDLE = "handle"
    HANDLE_ARRAY = "handle_array"
    # Out: one handle the call produces (the return value uses this too).
    HANDLE_OUT = "handle_out"
    # Out: an array of handles, with a capacity in and a count out.
    HANDLE_ARRAY_OUT = "handle_array_out"
    SCALAR_ARRAY = "scalar_array"
    SCALAR_OUT = "scalar_out"
    STRING = "string"
    STRING_ARRAY = "string_array"
    BYTES_IN = "bytes_in"
    BYTES_OUT = "bytes_out"
    # The clGetXInfo trio: a size in, a buffer out, a size out.
    INFO_VALUE = "info_value"
    STRUCT_IN = "struct_in"
    STRUCT_OUT = "struct_out"
    # A zero-terminated key/value list, some of whose values are handles.
    PROPERTIES = "properties"
    # A zero-terminated flat list. Device partition properties are this
    # rather than pairs: CL_DEVICE_PARTITION_BY_COUNTS is followed by a
    # count per sub-device and a zero of its own before the terminator,
    # so walking it two at a time runs off the end.
    PROPERTY_LIST = "property_list"
    # Nothing crosses. Callbacks and their user data: a function pointer
    # is meaningless in the other address space, so the guest keeps it
    # and calls it itself.
    IGNORED = "ignored"


@dataclass
class Param:
    name: str
    type: CType
    role: Role
    # The parameter that counts this one, for the array roles.
    len_param: str | None = None
    # For STRING_ARRAY: the parameter holding per-string lengths.
    lengths_param: str | None = None
    # For INFO_VALUE: the size in and the size out.
    size_param: str | None = None
    size_ret_param: str | None = None
    # Whether the pointer may be null.
    optional: bool = True
    # The element type for array roles, and the struct name for struct
    # roles; ``type.base`` with the pointer stripped.
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
    # A command whose answer the caller does not wait for. The ICD
    # returns CL_SUCCESS and the failure, if any, surfaces at the next
    # synchronising call — the same bargain the Vulkan side makes.
    is_async: bool = False

    @property
    def returns_handle(self) -> bool:
        return self.return_type.pointer_depth == 0 and \
            self.return_type.base.startswith("cl_") and \
            self.return_type.base not in ("cl_int", "cl_uint", "cl_ulong",
                                          "cl_bool")

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
