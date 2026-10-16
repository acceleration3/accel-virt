// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.opencl --xml cl.xml --out ...

#include "cl_dispatch_server.h"

#include <stddef.h>
#include <stdint.h>

#include "cl_protocol_ids.h"
#include "core/trace/function_scope.hpp"

namespace av::clproto {

// How far into a host pointer a transfer reaches: `offset`, then whole
// rows and slices at the pitches given, and the last row only as far as
// the region does. 0 for an empty region, which the ICD refuses without
// touching anything; false for a size that overflows.
static bool pitched_reach(const size_t* region, uint64_t element, uint64_t row,
                          uint64_t slice, uint64_t offset, uint64_t& bytes) {
    bytes = 0;
    if (region[0] == 0 || region[1] == 0 || region[2] == 0) return true;
    uint64_t rows = 0;
    uint64_t slices = 0;
    uint64_t last_row = 0;
    return !__builtin_mul_overflow(row, static_cast<uint64_t>(region[1] - 1), &rows) &&
           !__builtin_mul_overflow(slice, static_cast<uint64_t>(region[2] - 1), &slices) &&
           !__builtin_mul_overflow(static_cast<uint64_t>(region[0]), element, &last_row) &&
           !__builtin_add_overflow(offset, rows, &bytes) &&
           !__builtin_add_overflow(bytes, slices, &bytes) &&
           !__builtin_add_overflow(bytes, last_row, &bytes);
}

// A buffer rectangle: bytes, from the host origin, at the pitches the
// specification defaults a 0 to.
static bool rect_bytes(const size_t* origin, const size_t* region, size_t row_pitch,
                       size_t slice_pitch, uint64_t& bytes) {
    if (origin == nullptr || region == nullptr) return false;
    const uint64_t row = row_pitch != 0 ? row_pitch : region[0];
    uint64_t slice = slice_pitch;
    if (slice == 0 && __builtin_mul_overflow(static_cast<uint64_t>(region[0]),
                                             static_cast<uint64_t>(region[1]), &slice)) return false;
    uint64_t across = 0;
    uint64_t down = 0;
    uint64_t offset = 0;
    if (__builtin_mul_overflow(static_cast<uint64_t>(origin[1]), row, &across) ||
        __builtin_mul_overflow(static_cast<uint64_t>(origin[2]), slice, &down) ||
        __builtin_add_overflow(across, down, &offset) ||
        __builtin_add_overflow(offset, static_cast<uint64_t>(origin[0]), &offset)) return false;
    return pitched_reach(region, 1, row, slice, offset, bytes);
}

// An image transfer: elements of the image's own format, which the ICD
// is asked for rather than the guest - into the arena, like every other
// answer the ICD writes.
static bool image_transfer_bytes(ServerDispatch& s, ::av::WireArena& arena, cl_mem image,
                                 const size_t* region, size_t row_pitch, size_t slice_pitch,
                                 uint64_t& bytes) {
    if (region == nullptr) return false;
    auto* element = arena.allocate<size_t>(1);
    if (element == nullptr ||
        s.on_clGetImageInfo(image, CL_IMAGE_ELEMENT_SIZE, sizeof *element, element, nullptr) != CL_SUCCESS ||
        *element == 0) return false;
    uint64_t row = row_pitch;
    if (row == 0 && __builtin_mul_overflow(static_cast<uint64_t>(region[0]),
                                           static_cast<uint64_t>(*element), &row)) return false;
    uint64_t slice = slice_pitch;
    if (slice == 0 && __builtin_mul_overflow(row, static_cast<uint64_t>(region[1]), &slice)) return false;
    return pitched_reach(region, *element, row, slice, 0, bytes);
}

// The bytes one element of a format occupies, from the channel order and
// data type the way the specification's table does, or 0 for a pairing
// this server does not size. Where implementations disagree - a padded
// order, a 24-bit depth - the larger answer is the one taken: the bound
// is on what an ICD may read, so erring high refuses and erring low reads
// past the block.
static uint64_t format_element_bytes(const cl_image_format* format) {
    if (format == nullptr) return 0;
    const cl_channel_order order = format->image_channel_order;
    const cl_channel_type type = format->image_channel_data_type;
    if (type == CL_UNORM_SHORT_565 || type == CL_UNORM_SHORT_555)
        return order == CL_RGB || order == CL_RGBx ? 2 : 0;
    if (type == CL_UNORM_INT_101010)
        return order == CL_RGB || order == CL_RGBx ? 4 : 0;
    if (type == CL_UNORM_INT_101010_2) return order == CL_RGBA ? 4 : 0;
    if (order == CL_DEPTH_STENCIL)
        return type == CL_UNORM_INT24 ? 4 : type == CL_FLOAT ? 8 : 0;
    uint64_t channel = 0;
    switch (type) {
        case CL_SNORM_INT8: case CL_UNORM_INT8: case CL_SIGNED_INT8: case CL_UNSIGNED_INT8:
            channel = 1; break;
        case CL_SNORM_INT16: case CL_UNORM_INT16: case CL_SIGNED_INT16: case CL_UNSIGNED_INT16:
        case CL_HALF_FLOAT:
            channel = 2; break;
        case CL_SIGNED_INT32: case CL_UNSIGNED_INT32: case CL_FLOAT: case CL_UNORM_INT24:
            channel = 4; break;
        default: return 0;
    }
    switch (order) {
        case CL_R: case CL_A: case CL_INTENSITY: case CL_LUMINANCE: case CL_DEPTH:
            return channel;
        case CL_RG: case CL_RA: case CL_Rx:
            return 2 * channel;
        case CL_RGB: case CL_RGx: case CL_sRGB:
            return 3 * channel;
        case CL_RGBA: case CL_BGRA: case CL_ARGB: case CL_ABGR: case CL_RGBx: case CL_sRGBx:
        case CL_sRGBA: case CL_sBGRA:
            return 4 * channel;
        default: return 0;
    }
}

// The bytes an image made from a host pointer copies out of it: whole
// rows and slices at the pitches given, or at the tight ones where the
// pitch given is smaller, for every slice or array element. Whole blocks
// rather than the last row cut short, because an ICD may copy either way.
static bool image_host_bytes(const cl_image_format* format, cl_mem_object_type type,
                             size_t width, size_t height, size_t depth, size_t array_size,
                             size_t row_pitch, size_t slice_pitch, uint64_t& bytes) {
    const uint64_t element = format_element_bytes(format);
    if (element == 0) return false;
    uint64_t row = 0;
    if (__builtin_mul_overflow(static_cast<uint64_t>(width), element, &row)) return false;
    if (row < row_pitch) row = row_pitch;
    uint64_t rows = height;
    uint64_t slices = 1;
    switch (type) {
        case CL_MEM_OBJECT_IMAGE1D: case CL_MEM_OBJECT_IMAGE1D_BUFFER: rows = 1; break;
        case CL_MEM_OBJECT_IMAGE1D_ARRAY: rows = 1; slices = array_size; break;
        case CL_MEM_OBJECT_IMAGE2D: break;
        case CL_MEM_OBJECT_IMAGE2D_ARRAY: slices = array_size; break;
        case CL_MEM_OBJECT_IMAGE3D: slices = depth; break;
        default: return false;
    }
    uint64_t slice = 0;
    if (__builtin_mul_overflow(row, rows, &slice)) return false;
    if (slice < slice_pitch) slice = slice_pitch;
    return !__builtin_mul_overflow(slice, slices, &bytes);
}

// A build with -cl-kernel-arg-info after whatever the guest asked for, in
// the arena; null when there is no room.
static const char* with_kernel_arg_info(::av::WireArena& arena, const char* options) {
    static constexpr char flag[] = " -cl-kernel-arg-info";
    const size_t length = options != nullptr ? __builtin_strlen(options) : 0;
    char* joined = arena.allocate_chars(length + sizeof flag);
    if (joined == nullptr) return nullptr;
    if (length != 0) __builtin_memcpy(joined, options, length);
    __builtin_memcpy(joined + length, flag, sizeof flag);
    return joined;
}

// CL_KERNEL_EXEC_INFO_SVM_PTRS' value: a whole number of addresses, each
// inside an allocation or a mapping this server answered, handed on as
// the pointers they resolve to, in the arena.
static bool resolve_svm_pointers(ServerDispatch& s, ::av::WireArena& arena, size_t size,
                                 const void*& value) {
    if (value == nullptr || size == 0 || size % sizeof(uint64_t) != 0) return false;
    const size_t count = size / sizeof(uint64_t);
    auto* resolved = arena.allocate<void*>(count);
    if (resolved == nullptr) return false;
    for (size_t i = 0; i < count; ++i) {
        uint64_t address = 0;
        __builtin_memcpy(&address, static_cast<const uint8_t*>(value) + i * sizeof address, sizeof address);
        resolved[i] = s.svm_for(address, 1);
        if (resolved[i] == nullptr) return false;
    }
    value = resolved;
    return true;
}

static bool ends_with(const char* text, size_t length, const char* suffix) {
    const size_t tail = __builtin_strlen(suffix);
    return length >= tail && __builtin_memcmp(text + length - tail, suffix, tail) == 0;
}

// Asked of the ICD, into the arena like every answer it writes. unknown
// when it cannot say - a program from a binary or IL, or an ICD that keeps
// no argument information.
static KernelArgument classified(cl_kernel_arg_address_qualifier address,
                                 cl_kernel_arg_type_qualifier qualifier, const char* name) {
    const size_t used = __builtin_strlen(name);
    if (address == CL_KERNEL_ARG_ADDRESS_LOCAL) return KernelArgument::local;
    if (address == CL_KERNEL_ARG_ADDRESS_GLOBAL || address == CL_KERNEL_ARG_ADDRESS_CONSTANT ||
        ends_with(name, used, "*") || (qualifier & CL_KERNEL_ARG_TYPE_PIPE) != 0)
        return KernelArgument::memory;
    static constexpr const char* images[] = {"image1d_t", "image1d_array_t", "image1d_buffer_t",
                                             "image2d_t", "image2d_array_t", "image3d_t"};
    for (const char* image : images) {
        if (ends_with(name, used, image)) return KernelArgument::image;
    }
    return ends_with(name, used, "sampler_t") ? KernelArgument::sampler : KernelArgument::data;
}

// Asked of the ICD, into the arena like every answer it writes; false only
// when the arena has no room to ask. Local, global and constant space say
// what an argument is alone; a private one needs its type's name, which a
// host need not keep for a program from IL or from a binary. What the host
// cannot say the server may know otherwise (declared_argument), and what
// neither can say is unknown.
static bool kernel_argument(ServerDispatch& s, ::av::WireArena& arena, cl_kernel kernel,
                            cl_uint index, KernelArgument& argument) {
    argument = KernelArgument::unknown;
    auto* address = arena.allocate<cl_kernel_arg_address_qualifier>(1);
    if (address == nullptr) { return false; }
    auto* qualifier = arena.allocate<cl_kernel_arg_type_qualifier>(1);
    if (qualifier == nullptr) { return false; }
    auto* length = arena.allocate<size_t>(1);
    if (length == nullptr) { return false; }
    const bool addressed = s.on_clGetKernelArgInfo(kernel, index, CL_KERNEL_ARG_ADDRESS_QUALIFIER,
                                                   sizeof *address, address, nullptr) == CL_SUCCESS;
    if (addressed && *address == CL_KERNEL_ARG_ADDRESS_LOCAL) {
        argument = KernelArgument::local;
        return true;
    }
    if (addressed && (*address == CL_KERNEL_ARG_ADDRESS_GLOBAL || *address == CL_KERNEL_ARG_ADDRESS_CONSTANT)) {
        argument = KernelArgument::memory;
        return true;
    }
    if (addressed &&
        s.on_clGetKernelArgInfo(kernel, index, CL_KERNEL_ARG_TYPE_QUALIFIER, sizeof *qualifier,
                                qualifier, nullptr) == CL_SUCCESS &&
        s.on_clGetKernelArgInfo(kernel, index, CL_KERNEL_ARG_TYPE_NAME, 0, nullptr,
                                length) == CL_SUCCESS) {
        char* name = arena.allocate_chars(*length + 1);
        if (name == nullptr) { return false; }
        if (s.on_clGetKernelArgInfo(kernel, index, CL_KERNEL_ARG_TYPE_NAME, *length, name,
                                    nullptr) == CL_SUCCESS) {
            name[*length] = '\0';
            argument = classified(*address, *qualifier, name);
            return true;
        }
    }
    if (!s.declared_argument(kernel, index, argument)) argument = KernelArgument::unknown;
    return true;
}

// A kernel argument the ICD will read as an object must be one this
// server made, of that kind, and is handed over as the object resolved;
// one it cannot classify may not be the width of an object at all. A
// null buffer is the API's own, and a local argument has no value. False
// only when the arena has no room; `accepted` says whether the host may be
// handed the argument, and one that may not is answered CL_INVALID_ARG_VALUE
// - a well-formed command naming an argument this server cannot vouch for.
static bool resolve_kernel_argument(ServerDispatch& s, ::av::WireArena& arena, cl_kernel kernel,
                                    cl_uint index, size_t size, const void*& value, bool& accepted) {
    KernelArgument argument = KernelArgument::unknown;
    accepted = false;
    if (!kernel_argument(s, arena, kernel, index, argument)) return false;
    if (argument == KernelArgument::data) accepted = true;
    if (argument == KernelArgument::local) accepted = value == nullptr;
    if (argument == KernelArgument::unknown) accepted = value == nullptr || size != sizeof(cl_mem);
    if (argument == KernelArgument::data || argument == KernelArgument::local ||
        argument == KernelArgument::unknown) return true;
    if (value == nullptr || size != sizeof(void*)) {
        accepted = value == nullptr && argument == KernelArgument::memory;
        return true;
    }
    uint64_t wire_handle = 0;
    __builtin_memcpy(&wire_handle, value, sizeof wire_handle);
    if (wire_handle == 0) {
        accepted = argument == KernelArgument::memory;
        return true;
    }
    void* object = s.handle_for(argument == KernelArgument::sampler ? HandleKind::sampler : HandleKind::mem,
                                wire_handle);
    auto* resolved = arena.allocate<void*>(1);
    if (resolved == nullptr) { return false; }
    if (object == nullptr) return true;
    *resolved = object;
    value = resolved;
    accepted = true;
    return true;
}

static bool decode_clGetPlatformIDs(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetPlatformIDs");
    (void)r;
    (void)reply;
    (void)arena;
    cl_uint num_entries{};
    bool platforms_wanted = false;
    bool num_platforms_wanted = false;
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_entries = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        platforms_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        num_platforms_wanted = wire_wanted != 0;
    }
    cl_platform_id* platforms = nullptr;
    if (platforms_wanted) {
        platforms = arena.allocate<cl_platform_id>(static_cast<size_t>(num_entries) + 1);
        if (platforms == nullptr) return false;
    }
    cl_uint num_platforms_storage{};
    cl_uint* num_platforms = (num_platforms_wanted || platforms_wanted) ? &num_platforms_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((platforms_wanted ? static_cast<uint64_t>(num_entries) : 0u))) ? s.on_clGetPlatformIDs(num_entries, platforms, num_platforms) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (platforms_wanted) {
        const uint64_t wire_produced = (ret == CL_SUCCESS) ? static_cast<uint64_t>(num_platforms_storage) : 0u;
        const uint64_t wire_sent = wire_produced < static_cast<uint64_t>(num_entries)
            ? wire_produced : static_cast<uint64_t>(num_entries);
        reply.write_varint(wire_sent);
        for (uint64_t i = 0; i < wire_sent; ++i) {
            if (platforms[i] != nullptr) s.made(HandleKind::platform, platforms[i]);
            reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(platforms[i])));
        }
    }
    if (num_platforms_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(num_platforms_storage) : 0u);
    }
    return true;
}

