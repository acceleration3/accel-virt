// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.opencl --xml cl.xml --out ...

#pragma once

#include <stddef.h>
#include <stdint.h>

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

#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av::clproto {

// The objects a guest names by number. Which numbers are objects is
// the server's to say: see ServerDispatch::handle_for.
enum class HandleKind : uint8_t {
    command_queue,
    context,
    device,
    event,
    kernel,
    mem,
    platform,
    program,
    sampler,
};

// Implemented by the server; the table below calls into it.
class ServerDispatch {
public:
    virtual ~ServerDispatch() = default;

    // The object of this kind the server made under this number, or
    // null for a number it did not: the decoder refuses the command
    // rather than hand the host's OpenCL a pointer a guest invented.
    virtual void* handle_for(HandleKind kind, uint64_t wire) const = 0;
    // An object a successful call is about to give the guest a number
    // for; from here on that number is the object.
    virtual void made(HandleKind kind, void* object) = 0;
    // A successful retain or release of one. After its last release
    // the number names nothing.
    virtual void retained(HandleKind kind, void* object) = 0;
    virtual void released(HandleKind kind, void* object) = 0;
    // The address a guest names, as a pointer, or null for one that
    // does not lie - with the `bytes` the command will read or write
    // from it - inside a range this server handed out. Unlike a
    // handle it need not name the start of one: a kernel argument
    // may point into the middle of an allocation.
    virtual void* svm_for(uint64_t address, uint64_t bytes) const = 0;
    // The guest's own memory at this offset of its context's blob
    // arena, `bytes` long, as a pointer, or null for a span that is not
    // wholly inside it. A lent parameter names this instead of carrying
    // its bytes, and the call reads or writes it in place.
    virtual void* lent_for(uint64_t offset, uint64_t bytes) const = 0;

