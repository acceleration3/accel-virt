from __future__ import annotations

from pathlib import Path

from ..emit.common import write
from .model import SCALARS, Command, Param, Registry

PREFIX = "d3d"
NS = "av::d3dproto"
HEADER = ""


def _use(reg: Registry) -> None:
    global PREFIX, NS, HEADER
    PREFIX = reg.prefix
    NS = f"av::{reg.prefix}proto"
    HEADER = ("// THIS FILE IS GENERATED. DO NOT EDIT.\n"
              f"// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/{reg.prefix}.toml "
              "--out ...\n")


def _inc(name: str) -> str:
    return f'"proto/generated/{PREFIX}/{PREFIX}_{name}"'

INLINE_BYTES = 4096
INLINE_REPLY_BYTES = 32 * 1024


def _ctype(reg: Registry, t: str) -> str:
    return SCALARS[t][0] if t in SCALARS else t


def _client_params(reg: Registry, cmd: Command) -> list[str]:
    out = []
    for p in cmd.params:
        if p.type == "bytes":
            out += [f"const void* {p.name}", f"uint64_t {p.name}_size"]
        elif p.is_array:
            out.append(f"const {_ctype(reg, p.type)}* {p.name}")
        elif p.type in SCALARS:
            out.append(f"{_ctype(reg, p.type)} {p.name}")
        else:
            out.append(f"const {p.type}& {p.name}")
    for p in cmd.reply:
        if p.type == "out_bytes":
            out += [f"void* {p.name}", f"uint64_t {p.name}_capacity", f"uint64_t* {p.name}_size"]
        else:
            out.append(f"{_ctype(reg, p.type)}* {p.name}")
    return out


def _server_params(reg: Registry, cmd: Command) -> list[str]:
    out = []
    for p in cmd.params:
        if p.type == "bytes":
            out += [f"const uint8_t* {p.name}", f"uint64_t {p.name}_size"]
        elif p.is_array:
            out.append(f"const {_ctype(reg, p.type)}* {p.name}")
        elif p.type in SCALARS:
            out.append(f"{_ctype(reg, p.type)} {p.name}")
        else:
            out.append(f"const {p.type}& {p.name}")
    for p in cmd.reply:
        if p.type == "out_bytes":
            out += [f"uint8_t* {p.name}", f"uint64_t {p.name}_capacity", f"uint64_t& {p.name}_size"]
        else:
            out.append(f"{_ctype(reg, p.type)}& {p.name}")
    return out


def _server_args(cmd: Command) -> list[str]:
    out = []
    for p in cmd.params:
        out += [p.name, f"{p.name}_size"] if p.type == "bytes" else [p.name]
    for p in cmd.reply:
        out += [p.name, f"{p.name}_capacity", f"{p.name}_size"] if p.type == "out_bytes" else [p.name]
    return out


def _const(cmd: Command) -> str:
    return f"kCmd_{cmd.name}"


def _command_id(reg: Registry, cmd: Command) -> int:
    return (reg.api << 16) | cmd.id


def emit_ids(reg: Registry) -> str:
    lines = [HEADER, "#pragma once", "", "#include <stdint.h>", "", f"namespace {NS} {{", "",
             f"inline constexpr uint16_t kApi = {reg.api};", ""]
    for cmd in sorted(reg.commands, key=lambda c: c.id):
        lines.append(f"inline constexpr uint32_t {_const(cmd)} = 0x{_command_id(reg, cmd):08x};")
    lines += ["", f"}}  // namespace {NS}", ""]
    return "\n".join(lines)


def emit_types(reg: Registry) -> str:
    lines = [HEADER, "#pragma once", "", "#include <stddef.h>", "#include <stdint.h>", "",
             f"namespace {NS} {{", ""]
    for s in reg.structs.values():
        lines.append(f"struct {s.name} {{")
        for f in s.fields:
            lines.append(f"    {f.c_decl(reg.structs)}")
        lines.append("};")
        lines.append(f"static_assert(sizeof({s.name}) == {s.size}, \"{s.name} crosses as {s.size} bytes\");")
        lines.append(f"static_assert(__is_trivially_copyable({s.name}), \"{s.name} is plain data\");")
        lines.append("")
    lines += [f"}}  // namespace {NS}", ""]
    return "\n".join(lines)


