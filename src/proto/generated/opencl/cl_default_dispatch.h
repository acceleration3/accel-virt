// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.opencl --xml cl.xml --out ...

#pragma once

#include <stddef.h>

#ifndef CL_TARGET_OPENCL_VERSION
#define CL_TARGET_OPENCL_VERSION 300
#endif
#define CL_USE_DEPRECATED_OPENCL_1_0_APIS
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#define CL_USE_DEPRECATED_OPENCL_2_1_APIS
#define CL_USE_DEPRECATED_OPENCL_2_2_APIS

#include <CL/cl.h>
#include <CL/cl_gl.h>

#include "cl_dispatch_server.h"
#include "cl_protocol_ids.h"

namespace av::clproto {

// Every command answered with no OpenCL behind it, for judging the
// decoder rather than a driver. Each call says which it is to
// `called`, hands every object it was given to `handed` and every
// span it would read or write to `touched` - that is where a fake
// holds the decoder to what it resolved and bounded - then
// succeeds, answering whatever objects it makes with `produce`.
class DefaultDispatch : public ServerDispatch {
public:
    using ServerDispatch::ServerDispatch;

    cl_int on_clGetPlatformIDs(cl_uint num_entries, cl_platform_id* platforms, cl_uint* num_platforms) override {
        (void)num_entries;
        (void)platforms;
        (void)num_platforms;
        called(kCmd_clGetPlatformIDs);
        for (size_t i = 0; platforms != nullptr && i < static_cast<size_t>(num_entries) && i < 2u; ++i) {
            platforms[i] = static_cast<cl_platform_id>(produce(HandleKind::platform));
        }
        if (num_platforms != nullptr) *num_platforms = 2u;
        return CL_SUCCESS;
    }
    cl_int on_clGetPlatformInfo(cl_platform_id platform, cl_platform_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)platform;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetPlatformInfo);
        handed(HandleKind::platform, platform);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries, cl_device_id* devices, cl_uint* num_devices) override {
        (void)platform;
        (void)device_type;
        (void)num_entries;
        (void)devices;
        (void)num_devices;
        called(kCmd_clGetDeviceIDs);
        handed(HandleKind::platform, platform);
        for (size_t i = 0; devices != nullptr && i < static_cast<size_t>(num_entries) && i < 2u; ++i) {
            devices[i] = static_cast<cl_device_id>(produce(HandleKind::device));
        }
        if (num_devices != nullptr) *num_devices = 2u;
        return CL_SUCCESS;
    }
    cl_int on_clGetDeviceInfo(cl_device_id device, cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)device;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetDeviceInfo);
        handed(HandleKind::device, device);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_context on_clCreateContext(const cl_context_properties* properties, cl_uint num_devices, const cl_device_id* devices, cl_int* errcode_ret) override {
        (void)properties;
        (void)num_devices;
        (void)devices;
        (void)errcode_ret;
        called(kCmd_clCreateContext);
        for (size_t i = 0; properties != nullptr && properties[i] != 0; i += 2) {
            if (properties[i] == CL_CONTEXT_PLATFORM) handed(HandleKind::platform, reinterpret_cast<void*>(properties[i + 1]));
        }
        for (size_t i = 0; devices != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_devices)); ++i) handed(HandleKind::device, devices[i]);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_context>(produce(HandleKind::context));
    }
    cl_context on_clCreateContextFromType(const cl_context_properties* properties, cl_device_type device_type, cl_int* errcode_ret) override {
        (void)properties;
        (void)device_type;
        (void)errcode_ret;
        called(kCmd_clCreateContextFromType);
        for (size_t i = 0; properties != nullptr && properties[i] != 0; i += 2) {
            if (properties[i] == CL_CONTEXT_PLATFORM) handed(HandleKind::platform, reinterpret_cast<void*>(properties[i + 1]));
        }
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_context>(produce(HandleKind::context));
    }
    cl_int on_clRetainContext(cl_context context) override {
        (void)context;
        called(kCmd_clRetainContext);
        handed(HandleKind::context, context);
        return CL_SUCCESS;
    }
    cl_int on_clReleaseContext(cl_context context) override {
        (void)context;
        called(kCmd_clReleaseContext);
        handed(HandleKind::context, context);
        return CL_SUCCESS;
    }
    cl_int on_clGetContextInfo(cl_context context, cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)context;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetContextInfo);
        handed(HandleKind::context, context);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_command_queue on_clCreateCommandQueue(cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int* errcode_ret) override {
        (void)context;
        (void)device;
        (void)properties;
        (void)errcode_ret;
        called(kCmd_clCreateCommandQueue);
        handed(HandleKind::context, context);
        handed(HandleKind::device, device);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_command_queue>(produce(HandleKind::command_queue));
    }
    cl_command_queue on_clCreateCommandQueueWithProperties(cl_context context, cl_device_id device, const cl_queue_properties* properties, cl_int* errcode_ret) override {
        (void)context;
        (void)device;
        (void)properties;
        (void)errcode_ret;
        called(kCmd_clCreateCommandQueueWithProperties);
        handed(HandleKind::context, context);
        handed(HandleKind::device, device);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_command_queue>(produce(HandleKind::command_queue));
    }
    cl_int on_clRetainCommandQueue(cl_command_queue command_queue) override {
        (void)command_queue;
        called(kCmd_clRetainCommandQueue);
        handed(HandleKind::command_queue, command_queue);
        return CL_SUCCESS;
    }
    cl_int on_clReleaseCommandQueue(cl_command_queue command_queue) override {
        (void)command_queue;
        called(kCmd_clReleaseCommandQueue);
        handed(HandleKind::command_queue, command_queue);
        return CL_SUCCESS;
    }
    cl_int on_clGetCommandQueueInfo(cl_command_queue command_queue, cl_command_queue_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)command_queue;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetCommandQueueInfo);
        handed(HandleKind::command_queue, command_queue);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clFlush(cl_command_queue command_queue) override {
        (void)command_queue;
        called(kCmd_clFlush);
        handed(HandleKind::command_queue, command_queue);
        return CL_SUCCESS;
    }
    cl_int on_clFinish(cl_command_queue command_queue) override {
        (void)command_queue;
        called(kCmd_clFinish);
        handed(HandleKind::command_queue, command_queue);
        return CL_SUCCESS;
    }
    cl_mem on_clCreateBuffer(cl_context context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) override {
        (void)context;
        (void)flags;
        (void)size;
        (void)host_ptr;
        (void)errcode_ret;
        called(kCmd_clCreateBuffer);
        handed(HandleKind::context, context);
        if (host_ptr != nullptr) touched(host_ptr, static_cast<size_t>(static_cast<uint64_t>(size)));
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_mem on_clCreateSubBuffer(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const cl_buffer_region* buffer_create_info, cl_int* errcode_ret) override {
        (void)buffer;
        (void)flags;
        (void)buffer_create_type;
        (void)buffer_create_info;
        (void)errcode_ret;
        called(kCmd_clCreateSubBuffer);
        handed(HandleKind::mem, buffer);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_mem on_clCreateImage(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, cl_int* errcode_ret) override {
        (void)context;
        (void)flags;
        (void)image_format;
        (void)image_desc;
        (void)host_ptr;
        (void)errcode_ret;
        called(kCmd_clCreateImage);
        handed(HandleKind::context, context);
        if (image_desc != nullptr) handed(HandleKind::mem, image_desc->buffer);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_int on_clRetainMemObject(cl_mem memobj) override {
        (void)memobj;
        called(kCmd_clRetainMemObject);
        handed(HandleKind::mem, memobj);
        return CL_SUCCESS;
    }
    cl_int on_clReleaseMemObject(cl_mem memobj) override {
        (void)memobj;
        called(kCmd_clReleaseMemObject);
        handed(HandleKind::mem, memobj);
        return CL_SUCCESS;
    }
    cl_int on_clGetMemObjectInfo(cl_mem memobj, cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)memobj;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetMemObjectInfo);
        handed(HandleKind::mem, memobj);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clGetImageInfo(cl_mem image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)image;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetImageInfo);
        handed(HandleKind::mem, image);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clGetSupportedImageFormats(cl_context context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries, cl_image_format* image_formats, cl_uint* num_image_formats) override {
        (void)context;
        (void)flags;
        (void)image_type;
        (void)num_entries;
        (void)image_formats;
        (void)num_image_formats;
        called(kCmd_clGetSupportedImageFormats);
        handed(HandleKind::context, context);
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueReadBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)buffer;
        (void)blocking_read;
        (void)offset;
        (void)size;
        (void)ptr;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueReadBuffer);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, buffer);
        if (ptr != nullptr) touched(ptr, static_cast<size_t>(static_cast<uint64_t>(size)));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueWriteBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)buffer;
        (void)blocking_write;
        (void)offset;
        (void)size;
        (void)ptr;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueWriteBuffer);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, buffer);
        if (ptr != nullptr) touched(ptr, static_cast<size_t>(static_cast<uint64_t>(size)));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueCopyBuffer(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)src_buffer;
        (void)dst_buffer;
        (void)src_offset;
        (void)dst_offset;
        (void)size;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueCopyBuffer);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, src_buffer);
        handed(HandleKind::mem, dst_buffer);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueFillBuffer(cl_command_queue command_queue, cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)buffer;
        (void)pattern;
        (void)pattern_size;
        (void)offset;
        (void)size;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueFillBuffer);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, buffer);
        if (pattern != nullptr) touched(pattern, static_cast<size_t>(static_cast<uint64_t>(pattern_size)));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueReadImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read, const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)image;
        (void)blocking_read;
        (void)origin;
        (void)region;
        (void)row_pitch;
        (void)slice_pitch;
        (void)ptr;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueReadImage);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, image);
        if (origin != nullptr) touched(origin, static_cast<size_t>(3u) * sizeof(*origin));
        if (region != nullptr) touched(region, static_cast<size_t>(3u) * sizeof(*region));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueWriteImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_write, const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)image;
        (void)blocking_write;
        (void)origin;
        (void)region;
        (void)input_row_pitch;
        (void)input_slice_pitch;
        (void)ptr;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueWriteImage);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, image);
        if (origin != nullptr) touched(origin, static_cast<size_t>(3u) * sizeof(*origin));
        if (region != nullptr) touched(region, static_cast<size_t>(3u) * sizeof(*region));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueCopyImage(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_image, const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)src_image;
        (void)dst_image;
        (void)src_origin;
        (void)dst_origin;
        (void)region;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueCopyImage);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, src_image);
        handed(HandleKind::mem, dst_image);
        if (src_origin != nullptr) touched(src_origin, static_cast<size_t>(3u) * sizeof(*src_origin));
        if (dst_origin != nullptr) touched(dst_origin, static_cast<size_t>(3u) * sizeof(*dst_origin));
        if (region != nullptr) touched(region, static_cast<size_t>(3u) * sizeof(*region));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueCopyImageToBuffer(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_buffer, const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)src_image;
        (void)dst_buffer;
        (void)src_origin;
        (void)region;
        (void)dst_offset;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueCopyImageToBuffer);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, src_image);
        handed(HandleKind::mem, dst_buffer);
        if (src_origin != nullptr) touched(src_origin, static_cast<size_t>(3u) * sizeof(*src_origin));
        if (region != nullptr) touched(region, static_cast<size_t>(3u) * sizeof(*region));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueCopyBufferToImage(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_image, size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)src_buffer;
        (void)dst_image;
        (void)src_offset;
        (void)dst_origin;
        (void)region;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueCopyBufferToImage);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, src_buffer);
        handed(HandleKind::mem, dst_image);
        if (dst_origin != nullptr) touched(dst_origin, static_cast<size_t>(3u) * sizeof(*dst_origin));
        if (region != nullptr) touched(region, static_cast<size_t>(3u) * sizeof(*region));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_program on_clCreateProgramWithSource(cl_context context, cl_uint count, const char** strings, const size_t* lengths, cl_int* errcode_ret) override {
        (void)context;
        (void)count;
        (void)strings;
        (void)lengths;
        (void)errcode_ret;
        called(kCmd_clCreateProgramWithSource);
        handed(HandleKind::context, context);
        if (strings != nullptr) touched(strings, static_cast<size_t>(static_cast<uint64_t>(count)) * sizeof(*strings));
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_program>(produce(HandleKind::program));
    }
    cl_program on_clCreateProgramWithBinary(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const size_t* lengths, const unsigned char** binaries, cl_int* binary_status, cl_int* errcode_ret) override {
        (void)context;
        (void)num_devices;
        (void)device_list;
        (void)lengths;
        (void)binaries;
        (void)binary_status;
        (void)errcode_ret;
        called(kCmd_clCreateProgramWithBinary);
        handed(HandleKind::context, context);
        for (size_t i = 0; device_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_devices)); ++i) handed(HandleKind::device, device_list[i]);
        if (binaries != nullptr) touched(binaries, static_cast<size_t>(static_cast<uint64_t>(num_devices)) * sizeof(*binaries));
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_program>(produce(HandleKind::program));
    }
    cl_int on_clRetainProgram(cl_program program) override {
        (void)program;
        called(kCmd_clRetainProgram);
        handed(HandleKind::program, program);
        return CL_SUCCESS;
    }
    cl_int on_clReleaseProgram(cl_program program) override {
        (void)program;
        called(kCmd_clReleaseProgram);
        handed(HandleKind::program, program);
        return CL_SUCCESS;
    }
    cl_int on_clBuildProgram(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options) override {
        (void)program;
        (void)num_devices;
        (void)device_list;
        (void)options;
        called(kCmd_clBuildProgram);
        handed(HandleKind::program, program);
        for (size_t i = 0; device_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_devices)); ++i) handed(HandleKind::device, device_list[i]);
        return CL_SUCCESS;
    }
    cl_int on_clGetProgramInfo(cl_program program, cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)program;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetProgramInfo);
        handed(HandleKind::program, program);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clGetProgramBuildInfo(cl_program program, cl_device_id device, cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)program;
        (void)device;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetProgramBuildInfo);
        handed(HandleKind::program, program);
        handed(HandleKind::device, device);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clUnloadPlatformCompiler(cl_platform_id platform) override {
        (void)platform;
        called(kCmd_clUnloadPlatformCompiler);
        handed(HandleKind::platform, platform);
        return CL_SUCCESS;
    }
    cl_kernel on_clCreateKernel(cl_program program, const char* kernel_name, cl_int* errcode_ret) override {
        (void)program;
        (void)kernel_name;
        (void)errcode_ret;
        called(kCmd_clCreateKernel);
        handed(HandleKind::program, program);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_kernel>(produce(HandleKind::kernel));
    }
    cl_int on_clCreateKernelsInProgram(cl_program program, cl_uint num_kernels, cl_kernel* kernels, cl_uint* num_kernels_ret) override {
        (void)program;
        (void)num_kernels;
        (void)kernels;
        (void)num_kernels_ret;
        called(kCmd_clCreateKernelsInProgram);
        handed(HandleKind::program, program);
        for (size_t i = 0; kernels != nullptr && i < static_cast<size_t>(num_kernels) && i < 2u; ++i) {
            kernels[i] = static_cast<cl_kernel>(produce(HandleKind::kernel));
        }
        if (num_kernels_ret != nullptr) *num_kernels_ret = 2u;
        return CL_SUCCESS;
    }
    cl_int on_clRetainKernel(cl_kernel kernel) override {
        (void)kernel;
        called(kCmd_clRetainKernel);
        handed(HandleKind::kernel, kernel);
        return CL_SUCCESS;
    }
    cl_int on_clReleaseKernel(cl_kernel kernel) override {
        (void)kernel;
        called(kCmd_clReleaseKernel);
        handed(HandleKind::kernel, kernel);
        return CL_SUCCESS;
    }
    cl_int on_clSetKernelArg(cl_kernel kernel, cl_uint arg_index, size_t arg_size, const void* arg_value) override {
        (void)kernel;
        (void)arg_index;
        (void)arg_size;
        (void)arg_value;
        called(kCmd_clSetKernelArg);
        handed(HandleKind::kernel, kernel);
        if (arg_value != nullptr) touched(arg_value, static_cast<size_t>(static_cast<uint64_t>(arg_size)));
        return CL_SUCCESS;
    }
    cl_int on_clGetKernelInfo(cl_kernel kernel, cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)kernel;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetKernelInfo);
        handed(HandleKind::kernel, kernel);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clGetKernelArgInfo(cl_kernel kernel, cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)kernel;
        (void)arg_index;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetKernelArgInfo);
        handed(HandleKind::kernel, kernel);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clGetKernelWorkGroupInfo(cl_kernel kernel, cl_device_id device, cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)kernel;
        (void)device;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetKernelWorkGroupInfo);
        handed(HandleKind::kernel, kernel);
        handed(HandleKind::device, device);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueNDRangeKernel(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)kernel;
        (void)work_dim;
        (void)global_work_offset;
        (void)global_work_size;
        (void)local_work_size;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueNDRangeKernel);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::kernel, kernel);
        if (global_work_offset != nullptr) touched(global_work_offset, static_cast<size_t>(static_cast<uint64_t>(work_dim)) * sizeof(*global_work_offset));
        if (global_work_size != nullptr) touched(global_work_size, static_cast<size_t>(static_cast<uint64_t>(work_dim)) * sizeof(*global_work_size));
        if (local_work_size != nullptr) touched(local_work_size, static_cast<size_t>(static_cast<uint64_t>(work_dim)) * sizeof(*local_work_size));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueMarkerWithWaitList(cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueMarkerWithWaitList);
        handed(HandleKind::command_queue, command_queue);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueBarrierWithWaitList(cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueBarrierWithWaitList);
        handed(HandleKind::command_queue, command_queue);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clWaitForEvents(cl_uint num_events, const cl_event* event_list) override {
        (void)num_events;
        (void)event_list;
        called(kCmd_clWaitForEvents);
        for (size_t i = 0; event_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events)); ++i) handed(HandleKind::event, event_list[i]);
        return CL_SUCCESS;
    }
    cl_int on_clGetEventInfo(cl_event event, cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)event;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetEventInfo);
        handed(HandleKind::event, event);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_event on_clCreateUserEvent(cl_context context, cl_int* errcode_ret) override {
        (void)context;
        (void)errcode_ret;
        called(kCmd_clCreateUserEvent);
        handed(HandleKind::context, context);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_event>(produce(HandleKind::event));
    }
    cl_int on_clSetUserEventStatus(cl_event event, cl_int execution_status) override {
        (void)event;
        (void)execution_status;
        called(kCmd_clSetUserEventStatus);
        handed(HandleKind::event, event);
        return CL_SUCCESS;
    }
    cl_int on_clRetainEvent(cl_event event) override {
        (void)event;
        called(kCmd_clRetainEvent);
        handed(HandleKind::event, event);
        return CL_SUCCESS;
    }
    cl_int on_clReleaseEvent(cl_event event) override {
        (void)event;
        called(kCmd_clReleaseEvent);
        handed(HandleKind::event, event);
        return CL_SUCCESS;
    }
    cl_int on_clGetEventProfilingInfo(cl_event event, cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)event;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetEventProfilingInfo);
        handed(HandleKind::event, event);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_mem on_clCreateFromGLBuffer(cl_context context, cl_mem_flags flags, cl_GLuint bufobj, cl_int* errcode_ret) override {
        (void)context;
        (void)flags;
        (void)bufobj;
        (void)errcode_ret;
        called(kCmd_clCreateFromGLBuffer);
        handed(HandleKind::context, context);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_mem on_clCreateFromGLTexture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) override {
        (void)context;
        (void)flags;
        (void)target;
        (void)miplevel;
        (void)texture;
        (void)errcode_ret;
        called(kCmd_clCreateFromGLTexture);
        handed(HandleKind::context, context);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_mem on_clCreateFromGLTexture2D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) override {
        (void)context;
        (void)flags;
        (void)target;
        (void)miplevel;
        (void)texture;
        (void)errcode_ret;
        called(kCmd_clCreateFromGLTexture2D);
        handed(HandleKind::context, context);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_mem on_clCreateFromGLTexture3D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) override {
        (void)context;
        (void)flags;
        (void)target;
        (void)miplevel;
        (void)texture;
        (void)errcode_ret;
        called(kCmd_clCreateFromGLTexture3D);
        handed(HandleKind::context, context);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_mem on_clCreateFromGLRenderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int* errcode_ret) override {
        (void)context;
        (void)flags;
        (void)renderbuffer;
        (void)errcode_ret;
        called(kCmd_clCreateFromGLRenderbuffer);
        handed(HandleKind::context, context);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_int on_clGetGLObjectInfo(cl_mem memobj, cl_gl_object_type* gl_object_type, cl_GLuint* gl_object_name) override {
        (void)memobj;
        (void)gl_object_type;
        (void)gl_object_name;
        called(kCmd_clGetGLObjectInfo);
        handed(HandleKind::mem, memobj);
        return CL_SUCCESS;
    }
    cl_int on_clGetGLTextureInfo(cl_mem memobj, cl_gl_texture_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)memobj;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetGLTextureInfo);
        handed(HandleKind::mem, memobj);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueAcquireGLObjects(cl_command_queue command_queue, cl_uint num_objects, const cl_mem* mem_objects, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)num_objects;
        (void)mem_objects;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueAcquireGLObjects);
        handed(HandleKind::command_queue, command_queue);
        for (size_t i = 0; mem_objects != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_objects)); ++i) handed(HandleKind::mem, mem_objects[i]);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueReleaseGLObjects(cl_command_queue command_queue, cl_uint num_objects, const cl_mem* mem_objects, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)num_objects;
        (void)mem_objects;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueReleaseGLObjects);
        handed(HandleKind::command_queue, command_queue);
        for (size_t i = 0; mem_objects != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_objects)); ++i) handed(HandleKind::mem, mem_objects[i]);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clGetGLContextInfoKHR(const cl_context_properties* properties, cl_gl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)properties;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetGLContextInfoKHR);
        for (size_t i = 0; properties != nullptr && properties[i] != 0; i += 2) {
            if (properties[i] == CL_CONTEXT_PLATFORM) handed(HandleKind::platform, reinterpret_cast<void*>(properties[i + 1]));
        }
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clRetainDevice(cl_device_id device) override {
        (void)device;
        called(kCmd_clRetainDevice);
        handed(HandleKind::device, device);
        return CL_SUCCESS;
    }
    cl_int on_clReleaseDevice(cl_device_id device) override {
        (void)device;
        called(kCmd_clReleaseDevice);
        handed(HandleKind::device, device);
        return CL_SUCCESS;
    }
    cl_int on_clCreateSubDevices(cl_device_id in_device, const cl_device_partition_property* properties, cl_uint num_devices, cl_device_id* out_devices, cl_uint* num_devices_ret) override {
        (void)in_device;
        (void)properties;
        (void)num_devices;
        (void)out_devices;
        (void)num_devices_ret;
        called(kCmd_clCreateSubDevices);
        handed(HandleKind::device, in_device);
        for (size_t i = 0; out_devices != nullptr && i < static_cast<size_t>(num_devices) && i < 2u; ++i) {
            out_devices[i] = static_cast<cl_device_id>(produce(HandleKind::device));
        }
        if (num_devices_ret != nullptr) *num_devices_ret = 2u;
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueTask(cl_command_queue command_queue, cl_kernel kernel, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)kernel;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueTask);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::kernel, kernel);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueMarker(cl_command_queue command_queue, cl_event* event) override {
        (void)command_queue;
        (void)event;
        called(kCmd_clEnqueueMarker);
        handed(HandleKind::command_queue, command_queue);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueBarrier(cl_command_queue command_queue) override {
        (void)command_queue;
        called(kCmd_clEnqueueBarrier);
        handed(HandleKind::command_queue, command_queue);
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueWaitForEvents(cl_command_queue command_queue, cl_uint num_events, const cl_event* event_list) override {
        (void)command_queue;
        (void)num_events;
        (void)event_list;
        called(kCmd_clEnqueueWaitForEvents);
        handed(HandleKind::command_queue, command_queue);
        for (size_t i = 0; event_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events)); ++i) handed(HandleKind::event, event_list[i]);
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueMigrateMemObjects(cl_command_queue command_queue, cl_uint num_mem_objects, const cl_mem* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)num_mem_objects;
        (void)mem_objects;
        (void)flags;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueMigrateMemObjects);
        handed(HandleKind::command_queue, command_queue);
        for (size_t i = 0; mem_objects != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_mem_objects)); ++i) handed(HandleKind::mem, mem_objects[i]);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueReadBufferRect(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)buffer;
        (void)blocking_read;
        (void)buffer_origin;
        (void)host_origin;
        (void)region;
        (void)buffer_row_pitch;
        (void)buffer_slice_pitch;
        (void)host_row_pitch;
        (void)host_slice_pitch;
        (void)ptr;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueReadBufferRect);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, buffer);
        if (buffer_origin != nullptr) touched(buffer_origin, static_cast<size_t>(3u) * sizeof(*buffer_origin));
        if (host_origin != nullptr) touched(host_origin, static_cast<size_t>(3u) * sizeof(*host_origin));
        if (region != nullptr) touched(region, static_cast<size_t>(3u) * sizeof(*region));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueWriteBufferRect(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)buffer;
        (void)blocking_write;
        (void)buffer_origin;
        (void)host_origin;
        (void)region;
        (void)buffer_row_pitch;
        (void)buffer_slice_pitch;
        (void)host_row_pitch;
        (void)host_slice_pitch;
        (void)ptr;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueWriteBufferRect);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, buffer);
        if (buffer_origin != nullptr) touched(buffer_origin, static_cast<size_t>(3u) * sizeof(*buffer_origin));
        if (host_origin != nullptr) touched(host_origin, static_cast<size_t>(3u) * sizeof(*host_origin));
        if (region != nullptr) touched(region, static_cast<size_t>(3u) * sizeof(*region));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueCopyBufferRect(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer, const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch, size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)src_buffer;
        (void)dst_buffer;
        (void)src_origin;
        (void)dst_origin;
        (void)region;
        (void)src_row_pitch;
        (void)src_slice_pitch;
        (void)dst_row_pitch;
        (void)dst_slice_pitch;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueCopyBufferRect);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, src_buffer);
        handed(HandleKind::mem, dst_buffer);
        if (src_origin != nullptr) touched(src_origin, static_cast<size_t>(3u) * sizeof(*src_origin));
        if (dst_origin != nullptr) touched(dst_origin, static_cast<size_t>(3u) * sizeof(*dst_origin));
        if (region != nullptr) touched(region, static_cast<size_t>(3u) * sizeof(*region));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueFillImage(cl_command_queue command_queue, cl_mem image, const void* fill_color, const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)image;
        (void)fill_color;
        (void)origin;
        (void)region;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueFillImage);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, image);
        if (origin != nullptr) touched(origin, static_cast<size_t>(3u) * sizeof(*origin));
        if (region != nullptr) touched(region, static_cast<size_t>(3u) * sizeof(*region));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_mem on_clCreateImage2D(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, cl_int* errcode_ret) override {
        (void)context;
        (void)flags;
        (void)image_format;
        (void)image_width;
        (void)image_height;
        (void)image_row_pitch;
        (void)host_ptr;
        (void)errcode_ret;
        called(kCmd_clCreateImage2D);
        handed(HandleKind::context, context);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_mem on_clCreateImage3D(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch, void* host_ptr, cl_int* errcode_ret) override {
        (void)context;
        (void)flags;
        (void)image_format;
        (void)image_width;
        (void)image_height;
        (void)image_depth;
        (void)image_row_pitch;
        (void)image_slice_pitch;
        (void)host_ptr;
        (void)errcode_ret;
        called(kCmd_clCreateImage3D);
        handed(HandleKind::context, context);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_int on_clUnloadCompiler() override {
        called(kCmd_clUnloadCompiler);
        return CL_SUCCESS;
    }
    cl_int on_clSetCommandQueueProperty(cl_command_queue command_queue, cl_command_queue_properties properties, cl_bool enable, cl_command_queue_properties* old_properties) override {
        (void)command_queue;
        (void)properties;
        (void)enable;
        (void)old_properties;
        called(kCmd_clSetCommandQueueProperty);
        handed(HandleKind::command_queue, command_queue);
        return CL_SUCCESS;
    }
    cl_program on_clCreateProgramWithIL(cl_context context, const void* il, size_t length, cl_int* errcode_ret) override {
        (void)context;
        (void)il;
        (void)length;
        (void)errcode_ret;
        called(kCmd_clCreateProgramWithIL);
        handed(HandleKind::context, context);
        if (il != nullptr) touched(il, static_cast<size_t>(static_cast<uint64_t>(length)));
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_program>(produce(HandleKind::program));
    }
    cl_int on_clCompileProgram(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_headers, const cl_program* input_headers, const char** header_include_names) override {
        (void)program;
        (void)num_devices;
        (void)device_list;
        (void)options;
        (void)num_input_headers;
        (void)input_headers;
        (void)header_include_names;
        called(kCmd_clCompileProgram);
        handed(HandleKind::program, program);
        for (size_t i = 0; device_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_devices)); ++i) handed(HandleKind::device, device_list[i]);
        for (size_t i = 0; input_headers != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_input_headers)); ++i) handed(HandleKind::program, input_headers[i]);
        if (header_include_names != nullptr) touched(header_include_names, static_cast<size_t>(static_cast<uint64_t>(num_input_headers)) * sizeof(*header_include_names));
        return CL_SUCCESS;
    }
    cl_program on_clLinkProgram(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_programs, const cl_program* input_programs, cl_int* errcode_ret) override {
        (void)context;
        (void)num_devices;
        (void)device_list;
        (void)options;
        (void)num_input_programs;
        (void)input_programs;
        (void)errcode_ret;
        called(kCmd_clLinkProgram);
        handed(HandleKind::context, context);
        for (size_t i = 0; device_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_devices)); ++i) handed(HandleKind::device, device_list[i]);
        for (size_t i = 0; input_programs != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_input_programs)); ++i) handed(HandleKind::program, input_programs[i]);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_program>(produce(HandleKind::program));
    }
    cl_int on_clSetProgramSpecializationConstant(cl_program program, cl_uint spec_id, size_t spec_size, const void* spec_value) override {
        (void)program;
        (void)spec_id;
        (void)spec_size;
        (void)spec_value;
        called(kCmd_clSetProgramSpecializationConstant);
        handed(HandleKind::program, program);
        if (spec_value != nullptr) touched(spec_value, static_cast<size_t>(static_cast<uint64_t>(spec_size)));
        return CL_SUCCESS;
    }
    cl_kernel on_clCloneKernel(cl_kernel source_kernel, cl_int* errcode_ret) override {
        (void)source_kernel;
        (void)errcode_ret;
        called(kCmd_clCloneKernel);
        handed(HandleKind::kernel, source_kernel);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_kernel>(produce(HandleKind::kernel));
    }
    cl_int on_clSetKernelExecInfo(cl_kernel kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value) override {
        (void)kernel;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        called(kCmd_clSetKernelExecInfo);
        handed(HandleKind::kernel, kernel);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(static_cast<uint64_t>(param_value_size)));
        return CL_SUCCESS;
    }
    cl_int on_clGetKernelSubGroupInfo(cl_kernel kernel, cl_device_id device, cl_kernel_sub_group_info param_name, size_t input_value_size, const void* input_value, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)kernel;
        (void)device;
        (void)param_name;
        (void)input_value_size;
        (void)input_value;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetKernelSubGroupInfo);
        handed(HandleKind::kernel, kernel);
        handed(HandleKind::device, device);
        if (input_value != nullptr) touched(input_value, static_cast<size_t>(static_cast<uint64_t>(input_value_size)));
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_int on_clGetHostTimer(cl_device_id device, cl_ulong* host_timestamp) override {
        (void)device;
        (void)host_timestamp;
        called(kCmd_clGetHostTimer);
        handed(HandleKind::device, device);
        return CL_SUCCESS;
    }
    cl_int on_clGetDeviceAndHostTimer(cl_device_id device, cl_ulong* device_timestamp, cl_ulong* host_timestamp) override {
        (void)device;
        (void)device_timestamp;
        (void)host_timestamp;
        called(kCmd_clGetDeviceAndHostTimer);
        handed(HandleKind::device, device);
        return CL_SUCCESS;
    }
    cl_mem on_clCreateBufferWithProperties(cl_context context, const cl_mem_properties* properties, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) override {
        (void)context;
        (void)properties;
        (void)flags;
        (void)size;
        (void)host_ptr;
        (void)errcode_ret;
        called(kCmd_clCreateBufferWithProperties);
        handed(HandleKind::context, context);
        if (host_ptr != nullptr) touched(host_ptr, static_cast<size_t>(static_cast<uint64_t>(size)));
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_mem on_clCreateImageWithProperties(cl_context context, const cl_mem_properties* properties, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, cl_int* errcode_ret) override {
        (void)context;
        (void)properties;
        (void)flags;
        (void)image_format;
        (void)image_desc;
        (void)host_ptr;
        (void)errcode_ret;
        called(kCmd_clCreateImageWithProperties);
        handed(HandleKind::context, context);
        if (image_desc != nullptr) handed(HandleKind::mem, image_desc->buffer);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_sampler on_clCreateSamplerWithProperties(cl_context context, const cl_sampler_properties* sampler_properties, cl_int* errcode_ret) override {
        (void)context;
        (void)sampler_properties;
        (void)errcode_ret;
        called(kCmd_clCreateSamplerWithProperties);
        handed(HandleKind::context, context);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_sampler>(produce(HandleKind::sampler));
    }
    cl_int on_clSetDefaultDeviceCommandQueue(cl_context context, cl_device_id device, cl_command_queue command_queue) override {
        (void)context;
        (void)device;
        (void)command_queue;
        called(kCmd_clSetDefaultDeviceCommandQueue);
        handed(HandleKind::context, context);
        handed(HandleKind::device, device);
        handed(HandleKind::command_queue, command_queue);
        return CL_SUCCESS;
    }
    cl_sampler on_clCreateSampler(cl_context context, cl_bool normalized_coords, cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret) override {
        (void)context;
        (void)normalized_coords;
        (void)addressing_mode;
        (void)filter_mode;
        (void)errcode_ret;
        called(kCmd_clCreateSampler);
        handed(HandleKind::context, context);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_sampler>(produce(HandleKind::sampler));
    }
    cl_int on_clRetainSampler(cl_sampler sampler) override {
        (void)sampler;
        called(kCmd_clRetainSampler);
        handed(HandleKind::sampler, sampler);
        return CL_SUCCESS;
    }
    cl_int on_clReleaseSampler(cl_sampler sampler) override {
        (void)sampler;
        called(kCmd_clReleaseSampler);
        handed(HandleKind::sampler, sampler);
        return CL_SUCCESS;
    }
    cl_int on_clGetSamplerInfo(cl_sampler sampler, cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)sampler;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetSamplerInfo);
        handed(HandleKind::sampler, sampler);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_mem on_clCreatePipe(cl_context context, cl_mem_flags flags, cl_uint pipe_packet_size, cl_uint pipe_max_packets, const cl_pipe_properties* properties, cl_int* errcode_ret) override {
        (void)context;
        (void)flags;
        (void)pipe_packet_size;
        (void)pipe_max_packets;
        (void)properties;
        (void)errcode_ret;
        called(kCmd_clCreatePipe);
        handed(HandleKind::context, context);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_mem>(produce(HandleKind::mem));
    }
    cl_int on_clGetPipeInfo(cl_mem pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        (void)pipe;
        (void)param_name;
        (void)param_value_size;
        (void)param_value;
        (void)param_value_size_ret;
        called(kCmd_clGetPipeInfo);
        handed(HandleKind::mem, pipe);
        if (param_value != nullptr) touched(param_value, static_cast<size_t>(param_value_size));
        return CL_SUCCESS;
    }
    cl_program on_clCreateProgramWithBuiltInKernels(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* kernel_names, cl_int* errcode_ret) override {
        (void)context;
        (void)num_devices;
        (void)device_list;
        (void)kernel_names;
        (void)errcode_ret;
        called(kCmd_clCreateProgramWithBuiltInKernels);
        handed(HandleKind::context, context);
        for (size_t i = 0; device_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_devices)); ++i) handed(HandleKind::device, device_list[i]);
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return static_cast<cl_program>(produce(HandleKind::program));
    }
    void* on_clEnqueueMapBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map, cl_map_flags map_flags, size_t offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event, cl_int* errcode_ret) override {
        (void)command_queue;
        (void)buffer;
        (void)blocking_map;
        (void)map_flags;
        (void)offset;
        (void)size;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        (void)errcode_ret;
        called(kCmd_clEnqueueMapBuffer);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, buffer);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return nullptr;
    }
    cl_int on_clEnqueueUnmapMemObject(cl_command_queue command_queue, cl_mem memobj, void* mapped_ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)memobj;
        (void)mapped_ptr;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueUnmapMemObject);
        handed(HandleKind::command_queue, command_queue);
        handed(HandleKind::mem, memobj);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    void* on_clSVMAlloc(cl_context context, cl_svm_mem_flags flags, size_t size, cl_uint alignment) override {
        (void)context;
        (void)flags;
        (void)size;
        (void)alignment;
        called(kCmd_clSVMAlloc);
        handed(HandleKind::context, context);
        return nullptr;
    }
    void on_clSVMFree(cl_context context, void* svm_pointer) override {
        (void)context;
        (void)svm_pointer;
        called(kCmd_clSVMFree);
        handed(HandleKind::context, context);
    }
    cl_int on_clEnqueueSVMMap(cl_command_queue command_queue, cl_bool blocking_map, cl_map_flags flags, void* svm_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)blocking_map;
        (void)flags;
        (void)svm_ptr;
        (void)size;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueSVMMap);
        handed(HandleKind::command_queue, command_queue);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueSVMUnmap(cl_command_queue command_queue, void* svm_ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)svm_ptr;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueSVMUnmap);
        handed(HandleKind::command_queue, command_queue);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueSVMMemcpy(cl_command_queue command_queue, cl_bool blocking_copy, void* dst_ptr, const void* src_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)blocking_copy;
        (void)dst_ptr;
        (void)src_ptr;
        (void)size;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueSVMMemcpy);
        handed(HandleKind::command_queue, command_queue);
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clEnqueueSVMMemFill(cl_command_queue command_queue, void* svm_ptr, const void* pattern, size_t pattern_size, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        (void)command_queue;
        (void)svm_ptr;
        (void)pattern;
        (void)pattern_size;
        (void)size;
        (void)num_events_in_wait_list;
        (void)event_wait_list;
        (void)event;
        called(kCmd_clEnqueueSVMMemFill);
        handed(HandleKind::command_queue, command_queue);
        if (pattern != nullptr) touched(pattern, static_cast<size_t>(static_cast<uint64_t>(pattern_size)));
        for (size_t i = 0; event_wait_list != nullptr && i < static_cast<size_t>(static_cast<uint64_t>(num_events_in_wait_list)); ++i) handed(HandleKind::event, event_wait_list[i]);
        if (event != nullptr) *event = static_cast<cl_event>(produce(HandleKind::event));
        return CL_SUCCESS;
    }
    cl_int on_clSetKernelArgSVMPointer(cl_kernel kernel, cl_uint arg_index, const void* arg_value) override {
        (void)kernel;
        (void)arg_index;
        (void)arg_value;
        called(kCmd_clSetKernelArgSVMPointer);
        handed(HandleKind::kernel, kernel);
        return CL_SUCCESS;
    }
    cl_int on_clSetEventCallback(cl_event event, cl_int command_exec_callback_type) override {
        (void)event;
        (void)command_exec_callback_type;
        called(kCmd_clSetEventCallback);
        handed(HandleKind::event, event);
        return CL_SUCCESS;
    }
    bool declared_argument(cl_kernel, cl_uint, KernelArgument&) override { return false; }

protected:
    virtual void called(uint32_t command_id) = 0;
    virtual void handed(HandleKind kind, void* object) = 0;
    virtual void touched(const void* bytes, size_t length) = 0;
    virtual void* produce(HandleKind kind) = 0;
};

}  // namespace av::clproto