    virtual cl_int on_clGetPlatformIDs(cl_uint num_entries, cl_platform_id* platforms, cl_uint* num_platforms) = 0;
    virtual cl_int on_clGetPlatformInfo(cl_platform_id platform, cl_platform_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries, cl_device_id* devices, cl_uint* num_devices) = 0;
    virtual cl_int on_clGetDeviceInfo(cl_device_id device, cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_context on_clCreateContext(const cl_context_properties* properties, cl_uint num_devices, const cl_device_id* devices, cl_int* errcode_ret) = 0;
    virtual cl_context on_clCreateContextFromType(const cl_context_properties* properties, cl_device_type device_type, cl_int* errcode_ret) = 0;
    virtual cl_int on_clRetainContext(cl_context context) = 0;
    virtual cl_int on_clReleaseContext(cl_context context) = 0;
    virtual cl_int on_clGetContextInfo(cl_context context, cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_command_queue on_clCreateCommandQueue(cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int* errcode_ret) = 0;
    virtual cl_command_queue on_clCreateCommandQueueWithProperties(cl_context context, cl_device_id device, const cl_queue_properties* properties, cl_int* errcode_ret) = 0;
    virtual cl_int on_clRetainCommandQueue(cl_command_queue command_queue) = 0;
    virtual cl_int on_clReleaseCommandQueue(cl_command_queue command_queue) = 0;
    virtual cl_int on_clGetCommandQueueInfo(cl_command_queue command_queue, cl_command_queue_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clFlush(cl_command_queue command_queue) = 0;
    virtual cl_int on_clFinish(cl_command_queue command_queue) = 0;
    virtual cl_mem on_clCreateBuffer(cl_context context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) = 0;
    virtual cl_mem on_clCreateSubBuffer(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const cl_buffer_region* buffer_create_info, cl_int* errcode_ret) = 0;
    virtual cl_mem on_clCreateImage(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, cl_int* errcode_ret) = 0;
    virtual cl_int on_clRetainMemObject(cl_mem memobj) = 0;
    virtual cl_int on_clReleaseMemObject(cl_mem memobj) = 0;
    virtual cl_int on_clGetMemObjectInfo(cl_mem memobj, cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clGetImageInfo(cl_mem image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clGetSupportedImageFormats(cl_context context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries, cl_image_format* image_formats, cl_uint* num_image_formats) = 0;
    virtual cl_int on_clEnqueueReadBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueWriteBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueCopyBuffer(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueFillBuffer(cl_command_queue command_queue, cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueReadImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read, const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueWriteImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_write, const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueCopyImage(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_image, const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueCopyImageToBuffer(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_buffer, const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueCopyBufferToImage(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_image, size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_program on_clCreateProgramWithSource(cl_context context, cl_uint count, const char** strings, const size_t* lengths, cl_int* errcode_ret) = 0;
    virtual cl_program on_clCreateProgramWithBinary(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const size_t* lengths, const unsigned char** binaries, cl_int* binary_status, cl_int* errcode_ret) = 0;
    virtual cl_int on_clRetainProgram(cl_program program) = 0;
    virtual cl_int on_clReleaseProgram(cl_program program) = 0;
    virtual cl_int on_clBuildProgram(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options) = 0;
    virtual cl_int on_clGetProgramInfo(cl_program program, cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clGetProgramBuildInfo(cl_program program, cl_device_id device, cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clUnloadPlatformCompiler(cl_platform_id platform) = 0;
    virtual cl_kernel on_clCreateKernel(cl_program program, const char* kernel_name, cl_int* errcode_ret) = 0;
    virtual cl_int on_clCreateKernelsInProgram(cl_program program, cl_uint num_kernels, cl_kernel* kernels, cl_uint* num_kernels_ret) = 0;
    virtual cl_int on_clRetainKernel(cl_kernel kernel) = 0;
    virtual cl_int on_clReleaseKernel(cl_kernel kernel) = 0;
    virtual cl_int on_clSetKernelArg(cl_kernel kernel, cl_uint arg_index, size_t arg_size, const void* arg_value) = 0;
    virtual cl_int on_clGetKernelInfo(cl_kernel kernel, cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clGetKernelArgInfo(cl_kernel kernel, cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clGetKernelWorkGroupInfo(cl_kernel kernel, cl_device_id device, cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clEnqueueNDRangeKernel(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueMarkerWithWaitList(cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueBarrierWithWaitList(cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clWaitForEvents(cl_uint num_events, const cl_event* event_list) = 0;
    virtual cl_int on_clGetEventInfo(cl_event event, cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_event on_clCreateUserEvent(cl_context context, cl_int* errcode_ret) = 0;
    virtual cl_int on_clSetUserEventStatus(cl_event event, cl_int execution_status) = 0;
    virtual cl_int on_clRetainEvent(cl_event event) = 0;
    virtual cl_int on_clReleaseEvent(cl_event event) = 0;
    virtual cl_int on_clGetEventProfilingInfo(cl_event event, cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_mem on_clCreateFromGLBuffer(cl_context context, cl_mem_flags flags, cl_GLuint bufobj, cl_int* errcode_ret) = 0;
    virtual cl_mem on_clCreateFromGLTexture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) = 0;
    virtual cl_mem on_clCreateFromGLTexture2D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) = 0;
    virtual cl_mem on_clCreateFromGLTexture3D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) = 0;
    virtual cl_mem on_clCreateFromGLRenderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int* errcode_ret) = 0;
    virtual cl_int on_clGetGLObjectInfo(cl_mem memobj, cl_gl_object_type* gl_object_type, cl_GLuint* gl_object_name) = 0;
    virtual cl_int on_clGetGLTextureInfo(cl_mem memobj, cl_gl_texture_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clEnqueueAcquireGLObjects(cl_command_queue command_queue, cl_uint num_objects, const cl_mem* mem_objects, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueReleaseGLObjects(cl_command_queue command_queue, cl_uint num_objects, const cl_mem* mem_objects, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clGetGLContextInfoKHR(const cl_context_properties* properties, cl_gl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clRetainDevice(cl_device_id device) = 0;
    virtual cl_int on_clReleaseDevice(cl_device_id device) = 0;
    virtual cl_int on_clCreateSubDevices(cl_device_id in_device, const cl_device_partition_property* properties, cl_uint num_devices, cl_device_id* out_devices, cl_uint* num_devices_ret) = 0;
    virtual cl_int on_clEnqueueTask(cl_command_queue command_queue, cl_kernel kernel, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueMarker(cl_command_queue command_queue, cl_event* event) = 0;
    virtual cl_int on_clEnqueueBarrier(cl_command_queue command_queue) = 0;
    virtual cl_int on_clEnqueueWaitForEvents(cl_command_queue command_queue, cl_uint num_events, const cl_event* event_list) = 0;
    virtual cl_int on_clEnqueueMigrateMemObjects(cl_command_queue command_queue, cl_uint num_mem_objects, const cl_mem* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueReadBufferRect(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueWriteBufferRect(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueCopyBufferRect(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer, const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch, size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueFillImage(cl_command_queue command_queue, cl_mem image, const void* fill_color, const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_mem on_clCreateImage2D(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, cl_int* errcode_ret) = 0;
    virtual cl_mem on_clCreateImage3D(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch, void* host_ptr, cl_int* errcode_ret) = 0;
    virtual cl_int on_clUnloadCompiler() = 0;
    virtual cl_int on_clSetCommandQueueProperty(cl_command_queue command_queue, cl_command_queue_properties properties, cl_bool enable, cl_command_queue_properties* old_properties) = 0;
    virtual cl_program on_clCreateProgramWithIL(cl_context context, const void* il, size_t length, cl_int* errcode_ret) = 0;
    virtual cl_int on_clCompileProgram(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_headers, const cl_program* input_headers, const char** header_include_names) = 0;
    virtual cl_program on_clLinkProgram(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_programs, const cl_program* input_programs, cl_int* errcode_ret) = 0;
    virtual cl_int on_clSetProgramSpecializationConstant(cl_program program, cl_uint spec_id, size_t spec_size, const void* spec_value) = 0;
    virtual cl_kernel on_clCloneKernel(cl_kernel source_kernel, cl_int* errcode_ret) = 0;
    virtual cl_int on_clSetKernelExecInfo(cl_kernel kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value) = 0;
    virtual cl_int on_clGetKernelSubGroupInfo(cl_kernel kernel, cl_device_id device, cl_kernel_sub_group_info param_name, size_t input_value_size, const void* input_value, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_int on_clGetHostTimer(cl_device_id device, cl_ulong* host_timestamp) = 0;
    virtual cl_int on_clGetDeviceAndHostTimer(cl_device_id device, cl_ulong* device_timestamp, cl_ulong* host_timestamp) = 0;
    virtual cl_mem on_clCreateBufferWithProperties(cl_context context, const cl_mem_properties* properties, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) = 0;
    virtual cl_mem on_clCreateImageWithProperties(cl_context context, const cl_mem_properties* properties, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, cl_int* errcode_ret) = 0;
    virtual cl_sampler on_clCreateSamplerWithProperties(cl_context context, const cl_sampler_properties* sampler_properties, cl_int* errcode_ret) = 0;
    virtual cl_int on_clSetDefaultDeviceCommandQueue(cl_context context, cl_device_id device, cl_command_queue command_queue) = 0;
    virtual cl_sampler on_clCreateSampler(cl_context context, cl_bool normalized_coords, cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret) = 0;
    virtual cl_int on_clRetainSampler(cl_sampler sampler) = 0;
    virtual cl_int on_clReleaseSampler(cl_sampler sampler) = 0;
    virtual cl_int on_clGetSamplerInfo(cl_sampler sampler, cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_mem on_clCreatePipe(cl_context context, cl_mem_flags flags, cl_uint pipe_packet_size, cl_uint pipe_max_packets, const cl_pipe_properties* properties, cl_int* errcode_ret) = 0;
    virtual cl_int on_clGetPipeInfo(cl_mem pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) = 0;
    virtual cl_program on_clCreateProgramWithBuiltInKernels(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* kernel_names, cl_int* errcode_ret) = 0;
    virtual void* on_clEnqueueMapBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map, cl_map_flags map_flags, size_t offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event, cl_int* errcode_ret) = 0;
    virtual cl_int on_clEnqueueUnmapMemObject(cl_command_queue command_queue, cl_mem memobj, void* mapped_ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual void* on_clSVMAlloc(cl_context context, cl_svm_mem_flags flags, size_t size, cl_uint alignment) = 0;
    virtual void on_clSVMFree(cl_context context, void* svm_pointer) = 0;
    virtual cl_int on_clEnqueueSVMMap(cl_command_queue command_queue, cl_bool blocking_map, cl_map_flags flags, void* svm_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueSVMUnmap(cl_command_queue command_queue, void* svm_ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueSVMMemcpy(cl_command_queue command_queue, cl_bool blocking_copy, void* dst_ptr, const void* src_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clEnqueueSVMMemFill(cl_command_queue command_queue, void* svm_ptr, const void* pattern, size_t pattern_size, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) = 0;
    virtual cl_int on_clSetKernelArgSVMPointer(cl_kernel kernel, cl_uint arg_index, const void* arg_value) = 0;
    virtual cl_int on_clSetEventCallback(cl_event event, cl_int command_exec_callback_type) = 0;
};

// `arena` is where everything a decoded command points at lives,
// for the length of the call; it is the caller's block, because it
// has to outlive the decode that filled it.
[[nodiscard]] bool dispatch_command(ServerDispatch& s, uint32_t cmd_id,
                                    ::av::WireReader& r, ::av::WireWriter& reply,
                                    ::av::WireArena& arena);

}  // namespace av::clproto
