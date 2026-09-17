"""Generate Linux Vulkan ICD wrappers + name -> fnptr table.

The ICD layer is mechanically driven by each command's parameter list:

  * dispatchable handle inputs (VkInstance, VkPhysicalDevice, VkDevice,
    VkQueue, VkCommandBuffer) get unwrapped from the loader-magic-marked
    wrapper to expose the server-side handle on the wire;
  * dispatchable handle outputs get wrapped on the way back up, inheriting
    the parent ``Connection`` from the first input dispatchable handle;
  * arrays of dispatchable handles (the count_array_pair shape) get
    wrapped element-by-element into the caller's array;
  * VkAllocationCallbacks pass through; the wire side discards them;
  * the rest pass through unchanged.

Per-VkInstance Connection: the Connection is acquired in
``vkCreateInstance``, attached to the wrapped VkInstance, inherited by
every child handle, and released in ``vkDestroyInstance``. There is no
process-wide transport singleton anymore.

Hand-writing this for every newly added command is tedious and exactly
the kind of thing the codegen should own. With this module each new
command in ``profile.toml`` produces an ICD wrapper for free; the only
remaining manual ICD edit is for entry points like vkGetInstanceProcAddr
that don't go on the wire.
"""

from __future__ import annotations

from pathlib import Path

from ..ir import Command, Param, Registry, StructMember
from . import classify
from .common import GENERATED_BANNER, header_guard, write

# Dispatchable handle types we wrap with loader-magic. Non-dispatchable
# handles (VkBuffer, VkImage, etc.) cross the wire as raw 64-bit values
# and don't need this treatment. Defined once, in classify.
DISPATCHABLE_HANDLES = classify.DISPATCHABLE_HANDLES


def _is_recordable(cmd: Command, parent: Param) -> bool:
    """Does this command belong in a per-command-buffer batch?

    ``vkCmd*`` taking a VkCommandBuffer, plus Begin and End. Those are
    the commands that record into a buffer rather than acting
    immediately, and the buffer they record into is externally
    synchronised — so accumulating them per buffer needs no lock and
    cannot reorder anything.

    Begin and End earn their place: they bracket the recording rather
    than being global state, so putting them IN the batch removes two
    blocking round trips per frame and costs no extra message. The
    wrappers in ``src/icd/src/cmdbuf.cpp`` still exist — Begin drops the
    previous batch before recording into it, and End ships the batch
    once the End itself has been recorded — but the serialization goes
    through the recorder either way.

    Reset is NOT here. It has to reach the server even if the
    application never records into that buffer again, so it goes
    straight out on the transport (asynchronously) rather than sitting
    in a batch that might never be flushed.
    """
    if parent.type.base != "VkCommandBuffer":
        return False
    return cmd.name.startswith("vkCmd") or cmd.name in (
        "vkBeginCommandBuffer",
        "vkEndCommandBuffer",
    )


# ---- Helpers ---------------------------------------------------------------


# The object the loader dispatches a command on, which is its first
# parameter's type in vk.xml. The loader asks an ICD's
# vk_icdGetPhysicalDeviceProcAddr only for physical-device commands, and
# answering it a device-level function would have the loader call that
# function with a physical device, so the table has to say which is which.
# A command whose first parameter is no dispatchable handle
# (vkCreateInstance, the instance-less enumerations) is global.
_DISPATCH_LEVELS = {
    "VkInstance": "instance",
    "VkPhysicalDevice": "physical_device",
    "VkDevice": "device",
    "VkQueue": "device",
    "VkCommandBuffer": "device",
}


def dispatch_level(cmd: Command) -> str:
    """The DispatchLevel enumerator naming ``cmd``'s dispatch level."""
    if not cmd.params:
        return "global"
    first = cmd.params[0].type
    if first.pointer_depth != 0 or first.fixed_dim is not None:
        return "global"
    return _DISPATCH_LEVELS.get(first.base, "global")


def _is_count_array_pair(params: list[Param], i: int) -> bool:
    if i + 1 >= len(params):
        return False
    a, b = params[i], params[i + 1]
    if a.type.base not in ("uint32_t", "size_t"):
        return False
    if a.type.pointer_depth != 1 or b.type.pointer_depth != 1:
        return False
    return b.len_expr == a.name


def _param_cpp(t) -> str:
    """Decay fixed-array params to pointers in the wrapper signatures."""
    s = t.base
    eff_ptr = t.pointer_depth + (1 if t.fixed_dim is not None else 0)
    if eff_ptr:
        s += "*" * eff_ptr
    if t.is_const:
        s = "const " + s
    return s