def emit_names_h(reg: Registry) -> str:
    return "\n".join([HEADER, "#pragma once", "", "#include <stdint.h>", "", f"namespace {NS} {{", "",
                      "// The registry's name for a command id, or null for one it does not have.",
                      "const char* command_name(uint32_t id);", "", f"}}  // namespace {NS}", ""])


def emit_names_cpp(reg: Registry) -> str:
    lines = [HEADER, f'#include {_inc("command_names.h")}', "",
             f'#include {_inc("protocol_ids.h")}', "", f"namespace {NS} {{", "",
             "const char* command_name(uint32_t id) {", "    switch (id) {"]
    for cmd in sorted(reg.commands, key=lambda c: c.id):
        lines.append(f'        case {_const(cmd)}: return "{cmd.name}";')
    lines += ["        default: return nullptr;", "    }", "}", "", f"}}  // namespace {NS}", ""]
    return "\n".join(lines)


def emit_client_h(reg: Registry) -> str:
    lines = [HEADER, "#pragma once", "", "#include <stdint.h>", "",
             '#include "proto/client/client_transport.hpp"', f'#include {_inc("wire_types.h")}', "",
             f"namespace {NS} {{", "",
             "// One wrapper per command: false when it could not be encoded or sent, or -",
             "// for a command with a reply - when the answer did not decode.", ""]
    for cmd in reg.commands:
        if cmd.doc:
            lines += [f"// {line}" for line in cmd.doc.strip().splitlines()]
        params = ", ".join(["::av::ClientTransport& t"] + _client_params(reg, cmd))
        lines.append(f"[[nodiscard]] bool av_{PREFIX}_{cmd.name}({params});")
    lines += ["", f"}}  // namespace {NS}", ""]
    return "\n".join(lines)


CLIENT_HELPERS = f"""
namespace {{

constexpr uint64_t inline_bytes = {INLINE_BYTES}u;
constexpr uint64_t inline_reply_bytes = {INLINE_REPLY_BYTES}u;
constexpr uint8_t tag_inline = 0u;
constexpr uint8_t tag_blob = 1u;

bool put_bytes(::av::ClientTransport& t, ::av::WireWriter& b, const void* data, uint64_t size) {{
    if (size != 0u && data == nullptr) return false;
    b.write_pod<uint64_t>(size);
    ::av::ClientBlobArena& arena = t.blobs();
    if (size > inline_bytes && arena.ready()) {{
        const ::av::ClientBlobLease lease = arena.take(static_cast<size_t>(size));
        if (lease.taken()) {{
            __builtin_memcpy(lease.data(), data, static_cast<size_t>(size));
            b.write_pod<uint8_t>(tag_blob);
            b.write_pod<uint64_t>(lease.offset());
            return true;
        }}
    }}
    b.write_pod<uint8_t>(tag_inline);
    if (size != 0u) b.write(data, static_cast<size_t>(size));
    return b.status() == ::av::Status::ok;
}}

bool ask_bytes(::av::ClientTransport& t, ::av::WireWriter& b, void* data, uint64_t capacity,
               ::av::ClientBlobLease& lease) {{
    if (capacity != 0u && data == nullptr) return false;
    b.write_pod<uint64_t>(capacity);
    ::av::ClientBlobArena& arena = t.blobs();
    if (capacity > inline_reply_bytes) {{
        if (!arena.ready()) return false;
        lease = arena.take(static_cast<size_t>(capacity));
        if (!lease.taken()) return false;
        b.write_pod<uint8_t>(tag_blob);
        b.write_pod<uint64_t>(lease.offset());
        return true;
    }}
    b.write_pod<uint8_t>(tag_inline);
    return true;
}}

/* A command given up before it was sent still ends its call: a transport
   that lent the writer under a lock lets the lock go there. */
bool abandon(::av::ClientTransport& t) {{
    t.end_call();
    return false;
}}

bool take_bytes(::av::WireReader& r, void* data, uint64_t capacity, const ::av::ClientBlobLease& lease,
                uint64_t* size) {{
    uint64_t answered = 0u;
    if (!r.read_pod(answered) || answered > capacity) return false;
    if (lease.taken()) {{
        if (answered != 0u) __builtin_memcpy(data, lease.data(), static_cast<size_t>(answered));
    }} else if (answered != 0u && !r.read(data, static_cast<size_t>(answered))) {{
        return false;
    }}
    if (size != nullptr) *size = answered;
    return true;
}}

}}  // namespace
"""


