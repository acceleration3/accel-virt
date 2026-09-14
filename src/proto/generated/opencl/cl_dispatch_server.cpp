// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opencl --xml cl.xml --out ...

#include "cl_dispatch_server.h"

#include <stddef.h>
#include <stdint.h>

#include "cl_protocol_ids.h"

namespace av::clproto {

static bool decode_clGetPlatformIDs(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
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
    cl_uint* num_platforms = num_platforms_wanted ? &num_platforms_storage : nullptr;
    const cl_int ret = s.on_clGetPlatformIDs(num_entries, platforms, num_platforms);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (platforms_wanted) {
        const uint64_t wire_produced = num_platforms != nullptr
            ? static_cast<uint64_t>(*num_platforms)
            : static_cast<uint64_t>(num_entries);
        const uint64_t wire_sent = wire_produced < static_cast<uint64_t>(num_entries)
            ? wire_produced : static_cast<uint64_t>(num_entries);
        reply.write_varint(wire_sent);
        for (uint64_t i = 0; i < wire_sent; ++i) {
            reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(platforms[i])));
        }
    }
    if (num_platforms_wanted) {
        reply.write_varint(static_cast<uint64_t>(num_platforms_storage));
    }
    return true;
}

static bool decode_clGetPlatformInfo(ServerDispatch& s, ::av::WireReader& r,
                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      platform = reinterpret_cast<cl_platform_id>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetPlatformInfo(platform, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clGetDeviceIDs(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      platform = reinterpret_cast<cl_platform_id>(static_cast<uintptr_t>(wire_handle)); }
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
    cl_uint* num_devices = num_devices_wanted ? &num_devices_storage : nullptr;
    const cl_int ret = s.on_clGetDeviceIDs(platform, device_type, num_entries, devices, num_devices);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (devices_wanted) {
        const uint64_t wire_produced = num_devices != nullptr
            ? static_cast<uint64_t>(*num_devices)
            : static_cast<uint64_t>(num_entries);
        const uint64_t wire_sent = wire_produced < static_cast<uint64_t>(num_entries)
            ? wire_produced : static_cast<uint64_t>(num_entries);
        reply.write_varint(wire_sent);
        for (uint64_t i = 0; i < wire_sent; ++i) {
            reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(devices[i])));
        }
    }
    if (num_devices_wanted) {
        reply.write_varint(static_cast<uint64_t>(num_devices_storage));
    }
    return true;
}

