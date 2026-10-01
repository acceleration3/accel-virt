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

#include "proto/client_transport.hpp"

namespace av::clproto {

cl_int av_clGetPlatformIDs(::av::ClientTransport& t, cl_uint num_entries, uint64_t* platforms, cl_uint* num_platforms);
cl_int av_clGetPlatformInfo(::av::ClientTransport& t, uint64_t platform, cl_platform_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clGetDeviceIDs(::av::ClientTransport& t, uint64_t platform, cl_device_type device_type, cl_uint num_entries, uint64_t* devices, cl_uint* num_devices);
cl_int av_clGetDeviceInfo(::av::ClientTransport& t, uint64_t device, cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
uint64_t av_clCreateContext(::av::ClientTransport& t, const uint64_t* properties, cl_uint num_devices, const uint64_t* devices, cl_int* errcode_ret);
uint64_t av_clCreateContextFromType(::av::ClientTransport& t, const uint64_t* properties, cl_device_type device_type, cl_int* errcode_ret);
cl_int av_clRetainContext(::av::ClientTransport& t, uint64_t context);
cl_int av_clReleaseContext(::av::ClientTransport& t, uint64_t context);
cl_int av_clGetContextInfo(::av::ClientTransport& t, uint64_t context, cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
uint64_t av_clCreateCommandQueue(::av::ClientTransport& t, uint64_t context, uint64_t device, cl_command_queue_properties properties, cl_int* errcode_ret);
uint64_t av_clCreateCommandQueueWithProperties(::av::ClientTransport& t, uint64_t context, uint64_t device, const uint64_t* properties, cl_int* errcode_ret);
cl_int av_clRetainCommandQueue(::av::ClientTransport& t, uint64_t command_queue);
cl_int av_clReleaseCommandQueue(::av::ClientTransport& t, uint64_t command_queue);
cl_int av_clGetCommandQueueInfo(::av::ClientTransport& t, uint64_t command_queue, cl_command_queue_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clFlush(::av::ClientTransport& t, uint64_t command_queue);
cl_int av_clFinish(::av::ClientTransport& t, uint64_t command_queue);
uint64_t av_clCreateBuffer(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret);
uint64_t av_clCreateSubBuffer(::av::ClientTransport& t, uint64_t buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const cl_buffer_region* buffer_create_info, cl_int* errcode_ret);
uint64_t av_clCreateImage(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret);
cl_int av_clRetainMemObject(::av::ClientTransport& t, uint64_t memobj);
cl_int av_clReleaseMemObject(::av::ClientTransport& t, uint64_t memobj);
cl_int av_clGetMemObjectInfo(::av::ClientTransport& t, uint64_t memobj, cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clGetImageInfo(::av::ClientTransport& t, uint64_t image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clGetSupportedImageFormats(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries, cl_image_format* image_formats, size_t image_formats_bytes, cl_uint* num_image_formats);
cl_int av_clEnqueueReadBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_read, size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueWriteBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_write, size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueCopyBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_buffer, uint64_t dst_buffer, size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueFillBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueReadImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t image, cl_bool blocking_read, const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueWriteImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t image, cl_bool blocking_write, const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueCopyImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_image, uint64_t dst_image, const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueCopyImageToBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_image, uint64_t dst_buffer, const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueCopyBufferToImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_buffer, uint64_t dst_image, size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
uint64_t av_clCreateProgramWithSource(::av::ClientTransport& t, uint64_t context, cl_uint count, const char** strings, const size_t* lengths, cl_int* errcode_ret);
uint64_t av_clCreateProgramWithBinary(::av::ClientTransport& t, uint64_t context, cl_uint num_devices, const uint64_t* device_list, const size_t* lengths, const unsigned char** binaries, cl_int* binary_status, cl_int* errcode_ret);
cl_int av_clRetainProgram(::av::ClientTransport& t, uint64_t program);
cl_int av_clReleaseProgram(::av::ClientTransport& t, uint64_t program);
cl_int av_clBuildProgram(::av::ClientTransport& t, uint64_t program, cl_uint num_devices, const uint64_t* device_list, const char* options);
cl_int av_clGetProgramInfo(::av::ClientTransport& t, uint64_t program, cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clGetProgramBuildInfo(::av::ClientTransport& t, uint64_t program, uint64_t device, cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clUnloadPlatformCompiler(::av::ClientTransport& t, uint64_t platform);
uint64_t av_clCreateKernel(::av::ClientTransport& t, uint64_t program, const char* kernel_name, cl_int* errcode_ret);
cl_int av_clCreateKernelsInProgram(::av::ClientTransport& t, uint64_t program, cl_uint num_kernels, uint64_t* kernels, cl_uint* num_kernels_ret);
cl_int av_clRetainKernel(::av::ClientTransport& t, uint64_t kernel);
cl_int av_clReleaseKernel(::av::ClientTransport& t, uint64_t kernel);
cl_int av_clSetKernelArg(::av::ClientTransport& t, uint64_t kernel, cl_uint arg_index, size_t arg_size, const void* arg_value);
cl_int av_clGetKernelInfo(::av::ClientTransport& t, uint64_t kernel, cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clGetKernelArgInfo(::av::ClientTransport& t, uint64_t kernel, cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clGetKernelWorkGroupInfo(::av::ClientTransport& t, uint64_t kernel, uint64_t device, cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clEnqueueNDRangeKernel(::av::ClientTransport& t, uint64_t command_queue, uint64_t kernel, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueMarkerWithWaitList(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueBarrierWithWaitList(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clWaitForEvents(::av::ClientTransport& t, cl_uint num_events, const uint64_t* event_list);
cl_int av_clGetEventInfo(::av::ClientTransport& t, uint64_t event, cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
uint64_t av_clCreateUserEvent(::av::ClientTransport& t, uint64_t context, cl_int* errcode_ret);
cl_int av_clSetUserEventStatus(::av::ClientTransport& t, uint64_t event, cl_int execution_status);
cl_int av_clRetainEvent(::av::ClientTransport& t, uint64_t event);
cl_int av_clReleaseEvent(::av::ClientTransport& t, uint64_t event);
cl_int av_clGetEventProfilingInfo(::av::ClientTransport& t, uint64_t event, cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
uint64_t av_clCreateFromGLBuffer(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLuint bufobj, cl_int* errcode_ret);
uint64_t av_clCreateFromGLTexture(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret);
uint64_t av_clCreateFromGLTexture2D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret);
uint64_t av_clCreateFromGLTexture3D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret);
uint64_t av_clCreateFromGLRenderbuffer(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int* errcode_ret);
cl_int av_clGetGLObjectInfo(::av::ClientTransport& t, uint64_t memobj, cl_gl_object_type* gl_object_type, cl_GLuint* gl_object_name);
cl_int av_clGetGLTextureInfo(::av::ClientTransport& t, uint64_t memobj, cl_gl_texture_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clEnqueueAcquireGLObjects(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_objects, const uint64_t* mem_objects, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueReleaseGLObjects(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_objects, const uint64_t* mem_objects, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clGetGLContextInfoKHR(::av::ClientTransport& t, const uint64_t* properties, cl_gl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clRetainDevice(::av::ClientTransport& t, uint64_t device);
cl_int av_clReleaseDevice(::av::ClientTransport& t, uint64_t device);
cl_int av_clCreateSubDevices(::av::ClientTransport& t, uint64_t in_device, const uint64_t* properties, cl_uint num_devices, uint64_t* out_devices, cl_uint* num_devices_ret);
cl_int av_clEnqueueTask(::av::ClientTransport& t, uint64_t command_queue, uint64_t kernel, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueMarker(::av::ClientTransport& t, uint64_t command_queue, uint64_t* event);
cl_int av_clEnqueueBarrier(::av::ClientTransport& t, uint64_t command_queue);
cl_int av_clEnqueueWaitForEvents(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_events, const uint64_t* event_list);
cl_int av_clEnqueueMigrateMemObjects(::av::ClientTransport& t, uint64_t command_queue, cl_uint num_mem_objects, const uint64_t* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueReadBufferRect(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_read, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueWriteBufferRect(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_write, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr, size_t ptr_bytes, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueCopyBufferRect(::av::ClientTransport& t, uint64_t command_queue, uint64_t src_buffer, uint64_t dst_buffer, const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch, size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueFillImage(::av::ClientTransport& t, uint64_t command_queue, uint64_t image, const void* fill_color, size_t fill_color_bytes, const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
uint64_t av_clCreateImage2D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret);
uint64_t av_clCreateImage3D(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret);
cl_int av_clUnloadCompiler(::av::ClientTransport& t);
cl_int av_clSetCommandQueueProperty(::av::ClientTransport& t, uint64_t command_queue, cl_command_queue_properties properties, cl_bool enable, cl_command_queue_properties* old_properties);
uint64_t av_clCreateProgramWithIL(::av::ClientTransport& t, uint64_t context, const void* il, size_t length, cl_int* errcode_ret);
cl_int av_clCompileProgram(::av::ClientTransport& t, uint64_t program, cl_uint num_devices, const uint64_t* device_list, const char* options, cl_uint num_input_headers, const uint64_t* input_headers, const char** header_include_names);
uint64_t av_clLinkProgram(::av::ClientTransport& t, uint64_t context, cl_uint num_devices, const uint64_t* device_list, const char* options, cl_uint num_input_programs, const uint64_t* input_programs, cl_int* errcode_ret);
cl_int av_clSetProgramSpecializationConstant(::av::ClientTransport& t, uint64_t program, cl_uint spec_id, size_t spec_size, const void* spec_value);
uint64_t av_clCloneKernel(::av::ClientTransport& t, uint64_t source_kernel, cl_int* errcode_ret);
cl_int av_clSetKernelExecInfo(::av::ClientTransport& t, uint64_t kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value);
cl_int av_clGetKernelSubGroupInfo(::av::ClientTransport& t, uint64_t kernel, uint64_t device, cl_kernel_sub_group_info param_name, size_t input_value_size, const void* input_value, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
cl_int av_clGetHostTimer(::av::ClientTransport& t, uint64_t device, cl_ulong* host_timestamp);
cl_int av_clGetDeviceAndHostTimer(::av::ClientTransport& t, uint64_t device, cl_ulong* device_timestamp, cl_ulong* host_timestamp);
uint64_t av_clCreateBufferWithProperties(::av::ClientTransport& t, uint64_t context, const uint64_t* properties, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret);
uint64_t av_clCreateImageWithProperties(::av::ClientTransport& t, uint64_t context, const uint64_t* properties, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, size_t host_ptr_bytes, cl_int* errcode_ret);
uint64_t av_clCreateSamplerWithProperties(::av::ClientTransport& t, uint64_t context, const uint64_t* sampler_properties, cl_int* errcode_ret);
cl_int av_clSetDefaultDeviceCommandQueue(::av::ClientTransport& t, uint64_t context, uint64_t device, uint64_t command_queue);
uint64_t av_clCreateSampler(::av::ClientTransport& t, uint64_t context, cl_bool normalized_coords, cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret);
cl_int av_clRetainSampler(::av::ClientTransport& t, uint64_t sampler);
cl_int av_clReleaseSampler(::av::ClientTransport& t, uint64_t sampler);
cl_int av_clGetSamplerInfo(::av::ClientTransport& t, uint64_t sampler, cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
uint64_t av_clCreatePipe(::av::ClientTransport& t, uint64_t context, cl_mem_flags flags, cl_uint pipe_packet_size, cl_uint pipe_max_packets, const uint64_t* properties, cl_int* errcode_ret);
cl_int av_clGetPipeInfo(::av::ClientTransport& t, uint64_t pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
uint64_t av_clCreateProgramWithBuiltInKernels(::av::ClientTransport& t, uint64_t context, cl_uint num_devices, const uint64_t* device_list, const char* kernel_names, cl_int* errcode_ret);
uint64_t av_clEnqueueMapBuffer(::av::ClientTransport& t, uint64_t command_queue, uint64_t buffer, cl_bool blocking_map, cl_map_flags map_flags, size_t offset, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event, cl_int* errcode_ret);
cl_int av_clEnqueueUnmapMemObject(::av::ClientTransport& t, uint64_t command_queue, uint64_t memobj, uint64_t mapped_ptr, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
uint64_t av_clSVMAlloc(::av::ClientTransport& t, uint64_t context, cl_svm_mem_flags flags, size_t size, cl_uint alignment);
void av_clSVMFree(::av::ClientTransport& t, uint64_t context, uint64_t svm_pointer);
cl_int av_clEnqueueSVMMap(::av::ClientTransport& t, uint64_t command_queue, cl_bool blocking_map, cl_map_flags flags, uint64_t svm_ptr, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueSVMUnmap(::av::ClientTransport& t, uint64_t command_queue, uint64_t svm_ptr, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueSVMMemcpy(::av::ClientTransport& t, uint64_t command_queue, cl_bool blocking_copy, uint64_t dst_ptr, uint64_t src_ptr, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clEnqueueSVMMemFill(::av::ClientTransport& t, uint64_t command_queue, uint64_t svm_ptr, const void* pattern, size_t pattern_size, size_t size, cl_uint num_events_in_wait_list, const uint64_t* event_wait_list, uint64_t* event);
cl_int av_clSetKernelArgSVMPointer(::av::ClientTransport& t, uint64_t kernel, cl_uint arg_index, uint64_t arg_value);
cl_int av_clSetEventCallback(::av::ClientTransport& t, uint64_t event, cl_int command_exec_callback_type);

}  // namespace av::clproto
