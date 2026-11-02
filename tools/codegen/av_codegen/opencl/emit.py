from __future__ import annotations

import re
from pathlib import Path

from ..emit.common import write
from .model import Command, Param, Registry, Role

HEADER = ("// THIS FILE IS GENERATED. DO NOT EDIT.\n"
          "// Regenerate with: python -m av_codegen.opencl --xml cl.xml --out ...\n")

HANDLE_T = "uint64_t"

CL_PREAMBLE = [
    "#ifndef CL_TARGET_OPENCL_VERSION",
    "#define CL_TARGET_OPENCL_VERSION 300",
    "#endif",
    "#define CL_USE_DEPRECATED_OPENCL_1_0_APIS",
    "#define CL_USE_DEPRECATED_OPENCL_1_1_APIS",
    "#define CL_USE_DEPRECATED_OPENCL_1_2_APIS",
    "#define CL_USE_DEPRECATED_OPENCL_2_0_APIS",
    "#define CL_USE_DEPRECATED_OPENCL_2_1_APIS",
    "#define CL_USE_DEPRECATED_OPENCL_2_2_APIS",
    "",
    "#include <CL/cl.h>",
    "#include <CL/cl_gl.h>",
]



def _is_handle(reg: Registry, base: str) -> bool:
    return base in reg.handles


def _kind(base: str) -> str:
    name = base[len("cl_"):]
    return name[:-len("_id")] if name.endswith("_id") else name


def _kinds(reg: Registry) -> list[str]:
    used = {p.element or p.type.base for cmd in reg.commands.values() for p in cmd.params
            if p.role in (Role.HANDLE, Role.HANDLE_ARRAY, Role.HANDLE_OUT,
                          Role.HANDLE_ARRAY_OUT)}
    used |= {cmd.return_type.base for cmd in reg.commands.values() if cmd.returns_handle}
    return sorted(used)


STRUCT_HANDLES = {
    "cl_image_desc": (("buffer", "mem"),),
}

CONTEXT_PROPERTY_SCALARS = ("CL_CONTEXT_INTEROP_USER_SYNC",)

MEM_PROPERTY_REFUSED = (("0x2051", "CL_MEM_DEVICE_HANDLE_LIST_KHR"),)

CALLBACK_COMMANDS = ("clSetEventCallback",)

EXEC_INFO_POINTERS = (("0x40B8", "CL_KERNEL_EXEC_INFO_SVM_PTRS_ARM"),
                      ("0x4203", "CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL"))

TRANSFER_PITCHES = {
    "clEnqueueReadImage": ("image", "region", "row_pitch", "slice_pitch"),
    "clEnqueueWriteImage": ("image", "region", "input_row_pitch", "input_slice_pitch"),
    "clEnqueueReadBufferRect": ("host_origin", "region", "host_row_pitch", "host_slice_pitch"),
    "clEnqueueWriteBufferRect": ("host_origin", "region", "host_row_pitch", "host_slice_pitch"),
}

FILL_COLOR_BYTES = 16

IMAGE_HOST_EXTENTS = {
    "clCreateImage": ("image_desc->image_type", "image_desc->image_width",
                      "image_desc->image_height", "image_desc->image_depth",
                      "image_desc->image_array_size", "image_desc->image_row_pitch",
                      "image_desc->image_slice_pitch"),
    "clCreateImageWithProperties": ("image_desc->image_type", "image_desc->image_width",
                                    "image_desc->image_height", "image_desc->image_depth",
                                    "image_desc->image_array_size",
                                    "image_desc->image_row_pitch",
                                    "image_desc->image_slice_pitch"),
    "clCreateImage2D": ("CL_MEM_OBJECT_IMAGE2D", "image_width", "image_height", "1", "1",
                        "image_row_pitch", "0"),
    "clCreateImage3D": ("CL_MEM_OBJECT_IMAGE3D", "image_width", "image_height", "image_depth",
                        "1", "image_row_pitch", "image_slice_pitch"),
}

KERNEL_ARG_INFO_BUILDS = ("clBuildProgram", "clCompileProgram")


def _decode_property_keys(p: Param, out: list[str]) -> None:
    if p.type.base == "cl_context_properties":
        allowed = " && ".join(f"wire_items[i] != {key}" for key in CONTEXT_PROPERTY_SCALARS)
        out.append("            for (uint64_t i = 0; wire_items[i] != 0; i += 2) {")
        out.append("                if (wire_items[i] == CL_CONTEXT_PLATFORM) {")
        out.append(f"                    const {HANDLE_T} wire_handle = static_cast<{HANDLE_T}>(wire_items[i + 1]);")
        out.append("                    void* wire_platform = s.handle_for(HandleKind::platform, wire_handle);")
        out.append("                    if (wire_platform == nullptr) return false;")
        out.append("                    wire_items[i + 1] = static_cast<cl_context_properties>("
                   "reinterpret_cast<intptr_t>(wire_platform));")
        out.append(f"                }} else if ({allowed}) {{")
        out.append("                    return false;")
        out.append("                }")
        out.append("            }")
    elif p.type.base == "cl_mem_properties":
        out.append("            for (uint64_t i = 0; wire_items[i] != 0; i += 2) {")
        for value, name in MEM_PROPERTY_REFUSED:
            out.append(f"                if (wire_items[i] == {value}) return false;  // {name}")
        out.append("            }")


def _count_of(cmd: Command, len_param: str | None) -> str | None:
    if len_param is None or len_param.startswith("@"):
        return None
    if len_param.isdigit():
        return f"{len_param}u"
    names = {q.name for q in cmd.params if q.role == Role.SCALAR}
    if len_param not in names:
        raise ValueError(f"{cmd.name}: {len_param} counts an array but is not a scalar")
    return f"static_cast<uint64_t>({len_param})"


def _refusals(cmd: Command) -> list[str]:
    out: list[str] = []
    for p in cmd.params:
        n = p.name
        count = _count_of(cmd, p.len_param)
        if p.role in (Role.HANDLE_ARRAY, Role.SCALAR_ARRAY, Role.STRING_ARRAY, Role.BYTES_IN) \
                and count is not None:
            out.append(f"    if ({n} != nullptr && {n}_count < {count}) return false;")
        elif p.role == Role.BYTES_OUT and count is not None:
            out.append(f"    if ({n}_wanted && {count} > static_cast<uint64_t>({n}_capacity)) return false;")
        elif p.role == Role.BYTES_OUT and p.len_param == "@image_format_bytes":
            out.append(f"    if ({n}_wanted && static_cast<uint64_t>(num_entries) * sizeof(cl_image_format) >"
                       f" static_cast<uint64_t>({n}_capacity)) return false;")
        elif p.role in (Role.BYTES_IN, Role.BYTES_OUT) and cmd.name in TRANSFER_PITCHES:
            first, region, row, slice_ = TRANSFER_PITCHES[cmd.name]
            helper = "image_transfer_bytes(s, arena, " if first == "image" else "rect_bytes("
            present = f"{n} != nullptr" if p.role == Role.BYTES_IN else f"{n}_wanted"
            room = f"{n}_count" if p.role == Role.BYTES_IN else f"static_cast<uint64_t>({n}_capacity)"
            out.append("    {")
            out.append("        uint64_t wire_needed = 0;")
            out.append(f"        if ({present} && (!{helper}{first}, {region}, {row}, {slice_}, wire_needed) ||")
            out.append(f"                         {room} < wire_needed)) return false;")
            out.append("    }")
        elif p.role == Role.BYTES_IN and p.len_param == "@fill_bytes":
            out.append(f"    if ({n} != nullptr && {n}_count < {FILL_COLOR_BYTES}u) return false;")
        elif p.role == Role.BYTES_IN and cmd.name in IMAGE_HOST_EXTENTS:
            kind, width, height, depth, array, row, slice_ = IMAGE_HOST_EXTENTS[cmd.name]
            described = "image_desc == nullptr || " if kind.startswith("image_desc") else ""
            out.append(f"    if ({n} != nullptr) {{")
            out.append("        uint64_t wire_needed = 0;")
            out.append(f"        if ({described}!image_host_bytes(image_format, {kind}, {width}, {height},")
            out.append(f"                                 {depth}, {array}, {row}, {slice_}, wire_needed) ||")
            out.append(f"            {n}_count < wire_needed) return false;")
            out.append("    }")
        elif p.role == Role.BYTES_IN:
            raise ValueError(f"{cmd.name}.{n}: bytes in with no size the server can check")
    if cmd.name == "clSetKernelArg":
        out.append("    bool accepted = false;")
        out.append("    if (!resolve_kernel_argument(s, arena, kernel, arg_index, arg_size, arg_value, accepted)) "
                   "return false;")
        out.append("    if (!accepted) {")
        out.append("        reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(CL_INVALID_ARG_VALUE)));")
        out.append("        return true;")
        out.append("    }")
    if cmd.name in KERNEL_ARG_INFO_BUILDS:
        out.append("    options = with_kernel_arg_info(arena, options);")
        out.append("    if (options == nullptr) return false;")
    if any(q.role == Role.BYTES_IN for q in cmd.params) and \
            any(q.name == "flags" and q.type.base == "cl_mem_flags" for q in cmd.params):
        out.append("    if ((flags & CL_MEM_USE_HOST_PTR) != 0) return false;")
    if cmd.name == "clSetKernelExecInfo":
        for value, name in EXEC_INFO_POINTERS:
            out.append(f"    if (param_name == {value}) return false;  // {name}")
        out.append("    if (param_name == CL_KERNEL_EXEC_INFO_SVM_PTRS &&")
        out.append("        !resolve_svm_pointers(s, arena, param_value_size, param_value)) return false;")
    return out


