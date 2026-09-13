// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opencl --xml cl.xml --out ...

#include "cl_dispatch_client.h"

#include <cstring>

#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"
#include "cl_protocol_ids.h"

namespace av::clproto {

cl_int rvk_clGetPlatformIDs(::rvk::protocol::Transport& t, cl_uint num_entries, std::uint64_t* platforms, cl_uint* num_platforms) {
    ::av::WireWriter b;
    b.write_varint(static_cast<std::uint64_t>(num_entries));
    b.write_pod<std::uint8_t>(platforms != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(num_platforms != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetPlatformIDs, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (platforms != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(platforms[i])) return ret;
        }
    }
    if (num_platforms != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *num_platforms = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetPlatformInfo(::rvk::protocol::Transport& t, std::uint64_t platform, cl_platform_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(platform);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetPlatformInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetDeviceIDs(::rvk::protocol::Transport& t, std::uint64_t platform, cl_device_type device_type, cl_uint num_entries, std::uint64_t* devices, cl_uint* num_devices) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(platform);
    b.write_varint(static_cast<std::uint64_t>(device_type));
    b.write_varint(static_cast<std::uint64_t>(num_entries));
    b.write_pod<std::uint8_t>(devices != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(num_devices != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetDeviceIDs, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (devices != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(devices[i])) return ret;
        }
    }
    if (num_devices != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *num_devices = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetDeviceInfo(::rvk::protocol::Transport& t, std::uint64_t device, cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(device);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetDeviceInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateContext(::rvk::protocol::Transport& t, const std::uint64_t* properties, cl_uint num_devices, const std::uint64_t* devices, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        std::uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<std::uint64_t>(properties[i]);
        }
    }
    b.write_varint(static_cast<std::uint64_t>(num_devices));
    b.write_pod<std::uint8_t>(devices != nullptr ? 1 : 0);
    if (devices != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_devices));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_devices); ++i) {
            b.write_pod<std::uint64_t>(devices[i]);
        }
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateContext, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateContextFromType(::rvk::protocol::Transport& t, const std::uint64_t* properties, cl_device_type device_type, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        std::uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<std::uint64_t>(properties[i]);
        }
    }
    b.write_varint(static_cast<std::uint64_t>(device_type));
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateContextFromType, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainContext(::rvk::protocol::Transport& t, std::uint64_t context) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    (void)t.send_async(kCmd_clRetainContext, b.bytes());
    return CL_SUCCESS;
}

