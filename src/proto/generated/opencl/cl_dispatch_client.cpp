// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opencl --xml cl.xml --out ...

#include "cl_dispatch_client.h"

#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"
#include "cl_protocol_ids.h"

namespace av::clproto {

cl_int rvk_clGetPlatformIDs(::av::ClientTransport& t, cl_uint num_entries, uint64_t* platforms, cl_uint* num_platforms) {
    ::av::WireWriter b = t.request_writer();
    b.write_varint(static_cast<uint64_t>(num_entries));
    b.write_pod<uint8_t>(platforms != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(num_platforms != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetPlatformIDs, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (platforms != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        for (uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(platforms[i])) return ret;
        }
    }
    if (num_platforms != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *num_platforms = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetPlatformInfo(::av::ClientTransport& t, uint64_t platform, cl_platform_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(platform);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetPlatformInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetDeviceIDs(::av::ClientTransport& t, uint64_t platform, cl_device_type device_type, cl_uint num_entries, uint64_t* devices, cl_uint* num_devices) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(platform);
    b.write_varint(static_cast<uint64_t>(device_type));
    b.write_varint(static_cast<uint64_t>(num_entries));
    b.write_pod<uint8_t>(devices != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(num_devices != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetDeviceIDs, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (devices != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        for (uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(devices[i])) return ret;
        }
    }
    if (num_devices != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *num_devices = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetDeviceInfo(::av::ClientTransport& t, uint64_t device, cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(device);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetDeviceInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateContext(::av::ClientTransport& t, const uint64_t* properties, cl_uint num_devices, const uint64_t* devices, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<uint64_t>(properties[i]);
        }
    }
    b.write_varint(static_cast<uint64_t>(num_devices));
    b.write_pod<uint8_t>(devices != nullptr ? 1 : 0);
    if (devices != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_devices));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_devices); ++i) {
            b.write_pod<uint64_t>(devices[i]);
        }
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateContext, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateContextFromType(::av::ClientTransport& t, const uint64_t* properties, cl_device_type device_type, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<uint64_t>(properties[i]);
        }
    }
    b.write_varint(static_cast<uint64_t>(device_type));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateContextFromType, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainContext(::av::ClientTransport& t, uint64_t context) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    if (t.send_async(kCmd_clRetainContext, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int rvk_clReleaseContext(::av::ClientTransport& t, uint64_t context) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseContext, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetContextInfo(::av::ClientTransport& t, uint64_t context, cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetContextInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateCommandQueue(::av::ClientTransport& t, uint64_t context, uint64_t device, cl_command_queue_properties properties, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint64_t>(device);
    b.write_varint(static_cast<uint64_t>(properties));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateCommandQueue, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateCommandQueueWithProperties(::av::ClientTransport& t, uint64_t context, uint64_t device, const uint64_t* properties, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint64_t>(device);
    b.write_pod<uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<uint64_t>(properties[i]);
        }
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateCommandQueueWithProperties, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainCommandQueue(::av::ClientTransport& t, uint64_t command_queue) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    if (t.send_async(kCmd_clRetainCommandQueue, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int rvk_clReleaseCommandQueue(::av::ClientTransport& t, uint64_t command_queue) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseCommandQueue, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetCommandQueueInfo(::av::ClientTransport& t, uint64_t command_queue, cl_command_queue_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetCommandQueueInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clFlush(::av::ClientTransport& t, uint64_t command_queue) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clFlush, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clFinish(::av::ClientTransport& t, uint64_t command_queue) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clFinish, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

uint64_t rvk_clCreateBuffer(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(size));
    b.write_pod<uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<uint64_t>(size));
        b.write(host_ptr, static_cast<size_t>(size));
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateSubBuffer(::av::ClientTransport& t, uint64_t buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const cl_buffer_region* buffer_create_info, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(buffer);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(buffer_create_type));
    b.write_pod<uint8_t>(buffer_create_info != nullptr ? 1 : 0);
    if (buffer_create_info != nullptr) b.write(buffer_create_info, sizeof(*buffer_create_info));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateSubBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateImage(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_pod<uint8_t>(image_format != nullptr ? 1 : 0);
    if (image_format != nullptr) b.write(image_format, sizeof(*image_format));
    b.write_pod<uint8_t>(image_desc != nullptr ? 1 : 0);
    if (image_desc != nullptr) b.write(image_desc, sizeof(*image_desc));
    b.write_pod<uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<uint64_t>(host_ptr_bytes));
        b.write(host_ptr, static_cast<size_t>(host_ptr_bytes));
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateImage, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainMemObject(::av::ClientTransport& t, uint64_t memobj) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(memobj);
    if (t.send_async(kCmd_clRetainMemObject, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int rvk_clReleaseMemObject(::av::ClientTransport& t, uint64_t memobj) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(memobj);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseMemObject, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetMemObjectInfo(::av::ClientTransport& t, uint64_t memobj, cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(memobj);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetMemObjectInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetImageInfo(::av::ClientTransport& t, uint64_t image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(image);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetImageInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetSupportedImageFormats(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries, cl_image_format* image_formats, size_t image_formats_bytes, cl_uint* num_image_formats) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(image_type));
    b.write_varint(static_cast<uint64_t>(num_entries));
    b.write_pod<uint8_t>(image_formats != nullptr ? 1 : 0);
    if (image_formats != nullptr) b.write_varint(static_cast<uint64_t>(image_formats_bytes));
    b.write_pod<uint8_t>(num_image_formats != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetSupportedImageFormats, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (image_formats != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(image_formats, static_cast<size_t>(wire_count))) return ret;
    }
    if (num_image_formats != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *num_image_formats = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int rvk_clEnqueueReadBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_read, size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(buffer);
    b.write_varint(static_cast<uint64_t>(blocking_read));
    b.write_varint(static_cast<uint64_t>(offset));
    b.write_varint(static_cast<uint64_t>(size));
    b.write_pod<uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) b.write_varint(static_cast<uint64_t>(size));
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueReadBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (ptr != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(ptr, static_cast<size_t>(wire_count))) return ret;
    }
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueWriteBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_write, size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(buffer);
    b.write_varint(static_cast<uint64_t>(blocking_write));
    b.write_varint(static_cast<uint64_t>(offset));
    b.write_varint(static_cast<uint64_t>(size));
    b.write_pod<uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) {
        b.write_varint(static_cast<uint64_t>(size));
        b.write(ptr, static_cast<size_t>(size));
    }
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueWriteBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueCopyBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_buffer, uint64_t dst_buffer, size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(src_buffer);
    b.write_pod<uint64_t>(dst_buffer);
    b.write_varint(static_cast<uint64_t>(src_offset));
    b.write_varint(static_cast<uint64_t>(dst_offset));
    b.write_varint(static_cast<uint64_t>(size));
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueCopyBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueFillBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(buffer);
    b.write_pod<uint8_t>(pattern != nullptr ? 1 : 0);
    if (pattern != nullptr) {
        b.write_varint(static_cast<uint64_t>(pattern_size));
        b.write(pattern, static_cast<size_t>(pattern_size));
    }
    b.write_varint(static_cast<uint64_t>(pattern_size));
    b.write_varint(static_cast<uint64_t>(offset));
    b.write_varint(static_cast<uint64_t>(size));
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueFillBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueReadImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t image, cl_bool blocking_read, const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(image);
    b.write_varint(static_cast<uint64_t>(blocking_read));
    b.write_pod<uint8_t>(origin != nullptr ? 1 : 0);
    if (origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(origin[i]));
        }
    }
    b.write_pod<uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<uint64_t>(row_pitch));
    b.write_varint(static_cast<uint64_t>(slice_pitch));
    b.write_pod<uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) b.write_varint(static_cast<uint64_t>(ptr_bytes));
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueReadImage, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (ptr != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(ptr, static_cast<size_t>(wire_count))) return ret;
    }
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueWriteImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t image, cl_bool blocking_write, const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(image);
    b.write_varint(static_cast<uint64_t>(blocking_write));
    b.write_pod<uint8_t>(origin != nullptr ? 1 : 0);
    if (origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(origin[i]));
        }
    }
    b.write_pod<uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<uint64_t>(input_row_pitch));
    b.write_varint(static_cast<uint64_t>(input_slice_pitch));
    b.write_pod<uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) {
        b.write_varint(static_cast<uint64_t>(ptr_bytes));
        b.write(ptr, static_cast<size_t>(ptr_bytes));
    }
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueWriteImage, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueCopyImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_image, uint64_t dst_image, const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(src_image);
    b.write_pod<uint64_t>(dst_image);
    b.write_pod<uint8_t>(src_origin != nullptr ? 1 : 0);
    if (src_origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(src_origin[i]));
        }
    }
    b.write_pod<uint8_t>(dst_origin != nullptr ? 1 : 0);
    if (dst_origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(dst_origin[i]));
        }
    }
    b.write_pod<uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueCopyImage, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueCopyImageToBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_image, uint64_t dst_buffer, const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(src_image);
    b.write_pod<uint64_t>(dst_buffer);
    b.write_pod<uint8_t>(src_origin != nullptr ? 1 : 0);
    if (src_origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(src_origin[i]));
        }
    }
    b.write_pod<uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<uint64_t>(dst_offset));
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueCopyImageToBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueCopyBufferToImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_buffer, uint64_t dst_image, size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(src_buffer);
    b.write_pod<uint64_t>(dst_image);
    b.write_varint(static_cast<uint64_t>(src_offset));
    b.write_pod<uint8_t>(dst_origin != nullptr ? 1 : 0);
    if (dst_origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(dst_origin[i]));
        }
    }
    b.write_pod<uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueCopyBufferToImage, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