SIZE_HELPERS = [
    "// How far into a host pointer a transfer reaches: `offset`, then whole",
    "// rows and slices at the pitches given, and the last row only as far as",
    "// the region does. 0 for an empty region, which the ICD refuses without",
    "// touching anything; false for a size that overflows.",
    "static bool pitched_reach(const size_t* region, uint64_t element, uint64_t row,",
    "                          uint64_t slice, uint64_t offset, uint64_t& bytes) {",
    "    bytes = 0;",
    "    if (region[0] == 0 || region[1] == 0 || region[2] == 0) return true;",
    "    uint64_t rows = 0;",
    "    uint64_t slices = 0;",
    "    uint64_t last_row = 0;",
    "    return !__builtin_mul_overflow(row, static_cast<uint64_t>(region[1] - 1), &rows) &&",
    "           !__builtin_mul_overflow(slice, static_cast<uint64_t>(region[2] - 1), &slices) &&",
    "           !__builtin_mul_overflow(static_cast<uint64_t>(region[0]), element, &last_row) &&",
    "           !__builtin_add_overflow(offset, rows, &bytes) &&",
    "           !__builtin_add_overflow(bytes, slices, &bytes) &&",
    "           !__builtin_add_overflow(bytes, last_row, &bytes);",
    "}",
    "",
    "// A buffer rectangle: bytes, from the host origin, at the pitches the",
    "// specification defaults a 0 to.",
    "static bool rect_bytes(const size_t* origin, const size_t* region, size_t row_pitch,",
    "                       size_t slice_pitch, uint64_t& bytes) {",
    "    if (origin == nullptr || region == nullptr) return false;",
    "    const uint64_t row = row_pitch != 0 ? row_pitch : region[0];",
    "    uint64_t slice = slice_pitch;",
    "    if (slice == 0 && __builtin_mul_overflow(static_cast<uint64_t>(region[0]),",
    "                                             static_cast<uint64_t>(region[1]), &slice)) return false;",
    "    uint64_t across = 0;",
    "    uint64_t down = 0;",
    "    uint64_t offset = 0;",
    "    if (__builtin_mul_overflow(static_cast<uint64_t>(origin[1]), row, &across) ||",
    "        __builtin_mul_overflow(static_cast<uint64_t>(origin[2]), slice, &down) ||",
    "        __builtin_add_overflow(across, down, &offset) ||",
    "        __builtin_add_overflow(offset, static_cast<uint64_t>(origin[0]), &offset)) return false;",
    "    return pitched_reach(region, 1, row, slice, offset, bytes);",
    "}",
    "",
    "// An image transfer: elements of the image's own format, which the ICD",
    "// is asked for rather than the guest - into the arena, like every other",
    "// answer the ICD writes.",
    "static bool image_transfer_bytes(ServerDispatch& s, ::av::WireArena& arena, cl_mem image,",
    "                                 const size_t* region, size_t row_pitch, size_t slice_pitch,",
    "                                 uint64_t& bytes) {",
    "    if (region == nullptr) return false;",
    "    auto* element = arena.allocate<size_t>(1);",
    "    if (element == nullptr ||",
    "        s.on_clGetImageInfo(image, CL_IMAGE_ELEMENT_SIZE, sizeof *element, element, nullptr) != CL_SUCCESS ||",
    "        *element == 0) return false;",
    "    uint64_t row = row_pitch;",
    "    if (row == 0 && __builtin_mul_overflow(static_cast<uint64_t>(region[0]),",
    "                                           static_cast<uint64_t>(*element), &row)) return false;",
    "    uint64_t slice = slice_pitch;",
    "    if (slice == 0 && __builtin_mul_overflow(row, static_cast<uint64_t>(region[1]), &slice)) return false;",
    "    return pitched_reach(region, *element, row, slice, 0, bytes);",
    "}",
    "",
    "// The bytes one element of a format occupies, from the channel order and",
    "// data type the way the specification's table does, or 0 for a pairing",
    "// this server does not size. Where implementations disagree - a padded",
    "// order, a 24-bit depth - the larger answer is the one taken: the bound",
    "// is on what an ICD may read, so erring high refuses and erring low reads",
    "// past the block.",
    "static uint64_t format_element_bytes(const cl_image_format* format) {",
    "    if (format == nullptr) return 0;",
    "    const cl_channel_order order = format->image_channel_order;",
    "    const cl_channel_type type = format->image_channel_data_type;",
    "    if (type == CL_UNORM_SHORT_565 || type == CL_UNORM_SHORT_555)",
    "        return order == CL_RGB || order == CL_RGBx ? 2 : 0;",
    "    if (type == CL_UNORM_INT_101010)",
    "        return order == CL_RGB || order == CL_RGBx ? 4 : 0;",
    "    if (type == CL_UNORM_INT_101010_2) return order == CL_RGBA ? 4 : 0;",
    "    if (order == CL_DEPTH_STENCIL)",
    "        return type == CL_UNORM_INT24 ? 4 : type == CL_FLOAT ? 8 : 0;",
    "    uint64_t channel = 0;",
    "    switch (type) {",
    "        case CL_SNORM_INT8: case CL_UNORM_INT8: case CL_SIGNED_INT8: case CL_UNSIGNED_INT8:",
    "            channel = 1; break;",
    "        case CL_SNORM_INT16: case CL_UNORM_INT16: case CL_SIGNED_INT16: case CL_UNSIGNED_INT16:",
    "        case CL_HALF_FLOAT:",
    "            channel = 2; break;",
    "        case CL_SIGNED_INT32: case CL_UNSIGNED_INT32: case CL_FLOAT: case CL_UNORM_INT24:",
    "            channel = 4; break;",
    "        default: return 0;",
    "    }",
    "    switch (order) {",
    "        case CL_R: case CL_A: case CL_INTENSITY: case CL_LUMINANCE: case CL_DEPTH:",
    "            return channel;",
    "        case CL_RG: case CL_RA: case CL_Rx:",
    "            return 2 * channel;",
    "        case CL_RGB: case CL_RGx: case CL_sRGB:",
    "            return 3 * channel;",
    "        case CL_RGBA: case CL_BGRA: case CL_ARGB: case CL_ABGR: case CL_RGBx: case CL_sRGBx:",
    "        case CL_sRGBA: case CL_sBGRA:",
    "            return 4 * channel;",
    "        default: return 0;",
    "    }",
    "}",
    "",
    "// The bytes an image made from a host pointer copies out of it: whole",
    "// rows and slices at the pitches given, or at the tight ones where the",
    "// pitch given is smaller, for every slice or array element. Whole blocks",
    "// rather than the last row cut short, because an ICD may copy either way.",
    "static bool image_host_bytes(const cl_image_format* format, cl_mem_object_type type,",
    "                             size_t width, size_t height, size_t depth, size_t array_size,",
    "                             size_t row_pitch, size_t slice_pitch, uint64_t& bytes) {",
    "    const uint64_t element = format_element_bytes(format);",
    "    if (element == 0) return false;",
    "    uint64_t row = 0;",
    "    if (__builtin_mul_overflow(static_cast<uint64_t>(width), element, &row)) return false;",
    "    if (row < row_pitch) row = row_pitch;",
    "    uint64_t rows = height;",
    "    uint64_t slices = 1;",
    "    switch (type) {",
    "        case CL_MEM_OBJECT_IMAGE1D: case CL_MEM_OBJECT_IMAGE1D_BUFFER: rows = 1; break;",
    "        case CL_MEM_OBJECT_IMAGE1D_ARRAY: rows = 1; slices = array_size; break;",
    "        case CL_MEM_OBJECT_IMAGE2D: break;",
    "        case CL_MEM_OBJECT_IMAGE2D_ARRAY: slices = array_size; break;",
    "        case CL_MEM_OBJECT_IMAGE3D: slices = depth; break;",
    "        default: return false;",
    "    }",
    "    uint64_t slice = 0;",
    "    if (__builtin_mul_overflow(row, rows, &slice)) return false;",
    "    if (slice < slice_pitch) slice = slice_pitch;",
    "    return !__builtin_mul_overflow(slice, slices, &bytes);",
    "}",
    "",
    "// A build with -cl-kernel-arg-info after whatever the guest asked for, in",
    "// the arena; null when there is no room.",
    "static const char* with_kernel_arg_info(::av::WireArena& arena, const char* options) {",
    "    static constexpr char flag[] = \" -cl-kernel-arg-info\";",
    "    const size_t length = options != nullptr ? __builtin_strlen(options) : 0;",
    "    char* joined = arena.allocate_chars(length + sizeof flag);",
    "    if (joined == nullptr) return nullptr;",
    "    if (length != 0) __builtin_memcpy(joined, options, length);",
    "    __builtin_memcpy(joined + length, flag, sizeof flag);",
    "    return joined;",
    "}",
    "",
    "// CL_KERNEL_EXEC_INFO_SVM_PTRS' value: a whole number of addresses, each",
    "// inside an allocation or a mapping this server answered, handed on as",
    "// the pointers they resolve to, in the arena.",
    "static bool resolve_svm_pointers(ServerDispatch& s, ::av::WireArena& arena, size_t size,",
    "                                 const void*& value) {",
    "    if (value == nullptr || size == 0 || size % sizeof(uint64_t) != 0) return false;",
    "    const size_t count = size / sizeof(uint64_t);",
    "    auto* resolved = arena.allocate<void*>(count);",
    "    if (resolved == nullptr) return false;",
    "    for (size_t i = 0; i < count; ++i) {",
    "        uint64_t address = 0;",
    "        __builtin_memcpy(&address, static_cast<const uint8_t*>(value) + i * sizeof address, sizeof address);",
    "        resolved[i] = s.svm_for(address, 1);",
    "        if (resolved[i] == nullptr) return false;",
    "    }",
    "    value = resolved;",
    "    return true;",
    "}",
    "",
    "static bool ends_with(const char* text, size_t length, const char* suffix) {",
    "    const size_t tail = __builtin_strlen(suffix);",
    "    return length >= tail && __builtin_memcmp(text + length - tail, suffix, tail) == 0;",
    "}",
    "",
    "// Asked of the ICD, into the arena like every answer it writes. unknown",
    "// when it cannot say - a program from a binary or IL, or an ICD that keeps",
    "// no argument information.",
    "static KernelArgument classified(cl_kernel_arg_address_qualifier address,",
    "                                 cl_kernel_arg_type_qualifier qualifier, const char* name) {",
    "    const size_t used = __builtin_strlen(name);",
    "    if (address == CL_KERNEL_ARG_ADDRESS_LOCAL) return KernelArgument::local;",
    "    if (address == CL_KERNEL_ARG_ADDRESS_GLOBAL || address == CL_KERNEL_ARG_ADDRESS_CONSTANT ||",
    "        ends_with(name, used, \"*\") || (qualifier & CL_KERNEL_ARG_TYPE_PIPE) != 0)",
    "        return KernelArgument::memory;",
    "    static constexpr const char* images[] = {\"image1d_t\", \"image1d_array_t\", \"image1d_buffer_t\",",
    "                                             \"image2d_t\", \"image2d_array_t\", \"image3d_t\"};",
    "    for (const char* image : images) {",
    "        if (ends_with(name, used, image)) return KernelArgument::image;",
    "    }",
    "    return ends_with(name, used, \"sampler_t\") ? KernelArgument::sampler : KernelArgument::data;",
    "}",
    "",
    "// Asked of the ICD, into the arena like every answer it writes; false only",
    "// when the arena has no room to ask. Local, global and constant space say",
    "// what an argument is alone; a private one needs its type's name, which a",
    "// host need not keep for a program from IL or from a binary. What the host",
    "// cannot say the server may know otherwise (declared_argument), and what",
    "// neither can say is unknown.",
    "static bool kernel_argument(ServerDispatch& s, ::av::WireArena& arena, cl_kernel kernel,",
    "                            cl_uint index, KernelArgument& argument) {",
    "    argument = KernelArgument::unknown;",
    "    auto* address = arena.allocate<cl_kernel_arg_address_qualifier>(1);",
    "    auto* qualifier = arena.allocate<cl_kernel_arg_type_qualifier>(1);",
    "    auto* length = arena.allocate<size_t>(1);",
    "    const bool addressed = s.on_clGetKernelArgInfo(kernel, index, CL_KERNEL_ARG_ADDRESS_QUALIFIER,",
    "                                                   sizeof *address, address, nullptr) == CL_SUCCESS;",
    "    if (addressed && *address == CL_KERNEL_ARG_ADDRESS_LOCAL) {",
    "        argument = KernelArgument::local;",
    "        return true;",
    "    }",
    "    if (addressed && (*address == CL_KERNEL_ARG_ADDRESS_GLOBAL || *address == CL_KERNEL_ARG_ADDRESS_CONSTANT)) {",
    "        argument = KernelArgument::memory;",
    "        return true;",
    "    }",
    "    if (addressed &&",
    "        s.on_clGetKernelArgInfo(kernel, index, CL_KERNEL_ARG_TYPE_QUALIFIER, sizeof *qualifier,",
    "                                qualifier, nullptr) == CL_SUCCESS &&",
    "        s.on_clGetKernelArgInfo(kernel, index, CL_KERNEL_ARG_TYPE_NAME, 0, nullptr,",
    "                                length) == CL_SUCCESS) {",
    "        char* name = arena.allocate_chars(*length + 1);",
    "        if (s.on_clGetKernelArgInfo(kernel, index, CL_KERNEL_ARG_TYPE_NAME, *length, name,",
    "                                    nullptr) == CL_SUCCESS) {",
    "            name[*length] = '\\0';",
    "            argument = classified(*address, *qualifier, name);",
    "            return true;",
    "        }",
    "    }",
    "    if (!s.declared_argument(kernel, index, argument)) argument = KernelArgument::unknown;",
    "    return true;",
    "}",
    "",
    "// A kernel argument the ICD will read as an object must be one this",
    "// server made, of that kind, and is handed over as the object resolved;",
    "// one it cannot classify may not be the width of an object at all. A",
    "// null buffer is the API's own, and a local argument has no value. False",
    "// only when the arena has no room; `accepted` says whether the host may be",
    "// handed the argument, and one that may not is answered CL_INVALID_ARG_VALUE",
    "// - a well-formed command naming an argument this server cannot vouch for.",
    "static bool resolve_kernel_argument(ServerDispatch& s, ::av::WireArena& arena, cl_kernel kernel,",
    "                                    cl_uint index, size_t size, const void*& value, bool& accepted) {",
    "    KernelArgument argument = KernelArgument::unknown;",
    "    accepted = false;",
    "    if (!kernel_argument(s, arena, kernel, index, argument)) return false;",
    "    if (argument == KernelArgument::data) accepted = true;",
    "    if (argument == KernelArgument::local) accepted = value == nullptr;",
    "    if (argument == KernelArgument::unknown) accepted = value == nullptr || size != sizeof(cl_mem);",
    "    if (argument == KernelArgument::data || argument == KernelArgument::local ||",
    "        argument == KernelArgument::unknown) return true;",
    "    if (value == nullptr || size != sizeof(void*)) {",
    "        accepted = value == nullptr && argument == KernelArgument::memory;",
    "        return true;",
    "    }",
    "    uint64_t wire_handle = 0;",
    "    __builtin_memcpy(&wire_handle, value, sizeof wire_handle);",
    "    if (wire_handle == 0) {",
    "        accepted = argument == KernelArgument::memory;",
    "        return true;",
    "    }",
    "    void* object = s.handle_for(argument == KernelArgument::sampler ? HandleKind::sampler : HandleKind::mem,",
    "                                wire_handle);",
    "    auto* resolved = arena.allocate<void*>(1);",
    "    if (object == nullptr) return true;",
    "    *resolved = object;",
    "    value = resolved;",
    "    accepted = true;",
    "    return true;",
    "}",
    "",
]


