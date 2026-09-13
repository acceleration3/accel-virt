"""Raw registry plus profile, into commands the emitters can write out.

The rules here decide a role and a length for every parameter, and
refuse rather than guess. A refusal is a line in the profile waiting to
be written: it names the command, the parameter and what the registry
said, and generation stops until someone says what it means.
"""

from __future__ import annotations

from .model import Command, Length, LengthKind, Param, Registry, Role
from .parse import RawCommand, RawParam, RawRegistry
from .profile import Profile

# Types that are a function pointer rather than data. The registry
# declares them as plain typedefs, so there is nothing in the document
# to recognise them by.
CALLBACK_TYPES = {"GLDEBUGPROC", "GLDEBUGPROCARB", "GLDEBUGPROCAMD",
                  "GLVULKANPROCNV", "PROC", "FARPROC"}

# The pixel-length forms. Every one of them is the same question with a
# different number of extents.
PIXEL_LENS = {
    "COMPSIZE(format,type)",
    "COMPSIZE(format,type,width)",
    "COMPSIZE(format,type,width,height)",
    "COMPSIZE(format,type,width,height,depth)",
    "COMPSIZE(target,level,format,type)",
}


class LoweringError(Exception):
    pass


def _compsize_reads(text: str) -> tuple[str, ...]:
    inner = text[len("COMPSIZE("):-1]
    return tuple(x.strip() for x in inner.split(",") if x.strip())


def _length_from(cmd: RawCommand, param: RawParam,
                 override: dict) -> Length | None:
    # A length written in the profile is C the author meant, and is
    # taken as written: it may name a helper or a constant, neither of
    # which is a sibling parameter.
    if "len" in override:
        text = str(override["len"])
        if text.isdigit():
            return Length(LengthKind.FIXED, count=int(text))
        names = {p.name for p in cmd.params}
        kind = LengthKind.PARAM if text in names else LengthKind.EXPR
        return Length(kind, text=text)
    text = param.len_text
    if text is None:
        return None
    text = str(text)
    if text.isdigit():
        return Length(LengthKind.FIXED, count=int(text))
    if not text.startswith("COMPSIZE("):
        # A sibling, or arithmetic over siblings. Both are valid C in
        # the scope the generated code writes them into, so they are
        # kept as written; a sibling that does not exist is a mistake
        # worth stopping for.
        names = {p.name for p in cmd.params}
        used = [w for w in _identifiers(text)]
        unknown = [w for w in used if w not in names]
        if unknown:
            raise LoweringError(
                f"{cmd.name}.{param.name}: len=\"{text}\" names "
                f"{unknown[0]}, which is not a parameter")
        kind = LengthKind.PARAM if text in names else LengthKind.EXPR
        return Length(kind, text=text)
    reads = _compsize_reads(text)
    if text in PIXEL_LENS:
        return Length(LengthKind.PIXELS, text=text, reads=reads)
    if reads == ("pname",) or (len(reads) == 1 and reads[0].endswith("pname")):
        return Length(LengthKind.PNAME, text=text, reads=reads)
    if len(reads) == 2 and reads[1] == "type":
        return Length(LengthKind.TYPED_COUNT, text=text, reads=reads)
    raise LoweringError(
        f"{cmd.name}.{param.name}: len=\"{text}\" is a COMPSIZE the rules "
        f"cannot read; say what it means in the profile")


def _identifiers(text: str) -> list[str]:
    out: list[str] = []
    word = ""
    for ch in text:
        if ch.isalnum() or ch == "_":
            word += ch
        else:
            if word and not word.isdigit():
                out.append(word)
            word = ""
    if word and not word.isdigit():
        out.append(word)
    return out