uint64_t rvk_clCreateProgramWithSource(::av::ClientTransport& t, uint64_t context, cl_uint count, const char** strings, const size_t* lengths, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(count));
    b.write_pod<uint8_t>(strings != nullptr ? 1 : 0);
    if (strings != nullptr) {
        b.write_varint(static_cast<uint64_t>(count));
        for (uint64_t i = 0; i < static_cast<uint64_t>(count); ++i) {
            const size_t wire_len = (lengths != nullptr && lengths[i] != 0)
                ? lengths[i]
                : (strings[i] != nullptr ? __builtin_strlen(reinterpret_cast<const char*>(strings[i])) : 0);
            b.write_varint(wire_len);
            if (wire_len != 0) b.write(strings[i], wire_len);
        }
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateProgramWithSource, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateProgramWithBinary(::av::ClientTransport& t, uint64_t context, cl_uint num_devices, const uint64_t* device_list, const size_t* lengths, const unsigned char** binaries, cl_int* binary_status, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(num_devices));
    b.write_pod<uint8_t>(device_list != nullptr ? 1 : 0);
    if (device_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_devices));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_devices); ++i) {
            b.write_pod<uint64_t>(device_list[i]);
        }
    }
    b.write_pod<uint8_t>(binaries != nullptr ? 1 : 0);
    if (binaries != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_devices));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_devices); ++i) {
            const size_t wire_len = (lengths != nullptr && lengths[i] != 0)
                ? lengths[i]
                : (binaries[i] != nullptr ? __builtin_strlen(reinterpret_cast<const char*>(binaries[i])) : 0);
            b.write_varint(wire_len);
            if (wire_len != 0) b.write(binaries[i], wire_len);
        }
    }
    b.write_pod<uint8_t>(binary_status != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateProgramWithBinary, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (binary_status != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *binary_status = static_cast<cl_int>(wire_value);
    }
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainProgram(::av::ClientTransport& t, uint64_t program) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    if (t.send_async(kCmd_clRetainProgram, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int rvk_clReleaseProgram(::av::ClientTransport& t, uint64_t program) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseProgram, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clBuildProgram(::av::ClientTransport& t, uint64_t program, cl_uint num_devices, const uint64_t* device_list, const char* options) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_varint(static_cast<uint64_t>(num_devices));
    b.write_pod<uint8_t>(device_list != nullptr ? 1 : 0);
    if (device_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_devices));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_devices); ++i) {
            b.write_pod<uint64_t>(device_list[i]);
        }
    }
    b.write_pod<uint8_t>(options != nullptr ? 1 : 0);
    if (options != nullptr) {
        const size_t wire_len = __builtin_strlen(options);
        b.write_varint(wire_len);
        b.write(options, wire_len);
    }
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clBuildProgram, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetProgramInfo(::av::ClientTransport& t, uint64_t program, cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetProgramInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetProgramBuildInfo(::av::ClientTransport& t, uint64_t program, uint64_t device, cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_pod<uint64_t>(device);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetProgramBuildInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clUnloadPlatformCompiler(::av::ClientTransport& t, uint64_t platform) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(platform);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clUnloadPlatformCompiler, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

uint64_t rvk_clCreateKernel(::av::ClientTransport& t, uint64_t program, const char* kernel_name, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_pod<uint8_t>(kernel_name != nullptr ? 1 : 0);
    if (kernel_name != nullptr) {
        const size_t wire_len = __builtin_strlen(kernel_name);
        b.write_varint(wire_len);
        b.write(kernel_name, wire_len);
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateKernel, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clCreateKernelsInProgram(::av::ClientTransport& t, uint64_t program, cl_uint num_kernels, uint64_t* kernels, cl_uint* num_kernels_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_varint(static_cast<uint64_t>(num_kernels));
    b.write_pod<uint8_t>(kernels != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(num_kernels_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateKernelsInProgram, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (kernels != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        for (uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(kernels[i])) return ret;
        }
    }
    if (num_kernels_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *num_kernels_ret = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainKernel(::av::ClientTransport& t, uint64_t kernel) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    if (t.send_async(kCmd_clRetainKernel, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int rvk_clReleaseKernel(::av::ClientTransport& t, uint64_t kernel) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseKernel, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clSetKernelArg(::av::ClientTransport& t, uint64_t kernel, cl_uint arg_index, size_t arg_size, const void* arg_value) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_varint(static_cast<uint64_t>(arg_index));
    b.write_varint(static_cast<uint64_t>(arg_size));
    b.write_pod<uint8_t>(arg_value != nullptr ? 1 : 0);
    if (arg_value != nullptr) {
        b.write_varint(static_cast<uint64_t>(arg_size));
        b.write(arg_value, static_cast<size_t>(arg_size));
    }
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetKernelArg, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetKernelInfo(::av::ClientTransport& t, uint64_t kernel, cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetKernelInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetKernelArgInfo(::av::ClientTransport& t, uint64_t kernel, cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_varint(static_cast<uint64_t>(arg_index));
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetKernelArgInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetKernelWorkGroupInfo(::av::ClientTransport& t, uint64_t kernel, uint64_t device, cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_pod<uint64_t>(device);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetKernelWorkGroupInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clEnqueueNDRangeKernel(::av::ClientTransport& t, uint64_t command_queue, uint64_t kernel, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(kernel);
    b.write_varint(static_cast<uint64_t>(work_dim));
    b.write_pod<uint8_t>(global_work_offset != nullptr ? 1 : 0);
    if (global_work_offset != nullptr) {
        b.write_varint(static_cast<uint64_t>(work_dim));
        for (uint64_t i = 0; i < static_cast<uint64_t>(work_dim); ++i) {
            b.write_varint(static_cast<uint64_t>(global_work_offset[i]));
        }
    }
    b.write_pod<uint8_t>(global_work_size != nullptr ? 1 : 0);
    if (global_work_size != nullptr) {
        b.write_varint(static_cast<uint64_t>(work_dim));
        for (uint64_t i = 0; i < static_cast<uint64_t>(work_dim); ++i) {
            b.write_varint(static_cast<uint64_t>(global_work_size[i]));
        }
    }
    b.write_pod<uint8_t>(local_work_size != nullptr ? 1 : 0);
    if (local_work_size != nullptr) {
        b.write_varint(static_cast<uint64_t>(work_dim));
        for (uint64_t i = 0; i < static_cast<uint64_t>(work_dim); ++i) {
            b.write_varint(static_cast<uint64_t>(local_work_size[i]));
        }
    }
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueNDRangeKernel, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueMarkerWithWaitList(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueMarkerWithWaitList, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueBarrierWithWaitList(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueBarrierWithWaitList, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clWaitForEvents(::av::ClientTransport& t, cl_uint num_events, const uint64_t* event_list) {
    ::av::WireWriter b = t.request_writer();
    b.write_varint(static_cast<uint64_t>(num_events));
    b.write_pod<uint8_t>(event_list != nullptr ? 1 : 0);
    if (event_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events); ++i) {
            b.write_pod<uint64_t>(event_list[i]);
        }
    }
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clWaitForEvents, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetEventInfo(::av::ClientTransport& t, uint64_t event, cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(event);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetEventInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateUserEvent(::av::ClientTransport& t, uint64_t context, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateUserEvent, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clSetUserEventStatus(::av::ClientTransport& t, uint64_t event, cl_int execution_status) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(event);
    b.write_varint(static_cast<uint64_t>(execution_status));
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetUserEventStatus, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clRetainEvent(::av::ClientTransport& t, uint64_t event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(event);
    if (t.send_async(kCmd_clRetainEvent, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int rvk_clReleaseEvent(::av::ClientTransport& t, uint64_t event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(event);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseEvent, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetEventProfilingInfo(::av::ClientTransport& t, uint64_t event, cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(event);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetEventProfilingInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateFromGLBuffer(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLuint bufobj, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(bufobj));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateFromGLBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateFromGLTexture(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(target));
    b.write_varint(static_cast<uint64_t>(miplevel));
    b.write_varint(static_cast<uint64_t>(texture));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateFromGLTexture, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateFromGLTexture2D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(target));
    b.write_varint(static_cast<uint64_t>(miplevel));
    b.write_varint(static_cast<uint64_t>(texture));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateFromGLTexture2D, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateFromGLTexture3D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(target));
    b.write_varint(static_cast<uint64_t>(miplevel));
    b.write_varint(static_cast<uint64_t>(texture));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateFromGLTexture3D, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateFromGLRenderbuffer(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(renderbuffer));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateFromGLRenderbuffer, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetGLObjectInfo(::av::ClientTransport& t, uint64_t memobj, cl_gl_object_type* gl_object_type, cl_GLuint* gl_object_name) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(memobj);
    b.write_pod<uint8_t>(gl_object_type != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(gl_object_name != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetGLObjectInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (gl_object_type != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *gl_object_type = static_cast<cl_gl_object_type>(wire_value);
    }
    if (gl_object_name != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *gl_object_name = static_cast<cl_GLuint>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetGLTextureInfo(::av::ClientTransport& t, uint64_t memobj, cl_gl_texture_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(memobj);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetGLTextureInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clEnqueueAcquireGLObjects(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_objects, const uint64_t* mem_objects, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(num_objects));
    b.write_pod<uint8_t>(mem_objects != nullptr ? 1 : 0);
    if (mem_objects != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_objects));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_objects); ++i) {
            b.write_pod<uint64_t>(mem_objects[i]);
        }
    }
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueAcquireGLObjects, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueReleaseGLObjects(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_objects, const uint64_t* mem_objects, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(num_objects));
    b.write_pod<uint8_t>(mem_objects != nullptr ? 1 : 0);
    if (mem_objects != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_objects));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_objects); ++i) {
            b.write_pod<uint64_t>(mem_objects[i]);
        }
    }
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueReleaseGLObjects, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clGetGLContextInfoKHR(::av::ClientTransport& t, const uint64_t* properties, cl_gl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<uint64_t>(properties[i]);
        }
    }
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetGLContextInfoKHR, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainDevice(::av::ClientTransport& t, uint64_t device) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(device);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clRetainDevice, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clReleaseDevice(::av::ClientTransport& t, uint64_t device) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(device);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseDevice, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clCreateSubDevices(::av::ClientTransport& t, uint64_t in_device, const uint64_t* properties, cl_uint num_devices, uint64_t* out_devices, cl_uint* num_devices_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(in_device);
    b.write_pod<uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        uint64_t wire_count = 0;
        while (properties[wire_count] != 0) ++wire_count;
        b.write_varint(wire_count);
        for (uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<uint64_t>(static_cast<uint64_t>(properties[i]));
        }
    }
    b.write_varint(static_cast<uint64_t>(num_devices));
    b.write_pod<uint8_t>(out_devices != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(num_devices_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateSubDevices, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (out_devices != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        for (uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(out_devices[i])) return ret;
        }
    }
    if (num_devices_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *num_devices_ret = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int rvk_clEnqueueTask(::av::ClientTransport& t, uint64_t command_queue, uint64_t kernel, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(kernel);
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueTask, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueMarker(::av::ClientTransport& t, uint64_t command_queue, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueMarker, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueBarrier(::av::ClientTransport& t, uint64_t command_queue) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueBarrier, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clEnqueueWaitForEvents(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_events, const uint64_t* event_list) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(num_events));
    b.write_pod<uint8_t>(event_list != nullptr ? 1 : 0);
    if (event_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events); ++i) {
            b.write_pod<uint64_t>(event_list[i]);
        }
    }
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueWaitForEvents, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clEnqueueMigrateMemObjects(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_mem_objects, const uint64_t* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(num_mem_objects));
    b.write_pod<uint8_t>(mem_objects != nullptr ? 1 : 0);
    if (mem_objects != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_mem_objects));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_mem_objects); ++i) {
            b.write_pod<uint64_t>(mem_objects[i]);
        }
    }
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueMigrateMemObjects, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueReadBufferRect(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_read, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(buffer);
    b.write_varint(static_cast<uint64_t>(blocking_read));
    b.write_pod<uint8_t>(buffer_origin != nullptr ? 1 : 0);
    if (buffer_origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(buffer_origin[i]));
        }
    }
    b.write_pod<uint8_t>(host_origin != nullptr ? 1 : 0);
    if (host_origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(host_origin[i]));
        }
    }
    b.write_pod<uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<uint64_t>(buffer_row_pitch));
    b.write_varint(static_cast<uint64_t>(buffer_slice_pitch));
    b.write_varint(static_cast<uint64_t>(host_row_pitch));
    b.write_varint(static_cast<uint64_t>(host_slice_pitch));
    b.write_pod<uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) b.write_varint(static_cast<uint64_t>(ptr_bytes));
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueReadBufferRect, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (ptr != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(ptr, static_cast<size_t>(wire_count))) return ret;
    }
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueWriteBufferRect(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_write, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(buffer);
    b.write_varint(static_cast<uint64_t>(blocking_write));
    b.write_pod<uint8_t>(buffer_origin != nullptr ? 1 : 0);
    if (buffer_origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(buffer_origin[i]));
        }
    }
    b.write_pod<uint8_t>(host_origin != nullptr ? 1 : 0);
    if (host_origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(host_origin[i]));
        }
    }
    b.write_pod<uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<uint64_t>(buffer_row_pitch));
    b.write_varint(static_cast<uint64_t>(buffer_slice_pitch));
    b.write_varint(static_cast<uint64_t>(host_row_pitch));
    b.write_varint(static_cast<uint64_t>(host_slice_pitch));
    b.write_pod<uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) {
        b.write_varint(static_cast<uint64_t>(ptr_bytes));
        b.write(ptr, static_cast<size_t>(ptr_bytes));
    }
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueWriteBufferRect, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueCopyBufferRect(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_buffer, uint64_t dst_buffer, const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch, size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(src_buffer);
    b.write_pod<uint64_t>(dst_buffer);
    b.write_pod<uint8_t>(src_origin != nullptr ? 1 : 0);
    if (src_origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(src_origin[i]));
        }
    }
    b.write_pod<uint8_t>(dst_origin != nullptr ? 1 : 0);
    if (dst_origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(dst_origin[i]));
        }
    }
    b.write_pod<uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<uint64_t>(src_row_pitch));
    b.write_varint(static_cast<uint64_t>(src_slice_pitch));
    b.write_varint(static_cast<uint64_t>(dst_row_pitch));
    b.write_varint(static_cast<uint64_t>(dst_slice_pitch));
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueCopyBufferRect, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueFillImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t image, const void* fill_color, size_t fill_color_bytes, const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(image);
    b.write_pod<uint8_t>(fill_color != nullptr ? 1 : 0);
    if (fill_color != nullptr) {
        b.write_varint(static_cast<uint64_t>(fill_color_bytes));
        b.write(fill_color, static_cast<size_t>(fill_color_bytes));
    }
    b.write_pod<uint8_t>(origin != nullptr ? 1 : 0);
    if (origin != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(origin[i]));
        }
    }
    b.write_pod<uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<uint64_t>(3));
        for (uint64_t i = 0; i < static_cast<uint64_t>(3); ++i) {
            b.write_varint(static_cast<uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueFillImage, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

uint64_t rvk_clCreateImage2D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_pod<uint8_t>(image_format != nullptr ? 1 : 0);
    if (image_format != nullptr) b.write(image_format, sizeof(*image_format));
    b.write_varint(static_cast<uint64_t>(image_width));
    b.write_varint(static_cast<uint64_t>(image_height));
    b.write_varint(static_cast<uint64_t>(image_row_pitch));
    b.write_pod<uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<uint64_t>(host_ptr_bytes));
        b.write(host_ptr, static_cast<size_t>(host_ptr_bytes));
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateImage2D, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateImage3D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_pod<uint8_t>(image_format != nullptr ? 1 : 0);
    if (image_format != nullptr) b.write(image_format, sizeof(*image_format));
    b.write_varint(static_cast<uint64_t>(image_width));
    b.write_varint(static_cast<uint64_t>(image_height));
    b.write_varint(static_cast<uint64_t>(image_depth));
    b.write_varint(static_cast<uint64_t>(image_row_pitch));
    b.write_varint(static_cast<uint64_t>(image_slice_pitch));
    b.write_pod<uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<uint64_t>(host_ptr_bytes));
        b.write(host_ptr, static_cast<size_t>(host_ptr_bytes));
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateImage3D, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clUnloadCompiler(::av::ClientTransport& t) {
    ::av::WireWriter b = t.request_writer();
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clUnloadCompiler, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clSetCommandQueueProperty(::av::ClientTransport& t, uint64_t command_queue, cl_command_queue_properties properties, cl_bool enable, cl_command_queue_properties* old_properties) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(properties));
    b.write_varint(static_cast<uint64_t>(enable));
    b.write_pod<uint8_t>(old_properties != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetCommandQueueProperty, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (old_properties != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *old_properties = static_cast<cl_command_queue_properties>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateProgramWithIL(::av::ClientTransport& t, uint64_t context, const void* il, size_t length, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint8_t>(il != nullptr ? 1 : 0);
    if (il != nullptr) {
        b.write_varint(static_cast<uint64_t>(length));
        b.write(il, static_cast<size_t>(length));
    }
    b.write_varint(static_cast<uint64_t>(length));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateProgramWithIL, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clCompileProgram(::av::ClientTransport& t, uint64_t program, cl_uint num_devices, const uint64_t* device_list, const char* options, cl_uint num_input_headers, const uint64_t* input_headers, const char** header_include_names) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_varint(static_cast<uint64_t>(num_devices));
    b.write_pod<uint8_t>(device_list != nullptr ? 1 : 0);
    if (device_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_devices));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_devices); ++i) {
            b.write_pod<uint64_t>(device_list[i]);
        }
    }
    b.write_pod<uint8_t>(options != nullptr ? 1 : 0);
    if (options != nullptr) {
        const size_t wire_len = __builtin_strlen(options);
        b.write_varint(wire_len);
        b.write(options, wire_len);
    }
    b.write_varint(static_cast<uint64_t>(num_input_headers));
    b.write_pod<uint8_t>(input_headers != nullptr ? 1 : 0);
    if (input_headers != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_input_headers));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_input_headers); ++i) {
            b.write_pod<uint64_t>(input_headers[i]);
        }
    }
    b.write_pod<uint8_t>(header_include_names != nullptr ? 1 : 0);
    if (header_include_names != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_input_headers));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_input_headers); ++i) {
            const size_t wire_len = header_include_names[i] != nullptr ? __builtin_strlen(header_include_names[i]) : 0;
            b.write_varint(wire_len);
            if (wire_len != 0) b.write(header_include_names[i], wire_len);
        }
    }
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCompileProgram, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

