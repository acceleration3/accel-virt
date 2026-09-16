"""What a lowered OpenGL command looks like.

OpenGL differs from the other two APIs on the wire in three ways, and
the model is shaped by them.

There are no handles. A GL object is a name the driver hands out, and
this driver hands out the host's names unchanged: nothing has to be
translated, because a GLuint means the same thing to the guest as it
does to the server. The one exception is GLsync, which is a pointer,
and which travels as the server's own — an application may compare it
but may not read through it.

Most calls answer nothing. A command with no return value and no output
parameter has nothing the caller can wait for, so it goes on the ring
and the caller carries on. That is the difference between a GL that is
usable over a VM boundary and one that is not, and it is decided here:
``Command.is_async``.

And the registry says how long its pointers are, which cl.xml did not.
``len`` is either a number, a sibling, an expression over siblings, or
COMPSIZE(...) — a note that the length depends on an enum whose meaning
the registry does not carry. The Length below is that answer made
explicit, and anything the rules cannot decide is a generation error
rather than a guess.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum

from ..ir import CType


class Role(str, Enum):
    SCALAR = "scalar"
    # A run of scalars, in or out, whose length is a Length below.
    ARRAY_IN = "array_in"
    ARRAY_OUT = "array_out"
    # A null-terminated string in; an array of them for glShaderSource.
    STRING_IN = "string_in"
    STRING_ARRAY_IN = "string_array_in"
    # A buffer the call fills with text, with a capacity in and a
    # length out: the info logs and the name queries.
    STRING_OUT = "string_out"
    # Pixels, whose length depends on the format, the type, the extent
    # and the pixel store state. The guest works it out; the wire
    # carries the bytes.
    PIXELS_IN = "pixels_in"
    PIXELS_OUT = "pixels_out"
    # A void* that is an offset into whatever buffer is bound, not an
    # address — glDrawElements' indices, glVertexAttribPointer's
    # pointer. Which one it is depends on the binding at the time, so
    # the guest decides and the wire carries an integer.
    BUFFER_OFFSET = "buffer_offset"
    # An array of void* that are offsets into bound buffers rather than
    # addresses: glMultiDrawElements' indices. Carried as integers.
    OFFSET_ARRAY = "offset_array"
    # GLsync: the server's own pointer, carried as an opaque 64 bits.
    SYNC = "sync"
    # Nothing crosses: a function pointer and the data that goes with
    # it. The guest keeps both and calls the callback itself.
    IGNORED = "ignored"


class LengthKind(str, Enum):
    FIXED = "fixed"          # a number written in the registry
    PARAM = "param"          # a sibling parameter holds the count
    EXPR = "expr"            # an expression over siblings, valid C
    STRING = "string"        # null-terminated
    PNAME = "pname"          # how many values that enum answers with
    PIXELS = "pixels"        # format, type and extent
    TYPED_COUNT = "typed"    # a count of elements of an enum-named type


@dataclass
class Length:
    kind: LengthKind
    # For PARAM and EXPR: the text, which is C in both cases.
    text: str = ""
    # For FIXED.
    count: int = 0
    # For PNAME, PIXELS and TYPED_COUNT: the parameters it reads.
    reads: tuple[str, ...] = ()

    def is_known_at_compile_time(self) -> bool:
        return self.kind is LengthKind.FIXED


@dataclass
class Param:
    name: str
    type: CType
    role: Role
    length: Length | None = None
    # The element type for the array roles: ``type.base``.
    element: str = ""
    # STRING_OUT: the capacity in and the length out.
    size_param: str | None = None
    length_param: str | None = None
    # STRING_ARRAY_IN: whether the command carries per-string lengths,
    # as glShaderSource does and glGetUniformIndices does not.
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
    # Which tier of the profile asked for it.
    tier: str = ""
    # The profile says this one waits even though the wire cannot tell
    # that it must. See Registry.synchronous.
    force_sync: bool = False

    @property
    def returns_void(self) -> bool:
        return self.return_type.base == "void" and self.return_type.pointer_depth == 0

    @property
    def has_out(self) -> bool:
        return any(p.is_out for p in self.params)

    @property
    def mints_handle(self) -> bool:
        """Answers with a handle the guest can name for itself.

        A GLsync is opaque: made, waited on, deleted, never inspected.
        So the guest invents the number and the server records which of
        its own the number stands for, and the create stops being a
        round trip. See the rvk_sync_* fields on RealGl.
        """
        return (self.return_type.base == "GLsync"
                and self.return_type.pointer_depth == 0)

    @property
    def is_async(self) -> bool:
        """Nothing to wait for: no answer and nothing written back.

        A command that mints its own handle answers with a number the
        guest already has, so there is nothing to wait for there either.

        Except where the command's whole meaning is the waiting, which
        no signature shows — see ``force_sync``.
        """
        if self.force_sync:
            return False
        if self.mints_handle:
            return not self.has_out
        return self.returns_void and not self.has_out


@dataclass
class Registry:
    commands: dict[str, Command] = field(default_factory=dict)
    # The registry's own type declarations, in document order. The
    # generated tree defines them itself rather than including a GL
    # header: there is no OpenGL SDK on the host side of this project.
    types: dict[str, str] = field(default_factory=dict)
    # Enum name -> how many values a query with it answers with.
    pname_counts: dict[str, int] = field(default_factory=dict)
    # Commands the profile marks as written by hand: generated code
    # declares them and calls them, and does not define them.
    handwritten: set[str] = field(default_factory=set)
    # Commands the guest answers for itself. The generated client
    # declares them and does not define them; the definition is in
    # gl_helpers.cpp and never touches the wire.
    answered_by_guest: set[str] = field(default_factory=set)
    # The driver's own entry points the wire does not carry (the profile's
    # driver_implemented that the registry declares): name -> (return type,
    # parameter list), as gl.xml writes them.
    driver_entries: dict[str, tuple[str, str]] = field(default_factory=dict)
    # The other spellings of a command this driver carries.
    #
    # The registry says glDebugMessageInsertARB is glDebugMessageInsert
    # under another name, and a loader that asks for the ARB spelling of
    # a command means the same command. Answering nothing leaves a null
    # in the loader's table and the application calls through it: the
    # samples pack does exactly that, unconditionally, and crashes at
    # address zero.
    aliases: dict[str, str] = field(default_factory=dict)
    # Commands that must wait for the server even though they answer
    # with nothing. glFinish is the whole of the case and the reason
    # the rule exists: its signature is indistinguishable from
    # glDisable's, and sending it one-way makes it a no-op — the caller
    # returns immediately and every measurement taken after it is of a
    # queue rather than of a GPU.
    synchronous: set[str] = field(default_factory=set)
    # The highest GL version any tier asked for: what this driver may
    # honestly report.
    tier_major: int = 1
    tier_minor: int = 0
    # The extensions every one of whose commands is generated. An
    # extension that needs an entry point the wire does not carry is
    # not one this driver can offer, whatever the host says.
    carried_extensions: list[str] = field(default_factory=list)