def _signature(cmd: Command) -> str:
    args = ", ".join(f"{_param_cpp(p.type)} {p.name}" for p in cmd.params)
    return f"VKAPI_ATTR {cmd.return_type.cpp()} VKAPI_CALL Wrap_{cmd.name}({args})"


def _first_input_dispatchable(cmd: Command) -> Param | None:
    """Find the first input dispatchable handle param.

    Used to find the ``Connection`` for each wire call. ``vkCreateInstance``
    has none and is special-cased upstream.
    """
    for p in cmd.params:
        if p.type.base in DISPATCHABLE_HANDLES and p.type.pointer_depth == 0:
            return p
    return None


# ---- Per-shape body builders ----------------------------------------------


def _emit_create_instance(cmd: Command) -> list[str]:
    """vkCreateInstance: open a fresh Connection, wrap the returned handle.

    Releases the connection if the wire call fails. The wrapping pattern
    inherits via the same connection pointer for every child handle.
    """
    out_inst = next(
        (p for p in cmd.params
         if p.type.base == "VkInstance" and p.type.pointer_depth == 1),
        None,
    )
    assert out_inst is not None, "vkCreateInstance must have VkInstance* out param"

    forwarded: list[str] = []
    for p in cmd.params:
        if p.name == out_inst.name:
            forwarded.append("&server_instance")
        else:
            forwarded.append(p.name)

    return [
        "    auto* conn = ::av::icd::acquire_default_connection();",
        "    if (conn == nullptr) {",
        "        return VK_ERROR_INITIALIZATION_FAILED;",
        "    }",
        "    // 64 bits wide because that is the wire's width. Holding the",
        "    // server's handle in a VkInstance would narrow it on a 32-bit",
        "    // client and send back a handle no object is at.",
        "    uint64_t server_instance = 0;",
        f"    const VkResult rvk_rc = ::av::vkproto::rvk_{cmd.name}("
        + "conn->transport(), "
        + ", ".join(forwarded)
        + ");",
        "    if (rvk_rc != VK_SUCCESS) {",
        "        ::av::icd::release_connection(conn);",
        "        return rvk_rc;",
        "    }",
        f"    *{out_inst.name} = ::av::icd::wrap_server_handle<{out_inst.type.base}>("
        f"server_instance, conn);",
        "    return rvk_rc;",
    ]


def _emit_destroy_instance(cmd: Command) -> list[str]:
    """vkDestroyInstance: extract Connection from instance, do call, free both."""
    inst_param = next(
        (p for p in cmd.params
         if p.type.base == "VkInstance" and p.type.pointer_depth == 0),
        None,
    )
    assert inst_param is not None, "vkDestroyInstance must take VkInstance"

    forwarded: list[str] = []
    for p in cmd.params:
        # Pass the wrapped handle through; serialize_<H> in the wire
        # layer routes through ``handle_to_wire`` which the ICD overrides
        # to do the unwrap. Avoiding it here prevents double-unwrap.
        forwarded.append(p.name)

    return [
        f"    if ({inst_param.name} == VK_NULL_HANDLE) {{",
        "        return;",
        "    }",
        f"    auto* conn = ::av::icd::connection_of({inst_param.name});",
        f"    ::av::vkproto::rvk_{cmd.name}(conn->transport(), "
        + ", ".join(forwarded) + ");",
        f"    ::av::icd::destroy_wrapper({inst_param.name});",
        "    ::av::icd::release_connection(conn);",
    ]


def _staging_array(cmd: Command, name: str, count_expr: str) -> list[str]:
    """Where an array of raw server handles is staged on the way back up.

    A fixed block on the stack, not a container: there is no standard
    library in this repository's freestanding subset, the count is one the
    application chose, and a request for more handles than the block holds
    is a Vulkan error rather than an allocation sized by the caller.
    """
    if cmd.return_type.base == "void":
        raise ValueError(
            f"command {cmd.name} returns void but has a dispatchable handle "
            "array output; it has no way to refuse an oversized request"
        )
    return [
        f"    if (static_cast<size_t>({count_expr}) > kMaxStagedHandles) {{",
        "        return VK_ERROR_OUT_OF_HOST_MEMORY;",
        "    }",
        f"    uint64_t {name}[kMaxStagedHandles] = {{}};",
    ]