uint64_t rvk_clLinkProgram(::av::ClientTransport& t, uint64_t context, cl_uint num_devices, const uint64_t* device_list, const char* options, cl_uint num_input_programs, const uint64_t* input_programs, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(num_devices));
    b.write_pod<uint8_t>(device_list != nullptr ? 1 : 0);
    if (device_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_devices));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_devices); ++i) {
            b.write_pod<uint64_t>(device_list[i]);
        }
    }
    b.write_pod<uint8_t>(options != nullptr ? 1 : 0);
    if (options != nullptr) {
        const size_t wire_len = __builtin_strlen(options);
        b.write_varint(wire_len);
        b.write(options, wire_len);
    }
    b.write_varint(static_cast<uint64_t>(num_input_programs));
    b.write_pod<uint8_t>(input_programs != nullptr ? 1 : 0);
    if (input_programs != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_input_programs));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_input_programs); ++i) {
            b.write_pod<uint64_t>(input_programs[i]);
        }
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clLinkProgram, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clSetProgramSpecializationConstant(::av::ClientTransport& t, uint64_t program, cl_uint spec_id, size_t spec_size, const void* spec_value) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_varint(static_cast<uint64_t>(spec_id));
    b.write_varint(static_cast<uint64_t>(spec_size));
    b.write_pod<uint8_t>(spec_value != nullptr ? 1 : 0);
    if (spec_value != nullptr) {
        b.write_varint(static_cast<uint64_t>(spec_size));
        b.write(spec_value, static_cast<size_t>(spec_size));
    }
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetProgramSpecializationConstant, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