static bool decode_clGetPlatformInfo(ServerDispatch& s, ::av::WireReader& r,
                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetPlatformInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_platform_id platform = nullptr;
    cl_platform_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          platform = static_cast<cl_platform_id>(s.handle_for(HandleKind::platform, wire_handle));
          if (platform == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_platform_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetPlatformInfo(platform, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clGetDeviceIDs(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetDeviceIDs");
    (void)r;
    (void)reply;
    (void)arena;
    cl_platform_id platform = nullptr;
    cl_device_type device_type{};
    cl_uint num_entries{};
    bool devices_wanted = false;
    bool num_devices_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          platform = static_cast<cl_platform_id>(s.handle_for(HandleKind::platform, wire_handle));
          if (platform == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        device_type = static_cast<cl_device_type>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_entries = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        devices_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        num_devices_wanted = wire_wanted != 0;
    }
    cl_device_id* devices = nullptr;
    if (devices_wanted) {
        devices = arena.allocate<cl_device_id>(static_cast<size_t>(num_entries) + 1);
        if (devices == nullptr) return false;
    }
    cl_uint num_devices_storage{};
    cl_uint* num_devices = (num_devices_wanted || devices_wanted) ? &num_devices_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((devices_wanted ? static_cast<uint64_t>(num_entries) : 0u))) ? s.on_clGetDeviceIDs(platform, device_type, num_entries, devices, num_devices) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (devices_wanted) {
        const uint64_t wire_produced = (ret == CL_SUCCESS) ? static_cast<uint64_t>(num_devices_storage) : 0u;
        const uint64_t wire_sent = wire_produced < static_cast<uint64_t>(num_entries)
            ? wire_produced : static_cast<uint64_t>(num_entries);
        reply.write_varint(wire_sent);
        for (uint64_t i = 0; i < wire_sent; ++i) {
            if (devices[i] != nullptr) s.made(HandleKind::device, devices[i]);
            reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(devices[i])));
        }
    }
    if (num_devices_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(num_devices_storage) : 0u);
    }
    return true;
}