cl_int rvk_clReleaseContext(::rvk::protocol::Transport& t, std::uint64_t context) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clReleaseContext, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetContextInfo(::rvk::protocol::Transport& t, std::uint64_t context, cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetContextInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateCommandQueue(::rvk::protocol::Transport& t, std::uint64_t context, std::uint64_t device, cl_command_queue_properties properties, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_pod<std::uint64_t>(device);
    b.write_varint(static_cast<std::uint64_t>(properties));
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateCommandQueue, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateCommandQueueWithProperties(::rvk::protocol::Transport& t, std::uint64_t context, std::uint64_t device, const std::uint64_t* properties, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_pod<std::uint64_t>(device);
    b.write_pod<std::uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        std::uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<std::uint64_t>(properties[i]);
        }
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateCommandQueueWithProperties, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainCommandQueue(::rvk::protocol::Transport& t, std::uint64_t command_queue) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    (void)t.send_async(kCmd_clRetainCommandQueue, b.bytes());
    return CL_SUCCESS;
}

cl_int rvk_clReleaseCommandQueue(::rvk::protocol::Transport& t, std::uint64_t command_queue) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clReleaseCommandQueue, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetCommandQueueInfo(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_command_queue_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetCommandQueueInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clFlush(::rvk::protocol::Transport& t, std::uint64_t command_queue) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clFlush, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clFinish(::rvk::protocol::Transport& t, std::uint64_t command_queue) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clFinish, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

std::uint64_t rvk_clCreateBuffer(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_varint(static_cast<std::uint64_t>(size));
    b.write_pod<std::uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(size));
        b.write(host_ptr, static_cast<std::size_t>(size));
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateBuffer, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateSubBuffer(::rvk::protocol::Transport& t, std::uint64_t buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const cl_buffer_region* buffer_create_info, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(buffer);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_varint(static_cast<std::uint64_t>(buffer_create_type));
    b.write_pod<std::uint8_t>(buffer_create_info != nullptr ? 1 : 0);
    if (buffer_create_info != nullptr) b.write(buffer_create_info, sizeof(*buffer_create_info));
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateSubBuffer, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateImage(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, std::size_t host_ptr_bytes, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_pod<std::uint8_t>(image_format != nullptr ? 1 : 0);
    if (image_format != nullptr) b.write(image_format, sizeof(*image_format));
    b.write_pod<std::uint8_t>(image_desc != nullptr ? 1 : 0);
    if (image_desc != nullptr) b.write(image_desc, sizeof(*image_desc));
    b.write_pod<std::uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(host_ptr_bytes));
        b.write(host_ptr, static_cast<std::size_t>(host_ptr_bytes));
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateImage, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainMemObject(::rvk::protocol::Transport& t, std::uint64_t memobj) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(memobj);
    (void)t.send_async(kCmd_clRetainMemObject, b.bytes());
    return CL_SUCCESS;
}

cl_int rvk_clReleaseMemObject(::rvk::protocol::Transport& t, std::uint64_t memobj) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(memobj);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clReleaseMemObject, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetMemObjectInfo(::rvk::protocol::Transport& t, std::uint64_t memobj, cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(memobj);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetMemObjectInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetImageInfo(::rvk::protocol::Transport& t, std::uint64_t image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(image);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetImageInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetSupportedImageFormats(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries, cl_image_format* image_formats, std::size_t image_formats_bytes, cl_uint* num_image_formats) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_varint(static_cast<std::uint64_t>(image_type));
    b.write_varint(static_cast<std::uint64_t>(num_entries));
    b.write_pod<std::uint8_t>(image_formats != nullptr ? 1 : 0);
    if (image_formats != nullptr) b.write_varint(static_cast<std::uint64_t>(image_formats_bytes));
    b.write_pod<std::uint8_t>(num_image_formats != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetSupportedImageFormats, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (image_formats != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(image_formats, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (num_image_formats != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *num_image_formats = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int rvk_clEnqueueReadBuffer(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t buffer, cl_bool blocking_read, size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(buffer);
    b.write_varint(static_cast<std::uint64_t>(blocking_read));
    b.write_varint(static_cast<std::uint64_t>(offset));
    b.write_varint(static_cast<std::uint64_t>(size));
    b.write_pod<std::uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) b.write_varint(static_cast<std::uint64_t>(size));
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueReadBuffer, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (ptr != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(ptr, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueWriteBuffer(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t buffer, cl_bool blocking_write, size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(buffer);
    b.write_varint(static_cast<std::uint64_t>(blocking_write));
    b.write_varint(static_cast<std::uint64_t>(offset));
    b.write_varint(static_cast<std::uint64_t>(size));
    b.write_pod<std::uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(size));
        b.write(ptr, static_cast<std::size_t>(size));
    }
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueWriteBuffer, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueCopyBuffer(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t src_buffer, std::uint64_t dst_buffer, size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(src_buffer);
    b.write_pod<std::uint64_t>(dst_buffer);
    b.write_varint(static_cast<std::uint64_t>(src_offset));
    b.write_varint(static_cast<std::uint64_t>(dst_offset));
    b.write_varint(static_cast<std::uint64_t>(size));
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueCopyBuffer, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueFillBuffer(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(buffer);
    b.write_pod<std::uint8_t>(pattern != nullptr ? 1 : 0);
    if (pattern != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(pattern_size));
        b.write(pattern, static_cast<std::size_t>(pattern_size));
    }
    b.write_varint(static_cast<std::uint64_t>(pattern_size));
    b.write_varint(static_cast<std::uint64_t>(offset));
    b.write_varint(static_cast<std::uint64_t>(size));
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueFillBuffer, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueReadImage(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t image, cl_bool blocking_read, const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, std::size_t ptr_bytes, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(image);
    b.write_varint(static_cast<std::uint64_t>(blocking_read));
    b.write_pod<std::uint8_t>(origin != nullptr ? 1 : 0);
    if (origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<std::uint64_t>(row_pitch));
    b.write_varint(static_cast<std::uint64_t>(slice_pitch));
    b.write_pod<std::uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) b.write_varint(static_cast<std::uint64_t>(ptr_bytes));
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueReadImage, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (ptr != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(ptr, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueWriteImage(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t image, cl_bool blocking_write, const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr, std::size_t ptr_bytes, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(image);
    b.write_varint(static_cast<std::uint64_t>(blocking_write));
    b.write_pod<std::uint8_t>(origin != nullptr ? 1 : 0);
    if (origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<std::uint64_t>(input_row_pitch));
    b.write_varint(static_cast<std::uint64_t>(input_slice_pitch));
    b.write_pod<std::uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(ptr_bytes));
        b.write(ptr, static_cast<std::size_t>(ptr_bytes));
    }
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueWriteImage, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueCopyImage(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t src_image, std::uint64_t dst_image, const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(src_image);
    b.write_pod<std::uint64_t>(dst_image);
    b.write_pod<std::uint8_t>(src_origin != nullptr ? 1 : 0);
    if (src_origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(src_origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(dst_origin != nullptr ? 1 : 0);
    if (dst_origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(dst_origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueCopyImage, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueCopyImageToBuffer(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t src_image, std::uint64_t dst_buffer, const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(src_image);
    b.write_pod<std::uint64_t>(dst_buffer);
    b.write_pod<std::uint8_t>(src_origin != nullptr ? 1 : 0);
    if (src_origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(src_origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<std::uint64_t>(dst_offset));
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueCopyImageToBuffer, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueCopyBufferToImage(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t src_buffer, std::uint64_t dst_image, size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(src_buffer);
    b.write_pod<std::uint64_t>(dst_image);
    b.write_varint(static_cast<std::uint64_t>(src_offset));
    b.write_pod<std::uint8_t>(dst_origin != nullptr ? 1 : 0);
    if (dst_origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(dst_origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueCopyBufferToImage, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

std::uint64_t rvk_clCreateProgramWithSource(::rvk::protocol::Transport& t, std::uint64_t context, cl_uint count, const char** strings, const std::size_t* lengths, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(count));
    b.write_pod<std::uint8_t>(strings != nullptr ? 1 : 0);
    if (strings != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(count));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(count); ++i) {
            const std::size_t wire_len = (lengths != nullptr && lengths[i] != 0)
                ? lengths[i]
                : (strings[i] != nullptr ? std::strlen(reinterpret_cast<const char*>(strings[i])) : 0);
            b.write_varint(wire_len);
            if (wire_len != 0) b.write(strings[i], wire_len);
        }
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateProgramWithSource, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateProgramWithBinary(::rvk::protocol::Transport& t, std::uint64_t context, cl_uint num_devices, const std::uint64_t* device_list, const std::size_t* lengths, const unsigned char** binaries, cl_int* binary_status, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(num_devices));
    b.write_pod<std::uint8_t>(device_list != nullptr ? 1 : 0);
    if (device_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_devices));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_devices); ++i) {
            b.write_pod<std::uint64_t>(device_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(binaries != nullptr ? 1 : 0);
    if (binaries != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_devices));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_devices); ++i) {
            const std::size_t wire_len = (lengths != nullptr && lengths[i] != 0)
                ? lengths[i]
                : (binaries[i] != nullptr ? std::strlen(reinterpret_cast<const char*>(binaries[i])) : 0);
            b.write_varint(wire_len);
            if (wire_len != 0) b.write(binaries[i], wire_len);
        }
    }
    b.write_pod<std::uint8_t>(binary_status != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateProgramWithBinary, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (binary_status != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *binary_status = static_cast<cl_int>(wire_value);
    }
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainProgram(::rvk::protocol::Transport& t, std::uint64_t program) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(program);
    (void)t.send_async(kCmd_clRetainProgram, b.bytes());
    return CL_SUCCESS;
}

cl_int rvk_clReleaseProgram(::rvk::protocol::Transport& t, std::uint64_t program) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(program);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clReleaseProgram, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clBuildProgram(::rvk::protocol::Transport& t, std::uint64_t program, cl_uint num_devices, const std::uint64_t* device_list, const char* options) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(program);
    b.write_varint(static_cast<std::uint64_t>(num_devices));
    b.write_pod<std::uint8_t>(device_list != nullptr ? 1 : 0);
    if (device_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_devices));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_devices); ++i) {
            b.write_pod<std::uint64_t>(device_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(options != nullptr ? 1 : 0);
    if (options != nullptr) {
        const std::size_t wire_len = std::strlen(options);
        b.write_varint(wire_len);
        b.write(options, wire_len);
    }
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clBuildProgram, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetProgramInfo(::rvk::protocol::Transport& t, std::uint64_t program, cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(program);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetProgramInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetProgramBuildInfo(::rvk::protocol::Transport& t, std::uint64_t program, std::uint64_t device, cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(program);
    b.write_pod<std::uint64_t>(device);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetProgramBuildInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clUnloadPlatformCompiler(::rvk::protocol::Transport& t, std::uint64_t platform) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(platform);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clUnloadPlatformCompiler, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

std::uint64_t rvk_clCreateKernel(::rvk::protocol::Transport& t, std::uint64_t program, const char* kernel_name, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(program);
    b.write_pod<std::uint8_t>(kernel_name != nullptr ? 1 : 0);
    if (kernel_name != nullptr) {
        const std::size_t wire_len = std::strlen(kernel_name);
        b.write_varint(wire_len);
        b.write(kernel_name, wire_len);
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateKernel, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clCreateKernelsInProgram(::rvk::protocol::Transport& t, std::uint64_t program, cl_uint num_kernels, std::uint64_t* kernels, cl_uint* num_kernels_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(program);
    b.write_varint(static_cast<std::uint64_t>(num_kernels));
    b.write_pod<std::uint8_t>(kernels != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(num_kernels_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateKernelsInProgram, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (kernels != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(kernels[i])) return ret;
        }
    }
    if (num_kernels_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *num_kernels_ret = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainKernel(::rvk::protocol::Transport& t, std::uint64_t kernel) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(kernel);
    (void)t.send_async(kCmd_clRetainKernel, b.bytes());
    return CL_SUCCESS;
}

cl_int rvk_clReleaseKernel(::rvk::protocol::Transport& t, std::uint64_t kernel) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(kernel);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clReleaseKernel, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clSetKernelArg(::rvk::protocol::Transport& t, std::uint64_t kernel, cl_uint arg_index, size_t arg_size, const void* arg_value) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(kernel);
    b.write_varint(static_cast<std::uint64_t>(arg_index));
    b.write_varint(static_cast<std::uint64_t>(arg_size));
    b.write_pod<std::uint8_t>(arg_value != nullptr ? 1 : 0);
    if (arg_value != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(arg_size));
        b.write(arg_value, static_cast<std::size_t>(arg_size));
    }
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clSetKernelArg, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetKernelInfo(::rvk::protocol::Transport& t, std::uint64_t kernel, cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(kernel);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetKernelInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetKernelArgInfo(::rvk::protocol::Transport& t, std::uint64_t kernel, cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(kernel);
    b.write_varint(static_cast<std::uint64_t>(arg_index));
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetKernelArgInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetKernelWorkGroupInfo(::rvk::protocol::Transport& t, std::uint64_t kernel, std::uint64_t device, cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(kernel);
    b.write_pod<std::uint64_t>(device);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetKernelWorkGroupInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clEnqueueNDRangeKernel(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t kernel, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(kernel);
    b.write_varint(static_cast<std::uint64_t>(work_dim));
    b.write_pod<std::uint8_t>(global_work_offset != nullptr ? 1 : 0);
    if (global_work_offset != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(work_dim));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(work_dim); ++i) {
            b.write_varint(static_cast<std::uint64_t>(global_work_offset[i]));
        }
    }
    b.write_pod<std::uint8_t>(global_work_size != nullptr ? 1 : 0);
    if (global_work_size != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(work_dim));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(work_dim); ++i) {
            b.write_varint(static_cast<std::uint64_t>(global_work_size[i]));
        }
    }
    b.write_pod<std::uint8_t>(local_work_size != nullptr ? 1 : 0);
    if (local_work_size != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(work_dim));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(work_dim); ++i) {
            b.write_varint(static_cast<std::uint64_t>(local_work_size[i]));
        }
    }
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueNDRangeKernel, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueMarkerWithWaitList(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueMarkerWithWaitList, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueBarrierWithWaitList(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueBarrierWithWaitList, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clWaitForEvents(::rvk::protocol::Transport& t, cl_uint num_events, const std::uint64_t* event_list) {
    ::av::WireWriter b;
    b.write_varint(static_cast<std::uint64_t>(num_events));
    b.write_pod<std::uint8_t>(event_list != nullptr ? 1 : 0);
    if (event_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events); ++i) {
            b.write_pod<std::uint64_t>(event_list[i]);
        }
    }
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clWaitForEvents, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetEventInfo(::rvk::protocol::Transport& t, std::uint64_t event, cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(event);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetEventInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateUserEvent(::rvk::protocol::Transport& t, std::uint64_t context, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateUserEvent, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clSetUserEventStatus(::rvk::protocol::Transport& t, std::uint64_t event, cl_int execution_status) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(event);
    b.write_varint(static_cast<std::uint64_t>(execution_status));
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clSetUserEventStatus, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clRetainEvent(::rvk::protocol::Transport& t, std::uint64_t event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(event);
    (void)t.send_async(kCmd_clRetainEvent, b.bytes());
    return CL_SUCCESS;
}

cl_int rvk_clReleaseEvent(::rvk::protocol::Transport& t, std::uint64_t event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(event);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clReleaseEvent, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetEventProfilingInfo(::rvk::protocol::Transport& t, std::uint64_t event, cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(event);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetEventProfilingInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateFromGLBuffer(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_GLuint bufobj, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_varint(static_cast<std::uint64_t>(bufobj));
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateFromGLBuffer, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateFromGLTexture(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_varint(static_cast<std::uint64_t>(target));
    b.write_varint(static_cast<std::uint64_t>(miplevel));
    b.write_varint(static_cast<std::uint64_t>(texture));
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateFromGLTexture, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateFromGLTexture2D(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_varint(static_cast<std::uint64_t>(target));
    b.write_varint(static_cast<std::uint64_t>(miplevel));
    b.write_varint(static_cast<std::uint64_t>(texture));
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateFromGLTexture2D, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateFromGLTexture3D(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_varint(static_cast<std::uint64_t>(target));
    b.write_varint(static_cast<std::uint64_t>(miplevel));
    b.write_varint(static_cast<std::uint64_t>(texture));
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateFromGLTexture3D, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateFromGLRenderbuffer(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_varint(static_cast<std::uint64_t>(renderbuffer));
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateFromGLRenderbuffer, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetGLObjectInfo(::rvk::protocol::Transport& t, std::uint64_t memobj, cl_gl_object_type* gl_object_type, cl_GLuint* gl_object_name) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(memobj);
    b.write_pod<std::uint8_t>(gl_object_type != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(gl_object_name != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetGLObjectInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (gl_object_type != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *gl_object_type = static_cast<cl_gl_object_type>(wire_value);
    }
    if (gl_object_name != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *gl_object_name = static_cast<cl_GLuint>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetGLTextureInfo(::rvk::protocol::Transport& t, std::uint64_t memobj, cl_gl_texture_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(memobj);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetGLTextureInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clEnqueueAcquireGLObjects(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_objects, const std::uint64_t* mem_objects, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_varint(static_cast<std::uint64_t>(num_objects));
    b.write_pod<std::uint8_t>(mem_objects != nullptr ? 1 : 0);
    if (mem_objects != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_objects));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_objects); ++i) {
            b.write_pod<std::uint64_t>(mem_objects[i]);
        }
    }
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueAcquireGLObjects, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueReleaseGLObjects(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_objects, const std::uint64_t* mem_objects, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_varint(static_cast<std::uint64_t>(num_objects));
    b.write_pod<std::uint8_t>(mem_objects != nullptr ? 1 : 0);
    if (mem_objects != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_objects));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_objects); ++i) {
            b.write_pod<std::uint64_t>(mem_objects[i]);
        }
    }
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueReleaseGLObjects, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clGetGLContextInfoKHR(::rvk::protocol::Transport& t, const std::uint64_t* properties, cl_gl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        std::uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<std::uint64_t>(properties[i]);
        }
    }
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetGLContextInfoKHR, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainDevice(::rvk::protocol::Transport& t, std::uint64_t device) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(device);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clRetainDevice, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clReleaseDevice(::rvk::protocol::Transport& t, std::uint64_t device) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(device);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clReleaseDevice, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clCreateSubDevices(::rvk::protocol::Transport& t, std::uint64_t in_device, const std::uint64_t* properties, cl_uint num_devices, std::uint64_t* out_devices, cl_uint* num_devices_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(in_device);
    b.write_pod<std::uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        std::uint64_t wire_count = 0;
        while (properties[wire_count] != 0) ++wire_count;
        b.write_varint(wire_count);
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<std::uint64_t>(static_cast<std::uint64_t>(properties[i]));
        }
    }
    b.write_varint(static_cast<std::uint64_t>(num_devices));
    b.write_pod<std::uint8_t>(out_devices != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(num_devices_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateSubDevices, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (out_devices != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            if (!r.read_pod(out_devices[i])) return ret;
        }
    }
    if (num_devices_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *num_devices_ret = static_cast<cl_uint>(wire_value);
    }
    return ret;
}

cl_int rvk_clEnqueueTask(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t kernel, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(kernel);
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueTask, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueMarker(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueMarker, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueBarrier(::rvk::protocol::Transport& t, std::uint64_t command_queue) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueBarrier, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clEnqueueWaitForEvents(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_events, const std::uint64_t* event_list) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_varint(static_cast<std::uint64_t>(num_events));
    b.write_pod<std::uint8_t>(event_list != nullptr ? 1 : 0);
    if (event_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events); ++i) {
            b.write_pod<std::uint64_t>(event_list[i]);
        }
    }
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueWaitForEvents, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clEnqueueMigrateMemObjects(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_mem_objects, const std::uint64_t* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_varint(static_cast<std::uint64_t>(num_mem_objects));
    b.write_pod<std::uint8_t>(mem_objects != nullptr ? 1 : 0);
    if (mem_objects != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_mem_objects));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_mem_objects); ++i) {
            b.write_pod<std::uint64_t>(mem_objects[i]);
        }
    }
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueMigrateMemObjects, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueReadBufferRect(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t buffer, cl_bool blocking_read, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr, std::size_t ptr_bytes, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(buffer);
    b.write_varint(static_cast<std::uint64_t>(blocking_read));
    b.write_pod<std::uint8_t>(buffer_origin != nullptr ? 1 : 0);
    if (buffer_origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(buffer_origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(host_origin != nullptr ? 1 : 0);
    if (host_origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(host_origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<std::uint64_t>(buffer_row_pitch));
    b.write_varint(static_cast<std::uint64_t>(buffer_slice_pitch));
    b.write_varint(static_cast<std::uint64_t>(host_row_pitch));
    b.write_varint(static_cast<std::uint64_t>(host_slice_pitch));
    b.write_pod<std::uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) b.write_varint(static_cast<std::uint64_t>(ptr_bytes));
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueReadBufferRect, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (ptr != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(ptr, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueWriteBufferRect(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t buffer, cl_bool blocking_write, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr, std::size_t ptr_bytes, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(buffer);
    b.write_varint(static_cast<std::uint64_t>(blocking_write));
    b.write_pod<std::uint8_t>(buffer_origin != nullptr ? 1 : 0);
    if (buffer_origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(buffer_origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(host_origin != nullptr ? 1 : 0);
    if (host_origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(host_origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<std::uint64_t>(buffer_row_pitch));
    b.write_varint(static_cast<std::uint64_t>(buffer_slice_pitch));
    b.write_varint(static_cast<std::uint64_t>(host_row_pitch));
    b.write_varint(static_cast<std::uint64_t>(host_slice_pitch));
    b.write_pod<std::uint8_t>(ptr != nullptr ? 1 : 0);
    if (ptr != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(ptr_bytes));
        b.write(ptr, static_cast<std::size_t>(ptr_bytes));
    }
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueWriteBufferRect, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueCopyBufferRect(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t src_buffer, std::uint64_t dst_buffer, const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch, size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(src_buffer);
    b.write_pod<std::uint64_t>(dst_buffer);
    b.write_pod<std::uint8_t>(src_origin != nullptr ? 1 : 0);
    if (src_origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(src_origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(dst_origin != nullptr ? 1 : 0);
    if (dst_origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(dst_origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<std::uint64_t>(src_row_pitch));
    b.write_varint(static_cast<std::uint64_t>(src_slice_pitch));
    b.write_varint(static_cast<std::uint64_t>(dst_row_pitch));
    b.write_varint(static_cast<std::uint64_t>(dst_slice_pitch));
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueCopyBufferRect, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

cl_int rvk_clEnqueueFillImage(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t image, const void* fill_color, std::size_t fill_color_bytes, const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_pod<std::uint64_t>(image);
    b.write_pod<std::uint8_t>(fill_color != nullptr ? 1 : 0);
    if (fill_color != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(fill_color_bytes));
        b.write(fill_color, static_cast<std::size_t>(fill_color_bytes));
    }
    b.write_pod<std::uint8_t>(origin != nullptr ? 1 : 0);
    if (origin != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(origin[i]));
        }
    }
    b.write_pod<std::uint8_t>(region != nullptr ? 1 : 0);
    if (region != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(3));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(3); ++i) {
            b.write_varint(static_cast<std::uint64_t>(region[i]));
        }
    }
    b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
    b.write_pod<std::uint8_t>(event_wait_list != nullptr ? 1 : 0);
    if (event_wait_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_events_in_wait_list));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_events_in_wait_list); ++i) {
            b.write_pod<std::uint64_t>(event_wait_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(event != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clEnqueueFillImage, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (event != nullptr) {
        if (!r.read_pod(*event)) return ret;
    }
    return ret;
}

std::uint64_t rvk_clCreateImage2D(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, std::size_t host_ptr_bytes, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_pod<std::uint8_t>(image_format != nullptr ? 1 : 0);
    if (image_format != nullptr) b.write(image_format, sizeof(*image_format));
    b.write_varint(static_cast<std::uint64_t>(image_width));
    b.write_varint(static_cast<std::uint64_t>(image_height));
    b.write_varint(static_cast<std::uint64_t>(image_row_pitch));
    b.write_pod<std::uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(host_ptr_bytes));
        b.write(host_ptr, static_cast<std::size_t>(host_ptr_bytes));
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateImage2D, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateImage3D(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch, void* host_ptr, std::size_t host_ptr_bytes, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_pod<std::uint8_t>(image_format != nullptr ? 1 : 0);
    if (image_format != nullptr) b.write(image_format, sizeof(*image_format));
    b.write_varint(static_cast<std::uint64_t>(image_width));
    b.write_varint(static_cast<std::uint64_t>(image_height));
    b.write_varint(static_cast<std::uint64_t>(image_depth));
    b.write_varint(static_cast<std::uint64_t>(image_row_pitch));
    b.write_varint(static_cast<std::uint64_t>(image_slice_pitch));
    b.write_pod<std::uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(host_ptr_bytes));
        b.write(host_ptr, static_cast<std::size_t>(host_ptr_bytes));
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateImage3D, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clUnloadCompiler(::rvk::protocol::Transport& t) {
    ::av::WireWriter b;
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clUnloadCompiler, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clSetCommandQueueProperty(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_command_queue_properties properties, cl_bool enable, cl_command_queue_properties* old_properties) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(command_queue);
    b.write_varint(static_cast<std::uint64_t>(properties));
    b.write_varint(static_cast<std::uint64_t>(enable));
    b.write_pod<std::uint8_t>(old_properties != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clSetCommandQueueProperty, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (old_properties != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *old_properties = static_cast<cl_command_queue_properties>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateProgramWithIL(::rvk::protocol::Transport& t, std::uint64_t context, const void* il, size_t length, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_pod<std::uint8_t>(il != nullptr ? 1 : 0);
    if (il != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(length));
        b.write(il, static_cast<std::size_t>(length));
    }
    b.write_varint(static_cast<std::uint64_t>(length));
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateProgramWithIL, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clCompileProgram(::rvk::protocol::Transport& t, std::uint64_t program, cl_uint num_devices, const std::uint64_t* device_list, const char* options, cl_uint num_input_headers, const std::uint64_t* input_headers, const char** header_include_names) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(program);
    b.write_varint(static_cast<std::uint64_t>(num_devices));
    b.write_pod<std::uint8_t>(device_list != nullptr ? 1 : 0);
    if (device_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_devices));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_devices); ++i) {
            b.write_pod<std::uint64_t>(device_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(options != nullptr ? 1 : 0);
    if (options != nullptr) {
        const std::size_t wire_len = std::strlen(options);
        b.write_varint(wire_len);
        b.write(options, wire_len);
    }
    b.write_varint(static_cast<std::uint64_t>(num_input_headers));
    b.write_pod<std::uint8_t>(input_headers != nullptr ? 1 : 0);
    if (input_headers != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_input_headers));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_input_headers); ++i) {
            b.write_pod<std::uint64_t>(input_headers[i]);
        }
    }
    b.write_pod<std::uint8_t>(header_include_names != nullptr ? 1 : 0);
    if (header_include_names != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_input_headers));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_input_headers); ++i) {
            const std::size_t wire_len = header_include_names[i] != nullptr ? std::strlen(header_include_names[i]) : 0;
            b.write_varint(wire_len);
            if (wire_len != 0) b.write(header_include_names[i], wire_len);
        }
    }
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCompileProgram, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

std::uint64_t rvk_clLinkProgram(::rvk::protocol::Transport& t, std::uint64_t context, cl_uint num_devices, const std::uint64_t* device_list, const char* options, cl_uint num_input_programs, const std::uint64_t* input_programs, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(num_devices));
    b.write_pod<std::uint8_t>(device_list != nullptr ? 1 : 0);
    if (device_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_devices));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_devices); ++i) {
            b.write_pod<std::uint64_t>(device_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(options != nullptr ? 1 : 0);
    if (options != nullptr) {
        const std::size_t wire_len = std::strlen(options);
        b.write_varint(wire_len);
        b.write(options, wire_len);
    }
    b.write_varint(static_cast<std::uint64_t>(num_input_programs));
    b.write_pod<std::uint8_t>(input_programs != nullptr ? 1 : 0);
    if (input_programs != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_input_programs));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_input_programs); ++i) {
            b.write_pod<std::uint64_t>(input_programs[i]);
        }
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clLinkProgram, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clSetProgramSpecializationConstant(::rvk::protocol::Transport& t, std::uint64_t program, cl_uint spec_id, size_t spec_size, const void* spec_value) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(program);
    b.write_varint(static_cast<std::uint64_t>(spec_id));
    b.write_varint(static_cast<std::uint64_t>(spec_size));
    b.write_pod<std::uint8_t>(spec_value != nullptr ? 1 : 0);
    if (spec_value != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(spec_size));
        b.write(spec_value, static_cast<std::size_t>(spec_size));
    }
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clSetProgramSpecializationConstant, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

std::uint64_t rvk_clCloneKernel(::rvk::protocol::Transport& t, std::uint64_t source_kernel, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(source_kernel);
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCloneKernel, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clSetKernelExecInfo(::rvk::protocol::Transport& t, std::uint64_t kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(kernel);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    if (param_value != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(param_value_size));
        b.write(param_value, static_cast<std::size_t>(param_value_size));
    }
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clSetKernelExecInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetKernelSubGroupInfo(::rvk::protocol::Transport& t, std::uint64_t kernel, std::uint64_t device, cl_kernel_sub_group_info param_name, size_t input_value_size, const void* input_value, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(kernel);
    b.write_pod<std::uint64_t>(device);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(input_value_size));
    b.write_pod<std::uint8_t>(input_value != nullptr ? 1 : 0);
    if (input_value != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(input_value_size));
        b.write(input_value, static_cast<std::size_t>(input_value_size));
    }
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetKernelSubGroupInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetHostTimer(::rvk::protocol::Transport& t, std::uint64_t device, cl_ulong* host_timestamp) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(device);
    b.write_pod<std::uint8_t>(host_timestamp != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetHostTimer, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (host_timestamp != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *host_timestamp = static_cast<cl_ulong>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetDeviceAndHostTimer(::rvk::protocol::Transport& t, std::uint64_t device, cl_ulong* device_timestamp, cl_ulong* host_timestamp) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(device);
    b.write_pod<std::uint8_t>(device_timestamp != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(host_timestamp != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetDeviceAndHostTimer, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (device_timestamp != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *device_timestamp = static_cast<cl_ulong>(wire_value);
    }
    if (host_timestamp != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *host_timestamp = static_cast<cl_ulong>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateBufferWithProperties(::rvk::protocol::Transport& t, std::uint64_t context, const std::uint64_t* properties, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_pod<std::uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        std::uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<std::uint64_t>(properties[i]);
        }
    }
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_varint(static_cast<std::uint64_t>(size));
    b.write_pod<std::uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(size));
        b.write(host_ptr, static_cast<std::size_t>(size));
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateBufferWithProperties, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateImageWithProperties(::rvk::protocol::Transport& t, std::uint64_t context, const std::uint64_t* properties, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, std::size_t host_ptr_bytes, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_pod<std::uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        std::uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<std::uint64_t>(properties[i]);
        }
    }
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_pod<std::uint8_t>(image_format != nullptr ? 1 : 0);
    if (image_format != nullptr) b.write(image_format, sizeof(*image_format));
    b.write_pod<std::uint8_t>(image_desc != nullptr ? 1 : 0);
    if (image_desc != nullptr) b.write(image_desc, sizeof(*image_desc));
    b.write_pod<std::uint8_t>(host_ptr != nullptr ? 1 : 0);
    if (host_ptr != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(host_ptr_bytes));
        b.write(host_ptr, static_cast<std::size_t>(host_ptr_bytes));
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateImageWithProperties, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateSamplerWithProperties(::rvk::protocol::Transport& t, std::uint64_t context, const std::uint64_t* sampler_properties, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_pod<std::uint8_t>(sampler_properties != nullptr ? 1 : 0);
    if (sampler_properties != nullptr) {
        std::uint64_t wire_count = 0;
        while (sampler_properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<std::uint64_t>(sampler_properties[i]);
        }
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateSamplerWithProperties, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clSetDefaultDeviceCommandQueue(::rvk::protocol::Transport& t, std::uint64_t context, std::uint64_t device, std::uint64_t command_queue) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_pod<std::uint64_t>(device);
    b.write_pod<std::uint64_t>(command_queue);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clSetDefaultDeviceCommandQueue, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

std::uint64_t rvk_clCreateSampler(::rvk::protocol::Transport& t, std::uint64_t context, cl_bool normalized_coords, cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(normalized_coords));
    b.write_varint(static_cast<std::uint64_t>(addressing_mode));
    b.write_varint(static_cast<std::uint64_t>(filter_mode));
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateSampler, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clRetainSampler(::rvk::protocol::Transport& t, std::uint64_t sampler) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(sampler);
    (void)t.send_async(kCmd_clRetainSampler, b.bytes());
    return CL_SUCCESS;
}

cl_int rvk_clReleaseSampler(::rvk::protocol::Transport& t, std::uint64_t sampler) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(sampler);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clReleaseSampler, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    return ret;
}

cl_int rvk_clGetSamplerInfo(::rvk::protocol::Transport& t, std::uint64_t sampler, cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(sampler);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetSamplerInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreatePipe(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_uint pipe_packet_size, cl_uint pipe_max_packets, const std::uint64_t* properties, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(flags));
    b.write_varint(static_cast<std::uint64_t>(pipe_packet_size));
    b.write_varint(static_cast<std::uint64_t>(pipe_max_packets));
    b.write_pod<std::uint8_t>(properties != nullptr ? 1 : 0);
    if (properties != nullptr) {
        std::uint64_t wire_count = 0;
        while (properties[wire_count] != 0) wire_count += 2;
        b.write_varint(wire_count);
        for (std::uint64_t i = 0; i < wire_count; ++i) {
            b.write_pod<std::uint64_t>(properties[i]);
        }
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreatePipe, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

cl_int rvk_clGetPipeInfo(::rvk::protocol::Transport& t, std::uint64_t pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(pipe);
    b.write_varint(static_cast<std::uint64_t>(param_name));
    b.write_varint(static_cast<std::uint64_t>(param_value_size));
    b.write_pod<std::uint8_t>(param_value != nullptr ? 1 : 0);
    b.write_pod<std::uint8_t>(param_value_size_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clGetPipeInfo, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return CL_OUT_OF_RESOURCES;
    }
    ::av::WireReader r{reply.bytes()};
    cl_int ret{};
    std::int64_t ret_wire = 0;
    if (!r.read_varint(ret_wire)) return static_cast<cl_int>(-1);
    ret = static_cast<cl_int>(ret_wire);
    if (param_value != nullptr) {
        std::uint64_t wire_count = 0;
        if (!r.read_varint(wire_count)) return ret;
        if (wire_count != 0 && !r.read(param_value, static_cast<std::size_t>(wire_count))) return ret;
    }
    if (param_value_size_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return ret;
        *param_value_size_ret = static_cast<size_t>(wire_value);
    }
    return ret;
}

std::uint64_t rvk_clCreateProgramWithBuiltInKernels(::rvk::protocol::Transport& t, std::uint64_t context, cl_uint num_devices, const std::uint64_t* device_list, const char* kernel_names, cl_int* errcode_ret) {
    ::av::WireWriter b;
    b.write_pod<std::uint64_t>(context);
    b.write_varint(static_cast<std::uint64_t>(num_devices));
    b.write_pod<std::uint8_t>(device_list != nullptr ? 1 : 0);
    if (device_list != nullptr) {
        b.write_varint(static_cast<std::uint64_t>(num_devices));
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(num_devices); ++i) {
            b.write_pod<std::uint64_t>(device_list[i]);
        }
    }
    b.write_pod<std::uint8_t>(kernel_names != nullptr ? 1 : 0);
    if (kernel_names != nullptr) {
        const std::size_t wire_len = std::strlen(kernel_names);
        b.write_varint(wire_len);
        b.write(kernel_names, wire_len);
    }
    b.write_pod<std::uint8_t>(errcode_ret != nullptr ? 1 : 0);
    ::av::WireWriter reply;
    if (t.send_sync(kCmd_clCreateProgramWithBuiltInKernels, b.bytes(), &reply) !=
        ::rvk::protocol::TransportResult::Ok) {
        return 0;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t ret = 0;
    if (!r.read_pod(ret)) return 0;
    if (errcode_ret != nullptr) {
        std::uint64_t wire_value = 0;
        if (!r.read_varint(wire_value)) return 0;
        *errcode_ret = static_cast<cl_int>(wire_value);
    }
    return ret;
}

}  // namespace av::clproto
