from __future__ import annotations

from .model import Command, Length, LengthKind, Param, Registry, Role
from .parse import RawCommand, RawParam, RawRegistry
from .profile import Profile

CALLBACK_TYPES = {"GLDEBUGPROC", "GLDEBUGPROCARB", "GLDEBUGPROCAMD",
                  "GLVULKANPROCNV", "PROC", "FARPROC"}

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
        return Role.BUFFER_OFFSET
    if length is None:
        raise LoweringError(
            f"{cmd.name}.{param.name}: {param.text} has no length in the "
            f"registry; give it one in the profile")
    return Role.ARRAY_IN if param.type.is_const else Role.ARRAY_OUT


def _lower_command(raw: RawCommand, prof: Profile, tier: str) -> Command:
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
    reg = Registry(types=dict(raw.types),
                   pname_counts=dict(prof.pname_counts),
                   handwritten=set(prof.handwritten),
                   answered_by_guest=set(prof.answered_by_guest),
                   synchronous=set(prof.synchronous),
                   read_into_blob=set(prof.read_into_blob))
    wanted: list[tuple[str, str]] = []
    seen: set[str] = set()
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
    for name, raw_command in raw.commands.items():
        if name in reg.commands:
            continue
        target = raw_command.alias
        if target is None or target not in reg.commands:
            continue
        if reg.commands[target].api != "gl":
            continue
        reg.aliases[name] = target
    for name, target in prof.aliases.items():
        if name in reg.commands or target not in reg.commands:
            continue
        reg.aliases[name] = target

    for name in sorted(prof.driver_implemented):
        entry = raw.commands.get(name)
        if entry is None or entry.api != "gl" or name in reg.commands:
            continue
        if entry.alias in prof.driver_implemented and entry.alias in raw.commands:
            continue
        reg.driver_entries[name] = (entry.return_type.cpp(),
                                    ", ".join(p.text for p in entry.params))
    for name in sorted(prof.driver_implemented):
        entry = raw.commands.get(name)
        if entry is None or name in reg.driver_entries or name in reg.commands:
            continue
        if entry.alias in reg.driver_entries:
            reg.aliases[name] = entry.alias

    generated = set(reg.commands)
    for name, commands in sorted(raw.extensions.items()):
        if not name.startswith("GL_"):
            continue
        if all(command in generated or command in reg.aliases or
               command in prof.driver_implemented
               for command in commands):
            reg.carried_extensions.append(name)

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