uint64_t rvk_clCloneKernel(::av::ClientTransport& t, uint64_t source_kernel, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(source_kernel);
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCloneKernel, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clSetKernelExecInfo(::av::ClientTransport& t, uint64_t kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    if (param_value != nullptr) {
        b.write_varint(static_cast<uint64_t>(param_value_size));
        b.write(param_value, static_cast<size_t>(param_value_size));
    }
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetKernelExecInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetKernelSubGroupInfo(::av::ClientTransport& t, uint64_t kernel, uint64_t device, cl_kernel_sub_group_info param_name, size_t input_value_size, const void* input_value, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_pod<uint64_t>(device);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(input_value_size));
    b.write_pod<uint8_t>(input_value != nullptr ? 1 : 0);
    if (input_value != nullptr) {
        b.write_varint(static_cast<uint64_t>(input_value_size));
        b.write(input_value, static_cast<size_t>(input_value_size));
    }
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetKernelSubGroupInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetHostTimer(::av::ClientTransport& t, uint64_t device, cl_ulong* host_timestamp) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(device);
    b.write_pod<uint8_t>(host_timestamp != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetHostTimer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (host_timestamp != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *host_timestamp = static_cast<cl_ulong>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetDeviceAndHostTimer(::av::ClientTransport& t, uint64_t device, cl_ulong* device_timestamp, cl_ulong* host_timestamp) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(device);
    b.write_pod<uint8_t>(device_timestamp != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(host_timestamp != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetDeviceAndHostTimer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (device_timestamp != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *device_timestamp = static_cast<cl_ulong>(wire_value);
    }
    if (host_timestamp != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *host_timestamp = static_cast<cl_ulong>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateBufferWithProperties(::av::ClientTransport& t, uint64_t context, const uint64_t* properties, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<uint64_t>(properties[i]);
        }
    }
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(size));
    b.write_pod<uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<uint64_t>(size));
        b.write(host_ptr, static_cast<size_t>(size));
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateBufferWithProperties, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateImageWithProperties(::av::ClientTransport& t, uint64_t context, const uint64_t* properties, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<uint64_t>(properties[i]);
        }
    }
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_pod<uint8_t>(image_format != nullptr ? 1 : 0);
    if (image_format != nullptr) b.write(image_format, sizeof(*image_format));
    b.write_pod<uint8_t>(image_desc != nullptr ? 1 : 0);
    if (image_desc != nullptr) b.write(image_desc, sizeof(*image_desc));
    b.write_pod<uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<uint64_t>(host_ptr_bytes));
        b.write(host_ptr, static_cast<size_t>(host_ptr_bytes));
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateImageWithProperties, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateSamplerWithProperties(::av::ClientTransport& t, uint64_t context, const uint64_t* sampler_properties, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint8_t>(sampler_properties != nullptr ? 1 : 0);
    if (sampler_properties != nullptr) {
        uint64_t wire_count = 0;
        while (sampler_properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<uint64_t>(sampler_properties[i]);
        }
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateSamplerWithProperties, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clSetDefaultDeviceCommandQueue(::av::ClientTransport& t, uint64_t context, uint64_t device, uint64_t command_queue) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint64_t>(device);
    b.write_pod<uint64_t>(command_queue);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetDefaultDeviceCommandQueue, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

uint64_t rvk_clCreateSampler(::av::ClientTransport& t, uint64_t context, cl_bool normalized_coords, cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(normalized_coords));
    b.write_varint(static_cast<uint64_t>(addressing_mode));
    b.write_varint(static_cast<uint64_t>(filter_mode));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateSampler, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainSampler(::av::ClientTransport& t, uint64_t sampler) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(sampler);
    if (t.send_async(kCmd_clRetainSampler, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int rvk_clReleaseSampler(::av::ClientTransport& t, uint64_t sampler) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(sampler);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseSampler, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetSamplerInfo(::av::ClientTransport& t, uint64_t sampler, cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(sampler);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetSamplerInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreatePipe(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_uint pipe_packet_size, cl_uint pipe_max_packets, const uint64_t* properties, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(pipe_packet_size));
    b.write_varint(static_cast<uint64_t>(pipe_max_packets));
    b.write_pod<uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<uint64_t>(properties[i]);
        }
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreatePipe, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetPipeInfo(::av::ClientTransport& t, uint64_t pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(pipe);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetPipeInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t rvk_clCreateProgramWithBuiltInKernels(::av::ClientTransport& t, uint64_t context, cl_uint num_devices, const uint64_t* device_list, const char* kernel_names, cl_int* errcode_ret) {
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(num_devices));
    b.write_pod<uint8_t>(device_list != nullptr ? 1 : 0);
    if (device_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_devices));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_devices); ++i) {
            b.write_pod<uint64_t>(device_list[i]);
        }
    }
    b.write_pod<uint8_t>(kernel_names != nullptr ? 1 : 0);
    if (kernel_names != nullptr) {
        const size_t wire_len = __builtin_strlen(kernel_names);
        b.write_varint(wire_len);
        b.write(kernel_names, wire_len);
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateProgramWithBuiltInKernels, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

}  // namespace av::clproto