def _resolve_addresses(cmd: Command) -> list[str]:
    out: list[str] = []
    for p in cmd.params:
        if p.role != Role.SVM_POINTER:
            continue
        extent = f"static_cast<uint64_t>({p.len_param})" if p.len_param else "0u"
        out.append(f"    if ({p.name}_wire != 0) {{")
        out.append(f"        {p.name} = static_cast<{p.type.cpp()}>("
                   f"s.svm_for({p.name}_wire, {extent}));")
        out.append(f"        if ({p.name} == nullptr) return false;")
        out.append("    }")
    return out


def _succeeded(cmd: Command) -> str:
    if cmd.returns_handle or cmd.returns_address:
        return "ret != nullptr"
    if cmd.return_type.base == "cl_int" and cmd.return_type.pointer_depth == 0:
        return "ret == CL_SUCCESS"
    return "true"


def _lifecycle(cmd: Command) -> tuple[str, Param] | None:
    handles = [q for q in cmd.params if q.role == Role.HANDLE]
    if len(handles) != 1 or cmd.return_type.base != "cl_int":
        return None
    if cmd.name.startswith("clRetain"):
        return "retained", handles[0]
    if cmd.name.startswith("clRelease"):
        return "released", handles[0]
    return None


def _client_param_decl(reg: Registry, p: Param) -> list[str]:
    t = p.type
    if p.role == Role.IGNORED:
        return [f"{t.cpp()} {p.name}"]
    if p.role in (Role.HANDLE, Role.SVM_POINTER):
        return [f"{HANDLE_T} {p.name}"]
    if p.role in (Role.HANDLE_ARRAY,):
        return [f"const {HANDLE_T}* {p.name}"]
    if p.role in (Role.HANDLE_OUT,):
        return [f"{HANDLE_T}* {p.name}"]
    if p.role == Role.HANDLE_ARRAY_OUT:
        return [f"{HANDLE_T}* {p.name}"]
    if p.role in (Role.PROPERTIES, Role.PROPERTY_LIST):
        return [f"const {HANDLE_T}* {p.name}"]
    if p.role in (Role.STRUCT_IN,):
        return [f"const {p.element}* {p.name}"]
    if p.role in (Role.STRUCT_OUT,):
        return [f"{p.element}* {p.name}"]
    decl = [f"{t.cpp()} {p.name}"]
    if p.role in (Role.BYTES_IN, Role.BYTES_OUT) and \
            (p.len_param or "").startswith("@"):
        decl.append(f"size_t {p.name}_bytes")
    return decl


def _server_param_decl(reg: Registry, p: Param) -> str:
    t = p.type
    if p.role == Role.HANDLE:
        return f"{t.base} {p.name}"
    if p.role == Role.HANDLE_ARRAY:
        return f"const {p.element}* {p.name}"
    if p.role == Role.HANDLE_OUT:
        return f"{p.element}* {p.name}"
    if p.role == Role.HANDLE_ARRAY_OUT:
        return f"{p.element}* {p.name}"
    if p.role in (Role.PROPERTIES, Role.PROPERTY_LIST):
        return f"const {t.base}* {p.name}"
    if p.role == Role.STRUCT_IN:
        return f"const {p.element}* {p.name}"
    if p.role == Role.STRUCT_OUT:
        return f"{p.element}* {p.name}"
    return f"{t.cpp()} {p.name}"


def _client_signature(reg: Registry, cmd: Command) -> str:
    ret = HANDLE_T if (cmd.returns_handle or cmd.returns_address) else \
        ("void" if cmd.returns_void else cmd.return_type.cpp())
    params = ["::av::ClientTransport& t"]
    for p in cmd.params:
        if p.role == Role.IGNORED:
            if _lengths_owner(cmd, p) is not None:
                params.append(f"const size_t* {p.name}")
            continue
        params.extend(_client_param_decl(reg, p))
    return f"{ret} av_{cmd.name}({', '.join(params)})"


def _server_signature(reg: Registry, cmd: Command) -> str:
    ret = cmd.return_type.cpp()
    params = [_server_param_decl(reg, p) for p in cmd.params
              if p.role != Role.IGNORED]
    return f"{ret} on_{cmd.name}({', '.join(params)})"