def _role_of(cmd: RawCommand, param: RawParam, override: dict,
             length: Length | None) -> Role:
    if "role" in override:
        return Role(override["role"])
    if param.type.base in CALLBACK_TYPES:
        return Role.IGNORED
    if param.type.base == "GLsync":
        return Role.SYNC
    if param.type.pointer_depth == 0:
        return Role.SCALAR

    is_text = param.type.base in ("GLchar", "GLcharARB")
    if param.type.pointer_depth >= 2:
        if is_text:
            return Role.STRING_ARRAY_IN
        raise LoweringError(
            f"{cmd.name}.{param.name}: {param.text} is a pointer to a "
            f"pointer, and the rules do not know what it points at")
    if is_text:
        if param.type.is_const:
            return Role.STRING_IN
        return Role.STRING_OUT
    if length is not None and length.kind is LengthKind.PIXELS:
        return Role.PIXELS_IN if param.type.is_const else Role.PIXELS_OUT
    if param.type.base == "void" and length is None:
        # A void* with no length is an offset into the bound buffer.
        # Client-side arrays are the other reading, and the guest tells
        # them apart at the call: see the ICD.
        return Role.BUFFER_OFFSET
    if length is None:
        raise LoweringError(
            f"{cmd.name}.{param.name}: {param.text} has no length in the "
            f"registry; give it one in the profile")
    return Role.ARRAY_IN if param.type.is_const else Role.ARRAY_OUT


def _lower_command(raw: RawCommand, prof: Profile, tier: str) -> Command:
    # A command that answers with an address has no meaning across the
    # boundary: the address belongs to the process that made it. Those
    # need a driver-side answer — a shadow copy for the mapping ones —
    # and until one exists they are excluded rather than generated into
    # a function that returns nothing.
    ret = raw.return_type
    if (ret.pointer_depth > 0 and
            ret.base not in ("GLubyte", "GLchar", "char") and
            ret.base != "GLsync"):
        raise LoweringError(
            f"{raw.name}: answers with {ret.cpp()}, which is an address in "
            f"the process that made it; exclude it or write it by hand")
    cmd = Command(name=raw.name, return_type=raw.return_type,
                  since=raw.since, api=raw.api, tier=tier,
                  force_sync=raw.name in prof.synchronous)
    for raw_param in raw.params:
        override = prof.param_override(raw.name, raw_param.name)
        # A role written in the profile settles the question, and some
        # of those roles have no length to work out — so the role is
        # asked for first when the profile gives one.
        if "role" in override and "len" not in override:
            role = Role(override["role"])
            length = None
            if role in (Role.ARRAY_IN, Role.ARRAY_OUT, Role.STRING_OUT,
                        Role.STRING_ARRAY_IN, Role.PIXELS_IN, Role.PIXELS_OUT):
                length = _length_from(raw, raw_param, override)
        else:
            length = _length_from(raw, raw_param, override)
            role = _role_of(raw, raw_param, override, length)
        if role in (Role.SCALAR, Role.SYNC, Role.IGNORED,
                    Role.BUFFER_OFFSET):
            length = None
        if role is Role.OFFSET_ARRAY and length is None:
            length = _length_from(raw, raw_param, override)
        if role is Role.STRING_IN:
            length = Length(LengthKind.STRING)
        param = Param(name=raw_param.name, type=raw_param.type, role=role,
                      length=length, element=raw_param.type.base)
        if role is Role.STRING_ARRAY_IN:
            param.has_lengths = any(other.name == "length"
                                    for other in raw.params)
        if role is Role.STRING_OUT and length is not None:
            param.size_param = length.text if length.kind is LengthKind.PARAM else None
            names = {p.name for p in raw.params}
            param.length_param = "length" if "length" in names else None
        cmd.params.append(param)
    cmd.command_id = prof.command_id(raw.name)
    return cmd