def _handle_array_member(reg: Registry, struct_name: str) -> StructMember | None:
    """The fixed array of dispatchable handles a structure holds, with its
    length member (``VkPhysicalDeviceGroupProperties::physicalDevices``)."""
    struct = reg.structs.get(struct_name)
    if struct is None:
        return None
    return next(
        (m for m in struct.members
         if m.type.base in DISPATCHABLE_HANDLES and m.type.fixed_dim is not None
         and m.type.pointer_depth == 0 and m.len_expr is not None),
        None,
    )


def _wrap_member_handles(array: str, count_expr: str, member: StructMember) -> list[str]:
    """Wrap the host handles the decoder left in each element's handle array.

    The length member bounds the loop because the decoder already refused a
    length past the array (serialize.py's check on a len-bounded fixed array).
    A handle that cannot be wrapped leaves the application nothing it could
    dispatch through, so the call answers out of memory rather than a null.
    """
    return [
        f"    for (uint32_t k = 0; k < {count_expr}; ++k) {{",
        f"        for (uint32_t j = 0; j < {array}[k].{member.len_expr}; ++j) {{",
        f"            {array}[k].{member.name}[j] = ::av::icd::wrap_server_handle<{member.type.base}>("
        f"reinterpret_cast<uint64_t>({array}[k].{member.name}[j]), conn);",
        f"            if ({array}[k].{member.name}[j] == VK_NULL_HANDLE) {{",
        "                return VK_ERROR_OUT_OF_HOST_MEMORY;",
        "            }",
        "        }",
        "    }",
    ]