def _helpers(reg: Registry, text: str) -> str:
    kinds = {p.type for c in reg.commands for p in c.params + c.reply}
    unused = []
    if "bytes" not in kinds:
        unused += ["bool put_bytes(", "bool read_bytes("]
    if "out_bytes" not in kinds:
        unused += ["bool ask_bytes(", "bool take_bytes(", "bool read_out_bytes(", "void write_out_bytes("]
    for head in unused:
        text = text.replace("\n" + head, "\n[[maybe_unused]] " + head)
    return text


def _encode(reg: Registry, p: Param, lines: list[str]) -> None:
    if p.type == "bytes":
        lines.append(f"    if (!put_bytes(t, b, {p.name}, {p.name}_size)) return abandon(t);")
    elif p.is_array:
        lines.append(f"    if ({p.count} > {p.max}u || ({p.count} != 0u && {p.name} == nullptr)) return abandon(t);")
        lines.append(f"    if ({p.count} != 0u) b.write({p.name}, sizeof(*{p.name}) * {p.count});")
    elif p.type in SCALARS:
        lines.append(f"    b.write_pod<{_ctype(reg, p.type)}>({p.name});")
    else:
        lines.append(f"    b.write_pod({p.name});")


def emit_client_cpp(reg: Registry) -> str:
    lines = [HEADER, f'#include {_inc("dispatch_client.h")}', "",
             '#include "core/trace/function_scope.hpp"', '#include "proto/client/client_blob_arena.hpp"',
             f'#include {_inc("protocol_ids.h")}', '#include "proto/client/sync_call_scope.hpp"', "",
             f"namespace {NS} {{", _helpers(reg, CLIENT_HELPERS)]
    for cmd in reg.commands:
        params = ", ".join(["::av::ClientTransport& t"] + _client_params(reg, cmd))
        lines.append(f"bool av_{PREFIX}_{cmd.name}({params}) {{")
        lines.append(f'    AV_SCOPE("av_{PREFIX}_{cmd.name}");')
        lines.append("    ::av::WireWriter b = t.request_writer();")
        for p in cmd.params:
            _encode(reg, p, lines)
        leases = [p for p in cmd.reply if p.type == "out_bytes"]
        for p in leases:
            lines.append(f"    ::av::ClientBlobLease {p.name}_lease;")
            lines.append(f"    if (!ask_bytes(t, b, {p.name}, {p.name}_capacity, {p.name}_lease)) return abandon(t);")
        if not cmd.sync:
            lines.append(f"    return t.send_async({_const(cmd)}, b) == ::av::TransportResult::ok;")
            lines += ["}", ""]
            continue
        lines.append("    const ::av::SyncCallScope call{t};")
        lines.append("    ::av::WireReader r{nullptr, 0u};")
        lines.append(f"    if (t.send_sync({_const(cmd)}, b, r) != ::av::TransportResult::ok) return false;")
        for p in cmd.reply:
            if p.type == "out_bytes":
                lines.append(f"    if (!take_bytes(r, {p.name}, {p.name}_capacity, {p.name}_lease, {p.name}_size)) "
                             "return false;")
            else:
                local = "value" if p.name != "value" and all(q.name != "value" for q in cmd.params) else "answer"
                lines.append(f"    {{ {_ctype(reg, p.type)} {local}{{}};")
                lines.append(f"      if (!r.read_pod({local})) return false;")
                lines.append(f"      if ({p.name} != nullptr) *{p.name} = {local}; }}")
        lines.append("    return r.remaining() == 0u;")
        lines += ["}", ""]
    lines += [f"}}  // namespace {NS}", ""]
    return "\n".join(lines)