def lower(raw: RawRegistry, prof: Profile) -> Registry:
    reg = Registry(pname_counts=dict(prof.pname_counts),
                   handwritten=set(prof.handwritten),
                   answered_by_guest=set(prof.answered_by_guest),
                   synchronous=set(prof.synchronous))
    wanted: list[tuple[str, str]] = []
    seen: set[str] = set()
    # A tier is the union of what its features require, minus
    # everything any of them removed: the core profile is written in
    # the registry as a set of removals late in the list that apply to
    # what the early features asked for.
    for tier, features in prof.tiers.items():
        asked: set[str] = set()
        gone: set[str] = set()
        for feature in features:
            if feature not in raw.features:
                raise LoweringError(
                    f"tier {tier}: the registry has no feature {feature}")
            asked |= raw.features[feature]
            gone |= raw.removals.get(feature, set())
        for name in sorted(asked - gone):
            if name in seen or name in prof.excluded:
                continue
            seen.add(name)
            wanted.append((name, tier))
    for name in prof.extra:
        if name not in seen and name not in prof.excluded:
            seen.add(name)
            wanted.append((name, "extra"))

    # What version this driver may claim: the highest feature any tier
    # asked for. Claiming more than that is the mistake the OpenCL side
    # has a test for and this one now has a table for.
    for tier, features in prof.tiers.items():
        for feature in features:
            if not feature.startswith("GL_VERSION_"):
                continue
            parts = feature[len("GL_VERSION_"):].split("_")
            if len(parts) != 2:
                continue
            major, minor = int(parts[0]), int(parts[1])
            if (major, minor) > (reg.tier_major, reg.tier_minor):
                reg.tier_major, reg.tier_minor = major, minor
    # The boot tier names commands rather than features, so a profile
    # with no feature tiers still has to say what it is.
    if (reg.tier_major, reg.tier_minor) == (1, 0) and prof.extra:
        reg.tier_major, reg.tier_minor = 3, 3

    problems: list[str] = []
    for name, tier in wanted:
        if name not in raw.commands:
            problems.append(f"{name}: not in the registry")
            continue
        try:
            reg.commands[name] = _lower_command(raw.commands[name], prof, tier)
        except LoweringError as error:
            problems.append(str(error))
    # An extension may be claimed only if every command it needs is
    # generated. One with no commands of its own rides on the ones that
    # are already here and is safe to carry.
    # Every other spelling of what was generated. A raw command that
    # aliases one this driver carries, and is not carried itself, is a
    # name the loader may ask for and this driver can answer.
    for name, raw_command in raw.commands.items():
        if name in reg.commands:
            continue
        target = raw_command.alias
        if target is None or target not in reg.commands:
            continue
        if reg.commands[target].api != "gl":
            continue
        reg.aliases[name] = target
    # And the ones the profile declares, where the registry declares
    # none. See the [aliases] table there for why there are any.
    for name, target in prof.aliases.items():
        if name in reg.commands or target not in reg.commands:
            continue
        reg.aliases[name] = target

    generated = set(reg.commands)
    for name, commands in sorted(raw.extensions.items()):
        if not name.startswith("GL_"):
            continue
        # A command counts as carried under either of its names. The
        # EXT and ARB spellings of the framebuffer family are the core
        # entry points with another name on them, and the driver's
        # table answers to both — an application that asks for
        # GL_EXT_framebuffer_object and gets no for an extension it can
        # in fact use turns its graphics processor off. That is what
        # Photoshop's sniffer does.
        if all(command in generated or command in reg.aliases or
               command in prof.driver_implemented
               for command in commands):
            reg.carried_extensions.append(name)

    # An extension is no use without the one it is written on top of.
    #
    # A few carry no commands of their own — they are GLSL, or a
    # relaxation of a rule — and pass the test above by having nothing
    # to fail it, while everything an application does with them goes
    # through the base extension's entry points. The registry does not
    # record that dependency, so the profile does: see
    # [extension_requires] there, and gl-500-texture-sparse-ext, which
    # found GL_EXT_sparse_texture2 offered, asked for
    # glTexPageCommitmentARB, and called the null it was handed.
    carried = set(reg.carried_extensions)
    dropped = True
    while dropped:
        dropped = False
        for name in list(carried):
            needs = prof.extension_requires.get(name)
            if needs is not None and needs not in carried:
                carried.discard(name)
                dropped = True
    reg.carried_extensions = [name for name in reg.carried_extensions
                              if name in carried]

    if problems:
        raise LoweringError(
            f"{len(problems)} command(s) the profile has to explain:\n  " +
            "\n  ".join(problems[:40]) +
            ("\n  ..." if len(problems) > 40 else ""))
    return reg