def _encode_param(reg: Registry, cmd: Command, p: Param, out: list[str]) -> None:
    n = p.name
    if p.role == Role.IGNORED:
        return
    if p.role == Role.SCALAR:
        out.append(f"    b.write_varint(static_cast<uint64_t>({n}));")
    elif p.role in (Role.HANDLE, Role.SVM_POINTER):
        out.append(f"    b.write_pod<{HANDLE_T}>({n});")
    elif p.role == Role.HANDLE_ARRAY:
        count = p.len_param
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<uint64_t>({count}));")
        out.append(f"        for (uint64_t i = 0; i < static_cast<uint64_t>({count}); ++i) {{")
        out.append(f"            b.write_pod<{HANDLE_T}>({n}[i]);")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.SCALAR_ARRAY:
        count = p.len_param
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<uint64_t>({count}));")
        out.append(f"        for (uint64_t i = 0; i < static_cast<uint64_t>({count}); ++i) {{")
        out.append(f"            b.write_varint(static_cast<uint64_t>({n}[i]));")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRING:
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        const size_t wire_len = __builtin_strlen({n});")
        out.append("        b.write_varint(wire_len);")
        out.append(f"        b.write({n}, wire_len);")
        out.append("    }")
    elif p.role == Role.STRING_ARRAY:
        count = p.len_param
        lengths = p.lengths_param
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<uint64_t>({count}));")
        out.append(f"        for (uint64_t i = 0; i < static_cast<uint64_t>({count}); ++i) {{")
        if lengths:
            out.append(f"            const size_t wire_len = ({lengths} != nullptr && {lengths}[i] != 0)")
            out.append(f"                ? {lengths}[i]")
            out.append(f"                : ({n}[i] != nullptr ? __builtin_strlen(reinterpret_cast<const char*>({n}[i])) : 0);")
        else:
            out.append(f"            const size_t wire_len = {n}[i] != nullptr ? __builtin_strlen({n}[i]) : 0;")
        out.append("            b.write_varint(wire_len);")
        out.append(f"            if (wire_len != 0) b.write({n}[i], wire_len);")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.BYTES_IN and p.lent:
        size = f"{n}_bytes" if (p.len_param or "").startswith("@") else p.len_param
        _encode_lent(n, size, out)
        out.append(f"    if ({n}_lent) {{")
        out.append(f"        b.write_varint(static_cast<uint64_t>({size}));")
        out.append(f"        b.write_varint({n}_lent_at);")
        out.append(f"    }} else if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<uint64_t>({size}));")
        out.append(f"        b.write({n}, static_cast<size_t>({size}));")
        out.append("    }")
    elif p.role == Role.BYTES_IN:
        size = f"{n}_bytes" if (p.len_param or "").startswith("@") else p.len_param
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<uint64_t>({size}));")
        out.append(f"        b.write({n}, static_cast<size_t>({size}));")
        out.append("    }")
    elif p.role == Role.STRUCT_IN:
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) b.write({n}, sizeof(*{n}));")
    elif p.role == Role.PROPERTIES:
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append("        uint64_t wire_count = 0;")
        out.append(f"        while ({n}[wire_count] != 0) wire_count += 2;")
        out.append("        b.write_varint(wire_count);")
        out.append("        for (uint64_t i = 0; i < wire_count; ++i) {")
        out.append(f"            b.write_pod<{HANDLE_T}>({n}[i]);")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.PROPERTY_LIST:
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append("        uint64_t wire_count = 0;")
        out.append(f"        while ({n}[wire_count] != 0) ++wire_count;")
        out.append("        b.write_varint(wire_count);")
        out.append("        for (uint64_t i = 0; i < wire_count; ++i) {")
        out.append(f"            b.write_pod<{HANDLE_T}>(static_cast<{HANDLE_T}>({n}[i]));")
        out.append("        }")
        out.append("    }")
    elif p.role in (Role.HANDLE_OUT, Role.SCALAR_OUT, Role.STRUCT_OUT,
                    Role.HANDLE_ARRAY_OUT, Role.INFO_VALUE):
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
    elif p.role == Role.BYTES_OUT and p.lent:
        size = f"{n}_bytes" if (p.len_param or "").startswith("@") else p.len_param
        _encode_lent(n, size, out)
        out.append(f"    if ({n} != nullptr) b.write_varint(static_cast<uint64_t>({size}));")
        out.append(f"    if ({n}_lent) b.write_varint({n}_lent_at);")
    elif p.role == Role.BYTES_OUT:
        size = f"{n}_bytes" if (p.len_param or "").startswith("@") else p.len_param
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) b.write_varint(static_cast<uint64_t>({size}));")
    else:
        raise AssertionError(f"no encoder for {p.role}")


WIRE_ABSENT, WIRE_INLINE, WIRE_LENT = 0, 1, 2


def _encode_lent(n: str, size: str, out: list[str]) -> None:
    out.append(f"    uint64_t {n}_lent_at = 0;")
    out.append(f"    const bool {n}_lent = {n} != nullptr &&")
    out.append(f"        t.blobs().holds({n}, static_cast<size_t>({size}), {n}_lent_at);")
    out.append(f"    b.write_pod<uint8_t>({n} == nullptr ? {WIRE_ABSENT} : "
               f"({n}_lent ? {WIRE_LENT} : {WIRE_INLINE}));")


def _client_failure(cmd: Command) -> str:
    if cmd.returns_handle or cmd.returns_address:
        return "return 0;"
    if cmd.returns_void:
        return "return;"
    return "return CL_OUT_OF_RESOURCES;"


def _client_room(p: Param) -> str:
    if p.role == Role.HANDLE_ARRAY_OUT:
        return p.len_param
    if p.role == Role.INFO_VALUE:
        return p.size_param
    return f"{p.name}_bytes" if (p.len_param or "").startswith("@") else p.len_param


def _decode_reply(reg: Registry, cmd: Command, out: list[str]) -> None:
    fail = _client_failure(cmd)
    if cmd.returns_handle or cmd.returns_address:
        out.append("    " + HANDLE_T + " ret = 0;")
        out.append(f"    if (!r.read_pod(ret)) {fail}")
    elif not cmd.returns_void:
        out.append(f"    {cmd.return_type.cpp()} ret{{}};")
        out.append("    int64_t ret_wire = 0;")
        out.append(f"    if (!r.read_varint(ret_wire)) {fail}")
        out.append(f"    ret = static_cast<{cmd.return_type.cpp()}>(ret_wire);")

    for p in cmd.out_params:
        n = p.name
        out.append(f"    if ({n} != nullptr) {{")
        if p.role == Role.HANDLE_OUT:
            out.append(f"        if (!r.read_pod(*{n})) {fail}")
        elif p.role == Role.SCALAR_OUT:
            out.append("        uint64_t wire_value = 0;")
            out.append(f"        if (!r.read_varint(wire_value)) {fail}")
            reported = _reported_output(cmd, p)
            if reported is not None and reported.role == Role.INFO_VALUE:
                out.append(f"        if (ret == CL_SUCCESS && {reported.name} != nullptr &&")
                out.append(f"            wire_value > static_cast<uint64_t>({_client_room(reported)})) {fail}")
            out.append(f"        *{n} = static_cast<{p.type.base}>(wire_value);")
        elif p.role == Role.STRUCT_OUT:
            out.append(f"        if (!r.read({n}, sizeof(*{n}))) {fail}")
        elif p.role == Role.HANDLE_ARRAY_OUT:
            out.append("        uint64_t wire_count = 0;")
            out.append(f"        if (!r.read_varint(wire_count)) {fail}")
            out.append(f"        if (wire_count > static_cast<uint64_t>({_client_room(p)})) {fail}")
            out.append("        for (uint64_t i = 0; i < wire_count; ++i) {")
            out.append(f"            if (!r.read_pod({n}[i])) {fail}")
            out.append("        }")
        elif p.role in (Role.BYTES_OUT, Role.INFO_VALUE):
            inline = f"!{n}_lent && " if p.lent else ""
            out.append("        uint64_t wire_count = 0;")
            out.append(f"        if (!r.read_varint(wire_count)) {fail}")
            out.append(f"        if (wire_count > static_cast<uint64_t>({_client_room(p)})) {fail}")
            out.append(f"        if (wire_count != 0 && {inline}!r.read({n}, static_cast<size_t>(wire_count))) {fail}")
        else:
            raise AssertionError(f"no reply decoder for {p.role}")
        out.append("    }")


def emit_client(reg: Registry, out_dir: Path) -> None:
    h = [HEADER, "#pragma once\n",
         "#include <stddef.h>", "#include <stdint.h>", ""] + CL_PREAMBLE + ["",
         '#include "proto/client/client_transport.hpp"', "",
         "namespace av::clproto {", ""]
    c = [HEADER, '#include "cl_dispatch_client.h"', "",
         '#include "core/trace/function_scope.hpp"',
         '#include "proto/client/client_blob_arena.hpp"',
         '#include "proto/client/sync_call_scope.hpp"',
         '#include "proto/wire/wire_arena.hpp"',
         '#include "proto/wire/wire_reader.hpp"',
         '#include "proto/wire/wire_writer.hpp"',
         '#include "cl_protocol_ids.h"', "",
         "namespace av::clproto {", ""]

    for cmd in reg.commands.values():
        sig = _client_signature(reg, cmd)
        h.append(f"{sig};")
        c.append(f"{sig} {{")
        c.append(f'    AV_SCOPE("av_{cmd.name}");')
        c.append("    ::av::WireWriter b = t.request_writer();")
        for p in cmd.params:
            _encode_param(reg, cmd, p, c)
        fail = "        " + _client_failure(cmd)
        if cmd.is_async:
            c.append(f"    if (t.send_async(kCmd_{cmd.name}, b) != ::av::TransportResult::ok) {{")
            c.append(fail)
            c.append("    }")
            if cmd.returns_handle:
                c.append("    return 0;")
            elif not cmd.returns_void:
                c.append("    return CL_SUCCESS;")
        else:
            c.append("    const ::av::SyncCallScope av_call{t};")
            c.append("    ::av::WireReader r{nullptr, 0u};")
            c.append(f"    if (t.send_sync(kCmd_{cmd.name}, b, r) !=")
            c.append("        ::av::TransportResult::ok) {")
            c.append(fail)
            c.append("    }")
            c.append("    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();")
            _decode_reply(reg, cmd, c)
            if cmd.returns_handle or not cmd.returns_void:
                c.append("    return ret;")
        c.append("}")
        c.append("")

    h += ["", "}  // namespace av::clproto", ""]
    c += ["}  // namespace av::clproto", ""]
    write(out_dir / "cl_dispatch_client.h", "\n".join(h))
    write(out_dir / "cl_dispatch_client.cpp", "\n".join(c))


