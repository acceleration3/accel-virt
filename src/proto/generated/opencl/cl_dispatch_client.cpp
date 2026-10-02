// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.opencl --xml cl.xml --out ...

#include "cl_dispatch_client.h"

#include "core/trace/function_scope.hpp"
#include "proto/client/sync_call_scope.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"
#include "cl_protocol_ids.h"

namespace av::clproto {

cl_int av_clGetPlatformIDs(::av::ClientTransport& t, cl_uint num_entries, uint64_t* platforms, cl_uint* num_platforms) {
    AV_SCOPE("av_clGetPlatformIDs");
    ::av::WireWriter b = t.request_writer();
    b.write_varint(static_cast<uint64_t>(num_entries));
    b.write_pod<uint8_t>(platforms != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(num_platforms != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetPlatformIDs, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (platforms != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(num_entries)) return CL_OUT_OF_RESOURCES;
        for (uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(platforms[i])) return CL_OUT_OF_RESOURCES;
        }
    }
    if (num_platforms != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        *num_platforms = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int av_clGetPlatformInfo(::av::ClientTransport& t, uint64_t platform, cl_platform_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetPlatformInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(platform);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetPlatformInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clGetDeviceIDs(::av::ClientTransport& t, uint64_t platform, cl_device_type device_type, cl_uint num_entries, uint64_t* devices, cl_uint* num_devices) {
    AV_SCOPE("av_clGetDeviceIDs");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(platform);
    b.write_varint(static_cast<uint64_t>(device_type));
    b.write_varint(static_cast<uint64_t>(num_entries));
    b.write_pod<uint8_t>(devices != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(num_devices != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetDeviceIDs, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (devices != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(num_entries)) return CL_OUT_OF_RESOURCES;
        for (uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(devices[i])) return CL_OUT_OF_RESOURCES;
        }
    }
    if (num_devices != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        *num_devices = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int av_clGetDeviceInfo(::av::ClientTransport& t, uint64_t device, cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetDeviceInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(device);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetDeviceInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t av_clCreateContext(::av::ClientTransport& t, const uint64_t* properties, cl_uint num_devices, const uint64_t* devices, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateContext");
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
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateContextFromType(::av::ClientTransport& t, const uint64_t* properties, cl_device_type device_type, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateContextFromType");
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
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clRetainContext(::av::ClientTransport& t, uint64_t context) {
    AV_SCOPE("av_clRetainContext");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    if (t.send_async(kCmd_clRetainContext, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int av_clReleaseContext(::av::ClientTransport& t, uint64_t context) {
    AV_SCOPE("av_clReleaseContext");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseContext, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clGetContextInfo(::av::ClientTransport& t, uint64_t context, cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetContextInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetContextInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t av_clCreateCommandQueue(::av::ClientTransport& t, uint64_t context, uint64_t device, cl_command_queue_properties properties, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateCommandQueue");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint64_t>(device);
    b.write_varint(static_cast<uint64_t>(properties));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateCommandQueueWithProperties(::av::ClientTransport& t, uint64_t context, uint64_t device, const uint64_t* properties, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateCommandQueueWithProperties");
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
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clRetainCommandQueue(::av::ClientTransport& t, uint64_t command_queue) {
    AV_SCOPE("av_clRetainCommandQueue");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    if (t.send_async(kCmd_clRetainCommandQueue, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int av_clReleaseCommandQueue(::av::ClientTransport& t, uint64_t command_queue) {
    AV_SCOPE("av_clReleaseCommandQueue");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseCommandQueue, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clGetCommandQueueInfo(::av::ClientTransport& t, uint64_t command_queue, cl_command_queue_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetCommandQueueInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetCommandQueueInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clFlush(::av::ClientTransport& t, uint64_t command_queue) {
    AV_SCOPE("av_clFlush");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clFlush, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clFinish(::av::ClientTransport& t, uint64_t command_queue) {
    AV_SCOPE("av_clFinish");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clFinish, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

uint64_t av_clCreateBuffer(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateBuffer");
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
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateSubBuffer(::av::ClientTransport& t, uint64_t buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const cl_buffer_region* buffer_create_info, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateSubBuffer");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(buffer);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(buffer_create_type));
    b.write_pod<uint8_t>(buffer_create_info != nullptr ? 1 : 0);
    if (buffer_create_info != nullptr) b.write(buffer_create_info, sizeof(*buffer_create_info));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateImage(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateImage");
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
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clRetainMemObject(::av::ClientTransport& t, uint64_t memobj) {
    AV_SCOPE("av_clRetainMemObject");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(memobj);
    if (t.send_async(kCmd_clRetainMemObject, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int av_clReleaseMemObject(::av::ClientTransport& t, uint64_t memobj) {
    AV_SCOPE("av_clReleaseMemObject");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(memobj);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseMemObject, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clGetMemObjectInfo(::av::ClientTransport& t, uint64_t memobj, cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetMemObjectInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(memobj);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetMemObjectInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clGetImageInfo(::av::ClientTransport& t, uint64_t image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetImageInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(image);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetImageInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clGetSupportedImageFormats(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries, cl_image_format* image_formats, size_t image_formats_bytes, cl_uint* num_image_formats) {
    AV_SCOPE("av_clGetSupportedImageFormats");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(image_type));
    b.write_varint(static_cast<uint64_t>(num_entries));
    b.write_pod<uint8_t>(image_formats != nullptr ? 1 : 0);
    if (image_formats != nullptr) b.write_varint(static_cast<uint64_t>(image_formats_bytes));
    b.write_pod<uint8_t>(num_image_formats != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetSupportedImageFormats, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (image_formats != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(image_formats_bytes)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(image_formats, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (num_image_formats != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        *num_image_formats = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int av_clEnqueueReadBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_read, size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueReadBuffer");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueReadBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (ptr != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(ptr, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueWriteBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_write, size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueWriteBuffer");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueWriteBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueCopyBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_buffer, uint64_t dst_buffer, size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueCopyBuffer");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueCopyBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueFillBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueFillBuffer");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueFillBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueReadImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t image, cl_bool blocking_read, const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueReadImage");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueReadImage, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (ptr != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(ptr_bytes)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(ptr, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueWriteImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t image, cl_bool blocking_write, const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueWriteImage");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueWriteImage, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueCopyImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_image, uint64_t dst_image, const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueCopyImage");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueCopyImage, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueCopyImageToBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_image, uint64_t dst_buffer, const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueCopyImageToBuffer");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueCopyImageToBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueCopyBufferToImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_buffer, uint64_t dst_image, size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueCopyBufferToImage");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueCopyBufferToImage, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

uint64_t av_clCreateProgramWithSource(::av::ClientTransport& t, uint64_t context, cl_uint count, const char** strings, const size_t* lengths, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateProgramWithSource");
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
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateProgramWithBinary(::av::ClientTransport& t, uint64_t context, cl_uint num_devices, const uint64_t* device_list, const size_t* lengths, const unsigned char** binaries, cl_int* binary_status, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateProgramWithBinary");
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
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clRetainProgram(::av::ClientTransport& t, uint64_t program) {
    AV_SCOPE("av_clRetainProgram");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    if (t.send_async(kCmd_clRetainProgram, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int av_clReleaseProgram(::av::ClientTransport& t, uint64_t program) {
    AV_SCOPE("av_clReleaseProgram");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseProgram, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clBuildProgram(::av::ClientTransport& t, uint64_t program, cl_uint num_devices, const uint64_t* device_list, const char* options) {
    AV_SCOPE("av_clBuildProgram");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clBuildProgram, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clGetProgramInfo(::av::ClientTransport& t, uint64_t program, cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetProgramInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetProgramInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clGetProgramBuildInfo(::av::ClientTransport& t, uint64_t program, uint64_t device, cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetProgramBuildInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_pod<uint64_t>(device);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetProgramBuildInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clUnloadPlatformCompiler(::av::ClientTransport& t, uint64_t platform) {
    AV_SCOPE("av_clUnloadPlatformCompiler");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(platform);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clUnloadPlatformCompiler, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

uint64_t av_clCreateKernel(::av::ClientTransport& t, uint64_t program, const char* kernel_name, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateKernel");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_pod<uint8_t>(kernel_name != nullptr ? 1 : 0);
    if (kernel_name != nullptr) {
        const size_t wire_len = __builtin_strlen(kernel_name);
        b.write_varint(wire_len);
        b.write(kernel_name, wire_len);
    }
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clCreateKernelsInProgram(::av::ClientTransport& t, uint64_t program, cl_uint num_kernels, uint64_t* kernels, cl_uint* num_kernels_ret) {
    AV_SCOPE("av_clCreateKernelsInProgram");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_varint(static_cast<uint64_t>(num_kernels));
    b.write_pod<uint8_t>(kernels != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(num_kernels_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateKernelsInProgram, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (kernels != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(num_kernels)) return CL_OUT_OF_RESOURCES;
        for (uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(kernels[i])) return CL_OUT_OF_RESOURCES;
        }
    }
    if (num_kernels_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        *num_kernels_ret = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int av_clRetainKernel(::av::ClientTransport& t, uint64_t kernel) {
    AV_SCOPE("av_clRetainKernel");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    if (t.send_async(kCmd_clRetainKernel, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int av_clReleaseKernel(::av::ClientTransport& t, uint64_t kernel) {
    AV_SCOPE("av_clReleaseKernel");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseKernel, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clSetKernelArg(::av::ClientTransport& t, uint64_t kernel, cl_uint arg_index, size_t arg_size, const void* arg_value) {
    AV_SCOPE("av_clSetKernelArg");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_varint(static_cast<uint64_t>(arg_index));
    b.write_varint(static_cast<uint64_t>(arg_size));
    b.write_pod<uint8_t>(arg_value != nullptr ? 1 : 0);
    if (arg_value != nullptr) {
        b.write_varint(static_cast<uint64_t>(arg_size));
        b.write(arg_value, static_cast<size_t>(arg_size));
    }
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetKernelArg, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clGetKernelInfo(::av::ClientTransport& t, uint64_t kernel, cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetKernelInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetKernelInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clGetKernelArgInfo(::av::ClientTransport& t, uint64_t kernel, cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetKernelArgInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_varint(static_cast<uint64_t>(arg_index));
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetKernelArgInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clGetKernelWorkGroupInfo(::av::ClientTransport& t, uint64_t kernel, uint64_t device, cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetKernelWorkGroupInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_pod<uint64_t>(device);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetKernelWorkGroupInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clEnqueueNDRangeKernel(::av::ClientTransport& t, uint64_t command_queue, uint64_t kernel, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueNDRangeKernel");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueNDRangeKernel, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueMarkerWithWaitList(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueMarkerWithWaitList");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueMarkerWithWaitList, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueBarrierWithWaitList(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueBarrierWithWaitList");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueBarrierWithWaitList, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clWaitForEvents(::av::ClientTransport& t, cl_uint num_events, const uint64_t* event_list) {
    AV_SCOPE("av_clWaitForEvents");
    ::av::WireWriter b = t.request_writer();
    b.write_varint(static_cast<uint64_t>(num_events));
    b.write_pod<uint8_t>(event_list != nullptr ? 1 : 0);
    if (event_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events); ++i) {
            b.write_pod<uint64_t>(event_list[i]);
        }
    }
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clWaitForEvents, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clGetEventInfo(::av::ClientTransport& t, uint64_t event, cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetEventInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(event);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetEventInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t av_clCreateUserEvent(::av::ClientTransport& t, uint64_t context, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateUserEvent");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clSetUserEventStatus(::av::ClientTransport& t, uint64_t event, cl_int execution_status) {
    AV_SCOPE("av_clSetUserEventStatus");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(event);
    b.write_varint(static_cast<uint64_t>(execution_status));
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetUserEventStatus, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clRetainEvent(::av::ClientTransport& t, uint64_t event) {
    AV_SCOPE("av_clRetainEvent");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(event);
    if (t.send_async(kCmd_clRetainEvent, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int av_clReleaseEvent(::av::ClientTransport& t, uint64_t event) {
    AV_SCOPE("av_clReleaseEvent");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(event);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseEvent, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clGetEventProfilingInfo(::av::ClientTransport& t, uint64_t event, cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetEventProfilingInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(event);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetEventProfilingInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t av_clCreateFromGLBuffer(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLuint bufobj, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateFromGLBuffer");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(bufobj));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateFromGLTexture(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateFromGLTexture");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(target));
    b.write_varint(static_cast<uint64_t>(miplevel));
    b.write_varint(static_cast<uint64_t>(texture));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateFromGLTexture2D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateFromGLTexture2D");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(target));
    b.write_varint(static_cast<uint64_t>(miplevel));
    b.write_varint(static_cast<uint64_t>(texture));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateFromGLTexture3D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateFromGLTexture3D");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(target));
    b.write_varint(static_cast<uint64_t>(miplevel));
    b.write_varint(static_cast<uint64_t>(texture));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateFromGLRenderbuffer(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateFromGLRenderbuffer");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(renderbuffer));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clGetGLObjectInfo(::av::ClientTransport& t, uint64_t memobj, cl_gl_object_type* gl_object_type, cl_GLuint* gl_object_name) {
    AV_SCOPE("av_clGetGLObjectInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(memobj);
    b.write_pod<uint8_t>(gl_object_type != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(gl_object_name != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetGLObjectInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (gl_object_type != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        *gl_object_type = static_cast<cl_gl_object_type>(wire_value);
    }
    if (gl_object_name != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        *gl_object_name = static_cast<cl_GLuint>(wire_value);
    }
    return ret;
}

cl_int av_clGetGLTextureInfo(::av::ClientTransport& t, uint64_t memobj, cl_gl_texture_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetGLTextureInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(memobj);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetGLTextureInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clEnqueueAcquireGLObjects(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_objects, const uint64_t* mem_objects, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueAcquireGLObjects");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueAcquireGLObjects, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueReleaseGLObjects(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_objects, const uint64_t* mem_objects, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueReleaseGLObjects");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueReleaseGLObjects, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clGetGLContextInfoKHR(::av::ClientTransport& t, const uint64_t* properties, cl_gl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetGLContextInfoKHR");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetGLContextInfoKHR, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clRetainDevice(::av::ClientTransport& t, uint64_t device) {
    AV_SCOPE("av_clRetainDevice");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(device);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clRetainDevice, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clReleaseDevice(::av::ClientTransport& t, uint64_t device) {
    AV_SCOPE("av_clReleaseDevice");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(device);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseDevice, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clCreateSubDevices(::av::ClientTransport& t, uint64_t in_device, const uint64_t* properties, cl_uint num_devices, uint64_t* out_devices, cl_uint* num_devices_ret) {
    AV_SCOPE("av_clCreateSubDevices");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCreateSubDevices, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (out_devices != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(num_devices)) return CL_OUT_OF_RESOURCES;
        for (uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(out_devices[i])) return CL_OUT_OF_RESOURCES;
        }
    }
    if (num_devices_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        *num_devices_ret = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int av_clEnqueueTask(::av::ClientTransport& t, uint64_t command_queue, uint64_t kernel, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueTask");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueTask, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueMarker(::av::ClientTransport& t, uint64_t command_queue, uint64_t* event) {
    AV_SCOPE("av_clEnqueueMarker");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueMarker, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueBarrier(::av::ClientTransport& t, uint64_t command_queue) {
    AV_SCOPE("av_clEnqueueBarrier");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueBarrier, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clEnqueueWaitForEvents(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_events, const uint64_t* event_list) {
    AV_SCOPE("av_clEnqueueWaitForEvents");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueWaitForEvents, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clEnqueueMigrateMemObjects(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_mem_objects, const uint64_t* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueMigrateMemObjects");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueMigrateMemObjects, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueReadBufferRect(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_read, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueReadBufferRect");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueReadBufferRect, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (ptr != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(ptr_bytes)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(ptr, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueWriteBufferRect(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_write, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueWriteBufferRect");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueWriteBufferRect, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueCopyBufferRect(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_buffer, uint64_t dst_buffer, const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch, size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueCopyBufferRect");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueCopyBufferRect, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueFillImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t image, const void* fill_color, size_t fill_color_bytes, const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueFillImage");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueFillImage, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

uint64_t av_clCreateImage2D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateImage2D");
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
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateImage3D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateImage3D");
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
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clUnloadCompiler(::av::ClientTransport& t) {
    AV_SCOPE("av_clUnloadCompiler");
    ::av::WireWriter b = t.request_writer();
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clUnloadCompiler, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clSetCommandQueueProperty(::av::ClientTransport& t, uint64_t command_queue, cl_command_queue_properties properties, cl_bool enable, cl_command_queue_properties* old_properties) {
    AV_SCOPE("av_clSetCommandQueueProperty");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(properties));
    b.write_varint(static_cast<uint64_t>(enable));
    b.write_pod<uint8_t>(old_properties != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetCommandQueueProperty, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (old_properties != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        *old_properties = static_cast<cl_command_queue_properties>(wire_value);
    }
    return ret;
}

uint64_t av_clCreateProgramWithIL(::av::ClientTransport& t, uint64_t context, const void* il, size_t length, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateProgramWithIL");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint8_t>(il != nullptr ? 1 : 0);
    if (il != nullptr) {
        b.write_varint(static_cast<uint64_t>(length));
        b.write(il, static_cast<size_t>(length));
    }
    b.write_varint(static_cast<uint64_t>(length));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clCompileProgram(::av::ClientTransport& t, uint64_t program, cl_uint num_devices, const uint64_t* device_list, const char* options, cl_uint num_input_headers, const uint64_t* input_headers, const char** header_include_names) {
    AV_SCOPE("av_clCompileProgram");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clCompileProgram, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

uint64_t av_clLinkProgram(::av::ClientTransport& t, uint64_t context, cl_uint num_devices, const uint64_t* device_list, const char* options, cl_uint num_input_programs, const uint64_t* input_programs, cl_int* errcode_ret) {
    AV_SCOPE("av_clLinkProgram");
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
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clSetProgramSpecializationConstant(::av::ClientTransport& t, uint64_t program, cl_uint spec_id, size_t spec_size, const void* spec_value) {
    AV_SCOPE("av_clSetProgramSpecializationConstant");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(program);
    b.write_varint(static_cast<uint64_t>(spec_id));
    b.write_varint(static_cast<uint64_t>(spec_size));
    b.write_pod<uint8_t>(spec_value != nullptr ? 1 : 0);
    if (spec_value != nullptr) {
        b.write_varint(static_cast<uint64_t>(spec_size));
        b.write(spec_value, static_cast<size_t>(spec_size));
    }
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetProgramSpecializationConstant, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

uint64_t av_clCloneKernel(::av::ClientTransport& t, uint64_t source_kernel, cl_int* errcode_ret) {
    AV_SCOPE("av_clCloneKernel");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(source_kernel);
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clSetKernelExecInfo(::av::ClientTransport& t, uint64_t kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value) {
    AV_SCOPE("av_clSetKernelExecInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    if (param_value != nullptr) {
        b.write_varint(static_cast<uint64_t>(param_value_size));
        b.write(param_value, static_cast<size_t>(param_value_size));
    }
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetKernelExecInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clGetKernelSubGroupInfo(::av::ClientTransport& t, uint64_t kernel, uint64_t device, cl_kernel_sub_group_info param_name, size_t input_value_size, const void* input_value, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetKernelSubGroupInfo");
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetKernelSubGroupInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int av_clGetHostTimer(::av::ClientTransport& t, uint64_t device, cl_ulong* host_timestamp) {
    AV_SCOPE("av_clGetHostTimer");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(device);
    b.write_pod<uint8_t>(host_timestamp != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetHostTimer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (host_timestamp != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        *host_timestamp = static_cast<cl_ulong>(wire_value);
    }
    return ret;
}

cl_int av_clGetDeviceAndHostTimer(::av::ClientTransport& t, uint64_t device, cl_ulong* device_timestamp, cl_ulong* host_timestamp) {
    AV_SCOPE("av_clGetDeviceAndHostTimer");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(device);
    b.write_pod<uint8_t>(device_timestamp != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(host_timestamp != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetDeviceAndHostTimer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (device_timestamp != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        *device_timestamp = static_cast<cl_ulong>(wire_value);
    }
    if (host_timestamp != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        *host_timestamp = static_cast<cl_ulong>(wire_value);
    }
    return ret;
}

uint64_t av_clCreateBufferWithProperties(::av::ClientTransport& t, uint64_t context, const uint64_t* properties, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateBufferWithProperties");
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
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateImageWithProperties(::av::ClientTransport& t, uint64_t context, const uint64_t* properties, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateImageWithProperties");
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
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clCreateSamplerWithProperties(::av::ClientTransport& t, uint64_t context, const uint64_t* sampler_properties, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateSamplerWithProperties");
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
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clSetDefaultDeviceCommandQueue(::av::ClientTransport& t, uint64_t context, uint64_t device, uint64_t command_queue) {
    AV_SCOPE("av_clSetDefaultDeviceCommandQueue");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint64_t>(device);
    b.write_pod<uint64_t>(command_queue);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetDefaultDeviceCommandQueue, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

uint64_t av_clCreateSampler(::av::ClientTransport& t, uint64_t context, cl_bool normalized_coords, cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateSampler");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(normalized_coords));
    b.write_varint(static_cast<uint64_t>(addressing_mode));
    b.write_varint(static_cast<uint64_t>(filter_mode));
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clRetainSampler(::av::ClientTransport& t, uint64_t sampler) {
    AV_SCOPE("av_clRetainSampler");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(sampler);
    if (t.send_async(kCmd_clRetainSampler, b) != ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

cl_int av_clReleaseSampler(::av::ClientTransport& t, uint64_t sampler) {
    AV_SCOPE("av_clReleaseSampler");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(sampler);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clReleaseSampler, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clGetSamplerInfo(::av::ClientTransport& t, uint64_t sampler, cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetSamplerInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(sampler);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetSamplerInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t av_clCreatePipe(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_uint pipe_packet_size, cl_uint pipe_max_packets, const uint64_t* properties, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreatePipe");
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
    const ::av::SyncCallScope av_call{t};
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

cl_int av_clGetPipeInfo(::av::ClientTransport& t, uint64_t pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    AV_SCOPE("av_clGetPipeInfo");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(pipe);
    b.write_varint(static_cast<uint64_t>(param_name));
    b.write_varint(static_cast<uint64_t>(param_value_size));
    b.write_pod<uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clGetPipeInfo, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return CL_OUT_OF_RESOURCES;
        if (wire_count > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        if (wire_count != 0 && !r.read(param_value, static_cast<size_t>(wire_count))) return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return CL_OUT_OF_RESOURCES;
        if (ret == CL_SUCCESS && param_value != nullptr &&
            wire_value > static_cast<uint64_t>(param_value_size)) return CL_OUT_OF_RESOURCES;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

uint64_t av_clCreateProgramWithBuiltInKernels(::av::ClientTransport& t, uint64_t context, cl_uint num_devices, const uint64_t* device_list, const char* kernel_names, cl_int* errcode_ret) {
    AV_SCOPE("av_clCreateProgramWithBuiltInKernels");
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
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_clEnqueueMapBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_map, cl_map_flags map_flags, size_t offset, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event, cl_int* errcode_ret) {
    AV_SCOPE("av_clEnqueueMapBuffer");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(buffer);
    b.write_varint(static_cast<uint64_t>(blocking_map));
    b.write_varint(static_cast<uint64_t>(map_flags));
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
    b.write_pod<uint8_t>(errcode_ret != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueMapBuffer, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (event != nullptr) {
        if (!r.read_pod(*event)) return 0;
    }
    if (errcode_ret != nullptr) {
        uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int av_clEnqueueUnmapMemObject(::av::ClientTransport& t, uint64_t command_queue, uint64_t memobj, uint64_t mapped_ptr, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueUnmapMemObject");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(memobj);
    b.write_pod<uint64_t>(mapped_ptr);
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueUnmapMemObject, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

uint64_t av_clSVMAlloc(::av::ClientTransport& t, uint64_t context, cl_svm_mem_flags flags, size_t size, cl_uint alignment) {
    AV_SCOPE("av_clSVMAlloc");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_varint(static_cast<uint64_t>(size));
    b.write_varint(static_cast<uint64_t>(alignment));
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSVMAlloc, b, r) !=
        ::av::TransportResult::ok) {
        return 0;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    return ret;
}

void av_clSVMFree(::av::ClientTransport& t, uint64_t context, uint64_t svm_pointer) {
    AV_SCOPE("av_clSVMFree");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(context);
    b.write_pod<uint64_t>(svm_pointer);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSVMFree, b, r) !=
        ::av::TransportResult::ok) {
        return;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
}

cl_int av_clEnqueueSVMMap(::av::ClientTransport& t, uint64_t command_queue, cl_bool blocking_map, cl_map_flags flags, uint64_t svm_ptr, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueSVMMap");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(blocking_map));
    b.write_varint(static_cast<uint64_t>(flags));
    b.write_pod<uint64_t>(svm_ptr);
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueSVMMap, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueSVMUnmap(::av::ClientTransport& t, uint64_t command_queue, uint64_t svm_ptr, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueSVMUnmap");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(svm_ptr);
    b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
    b.write_pod<uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<uint64_t>(num_events_in_wait_list));
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<uint8_t>(event != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueSVMUnmap, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueSVMMemcpy(::av::ClientTransport& t, uint64_t command_queue, cl_bool blocking_copy, uint64_t dst_ptr, uint64_t src_ptr, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueSVMMemcpy");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_varint(static_cast<uint64_t>(blocking_copy));
    b.write_pod<uint64_t>(dst_ptr);
    b.write_pod<uint64_t>(src_ptr);
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueSVMMemcpy, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clEnqueueSVMMemFill(::av::ClientTransport& t, uint64_t command_queue, uint64_t svm_ptr, const void* pattern, size_t pattern_size, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event) {
    AV_SCOPE("av_clEnqueueSVMMemFill");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(command_queue);
    b.write_pod<uint64_t>(svm_ptr);
    b.write_pod<uint8_t>(pattern != nullptr ? 1 : 0);
    if (pattern != nullptr) {
        b.write_varint(static_cast<uint64_t>(pattern_size));
        b.write(pattern, static_cast<size_t>(pattern_size));
    }
    b.write_varint(static_cast<uint64_t>(pattern_size));
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
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clEnqueueSVMMemFill, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return CL_OUT_OF_RESOURCES;
    }
    return ret;
}

cl_int av_clSetKernelArgSVMPointer(::av::ClientTransport& t, uint64_t kernel, cl_uint arg_index, uint64_t arg_value) {
    AV_SCOPE("av_clSetKernelArgSVMPointer");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(kernel);
    b.write_varint(static_cast<uint64_t>(arg_index));
    b.write_pod<uint64_t>(arg_value);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetKernelArgSVMPointer, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int av_clSetEventCallback(::av::ClientTransport& t, uint64_t event, cl_int command_exec_callback_type) {
    AV_SCOPE("av_clSetEventCallback");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(event);
    b.write_varint(static_cast<uint64_t>(command_exec_callback_type));
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_clSetEventCallback, b, r) !=
        ::av::TransportResult::ok) {
        return CL_OUT_OF_RESOURCES;
    }
    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();
    cl_int ret{};
    int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return CL_OUT_OF_RESOURCES;
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

}  // namespace av::clproto