static bool decode_clGetDeviceInfo(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetDeviceInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_device_id device = nullptr;
    cl_device_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (device == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_device_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetDeviceInfo(device, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clCreateContext(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateContext");
    (void)r;
    (void)reply;
    (void)arena;
    const cl_context_properties* properties = nullptr;
    cl_uint num_devices{};
    const cl_device_id* devices = nullptr;
    uint64_t devices_count = 0;
    bool errcode_ret_wanted = false;
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_context_properties>(static_cast<size_t>(wire_count) + 2);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_context_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
            wire_items[wire_count + 1] = 0;
            for (uint64_t i = 0; wire_items[i] != 0; i += 2) {
                if (wire_items[i] == CL_CONTEXT_PLATFORM) {
                    const uint64_t wire_handle = static_cast<uint64_t>(wire_items[i + 1]);
                    void* wire_platform = s.handle_for(HandleKind::platform, wire_handle);
                    if (wire_platform == nullptr) return false;
                    wire_items[i + 1] = static_cast<cl_context_properties>(reinterpret_cast<intptr_t>(wire_platform));
                } else if (wire_items[i] != CL_CONTEXT_INTEROP_USER_SYNC) {
                    return false;
                }
            }
            properties = wire_items;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_devices = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_device_id>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            devices = wire_items;
            devices_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (devices != nullptr && devices_count < static_cast<uint64_t>(num_devices)) return false;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_context ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateContext(properties, num_devices, devices, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::context, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateContextFromType(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateContextFromType");
    (void)r;
    (void)reply;
    (void)arena;
    const cl_context_properties* properties = nullptr;
    cl_device_type device_type{};
    bool errcode_ret_wanted = false;
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_context_properties>(static_cast<size_t>(wire_count) + 2);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_context_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
            wire_items[wire_count + 1] = 0;
            for (uint64_t i = 0; wire_items[i] != 0; i += 2) {
                if (wire_items[i] == CL_CONTEXT_PLATFORM) {
                    const uint64_t wire_handle = static_cast<uint64_t>(wire_items[i + 1]);
                    void* wire_platform = s.handle_for(HandleKind::platform, wire_handle);
                    if (wire_platform == nullptr) return false;
                    wire_items[i + 1] = static_cast<cl_context_properties>(reinterpret_cast<intptr_t>(wire_platform));
                } else if (wire_items[i] != CL_CONTEXT_INTEROP_USER_SYNC) {
                    return false;
                }
            }
            properties = wire_items;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        device_type = static_cast<cl_device_type>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_context ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateContextFromType(properties, device_type, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::context, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clRetainContext(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clRetainContext");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    const cl_int ret = s.on_clRetainContext(context);
    if (ret == CL_SUCCESS) s.retained(HandleKind::context, context);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseContext(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clReleaseContext");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    const cl_int ret = s.on_clReleaseContext(context);
    if (ret == CL_SUCCESS) s.released(HandleKind::context, context);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetContextInfo(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetContextInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_context_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_context_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetContextInfo(context, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clCreateCommandQueue(ServerDispatch& s, ::av::WireReader& r,
                                       ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateCommandQueue");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_device_id device = nullptr;
    cl_command_queue_properties properties{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (device == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        properties = static_cast<cl_command_queue_properties>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_command_queue ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateCommandQueue(context, device, properties, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::command_queue, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateCommandQueueWithProperties(ServerDispatch& s, ::av::WireReader& r,
                                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateCommandQueueWithProperties");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_device_id device = nullptr;
    const cl_queue_properties* properties = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (device == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_queue_properties>(static_cast<size_t>(wire_count) + 2);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_queue_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
            wire_items[wire_count + 1] = 0;
            properties = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_command_queue ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateCommandQueueWithProperties(context, device, properties, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::command_queue, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clRetainCommandQueue(ServerDispatch& s, ::av::WireReader& r,
                                       ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clRetainCommandQueue");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    const cl_int ret = s.on_clRetainCommandQueue(command_queue);
    if (ret == CL_SUCCESS) s.retained(HandleKind::command_queue, command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseCommandQueue(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clReleaseCommandQueue");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    const cl_int ret = s.on_clReleaseCommandQueue(command_queue);
    if (ret == CL_SUCCESS) s.released(HandleKind::command_queue, command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetCommandQueueInfo(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetCommandQueueInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_command_queue_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_command_queue_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetCommandQueueInfo(command_queue, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clFlush(ServerDispatch& s, ::av::WireReader& r,
                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clFlush");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    const cl_int ret = s.on_clFlush(command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clFinish(ServerDispatch& s, ::av::WireReader& r,
                           ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clFinish");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    const cl_int ret = s.on_clFinish(command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clCreateBuffer(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateBuffer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    size_t size{};
    void* host_ptr = nullptr;
    uint64_t host_ptr_count = 0;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            host_ptr = reinterpret_cast<void*>(wire_bytes);
            host_ptr_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (host_ptr != nullptr && host_ptr_count < static_cast<uint64_t>(size)) return false;
    if ((flags & CL_MEM_USE_HOST_PTR) != 0) return false;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateBuffer(context, flags, size, host_ptr, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateSubBuffer(ServerDispatch& s, ::av::WireReader& r,
                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateSubBuffer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_mem buffer = nullptr;
    cl_mem_flags flags{};
    cl_buffer_create_type buffer_create_type{};
    const cl_buffer_region* buffer_create_info = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (buffer == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        buffer_create_type = static_cast<cl_buffer_create_type>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            auto* wire_item = arena.allocate<cl_buffer_region>(1);
            if (wire_item == nullptr) return false;
            if (!r.read(wire_item, sizeof(*wire_item))) return false;
            buffer_create_info = wire_item;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateSubBuffer(buffer, flags, buffer_create_type, buffer_create_info, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateImage(ServerDispatch& s, ::av::WireReader& r,
                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateImage");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    const cl_image_format* image_format = nullptr;
    const cl_image_desc* image_desc = nullptr;
    void* host_ptr = nullptr;
    uint64_t host_ptr_count = 0;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            auto* wire_item = arena.allocate<cl_image_format>(1);
            if (wire_item == nullptr) return false;
            if (!r.read(wire_item, sizeof(*wire_item))) return false;
            image_format = wire_item;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            auto* wire_item = arena.allocate<cl_image_desc>(1);
            if (wire_item == nullptr) return false;
            if (!r.read(wire_item, sizeof(*wire_item))) return false;
            if (wire_item->buffer != nullptr) {
                const uint64_t wire_handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(wire_item->buffer));
                wire_item->buffer = static_cast<decltype(wire_item->buffer)>(s.handle_for(HandleKind::mem, wire_handle));
                if (wire_item->buffer == nullptr) return false;
            }
            image_desc = wire_item;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            host_ptr = reinterpret_cast<void*>(wire_bytes);
            host_ptr_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (host_ptr != nullptr) {
        uint64_t wire_needed = 0;
        if (image_desc == nullptr || !image_host_bytes(image_format, image_desc->image_type, image_desc->image_width, image_desc->image_height,
                                 image_desc->image_depth, image_desc->image_array_size, image_desc->image_row_pitch, image_desc->image_slice_pitch, wire_needed) ||
            host_ptr_count < wire_needed) return false;
    }
    if ((flags & CL_MEM_USE_HOST_PTR) != 0) return false;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateImage(context, flags, image_format, image_desc, host_ptr, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clRetainMemObject(ServerDispatch& s, ::av::WireReader& r,
                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clRetainMemObject");
    (void)r;
    (void)reply;
    (void)arena;
    cl_mem memobj = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          memobj = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (memobj == nullptr) return false;
      } }
    const cl_int ret = s.on_clRetainMemObject(memobj);
    if (ret == CL_SUCCESS) s.retained(HandleKind::mem, memobj);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseMemObject(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clReleaseMemObject");
    (void)r;
    (void)reply;
    (void)arena;
    cl_mem memobj = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          memobj = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (memobj == nullptr) return false;
      } }
    const cl_int ret = s.on_clReleaseMemObject(memobj);
    if (ret == CL_SUCCESS) s.released(HandleKind::mem, memobj);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetMemObjectInfo(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetMemObjectInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_mem memobj = nullptr;
    cl_mem_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          memobj = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (memobj == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_mem_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetMemObjectInfo(memobj, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clGetImageInfo(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetImageInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_mem image = nullptr;
    cl_image_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          image = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (image == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_image_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetImageInfo(image, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clGetSupportedImageFormats(ServerDispatch& s, ::av::WireReader& r,
                                             ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetSupportedImageFormats");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    cl_mem_object_type image_type{};
    cl_uint num_entries{};
    bool image_formats_wanted = false;
    size_t image_formats_capacity = 0;
    bool num_image_formats_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        image_type = static_cast<cl_mem_object_type>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_entries = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        image_formats_wanted = wire_wanted != 0;
        if (image_formats_wanted) {
            uint64_t cap = 0;
            if (!r.read_varint(cap)) return false;
            image_formats_capacity = static_cast<size_t>(cap);
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        num_image_formats_wanted = wire_wanted != 0;
    }
    if (image_formats_wanted && static_cast<uint64_t>(num_entries) * sizeof(cl_image_format) > static_cast<uint64_t>(image_formats_capacity)) return false;
    cl_image_format* image_formats = nullptr;
    if (image_formats_wanted) {
        image_formats = reinterpret_cast<cl_image_format*>(arena.allocate<uint8_t>(static_cast<size_t>(image_formats_capacity) + 1));
        if (image_formats == nullptr) return false;
    }
    cl_uint num_image_formats_storage{};
    cl_uint* num_image_formats = (num_image_formats_wanted || image_formats_wanted) ? &num_image_formats_storage : nullptr;
    const cl_int ret = s.on_clGetSupportedImageFormats(context, flags, image_type, num_entries, image_formats, num_image_formats);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (image_formats_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(num_image_formats_storage) * sizeof(cl_image_format) : 0u;
        if (wire_written > static_cast<uint64_t>(image_formats_capacity))
            wire_written = static_cast<uint64_t>(image_formats_capacity);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(image_formats, static_cast<size_t>(wire_written));
    }
    if (num_image_formats_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(num_image_formats_storage) : 0u);
    }
    return true;
}

static bool decode_clEnqueueReadBuffer(ServerDispatch& s, ::av::WireReader& r,
                                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueReadBuffer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem buffer = nullptr;
    cl_bool blocking_read{};
    size_t offset{};
    size_t size{};
    bool ptr_wanted = false;
    size_t ptr_capacity = 0;
    void* ptr_lent = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (buffer == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        blocking_read = static_cast<cl_bool>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        offset = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        ptr_wanted = wire_wanted != 0;
        if (ptr_wanted) {
            uint64_t cap = 0;
            if (!r.read_varint(cap)) return false;
            ptr_capacity = static_cast<size_t>(cap);
        }
        if (wire_wanted == 2) {
            uint64_t wire_offset = 0;
            if (!r.read_varint(wire_offset)) return false;
            ptr_lent = s.lent_for(wire_offset, static_cast<uint64_t>(ptr_capacity));
            if (ptr_lent == nullptr) return false;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (ptr_wanted && static_cast<uint64_t>(size) > static_cast<uint64_t>(ptr_capacity)) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    void* ptr = nullptr;
    if (ptr_lent != nullptr) {
        ptr = static_cast<void*>(ptr_lent);
    } else if (ptr_wanted) {
        ptr = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(ptr_capacity) + 1));
        if (ptr == nullptr) return false;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueReadBuffer(command_queue, buffer, blocking_read, offset, size, ptr, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (ptr_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(ptr_capacity) : 0u;
        if (wire_written > static_cast<uint64_t>(ptr_capacity))
            wire_written = static_cast<uint64_t>(ptr_capacity);
        reply.write_varint(wire_written);
        if (wire_written != 0 && ptr_lent == nullptr) reply.write(ptr, static_cast<size_t>(wire_written));
    }
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueWriteBuffer(ServerDispatch& s, ::av::WireReader& r,
                                       ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueWriteBuffer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem buffer = nullptr;
    cl_bool blocking_write{};
    size_t offset{};
    size_t size{};
    const void* ptr = nullptr;
    uint64_t ptr_count = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (buffer == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        blocking_write = static_cast<cl_bool>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        offset = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present == 2) {
            uint64_t wire_count = 0;
            uint64_t wire_offset = 0;
            if (!r.read_varint(wire_count) || !r.read_varint(wire_offset)) return false;
            void* wire_bytes = s.lent_for(wire_offset, wire_count);
            if (wire_bytes == nullptr) return false;
            ptr = static_cast<const void*>(wire_bytes);
            ptr_count = wire_count;
        } else if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            ptr = reinterpret_cast<const void*>(wire_bytes);
            ptr_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (ptr != nullptr && ptr_count < static_cast<uint64_t>(size)) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueWriteBuffer(command_queue, buffer, blocking_write, offset, size, ptr, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueCopyBuffer(ServerDispatch& s, ::av::WireReader& r,
                                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueCopyBuffer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem src_buffer = nullptr;
    cl_mem dst_buffer = nullptr;
    size_t src_offset{};
    size_t dst_offset{};
    size_t size{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          src_buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (src_buffer == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          dst_buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (dst_buffer == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        src_offset = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        dst_offset = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        size = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueCopyBuffer(command_queue, src_buffer, dst_buffer, src_offset, dst_offset, size, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueFillBuffer(ServerDispatch& s, ::av::WireReader& r,
                                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueFillBuffer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem buffer = nullptr;
    const void* pattern = nullptr;
    uint64_t pattern_count = 0;
    size_t pattern_size{};
    size_t offset{};
    size_t size{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (buffer == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            pattern = reinterpret_cast<const void*>(wire_bytes);
            pattern_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        pattern_size = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        offset = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        size = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (pattern != nullptr && pattern_count < static_cast<uint64_t>(pattern_size)) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueFillBuffer(command_queue, buffer, pattern, pattern_size, offset, size, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueReadImage(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueReadImage");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem image = nullptr;
    cl_bool blocking_read{};
    const size_t* origin = nullptr;
    uint64_t origin_count = 0;
    const size_t* region = nullptr;
    uint64_t region_count = 0;
    size_t row_pitch{};
    size_t slice_pitch{};
    bool ptr_wanted = false;
    size_t ptr_capacity = 0;
    void* ptr_lent = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          image = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (image == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        blocking_read = static_cast<cl_bool>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            origin = wire_items;
            origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            region = wire_items;
            region_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        row_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        slice_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        ptr_wanted = wire_wanted != 0;
        if (ptr_wanted) {
            uint64_t cap = 0;
            if (!r.read_varint(cap)) return false;
            ptr_capacity = static_cast<size_t>(cap);
        }
        if (wire_wanted == 2) {
            uint64_t wire_offset = 0;
            if (!r.read_varint(wire_offset)) return false;
            ptr_lent = s.lent_for(wire_offset, static_cast<uint64_t>(ptr_capacity));
            if (ptr_lent == nullptr) return false;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (origin != nullptr && origin_count < 3u) return false;
    if (region != nullptr && region_count < 3u) return false;
    {
        uint64_t wire_needed = 0;
        if (ptr_wanted && (!image_transfer_bytes(s, arena, image, region, row_pitch, slice_pitch, wire_needed) ||
                         static_cast<uint64_t>(ptr_capacity) < wire_needed)) return false;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    void* ptr = nullptr;
    if (ptr_lent != nullptr) {
        ptr = static_cast<void*>(ptr_lent);
    } else if (ptr_wanted) {
        ptr = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(ptr_capacity) + 1));
        if (ptr == nullptr) return false;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueReadImage(command_queue, image, blocking_read, origin, region, row_pitch, slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (ptr_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(ptr_capacity) : 0u;
        if (wire_written > static_cast<uint64_t>(ptr_capacity))
            wire_written = static_cast<uint64_t>(ptr_capacity);
        reply.write_varint(wire_written);
        if (wire_written != 0 && ptr_lent == nullptr) reply.write(ptr, static_cast<size_t>(wire_written));
    }
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueWriteImage(ServerDispatch& s, ::av::WireReader& r,
                                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueWriteImage");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem image = nullptr;
    cl_bool blocking_write{};
    const size_t* origin = nullptr;
    uint64_t origin_count = 0;
    const size_t* region = nullptr;
    uint64_t region_count = 0;
    size_t input_row_pitch{};
    size_t input_slice_pitch{};
    const void* ptr = nullptr;
    uint64_t ptr_count = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          image = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (image == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        blocking_write = static_cast<cl_bool>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            origin = wire_items;
            origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            region = wire_items;
            region_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        input_row_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        input_slice_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present == 2) {
            uint64_t wire_count = 0;
            uint64_t wire_offset = 0;
            if (!r.read_varint(wire_count) || !r.read_varint(wire_offset)) return false;
            void* wire_bytes = s.lent_for(wire_offset, wire_count);
            if (wire_bytes == nullptr) return false;
            ptr = static_cast<const void*>(wire_bytes);
            ptr_count = wire_count;
        } else if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            ptr = reinterpret_cast<const void*>(wire_bytes);
            ptr_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (origin != nullptr && origin_count < 3u) return false;
    if (region != nullptr && region_count < 3u) return false;
    {
        uint64_t wire_needed = 0;
        if (ptr != nullptr && (!image_transfer_bytes(s, arena, image, region, input_row_pitch, input_slice_pitch, wire_needed) ||
                         ptr_count < wire_needed)) return false;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueWriteImage(command_queue, image, blocking_write, origin, region, input_row_pitch, input_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueCopyImage(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueCopyImage");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem src_image = nullptr;
    cl_mem dst_image = nullptr;
    const size_t* src_origin = nullptr;
    uint64_t src_origin_count = 0;
    const size_t* dst_origin = nullptr;
    uint64_t dst_origin_count = 0;
    const size_t* region = nullptr;
    uint64_t region_count = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          src_image = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (src_image == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          dst_image = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (dst_image == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            src_origin = wire_items;
            src_origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            dst_origin = wire_items;
            dst_origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            region = wire_items;
            region_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (src_origin != nullptr && src_origin_count < 3u) return false;
    if (dst_origin != nullptr && dst_origin_count < 3u) return false;
    if (region != nullptr && region_count < 3u) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueCopyImage(command_queue, src_image, dst_image, src_origin, dst_origin, region, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueCopyImageToBuffer(ServerDispatch& s, ::av::WireReader& r,
                                             ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueCopyImageToBuffer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem src_image = nullptr;
    cl_mem dst_buffer = nullptr;
    const size_t* src_origin = nullptr;
    uint64_t src_origin_count = 0;
    const size_t* region = nullptr;
    uint64_t region_count = 0;
    size_t dst_offset{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          src_image = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (src_image == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          dst_buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (dst_buffer == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            src_origin = wire_items;
            src_origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            region = wire_items;
            region_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        dst_offset = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (src_origin != nullptr && src_origin_count < 3u) return false;
    if (region != nullptr && region_count < 3u) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueCopyImageToBuffer(command_queue, src_image, dst_buffer, src_origin, region, dst_offset, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueCopyBufferToImage(ServerDispatch& s, ::av::WireReader& r,
                                             ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueCopyBufferToImage");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem src_buffer = nullptr;
    cl_mem dst_image = nullptr;
    size_t src_offset{};
    const size_t* dst_origin = nullptr;
    uint64_t dst_origin_count = 0;
    const size_t* region = nullptr;
    uint64_t region_count = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          src_buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (src_buffer == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          dst_image = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (dst_image == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        src_offset = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            dst_origin = wire_items;
            dst_origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            region = wire_items;
            region_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (dst_origin != nullptr && dst_origin_count < 3u) return false;
    if (region != nullptr && region_count < 3u) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueCopyBufferToImage(command_queue, src_buffer, dst_image, src_offset, dst_origin, region, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clCreateProgramWithSource(ServerDispatch& s, ::av::WireReader& r,
                                            ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateProgramWithSource");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_uint count{};
    const char** strings = nullptr;
    uint64_t strings_count = 0;
    const size_t* strings_lengths = nullptr;
    const size_t* lengths = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        count = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto** wire_items = arena.allocate<const char*>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            auto* wire_lengths = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_lengths == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_len = 0;
                if (!r.read_varint(wire_len)) return false;
                char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);
                if (wire_text == nullptr) return false;
                if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;
                wire_text[wire_len] = '\0';
                wire_items[i] = reinterpret_cast<const char*>(wire_text);
                wire_lengths[i] = static_cast<size_t>(wire_len);
            }
            strings = wire_items;
            strings_count = wire_count;
            strings_lengths = wire_lengths;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (strings != nullptr && strings_count < static_cast<uint64_t>(count)) return false;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    lengths = strings_lengths;
    cl_program ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateProgramWithSource(context, count, strings, lengths, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::program, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateProgramWithBinary(ServerDispatch& s, ::av::WireReader& r,
                                            ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateProgramWithBinary");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_uint num_devices{};
    const cl_device_id* device_list = nullptr;
    uint64_t device_list_count = 0;
    const size_t* lengths = nullptr;
    const unsigned char** binaries = nullptr;
    uint64_t binaries_count = 0;
    const size_t* binaries_lengths = nullptr;
    bool binary_status_wanted = false;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_devices = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_device_id>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            device_list = wire_items;
            device_list_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto** wire_items = arena.allocate<const unsigned char*>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            auto* wire_lengths = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_lengths == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_len = 0;
                if (!r.read_varint(wire_len)) return false;
                char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);
                if (wire_text == nullptr) return false;
                if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;
                wire_text[wire_len] = '\0';
                wire_items[i] = reinterpret_cast<const unsigned char*>(wire_text);
                wire_lengths[i] = static_cast<size_t>(wire_len);
            }
            binaries = wire_items;
            binaries_count = wire_count;
            binaries_lengths = wire_lengths;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        binary_status_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (device_list != nullptr && device_list_count < static_cast<uint64_t>(num_devices)) return false;
    if (binaries != nullptr && binaries_count < static_cast<uint64_t>(num_devices)) return false;
    cl_int binary_status_storage{};
    cl_int* binary_status = binary_status_wanted ? &binary_status_storage : nullptr;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    lengths = binaries_lengths;
    cl_program ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateProgramWithBinary(context, num_devices, device_list, lengths, binaries, binary_status, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::program, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (binary_status_wanted) {
        reply.write_varint(static_cast<uint64_t>(binary_status_storage));
    }
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clRetainProgram(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clRetainProgram");
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          program = static_cast<cl_program>(s.handle_for(HandleKind::program, wire_handle));
          if (program == nullptr) return false;
      } }
    const cl_int ret = s.on_clRetainProgram(program);
    if (ret == CL_SUCCESS) s.retained(HandleKind::program, program);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseProgram(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clReleaseProgram");
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          program = static_cast<cl_program>(s.handle_for(HandleKind::program, wire_handle));
          if (program == nullptr) return false;
      } }
    const cl_int ret = s.on_clReleaseProgram(program);
    if (ret == CL_SUCCESS) s.released(HandleKind::program, program);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clBuildProgram(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clBuildProgram");
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    cl_uint num_devices{};
    const cl_device_id* device_list = nullptr;
    uint64_t device_list_count = 0;
    const char* options = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          program = static_cast<cl_program>(s.handle_for(HandleKind::program, wire_handle));
          if (program == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_devices = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_device_id>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            device_list = wire_items;
            device_list_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_len = 0;
            if (!r.read_varint(wire_len)) return false;
            char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);
            if (wire_text == nullptr) return false;
            if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;
            wire_text[wire_len] = '\0';
            options = wire_text;
        }
    }
    if (device_list != nullptr && device_list_count < static_cast<uint64_t>(num_devices)) return false;
    options = with_kernel_arg_info(arena, options);
    if (options == nullptr) return false;
    const cl_int ret = s.on_clBuildProgram(program, num_devices, device_list, options);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetProgramInfo(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetProgramInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    cl_program_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          program = static_cast<cl_program>(s.handle_for(HandleKind::program, wire_handle));
          if (program == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_program_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetProgramInfo(program, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clGetProgramBuildInfo(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetProgramBuildInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    cl_device_id device = nullptr;
    cl_program_build_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          program = static_cast<cl_program>(s.handle_for(HandleKind::program, wire_handle));
          if (program == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (device == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_program_build_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetProgramBuildInfo(program, device, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clUnloadPlatformCompiler(ServerDispatch& s, ::av::WireReader& r,
                                           ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clUnloadPlatformCompiler");
    (void)r;
    (void)reply;
    (void)arena;
    cl_platform_id platform = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          platform = static_cast<cl_platform_id>(s.handle_for(HandleKind::platform, wire_handle));
          if (platform == nullptr) return false;
      } }
    const cl_int ret = s.on_clUnloadPlatformCompiler(platform);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clCreateKernel(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateKernel");
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    const char* kernel_name = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          program = static_cast<cl_program>(s.handle_for(HandleKind::program, wire_handle));
          if (program == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_len = 0;
            if (!r.read_varint(wire_len)) return false;
            char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);
            if (wire_text == nullptr) return false;
            if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;
            wire_text[wire_len] = '\0';
            kernel_name = wire_text;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_kernel ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateKernel(program, kernel_name, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::kernel, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateKernelsInProgram(ServerDispatch& s, ::av::WireReader& r,
                                           ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateKernelsInProgram");
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    cl_uint num_kernels{};
    bool kernels_wanted = false;
    bool num_kernels_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          program = static_cast<cl_program>(s.handle_for(HandleKind::program, wire_handle));
          if (program == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_kernels = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        kernels_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        num_kernels_ret_wanted = wire_wanted != 0;
    }
    cl_kernel* kernels = nullptr;
    if (kernels_wanted) {
        kernels = arena.allocate<cl_kernel>(static_cast<size_t>(num_kernels) + 1);
        if (kernels == nullptr) return false;
    }
    cl_uint num_kernels_ret_storage{};
    cl_uint* num_kernels_ret = (num_kernels_ret_wanted || kernels_wanted) ? &num_kernels_ret_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((kernels_wanted ? static_cast<uint64_t>(num_kernels) : 0u))) ? s.on_clCreateKernelsInProgram(program, num_kernels, kernels, num_kernels_ret) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (kernels_wanted) {
        const uint64_t wire_produced = (ret == CL_SUCCESS) ? static_cast<uint64_t>(num_kernels_ret_storage) : 0u;
        const uint64_t wire_sent = wire_produced < static_cast<uint64_t>(num_kernels)
            ? wire_produced : static_cast<uint64_t>(num_kernels);
        reply.write_varint(wire_sent);
        for (uint64_t i = 0; i < wire_sent; ++i) {
            if (kernels[i] != nullptr) s.made(HandleKind::kernel, kernels[i]);
            reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(kernels[i])));
        }
    }
    if (num_kernels_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(num_kernels_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clRetainKernel(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clRetainKernel");
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (kernel == nullptr) return false;
      } }
    const cl_int ret = s.on_clRetainKernel(kernel);
    if (ret == CL_SUCCESS) s.retained(HandleKind::kernel, kernel);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseKernel(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clReleaseKernel");
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (kernel == nullptr) return false;
      } }
    const cl_int ret = s.on_clReleaseKernel(kernel);
    if (ret == CL_SUCCESS) s.released(HandleKind::kernel, kernel);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clSetKernelArg(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clSetKernelArg");
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    cl_uint arg_index{};
    size_t arg_size{};
    const void* arg_value = nullptr;
    uint64_t arg_value_count = 0;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (kernel == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        arg_index = static_cast<cl_uint>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        arg_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            arg_value = reinterpret_cast<const void*>(wire_bytes);
            arg_value_count = wire_count;
        }
    }
    if (arg_value != nullptr && arg_value_count < static_cast<uint64_t>(arg_size)) return false;
    bool accepted = false;
    if (!resolve_kernel_argument(s, arena, kernel, arg_index, arg_size, arg_value, accepted)) return false;
    if (!accepted) {
        reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(CL_INVALID_ARG_VALUE)));
        return true;
    }
    const cl_int ret = s.on_clSetKernelArg(kernel, arg_index, arg_size, arg_value);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetKernelInfo(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetKernelInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    cl_kernel_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (kernel == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_kernel_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetKernelInfo(kernel, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clGetKernelArgInfo(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetKernelArgInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    cl_uint arg_index{};
    cl_kernel_arg_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (kernel == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        arg_index = static_cast<cl_uint>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_kernel_arg_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetKernelArgInfo(kernel, arg_index, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clGetKernelWorkGroupInfo(ServerDispatch& s, ::av::WireReader& r,
                                           ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetKernelWorkGroupInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    cl_device_id device = nullptr;
    cl_kernel_work_group_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (kernel == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (device == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_kernel_work_group_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetKernelWorkGroupInfo(kernel, device, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clEnqueueNDRangeKernel(ServerDispatch& s, ::av::WireReader& r,
                                         ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueNDRangeKernel");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_kernel kernel = nullptr;
    cl_uint work_dim{};
    const size_t* global_work_offset = nullptr;
    uint64_t global_work_offset_count = 0;
    const size_t* global_work_size = nullptr;
    uint64_t global_work_size_count = 0;
    const size_t* local_work_size = nullptr;
    uint64_t local_work_size_count = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (kernel == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        work_dim = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            global_work_offset = wire_items;
            global_work_offset_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            global_work_size = wire_items;
            global_work_size_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            local_work_size = wire_items;
            local_work_size_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (global_work_offset != nullptr && global_work_offset_count < static_cast<uint64_t>(work_dim)) return false;
    if (global_work_size != nullptr && global_work_size_count < static_cast<uint64_t>(work_dim)) return false;
    if (local_work_size != nullptr && local_work_size_count < static_cast<uint64_t>(work_dim)) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueNDRangeKernel(command_queue, kernel, work_dim, global_work_offset, global_work_size, local_work_size, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueMarkerWithWaitList(ServerDispatch& s, ::av::WireReader& r,
                                              ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueMarkerWithWaitList");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueMarkerWithWaitList(command_queue, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueBarrierWithWaitList(ServerDispatch& s, ::av::WireReader& r,
                                               ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueBarrierWithWaitList");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueBarrierWithWaitList(command_queue, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clWaitForEvents(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clWaitForEvents");
    (void)r;
    (void)reply;
    (void)arena;
    cl_uint num_events{};
    const cl_event* event_list = nullptr;
    uint64_t event_list_count = 0;
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_list = wire_items;
            event_list_count = wire_count;
        }
    }
    if (event_list != nullptr && event_list_count < static_cast<uint64_t>(num_events)) return false;
    const cl_int ret = s.on_clWaitForEvents(num_events, event_list);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetEventInfo(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetEventInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_event event = nullptr;
    cl_event_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          event = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
          if (event == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_event_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetEventInfo(event, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clCreateUserEvent(ServerDispatch& s, ::av::WireReader& r,
                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateUserEvent");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_event ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateUserEvent(context, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::event, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clSetUserEventStatus(ServerDispatch& s, ::av::WireReader& r,
                                       ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clSetUserEventStatus");
    (void)r;
    (void)reply;
    (void)arena;
    cl_event event = nullptr;
    cl_int execution_status{};
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          event = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
          if (event == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        execution_status = static_cast<cl_int>(wire_value);
    }
    const cl_int ret = s.on_clSetUserEventStatus(event, execution_status);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clRetainEvent(ServerDispatch& s, ::av::WireReader& r,
                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clRetainEvent");
    (void)r;
    (void)reply;
    (void)arena;
    cl_event event = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          event = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
          if (event == nullptr) return false;
      } }
    const cl_int ret = s.on_clRetainEvent(event);
    if (ret == CL_SUCCESS) s.retained(HandleKind::event, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseEvent(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clReleaseEvent");
    (void)r;
    (void)reply;
    (void)arena;
    cl_event event = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          event = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
          if (event == nullptr) return false;
      } }
    const cl_int ret = s.on_clReleaseEvent(event);
    if (ret == CL_SUCCESS) s.released(HandleKind::event, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetEventProfilingInfo(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetEventProfilingInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_event event = nullptr;
    cl_profiling_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          event = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
          if (event == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_profiling_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetEventProfilingInfo(event, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clCreateFromGLBuffer(ServerDispatch& s, ::av::WireReader& r,
                                       ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateFromGLBuffer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    cl_GLuint bufobj{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        bufobj = static_cast<cl_GLuint>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateFromGLBuffer(context, flags, bufobj, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateFromGLTexture(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateFromGLTexture");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    cl_GLenum target{};
    cl_GLint miplevel{};
    cl_GLuint texture{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        target = static_cast<cl_GLenum>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        miplevel = static_cast<cl_GLint>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        texture = static_cast<cl_GLuint>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateFromGLTexture(context, flags, target, miplevel, texture, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateFromGLTexture2D(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateFromGLTexture2D");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    cl_GLenum target{};
    cl_GLint miplevel{};
    cl_GLuint texture{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        target = static_cast<cl_GLenum>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        miplevel = static_cast<cl_GLint>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        texture = static_cast<cl_GLuint>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateFromGLTexture2D(context, flags, target, miplevel, texture, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateFromGLTexture3D(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateFromGLTexture3D");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    cl_GLenum target{};
    cl_GLint miplevel{};
    cl_GLuint texture{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        target = static_cast<cl_GLenum>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        miplevel = static_cast<cl_GLint>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        texture = static_cast<cl_GLuint>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateFromGLTexture3D(context, flags, target, miplevel, texture, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateFromGLRenderbuffer(ServerDispatch& s, ::av::WireReader& r,
                                             ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateFromGLRenderbuffer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    cl_GLuint renderbuffer{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        renderbuffer = static_cast<cl_GLuint>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateFromGLRenderbuffer(context, flags, renderbuffer, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clGetGLObjectInfo(ServerDispatch& s, ::av::WireReader& r,
                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetGLObjectInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_mem memobj = nullptr;
    bool gl_object_type_wanted = false;
    bool gl_object_name_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          memobj = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (memobj == nullptr) return false;
      } }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        gl_object_type_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        gl_object_name_wanted = wire_wanted != 0;
    }
    cl_gl_object_type gl_object_type_storage{};
    cl_gl_object_type* gl_object_type = gl_object_type_wanted ? &gl_object_type_storage : nullptr;
    cl_GLuint gl_object_name_storage{};
    cl_GLuint* gl_object_name = gl_object_name_wanted ? &gl_object_name_storage : nullptr;
    const cl_int ret = s.on_clGetGLObjectInfo(memobj, gl_object_type, gl_object_name);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (gl_object_type_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(gl_object_type_storage) : 0u);
    }
    if (gl_object_name_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(gl_object_name_storage) : 0u);
    }
    return true;
}

static bool decode_clGetGLTextureInfo(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetGLTextureInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_mem memobj = nullptr;
    cl_gl_texture_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          memobj = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (memobj == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_gl_texture_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetGLTextureInfo(memobj, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clEnqueueAcquireGLObjects(ServerDispatch& s, ::av::WireReader& r,
                                            ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueAcquireGLObjects");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_objects{};
    const cl_mem* mem_objects = nullptr;
    uint64_t mem_objects_count = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_objects = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_mem>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            mem_objects = wire_items;
            mem_objects_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (mem_objects != nullptr && mem_objects_count < static_cast<uint64_t>(num_objects)) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueAcquireGLObjects(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueReleaseGLObjects(ServerDispatch& s, ::av::WireReader& r,
                                            ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueReleaseGLObjects");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_objects{};
    const cl_mem* mem_objects = nullptr;
    uint64_t mem_objects_count = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_objects = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_mem>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            mem_objects = wire_items;
            mem_objects_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (mem_objects != nullptr && mem_objects_count < static_cast<uint64_t>(num_objects)) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueReleaseGLObjects(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clGetGLContextInfoKHR(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetGLContextInfoKHR");
    (void)r;
    (void)reply;
    (void)arena;
    const cl_context_properties* properties = nullptr;
    cl_gl_context_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_context_properties>(static_cast<size_t>(wire_count) + 2);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_context_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
            wire_items[wire_count + 1] = 0;
            for (uint64_t i = 0; wire_items[i] != 0; i += 2) {
                if (wire_items[i] == CL_CONTEXT_PLATFORM) {
                    const uint64_t wire_handle = static_cast<uint64_t>(wire_items[i + 1]);
                    void* wire_platform = s.handle_for(HandleKind::platform, wire_handle);
                    if (wire_platform == nullptr) return false;
                    wire_items[i + 1] = static_cast<cl_context_properties>(reinterpret_cast<intptr_t>(wire_platform));
                } else if (wire_items[i] != CL_CONTEXT_INTEROP_USER_SYNC) {
                    return false;
                }
            }
            properties = wire_items;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_gl_context_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetGLContextInfoKHR(properties, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clRetainDevice(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clRetainDevice");
    (void)r;
    (void)reply;
    (void)arena;
    cl_device_id device = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (device == nullptr) return false;
      } }
    const cl_int ret = s.on_clRetainDevice(device);
    if (ret == CL_SUCCESS) s.retained(HandleKind::device, device);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseDevice(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clReleaseDevice");
    (void)r;
    (void)reply;
    (void)arena;
    cl_device_id device = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (device == nullptr) return false;
      } }
    const cl_int ret = s.on_clReleaseDevice(device);
    if (ret == CL_SUCCESS) s.released(HandleKind::device, device);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clCreateSubDevices(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateSubDevices");
    (void)r;
    (void)reply;
    (void)arena;
    cl_device_id in_device = nullptr;
    const cl_device_partition_property* properties = nullptr;
    cl_uint num_devices{};
    bool out_devices_wanted = false;
    bool num_devices_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          in_device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (in_device == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_device_partition_property>(static_cast<size_t>(wire_count) + 2);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_device_partition_property>(wire_value);
            }
            wire_items[wire_count] = 0;
            wire_items[wire_count + 1] = 0;
            properties = wire_items;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_devices = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        out_devices_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        num_devices_ret_wanted = wire_wanted != 0;
    }
    cl_device_id* out_devices = nullptr;
    if (out_devices_wanted) {
        out_devices = arena.allocate<cl_device_id>(static_cast<size_t>(num_devices) + 1);
        if (out_devices == nullptr) return false;
    }
    cl_uint num_devices_ret_storage{};
    cl_uint* num_devices_ret = (num_devices_ret_wanted || out_devices_wanted) ? &num_devices_ret_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((out_devices_wanted ? static_cast<uint64_t>(num_devices) : 0u))) ? s.on_clCreateSubDevices(in_device, properties, num_devices, out_devices, num_devices_ret) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (out_devices_wanted) {
        const uint64_t wire_produced = (ret == CL_SUCCESS) ? static_cast<uint64_t>(num_devices_ret_storage) : 0u;
        const uint64_t wire_sent = wire_produced < static_cast<uint64_t>(num_devices)
            ? wire_produced : static_cast<uint64_t>(num_devices);
        reply.write_varint(wire_sent);
        for (uint64_t i = 0; i < wire_sent; ++i) {
            if (out_devices[i] != nullptr) s.made(HandleKind::device, out_devices[i]);
            reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(out_devices[i])));
        }
    }
    if (num_devices_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(num_devices_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clEnqueueTask(ServerDispatch& s, ::av::WireReader& r,
                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueTask");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_kernel kernel = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (kernel == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueTask(command_queue, kernel, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueMarker(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueMarker");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueMarker(command_queue, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueBarrier(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueBarrier");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    const cl_int ret = s.on_clEnqueueBarrier(command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clEnqueueWaitForEvents(ServerDispatch& s, ::av::WireReader& r,
                                         ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueWaitForEvents");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_events{};
    const cl_event* event_list = nullptr;
    uint64_t event_list_count = 0;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_list = wire_items;
            event_list_count = wire_count;
        }
    }
    if (event_list != nullptr && event_list_count < static_cast<uint64_t>(num_events)) return false;
    const cl_int ret = s.on_clEnqueueWaitForEvents(command_queue, num_events, event_list);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clEnqueueMigrateMemObjects(ServerDispatch& s, ::av::WireReader& r,
                                             ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueMigrateMemObjects");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_mem_objects{};
    const cl_mem* mem_objects = nullptr;
    uint64_t mem_objects_count = 0;
    cl_mem_migration_flags flags{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_mem_objects = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_mem>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            mem_objects = wire_items;
            mem_objects_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_migration_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (mem_objects != nullptr && mem_objects_count < static_cast<uint64_t>(num_mem_objects)) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueMigrateMemObjects(command_queue, num_mem_objects, mem_objects, flags, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueReadBufferRect(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueReadBufferRect");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem buffer = nullptr;
    cl_bool blocking_read{};
    const size_t* buffer_origin = nullptr;
    uint64_t buffer_origin_count = 0;
    const size_t* host_origin = nullptr;
    uint64_t host_origin_count = 0;
    const size_t* region = nullptr;
    uint64_t region_count = 0;
    size_t buffer_row_pitch{};
    size_t buffer_slice_pitch{};
    size_t host_row_pitch{};
    size_t host_slice_pitch{};
    bool ptr_wanted = false;
    size_t ptr_capacity = 0;
    void* ptr_lent = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (buffer == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        blocking_read = static_cast<cl_bool>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            buffer_origin = wire_items;
            buffer_origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            host_origin = wire_items;
            host_origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            region = wire_items;
            region_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        buffer_row_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        buffer_slice_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        host_row_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        host_slice_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        ptr_wanted = wire_wanted != 0;
        if (ptr_wanted) {
            uint64_t cap = 0;
            if (!r.read_varint(cap)) return false;
            ptr_capacity = static_cast<size_t>(cap);
        }
        if (wire_wanted == 2) {
            uint64_t wire_offset = 0;
            if (!r.read_varint(wire_offset)) return false;
            ptr_lent = s.lent_for(wire_offset, static_cast<uint64_t>(ptr_capacity));
            if (ptr_lent == nullptr) return false;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (buffer_origin != nullptr && buffer_origin_count < 3u) return false;
    if (host_origin != nullptr && host_origin_count < 3u) return false;
    if (region != nullptr && region_count < 3u) return false;
    {
        uint64_t wire_needed = 0;
        if (ptr_wanted && (!rect_bytes(host_origin, region, host_row_pitch, host_slice_pitch, wire_needed) ||
                         static_cast<uint64_t>(ptr_capacity) < wire_needed)) return false;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    void* ptr = nullptr;
    if (ptr_lent != nullptr) {
        ptr = static_cast<void*>(ptr_lent);
    } else if (ptr_wanted) {
        ptr = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(ptr_capacity) + 1));
        if (ptr == nullptr) return false;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueReadBufferRect(command_queue, buffer, blocking_read, buffer_origin, host_origin, region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (ptr_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(ptr_capacity) : 0u;
        if (wire_written > static_cast<uint64_t>(ptr_capacity))
            wire_written = static_cast<uint64_t>(ptr_capacity);
        reply.write_varint(wire_written);
        if (wire_written != 0 && ptr_lent == nullptr) reply.write(ptr, static_cast<size_t>(wire_written));
    }
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueWriteBufferRect(ServerDispatch& s, ::av::WireReader& r,
                                           ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueWriteBufferRect");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem buffer = nullptr;
    cl_bool blocking_write{};
    const size_t* buffer_origin = nullptr;
    uint64_t buffer_origin_count = 0;
    const size_t* host_origin = nullptr;
    uint64_t host_origin_count = 0;
    const size_t* region = nullptr;
    uint64_t region_count = 0;
    size_t buffer_row_pitch{};
    size_t buffer_slice_pitch{};
    size_t host_row_pitch{};
    size_t host_slice_pitch{};
    const void* ptr = nullptr;
    uint64_t ptr_count = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (buffer == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        blocking_write = static_cast<cl_bool>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            buffer_origin = wire_items;
            buffer_origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            host_origin = wire_items;
            host_origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            region = wire_items;
            region_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        buffer_row_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        buffer_slice_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        host_row_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        host_slice_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present == 2) {
            uint64_t wire_count = 0;
            uint64_t wire_offset = 0;
            if (!r.read_varint(wire_count) || !r.read_varint(wire_offset)) return false;
            void* wire_bytes = s.lent_for(wire_offset, wire_count);
            if (wire_bytes == nullptr) return false;
            ptr = static_cast<const void*>(wire_bytes);
            ptr_count = wire_count;
        } else if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            ptr = reinterpret_cast<const void*>(wire_bytes);
            ptr_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (buffer_origin != nullptr && buffer_origin_count < 3u) return false;
    if (host_origin != nullptr && host_origin_count < 3u) return false;
    if (region != nullptr && region_count < 3u) return false;
    {
        uint64_t wire_needed = 0;
        if (ptr != nullptr && (!rect_bytes(host_origin, region, host_row_pitch, host_slice_pitch, wire_needed) ||
                         ptr_count < wire_needed)) return false;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueWriteBufferRect(command_queue, buffer, blocking_write, buffer_origin, host_origin, region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueCopyBufferRect(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueCopyBufferRect");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem src_buffer = nullptr;
    cl_mem dst_buffer = nullptr;
    const size_t* src_origin = nullptr;
    uint64_t src_origin_count = 0;
    const size_t* dst_origin = nullptr;
    uint64_t dst_origin_count = 0;
    const size_t* region = nullptr;
    uint64_t region_count = 0;
    size_t src_row_pitch{};
    size_t src_slice_pitch{};
    size_t dst_row_pitch{};
    size_t dst_slice_pitch{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          src_buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (src_buffer == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          dst_buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (dst_buffer == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            src_origin = wire_items;
            src_origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            dst_origin = wire_items;
            dst_origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            region = wire_items;
            region_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        src_row_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        src_slice_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        dst_row_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        dst_slice_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (src_origin != nullptr && src_origin_count < 3u) return false;
    if (dst_origin != nullptr && dst_origin_count < 3u) return false;
    if (region != nullptr && region_count < 3u) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueCopyBufferRect(command_queue, src_buffer, dst_buffer, src_origin, dst_origin, region, src_row_pitch, src_slice_pitch, dst_row_pitch, dst_slice_pitch, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueFillImage(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueFillImage");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem image = nullptr;
    const void* fill_color = nullptr;
    uint64_t fill_color_count = 0;
    const size_t* origin = nullptr;
    uint64_t origin_count = 0;
    const size_t* region = nullptr;
    uint64_t region_count = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          image = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (image == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            fill_color = reinterpret_cast<const void*>(wire_bytes);
            fill_color_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            origin = wire_items;
            origin_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<size_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_varint(wire_value)) return false;
                wire_items[i] = static_cast<size_t>(wire_value);
            }
            region = wire_items;
            region_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (fill_color != nullptr && fill_color_count < 16u) return false;
    if (origin != nullptr && origin_count < 3u) return false;
    if (region != nullptr && region_count < 3u) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueFillImage(command_queue, image, fill_color, origin, region, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clCreateImage2D(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateImage2D");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    const cl_image_format* image_format = nullptr;
    size_t image_width{};
    size_t image_height{};
    size_t image_row_pitch{};
    void* host_ptr = nullptr;
    uint64_t host_ptr_count = 0;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            auto* wire_item = arena.allocate<cl_image_format>(1);
            if (wire_item == nullptr) return false;
            if (!r.read(wire_item, sizeof(*wire_item))) return false;
            image_format = wire_item;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        image_width = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        image_height = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        image_row_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            host_ptr = reinterpret_cast<void*>(wire_bytes);
            host_ptr_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (host_ptr != nullptr) {
        uint64_t wire_needed = 0;
        if (!image_host_bytes(image_format, CL_MEM_OBJECT_IMAGE2D, image_width, image_height,
                                 1, 1, image_row_pitch, 0, wire_needed) ||
            host_ptr_count < wire_needed) return false;
    }
    if ((flags & CL_MEM_USE_HOST_PTR) != 0) return false;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateImage2D(context, flags, image_format, image_width, image_height, image_row_pitch, host_ptr, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateImage3D(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateImage3D");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    const cl_image_format* image_format = nullptr;
    size_t image_width{};
    size_t image_height{};
    size_t image_depth{};
    size_t image_row_pitch{};
    size_t image_slice_pitch{};
    void* host_ptr = nullptr;
    uint64_t host_ptr_count = 0;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            auto* wire_item = arena.allocate<cl_image_format>(1);
            if (wire_item == nullptr) return false;
            if (!r.read(wire_item, sizeof(*wire_item))) return false;
            image_format = wire_item;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        image_width = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        image_height = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        image_depth = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        image_row_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        image_slice_pitch = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            host_ptr = reinterpret_cast<void*>(wire_bytes);
            host_ptr_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (host_ptr != nullptr) {
        uint64_t wire_needed = 0;
        if (!image_host_bytes(image_format, CL_MEM_OBJECT_IMAGE3D, image_width, image_height,
                                 image_depth, 1, image_row_pitch, image_slice_pitch, wire_needed) ||
            host_ptr_count < wire_needed) return false;
    }
    if ((flags & CL_MEM_USE_HOST_PTR) != 0) return false;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateImage3D(context, flags, image_format, image_width, image_height, image_depth, image_row_pitch, image_slice_pitch, host_ptr, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clUnloadCompiler(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clUnloadCompiler");
    (void)r;
    (void)reply;
    (void)arena;
    const cl_int ret = s.on_clUnloadCompiler();
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clSetCommandQueueProperty(ServerDispatch& s, ::av::WireReader& r,
                                            ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clSetCommandQueueProperty");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_command_queue_properties properties{};
    cl_bool enable{};
    bool old_properties_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        properties = static_cast<cl_command_queue_properties>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        enable = static_cast<cl_bool>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        old_properties_wanted = wire_wanted != 0;
    }
    cl_command_queue_properties old_properties_storage{};
    cl_command_queue_properties* old_properties = old_properties_wanted ? &old_properties_storage : nullptr;
    const cl_int ret = s.on_clSetCommandQueueProperty(command_queue, properties, enable, old_properties);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (old_properties_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(old_properties_storage) : 0u);
    }
    return true;
}

static bool decode_clCreateProgramWithIL(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateProgramWithIL");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    const void* il = nullptr;
    uint64_t il_count = 0;
    size_t length{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            il = reinterpret_cast<const void*>(wire_bytes);
            il_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        length = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (il != nullptr && il_count < static_cast<uint64_t>(length)) return false;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_program ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateProgramWithIL(context, il, length, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::program, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCompileProgram(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCompileProgram");
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    cl_uint num_devices{};
    const cl_device_id* device_list = nullptr;
    uint64_t device_list_count = 0;
    const char* options = nullptr;
    cl_uint num_input_headers{};
    const cl_program* input_headers = nullptr;
    uint64_t input_headers_count = 0;
    const char** header_include_names = nullptr;
    uint64_t header_include_names_count = 0;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          program = static_cast<cl_program>(s.handle_for(HandleKind::program, wire_handle));
          if (program == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_devices = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_device_id>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            device_list = wire_items;
            device_list_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_len = 0;
            if (!r.read_varint(wire_len)) return false;
            char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);
            if (wire_text == nullptr) return false;
            if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;
            wire_text[wire_len] = '\0';
            options = wire_text;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_input_headers = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_program>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_program>(s.handle_for(HandleKind::program, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            input_headers = wire_items;
            input_headers_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto** wire_items = arena.allocate<const char*>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_len = 0;
                if (!r.read_varint(wire_len)) return false;
                char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);
                if (wire_text == nullptr) return false;
                if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;
                wire_text[wire_len] = '\0';
                wire_items[i] = reinterpret_cast<const char*>(wire_text);
            }
            header_include_names = wire_items;
            header_include_names_count = wire_count;
        }
    }
    if (device_list != nullptr && device_list_count < static_cast<uint64_t>(num_devices)) return false;
    if (input_headers != nullptr && input_headers_count < static_cast<uint64_t>(num_input_headers)) return false;
    if (header_include_names != nullptr && header_include_names_count < static_cast<uint64_t>(num_input_headers)) return false;
    options = with_kernel_arg_info(arena, options);
    if (options == nullptr) return false;
    const cl_int ret = s.on_clCompileProgram(program, num_devices, device_list, options, num_input_headers, input_headers, header_include_names);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clLinkProgram(ServerDispatch& s, ::av::WireReader& r,
                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clLinkProgram");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_uint num_devices{};
    const cl_device_id* device_list = nullptr;
    uint64_t device_list_count = 0;
    const char* options = nullptr;
    cl_uint num_input_programs{};
    const cl_program* input_programs = nullptr;
    uint64_t input_programs_count = 0;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_devices = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_device_id>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            device_list = wire_items;
            device_list_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_len = 0;
            if (!r.read_varint(wire_len)) return false;
            char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);
            if (wire_text == nullptr) return false;
            if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;
            wire_text[wire_len] = '\0';
            options = wire_text;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_input_programs = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_program>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_program>(s.handle_for(HandleKind::program, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            input_programs = wire_items;
            input_programs_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (device_list != nullptr && device_list_count < static_cast<uint64_t>(num_devices)) return false;
    if (input_programs != nullptr && input_programs_count < static_cast<uint64_t>(num_input_programs)) return false;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_program ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clLinkProgram(context, num_devices, device_list, options, num_input_programs, input_programs, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::program, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clSetProgramSpecializationConstant(ServerDispatch& s, ::av::WireReader& r,
                                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clSetProgramSpecializationConstant");
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    cl_uint spec_id{};
    size_t spec_size{};
    const void* spec_value = nullptr;
    uint64_t spec_value_count = 0;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          program = static_cast<cl_program>(s.handle_for(HandleKind::program, wire_handle));
          if (program == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        spec_id = static_cast<cl_uint>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        spec_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            spec_value = reinterpret_cast<const void*>(wire_bytes);
            spec_value_count = wire_count;
        }
    }
    if (spec_value != nullptr && spec_value_count < static_cast<uint64_t>(spec_size)) return false;
    const cl_int ret = s.on_clSetProgramSpecializationConstant(program, spec_id, spec_size, spec_value);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clCloneKernel(ServerDispatch& s, ::av::WireReader& r,
                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCloneKernel");
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel source_kernel = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          source_kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (source_kernel == nullptr) return false;
      } }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_kernel ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCloneKernel(source_kernel, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::kernel, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clSetKernelExecInfo(ServerDispatch& s, ::av::WireReader& r,
                                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clSetKernelExecInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    cl_kernel_exec_info param_name{};
    size_t param_value_size{};
    const void* param_value = nullptr;
    uint64_t param_value_count = 0;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (kernel == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_kernel_exec_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            param_value = reinterpret_cast<const void*>(wire_bytes);
            param_value_count = wire_count;
        }
    }
    if (param_value != nullptr && param_value_count < static_cast<uint64_t>(param_value_size)) return false;
    if (param_name == 0x40B8) return false;  // CL_KERNEL_EXEC_INFO_SVM_PTRS_ARM
    if (param_name == 0x4203) return false;  // CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL
    if (param_name == CL_KERNEL_EXEC_INFO_SVM_PTRS &&
        !resolve_svm_pointers(s, arena, param_value_size, param_value)) return false;
    const cl_int ret = s.on_clSetKernelExecInfo(kernel, param_name, param_value_size, param_value);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetKernelSubGroupInfo(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetKernelSubGroupInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    cl_device_id device = nullptr;
    cl_kernel_sub_group_info param_name{};
    size_t input_value_size{};
    const void* input_value = nullptr;
    uint64_t input_value_count = 0;
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (kernel == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (device == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_kernel_sub_group_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        input_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            input_value = reinterpret_cast<const void*>(wire_bytes);
            input_value_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    if (input_value != nullptr && input_value_count < static_cast<uint64_t>(input_value_size)) return false;
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetKernelSubGroupInfo(kernel, device, param_name, input_value_size, input_value, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clGetHostTimer(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetHostTimer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_device_id device = nullptr;
    bool host_timestamp_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (device == nullptr) return false;
      } }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        host_timestamp_wanted = wire_wanted != 0;
    }
    cl_ulong host_timestamp_storage{};
    cl_ulong* host_timestamp = host_timestamp_wanted ? &host_timestamp_storage : nullptr;
    const cl_int ret = s.on_clGetHostTimer(device, host_timestamp);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (host_timestamp_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(host_timestamp_storage) : 0u);
    }
    return true;
}

static bool decode_clGetDeviceAndHostTimer(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetDeviceAndHostTimer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_device_id device = nullptr;
    bool device_timestamp_wanted = false;
    bool host_timestamp_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (device == nullptr) return false;
      } }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        device_timestamp_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        host_timestamp_wanted = wire_wanted != 0;
    }
    cl_ulong device_timestamp_storage{};
    cl_ulong* device_timestamp = device_timestamp_wanted ? &device_timestamp_storage : nullptr;
    cl_ulong host_timestamp_storage{};
    cl_ulong* host_timestamp = host_timestamp_wanted ? &host_timestamp_storage : nullptr;
    const cl_int ret = s.on_clGetDeviceAndHostTimer(device, device_timestamp, host_timestamp);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (device_timestamp_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(device_timestamp_storage) : 0u);
    }
    if (host_timestamp_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(host_timestamp_storage) : 0u);
    }
    return true;
}

static bool decode_clCreateBufferWithProperties(ServerDispatch& s, ::av::WireReader& r,
                                               ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateBufferWithProperties");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    const cl_mem_properties* properties = nullptr;
    cl_mem_flags flags{};
    size_t size{};
    void* host_ptr = nullptr;
    uint64_t host_ptr_count = 0;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_mem_properties>(static_cast<size_t>(wire_count) + 2);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_mem_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
            wire_items[wire_count + 1] = 0;
            for (uint64_t i = 0; wire_items[i] != 0; i += 2) {
                if (wire_items[i] == 0x2051) return false;  // CL_MEM_DEVICE_HANDLE_LIST_KHR
            }
            properties = wire_items;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            host_ptr = reinterpret_cast<void*>(wire_bytes);
            host_ptr_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (host_ptr != nullptr && host_ptr_count < static_cast<uint64_t>(size)) return false;
    if ((flags & CL_MEM_USE_HOST_PTR) != 0) return false;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateBufferWithProperties(context, properties, flags, size, host_ptr, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateImageWithProperties(ServerDispatch& s, ::av::WireReader& r,
                                              ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateImageWithProperties");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    const cl_mem_properties* properties = nullptr;
    cl_mem_flags flags{};
    const cl_image_format* image_format = nullptr;
    const cl_image_desc* image_desc = nullptr;
    void* host_ptr = nullptr;
    uint64_t host_ptr_count = 0;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_mem_properties>(static_cast<size_t>(wire_count) + 2);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_mem_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
            wire_items[wire_count + 1] = 0;
            for (uint64_t i = 0; wire_items[i] != 0; i += 2) {
                if (wire_items[i] == 0x2051) return false;  // CL_MEM_DEVICE_HANDLE_LIST_KHR
            }
            properties = wire_items;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            auto* wire_item = arena.allocate<cl_image_format>(1);
            if (wire_item == nullptr) return false;
            if (!r.read(wire_item, sizeof(*wire_item))) return false;
            image_format = wire_item;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            auto* wire_item = arena.allocate<cl_image_desc>(1);
            if (wire_item == nullptr) return false;
            if (!r.read(wire_item, sizeof(*wire_item))) return false;
            if (wire_item->buffer != nullptr) {
                const uint64_t wire_handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(wire_item->buffer));
                wire_item->buffer = static_cast<decltype(wire_item->buffer)>(s.handle_for(HandleKind::mem, wire_handle));
                if (wire_item->buffer == nullptr) return false;
            }
            image_desc = wire_item;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            host_ptr = reinterpret_cast<void*>(wire_bytes);
            host_ptr_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (host_ptr != nullptr) {
        uint64_t wire_needed = 0;
        if (image_desc == nullptr || !image_host_bytes(image_format, image_desc->image_type, image_desc->image_width, image_desc->image_height,
                                 image_desc->image_depth, image_desc->image_array_size, image_desc->image_row_pitch, image_desc->image_slice_pitch, wire_needed) ||
            host_ptr_count < wire_needed) return false;
    }
    if ((flags & CL_MEM_USE_HOST_PTR) != 0) return false;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateImageWithProperties(context, properties, flags, image_format, image_desc, host_ptr, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateSamplerWithProperties(ServerDispatch& s, ::av::WireReader& r,
                                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateSamplerWithProperties");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    const cl_sampler_properties* sampler_properties = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_sampler_properties>(static_cast<size_t>(wire_count) + 2);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_sampler_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
            wire_items[wire_count + 1] = 0;
            sampler_properties = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_sampler ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateSamplerWithProperties(context, sampler_properties, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::sampler, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clSetDefaultDeviceCommandQueue(ServerDispatch& s, ::av::WireReader& r,
                                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clSetDefaultDeviceCommandQueue");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_device_id device = nullptr;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          device = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
          if (device == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    const cl_int ret = s.on_clSetDefaultDeviceCommandQueue(context, device, command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clCreateSampler(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateSampler");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_bool normalized_coords{};
    cl_addressing_mode addressing_mode{};
    cl_filter_mode filter_mode{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        normalized_coords = static_cast<cl_bool>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        addressing_mode = static_cast<cl_addressing_mode>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        filter_mode = static_cast<cl_filter_mode>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_sampler ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateSampler(context, normalized_coords, addressing_mode, filter_mode, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::sampler, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clRetainSampler(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clRetainSampler");
    (void)r;
    (void)reply;
    (void)arena;
    cl_sampler sampler = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          sampler = static_cast<cl_sampler>(s.handle_for(HandleKind::sampler, wire_handle));
          if (sampler == nullptr) return false;
      } }
    const cl_int ret = s.on_clRetainSampler(sampler);
    if (ret == CL_SUCCESS) s.retained(HandleKind::sampler, sampler);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseSampler(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clReleaseSampler");
    (void)r;
    (void)reply;
    (void)arena;
    cl_sampler sampler = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          sampler = static_cast<cl_sampler>(s.handle_for(HandleKind::sampler, wire_handle));
          if (sampler == nullptr) return false;
      } }
    const cl_int ret = s.on_clReleaseSampler(sampler);
    if (ret == CL_SUCCESS) s.released(HandleKind::sampler, sampler);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetSamplerInfo(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetSamplerInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_sampler sampler = nullptr;
    cl_sampler_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          sampler = static_cast<cl_sampler>(s.handle_for(HandleKind::sampler, wire_handle));
          if (sampler == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_sampler_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetSamplerInfo(sampler, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clCreatePipe(ServerDispatch& s, ::av::WireReader& r,
                               ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreatePipe");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    cl_uint pipe_packet_size{};
    cl_uint pipe_max_packets{};
    const cl_pipe_properties* properties = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_mem_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        pipe_packet_size = static_cast<cl_uint>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        pipe_max_packets = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_pipe_properties>(static_cast<size_t>(wire_count) + 2);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_pipe_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
            wire_items[wire_count + 1] = 0;
            properties = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_mem ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreatePipe(context, flags, pipe_packet_size, pipe_max_packets, properties, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::mem, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clGetPipeInfo(ServerDispatch& s, ::av::WireReader& r,
                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clGetPipeInfo");
    (void)r;
    (void)reply;
    (void)arena;
    cl_mem pipe = nullptr;
    cl_pipe_info param_name{};
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          pipe = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (pipe == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_name = static_cast<cl_pipe_info>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        param_value_size = static_cast<size_t>(wire_value);
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        param_value_size_ret_wanted = wire_wanted != 0;
    }
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = (param_value_size_ret_wanted || param_value_wanted) ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetPipeInfo(pipe, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = (ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u;
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint((ret == CL_SUCCESS) ? static_cast<uint64_t>(param_value_size_ret_storage) : 0u);
    }
    return true;
}

static bool decode_clCreateProgramWithBuiltInKernels(ServerDispatch& s, ::av::WireReader& r,
                                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clCreateProgramWithBuiltInKernels");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_uint num_devices{};
    const cl_device_id* device_list = nullptr;
    uint64_t device_list_count = 0;
    const char* kernel_names = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_devices = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_device_id>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_device_id>(s.handle_for(HandleKind::device, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            device_list = wire_items;
            device_list_count = wire_count;
        }
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_len = 0;
            if (!r.read_varint(wire_len)) return false;
            char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);
            if (wire_text == nullptr) return false;
            if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;
            wire_text[wire_len] = '\0';
            kernel_names = wire_text;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (device_list != nullptr && device_list_count < static_cast<uint64_t>(num_devices)) return false;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    cl_program ret = nullptr;
    if (s.room_for(static_cast<uint64_t>(1u))) ret = s.on_clCreateProgramWithBuiltInKernels(context, num_devices, device_list, kernel_names, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    if (ret != nullptr) s.made(HandleKind::program, ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clEnqueueMapBuffer(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueMapBuffer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem buffer = nullptr;
    cl_bool blocking_map{};
    cl_map_flags map_flags{};
    size_t offset{};
    size_t size{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          buffer = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (buffer == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        blocking_map = static_cast<cl_bool>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        map_flags = static_cast<cl_map_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        offset = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        size = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    void* ret = nullptr;
    if (s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u)))) ret = s.on_clEnqueueMapBuffer(command_queue, buffer, blocking_map, map_flags, offset, size, num_events_in_wait_list, event_wait_list, event, errcode_ret);
    else if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (event_wanted) {
        if (!(ret != nullptr)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clEnqueueUnmapMemObject(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueUnmapMemObject");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem memobj = nullptr;
    void* mapped_ptr = nullptr;
    uint64_t mapped_ptr_wire = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          memobj = static_cast<cl_mem>(s.handle_for(HandleKind::mem, wire_handle));
          if (memobj == nullptr) return false;
      } }
    if (!r.read_pod(mapped_ptr_wire)) return false;
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    if (mapped_ptr_wire != 0) {
        mapped_ptr = static_cast<void*>(s.svm_for(mapped_ptr_wire, 0u));
        if (mapped_ptr == nullptr) return false;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueUnmapMemObject(command_queue, memobj, mapped_ptr, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clSVMAlloc(ServerDispatch& s, ::av::WireReader& r,
                             ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clSVMAlloc");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_svm_mem_flags flags{};
    size_t size{};
    cl_uint alignment{};
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_svm_mem_flags>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        size = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        alignment = static_cast<cl_uint>(wire_value);
    }
    const void* ret = s.on_clSVMAlloc(context, flags, size, alignment);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    return true;
}

static bool decode_clSVMFree(ServerDispatch& s, ::av::WireReader& r,
                            ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clSVMFree");
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    void* svm_pointer = nullptr;
    uint64_t svm_pointer_wire = 0;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          context = static_cast<cl_context>(s.handle_for(HandleKind::context, wire_handle));
          if (context == nullptr) return false;
      } }
    if (!r.read_pod(svm_pointer_wire)) return false;
    if (svm_pointer_wire != 0) {
        svm_pointer = static_cast<void*>(s.svm_for(svm_pointer_wire, 0u));
        if (svm_pointer == nullptr) return false;
    }
    s.on_clSVMFree(context, svm_pointer);
    return true;
}

static bool decode_clEnqueueSVMMap(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueSVMMap");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_bool blocking_map{};
    cl_map_flags flags{};
    void* svm_ptr = nullptr;
    uint64_t svm_ptr_wire = 0;
    size_t size{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        blocking_map = static_cast<cl_bool>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        flags = static_cast<cl_map_flags>(wire_value);
    }
    if (!r.read_pod(svm_ptr_wire)) return false;
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        size = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    if (svm_ptr_wire != 0) {
        svm_ptr = static_cast<void*>(s.svm_for(svm_ptr_wire, static_cast<uint64_t>(size)));
        if (svm_ptr == nullptr) return false;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueSVMMap(command_queue, blocking_map, flags, svm_ptr, size, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueSVMUnmap(ServerDispatch& s, ::av::WireReader& r,
                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueSVMUnmap");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    void* svm_ptr = nullptr;
    uint64_t svm_ptr_wire = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    if (!r.read_pod(svm_ptr_wire)) return false;
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    if (svm_ptr_wire != 0) {
        svm_ptr = static_cast<void*>(s.svm_for(svm_ptr_wire, 0u));
        if (svm_ptr == nullptr) return false;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueSVMUnmap(command_queue, svm_ptr, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueSVMMemcpy(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueSVMMemcpy");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_bool blocking_copy{};
    void* dst_ptr = nullptr;
    uint64_t dst_ptr_wire = 0;
    const void* src_ptr = nullptr;
    uint64_t src_ptr_wire = 0;
    size_t size{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        blocking_copy = static_cast<cl_bool>(wire_value);
    }
    if (!r.read_pod(dst_ptr_wire)) return false;
    if (!r.read_pod(src_ptr_wire)) return false;
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        size = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    if (dst_ptr_wire != 0) {
        dst_ptr = static_cast<void*>(s.svm_for(dst_ptr_wire, static_cast<uint64_t>(size)));
        if (dst_ptr == nullptr) return false;
    }
    if (src_ptr_wire != 0) {
        src_ptr = static_cast<const void*>(s.svm_for(src_ptr_wire, static_cast<uint64_t>(size)));
        if (src_ptr == nullptr) return false;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueSVMMemcpy(command_queue, blocking_copy, dst_ptr, src_ptr, size, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueSVMMemFill(ServerDispatch& s, ::av::WireReader& r,
                                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clEnqueueSVMMemFill");
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    void* svm_ptr = nullptr;
    uint64_t svm_ptr_wire = 0;
    const void* pattern = nullptr;
    uint64_t pattern_count = 0;
    size_t pattern_size{};
    size_t size{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    uint64_t event_wait_list_count = 0;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          command_queue = static_cast<cl_command_queue>(s.handle_for(HandleKind::command_queue, wire_handle));
          if (command_queue == nullptr) return false;
      } }
    if (!r.read_pod(svm_ptr_wire)) return false;
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            pattern = reinterpret_cast<const void*>(wire_bytes);
            pattern_count = wire_count;
        }
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        pattern_size = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        size = static_cast<size_t>(wire_value);
    }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        num_events_in_wait_list = static_cast<cl_uint>(wire_value);
    }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_event>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_handle = 0;
                if (!r.read_pod(wire_handle)) return false;
                wire_items[i] = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
                if (wire_items[i] == nullptr) return false;
            }
            event_wait_list = wire_items;
            event_wait_list_count = wire_count;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    if (pattern != nullptr && pattern_count < static_cast<uint64_t>(pattern_size)) return false;
    if (event_wait_list != nullptr && event_wait_list_count < static_cast<uint64_t>(num_events_in_wait_list)) return false;
    if (svm_ptr_wire != 0) {
        svm_ptr = static_cast<void*>(s.svm_for(svm_ptr_wire, static_cast<uint64_t>(size)));
        if (svm_ptr == nullptr) return false;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.room_for(static_cast<uint64_t>((event_wanted ? 1u : 0u))) ? s.on_clEnqueueSVMMemFill(command_queue, svm_ptr, pattern, pattern_size, size, num_events_in_wait_list, event_wait_list, event) : CL_OUT_OF_HOST_MEMORY;
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        if (!(ret == CL_SUCCESS)) event_storage = nullptr;
        if (event_storage != nullptr) s.made(HandleKind::event, event_storage);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clSetKernelArgSVMPointer(ServerDispatch& s, ::av::WireReader& r,
                                           ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clSetKernelArgSVMPointer");
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    cl_uint arg_index{};
    const void* arg_value = nullptr;
    uint64_t arg_value_wire = 0;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          kernel = static_cast<cl_kernel>(s.handle_for(HandleKind::kernel, wire_handle));
          if (kernel == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        arg_index = static_cast<cl_uint>(wire_value);
    }
    if (!r.read_pod(arg_value_wire)) return false;
    if (arg_value_wire != 0) {
        arg_value = static_cast<const void*>(s.svm_for(arg_value_wire, 0u));
        if (arg_value == nullptr) return false;
    }
    const cl_int ret = s.on_clSetKernelArgSVMPointer(kernel, arg_index, arg_value);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clSetEventCallback(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:clSetEventCallback");
    (void)r;
    (void)reply;
    (void)arena;
    cl_event event = nullptr;
    cl_int command_exec_callback_type{};
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      if (wire_handle != 0) {
          event = static_cast<cl_event>(s.handle_for(HandleKind::event, wire_handle));
          if (event == nullptr) return false;
      } }
    {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return false;
        command_exec_callback_type = static_cast<cl_int>(wire_value);
    }
    const cl_int ret = s.on_clSetEventCallback(event, command_exec_callback_type);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

bool dispatch_command(ServerDispatch& s, uint32_t cmd_id,
                      ::av::WireReader& r, ::av::WireWriter& reply,
                      ::av::WireArena& arena) {
    switch (cmd_id) {
        case kCmd_clGetPlatformIDs: return decode_clGetPlatformIDs(s, r, reply, arena);
        case kCmd_clGetPlatformInfo: return decode_clGetPlatformInfo(s, r, reply, arena);
        case kCmd_clGetDeviceIDs: return decode_clGetDeviceIDs(s, r, reply, arena);
        case kCmd_clGetDeviceInfo: return decode_clGetDeviceInfo(s, r, reply, arena);
        case kCmd_clCreateContext: return decode_clCreateContext(s, r, reply, arena);
        case kCmd_clCreateContextFromType: return decode_clCreateContextFromType(s, r, reply, arena);
        case kCmd_clRetainContext: return decode_clRetainContext(s, r, reply, arena);
        case kCmd_clReleaseContext: return decode_clReleaseContext(s, r, reply, arena);
        case kCmd_clGetContextInfo: return decode_clGetContextInfo(s, r, reply, arena);
        case kCmd_clCreateCommandQueue: return decode_clCreateCommandQueue(s, r, reply, arena);
        case kCmd_clCreateCommandQueueWithProperties: return decode_clCreateCommandQueueWithProperties(s, r, reply, arena);
        case kCmd_clRetainCommandQueue: return decode_clRetainCommandQueue(s, r, reply, arena);
        case kCmd_clReleaseCommandQueue: return decode_clReleaseCommandQueue(s, r, reply, arena);
        case kCmd_clGetCommandQueueInfo: return decode_clGetCommandQueueInfo(s, r, reply, arena);
        case kCmd_clFlush: return decode_clFlush(s, r, reply, arena);
        case kCmd_clFinish: return decode_clFinish(s, r, reply, arena);
        case kCmd_clCreateBuffer: return decode_clCreateBuffer(s, r, reply, arena);
        case kCmd_clCreateSubBuffer: return decode_clCreateSubBuffer(s, r, reply, arena);
        case kCmd_clCreateImage: return decode_clCreateImage(s, r, reply, arena);
        case kCmd_clRetainMemObject: return decode_clRetainMemObject(s, r, reply, arena);
        case kCmd_clReleaseMemObject: return decode_clReleaseMemObject(s, r, reply, arena);
        case kCmd_clGetMemObjectInfo: return decode_clGetMemObjectInfo(s, r, reply, arena);
        case kCmd_clGetImageInfo: return decode_clGetImageInfo(s, r, reply, arena);
        case kCmd_clGetSupportedImageFormats: return decode_clGetSupportedImageFormats(s, r, reply, arena);
        case kCmd_clEnqueueReadBuffer: return decode_clEnqueueReadBuffer(s, r, reply, arena);
        case kCmd_clEnqueueWriteBuffer: return decode_clEnqueueWriteBuffer(s, r, reply, arena);
        case kCmd_clEnqueueCopyBuffer: return decode_clEnqueueCopyBuffer(s, r, reply, arena);
        case kCmd_clEnqueueFillBuffer: return decode_clEnqueueFillBuffer(s, r, reply, arena);
        case kCmd_clEnqueueReadImage: return decode_clEnqueueReadImage(s, r, reply, arena);
        case kCmd_clEnqueueWriteImage: return decode_clEnqueueWriteImage(s, r, reply, arena);
        case kCmd_clEnqueueCopyImage: return decode_clEnqueueCopyImage(s, r, reply, arena);
        case kCmd_clEnqueueCopyImageToBuffer: return decode_clEnqueueCopyImageToBuffer(s, r, reply, arena);
        case kCmd_clEnqueueCopyBufferToImage: return decode_clEnqueueCopyBufferToImage(s, r, reply, arena);
        case kCmd_clCreateProgramWithSource: return decode_clCreateProgramWithSource(s, r, reply, arena);
        case kCmd_clCreateProgramWithBinary: return decode_clCreateProgramWithBinary(s, r, reply, arena);
        case kCmd_clRetainProgram: return decode_clRetainProgram(s, r, reply, arena);
        case kCmd_clReleaseProgram: return decode_clReleaseProgram(s, r, reply, arena);
        case kCmd_clBuildProgram: return decode_clBuildProgram(s, r, reply, arena);
        case kCmd_clGetProgramInfo: return decode_clGetProgramInfo(s, r, reply, arena);
        case kCmd_clGetProgramBuildInfo: return decode_clGetProgramBuildInfo(s, r, reply, arena);
        case kCmd_clUnloadPlatformCompiler: return decode_clUnloadPlatformCompiler(s, r, reply, arena);
        case kCmd_clCreateKernel: return decode_clCreateKernel(s, r, reply, arena);
        case kCmd_clCreateKernelsInProgram: return decode_clCreateKernelsInProgram(s, r, reply, arena);
        case kCmd_clRetainKernel: return decode_clRetainKernel(s, r, reply, arena);
        case kCmd_clReleaseKernel: return decode_clReleaseKernel(s, r, reply, arena);
        case kCmd_clSetKernelArg: return decode_clSetKernelArg(s, r, reply, arena);
        case kCmd_clGetKernelInfo: return decode_clGetKernelInfo(s, r, reply, arena);
        case kCmd_clGetKernelArgInfo: return decode_clGetKernelArgInfo(s, r, reply, arena);
        case kCmd_clGetKernelWorkGroupInfo: return decode_clGetKernelWorkGroupInfo(s, r, reply, arena);
        case kCmd_clEnqueueNDRangeKernel: return decode_clEnqueueNDRangeKernel(s, r, reply, arena);
        case kCmd_clEnqueueMarkerWithWaitList: return decode_clEnqueueMarkerWithWaitList(s, r, reply, arena);
        case kCmd_clEnqueueBarrierWithWaitList: return decode_clEnqueueBarrierWithWaitList(s, r, reply, arena);
        case kCmd_clWaitForEvents: return decode_clWaitForEvents(s, r, reply, arena);
        case kCmd_clGetEventInfo: return decode_clGetEventInfo(s, r, reply, arena);
        case kCmd_clCreateUserEvent: return decode_clCreateUserEvent(s, r, reply, arena);
        case kCmd_clSetUserEventStatus: return decode_clSetUserEventStatus(s, r, reply, arena);
        case kCmd_clRetainEvent: return decode_clRetainEvent(s, r, reply, arena);
        case kCmd_clReleaseEvent: return decode_clReleaseEvent(s, r, reply, arena);
        case kCmd_clGetEventProfilingInfo: return decode_clGetEventProfilingInfo(s, r, reply, arena);
        case kCmd_clCreateFromGLBuffer: return decode_clCreateFromGLBuffer(s, r, reply, arena);
        case kCmd_clCreateFromGLTexture: return decode_clCreateFromGLTexture(s, r, reply, arena);
        case kCmd_clCreateFromGLTexture2D: return decode_clCreateFromGLTexture2D(s, r, reply, arena);
        case kCmd_clCreateFromGLTexture3D: return decode_clCreateFromGLTexture3D(s, r, reply, arena);
        case kCmd_clCreateFromGLRenderbuffer: return decode_clCreateFromGLRenderbuffer(s, r, reply, arena);
        case kCmd_clGetGLObjectInfo: return decode_clGetGLObjectInfo(s, r, reply, arena);
        case kCmd_clGetGLTextureInfo: return decode_clGetGLTextureInfo(s, r, reply, arena);
        case kCmd_clEnqueueAcquireGLObjects: return decode_clEnqueueAcquireGLObjects(s, r, reply, arena);
        case kCmd_clEnqueueReleaseGLObjects: return decode_clEnqueueReleaseGLObjects(s, r, reply, arena);
        case kCmd_clGetGLContextInfoKHR: return decode_clGetGLContextInfoKHR(s, r, reply, arena);
        case kCmd_clRetainDevice: return decode_clRetainDevice(s, r, reply, arena);
        case kCmd_clReleaseDevice: return decode_clReleaseDevice(s, r, reply, arena);
        case kCmd_clCreateSubDevices: return decode_clCreateSubDevices(s, r, reply, arena);
        case kCmd_clEnqueueTask: return decode_clEnqueueTask(s, r, reply, arena);
        case kCmd_clEnqueueMarker: return decode_clEnqueueMarker(s, r, reply, arena);
        case kCmd_clEnqueueBarrier: return decode_clEnqueueBarrier(s, r, reply, arena);
        case kCmd_clEnqueueWaitForEvents: return decode_clEnqueueWaitForEvents(s, r, reply, arena);
        case kCmd_clEnqueueMigrateMemObjects: return decode_clEnqueueMigrateMemObjects(s, r, reply, arena);
        case kCmd_clEnqueueReadBufferRect: return decode_clEnqueueReadBufferRect(s, r, reply, arena);
        case kCmd_clEnqueueWriteBufferRect: return decode_clEnqueueWriteBufferRect(s, r, reply, arena);
        case kCmd_clEnqueueCopyBufferRect: return decode_clEnqueueCopyBufferRect(s, r, reply, arena);
        case kCmd_clEnqueueFillImage: return decode_clEnqueueFillImage(s, r, reply, arena);
        case kCmd_clCreateImage2D: return decode_clCreateImage2D(s, r, reply, arena);
        case kCmd_clCreateImage3D: return decode_clCreateImage3D(s, r, reply, arena);
        case kCmd_clUnloadCompiler: return decode_clUnloadCompiler(s, r, reply, arena);
        case kCmd_clSetCommandQueueProperty: return decode_clSetCommandQueueProperty(s, r, reply, arena);
        case kCmd_clCreateProgramWithIL: return decode_clCreateProgramWithIL(s, r, reply, arena);
        case kCmd_clCompileProgram: return decode_clCompileProgram(s, r, reply, arena);
        case kCmd_clLinkProgram: return decode_clLinkProgram(s, r, reply, arena);
        case kCmd_clSetProgramSpecializationConstant: return decode_clSetProgramSpecializationConstant(s, r, reply, arena);
        case kCmd_clCloneKernel: return decode_clCloneKernel(s, r, reply, arena);
        case kCmd_clSetKernelExecInfo: return decode_clSetKernelExecInfo(s, r, reply, arena);
        case kCmd_clGetKernelSubGroupInfo: return decode_clGetKernelSubGroupInfo(s, r, reply, arena);
        case kCmd_clGetHostTimer: return decode_clGetHostTimer(s, r, reply, arena);
        case kCmd_clGetDeviceAndHostTimer: return decode_clGetDeviceAndHostTimer(s, r, reply, arena);
        case kCmd_clCreateBufferWithProperties: return decode_clCreateBufferWithProperties(s, r, reply, arena);
        case kCmd_clCreateImageWithProperties: return decode_clCreateImageWithProperties(s, r, reply, arena);
        case kCmd_clCreateSamplerWithProperties: return decode_clCreateSamplerWithProperties(s, r, reply, arena);
        case kCmd_clSetDefaultDeviceCommandQueue: return decode_clSetDefaultDeviceCommandQueue(s, r, reply, arena);
        case kCmd_clCreateSampler: return decode_clCreateSampler(s, r, reply, arena);
        case kCmd_clRetainSampler: return decode_clRetainSampler(s, r, reply, arena);
        case kCmd_clReleaseSampler: return decode_clReleaseSampler(s, r, reply, arena);
        case kCmd_clGetSamplerInfo: return decode_clGetSamplerInfo(s, r, reply, arena);
        case kCmd_clCreatePipe: return decode_clCreatePipe(s, r, reply, arena);
        case kCmd_clGetPipeInfo: return decode_clGetPipeInfo(s, r, reply, arena);
        case kCmd_clCreateProgramWithBuiltInKernels: return decode_clCreateProgramWithBuiltInKernels(s, r, reply, arena);
        case kCmd_clEnqueueMapBuffer: return decode_clEnqueueMapBuffer(s, r, reply, arena);
        case kCmd_clEnqueueUnmapMemObject: return decode_clEnqueueUnmapMemObject(s, r, reply, arena);
        case kCmd_clSVMAlloc: return decode_clSVMAlloc(s, r, reply, arena);
        case kCmd_clSVMFree: return decode_clSVMFree(s, r, reply, arena);
        case kCmd_clEnqueueSVMMap: return decode_clEnqueueSVMMap(s, r, reply, arena);
        case kCmd_clEnqueueSVMUnmap: return decode_clEnqueueSVMUnmap(s, r, reply, arena);
        case kCmd_clEnqueueSVMMemcpy: return decode_clEnqueueSVMMemcpy(s, r, reply, arena);
        case kCmd_clEnqueueSVMMemFill: return decode_clEnqueueSVMMemFill(s, r, reply, arena);
        case kCmd_clSetKernelArgSVMPointer: return decode_clSetKernelArgSVMPointer(s, r, reply, arena);
        case kCmd_clSetEventCallback: return decode_clSetEventCallback(s, r, reply, arena);
        default: return false;
    }
}

}  // namespace av::clproto