def emit_server_h(reg: Registry) -> str:
    lines = [HEADER, "#pragma once", "", "#include <stddef.h>", "#include <stdint.h>", "",
             f'#include {_inc("wire_types.h")}', '#include "proto/wire/guest_bytes.hpp"',
             '#include "proto/wire/wire_arena.hpp"',
             '#include "proto/wire/wire_reader.hpp"', '#include "proto/wire/wire_writer.hpp"', "",
             f"namespace {NS} {{", "",
             "// Implemented by the executor; the table below calls into it. Every value",
             "// handed to an on_ method came off the wire: a count is already within the",
             "// registry's maximum and an array holds that many, but what the values mean",
             "// - an object id, a format, a box - is the implementation's to judge. An",
             "// on_ method answers false for a command it refuses, which the device",
             "// treats as one that did not decode.",
             "class ServerDispatch {", "public:",
             "    ServerDispatch(const ServerDispatch&) = delete;",
             "    ServerDispatch& operator=(const ServerDispatch&) = delete;", "",
             "    // `size` bytes at `offset` in the context's blob arena, or null for a range",
             "    // outside it.",
             "    uint8_t* guest_bytes(uint64_t offset, uint64_t size) const;", ""]
    for cmd in reg.commands:
        params = ", ".join(_server_params(reg, cmd))
        lines.append(f"    virtual bool on_{cmd.name}({params}) = 0;")
    lines += ["", "protected:",
              "    // `guest` is the context's blob arena, which the implementation owns and",
              "    // adopts its first import as.",
              "    explicit ServerDispatch(const ::av::GuestBytes& guest);",
              "    ~ServerDispatch() = default;", "",
              "private:", "    const ::av::GuestBytes& guest_;", "};", "",
              "// Whether the registry has this command at all.",
              "bool knows_command(uint32_t cmd_id);",
              "// Decodes one command and calls the dispatch; false for a frame that does",
              "// not decode, a count past its maximum, or a refusal.",
              "bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, ::av::WireReader& r,",
              "                      ::av::WireWriter& reply, ::av::WireArena& arena);", "",
              f"}}  // namespace {NS}", ""]
    return "\n".join(lines)


SERVER_HELPERS = f"""
namespace {{

constexpr uint64_t inline_reply_bytes = {INLINE_REPLY_BYTES}u;
constexpr uint8_t tag_inline = 0u;
constexpr uint8_t tag_blob = 1u;

bool read_bytes(ServerDispatch& s, ::av::WireReader& r, ::av::WireArena& arena, const uint8_t*& data,
                uint64_t& size) {{
    uint8_t tag = 0u;
    if (!r.read_pod(size) || !r.read_pod(tag)) return false;
    data = nullptr;
    if (tag == tag_blob) {{
        uint64_t offset = 0u;
        if (!r.read_pod(offset)) return false;
        data = s.guest_bytes(offset, size);
        return size == 0u || data != nullptr;
    }}
    if (tag != tag_inline) return false;
    if (size == 0u) return true;
    if (size > r.remaining()) return false;
    uint8_t* copy = arena.allocate<uint8_t>(static_cast<size_t>(size));
    if (copy == nullptr) return false;
    if (!r.read(copy, static_cast<size_t>(size))) return false;
    data = copy;
    return true;
}}

bool read_out_bytes(ServerDispatch& s, ::av::WireReader& r, ::av::WireArena& arena, uint8_t*& data,
                    uint64_t& capacity, bool& in_blob) {{
    uint8_t tag = 0u;
    if (!r.read_pod(capacity) || !r.read_pod(tag)) return false;
    data = nullptr;
    in_blob = tag == tag_blob;
    if (in_blob) {{
        uint64_t offset = 0u;
        if (!r.read_pod(offset)) return false;
        data = s.guest_bytes(offset, capacity);
        return capacity == 0u || data != nullptr;
    }}
    if (tag != tag_inline || capacity > inline_reply_bytes) return false;
    if (capacity == 0u) return true;
    data = arena.allocate<uint8_t>(static_cast<size_t>(capacity));
    return data != nullptr;
}}

void write_out_bytes(::av::WireWriter& reply, const uint8_t* data, uint64_t size, uint64_t capacity, bool in_blob) {{
    const uint64_t answered = size < capacity ? size : capacity;
    reply.write_pod<uint64_t>(answered);
    if (!in_blob && answered != 0u) reply.write(data, static_cast<size_t>(answered));
}}

}}  // namespace
"""