def _emit_default_body(cmd: Command, reg: Registry) -> list[str]:
    """Generic wrapper: unwrap input handles, call, wrap output handles."""
    is_void = cmd.return_type.base == "void"
    pre_call: list[str] = []
    post_call: list[str] = []
    call_args: list[str] = []
    has_handle_array_out = False

    parent = _first_input_dispatchable(cmd)
    if parent is None:
        # No input dispatchable handle and not vkCreateInstance — should not
        # happen in our profile, but assert loudly if a future command lands
        # here. We could fall back to a default connection, but anything in
        # this position is suspicious.
        raise ValueError(
            f"command {cmd.name} has no input dispatchable handle; "
            "either special-case it (like vkCreateInstance) or audit the profile"
        )

    # A recorded command never touches the Connection: its bytes go into
    # the command buffer's own batch, and it has no handle outputs to
    # wrap. Emitting the lookup anyway would be an unused variable in
    # every one of the hundred-odd vkCmd* wrappers.
    if not _is_recordable(cmd, parent):
        pre_call.append(
            f"    auto* conn = ::av::icd::connection_of({parent.name});"
        )

    # Where this command's bytes go. For a recorded command that is the
    # command buffer's own batch, which is a Transport that appends
    # instead of sending; for everything else it is the connection's
    # real transport. The generated body is otherwise identical, which
    # is the point — nothing below here knows batching exists.
    if _is_recordable(cmd, parent):
        transport = f"::av::icd::record_target({parent.name})"
    else:
        transport = "conn->transport()"

    i = 0
    while i < len(cmd.params):
        p = cmd.params[i]

        # count_array_pair: dispatchable handle output is a special case.
        if _is_count_array_pair(cmd.params, i):
            count = cmd.params[i]
            arr = cmd.params[i + 1]
            if arr.type.base in DISPATCHABLE_HANDLES:
                vec_name = f"server_{arr.name}"
                # Query-only mode forwards directly. Dispatchable handle
                # args pass through unwrapped — ``serialize_<H>`` does the
                # unwrap via ``handle_to_wire`` so double-unwrap doesn't
                # happen.
                pre_call.append(f"    if ({arr.name} == nullptr) {{")
                forward_args: list[str] = []
                for q in cmd.params:
                    if q.name == arr.name:
                        forward_args.append("nullptr")
                    else:
                        forward_args.append(q.name)
                pre_call.append(
                    f"        return ::av::vkproto::rvk_{cmd.name}("
                    + transport + ", " + ", ".join(forward_args) + ");"
                )
                pre_call.append("    }")
                pre_call.extend(_staging_array(cmd, vec_name, f"*{count.name}"))
                call_args.append(count.name)
                call_args.append(vec_name)
                post_call.append(
                    f"    for (uint32_t k = 0; k < *{count.name}; ++k) {{"
                )
                post_call.append(
                    f"        {arr.name}[k] = "
                    f"::av::icd::wrap_server_handle<{arr.type.base}>({vec_name}[k], conn);"
                )
                post_call.append("    }")
                has_handle_array_out = True
            elif _handle_array_member(reg, arr.type.base) is not None:
                # An array of structures each holding dispatchable handles
                # (vkEnumeratePhysicalDeviceGroups): the decoder writes the
                # host's handles into the application's structures, and each
                # is replaced by the ICD's wrapper, as the plain array is.
                member = _handle_array_member(reg, arr.type.base)
                pre_call.append(f"    if ({arr.name} == nullptr) {{")
                pre_call.append(
                    f"        return ::av::vkproto::rvk_{cmd.name}("
                    + transport + ", "
                    + ", ".join("nullptr" if q.name == arr.name else q.name for q in cmd.params)
                    + ");"
                )
                pre_call.append("    }")
                call_args.append(count.name)
                call_args.append(arr.name)
                post_call.extend(_wrap_member_handles(arr.name, f"*{count.name}", member))
                has_handle_array_out = True
            else:
                # Plain count_array_pair, no special handling.
                call_args.append(count.name)
                call_args.append(arr.name)
            i += 2
            continue

        if p.type.base in DISPATCHABLE_HANDLES and p.type.pointer_depth == 0:
            # Pass the wrapped handle through; serialize_<H> on the wire
            # layer unwraps via ``handle_to_wire`` (the ICD's strong
            # override). Avoids double-unwrap.
            call_args.append(p.name)
        elif (
            p.type.base in DISPATCHABLE_HANDLES
            and p.type.pointer_depth == 1
            and p.direction == "out"
            and p.len_expr is None
        ):
            # SINGLE dispatchable handle output (e.g. vkCreateDevice's pDevice).
            local = f"server_{p.name}"
            pre_call.append(f"    uint64_t {local} = 0;")
            call_args.append(f"&{local}")
            post_call.append(
                f"    *{p.name} = ::av::icd::wrap_server_handle<{p.type.base}>({local}, conn);"
            )
        elif (
            p.type.base in DISPATCHABLE_HANDLES
            and p.type.pointer_depth == 1
            and p.direction == "out"
            and p.len_expr is not None
        ):
            # Dispatchable handle ARRAY output where the length lives in
            # a sibling struct member (e.g. vkAllocateCommandBuffers's
            # pCommandBuffers, len="pAllocateInfo->commandBufferCount";
            # vkAllocateDescriptorSets's pDescriptorSets too). The wire
            # layer fills a staging array with raw server handles, then we
            # wrap each into the caller's array so subsequent unwrap calls
            # find a wrapper and not a raw value.
            vec = f"server_{p.name}"
            # The count expression references the caller's actual struct
            # pointer, which is in scope here.
            count_expr = p.len_expr
            pre_call.extend(_staging_array(cmd, vec, count_expr))
            call_args.append(vec)
            post_call.append(
                f"    for (uint32_t k = 0; k < static_cast<uint32_t>({count_expr}); ++k) {{"
            )
            post_call.append(
                f"        {p.name}[k] = ::av::icd::wrap_server_handle<{p.type.base}>({vec}[k], conn);"
            )
            post_call.append("    }")
            has_handle_array_out = True
        else:
            call_args.append(p.name)
        i += 1

    body: list[str] = []
    body.extend(pre_call)
    call_str = (
        f"::av::vkproto::rvk_{cmd.name}({transport}, "
        + ", ".join(call_args) + ")"
    )
    if is_void:
        body.append(f"    {call_str};")
        body.extend(post_call)
    elif post_call or has_handle_array_out:
        body.append(f"    const {cmd.return_type.cpp()} rvk_rc = {call_str};")
        body.append("    if (rvk_rc != VK_SUCCESS && rvk_rc != VK_INCOMPLETE) {")
        body.append("        return rvk_rc;")
        body.append("    }")
        body.extend(post_call)
        body.append("    return rvk_rc;")
    else:
        body.append(f"    return {call_str};")
    return body


def _emit_wrapper_body(cmd: Command, reg: Registry) -> list[str]:
    if cmd.name == "vkCreateInstance":
        return _emit_create_instance(cmd)
    if cmd.name == "vkDestroyInstance":
        return _emit_destroy_instance(cmd)
    return _emit_default_body(cmd, reg)


# ---- Output ---------------------------------------------------------------


