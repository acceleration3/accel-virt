// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opencl --xml cl.xml --out ...

#pragma once

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

namespace av::clproto {

// Every command forwarded to whatever OpenCL this process
// loaded. Which numbers are its objects is not this class's to
// say: the server that derives from it answers handle_for.
//
// An extension command is resolved rather than linked. The ICD
// loader on a machine is whichever one is first on the library
// path, and they do not all export the same extension symbols -
// linking clGetGLContextInfoKHR made the whole executor library
// unloadable on a host whose loader had no GL sharing (042).
class RealDispatch : public ServerDispatch {
public:
    cl_int on_clGetPlatformIDs(cl_uint num_entries, cl_platform_id* platforms, cl_uint* num_platforms) override {
        return ::clGetPlatformIDs(num_entries, platforms, num_platforms);
    }
    cl_int on_clGetPlatformInfo(cl_platform_id platform, cl_platform_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetPlatformInfo(platform, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries, cl_device_id* devices, cl_uint* num_devices) override {
        return ::clGetDeviceIDs(platform, device_type, num_entries, devices, num_devices);
    }
    cl_int on_clGetDeviceInfo(cl_device_id device, cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetDeviceInfo(device, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_context on_clCreateContext(const cl_context_properties* properties, cl_uint num_devices, const cl_device_id* devices, cl_int* errcode_ret) override {
        return ::clCreateContext(properties, num_devices, devices, nullptr, nullptr, errcode_ret);
    }
    cl_context on_clCreateContextFromType(const cl_context_properties* properties, cl_device_type device_type, cl_int* errcode_ret) override {
        return ::clCreateContextFromType(properties, device_type, nullptr, nullptr, errcode_ret);
    }
    cl_int on_clRetainContext(cl_context context) override {
        return ::clRetainContext(context);
    }
    cl_int on_clReleaseContext(cl_context context) override {
        return ::clReleaseContext(context);
    }
    cl_int on_clGetContextInfo(cl_context context, cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetContextInfo(context, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_command_queue on_clCreateCommandQueue(cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int* errcode_ret) override {
        return ::clCreateCommandQueue(context, device, properties, errcode_ret);
    }
    cl_command_queue on_clCreateCommandQueueWithProperties(cl_context context, cl_device_id device, const cl_queue_properties* properties, cl_int* errcode_ret) override {
        return ::clCreateCommandQueueWithProperties(context, device, properties, errcode_ret);
    }
    cl_int on_clRetainCommandQueue(cl_command_queue command_queue) override {
        return ::clRetainCommandQueue(command_queue);
    }
    cl_int on_clReleaseCommandQueue(cl_command_queue command_queue) override {
        return ::clReleaseCommandQueue(command_queue);
    }
    cl_int on_clGetCommandQueueInfo(cl_command_queue command_queue, cl_command_queue_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetCommandQueueInfo(command_queue, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clFlush(cl_command_queue command_queue) override {
        return ::clFlush(command_queue);
    }
    cl_int on_clFinish(cl_command_queue command_queue) override {
        return ::clFinish(command_queue);
    }
    cl_mem on_clCreateBuffer(cl_context context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) override {
        return ::clCreateBuffer(context, flags, size, host_ptr, errcode_ret);
    }
    cl_mem on_clCreateSubBuffer(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const cl_buffer_region* buffer_create_info, cl_int* errcode_ret) override {
        return ::clCreateSubBuffer(buffer, flags, buffer_create_type, buffer_create_info, errcode_ret);
    }
    cl_mem on_clCreateImage(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, cl_int* errcode_ret) override {
        return ::clCreateImage(context, flags, image_format, image_desc, host_ptr, errcode_ret);
    }
    cl_int on_clRetainMemObject(cl_mem memobj) override {
        return ::clRetainMemObject(memobj);
    }
    cl_int on_clReleaseMemObject(cl_mem memobj) override {
        return ::clReleaseMemObject(memobj);
    }
    cl_int on_clGetMemObjectInfo(cl_mem memobj, cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetMemObjectInfo(memobj, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clGetImageInfo(cl_mem image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetImageInfo(image, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clGetSupportedImageFormats(cl_context context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries, cl_image_format* image_formats, cl_uint* num_image_formats) override {
        return ::clGetSupportedImageFormats(context, flags, image_type, num_entries, image_formats, num_image_formats);
    }
    cl_int on_clEnqueueReadBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueReadBuffer(command_queue, buffer, blocking_read, offset, size, ptr, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueWriteBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueWriteBuffer(command_queue, buffer, blocking_write, offset, size, ptr, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueCopyBuffer(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueCopyBuffer(command_queue, src_buffer, dst_buffer, src_offset, dst_offset, size, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueFillBuffer(cl_command_queue command_queue, cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueFillBuffer(command_queue, buffer, pattern, pattern_size, offset, size, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueReadImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read, const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueReadImage(command_queue, image, blocking_read, origin, region, row_pitch, slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueWriteImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_write, const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueWriteImage(command_queue, image, blocking_write, origin, region, input_row_pitch, input_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueCopyImage(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_image, const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueCopyImage(command_queue, src_image, dst_image, src_origin, dst_origin, region, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueCopyImageToBuffer(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_buffer, const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueCopyImageToBuffer(command_queue, src_image, dst_buffer, src_origin, region, dst_offset, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueCopyBufferToImage(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_image, size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueCopyBufferToImage(command_queue, src_buffer, dst_image, src_offset, dst_origin, region, num_events_in_wait_list, event_wait_list, event);
    }
    cl_program on_clCreateProgramWithSource(cl_context context, cl_uint count, const char** strings, const size_t* lengths, cl_int* errcode_ret) override {
        return ::clCreateProgramWithSource(context, count, strings, lengths, errcode_ret);
    }
    cl_program on_clCreateProgramWithBinary(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const size_t* lengths, const unsigned char** binaries, cl_int* binary_status, cl_int* errcode_ret) override {
        return ::clCreateProgramWithBinary(context, num_devices, device_list, lengths, binaries, binary_status, errcode_ret);
    }
    cl_int on_clRetainProgram(cl_program program) override {
        return ::clRetainProgram(program);
    }
    cl_int on_clReleaseProgram(cl_program program) override {
        return ::clReleaseProgram(program);
    }
    cl_int on_clBuildProgram(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options) override {
        return ::clBuildProgram(program, num_devices, device_list, options, nullptr, nullptr);
    }
    cl_int on_clGetProgramInfo(cl_program program, cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetProgramInfo(program, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clGetProgramBuildInfo(cl_program program, cl_device_id device, cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetProgramBuildInfo(program, device, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clUnloadPlatformCompiler(cl_platform_id platform) override {
        return ::clUnloadPlatformCompiler(platform);
    }
    cl_kernel on_clCreateKernel(cl_program program, const char* kernel_name, cl_int* errcode_ret) override {
        return ::clCreateKernel(program, kernel_name, errcode_ret);
    }
    cl_int on_clCreateKernelsInProgram(cl_program program, cl_uint num_kernels, cl_kernel* kernels, cl_uint* num_kernels_ret) override {
        return ::clCreateKernelsInProgram(program, num_kernels, kernels, num_kernels_ret);
    }
    cl_int on_clRetainKernel(cl_kernel kernel) override {
        return ::clRetainKernel(kernel);
    }
    cl_int on_clReleaseKernel(cl_kernel kernel) override {
        return ::clReleaseKernel(kernel);
    }
    cl_int on_clSetKernelArg(cl_kernel kernel, cl_uint arg_index, size_t arg_size, const void* arg_value) override {
        return ::clSetKernelArg(kernel, arg_index, arg_size, arg_value);
    }
    cl_int on_clGetKernelInfo(cl_kernel kernel, cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetKernelInfo(kernel, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clGetKernelArgInfo(cl_kernel kernel, cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetKernelArgInfo(kernel, arg_index, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clGetKernelWorkGroupInfo(cl_kernel kernel, cl_device_id device, cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetKernelWorkGroupInfo(kernel, device, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clEnqueueNDRangeKernel(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueNDRangeKernel(command_queue, kernel, work_dim, global_work_offset, global_work_size, local_work_size, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueMarkerWithWaitList(cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueMarkerWithWaitList(command_queue, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueBarrierWithWaitList(cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueBarrierWithWaitList(command_queue, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clWaitForEvents(cl_uint num_events, const cl_event* event_list) override {
        return ::clWaitForEvents(num_events, event_list);
    }
    cl_int on_clGetEventInfo(cl_event event, cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetEventInfo(event, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_event on_clCreateUserEvent(cl_context context, cl_int* errcode_ret) override {
        return ::clCreateUserEvent(context, errcode_ret);
    }
    cl_int on_clSetUserEventStatus(cl_event event, cl_int execution_status) override {
        return ::clSetUserEventStatus(event, execution_status);
    }
    cl_int on_clRetainEvent(cl_event event) override {
        return ::clRetainEvent(event);
    }
    cl_int on_clReleaseEvent(cl_event event) override {
        return ::clReleaseEvent(event);
    }
    cl_int on_clGetEventProfilingInfo(cl_event event, cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetEventProfilingInfo(event, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_mem on_clCreateFromGLBuffer(cl_context context, cl_mem_flags flags, cl_GLuint bufobj, cl_int* errcode_ret) override {
        return ::clCreateFromGLBuffer(context, flags, bufobj, errcode_ret);
    }
    cl_mem on_clCreateFromGLTexture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) override {
        return ::clCreateFromGLTexture(context, flags, target, miplevel, texture, errcode_ret);
    }
    cl_mem on_clCreateFromGLTexture2D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) override {
        return ::clCreateFromGLTexture2D(context, flags, target, miplevel, texture, errcode_ret);
    }
    cl_mem on_clCreateFromGLTexture3D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int* errcode_ret) override {
        return ::clCreateFromGLTexture3D(context, flags, target, miplevel, texture, errcode_ret);
    }
    cl_mem on_clCreateFromGLRenderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int* errcode_ret) override {
        return ::clCreateFromGLRenderbuffer(context, flags, renderbuffer, errcode_ret);
    }
    cl_int on_clGetGLObjectInfo(cl_mem memobj, cl_gl_object_type* gl_object_type, cl_GLuint* gl_object_name) override {
        return ::clGetGLObjectInfo(memobj, gl_object_type, gl_object_name);
    }
    cl_int on_clGetGLTextureInfo(cl_mem memobj, cl_gl_texture_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetGLTextureInfo(memobj, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clEnqueueAcquireGLObjects(cl_command_queue command_queue, cl_uint num_objects, const cl_mem* mem_objects, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueAcquireGLObjects(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueReleaseGLObjects(cl_command_queue command_queue, cl_uint num_objects, const cl_mem* mem_objects, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueReleaseGLObjects(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);
    }
    using clGetGLContextInfoKHR_fn = decltype(&::clGetGLContextInfoKHR);
    cl_int on_clGetGLContextInfoKHR(const cl_context_properties* properties, cl_gl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        if (pfn_clGetGLContextInfoKHR_ == nullptr) {
            pfn_clGetGLContextInfoKHR_ = reinterpret_cast<clGetGLContextInfoKHR_fn>(
                ::clGetExtensionFunctionAddress("clGetGLContextInfoKHR"));
        }
        if (pfn_clGetGLContextInfoKHR_ == nullptr) {
            return CL_INVALID_OPERATION;
        }
        return pfn_clGetGLContextInfoKHR_(properties, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clRetainDevice(cl_device_id device) override {
        return ::clRetainDevice(device);
    }
    cl_int on_clReleaseDevice(cl_device_id device) override {
        return ::clReleaseDevice(device);
    }
    cl_int on_clCreateSubDevices(cl_device_id in_device, const cl_device_partition_property* properties, cl_uint num_devices, cl_device_id* out_devices, cl_uint* num_devices_ret) override {
        return ::clCreateSubDevices(in_device, properties, num_devices, out_devices, num_devices_ret);
    }
    cl_int on_clEnqueueTask(cl_command_queue command_queue, cl_kernel kernel, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueTask(command_queue, kernel, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueMarker(cl_command_queue command_queue, cl_event* event) override {
        return ::clEnqueueMarker(command_queue, event);
    }
    cl_int on_clEnqueueBarrier(cl_command_queue command_queue) override {
        return ::clEnqueueBarrier(command_queue);
    }
    cl_int on_clEnqueueWaitForEvents(cl_command_queue command_queue, cl_uint num_events, const cl_event* event_list) override {
        return ::clEnqueueWaitForEvents(command_queue, num_events, event_list);
    }
    cl_int on_clEnqueueMigrateMemObjects(cl_command_queue command_queue, cl_uint num_mem_objects, const cl_mem* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueMigrateMemObjects(command_queue, num_mem_objects, mem_objects, flags, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueReadBufferRect(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueReadBufferRect(command_queue, buffer, blocking_read, buffer_origin, host_origin, region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueWriteBufferRect(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueWriteBufferRect(command_queue, buffer, blocking_write, buffer_origin, host_origin, region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueCopyBufferRect(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer, const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch, size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueCopyBufferRect(command_queue, src_buffer, dst_buffer, src_origin, dst_origin, region, src_row_pitch, src_slice_pitch, dst_row_pitch, dst_slice_pitch, num_events_in_wait_list, event_wait_list, event);
    }
    cl_int on_clEnqueueFillImage(cl_command_queue command_queue, cl_mem image, const void* fill_color, const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) override {
        return ::clEnqueueFillImage(command_queue, image, fill_color, origin, region, num_events_in_wait_list, event_wait_list, event);
    }
    cl_mem on_clCreateImage2D(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, cl_int* errcode_ret) override {
        return ::clCreateImage2D(context, flags, image_format, image_width, image_height, image_row_pitch, host_ptr, errcode_ret);
    }
    cl_mem on_clCreateImage3D(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch, void* host_ptr, cl_int* errcode_ret) override {
        return ::clCreateImage3D(context, flags, image_format, image_width, image_height, image_depth, image_row_pitch, image_slice_pitch, host_ptr, errcode_ret);
    }
    cl_int on_clUnloadCompiler() override {
        return ::clUnloadCompiler();
    }
    cl_int on_clSetCommandQueueProperty(cl_command_queue command_queue, cl_command_queue_properties properties, cl_bool enable, cl_command_queue_properties* old_properties) override {
        return ::clSetCommandQueueProperty(command_queue, properties, enable, old_properties);
    }
    cl_program on_clCreateProgramWithIL(cl_context context, const void* il, size_t length, cl_int* errcode_ret) override {
        return ::clCreateProgramWithIL(context, il, length, errcode_ret);
    }
    cl_int on_clCompileProgram(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_headers, const cl_program* input_headers, const char** header_include_names) override {
        return ::clCompileProgram(program, num_devices, device_list, options, num_input_headers, input_headers, header_include_names, nullptr, nullptr);
    }
    cl_program on_clLinkProgram(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_programs, const cl_program* input_programs, cl_int* errcode_ret) override {
        return ::clLinkProgram(context, num_devices, device_list, options, num_input_programs, input_programs, nullptr, nullptr, errcode_ret);
    }
    cl_int on_clSetProgramSpecializationConstant(cl_program program, cl_uint spec_id, size_t spec_size, const void* spec_value) override {
        return ::clSetProgramSpecializationConstant(program, spec_id, spec_size, spec_value);
    }
    cl_kernel on_clCloneKernel(cl_kernel source_kernel, cl_int* errcode_ret) override {
        return ::clCloneKernel(source_kernel, errcode_ret);
    }
    cl_int on_clSetKernelExecInfo(cl_kernel kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value) override {
        return ::clSetKernelExecInfo(kernel, param_name, param_value_size, param_value);
    }
    cl_int on_clGetKernelSubGroupInfo(cl_kernel kernel, cl_device_id device, cl_kernel_sub_group_info param_name, size_t input_value_size, const void* input_value, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetKernelSubGroupInfo(kernel, device, param_name, input_value_size, input_value, param_value_size, param_value, param_value_size_ret);
    }
    cl_int on_clGetHostTimer(cl_device_id device, cl_ulong* host_timestamp) override {
        return ::clGetHostTimer(device, host_timestamp);
    }
    cl_int on_clGetDeviceAndHostTimer(cl_device_id device, cl_ulong* device_timestamp, cl_ulong* host_timestamp) override {
        return ::clGetDeviceAndHostTimer(device, device_timestamp, host_timestamp);
    }
    cl_mem on_clCreateBufferWithProperties(cl_context context, const cl_mem_properties* properties, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) override {
        return ::clCreateBufferWithProperties(context, properties, flags, size, host_ptr, errcode_ret);
    }
    cl_mem on_clCreateImageWithProperties(cl_context context, const cl_mem_properties* properties, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, cl_int* errcode_ret) override {
        return ::clCreateImageWithProperties(context, properties, flags, image_format, image_desc, host_ptr, errcode_ret);
    }
    cl_sampler on_clCreateSamplerWithProperties(cl_context context, const cl_sampler_properties* sampler_properties, cl_int* errcode_ret) override {
        return ::clCreateSamplerWithProperties(context, sampler_properties, errcode_ret);
    }
    cl_int on_clSetDefaultDeviceCommandQueue(cl_context context, cl_device_id device, cl_command_queue command_queue) override {
        return ::clSetDefaultDeviceCommandQueue(context, device, command_queue);
    }
    cl_sampler on_clCreateSampler(cl_context context, cl_bool normalized_coords, cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret) override {
        return ::clCreateSampler(context, normalized_coords, addressing_mode, filter_mode, errcode_ret);
    }
    cl_int on_clRetainSampler(cl_sampler sampler) override {
        return ::clRetainSampler(sampler);
    }
    cl_int on_clReleaseSampler(cl_sampler sampler) override {
        return ::clReleaseSampler(sampler);
    }
    cl_int on_clGetSamplerInfo(cl_sampler sampler, cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetSamplerInfo(sampler, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_mem on_clCreatePipe(cl_context context, cl_mem_flags flags, cl_uint pipe_packet_size, cl_uint pipe_max_packets, const cl_pipe_properties* properties, cl_int* errcode_ret) override {
        return ::clCreatePipe(context, flags, pipe_packet_size, pipe_max_packets, properties, errcode_ret);
    }
    cl_int on_clGetPipeInfo(cl_mem pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override {
        return ::clGetPipeInfo(pipe, param_name, param_value_size, param_value, param_value_size_ret);
    }
    cl_program on_clCreateProgramWithBuiltInKernels(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* kernel_names, cl_int* errcode_ret) override {
        return ::clCreateProgramWithBuiltInKernels(context, num_devices, device_list, kernel_names, errcode_ret);
    }

private:
    clGetGLContextInfoKHR_fn pfn_clGetGLContextInfoKHR_ = nullptr;
};

}  // namespace av::clproto