def _decode_param(reg: Registry, cmd: Command, p: Param, out: list[str]) -> None:
    n = p.name
    if p.role == Role.IGNORED:
        return
    if p.role == Role.SCALAR:
        out.append("    {")
        out.append("        uint64_t wire_value = 0;")
        out.append("        if (!r.read_varint(wire_value)) return false;")
        out.append(f"        {n} = static_cast<{p.type.base}>(wire_value);")
        out.append("    }")
    elif p.role == Role.HANDLE:
        out.append(f"    {{ {HANDLE_T} wire_handle = 0;")
        out.append("      if (!r.read_pod(wire_handle)) return false;")
        out.append("      if (wire_handle != 0) {")
        out.append(f"          {n} = static_cast<{p.type.base}>("
                   f"s.handle_for(HandleKind::{_kind(p.type.base)}, wire_handle));")
        out.append(f"          if ({n} == nullptr) return false;")
        out.append("      } }")
    elif p.role == Role.SVM_POINTER:
        out.append(f"    if (!r.read_pod({n}_wire)) return false;")
    elif p.role in (Role.HANDLE_ARRAY, Role.SCALAR_ARRAY):
        elem = p.element if p.role == Role.HANDLE_ARRAY else p.type.base
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        out.append(f"            auto* wire_items = arena.allocate<{elem}>(static_cast<size_t>(wire_count) + 1);")
        out.append("            if (wire_items == nullptr) return false;")
        out.append("            for (uint64_t i = 0; i < wire_count; ++i) {")
        if p.role == Role.HANDLE_ARRAY:
            out.append(f"                {HANDLE_T} wire_handle = 0;")
            out.append("                if (!r.read_pod(wire_handle)) return false;")
            out.append(f"                wire_items[i] = static_cast<{elem}>("
                       f"s.handle_for(HandleKind::{_kind(elem)}, wire_handle));")
            out.append("                if (wire_items[i] == nullptr) return false;")
        else:
            out.append("                uint64_t wire_value = 0;")
            out.append("                if (!r.read_varint(wire_value)) return false;")
            out.append(f"                wire_items[i] = static_cast<{elem}>(wire_value);")
        out.append("            }")
        out.append(f"            {n} = wire_items;")
        out.append(f"            {n}_count = wire_count;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRING:
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            uint64_t wire_len = 0;")
        out.append("            if (!r.read_varint(wire_len)) return false;")
        out.append("            char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);")
        out.append("            if (wire_text == nullptr) return false;")
        out.append("            if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;")
        out.append("            wire_text[wire_len] = '\\0';")
        out.append(f"            {n} = wire_text;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRING_ARRAY:
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        out.append(f"            auto** wire_items = arena.allocate<const {p.type.base}*>("
                   "static_cast<size_t>(wire_count) + 1);")
        out.append("            if (wire_items == nullptr) return false;")
        keeps_lengths = _lengths_consumer(cmd, p) is not None
        if keeps_lengths:
            out.append("            auto* wire_lengths = arena.allocate<size_t>("
                       "static_cast<size_t>(wire_count) + 1);")
            out.append("            if (wire_lengths == nullptr) return false;")
        out.append("            for (uint64_t i = 0; i < wire_count; ++i) {")
        out.append("                uint64_t wire_len = 0;")
        out.append("                if (!r.read_varint(wire_len)) return false;")
        out.append("                char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);")
        out.append("                if (wire_text == nullptr) return false;")
        out.append("                if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;")
        out.append("                wire_text[wire_len] = '\\0';")
        out.append(f"                wire_items[i] = reinterpret_cast<const {p.type.base}*>(wire_text);")
        if keeps_lengths:
            out.append("                wire_lengths[i] = static_cast<size_t>(wire_len);")
        out.append("            }")
        out.append(f"            {n} = wire_items;")
        out.append(f"            {n}_count = wire_count;")
        if keeps_lengths:
            out.append(f"            {n}_lengths = wire_lengths;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.BYTES_IN:
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        if p.lent:
            out.append(f"        if (wire_present == {WIRE_LENT}) {{")
            out.append("            uint64_t wire_count = 0;")
            out.append("            uint64_t wire_offset = 0;")
            out.append("            if (!r.read_varint(wire_count) || !r.read_varint(wire_offset)) return false;")
            out.append("            void* wire_bytes = s.guest_bytes(wire_offset, wire_count);")
            out.append("            if (wire_bytes == nullptr) return false;")
            out.append(f"            {n} = static_cast<{p.type.cpp()}>(wire_bytes);")
            out.append(f"            {n}_count = wire_count;")
            out.append(f"        }} else if (wire_present != {WIRE_ABSENT}) {{")
        else:
            out.append("        if (wire_present != 0) {")
        out.append("            uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        out.append("            auto* wire_bytes = arena.allocate<uint8_t>("
                   "static_cast<size_t>(wire_count) + 1);")
        out.append("            if (wire_bytes == nullptr) return false;")
        out.append("            if (wire_count != 0 && !r.read(wire_bytes, "
                   "static_cast<size_t>(wire_count))) return false;")
        out.append(f"            {n} = reinterpret_cast<{p.type.cpp()}>(wire_bytes);")
        out.append(f"            {n}_count = wire_count;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRUCT_IN:
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append(f"            auto* wire_item = arena.allocate<{p.element}>(1);")
        out.append("            if (wire_item == nullptr) return false;")
        out.append("            if (!r.read(wire_item, sizeof(*wire_item))) return false;")
        for field, kind in STRUCT_HANDLES.get(p.element, ()):
            out.append(f"            if (wire_item->{field} != nullptr) {{")
            out.append(f"                const {HANDLE_T} wire_handle = static_cast<{HANDLE_T}>("
                       f"reinterpret_cast<uintptr_t>(wire_item->{field}));")
            out.append(f"                wire_item->{field} = static_cast<decltype(wire_item->{field})>("
                       f"s.handle_for(HandleKind::{kind}, wire_handle));")
            out.append(f"                if (wire_item->{field} == nullptr) return false;")
            out.append("            }")
        out.append(f"            {n} = wire_item;")
        out.append("        }")
        out.append("    }")
    elif p.role in (Role.PROPERTIES, Role.PROPERTY_LIST):
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        out.append(f"            auto* wire_items = arena.allocate<{p.type.base}>(static_cast<size_t>(wire_count) + 2);")
        out.append("            if (wire_items == nullptr) return false;")
        out.append("            for (uint64_t i = 0; i < wire_count; ++i) {")
        out.append(f"                {HANDLE_T} wire_value = 0;")
        out.append("                if (!r.read_pod(wire_value)) return false;")
        out.append(f"                wire_items[i] = static_cast<{p.type.base}>(wire_value);")
        out.append("            }")
        out.append("            wire_items[wire_count] = 0;")
        out.append("            wire_items[wire_count + 1] = 0;")
        _decode_property_keys(p, out)
        out.append(f"            {n} = wire_items;")
        out.append("        }")
        out.append("    }")
    elif p.role in (Role.HANDLE_OUT, Role.SCALAR_OUT, Role.STRUCT_OUT,
                    Role.HANDLE_ARRAY_OUT, Role.INFO_VALUE, Role.BYTES_OUT):
        out.append("    {")
        out.append("        uint8_t wire_wanted = 0;")
        out.append("        if (!r.read_pod(wire_wanted)) return false;")
        out.append(f"        {n}_wanted = wire_wanted != 0;")
        if p.role == Role.BYTES_OUT:
            out.append(f"        if ({n}_wanted) {{")
            out.append("            uint64_t cap = 0;")
            out.append("            if (!r.read_varint(cap)) return false;")
            out.append(f"            {n}_capacity = static_cast<size_t>(cap);")
            out.append("        }")
        if p.role == Role.BYTES_OUT and p.lent:
            out.append(f"        if (wire_wanted == {WIRE_LENT}) {{")
            out.append("            uint64_t wire_offset = 0;")
            out.append("            if (!r.read_varint(wire_offset)) return false;")
            out.append(f"            {n}_lent = s.guest_bytes(wire_offset, static_cast<uint64_t>({n}_capacity));")
            out.append(f"            if ({n}_lent == nullptr) return false;")
            out.append("        }")
        out.append("    }")
    else:
        raise AssertionError(f"no server decoder for {p.role}")


def _lengths_consumer(cmd: Command, array: Param) -> Param | None:
    if not array.lengths_param:
        return None
    for other in cmd.params:
        if other.name == array.lengths_param and other.role == Role.IGNORED:
            return other
    return None


def _lengths_owner(cmd: Command, p: Param) -> Param | None:
    for other in cmd.params:
        if other.role == Role.STRING_ARRAY and other.lengths_param == p.name:
            return other
    return None


def _server_params(reg: Registry, cmd: Command) -> list[tuple[Param, str]]:
    result: list[tuple[Param, str]] = []
    for p in cmd.params:
        if p.role == Role.IGNORED:
            owner = _lengths_owner(cmd, p)
            if owner is not None:
                result.append((p, f"const size_t* {p.name}"))
            continue
        result.append((p, _server_param_decl(reg, p)))
    return result


def _server_signature2(reg: Registry, cmd: Command) -> str:
    decls = ", ".join(d for _, d in _server_params(reg, cmd))
    return f"{cmd.return_type.cpp()} on_{cmd.name}({decls})"


REPORTED_ELEMENTS = {"@image_format_bytes": "cl_image_format"}


def _reported_output(cmd: Command, p: Param) -> Param | None:
    for q in cmd.out_params:
        if q.role in (Role.HANDLE_ARRAY_OUT, Role.INFO_VALUE, Role.BYTES_OUT) and \
                q.size_ret_param == p.name:
            return q
    return None


def _answered_scalar(cmd: Command, p: Param) -> str:
    if p.type.base == "cl_int":
        return f"static_cast<uint64_t>({p.name}_storage)"
    return f"({_succeeded(cmd)}) ? static_cast<uint64_t>({p.name}_storage) : 0u"


def _produced_expr(cmd: Command, p: Param) -> str:
    cap = f"static_cast<uint64_t>({_capacity_expr(cmd, p)})"
    count = p.size_ret_param
    if count is None:
        reported = cap
    elif p.role == Role.BYTES_OUT:
        element = REPORTED_ELEMENTS[p.len_param]
        reported = f"static_cast<uint64_t>({count}_storage) * sizeof({element})"
    else:
        reported = f"static_cast<uint64_t>({count}_storage)"
    return f"({_succeeded(cmd)}) ? {reported} : 0u"


def _capacity_expr(cmd: Command, p: Param) -> str:
    if p.role == Role.HANDLE_ARRAY_OUT:
        return p.len_param or "0"
    if p.role == Role.INFO_VALUE:
        return p.size_param or "0"
    if p.role == Role.BYTES_OUT:
        return f"{p.name}_capacity"
    return "0"


def emit_server(reg: Registry, out_dir: Path) -> None:
    h = [HEADER, "#pragma once\n",
         "#include <stddef.h>", "#include <stdint.h>", ""] + CL_PREAMBLE + ["",
         '#include "proto/wire/guest_bytes.hpp"',
         '#include "proto/wire/wire_arena.hpp"',
         '#include "proto/wire/wire_reader.hpp"',
         '#include "proto/wire/wire_writer.hpp"', "",
         "namespace av::clproto {", "",
         "// The objects a guest names by number. Which numbers are objects is",
         "// the server's to say: see ServerDispatch::handle_for.",
         "enum class HandleKind : uint8_t {"]
    h += [f"    {_kind(base)}," for base in _kinds(reg)]
    h += ["};", "",
          "// What a kernel argument's bytes are to the ICD, as the kernel declared",
          "// it: a pointer in global or constant space, an image or a pipe is a",
          "// cl_mem, a sampler_t is a cl_sampler, a pointer in local space carries no",
          "// value at all, and anything else is data.",
          "enum class KernelArgument : uint8_t { data, memory, image, sampler, local, unknown };", "",
          "// Implemented by the server; the table below calls into it.",
          "class ServerDispatch {", "public:",
          "    virtual ~ServerDispatch() = default;", "",
          "    // The object of this kind the server made under this number, or",
          "    // null for a number it did not: the decoder refuses the command",
          "    // rather than hand the host's OpenCL a pointer a guest invented.",
          "    virtual void* handle_for(HandleKind kind, uint64_t wire) const = 0;",
          "    // An object a successful call is about to give the guest a number",
          "    // for; from here on that number is the object.",
          "    virtual void made(HandleKind kind, void* object) = 0;",
          "    // Whether `objects` more can be remembered, asked before the ICD",
          "    // makes any: false refuses the command as out of host memory.",
          "    virtual bool room_for(uint64_t objects) = 0;",
          "    // A successful retain or release of one. After its last release",
          "    // the number names nothing.",
          "    virtual void retained(HandleKind kind, void* object) = 0;",
          "    virtual void released(HandleKind kind, void* object) = 0;",
          "    // The address a guest names, as a pointer, or null for one that",
          "    // does not lie - with the `bytes` the command will read or write",
          "    // from it - inside a range this server handed out. Unlike a",
          "    // handle it need not name the start of one: a kernel argument",
          "    // may point into the middle of an allocation.",
          "    virtual void* svm_for(uint64_t address, uint64_t bytes) const = 0;",
          "    // The guest's own memory at this offset of its context's blob",
          "    // arena, `bytes` long, as a pointer, or null for a span that is not",
          "    // wholly inside it. A lent parameter names this instead of carrying",
          "    // its bytes, and the call reads or writes it in place.",
          "    uint8_t* guest_bytes(uint64_t offset, uint64_t bytes) const;",
          "    // What argument `index` of `kernel` is where the host's OpenCL",
          "    // will not say - a program from IL, whose argument types a host",
          "    // need not keep: false when the server knows no better either.",
          "    virtual bool declared_argument(cl_kernel kernel, cl_uint index, KernelArgument& argument) = 0;",
          ""]
    c = [HEADER, '#include "cl_dispatch_server.h"', "",
         "#include <stddef.h>", "#include <stdint.h>", "",
         '#include "cl_protocol_ids.h"',
         '#include "core/trace/function_scope.hpp"', "",
         "namespace av::clproto {", ""] + SIZE_HELPERS + [
         "ServerDispatch::ServerDispatch(const ::av::GuestBytes& guest) : guest_(guest) {}", "",
         "uint8_t* ServerDispatch::guest_bytes(uint64_t offset, uint64_t bytes) const {",
         "    return guest_.at(offset, bytes);", "}", ""]

    for cmd in reg.commands.values():
        h.append(f"    virtual {_server_signature2(reg, cmd)} = 0;")

        c.append(f"static bool decode_{cmd.name}(ServerDispatch& s, "
                 "::av::WireReader& r,")
        c.append(f"{' ' * (len(cmd.name) + 19)}::av::WireWriter& reply, "
                 "::av::WireArena& arena) {")
        c.append(f'    AV_SCOPE("server:{cmd.name}");')
        c.append("    (void)r;")
        c.append("    (void)reply;")
        c.append("    (void)arena;")
        for p in cmd.params:
            if p.role == Role.IGNORED:
                owner = _lengths_owner(cmd, p)
                if owner is not None:
                    c.append(f"    const size_t* {p.name} = nullptr;")
                continue
            if p.role == Role.SCALAR:
                c.append(f"    {p.type.base} {p.name}{{}};")
            elif p.role == Role.HANDLE:
                c.append(f"    {p.type.base} {p.name} = nullptr;")
            elif p.role == Role.SVM_POINTER:
                c.append(f"    {p.type.cpp()} {p.name} = nullptr;")
                c.append(f"    {HANDLE_T} {p.name}_wire = 0;")
            elif p.role == Role.HANDLE_ARRAY:
                c.append(f"    const {p.element}* {p.name} = nullptr;")
                c.append(f"    uint64_t {p.name}_count = 0;")
            elif p.role == Role.SCALAR_ARRAY:
                c.append(f"    const {p.type.base}* {p.name} = nullptr;")
                c.append(f"    uint64_t {p.name}_count = 0;")
            elif p.role == Role.STRING:
                c.append(f"    const char* {p.name} = nullptr;")
            elif p.role == Role.STRING_ARRAY:
                c.append(f"    const {p.type.base}** {p.name} = nullptr;")
                c.append(f"    uint64_t {p.name}_count = 0;")
                if _lengths_consumer(cmd, p) is not None:
                    c.append(f"    const size_t* {p.name}_lengths = nullptr;")
            elif p.role == Role.BYTES_IN:
                c.append(f"    {p.type.cpp()} {p.name} = nullptr;")
                c.append(f"    uint64_t {p.name}_count = 0;")
            elif p.role == Role.STRUCT_IN:
                c.append(f"    const {p.element}* {p.name} = nullptr;")
            elif p.role in (Role.PROPERTIES, Role.PROPERTY_LIST):
                c.append(f"    const {p.type.base}* {p.name} = nullptr;")
            else:
                c.append(f"    bool {p.name}_wanted = false;")
                if p.role == Role.BYTES_OUT:
                    c.append(f"    size_t {p.name}_capacity = 0;")
                if p.role == Role.BYTES_OUT and p.lent:
                    c.append(f"    void* {p.name}_lent = nullptr;")

        for p in cmd.params:
            _decode_param(reg, cmd, p, c)
        c.extend(_refusals(cmd))
        c.extend(_resolve_addresses(cmd))

        for p in cmd.out_params:
            n = p.name
            if p.role == Role.HANDLE_OUT:
                c.append(f"    {p.element} {n}_storage = nullptr;")
                c.append(f"    {p.element}* {n} = {n}_wanted ? &{n}_storage : nullptr;")
            elif p.role == Role.SCALAR_OUT:
                c.append(f"    {p.type.base} {n}_storage{{}};")
                reported = _reported_output(cmd, p)
                wanted = f"({n}_wanted || {reported.name}_wanted)" if reported else f"{n}_wanted"
                c.append(f"    {p.type.base}* {n} = {wanted} ? &{n}_storage : nullptr;")
            elif p.role == Role.STRUCT_OUT:
                c.append(f"    {p.element} {n}_storage{{}};")
                c.append(f"    {p.element}* {n} = {n}_wanted ? &{n}_storage : nullptr;")
            elif p.role == Role.HANDLE_ARRAY_OUT:
                cap = _capacity_expr(cmd, p)
                c.append(f"    {p.element}* {n} = nullptr;")
                c.append(f"    if ({n}_wanted) {{")
                c.append(f"        {n} = arena.allocate<{p.element}>(static_cast<size_t>({cap}) + 1);")
                c.append(f"        if ({n} == nullptr) return false;")
                c.append("    }")
            elif p.role in (Role.INFO_VALUE, Role.BYTES_OUT):
                cap = _capacity_expr(cmd, p)
                ptr_t = "void*" if p.type.base == "void" else f"{p.type.base}*"
                c.append(f"    {ptr_t} {n} = nullptr;")
                if p.lent:
                    c.append(f"    if ({n}_lent != nullptr) {{")
                    c.append(f"        {n} = static_cast<{ptr_t}>({n}_lent);")
                    c.append(f"    }} else if ({n}_wanted) {{")
                else:
                    c.append(f"    if ({n}_wanted) {{")
                c.append(f"        {n} = reinterpret_cast<{ptr_t}>("
                         f"arena.allocate<uint8_t>(static_cast<size_t>({cap}) + 1));")
                c.append(f"        if ({n} == nullptr) return false;")
                c.append("    }")

        for p in cmd.params:
            if p.role == Role.IGNORED and _lengths_owner(cmd, p) is not None:
                owner = _lengths_owner(cmd, p)
                c.append(f"    {p.name} = {owner.name}_lengths;")

        args = ", ".join(p.name for p, _ in _server_params(reg, cmd))
        room_terms = ["1u"] if cmd.returns_handle else []
        for p in cmd.out_params:
            if p.role == Role.HANDLE_OUT:
                room_terms.append(f"({p.name}_wanted ? 1u : 0u)")
            elif p.role == Role.HANDLE_ARRAY_OUT:
                room_terms.append(f"({p.name}_wanted ? static_cast<uint64_t>({_capacity_expr(cmd, p)}) : 0u)")
        room = ("s.room_for(" + " + ".join(f"static_cast<uint64_t>({t})" for t in room_terms) + ")"
                if room_terms else None)
        call = f"s.on_{cmd.name}({args})"
        if cmd.returns_void:
            c.append(f"    if ({room}) {call};" if room else f"    {call};")
        elif room is None:
            c.append(f"    const {cmd.return_type.cpp()} ret = {call};")
        elif cmd.returns_handle or cmd.returns_address:
            errcode = [p for p in cmd.out_params if p.role == Role.SCALAR_OUT and p.name == "errcode_ret"]
            c.append(f"    {cmd.return_type.cpp()} ret = nullptr;")
            c.append(f"    if ({room}) ret = {call};")
            if errcode:
                c.append("    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;")
        else:
            if cmd.return_type.cpp() != "cl_int":
                raise ValueError(f"{cmd.name} answers objects and no cl_int to refuse them with")
            c.append(f"    const cl_int ret = {room} ? {call} : CL_OUT_OF_HOST_MEMORY;")
        succeeded = _succeeded(cmd)
        lifecycle = _lifecycle(cmd)
        if lifecycle is not None:
            hook, obj = lifecycle
            c.append(f"    if ({succeeded}) s.{hook}(HandleKind::{_kind(obj.type.base)}, {obj.name});")

        if cmd.returns_handle:
            c.append(f"    if ({succeeded}) s.made(HandleKind::{_kind(cmd.return_type.base)}, ret);")
            c.append("    reply.write_pod<" + HANDLE_T +
                     ">(static_cast<" + HANDLE_T + ">(reinterpret_cast<uintptr_t>(ret)));")
        elif cmd.returns_address:
            c.append("    reply.write_pod<" + HANDLE_T +
                     ">(static_cast<" + HANDLE_T + ">(reinterpret_cast<uintptr_t>(ret)));")
        elif not cmd.returns_void:
            c.append("    reply.write_varint(static_cast<uint64_t>("
                     "static_cast<int64_t>(ret)));")

        for p in cmd.out_params:
            n = p.name
            c.append(f"    if ({n}_wanted) {{")
            if p.role == Role.HANDLE_OUT:
                c.append(f"        if (!({succeeded})) {n}_storage = nullptr;")
                c.append(f"        if ({n}_storage != nullptr) "
                         f"s.made(HandleKind::{_kind(p.element)}, {n}_storage);")
                c.append(f"        reply.write_pod<{HANDLE_T}>(static_cast<{HANDLE_T}>("
                         f"reinterpret_cast<uintptr_t>({n}_storage)));")
            elif p.role == Role.SCALAR_OUT:
                c.append(f"        reply.write_varint({_answered_scalar(cmd, p)});")
            elif p.role == Role.STRUCT_OUT:
                c.append(f"        reply.write(&{n}_storage, sizeof({n}_storage));")
            elif p.role == Role.HANDLE_ARRAY_OUT:
                cap = _capacity_expr(cmd, p)
                c.append(f"        const uint64_t wire_produced = {_produced_expr(cmd, p)};")
                c.append(f"        const uint64_t wire_sent = wire_produced < static_cast<uint64_t>({cap})")
                c.append(f"            ? wire_produced : static_cast<uint64_t>({cap});")
                c.append("        reply.write_varint(wire_sent);")
                c.append("        for (uint64_t i = 0; i < wire_sent; ++i) {")
                c.append(f"            if ({n}[i] != nullptr) "
                         f"s.made(HandleKind::{_kind(p.element)}, {n}[i]);")
                c.append(f"            reply.write_pod<{HANDLE_T}>(static_cast<{HANDLE_T}>("
                         f"reinterpret_cast<uintptr_t>({n}[i])));")
                c.append("        }")
            elif p.role in (Role.INFO_VALUE, Role.BYTES_OUT):
                cap = _capacity_expr(cmd, p)
                c.append(f"        uint64_t wire_written = {_produced_expr(cmd, p)};")
                c.append(f"        if (wire_written > static_cast<uint64_t>({cap}))")
                c.append(f"            wire_written = static_cast<uint64_t>({cap});")
                c.append("        reply.write_varint(wire_written);")
                inline = f" && {n}_lent == nullptr" if p.lent else ""
                c.append(f"        if (wire_written != 0{inline}) reply.write({n}, static_cast<size_t>(wire_written));")
            c.append("    }")

        c.append("    return true;")
        c.append("}")
        c.append("")

    c.append("bool dispatch_command(ServerDispatch& s, uint32_t cmd_id,")
    c.append("                      ::av::WireReader& r, ::av::WireWriter& reply,")
    c.append("                      ::av::WireArena& arena) {")
    c.append("    switch (cmd_id) {")
    for cmd in reg.commands.values():
        c.append(f"        case kCmd_{cmd.name}: "
                 f"return decode_{cmd.name}(s, r, reply, arena);")
    c.append("        default: return false;")
    c.append("    }")
    c.append("}")
    c.append("")

    h += ["", "protected:",
          "    // `guest` is the context's blob arena, which the implementation owns",
          "    // and adopts its first import as.",
          "    explicit ServerDispatch(const ::av::GuestBytes& guest);", "",
          "private:", "    const ::av::GuestBytes& guest_;",
          "};", "",
          "// `arena` is where everything a decoded command points at lives,",
          "// for the length of the call; it is the caller's block, because it",
          "// has to outlive the decode that filled it.",
          "[[nodiscard]] bool dispatch_command(ServerDispatch& s, uint32_t cmd_id,",
          "                                    ::av::WireReader& r, ::av::WireWriter& reply,",
          "                                    ::av::WireArena& arena);", "",
          "}  // namespace av::clproto", ""]
    c += ["}  // namespace av::clproto", ""]
    write(out_dir / "cl_dispatch_server.h", "\n".join(h))
    write(out_dir / "cl_dispatch_server.cpp", "\n".join(c))



_EXTENSION_SUFFIXES = ("KHR", "EXT", "INTEL", "AMD", "APPLE", "ARM", "NV", "QCOM", "IMG")


def _is_extension(name: str) -> bool:
    return name.endswith(_EXTENSION_SUFFIXES)


def _absent_result(cmd) -> str:
    base = cmd.return_type.base if hasattr(cmd, "return_type") else "cl_int"
    if base == "cl_int":
        return "CL_INVALID_OPERATION"
    return "nullptr"


DEFAULT_ENUMERATED = 2


def _default_body(reg: Registry, cmd: Command) -> list[str]:
    body = [f"        (void){p.name};" for p, _ in _server_params(reg, cmd)]
    body.append(f"        called(kCmd_{cmd.name});")
    for p in cmd.params:
        n = p.name
        count = _count_of(cmd, p.len_param)
        span = f"static_cast<size_t>({count})" if count else None
        if p.role == Role.HANDLE:
            body.append(f"        handed(HandleKind::{_kind(p.type.base)}, {n});")
        elif p.role == Role.HANDLE_ARRAY and span:
            body.append(f"        for (size_t i = 0; {n} != nullptr && i < {span}; ++i) "
                        f"handed(HandleKind::{_kind(p.element)}, {n}[i]);")
        elif p.role in (Role.SCALAR_ARRAY, Role.STRING_ARRAY) and span:
            body.append(f"        if ({n} != nullptr) touched({n}, {span} * sizeof(*{n}));")
        elif p.role in (Role.BYTES_IN, Role.BYTES_OUT) and span:
            body.append(f"        if ({n} != nullptr) touched({n}, {span});")
        elif p.role == Role.INFO_VALUE and p.size_param:
            body.append(f"        if ({n} != nullptr) touched({n}, static_cast<size_t>({p.size_param}));")
        elif p.role == Role.STRUCT_IN:
            for field, kind in STRUCT_HANDLES.get(p.element, ()):
                body.append(f"        if ({n} != nullptr) handed(HandleKind::{kind}, {n}->{field});")
        elif p.role == Role.PROPERTIES and p.type.base == "cl_context_properties":
            body.append(f"        for (size_t i = 0; {n} != nullptr && {n}[i] != 0; i += 2) {{")
            body.append(f"            if ({n}[i] == CL_CONTEXT_PLATFORM) "
                        f"handed(HandleKind::platform, reinterpret_cast<void*>({n}[i + 1]));")
            body.append("        }")
        elif p.role == Role.HANDLE_OUT:
            body.append(f"        if ({n} != nullptr) *{n} = static_cast<{p.element}>("
                        f"produce(HandleKind::{_kind(p.element)}));")
        elif p.role == Role.HANDLE_ARRAY_OUT:
            body.append(f"        for (size_t i = 0; {n} != nullptr && i < static_cast<size_t>({p.len_param}) && "
                        f"i < {DEFAULT_ENUMERATED}u; ++i) {{")
            body.append(f"            {n}[i] = static_cast<{p.element}>(produce(HandleKind::{_kind(p.element)}));")
            body.append("        }")
            if p.size_ret_param:
                body.append(f"        if ({p.size_ret_param} != nullptr) *{p.size_ret_param} = {DEFAULT_ENUMERATED}u;")
        elif p.role == Role.SCALAR_OUT and n == "errcode_ret":
            body.append(f"        if ({n} != nullptr) *{n} = CL_SUCCESS;")
    if cmd.returns_handle:
        body.append(f"        return static_cast<{cmd.return_type.base}>("
                    f"produce(HandleKind::{_kind(cmd.return_type.base)}));")
    elif cmd.returns_address:
        body.append("        return nullptr;")
    elif cmd.return_type.base == "cl_int":
        body.append("        return CL_SUCCESS;")
    elif not cmd.returns_void:
        body.append("        return {};")
    return body


def emit_default_dispatch(reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n", "#include <stddef.h>", ""] + CL_PREAMBLE + ["",
           '#include "cl_dispatch_server.h"',
           '#include "cl_protocol_ids.h"', "",
           "namespace av::clproto {", "",
           "// Every command answered with no OpenCL behind it, for judging the",
           "// decoder rather than a driver. Each call says which it is to",
           "// `called`, hands every object it was given to `handed` and every",
           "// span it would read or write to `touched` - that is where a fake",
           "// holds the decoder to what it resolved and bounded - then",
           "// succeeds, answering whatever objects it makes with `produce`.",
           "class DefaultDispatch : public ServerDispatch {", "public:",
           "    using ServerDispatch::ServerDispatch;", ""]
    for cmd in reg.commands.values():
        out.append(f"    {_server_signature2(reg, cmd)} override {{")
        out.extend(_default_body(reg, cmd))
        out.append("    }")
    out += ["    bool declared_argument(cl_kernel, cl_uint, KernelArgument&) override { return false; }",
            "", "protected:",
            "    virtual void called(uint32_t command_id) = 0;",
            "    virtual void handed(HandleKind kind, void* object) = 0;",
            "    virtual void touched(const void* bytes, size_t length) = 0;",
            "    virtual void* produce(HandleKind kind) = 0;",
            "};", "", "}  // namespace av::clproto", ""]
    write(out_dir / "cl_default_dispatch.h", "\n".join(out))


def emit_real_dispatch(reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n"] + CL_PREAMBLE + ["",
           '#include "cl_dispatch_server.h"', "",
           "namespace av::clproto {", "",
           "// Every command forwarded to whatever OpenCL this process",
           "// loaded. Which numbers are its objects is not this class's to",
           "// say: the server that derives from it answers handle_for.",
           "//",
           "// An extension command is resolved rather than linked. The ICD",
           "// loader on a machine is whichever one is first on the library",
           "// path, and they do not all export the same extension symbols -",
           "// linking clGetGLContextInfoKHR made the whole executor library",
           "// unloadable on a host whose loader had no GL sharing (042).",
           "class RealDispatch : public ServerDispatch {", "public:",
           "    using ServerDispatch::ServerDispatch;", ""]
    members: list[str] = []
    for cmd in reg.commands.values():
        decls = _server_params(reg, cmd)
        sig = _server_signature2(reg, cmd)
        if cmd.name in CALLBACK_COMMANDS:
            out.append(f"    {sig} override = 0;")
            continue
        args = []
        for p in cmd.params:
            if p.role == Role.IGNORED:
                if _lengths_owner(cmd, p) is not None:
                    args.append(p.name)
                else:
                    args.append("nullptr")
                continue
            args.append(p.name)
        if _is_extension(cmd.name):
            pointer = f"pfn_{cmd.name}_"
            members.append(f"    {cmd.name}_fn {pointer} = nullptr;")
            out.append(f"    using {cmd.name}_fn = decltype(&::{cmd.name});")
            out.append(f"    {sig} override {{")
            out.append(f"        if ({pointer} == nullptr) {{")
            out.append(f"            {pointer} = reinterpret_cast<{cmd.name}_fn>(")
            out.append("                ::clGetExtensionFunctionAddress("
                       f'"{cmd.name}"));')
            out.append("        }")
            out.append(f"        if ({pointer} == nullptr) {{")
            if cmd.returns_void:
                out.append("            return;")
            else:
                out.append(f"            return {_absent_result(cmd)};")
            out.append("        }")
            call = f"{pointer}({', '.join(args)})"
            if cmd.returns_void:
                out.append(f"        {call};")
            else:
                out.append(f"        return {call};")
            out.append("    }")
            continue
        call = f"::{cmd.name}({', '.join(args)})"
        out.append(f"    {sig} override {{")
        if cmd.returns_void:
            out.append(f"        {call};")
        else:
            out.append(f"        return {call};")
        out.append("    }")
    out.append("    bool declared_argument(cl_kernel, cl_uint, KernelArgument&) override { return false; }")
    out += ["", "private:"] + members
    out += ["};", "", "}  // namespace av::clproto", ""]
    write(out_dir / "cl_real_dispatch.h", "\n".join(out))


def emit_ids(reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n", "#include <stdint.h>", "",
           "namespace av::clproto {", ""]
    for cmd in reg.commands.values():
        out.append(f"inline constexpr uint32_t kCmd_{cmd.name} = "
                   f"0x{cmd.cmd_id:08X}U;")
    out += ["", "}  // namespace av::clproto", ""]
    write(out_dir / "cl_protocol_ids.h", "\n".join(out))


def emit_names(reg: Registry, out_dir: Path) -> None:
    h = [HEADER, "#pragma once\n", "#include <stdint.h>", "",
         "namespace av::clproto {", "",
         "// A static name for a wire id; \"unknown\" for anything else,",
         "// never null.",
         "[[nodiscard]] const char* cmd_id_to_string(uint32_t cmd_id) noexcept;",
         "// Whether the profile carries `cmd_id`: what a session decodes at all.",
         "[[nodiscard]] bool knows_command(uint32_t cmd_id) noexcept;",
         "", "}  // namespace av::clproto", ""]
    c = [HEADER, '#include "cl_command_names.h"', "",
         '#include "cl_protocol_ids.h"', "",
         "namespace av::clproto {", "",
         "const char* cmd_id_to_string(uint32_t cmd_id) noexcept {",
         "    switch (cmd_id) {"]
    for cmd in reg.commands.values():
        c.append(f'        case kCmd_{cmd.name}: return "{cmd.name}";')
    c += ["        default: return \"unknown\";", "    }", "}", "",
          "bool knows_command(uint32_t cmd_id) noexcept {", "    switch (cmd_id) {"]
    for cmd in reg.commands.values():
        c.append(f"        case kCmd_{cmd.name}:")
    c += ["            return true;", "        default: return false;", "    }", "}", "",
          "}  // namespace av::clproto", ""]
    write(out_dir / "cl_command_names.h", "\n".join(h))
    write(out_dir / "cl_command_names.cpp", "\n".join(c))


def emit_all(reg: Registry, out_dir: Path, raw=None) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    emit_ids(reg, out_dir)
    emit_names(reg, out_dir)
    emit_client(reg, out_dir)
    emit_server(reg, out_dir)
    emit_real_dispatch(reg, out_dir)
    emit_default_dispatch(reg, out_dir)
    emit_supported_extensions(reg, out_dir)
    if raw is not None:
        emit_icd_stubs(raw, reg, out_dir)


def emit_supported_extensions(reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n", "#include <stddef.h>", "",
           "namespace av::clproto {", "",
           "// Every extension whose commands this build answers - over the",
           "// wire or in the driver - and whose own dependencies it answers",
           "// too. An extension needing an entry point that does neither is",
           "// not one this driver can offer, whatever the host reports.",
           "inline constexpr const char* kCarriedExtensions[] = {"]
    for name in reg.carried_extensions:
        out.append(f'    "{name}",')
    out += ["};", "",
            "inline constexpr size_t kCarriedExtensionCount =",
            "    sizeof(kCarriedExtensions) / sizeof(kCarriedExtensions[0]);",
            "",
            "[[nodiscard]] inline bool extension_is_carried(const char* name) noexcept {",
            "    if (name == nullptr) return false;",
            "    for (size_t i = 0; i < kCarriedExtensionCount; ++i) {",
            "        if (__builtin_strcmp(name, kCarriedExtensions[i]) == 0) return true;",
            "    }",
            "    return false;",
            "}",
            "",
            "}  // namespace av::clproto", ""]
    write(out_dir / "cl_supported_extensions.h", "\n".join(out))


def _stub_signature(raw, name: str) -> tuple[str, str, str]:
    ret = raw.return_type.cpp()
    params = []
    has_errcode = False
    for i, p in enumerate(raw.params):
        if p.is_callback:
            params.append(re.sub(rf"\b{p.name}\b", f"/*{p.name}*/", p.text,
                                 count=1))
            continue
        if p.name == "errcode_ret" and p.type.cpp() == "cl_int*":
            has_errcode = True
            params.append("cl_int* errcode_ret")
            continue
        params.append(f"{p.type.cpp()} /*{p.name}*/")
    prologue = ("if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; "
                if has_errcode else "")
    if ret == "cl_int":
        answer = prologue + "return CL_INVALID_OPERATION;"
    elif raw.return_type.pointer_depth > 0 or ret.startswith("cl_") and \
            ret not in ("cl_int", "cl_uint", "cl_ulong", "cl_bool"):
        answer = prologue + "return nullptr;"
    elif ret == "void":
        answer = ""
    else:
        answer = "return 0;"
    return ret, ", ".join(params) if params else "", answer


def emit_icd_stubs(raw, reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n",
           "#include <type_traits>", ""] + CL_PREAMBLE + [
           "#include <CL/cl_icd.h>", "",
           '#include "proto/client/icd_stub_report.hpp"', "",
           "namespace av::clproto {", "",
           "// Every one of these answers CL_INVALID_OPERATION, and says so",
           "// once: an application that asks for something this driver does",
           "// not carry gets a clean refusal, and whoever is reading the log",
           "// gets the name of what it asked for. See",
           "// proto/client/icd_stub_report.hpp.",
           "namespace stubs {", ""]

    def declarable(t) -> bool:
        base = t.base
        if base.startswith("cl_GL") or base.startswith("cl_gl"):
            return False
        if base.startswith("cl_") and not any(
                token in base.lower()
                for token in ("d3d", "dx9", "egl", "va_api", "gl_")):
            return True
        return base in ("size_t", "void", "char", "unsigned char", "int",
                        "unsigned int", "float", "double", "intptr_t")

    names: list[str] = []
    everything: list[str] = list(raw.commands)
    for name, cmd in raw.commands.items():
        if not cmd.since.startswith("CL_VERSION_"):
            continue
        if not declarable(cmd.return_type):
            continue
        if not all(p.is_callback or declarable(p.type) for p in cmd.params):
            continue
        ret, params, answer = _stub_signature(cmd, name)
        out.append(f"inline {ret} CL_API_CALL {name}({params}) {{")
        out.append(f'    ::av::icd::stub_called("cl", "{name}", '
                   '"this driver does not carry it");')
        if answer:
            out.append(f"    {answer}")
        out.append("}")
        names.append(name)
    out.append("")
    out.append("}  // namespace stubs")
    out.append("")
    out.append("// A refusal built from the table's own slot type.")
    out.append("//")
    out.append("// The named stubs above are wire_written from the registry, and")
    out.append("// for a handful of commands the registry and cl_icd.h")
    out.append("// disagree about a parameter — void* against void*[], and")
    out.append("// the like. Those slots cannot take their named stub, and")
    out.append("// leaving them as they were found means leaving them null,")
    out.append("// which is the one outcome worth avoiding: the loader does")
    out.append("// not check, and calls through.")
    out.append("//")
    out.append("// So a second refusal is generated from the slot's own")
    out.append("// type. It cannot report through errcode_ret, because it")
    out.append("// does not know which parameter that is, but it returns")
    out.append("// what the return type means by failure and it is exactly")
    out.append("// the type the table wants.")
    out.append("template <typename Fn>")
    out.append("struct Refuse;")
    out.append("")
    out.append("template <typename R, typename... A>")
    out.append("struct Refuse<R(CL_API_CALL*)(A...)> {")
    out.append("    static R CL_API_CALL call(A...) {")
    out.append("        if constexpr (std::is_void_v<R>) {")
    out.append("            return;")
    out.append("        } else if constexpr (std::is_pointer_v<R>) {")
    out.append("            return nullptr;")
    out.append("        } else {")
    out.append("            return static_cast<R>(CL_INVALID_OPERATION);")
    out.append("        }")
    out.append("    }")
    out.append("};")
    out.append("")
    out.append("// Whether a slot is a function pointer at all. cl_icd.h")
    out.append("// declares the platform-tied ones as void*, and nothing")
    out.append("// can be put in those.")
    out.append("template <typename T>")
    out.append("struct IsRefusable : std::false_type {};")
    out.append("")
    out.append("template <typename R, typename... A>")
    out.append("struct IsRefusable<R(CL_API_CALL*)(A...)> : std::true_type {};")
    out.append("")
    out.append("// Fill every slot the driver did not claim.")
    out.append("//")
    out.append("// The named stub goes in when its type matches the slot")
    out.append("// exactly; the generic refusal goes in when the slot is a")
    out.append("// function pointer of some other shape; and where the")
    out.append("// header declares the slot as a bare void* — which is what")
    out.append("// it does for a version this translation unit was not")
    out.append("// built for — the named stub goes in through a cast,")
    out.append("// because a loader built for that version will call it")
    out.append("// with the signature the registry gave. Either way the")
    out.append("// slot is not left null.")
    out.append("//")
    out.append("// A template because `if constexpr` only discards what it")
    out.append("// does not take in a template: in an ordinary function")
    out.append("// the branch not taken is still type-checked, and the")
    out.append("// whole point here is that some of those branches do not")
    out.append("// type-check.")
    out.append("template <typename Table>")
    out.append("inline void fill_unimplemented(Table& table) {")
    named = set(names)
    for name in everything:
        out.append(f"    if constexpr (requires {{ table.{name}; }}) {{")
        if name in named:
            out.append(f"        if constexpr (std::is_same_v<decltype(table.{name}),")
            out.append(f"                                     decltype(&stubs::{name})>) {{")
            out.append(f"            if (table.{name} == nullptr) table.{name} = &stubs::{name};")
            out.append(f"        }} else if constexpr (IsRefusable<decltype(table.{name})>::value) {{")
            out.append(f"            if (table.{name} == nullptr)")
            out.append(f"                table.{name} = &Refuse<decltype(table.{name})>::call;")
            out.append(f"        }} else if constexpr (std::is_same_v<decltype(table.{name}), void*>) {{")
            out.append(f"            if (table.{name} == nullptr)")
            out.append(f"                table.{name} = reinterpret_cast<void*>(&stubs::{name});")
            out.append("        }")
        else:
            out.append(f"        if constexpr (IsRefusable<decltype(table.{name})>::value) {{")
            out.append(f"            if (table.{name} == nullptr)")
            out.append(f"                table.{name} = &Refuse<decltype(table.{name})>::call;")
            out.append("        }")
        out.append("    }")
    out.append("}")
    out += ["", "}  // namespace av::clproto", ""]
    write(out_dir / "cl_icd_stubs.h", "\n".join(out))