def _decode(reg: Registry, p: Param, lines: list[str]) -> None:
    ct = _ctype(reg, p.type)
    if p.type == "bytes":
        lines.append(f"    const uint8_t* {p.name} = nullptr;")
        lines.append(f"    uint64_t {p.name}_size = 0u;")
        lines.append(f"    if (!read_bytes(s, r, arena, {p.name}, {p.name}_size)) return false;")
    elif p.is_array:
        lines.append(f"    if ({p.count} > {p.max}u) return false;")
        lines.append(f"    {ct}* {p.name} = nullptr;")
        lines.append(f"    if ({p.count} != 0u) {{")
        lines.append(f"        {p.name} = arena.allocate<{ct}>({p.count});")
        lines.append(f"        if ({p.name} == nullptr) return false;")
        lines.append(f"        if (!r.read({p.name}, sizeof({ct}) * {p.count})) return false;")
        lines.append("    }")
    else:
        lines.append(f"    {ct} {p.name}{{}};")
        lines.append(f"    if (!r.read_pod({p.name})) return false;")


def emit_server_cpp(reg: Registry) -> str:
    lines = [HEADER, f'#include {_inc("dispatch_server.h")}', "",
             '#include "core/trace/function_scope.hpp"', f'#include {_inc("protocol_ids.h")}', "",
             f"namespace {NS} {{", _helpers(reg, SERVER_HELPERS),
             "ServerDispatch::ServerDispatch(const ::av::GuestBytes& guest) : guest_(guest) {}", "",
             "uint8_t* ServerDispatch::guest_bytes(uint64_t offset, uint64_t size) const {",
             "    return guest_.at(offset, size);", "}", ""]
    for cmd in reg.commands:
        lines.append(f"static bool decode_{cmd.name}(ServerDispatch& s, ::av::WireReader& r, "
                     "::av::WireWriter& reply, ::av::WireArena& arena) {")
        lines.append(f'    AV_SCOPE("server:{PREFIX}:{cmd.name}");')
        lines += ["    (void)s;", "    (void)reply;", "    (void)arena;"]
        for p in cmd.params:
            _decode(reg, p, lines)
        for p in cmd.reply:
            if p.type == "out_bytes":
                lines.append(f"    uint8_t* {p.name} = nullptr;")
                lines.append(f"    uint64_t {p.name}_capacity = 0u;")
                lines.append(f"    uint64_t {p.name}_size = 0u;")
                lines.append(f"    bool {p.name}_in_blob = false;")
                lines.append(f"    if (!read_out_bytes(s, r, arena, {p.name}, {p.name}_capacity, {p.name}_in_blob)) "
                             "return false;")
            else:
                lines.append(f"    {_ctype(reg, p.type)} {p.name}{{}};")
        lines.append("    if (r.remaining() != 0u) return false;")
        lines.append(f"    if (!s.on_{cmd.name}({', '.join(_server_args(cmd))})) return false;")
        for p in cmd.reply:
            if p.type == "out_bytes":
                lines.append(f"    write_out_bytes(reply, {p.name}, {p.name}_size, {p.name}_capacity, "
                             f"{p.name}_in_blob);")
            else:
                lines.append(f"    reply.write_pod({p.name});")
        lines += ["    return true;", "}", ""]
    lines += ["bool knows_command(uint32_t cmd_id) {", "    switch (cmd_id) {"]
    for cmd in reg.commands:
        lines.append(f"        case {_const(cmd)}:")
    lines += ["            return true;", "        default: return false;", "    }", "}", ""]
    lines += ["bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, ::av::WireReader& r,",
              "                      ::av::WireWriter& reply, ::av::WireArena& arena) {",
              "    switch (cmd_id) {"]
    for cmd in reg.commands:
        lines.append(f"        case {_const(cmd)}: return decode_{cmd.name}(s, r, reply, arena);")
    lines += ["        default: return false;", "    }", "}", "", f"}}  // namespace {NS}", ""]
    return "\n".join(lines)


def emit_all(reg: Registry, out: Path) -> None:
    _use(reg)
    p = reg.prefix
    files = {
        f"{p}_protocol_ids.h": emit_ids(reg),
        f"{p}_wire_types.h": emit_types(reg),
        f"{p}_command_names.h": emit_names_h(reg),
        f"{p}_command_names.cpp": emit_names_cpp(reg),
        f"{p}_dispatch_client.h": emit_client_h(reg),
        f"{p}_dispatch_client.cpp": emit_client_cpp(reg),
        f"{p}_dispatch_server.h": emit_server_h(reg),
        f"{p}_dispatch_server.cpp": emit_server_cpp(reg),
    }
    for name, content in files.items():
        write(out / name, content)