static bool decode_clGetDeviceInfo(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetDeviceInfo(device, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clCreateContext(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    const cl_context_properties* properties = nullptr;
    cl_uint num_devices{};
    const cl_device_id* devices = nullptr;
    bool errcode_ret_wanted = false;
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_context_properties>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_context_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
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
                wire_items[i] = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle));
            }
            devices = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_context ret = s.on_clCreateContext(properties, num_devices, devices, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateContextFromType(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
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
            auto* wire_items = arena.allocate<cl_context_properties>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_context_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
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
    const cl_context ret = s.on_clCreateContextFromType(properties, device_type, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clRetainContext(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clRetainContext(context);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseContext(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clReleaseContext(context);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetContextInfo(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetContextInfo(context, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clCreateCommandQueue(ServerDispatch& s, ::av::WireReader& r,
                                       ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_device_id device = nullptr;
    cl_command_queue_properties properties{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
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
    const cl_command_queue ret = s.on_clCreateCommandQueue(context, device, properties, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateCommandQueueWithProperties(ServerDispatch& s, ::av::WireReader& r,
                                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_device_id device = nullptr;
    const cl_queue_properties* properties = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_queue_properties>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_queue_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
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
    const cl_command_queue ret = s.on_clCreateCommandQueueWithProperties(context, device, properties, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clRetainCommandQueue(ServerDispatch& s, ::av::WireReader& r,
                                       ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clRetainCommandQueue(command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseCommandQueue(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clReleaseCommandQueue(command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetCommandQueueInfo(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetCommandQueueInfo(command_queue, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clFlush(ServerDispatch& s, ::av::WireReader& r,
                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clFlush(command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clFinish(ServerDispatch& s, ::av::WireReader& r,
                           ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clFinish(command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clCreateBuffer(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    size_t size{};
    void* host_ptr = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_mem ret = s.on_clCreateBuffer(context, flags, size, host_ptr, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateSubBuffer(ServerDispatch& s, ::av::WireReader& r,
                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
    const cl_mem ret = s.on_clCreateSubBuffer(buffer, flags, buffer_create_type, buffer_create_info, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateImage(ServerDispatch& s, ::av::WireReader& r,
                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    const cl_image_format* image_format = nullptr;
    const cl_image_desc* image_desc = nullptr;
    void* host_ptr = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_mem ret = s.on_clCreateImage(context, flags, image_format, image_desc, host_ptr, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clRetainMemObject(ServerDispatch& s, ::av::WireReader& r,
                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_mem memobj = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      memobj = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clRetainMemObject(memobj);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseMemObject(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_mem memobj = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      memobj = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clReleaseMemObject(memobj);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetMemObjectInfo(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      memobj = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetMemObjectInfo(memobj, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clGetImageInfo(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      image = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetImageInfo(image, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clGetSupportedImageFormats(ServerDispatch& s, ::av::WireReader& r,
                                             ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
    cl_image_format* image_formats = nullptr;
    if (image_formats_wanted) {
        image_formats = reinterpret_cast<cl_image_format*>(arena.allocate<uint8_t>(static_cast<size_t>(image_formats_capacity) + 1));
        if (image_formats == nullptr) return false;
    }
    cl_uint num_image_formats_storage{};
    cl_uint* num_image_formats = num_image_formats_wanted ? &num_image_formats_storage : nullptr;
    const cl_int ret = s.on_clGetSupportedImageFormats(context, flags, image_type, num_entries, image_formats, num_image_formats);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (image_formats_wanted) {
        reply.write_varint(static_cast<uint64_t>(image_formats_capacity));
        if (image_formats_capacity != 0) reply.write(image_formats, static_cast<size_t>(image_formats_capacity));
    }
    if (num_image_formats_wanted) {
        reply.write_varint(static_cast<uint64_t>(num_image_formats_storage));
    }
    return true;
}

static bool decode_clEnqueueReadBuffer(ServerDispatch& s, ::av::WireReader& r,
                                      ::av::WireWriter& reply, ::av::WireArena& arena) {
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
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    void* ptr = nullptr;
    if (ptr_wanted) {
        ptr = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(ptr_capacity) + 1));
        if (ptr == nullptr) return false;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueReadBuffer(command_queue, buffer, blocking_read, offset, size, ptr, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (ptr_wanted) {
        reply.write_varint(static_cast<uint64_t>(ptr_capacity));
        if (ptr_capacity != 0) reply.write(ptr, static_cast<size_t>(ptr_capacity));
    }
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueWriteBuffer(ServerDispatch& s, ::av::WireReader& r,
                                       ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem buffer = nullptr;
    cl_bool blocking_write{};
    size_t offset{};
    size_t size{};
    const void* ptr = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            ptr = reinterpret_cast<const void*>(wire_bytes);
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueWriteBuffer(command_queue, buffer, blocking_write, offset, size, ptr, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueCopyBuffer(ServerDispatch& s, ::av::WireReader& r,
                                      ::av::WireWriter& reply, ::av::WireArena& arena) {
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
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      src_buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      dst_buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueCopyBuffer(command_queue, src_buffer, dst_buffer, src_offset, dst_offset, size, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueFillBuffer(ServerDispatch& s, ::av::WireReader& r,
                                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem buffer = nullptr;
    const void* pattern = nullptr;
    size_t pattern_size{};
    size_t offset{};
    size_t size{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueFillBuffer(command_queue, buffer, pattern, pattern_size, offset, size, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueReadImage(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem image = nullptr;
    cl_bool blocking_read{};
    const size_t* origin = nullptr;
    const size_t* region = nullptr;
    size_t row_pitch{};
    size_t slice_pitch{};
    bool ptr_wanted = false;
    size_t ptr_capacity = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      image = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    void* ptr = nullptr;
    if (ptr_wanted) {
        ptr = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(ptr_capacity) + 1));
        if (ptr == nullptr) return false;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueReadImage(command_queue, image, blocking_read, origin, region, row_pitch, slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (ptr_wanted) {
        reply.write_varint(static_cast<uint64_t>(ptr_capacity));
        if (ptr_capacity != 0) reply.write(ptr, static_cast<size_t>(ptr_capacity));
    }
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueWriteImage(ServerDispatch& s, ::av::WireReader& r,
                                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem image = nullptr;
    cl_bool blocking_write{};
    const size_t* origin = nullptr;
    const size_t* region = nullptr;
    size_t input_row_pitch{};
    size_t input_slice_pitch{};
    const void* ptr = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      image = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            ptr = reinterpret_cast<const void*>(wire_bytes);
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueWriteImage(command_queue, image, blocking_write, origin, region, input_row_pitch, input_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueCopyImage(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem src_image = nullptr;
    cl_mem dst_image = nullptr;
    const size_t* src_origin = nullptr;
    const size_t* dst_origin = nullptr;
    const size_t* region = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      src_image = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      dst_image = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueCopyImage(command_queue, src_image, dst_image, src_origin, dst_origin, region, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueCopyImageToBuffer(ServerDispatch& s, ::av::WireReader& r,
                                             ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem src_image = nullptr;
    cl_mem dst_buffer = nullptr;
    const size_t* src_origin = nullptr;
    const size_t* region = nullptr;
    size_t dst_offset{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      src_image = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      dst_buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueCopyImageToBuffer(command_queue, src_image, dst_buffer, src_origin, region, dst_offset, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueCopyBufferToImage(ServerDispatch& s, ::av::WireReader& r,
                                             ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem src_buffer = nullptr;
    cl_mem dst_image = nullptr;
    size_t src_offset{};
    const size_t* dst_origin = nullptr;
    const size_t* region = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      src_buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      dst_image = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueCopyBufferToImage(command_queue, src_buffer, dst_image, src_offset, dst_origin, region, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clCreateProgramWithSource(ServerDispatch& s, ::av::WireReader& r,
                                            ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_uint count{};
    const char** strings = nullptr;
    const size_t* strings_lengths = nullptr;
    const size_t* lengths = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
            strings_lengths = wire_lengths;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    lengths = strings_lengths;
    const cl_program ret = s.on_clCreateProgramWithSource(context, count, strings, lengths, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateProgramWithBinary(ServerDispatch& s, ::av::WireReader& r,
                                            ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_uint num_devices{};
    const cl_device_id* device_list = nullptr;
    const size_t* lengths = nullptr;
    const unsigned char** binaries = nullptr;
    const size_t* binaries_lengths = nullptr;
    bool binary_status_wanted = false;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle));
            }
            device_list = wire_items;
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
    cl_int binary_status_storage{};
    cl_int* binary_status = binary_status_wanted ? &binary_status_storage : nullptr;
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    lengths = binaries_lengths;
    const cl_program ret = s.on_clCreateProgramWithBinary(context, num_devices, device_list, lengths, binaries, binary_status, errcode_ret);
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
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      program = reinterpret_cast<cl_program>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clRetainProgram(program);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseProgram(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      program = reinterpret_cast<cl_program>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clReleaseProgram(program);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clBuildProgram(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    cl_uint num_devices{};
    const cl_device_id* device_list = nullptr;
    const char* options = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      program = reinterpret_cast<cl_program>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle));
            }
            device_list = wire_items;
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
    const cl_int ret = s.on_clBuildProgram(program, num_devices, device_list, options);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetProgramInfo(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      program = reinterpret_cast<cl_program>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetProgramInfo(program, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clGetProgramBuildInfo(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      program = reinterpret_cast<cl_program>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetProgramBuildInfo(program, device, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clUnloadPlatformCompiler(ServerDispatch& s, ::av::WireReader& r,
                                           ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_platform_id platform = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      platform = reinterpret_cast<cl_platform_id>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clUnloadPlatformCompiler(platform);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clCreateKernel(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    const char* kernel_name = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      program = reinterpret_cast<cl_program>(static_cast<uintptr_t>(wire_handle)); }
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
    const cl_kernel ret = s.on_clCreateKernel(program, kernel_name, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateKernelsInProgram(ServerDispatch& s, ::av::WireReader& r,
                                           ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    cl_uint num_kernels{};
    bool kernels_wanted = false;
    bool num_kernels_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      program = reinterpret_cast<cl_program>(static_cast<uintptr_t>(wire_handle)); }
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
    cl_uint* num_kernels_ret = num_kernels_ret_wanted ? &num_kernels_ret_storage : nullptr;
    const cl_int ret = s.on_clCreateKernelsInProgram(program, num_kernels, kernels, num_kernels_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (kernels_wanted) {
        const uint64_t wire_produced = num_kernels_ret != nullptr
            ? static_cast<uint64_t>(*num_kernels_ret)
            : static_cast<uint64_t>(num_kernels);
        const uint64_t wire_sent = wire_produced < static_cast<uint64_t>(num_kernels)
            ? wire_produced : static_cast<uint64_t>(num_kernels);
        reply.write_varint(wire_sent);
        for (uint64_t i = 0; i < wire_sent; ++i) {
            reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(kernels[i])));
        }
    }
    if (num_kernels_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(num_kernels_ret_storage));
    }
    return true;
}

static bool decode_clRetainKernel(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      kernel = reinterpret_cast<cl_kernel>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clRetainKernel(kernel);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseKernel(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      kernel = reinterpret_cast<cl_kernel>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clReleaseKernel(kernel);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clSetKernelArg(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    cl_uint arg_index{};
    size_t arg_size{};
    const void* arg_value = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      kernel = reinterpret_cast<cl_kernel>(static_cast<uintptr_t>(wire_handle)); }
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
        }
    }
    const cl_int ret = s.on_clSetKernelArg(kernel, arg_index, arg_size, arg_value);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetKernelInfo(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      kernel = reinterpret_cast<cl_kernel>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetKernelInfo(kernel, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clGetKernelArgInfo(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      kernel = reinterpret_cast<cl_kernel>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetKernelArgInfo(kernel, arg_index, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clGetKernelWorkGroupInfo(ServerDispatch& s, ::av::WireReader& r,
                                           ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      kernel = reinterpret_cast<cl_kernel>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetKernelWorkGroupInfo(kernel, device, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clEnqueueNDRangeKernel(ServerDispatch& s, ::av::WireReader& r,
                                         ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_kernel kernel = nullptr;
    cl_uint work_dim{};
    const size_t* global_work_offset = nullptr;
    const size_t* global_work_size = nullptr;
    const size_t* local_work_size = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      kernel = reinterpret_cast<cl_kernel>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueNDRangeKernel(command_queue, kernel, work_dim, global_work_offset, global_work_size, local_work_size, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueMarkerWithWaitList(ServerDispatch& s, ::av::WireReader& r,
                                              ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueMarkerWithWaitList(command_queue, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueBarrierWithWaitList(ServerDispatch& s, ::av::WireReader& r,
                                               ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueBarrierWithWaitList(command_queue, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clWaitForEvents(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_uint num_events{};
    const cl_event* event_list = nullptr;
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_list = wire_items;
        }
    }
    const cl_int ret = s.on_clWaitForEvents(num_events, event_list);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetEventInfo(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      event = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetEventInfo(event, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clCreateUserEvent(ServerDispatch& s, ::av::WireReader& r,
                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_event ret = s.on_clCreateUserEvent(context, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clSetUserEventStatus(ServerDispatch& s, ::av::WireReader& r,
                                       ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_event event = nullptr;
    cl_int execution_status{};
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      event = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle)); }
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
    (void)r;
    (void)reply;
    (void)arena;
    cl_event event = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      event = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clRetainEvent(event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseEvent(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_event event = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      event = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clReleaseEvent(event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetEventProfilingInfo(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      event = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetEventProfilingInfo(event, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clCreateFromGLBuffer(ServerDispatch& s, ::av::WireReader& r,
                                       ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    cl_GLuint bufobj{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
    const cl_mem ret = s.on_clCreateFromGLBuffer(context, flags, bufobj, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateFromGLTexture(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
    const cl_mem ret = s.on_clCreateFromGLTexture(context, flags, target, miplevel, texture, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateFromGLTexture2D(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
    const cl_mem ret = s.on_clCreateFromGLTexture2D(context, flags, target, miplevel, texture, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateFromGLTexture3D(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
    const cl_mem ret = s.on_clCreateFromGLTexture3D(context, flags, target, miplevel, texture, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateFromGLRenderbuffer(ServerDispatch& s, ::av::WireReader& r,
                                             ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_mem_flags flags{};
    cl_GLuint renderbuffer{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
    const cl_mem ret = s.on_clCreateFromGLRenderbuffer(context, flags, renderbuffer, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clGetGLObjectInfo(ServerDispatch& s, ::av::WireReader& r,
                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_mem memobj = nullptr;
    bool gl_object_type_wanted = false;
    bool gl_object_name_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      memobj = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
        reply.write_varint(static_cast<uint64_t>(gl_object_type_storage));
    }
    if (gl_object_name_wanted) {
        reply.write_varint(static_cast<uint64_t>(gl_object_name_storage));
    }
    return true;
}

static bool decode_clGetGLTextureInfo(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      memobj = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetGLTextureInfo(memobj, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clEnqueueAcquireGLObjects(ServerDispatch& s, ::av::WireReader& r,
                                            ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_objects{};
    const cl_mem* mem_objects = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle));
            }
            mem_objects = wire_items;
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueAcquireGLObjects(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueReleaseGLObjects(ServerDispatch& s, ::av::WireReader& r,
                                            ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_objects{};
    const cl_mem* mem_objects = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle));
            }
            mem_objects = wire_items;
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueReleaseGLObjects(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clGetGLContextInfoKHR(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
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
            auto* wire_items = arena.allocate<cl_context_properties>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_context_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetGLContextInfoKHR(properties, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clRetainDevice(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_device_id device = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clRetainDevice(device);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseDevice(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_device_id device = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clReleaseDevice(device);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clCreateSubDevices(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      in_device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_device_partition_property>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_device_partition_property>(wire_value);
            }
            wire_items[wire_count] = 0;
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
    cl_uint* num_devices_ret = num_devices_ret_wanted ? &num_devices_ret_storage : nullptr;
    const cl_int ret = s.on_clCreateSubDevices(in_device, properties, num_devices, out_devices, num_devices_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (out_devices_wanted) {
        const uint64_t wire_produced = num_devices_ret != nullptr
            ? static_cast<uint64_t>(*num_devices_ret)
            : static_cast<uint64_t>(num_devices);
        const uint64_t wire_sent = wire_produced < static_cast<uint64_t>(num_devices)
            ? wire_produced : static_cast<uint64_t>(num_devices);
        reply.write_varint(wire_sent);
        for (uint64_t i = 0; i < wire_sent; ++i) {
            reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(out_devices[i])));
        }
    }
    if (num_devices_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(num_devices_ret_storage));
    }
    return true;
}

static bool decode_clEnqueueTask(ServerDispatch& s, ::av::WireReader& r,
                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_kernel kernel = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      kernel = reinterpret_cast<cl_kernel>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueTask(command_queue, kernel, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueMarker(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueMarker(command_queue, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueBarrier(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clEnqueueBarrier(command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clEnqueueWaitForEvents(ServerDispatch& s, ::av::WireReader& r,
                                         ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_events{};
    const cl_event* event_list = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_list = wire_items;
        }
    }
    const cl_int ret = s.on_clEnqueueWaitForEvents(command_queue, num_events, event_list);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clEnqueueMigrateMemObjects(ServerDispatch& s, ::av::WireReader& r,
                                             ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_uint num_mem_objects{};
    const cl_mem* mem_objects = nullptr;
    cl_mem_migration_flags flags{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle));
            }
            mem_objects = wire_items;
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueMigrateMemObjects(command_queue, num_mem_objects, mem_objects, flags, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueReadBufferRect(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem buffer = nullptr;
    cl_bool blocking_read{};
    const size_t* buffer_origin = nullptr;
    const size_t* host_origin = nullptr;
    const size_t* region = nullptr;
    size_t buffer_row_pitch{};
    size_t buffer_slice_pitch{};
    size_t host_row_pitch{};
    size_t host_slice_pitch{};
    bool ptr_wanted = false;
    size_t ptr_capacity = 0;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    void* ptr = nullptr;
    if (ptr_wanted) {
        ptr = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(ptr_capacity) + 1));
        if (ptr == nullptr) return false;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueReadBufferRect(command_queue, buffer, blocking_read, buffer_origin, host_origin, region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (ptr_wanted) {
        reply.write_varint(static_cast<uint64_t>(ptr_capacity));
        if (ptr_capacity != 0) reply.write(ptr, static_cast<size_t>(ptr_capacity));
    }
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueWriteBufferRect(ServerDispatch& s, ::av::WireReader& r,
                                           ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem buffer = nullptr;
    cl_bool blocking_write{};
    const size_t* buffer_origin = nullptr;
    const size_t* host_origin = nullptr;
    const size_t* region = nullptr;
    size_t buffer_row_pitch{};
    size_t buffer_slice_pitch{};
    size_t host_row_pitch{};
    size_t host_slice_pitch{};
    const void* ptr = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_bytes = arena.allocate<uint8_t>(static_cast<size_t>(wire_count) + 1);
            if (wire_bytes == nullptr) return false;
            if (wire_count != 0 && !r.read(wire_bytes, static_cast<size_t>(wire_count))) return false;
            ptr = reinterpret_cast<const void*>(wire_bytes);
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueWriteBufferRect(command_queue, buffer, blocking_write, buffer_origin, host_origin, region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueCopyBufferRect(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem src_buffer = nullptr;
    cl_mem dst_buffer = nullptr;
    const size_t* src_origin = nullptr;
    const size_t* dst_origin = nullptr;
    const size_t* region = nullptr;
    size_t src_row_pitch{};
    size_t src_slice_pitch{};
    size_t dst_row_pitch{};
    size_t dst_slice_pitch{};
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      src_buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      dst_buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueCopyBufferRect(command_queue, src_buffer, dst_buffer, src_origin, dst_origin, region, src_row_pitch, src_slice_pitch, dst_row_pitch, dst_slice_pitch, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clEnqueueFillImage(ServerDispatch& s, ::av::WireReader& r,
                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_mem image = nullptr;
    const void* fill_color = nullptr;
    const size_t* origin = nullptr;
    const size_t* region = nullptr;
    cl_uint num_events_in_wait_list{};
    const cl_event* event_wait_list = nullptr;
    bool event_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      image = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_event>(static_cast<uintptr_t>(wire_handle));
            }
            event_wait_list = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        event_wanted = wire_wanted != 0;
    }
    cl_event event_storage = nullptr;
    cl_event* event = event_wanted ? &event_storage : nullptr;
    const cl_int ret = s.on_clEnqueueFillImage(command_queue, image, fill_color, origin, region, num_events_in_wait_list, event_wait_list, event);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (event_wanted) {
        reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(event_storage)));
    }
    return true;
}

static bool decode_clCreateImage2D(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
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
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_mem ret = s.on_clCreateImage2D(context, flags, image_format, image_width, image_height, image_row_pitch, host_ptr, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateImage3D(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
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
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_mem ret = s.on_clCreateImage3D(context, flags, image_format, image_width, image_height, image_depth, image_row_pitch, image_slice_pitch, host_ptr, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clUnloadCompiler(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    const cl_int ret = s.on_clUnloadCompiler();
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clSetCommandQueueProperty(ServerDispatch& s, ::av::WireReader& r,
                                            ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_command_queue command_queue = nullptr;
    cl_command_queue_properties properties{};
    cl_bool enable{};
    bool old_properties_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
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
        reply.write_varint(static_cast<uint64_t>(old_properties_storage));
    }
    return true;
}

static bool decode_clCreateProgramWithIL(ServerDispatch& s, ::av::WireReader& r,
                                        ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    const void* il = nullptr;
    size_t length{};
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_program ret = s.on_clCreateProgramWithIL(context, il, length, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCompileProgram(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    cl_uint num_devices{};
    const cl_device_id* device_list = nullptr;
    const char* options = nullptr;
    cl_uint num_input_headers{};
    const cl_program* input_headers = nullptr;
    const char** header_include_names = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      program = reinterpret_cast<cl_program>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle));
            }
            device_list = wire_items;
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
                wire_items[i] = reinterpret_cast<cl_program>(static_cast<uintptr_t>(wire_handle));
            }
            input_headers = wire_items;
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
        }
    }
    const cl_int ret = s.on_clCompileProgram(program, num_devices, device_list, options, num_input_headers, input_headers, header_include_names);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clLinkProgram(ServerDispatch& s, ::av::WireReader& r,
                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_uint num_devices{};
    const cl_device_id* device_list = nullptr;
    const char* options = nullptr;
    cl_uint num_input_programs{};
    const cl_program* input_programs = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle));
            }
            device_list = wire_items;
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
                wire_items[i] = reinterpret_cast<cl_program>(static_cast<uintptr_t>(wire_handle));
            }
            input_programs = wire_items;
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_program ret = s.on_clLinkProgram(context, num_devices, device_list, options, num_input_programs, input_programs, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clSetProgramSpecializationConstant(ServerDispatch& s, ::av::WireReader& r,
                                                     ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_program program = nullptr;
    cl_uint spec_id{};
    size_t spec_size{};
    const void* spec_value = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      program = reinterpret_cast<cl_program>(static_cast<uintptr_t>(wire_handle)); }
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
        }
    }
    const cl_int ret = s.on_clSetProgramSpecializationConstant(program, spec_id, spec_size, spec_value);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clCloneKernel(ServerDispatch& s, ::av::WireReader& r,
                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel source_kernel = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      source_kernel = reinterpret_cast<cl_kernel>(static_cast<uintptr_t>(wire_handle)); }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_kernel ret = s.on_clCloneKernel(source_kernel, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clSetKernelExecInfo(ServerDispatch& s, ::av::WireReader& r,
                                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    cl_kernel_exec_info param_name{};
    size_t param_value_size{};
    const void* param_value = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      kernel = reinterpret_cast<cl_kernel>(static_cast<uintptr_t>(wire_handle)); }
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
        }
    }
    const cl_int ret = s.on_clSetKernelExecInfo(kernel, param_name, param_value_size, param_value);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetKernelSubGroupInfo(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_kernel kernel = nullptr;
    cl_device_id device = nullptr;
    cl_kernel_sub_group_info param_name{};
    size_t input_value_size{};
    const void* input_value = nullptr;
    size_t param_value_size{};
    bool param_value_wanted = false;
    bool param_value_size_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      kernel = reinterpret_cast<cl_kernel>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
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
    void* param_value = nullptr;
    if (param_value_wanted) {
        param_value = reinterpret_cast<void*>(arena.allocate<uint8_t>(static_cast<size_t>(param_value_size) + 1));
        if (param_value == nullptr) return false;
    }
    size_t param_value_size_ret_storage{};
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetKernelSubGroupInfo(kernel, device, param_name, input_value_size, input_value, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clGetHostTimer(ServerDispatch& s, ::av::WireReader& r,
                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_device_id device = nullptr;
    bool host_timestamp_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
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
        reply.write_varint(static_cast<uint64_t>(host_timestamp_storage));
    }
    return true;
}

static bool decode_clGetDeviceAndHostTimer(ServerDispatch& s, ::av::WireReader& r,
                                          ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_device_id device = nullptr;
    bool device_timestamp_wanted = false;
    bool host_timestamp_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
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
        reply.write_varint(static_cast<uint64_t>(device_timestamp_storage));
    }
    if (host_timestamp_wanted) {
        reply.write_varint(static_cast<uint64_t>(host_timestamp_storage));
    }
    return true;
}

static bool decode_clCreateBufferWithProperties(ServerDispatch& s, ::av::WireReader& r,
                                               ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    const cl_mem_properties* properties = nullptr;
    cl_mem_flags flags{};
    size_t size{};
    void* host_ptr = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_mem_properties>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_mem_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
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
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_mem ret = s.on_clCreateBufferWithProperties(context, properties, flags, size, host_ptr, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateImageWithProperties(ServerDispatch& s, ::av::WireReader& r,
                                              ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    const cl_mem_properties* properties = nullptr;
    cl_mem_flags flags{};
    const cl_image_format* image_format = nullptr;
    const cl_image_desc* image_desc = nullptr;
    void* host_ptr = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_mem_properties>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_mem_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
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
        }
    }
    {
        uint8_t wire_wanted = 0;
        if (!r.read_pod(wire_wanted)) return false;
        errcode_ret_wanted = wire_wanted != 0;
    }
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_mem ret = s.on_clCreateImageWithProperties(context, properties, flags, image_format, image_desc, host_ptr, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clCreateSamplerWithProperties(ServerDispatch& s, ::av::WireReader& r,
                                                ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    const cl_sampler_properties* sampler_properties = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
    {
        uint8_t wire_present = 0;
        if (!r.read_pod(wire_present)) return false;
        if (wire_present != 0) {
            uint64_t wire_count = 0;
            if (!r.read_varint(wire_count)) return false;
            auto* wire_items = arena.allocate<cl_sampler_properties>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_sampler_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
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
    const cl_sampler ret = s.on_clCreateSamplerWithProperties(context, sampler_properties, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clSetDefaultDeviceCommandQueue(ServerDispatch& s, ::av::WireReader& r,
                                                 ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_device_id device = nullptr;
    cl_command_queue command_queue = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle)); }
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      command_queue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clSetDefaultDeviceCommandQueue(context, device, command_queue);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clCreateSampler(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
    const cl_sampler ret = s.on_clCreateSampler(context, normalized_coords, addressing_mode, filter_mode, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clRetainSampler(ServerDispatch& s, ::av::WireReader& r,
                                  ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_sampler sampler = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      sampler = reinterpret_cast<cl_sampler>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clRetainSampler(sampler);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clReleaseSampler(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_sampler sampler = nullptr;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      sampler = reinterpret_cast<cl_sampler>(static_cast<uintptr_t>(wire_handle)); }
    const cl_int ret = s.on_clReleaseSampler(sampler);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    return true;
}

static bool decode_clGetSamplerInfo(ServerDispatch& s, ::av::WireReader& r,
                                   ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      sampler = reinterpret_cast<cl_sampler>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetSamplerInfo(sampler, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clCreatePipe(ServerDispatch& s, ::av::WireReader& r,
                               ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
            auto* wire_items = arena.allocate<cl_pipe_properties>(static_cast<size_t>(wire_count) + 1);
            if (wire_items == nullptr) return false;
            for (uint64_t i = 0; i < wire_count; ++i) {
                uint64_t wire_value = 0;
                if (!r.read_pod(wire_value)) return false;
                wire_items[i] = static_cast<cl_pipe_properties>(wire_value);
            }
            wire_items[wire_count] = 0;
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
    const cl_mem ret = s.on_clCreatePipe(context, flags, pipe_packet_size, pipe_max_packets, properties, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
    return true;
}

static bool decode_clGetPipeInfo(ServerDispatch& s, ::av::WireReader& r,
                                ::av::WireWriter& reply, ::av::WireArena& arena) {
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
      pipe = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(wire_handle)); }
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
    size_t* param_value_size_ret = param_value_size_ret_wanted ? &param_value_size_ret_storage : nullptr;
    const cl_int ret = s.on_clGetPipeInfo(pipe, param_name, param_value_size, param_value, param_value_size_ret);
    reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(ret)));
    if (param_value_wanted) {
        uint64_t wire_written = param_value_size_ret != nullptr
            ? static_cast<uint64_t>(*param_value_size_ret)
            : static_cast<uint64_t>(param_value_size);
        if (wire_written > static_cast<uint64_t>(param_value_size))
            wire_written = static_cast<uint64_t>(param_value_size);
        reply.write_varint(wire_written);
        if (wire_written != 0) reply.write(param_value, static_cast<size_t>(wire_written));
    }
    if (param_value_size_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(param_value_size_ret_storage));
    }
    return true;
}

static bool decode_clCreateProgramWithBuiltInKernels(ServerDispatch& s, ::av::WireReader& r,
                                                    ::av::WireWriter& reply, ::av::WireArena& arena) {
    (void)r;
    (void)reply;
    (void)arena;
    cl_context context = nullptr;
    cl_uint num_devices{};
    const cl_device_id* device_list = nullptr;
    const char* kernel_names = nullptr;
    bool errcode_ret_wanted = false;
    { uint64_t wire_handle = 0;
      if (!r.read_pod(wire_handle)) return false;
      context = reinterpret_cast<cl_context>(static_cast<uintptr_t>(wire_handle)); }
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
                wire_items[i] = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(wire_handle));
            }
            device_list = wire_items;
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
    cl_int errcode_ret_storage{};
    cl_int* errcode_ret = errcode_ret_wanted ? &errcode_ret_storage : nullptr;
    const cl_program ret = s.on_clCreateProgramWithBuiltInKernels(context, num_devices, device_list, kernel_names, errcode_ret);
    reply.write_pod<uint64_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ret)));
    if (errcode_ret_wanted) {
        reply.write_varint(static_cast<uint64_t>(errcode_ret_storage));
    }
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
        default: return false;
    }
}

}  // namespace av::clproto