def emit(reg: Registry, out_dir: Path) -> None:
    cmds = sorted(reg.commands.values(), key=lambda c: c.cmd_id)

    # ---- Header ----
    h = [
        GENERATED_BANNER,
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "#include <vulkan/vulkan_core.h>",
        "",
        "namespace av::vkicd::generated {",
        "",
        "// Wrapper functions matching the application-facing Vulkan signatures.",
        "// Each one looks up the per-VkInstance Connection (or opens a fresh",
        "// one in vkCreateInstance), unwraps dispatchable handles, calls the",
        "// generated rvk_vk* dispatch over that Connection's transport, and",
        "// wraps any returned dispatchable handles back into the wrappers",
        "// the handle registry hands out, carrying the same Connection.",
        "",
    ]
    for cmd in cmds:
        h.append(_signature(cmd) + ";")
    h.append("")
    h.append("// The object the loader dispatches a command on, from its first")
    h.append("// parameter in vk.xml: a global command takes no dispatchable handle,")
    h.append("// and a queue or command buffer dispatches as its device.")
    h.append("enum class DispatchLevel : uint8_t { global, instance, physical_device, device };")
    h.append("")
    h.append("// Name -> function-pointer entries for vk_icdGetInstanceProcAddr.")
    h.append("struct Entry {")
    h.append("    const char* name;")
    h.append("    PFN_vkVoidFunction fn;")
    h.append("    DispatchLevel level;")
    h.append("};")
    h.append("")
    h.append("// The generated entries and how many there are. Not a std::span:")
    h.append("// this repository compiles freestanding, with no standard library,")
    h.append("// so the table carries its own length.")
    h.append("class EntryTable {")
    h.append("public:")
    h.append("    EntryTable(const Entry* first, size_t count) noexcept;")
    h.append("")
    h.append("    const Entry* begin() const noexcept;")
    h.append("    const Entry* end() const noexcept;")
    h.append("    size_t size() const noexcept;")
    h.append("")
    h.append("private:")
    h.append("    const Entry* first_;")
    h.append("    size_t count_;")
    h.append("};")
    h.append("")
    h.append("// The generated wrapper entries. The ICD layer prepends its own")
    h.append("// non-wire entries (loader negotiation, GetInstanceProcAddr, etc.).")
    h.append("EntryTable wrapper_entries() noexcept;")
    h.append("")
    h.append("}  // namespace av::vkicd::generated")
    h.append("")
    write(out_dir / "vk_icd_entries.h", header_guard("\n".join(h[1:])))

    # ---- Source ----
    s = [
        GENERATED_BANNER,
        "",
        '#include "vk_icd_entries.h"',
        "",
        '#include "core/trace/function_scope.hpp"',
        '#include "proto/icd_connection.hpp"',
        '#include "proto/icd_handle_registry.hpp"',
        '#include "vk_dispatch_client.h"',
        "",
        "namespace av::vkicd::generated {",
        "namespace {",
        "",
        "// How many raw server handles a wrapper stages on its stack while",
        "// wrapping an array the application asked for. A request for more is",
        "// refused with VK_ERROR_OUT_OF_HOST_MEMORY rather than allocated.",
        "constexpr size_t kMaxStagedHandles = 1024;",
        "",
        "}  // namespace",
        "",
    ]
    for cmd in cmds:
        s.append(_signature(cmd) + " {")
        # One trace scope per API call, named as the application called it
        # (docs/trace.md).
        s.append(f'    AV_SCOPE("{cmd.name}");')
        s.extend(_emit_wrapper_body(cmd, reg))
        s.append("}")
        s.append("")

    # Note: not ``constexpr`` -- reinterpret_cast to function pointer isn't a
    # constant expression. Plain runtime-initialised globals are fine here.
    s.append("namespace {")
    s.append(f"const Entry kWrapperTable[{len(cmds)}] = {{")
    for cmd in cmds:
        s.append(
            f'    Entry{{ "{cmd.name}", '
            f"reinterpret_cast<PFN_vkVoidFunction>(&Wrap_{cmd.name}), "
            f"DispatchLevel::{dispatch_level(cmd)} }},"
        )
    s.append("};")
    s.append("}  // namespace")
    s.append("")
    s.append(
        "EntryTable::EntryTable(const Entry* first, size_t count) noexcept"
    )
    s.append("    : first_(first), count_(count) {}")
    s.append("")
    s.append("const Entry* EntryTable::begin() const noexcept { return first_; }")
    s.append(
        "const Entry* EntryTable::end() const noexcept { return first_ + count_; }"
    )
    s.append("size_t EntryTable::size() const noexcept { return count_; }")
    s.append("")
    s.append(
        f"EntryTable wrapper_entries() noexcept {{ "
        f"return EntryTable{{kWrapperTable, {len(cmds)}u}}; }}"
    )
    s.append("")
    s.append("}  // namespace av::vkicd::generated")
    s.append("")
    write(out_dir / "vk_icd_entries.cpp", "\n".join(s[1:]))
