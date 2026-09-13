// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opencl --xml cl.xml --out ...

#pragma once

#include <cstddef>
#include <cstdint>

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

#include "rvk/protocol/transport.hpp"

namespace av::clproto {

cl_int rvk_clGetPlatformIDs(::rvk::protocol::Transport& t, cl_uint num_entries, std::uint64_t* platforms, cl_uint* num_platforms);
cl_int rvk_clGetPlatformInfo(::rvk::protocol::Transport& t, std::uint64_t platform, cl_platform_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clGetDeviceIDs(::rvk::protocol::Transport& t, std::uint64_t platform, cl_device_type device_type, cl_uint num_entries, std::uint64_t* devices, cl_uint* num_devices);
cl_int rvk_clGetDeviceInfo(::rvk::protocol::Transport& t, std::uint64_t device, cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
std::uint64_t rvk_clCreateContext(::rvk::protocol::Transport& t, const std::uint64_t* properties, cl_uint num_devices, const std::uint64_t* devices, cl_int* errcode_ret);
std::uint64_t rvk_clCreateContextFromType(::rvk::protocol::Transport& t, const std::uint64_t* properties, cl_device_type device_type, cl_int* errcode_ret);
cl_int rvk_clRetainContext(::rvk::protocol::Transport& t, std::uint64_t context);
cl_int rvk_clReleaseContext(::rvk::protocol::Transport& t, std::uint64_t context);
cl_int rvk_clGetContextInfo(::rvk::protocol::Transport& t, std::uint64_t context, cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
std::uint64_t rvk_clCreateCommandQueue(::rvk::protocol::Transport& t, std::uint64_t context, std::uint64_t device, cl_command_queue_properties properties, cl_int* errcode_ret);
std::uint64_t rvk_clCreateCommandQueueWithProperties(::rvk::protocol::Transport& t, std::uint64_t context, std::uint64_t device, const std::uint64_t* properties, cl_int* errcode_ret);
cl_int rvk_clRetainCommandQueue(::rvk::protocol::Transport& t, std::uint64_t command_queue);
cl_int rvk_clReleaseCommandQueue(::rvk::protocol::Transport& t, std::uint64_t command_queue);
cl_int rvk_clGetCommandQueueInfo(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_command_queue_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clFlush(::rvk::protocol::Transport& t, std::uint64_t command_queue);
cl_int rvk_clFinish(::rvk::protocol::Transport& t, std::uint64_t command_queue);
std::uint64_t rvk_clCreateBuffer(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret);
std::uint64_t rvk_clCreateSubBuffer(::rvk::protocol::Transport& t, std::uint64_t buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const cl_buffer_region* buffer_create_info, cl_int* errcode_ret);
std::uint64_t rvk_clCreateImage(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, std::size_t host_ptr_bytes, cl_int* errcode_ret);
cl_int rvk_clRetainMemObject(::rvk::protocol::Transport& t, std::uint64_t memobj);
cl_int rvk_clReleaseMemObject(::rvk::protocol::Transport& t, std::uint64_t memobj);
cl_int rvk_clGetMemObjectInfo(::rvk::protocol::Transport& t, std::uint64_t memobj, cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clGetImageInfo(::rvk::protocol::Transport& t, std::uint64_t image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clGetSupportedImageFormats(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries, cl_image_format* image_formats, std::size_t image_formats_bytes, cl_uint* num_image_formats);
cl_int rvk_clEnqueueReadBuffer(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t buffer, cl_bool blocking_read, size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueWriteBuffer(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t buffer, cl_bool blocking_write, size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueCopyBuffer(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t src_buffer, std::uint64_t dst_buffer, size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueFillBuffer(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueReadImage(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t image, cl_bool blocking_read, const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, std::size_t ptr_bytes, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueWriteImage(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t image, cl_bool blocking_write, const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr, std::size_t ptr_bytes, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueCopyImage(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t src_image, std::uint64_t dst_image, const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueCopyImageToBuffer(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t src_image, std::uint64_t dst_buffer, const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueCopyBufferToImage(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t src_buffer, std::uint64_t dst_image, size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
std::uint64_t rvk_clCreateProgramWithSource(::rvk::protocol::Transport& t, std::uint64_t context, cl_uint count, const char** strings, const std::size_t* lengths, cl_int* errcode_ret);
std::uint64_t rvk_clCreateProgramWithBinary(::rvk::protocol::Transport& t, std::uint64_t context, cl_uint num_devices, const std::uint64_t* device_list, const std::size_t* lengths, const unsigned char** binaries, cl_int* binary_status, cl_int* errcode_ret);
cl_int rvk_clRetainProgram(::rvk::protocol::Transport& t, std::uint64_t program);
cl_int rvk_clReleaseProgram(::rvk::protocol::Transport& t, std::uint64_t program);
cl_int rvk_clBuildProgram(::rvk::protocol::Transport& t, std::uint64_t program, cl_uint num_devices, const std::uint64_t* device_list, const char* options);
cl_int rvk_clGetProgramInfo(::rvk::protocol::Transport& t, std::uint64_t program, cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clGetProgramBuildInfo(::rvk::protocol::Transport& t, std::uint64_t program, std::uint64_t device, cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clUnloadPlatformCompiler(::rvk::protocol::Transport& t, std::uint64_t platform);
std::uint64_t rvk_clCreateKernel(::rvk::protocol::Transport& t, std::uint64_t program, const char* kernel_name, cl_int* errcode_ret);
cl_int rvk_clCreateKernelsInProgram(::rvk::protocol::Transport& t, std::uint64_t program, cl_uint num_kernels, std::uint64_t* kernels, cl_uint* num_kernels_ret);
cl_int rvk_clRetainKernel(::rvk::protocol::Transport& t, std::uint64_t kernel);
cl_int rvk_clReleaseKernel(::rvk::protocol::Transport& t, std::uint64_t kernel);
cl_int rvk_clSetKernelArg(::rvk::protocol::Transport& t, std::uint64_t kernel, cl_uint arg_index, size_t arg_size, const void* arg_value);
cl_int rvk_clGetKernelInfo(::rvk::protocol::Transport& t, std::uint64_t kernel, cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clGetKernelArgInfo(::rvk::protocol::Transport& t, std::uint64_t kernel, cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clGetKernelWorkGroupInfo(::rvk::protocol::Transport& t, std::uint64_t kernel, std::uint64_t device, cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clEnqueueNDRangeKernel(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t kernel, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueMarkerWithWaitList(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueBarrierWithWaitList(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clWaitForEvents(::rvk::protocol::Transport& t, cl_uint num_events, const std::uint64_t* event_list);
cl_int rvk_clGetEventInfo(::rvk::protocol::Transport& t, std::uint64_t event, cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
std::uint64_t rvk_clCreateUserEvent(::rvk::protocol::Transport& t, std::uint64_t context, cl_int* errcode_ret);
cl_int rvk_clSetUserEventStatus(::rvk::protocol::Transport& t, std::uint64_t event, cl_int execution_status);
cl_int rvk_clRetainEvent(::rvk::protocol::Transport& t, std::uint64_t event);
cl_int rvk_clReleaseEvent(::rvk::protocol::Transport& t, std::uint64_t event);
cl_int rvk_clGetEventProfilingInfo(::rvk::protocol::Transport& t, std::uint64_t event, cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
std::uint64_t rvk_clCreateFromGLBuffer(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_GLuint bufobj, cl_int* errcode_ret);
std::uint64_t rvk_clCreateFromGLTexture(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret);
std::uint64_t rvk_clCreateFromGLTexture2D(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret);
std::uint64_t rvk_clCreateFromGLTexture3D(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret);
std::uint64_t rvk_clCreateFromGLRenderbuffer(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int* errcode_ret);
cl_int rvk_clGetGLObjectInfo(::rvk::protocol::Transport& t, std::uint64_t memobj, cl_gl_object_type* gl_object_type, cl_GLuint* gl_object_name);
cl_int rvk_clGetGLTextureInfo(::rvk::protocol::Transport& t, std::uint64_t memobj, cl_gl_texture_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clEnqueueAcquireGLObjects(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_objects, const std::uint64_t* mem_objects, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueReleaseGLObjects(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_objects, const std::uint64_t* mem_objects, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clGetGLContextInfoKHR(::rvk::protocol::Transport& t, const std::uint64_t* properties, cl_gl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clRetainDevice(::rvk::protocol::Transport& t, std::uint64_t device);
cl_int rvk_clReleaseDevice(::rvk::protocol::Transport& t, std::uint64_t device);
cl_int rvk_clCreateSubDevices(::rvk::protocol::Transport& t, std::uint64_t in_device, const std::uint64_t* properties, cl_uint num_devices, std::uint64_t* out_devices, cl_uint* num_devices_ret);
cl_int rvk_clEnqueueTask(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t kernel, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueMarker(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t* event);
cl_int rvk_clEnqueueBarrier(::rvk::protocol::Transport& t, std::uint64_t command_queue);
cl_int rvk_clEnqueueWaitForEvents(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_events, const std::uint64_t* event_list);
cl_int rvk_clEnqueueMigrateMemObjects(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_uint num_mem_objects, const std::uint64_t* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueReadBufferRect(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t buffer, cl_bool blocking_read, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr, std::size_t ptr_bytes, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueWriteBufferRect(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t buffer, cl_bool blocking_write, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr, std::size_t ptr_bytes, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueCopyBufferRect(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t src_buffer, std::uint64_t dst_buffer, const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch, size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
cl_int rvk_clEnqueueFillImage(::rvk::protocol::Transport& t, std::uint64_t command_queue, std::uint64_t image, const void* fill_color, std::size_t fill_color_bytes, const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const std::uint64_t* event_wait_list, std::uint64_t* event);
std::uint64_t rvk_clCreateImage2D(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, std::size_t host_ptr_bytes, cl_int* errcode_ret);
std::uint64_t rvk_clCreateImage3D(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch, void* host_ptr, std::size_t host_ptr_bytes, cl_int* errcode_ret);
cl_int rvk_clUnloadCompiler(::rvk::protocol::Transport& t);
cl_int rvk_clSetCommandQueueProperty(::rvk::protocol::Transport& t, std::uint64_t command_queue, cl_command_queue_properties properties, cl_bool enable, cl_command_queue_properties* old_properties);
std::uint64_t rvk_clCreateProgramWithIL(::rvk::protocol::Transport& t, std::uint64_t context, const void* il, size_t length, cl_int* errcode_ret);
cl_int rvk_clCompileProgram(::rvk::protocol::Transport& t, std::uint64_t program, cl_uint num_devices, const std::uint64_t* device_list, const char* options, cl_uint num_input_headers, const std::uint64_t* input_headers, const char** header_include_names);
std::uint64_t rvk_clLinkProgram(::rvk::protocol::Transport& t, std::uint64_t context, cl_uint num_devices, const std::uint64_t* device_list, const char* options, cl_uint num_input_programs, const std::uint64_t* input_programs, cl_int* errcode_ret);
cl_int rvk_clSetProgramSpecializationConstant(::rvk::protocol::Transport& t, std::uint64_t program, cl_uint spec_id, size_t spec_size, const void* spec_value);
std::uint64_t rvk_clCloneKernel(::rvk::protocol::Transport& t, std::uint64_t source_kernel, cl_int* errcode_ret);
cl_int rvk_clSetKernelExecInfo(::rvk::protocol::Transport& t, std::uint64_t kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value);
cl_int rvk_clGetKernelSubGroupInfo(::rvk::protocol::Transport& t, std::uint64_t kernel, std::uint64_t device, cl_kernel_sub_group_info param_name, size_t input_value_size, const void* input_value, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int rvk_clGetHostTimer(::rvk::protocol::Transport& t, std::uint64_t device, cl_ulong* host_timestamp);
cl_int rvk_clGetDeviceAndHostTimer(::rvk::protocol::Transport& t, std::uint64_t device, cl_ulong* device_timestamp, cl_ulong* host_timestamp);
std::uint64_t rvk_clCreateBufferWithProperties(::rvk::protocol::Transport& t, std::uint64_t context, const std::uint64_t* properties, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret);
std::uint64_t rvk_clCreateImageWithProperties(::rvk::protocol::Transport& t, std::uint64_t context, const std::uint64_t* properties, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, std::size_t host_ptr_bytes, cl_int* errcode_ret);
std::uint64_t rvk_clCreateSamplerWithProperties(::rvk::protocol::Transport& t, std::uint64_t context, const std::uint64_t* sampler_properties, cl_int* errcode_ret);
cl_int rvk_clSetDefaultDeviceCommandQueue(::rvk::protocol::Transport& t, std::uint64_t context, std::uint64_t device, std::uint64_t command_queue);
std::uint64_t rvk_clCreateSampler(::rvk::protocol::Transport& t, std::uint64_t context, cl_bool normalized_coords, cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret);
cl_int rvk_clRetainSampler(::rvk::protocol::Transport& t, std::uint64_t sampler);
cl_int rvk_clReleaseSampler(::rvk::protocol::Transport& t, std::uint64_t sampler);
cl_int rvk_clGetSamplerInfo(::rvk::protocol::Transport& t, std::uint64_t sampler, cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
std::uint64_t rvk_clCreatePipe(::rvk::protocol::Transport& t, std::uint64_t context, cl_mem_flags flags, cl_uint pipe_packet_size, cl_uint pipe_max_packets, const std::uint64_t* properties, cl_int* errcode_ret);
cl_int rvk_clGetPipeInfo(::rvk::protocol::Transport& t, std::uint64_t pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
std::uint64_t rvk_clCreateProgramWithBuiltInKernels(::rvk::protocol::Transport& t, std::uint64_t context, cl_uint num_devices, const std::uint64_t* device_list, const char* kernel_names, cl_int* errcode_ret);

}  // namespace av::clproto
