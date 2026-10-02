// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.opencl --xml cl.xml --out ...

#pragma once

#include <type_traits>

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
#include <CL/cl_icd.h>

#include "proto/client/icd_stub_report.hpp"

namespace av::clproto {

// Every one of these answers CL_INVALID_OPERATION, and says so
// once: an application that asks for something this driver does
// not carry gets a clean refusal, and whoever is reading the log
// gets the name of what it asked for. See
// proto/client/icd_stub_report.hpp.
namespace stubs {

inline cl_int CL_API_CALL clGetPlatformIDs(cl_uint /*num_entries*/, cl_platform_id* /*platforms*/, cl_uint* /*num_platforms*/) {
    ::av::icd::stub_called("cl", "clGetPlatformIDs", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetPlatformInfo(cl_platform_id /*platform*/, cl_platform_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetPlatformInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetDeviceIDs(cl_platform_id /*platform*/, cl_device_type /*device_type*/, cl_uint /*num_entries*/, cl_device_id* /*devices*/, cl_uint* /*num_devices*/) {
    ::av::icd::stub_called("cl", "clGetDeviceIDs", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetDeviceInfo(cl_device_id /*device*/, cl_device_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetDeviceInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clCreateSubDevices(cl_device_id /*in_device*/, const cl_device_partition_property* /*properties*/, cl_uint /*num_devices*/, cl_device_id* /*out_devices*/, cl_uint* /*num_devices_ret*/) {
    ::av::icd::stub_called("cl", "clCreateSubDevices", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clRetainDevice(cl_device_id /*device*/) {
    ::av::icd::stub_called("cl", "clRetainDevice", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clReleaseDevice(cl_device_id /*device*/) {
    ::av::icd::stub_called("cl", "clReleaseDevice", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clSetDefaultDeviceCommandQueue(cl_context /*context*/, cl_device_id /*device*/, cl_command_queue /*command_queue*/) {
    ::av::icd::stub_called("cl", "clSetDefaultDeviceCommandQueue", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetDeviceAndHostTimer(cl_device_id /*device*/, cl_ulong* /*device_timestamp*/, cl_ulong* /*host_timestamp*/) {
    ::av::icd::stub_called("cl", "clGetDeviceAndHostTimer", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetHostTimer(cl_device_id /*device*/, cl_ulong* /*host_timestamp*/) {
    ::av::icd::stub_called("cl", "clGetHostTimer", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_context CL_API_CALL clCreateContext(const cl_context_properties* /*properties*/, cl_uint /*num_devices*/, const cl_device_id* /*devices*/, void (CL_CALLBACK* /*pfn_notify*/)(const char* errinfo, const void* private_info, size_t cb, void* user_data), void* /*user_data*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateContext", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_context CL_API_CALL clCreateContextFromType(const cl_context_properties* /*properties*/, cl_device_type /*device_type*/, void (CL_CALLBACK* /*pfn_notify*/)(const char* errinfo, const void* private_info, size_t cb, void* user_data), void* /*user_data*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateContextFromType", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clRetainContext(cl_context /*context*/) {
    ::av::icd::stub_called("cl", "clRetainContext", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clReleaseContext(cl_context /*context*/) {
    ::av::icd::stub_called("cl", "clReleaseContext", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetContextInfo(cl_context /*context*/, cl_context_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetContextInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clSetContextDestructorCallback(cl_context /*context*/, void (CL_CALLBACK* /*pfn_notify*/)(cl_context context, void* user_data), void* /*user_data*/) {
    ::av::icd::stub_called("cl", "clSetContextDestructorCallback", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_command_queue CL_API_CALL clCreateCommandQueueWithProperties(cl_context /*context*/, cl_device_id /*device*/, const cl_queue_properties* /*properties*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateCommandQueueWithProperties", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clRetainCommandQueue(cl_command_queue /*command_queue*/) {
    ::av::icd::stub_called("cl", "clRetainCommandQueue", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clReleaseCommandQueue(cl_command_queue /*command_queue*/) {
    ::av::icd::stub_called("cl", "clReleaseCommandQueue", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetCommandQueueInfo(cl_command_queue /*command_queue*/, cl_command_queue_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetCommandQueueInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_mem CL_API_CALL clCreateBuffer(cl_context /*context*/, cl_mem_flags /*flags*/, size_t /*size*/, void* /*host_ptr*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateBuffer", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_mem CL_API_CALL clCreateBufferWithProperties(cl_context /*context*/, const cl_mem_properties* /*properties*/, cl_mem_flags /*flags*/, size_t /*size*/, void* /*host_ptr*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateBufferWithProperties", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_mem CL_API_CALL clCreateSubBuffer(cl_mem /*buffer*/, cl_mem_flags /*flags*/, cl_buffer_create_type /*buffer_create_type*/, const void* /*buffer_create_info*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateSubBuffer", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_mem CL_API_CALL clCreateImage(cl_context /*context*/, cl_mem_flags /*flags*/, const cl_image_format* /*image_format*/, const cl_image_desc* /*image_desc*/, void* /*host_ptr*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateImage", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_mem CL_API_CALL clCreateImageWithProperties(cl_context /*context*/, const cl_mem_properties* /*properties*/, cl_mem_flags /*flags*/, const cl_image_format* /*image_format*/, const cl_image_desc* /*image_desc*/, void* /*host_ptr*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateImageWithProperties", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_mem CL_API_CALL clCreatePipe(cl_context /*context*/, cl_mem_flags /*flags*/, cl_uint /*pipe_packet_size*/, cl_uint /*pipe_max_packets*/, const cl_pipe_properties* /*properties*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreatePipe", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clRetainMemObject(cl_mem /*memobj*/) {
    ::av::icd::stub_called("cl", "clRetainMemObject", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clReleaseMemObject(cl_mem /*memobj*/) {
    ::av::icd::stub_called("cl", "clReleaseMemObject", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetSupportedImageFormats(cl_context /*context*/, cl_mem_flags /*flags*/, cl_mem_object_type /*image_type*/, cl_uint /*num_entries*/, cl_image_format* /*image_formats*/, cl_uint* /*num_image_formats*/) {
    ::av::icd::stub_called("cl", "clGetSupportedImageFormats", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetMemObjectInfo(cl_mem /*memobj*/, cl_mem_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetMemObjectInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetImageInfo(cl_mem /*image*/, cl_image_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetImageInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetPipeInfo(cl_mem /*pipe*/, cl_pipe_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetPipeInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clSetMemObjectDestructorCallback(cl_mem /*memobj*/, void (CL_CALLBACK* /*pfn_notify*/)(cl_mem memobj, void* user_data), void* /*user_data*/) {
    ::av::icd::stub_called("cl", "clSetMemObjectDestructorCallback", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline void* CL_API_CALL clSVMAlloc(cl_context /*context*/, cl_svm_mem_flags /*flags*/, size_t /*size*/, cl_uint /*alignment*/) {
    ::av::icd::stub_called("cl", "clSVMAlloc", "this driver does not carry it");
    return nullptr;
}
inline void CL_API_CALL clSVMFree(cl_context /*context*/, void* /*svm_pointer*/) {
    ::av::icd::stub_called("cl", "clSVMFree", "this driver does not carry it");
}
inline cl_sampler CL_API_CALL clCreateSamplerWithProperties(cl_context /*context*/, const cl_sampler_properties* /*sampler_properties*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateSamplerWithProperties", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clRetainSampler(cl_sampler /*sampler*/) {
    ::av::icd::stub_called("cl", "clRetainSampler", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clReleaseSampler(cl_sampler /*sampler*/) {
    ::av::icd::stub_called("cl", "clReleaseSampler", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetSamplerInfo(cl_sampler /*sampler*/, cl_sampler_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetSamplerInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_program CL_API_CALL clCreateProgramWithSource(cl_context /*context*/, cl_uint /*count*/, const char** /*strings*/, const size_t* /*lengths*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateProgramWithSource", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_program CL_API_CALL clCreateProgramWithBinary(cl_context /*context*/, cl_uint /*num_devices*/, const cl_device_id* /*device_list*/, const size_t* /*lengths*/, const unsigned char** /*binaries*/, cl_int* /*binary_status*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateProgramWithBinary", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_program CL_API_CALL clCreateProgramWithBuiltInKernels(cl_context /*context*/, cl_uint /*num_devices*/, const cl_device_id* /*device_list*/, const char* /*kernel_names*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateProgramWithBuiltInKernels", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_program CL_API_CALL clCreateProgramWithIL(cl_context /*context*/, const void* /*il*/, size_t /*length*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateProgramWithIL", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clRetainProgram(cl_program /*program*/) {
    ::av::icd::stub_called("cl", "clRetainProgram", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clReleaseProgram(cl_program /*program*/) {
    ::av::icd::stub_called("cl", "clReleaseProgram", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clBuildProgram(cl_program /*program*/, cl_uint /*num_devices*/, const cl_device_id* /*device_list*/, const char* /*options*/, void (CL_CALLBACK* /*pfn_notify*/)(cl_program program, void* user_data), void* /*user_data*/) {
    ::av::icd::stub_called("cl", "clBuildProgram", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clCompileProgram(cl_program /*program*/, cl_uint /*num_devices*/, const cl_device_id* /*device_list*/, const char* /*options*/, cl_uint /*num_input_headers*/, const cl_program* /*input_headers*/, const char** /*header_include_names*/, void (CL_CALLBACK* /*pfn_notify*/)(cl_program program, void* user_data), void* /*user_data*/) {
    ::av::icd::stub_called("cl", "clCompileProgram", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_program CL_API_CALL clLinkProgram(cl_context /*context*/, cl_uint /*num_devices*/, const cl_device_id* /*device_list*/, const char* /*options*/, cl_uint /*num_input_programs*/, const cl_program* /*input_programs*/, void (CL_CALLBACK* /*pfn_notify*/)(cl_program program, void* user_data), void* /*user_data*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clLinkProgram", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clSetProgramReleaseCallback(cl_program /*program*/, void (CL_CALLBACK* /*pfn_notify*/)(cl_program program, void* user_data), void* /*user_data*/) {
    ::av::icd::stub_called("cl", "clSetProgramReleaseCallback", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clSetProgramSpecializationConstant(cl_program /*program*/, cl_uint /*spec_id*/, size_t /*spec_size*/, const void* /*spec_value*/) {
    ::av::icd::stub_called("cl", "clSetProgramSpecializationConstant", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clUnloadPlatformCompiler(cl_platform_id /*platform*/) {
    ::av::icd::stub_called("cl", "clUnloadPlatformCompiler", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetProgramInfo(cl_program /*program*/, cl_program_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetProgramInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetProgramBuildInfo(cl_program /*program*/, cl_device_id /*device*/, cl_program_build_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetProgramBuildInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_kernel CL_API_CALL clCreateKernel(cl_program /*program*/, const char* /*kernel_name*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateKernel", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clCreateKernelsInProgram(cl_program /*program*/, cl_uint /*num_kernels*/, cl_kernel* /*kernels*/, cl_uint* /*num_kernels_ret*/) {
    ::av::icd::stub_called("cl", "clCreateKernelsInProgram", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_kernel CL_API_CALL clCloneKernel(cl_kernel /*source_kernel*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCloneKernel", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clRetainKernel(cl_kernel /*kernel*/) {
    ::av::icd::stub_called("cl", "clRetainKernel", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clReleaseKernel(cl_kernel /*kernel*/) {
    ::av::icd::stub_called("cl", "clReleaseKernel", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clSetKernelArg(cl_kernel /*kernel*/, cl_uint /*arg_index*/, size_t /*arg_size*/, const void* /*arg_value*/) {
    ::av::icd::stub_called("cl", "clSetKernelArg", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clSetKernelArgSVMPointer(cl_kernel /*kernel*/, cl_uint /*arg_index*/, const void* /*arg_value*/) {
    ::av::icd::stub_called("cl", "clSetKernelArgSVMPointer", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clSetKernelExecInfo(cl_kernel /*kernel*/, cl_kernel_exec_info /*param_name*/, size_t /*param_value_size*/, const void* /*param_value*/) {
    ::av::icd::stub_called("cl", "clSetKernelExecInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetKernelInfo(cl_kernel /*kernel*/, cl_kernel_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetKernelInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetKernelArgInfo(cl_kernel /*kernel*/, cl_uint /*arg_index*/, cl_kernel_arg_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetKernelArgInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetKernelWorkGroupInfo(cl_kernel /*kernel*/, cl_device_id /*device*/, cl_kernel_work_group_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetKernelWorkGroupInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetKernelSubGroupInfo(cl_kernel /*kernel*/, cl_device_id /*device*/, cl_kernel_sub_group_info /*param_name*/, size_t /*input_value_size*/, const void* /*input_value*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetKernelSubGroupInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clWaitForEvents(cl_uint /*num_events*/, const cl_event* /*event_list*/) {
    ::av::icd::stub_called("cl", "clWaitForEvents", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetEventInfo(cl_event /*event*/, cl_event_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetEventInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_event CL_API_CALL clCreateUserEvent(cl_context /*context*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateUserEvent", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clRetainEvent(cl_event /*event*/) {
    ::av::icd::stub_called("cl", "clRetainEvent", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clReleaseEvent(cl_event /*event*/) {
    ::av::icd::stub_called("cl", "clReleaseEvent", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clSetUserEventStatus(cl_event /*event*/, cl_int /*execution_status*/) {
    ::av::icd::stub_called("cl", "clSetUserEventStatus", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clSetEventCallback(cl_event /*event*/, cl_int /*command_exec_callback_type*/, void (CL_CALLBACK* /*pfn_notify*/)(cl_event event, cl_int event_command_status, void *user_data), void* /*user_data*/) {
    ::av::icd::stub_called("cl", "clSetEventCallback", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clGetEventProfilingInfo(cl_event /*event*/, cl_profiling_info /*param_name*/, size_t /*param_value_size*/, void* /*param_value*/, size_t* /*param_value_size_ret*/) {
    ::av::icd::stub_called("cl", "clGetEventProfilingInfo", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clFlush(cl_command_queue /*command_queue*/) {
    ::av::icd::stub_called("cl", "clFlush", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clFinish(cl_command_queue /*command_queue*/) {
    ::av::icd::stub_called("cl", "clFinish", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueReadBuffer(cl_command_queue /*command_queue*/, cl_mem /*buffer*/, cl_bool /*blocking_read*/, size_t /*offset*/, size_t /*size*/, void* /*ptr*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueReadBuffer", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueReadBufferRect(cl_command_queue /*command_queue*/, cl_mem /*buffer*/, cl_bool /*blocking_read*/, const size_t* /*buffer_origin*/, const size_t* /*host_origin*/, const size_t* /*region*/, size_t /*buffer_row_pitch*/, size_t /*buffer_slice_pitch*/, size_t /*host_row_pitch*/, size_t /*host_slice_pitch*/, void* /*ptr*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueReadBufferRect", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueWriteBuffer(cl_command_queue /*command_queue*/, cl_mem /*buffer*/, cl_bool /*blocking_write*/, size_t /*offset*/, size_t /*size*/, const void* /*ptr*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueWriteBuffer", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueWriteBufferRect(cl_command_queue /*command_queue*/, cl_mem /*buffer*/, cl_bool /*blocking_write*/, const size_t* /*buffer_origin*/, const size_t* /*host_origin*/, const size_t* /*region*/, size_t /*buffer_row_pitch*/, size_t /*buffer_slice_pitch*/, size_t /*host_row_pitch*/, size_t /*host_slice_pitch*/, const void* /*ptr*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueWriteBufferRect", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueFillBuffer(cl_command_queue /*command_queue*/, cl_mem /*buffer*/, const void* /*pattern*/, size_t /*pattern_size*/, size_t /*offset*/, size_t /*size*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueFillBuffer", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueCopyBuffer(cl_command_queue /*command_queue*/, cl_mem /*src_buffer*/, cl_mem /*dst_buffer*/, size_t /*src_offset*/, size_t /*dst_offset*/, size_t /*size*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueCopyBuffer", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueCopyBufferRect(cl_command_queue /*command_queue*/, cl_mem /*src_buffer*/, cl_mem /*dst_buffer*/, const size_t* /*src_origin*/, const size_t* /*dst_origin*/, const size_t* /*region*/, size_t /*src_row_pitch*/, size_t /*src_slice_pitch*/, size_t /*dst_row_pitch*/, size_t /*dst_slice_pitch*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueCopyBufferRect", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueReadImage(cl_command_queue /*command_queue*/, cl_mem /*image*/, cl_bool /*blocking_read*/, const size_t* /*origin*/, const size_t* /*region*/, size_t /*row_pitch*/, size_t /*slice_pitch*/, void* /*ptr*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueReadImage", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueWriteImage(cl_command_queue /*command_queue*/, cl_mem /*image*/, cl_bool /*blocking_write*/, const size_t* /*origin*/, const size_t* /*region*/, size_t /*input_row_pitch*/, size_t /*input_slice_pitch*/, const void* /*ptr*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueWriteImage", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueFillImage(cl_command_queue /*command_queue*/, cl_mem /*image*/, const void* /*fill_color*/, const size_t* /*origin*/, const size_t* /*region*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueFillImage", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueCopyImage(cl_command_queue /*command_queue*/, cl_mem /*src_image*/, cl_mem /*dst_image*/, const size_t* /*src_origin*/, const size_t* /*dst_origin*/, const size_t* /*region*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueCopyImage", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueCopyImageToBuffer(cl_command_queue /*command_queue*/, cl_mem /*src_image*/, cl_mem /*dst_buffer*/, const size_t* /*src_origin*/, const size_t* /*region*/, size_t /*dst_offset*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueCopyImageToBuffer", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueCopyBufferToImage(cl_command_queue /*command_queue*/, cl_mem /*src_buffer*/, cl_mem /*dst_image*/, size_t /*src_offset*/, const size_t* /*dst_origin*/, const size_t* /*region*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueCopyBufferToImage", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline void* CL_API_CALL clEnqueueMapBuffer(cl_command_queue /*command_queue*/, cl_mem /*buffer*/, cl_bool /*blocking_map*/, cl_map_flags /*map_flags*/, size_t /*offset*/, size_t /*size*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clEnqueueMapBuffer", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline void* CL_API_CALL clEnqueueMapImage(cl_command_queue /*command_queue*/, cl_mem /*image*/, cl_bool /*blocking_map*/, cl_map_flags /*map_flags*/, const size_t* /*origin*/, const size_t* /*region*/, size_t* /*image_row_pitch*/, size_t* /*image_slice_pitch*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clEnqueueMapImage", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clEnqueueUnmapMemObject(cl_command_queue /*command_queue*/, cl_mem /*memobj*/, void* /*mapped_ptr*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueUnmapMemObject", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueMigrateMemObjects(cl_command_queue /*command_queue*/, cl_uint /*num_mem_objects*/, const cl_mem* /*mem_objects*/, cl_mem_migration_flags /*flags*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueMigrateMemObjects", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueNDRangeKernel(cl_command_queue /*command_queue*/, cl_kernel /*kernel*/, cl_uint /*work_dim*/, const size_t* /*global_work_offset*/, const size_t* /*global_work_size*/, const size_t* /*local_work_size*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueNDRangeKernel", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueNativeKernel(cl_command_queue /*command_queue*/, void (CL_CALLBACK* /*user_func*/)(void*), void* /*args*/, size_t /*cb_args*/, cl_uint /*num_mem_objects*/, const cl_mem* /*mem_list*/, const void** /*args_mem_loc*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueNativeKernel", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueMarkerWithWaitList(cl_command_queue /*command_queue*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueMarkerWithWaitList", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueBarrierWithWaitList(cl_command_queue /*command_queue*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueBarrierWithWaitList", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueSVMFree(cl_command_queue /*command_queue*/, cl_uint /*num_svm_pointers*/, void* /*svm_pointers*/, void (CL_CALLBACK* /*pfn_free_func*/)(cl_command_queue queue, cl_uint num_svm_pointers, void* svm_pointers[], void* user_data), void* /*user_data*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueSVMFree", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueSVMMemcpy(cl_command_queue /*command_queue*/, cl_bool /*blocking_copy*/, void* /*dst_ptr*/, const void* /*src_ptr*/, size_t /*size*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueSVMMemcpy", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueSVMMemFill(cl_command_queue /*command_queue*/, void* /*svm_ptr*/, const void* /*pattern*/, size_t /*pattern_size*/, size_t /*size*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueSVMMemFill", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueSVMMap(cl_command_queue /*command_queue*/, cl_bool /*blocking_map*/, cl_map_flags /*flags*/, void* /*svm_ptr*/, size_t /*size*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueSVMMap", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueSVMUnmap(cl_command_queue /*command_queue*/, void* /*svm_ptr*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueSVMUnmap", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueSVMMigrateMem(cl_command_queue /*command_queue*/, cl_uint /*num_svm_pointers*/, const void** /*svm_pointers*/, const size_t* /*sizes*/, cl_mem_migration_flags /*flags*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueSVMMigrateMem", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline void* CL_API_CALL clGetExtensionFunctionAddressForPlatform(cl_platform_id /*platform*/, const char* /*func_name*/) {
    ::av::icd::stub_called("cl", "clGetExtensionFunctionAddressForPlatform", "this driver does not carry it");
    return nullptr;
}
inline cl_int CL_API_CALL clSetCommandQueueProperty(cl_command_queue /*command_queue*/, cl_command_queue_properties /*properties*/, cl_bool /*enable*/, cl_command_queue_properties* /*old_properties*/) {
    ::av::icd::stub_called("cl", "clSetCommandQueueProperty", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_mem CL_API_CALL clCreateImage2D(cl_context /*context*/, cl_mem_flags /*flags*/, const cl_image_format* /*image_format*/, size_t /*image_width*/, size_t /*image_height*/, size_t /*image_row_pitch*/, void* /*host_ptr*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateImage2D", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_mem CL_API_CALL clCreateImage3D(cl_context /*context*/, cl_mem_flags /*flags*/, const cl_image_format* /*image_format*/, size_t /*image_width*/, size_t /*image_height*/, size_t /*image_depth*/, size_t /*image_row_pitch*/, size_t /*image_slice_pitch*/, void* /*host_ptr*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateImage3D", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clEnqueueMarker(cl_command_queue /*command_queue*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueMarker", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueWaitForEvents(cl_command_queue /*command_queue*/, cl_uint /*num_events*/, const cl_event* /*event_list*/) {
    ::av::icd::stub_called("cl", "clEnqueueWaitForEvents", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clEnqueueBarrier(cl_command_queue /*command_queue*/) {
    ::av::icd::stub_called("cl", "clEnqueueBarrier", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline cl_int CL_API_CALL clUnloadCompiler() {
    ::av::icd::stub_called("cl", "clUnloadCompiler", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}
inline void* CL_API_CALL clGetExtensionFunctionAddress(const char* /*func_name*/) {
    ::av::icd::stub_called("cl", "clGetExtensionFunctionAddress", "this driver does not carry it");
    return nullptr;
}
inline cl_command_queue CL_API_CALL clCreateCommandQueue(cl_context /*context*/, cl_device_id /*device*/, cl_command_queue_properties /*properties*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateCommandQueue", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_sampler CL_API_CALL clCreateSampler(cl_context /*context*/, cl_bool /*normalized_coords*/, cl_addressing_mode /*addressing_mode*/, cl_filter_mode /*filter_mode*/, cl_int* errcode_ret) {
    ::av::icd::stub_called("cl", "clCreateSampler", "this driver does not carry it");
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; return nullptr;
}
inline cl_int CL_API_CALL clEnqueueTask(cl_command_queue /*command_queue*/, cl_kernel /*kernel*/, cl_uint /*num_events_in_wait_list*/, const cl_event* /*event_wait_list*/, cl_event* /*event*/) {
    ::av::icd::stub_called("cl", "clEnqueueTask", "this driver does not carry it");
    return CL_INVALID_OPERATION;
}

}  // namespace stubs

// A refusal built from the table's own slot type.
//
// The named stubs above are wire_written from the registry, and
// for a handful of commands the registry and cl_icd.h
// disagree about a parameter — void* against void*[], and
// the like. Those slots cannot take their named stub, and
// leaving them as they were found means leaving them null,
// which is the one outcome worth avoiding: the loader does
// not check, and calls through.
//
// So a second refusal is generated from the slot's own
// type. It cannot report through errcode_ret, because it
// does not know which parameter that is, but it returns
// what the return type means by failure and it is exactly
// the type the table wants.
template <typename Fn>
struct Refuse;

template <typename R, typename... A>
struct Refuse<R(CL_API_CALL*)(A...)> {
    static R CL_API_CALL call(A...) {
        if constexpr (std::is_void_v<R>) {
            return;
        } else if constexpr (std::is_pointer_v<R>) {
            return nullptr;
        } else {
            return static_cast<R>(CL_INVALID_OPERATION);
        }
    }
};

// Whether a slot is a function pointer at all. cl_icd.h
// declares the platform-tied ones as void*, and nothing
// can be put in those.
template <typename T>
struct IsRefusable : std::false_type {};

template <typename R, typename... A>
struct IsRefusable<R(CL_API_CALL*)(A...)> : std::true_type {};

// Fill every slot the driver did not claim.
//
// The named stub goes in when its type matches the slot
// exactly; the generic refusal goes in when the slot is a
// function pointer of some other shape; and where the
// header declares the slot as a bare void* — which is what
// it does for a version this translation unit was not
// built for — the named stub goes in through a cast,
// because a loader built for that version will call it
// with the signature the registry gave. Either way the
// slot is not left null.
//
// A template because `if constexpr` only discards what it
// does not take in a template: in an ordinary function
// the branch not taken is still type-checked, and the
// whole point here is that some of those branches do not
// type-check.
template <typename Table>
inline void fill_unimplemented(Table& table) {
    if constexpr (requires { table.clGetDeviceIDsFromD3D10KHR; }) {
        if constexpr (IsRefusable<decltype(table.clGetDeviceIDsFromD3D10KHR)>::value) {
            if (table.clGetDeviceIDsFromD3D10KHR == nullptr)
                table.clGetDeviceIDsFromD3D10KHR = &Refuse<decltype(table.clGetDeviceIDsFromD3D10KHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromD3D10BufferKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromD3D10BufferKHR)>::value) {
            if (table.clCreateFromD3D10BufferKHR == nullptr)
                table.clCreateFromD3D10BufferKHR = &Refuse<decltype(table.clCreateFromD3D10BufferKHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromD3D10Texture2DKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromD3D10Texture2DKHR)>::value) {
            if (table.clCreateFromD3D10Texture2DKHR == nullptr)
                table.clCreateFromD3D10Texture2DKHR = &Refuse<decltype(table.clCreateFromD3D10Texture2DKHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromD3D10Texture3DKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromD3D10Texture3DKHR)>::value) {
            if (table.clCreateFromD3D10Texture3DKHR == nullptr)
                table.clCreateFromD3D10Texture3DKHR = &Refuse<decltype(table.clCreateFromD3D10Texture3DKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueAcquireD3D10ObjectsKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueAcquireD3D10ObjectsKHR)>::value) {
            if (table.clEnqueueAcquireD3D10ObjectsKHR == nullptr)
                table.clEnqueueAcquireD3D10ObjectsKHR = &Refuse<decltype(table.clEnqueueAcquireD3D10ObjectsKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueReleaseD3D10ObjectsKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueReleaseD3D10ObjectsKHR)>::value) {
            if (table.clEnqueueReleaseD3D10ObjectsKHR == nullptr)
                table.clEnqueueReleaseD3D10ObjectsKHR = &Refuse<decltype(table.clEnqueueReleaseD3D10ObjectsKHR)>::call;
        }
    }
    if constexpr (requires { table.clGetDeviceIDsFromD3D11KHR; }) {
        if constexpr (IsRefusable<decltype(table.clGetDeviceIDsFromD3D11KHR)>::value) {
            if (table.clGetDeviceIDsFromD3D11KHR == nullptr)
                table.clGetDeviceIDsFromD3D11KHR = &Refuse<decltype(table.clGetDeviceIDsFromD3D11KHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromD3D11BufferKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromD3D11BufferKHR)>::value) {
            if (table.clCreateFromD3D11BufferKHR == nullptr)
                table.clCreateFromD3D11BufferKHR = &Refuse<decltype(table.clCreateFromD3D11BufferKHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromD3D11Texture2DKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromD3D11Texture2DKHR)>::value) {
            if (table.clCreateFromD3D11Texture2DKHR == nullptr)
                table.clCreateFromD3D11Texture2DKHR = &Refuse<decltype(table.clCreateFromD3D11Texture2DKHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromD3D11Texture3DKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromD3D11Texture3DKHR)>::value) {
            if (table.clCreateFromD3D11Texture3DKHR == nullptr)
                table.clCreateFromD3D11Texture3DKHR = &Refuse<decltype(table.clCreateFromD3D11Texture3DKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueAcquireD3D11ObjectsKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueAcquireD3D11ObjectsKHR)>::value) {
            if (table.clEnqueueAcquireD3D11ObjectsKHR == nullptr)
                table.clEnqueueAcquireD3D11ObjectsKHR = &Refuse<decltype(table.clEnqueueAcquireD3D11ObjectsKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueReleaseD3D11ObjectsKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueReleaseD3D11ObjectsKHR)>::value) {
            if (table.clEnqueueReleaseD3D11ObjectsKHR == nullptr)
                table.clEnqueueReleaseD3D11ObjectsKHR = &Refuse<decltype(table.clEnqueueReleaseD3D11ObjectsKHR)>::call;
        }
    }
    if constexpr (requires { table.clGetDeviceIDsFromDX9MediaAdapterKHR; }) {
        if constexpr (IsRefusable<decltype(table.clGetDeviceIDsFromDX9MediaAdapterKHR)>::value) {
            if (table.clGetDeviceIDsFromDX9MediaAdapterKHR == nullptr)
                table.clGetDeviceIDsFromDX9MediaAdapterKHR = &Refuse<decltype(table.clGetDeviceIDsFromDX9MediaAdapterKHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromDX9MediaSurfaceKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromDX9MediaSurfaceKHR)>::value) {
            if (table.clCreateFromDX9MediaSurfaceKHR == nullptr)
                table.clCreateFromDX9MediaSurfaceKHR = &Refuse<decltype(table.clCreateFromDX9MediaSurfaceKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueAcquireDX9MediaSurfacesKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueAcquireDX9MediaSurfacesKHR)>::value) {
            if (table.clEnqueueAcquireDX9MediaSurfacesKHR == nullptr)
                table.clEnqueueAcquireDX9MediaSurfacesKHR = &Refuse<decltype(table.clEnqueueAcquireDX9MediaSurfacesKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueReleaseDX9MediaSurfacesKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueReleaseDX9MediaSurfacesKHR)>::value) {
            if (table.clEnqueueReleaseDX9MediaSurfacesKHR == nullptr)
                table.clEnqueueReleaseDX9MediaSurfacesKHR = &Refuse<decltype(table.clEnqueueReleaseDX9MediaSurfacesKHR)>::call;
        }
    }
    if constexpr (requires { table.clGetDeviceIDsFromDX9INTEL; }) {
        if constexpr (IsRefusable<decltype(table.clGetDeviceIDsFromDX9INTEL)>::value) {
            if (table.clGetDeviceIDsFromDX9INTEL == nullptr)
                table.clGetDeviceIDsFromDX9INTEL = &Refuse<decltype(table.clGetDeviceIDsFromDX9INTEL)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromDX9MediaSurfaceINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromDX9MediaSurfaceINTEL)>::value) {
            if (table.clCreateFromDX9MediaSurfaceINTEL == nullptr)
                table.clCreateFromDX9MediaSurfaceINTEL = &Refuse<decltype(table.clCreateFromDX9MediaSurfaceINTEL)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueAcquireDX9ObjectsINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueAcquireDX9ObjectsINTEL)>::value) {
            if (table.clEnqueueAcquireDX9ObjectsINTEL == nullptr)
                table.clEnqueueAcquireDX9ObjectsINTEL = &Refuse<decltype(table.clEnqueueAcquireDX9ObjectsINTEL)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueReleaseDX9ObjectsINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueReleaseDX9ObjectsINTEL)>::value) {
            if (table.clEnqueueReleaseDX9ObjectsINTEL == nullptr)
                table.clEnqueueReleaseDX9ObjectsINTEL = &Refuse<decltype(table.clEnqueueReleaseDX9ObjectsINTEL)>::call;
        }
    }
    if constexpr (requires { table.clCreateEventFromEGLSyncKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateEventFromEGLSyncKHR)>::value) {
            if (table.clCreateEventFromEGLSyncKHR == nullptr)
                table.clCreateEventFromEGLSyncKHR = &Refuse<decltype(table.clCreateEventFromEGLSyncKHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromEGLImageKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromEGLImageKHR)>::value) {
            if (table.clCreateFromEGLImageKHR == nullptr)
                table.clCreateFromEGLImageKHR = &Refuse<decltype(table.clCreateFromEGLImageKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueAcquireEGLObjectsKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueAcquireEGLObjectsKHR)>::value) {
            if (table.clEnqueueAcquireEGLObjectsKHR == nullptr)
                table.clEnqueueAcquireEGLObjectsKHR = &Refuse<decltype(table.clEnqueueAcquireEGLObjectsKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueReleaseEGLObjectsKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueReleaseEGLObjectsKHR)>::value) {
            if (table.clEnqueueReleaseEGLObjectsKHR == nullptr)
                table.clEnqueueReleaseEGLObjectsKHR = &Refuse<decltype(table.clEnqueueReleaseEGLObjectsKHR)>::call;
        }
    }
    if constexpr (requires { table.clLogMessagesToSystemLogAPPLE; }) {
        if constexpr (IsRefusable<decltype(table.clLogMessagesToSystemLogAPPLE)>::value) {
            if (table.clLogMessagesToSystemLogAPPLE == nullptr)
                table.clLogMessagesToSystemLogAPPLE = &Refuse<decltype(table.clLogMessagesToSystemLogAPPLE)>::call;
        }
    }
    if constexpr (requires { table.clLogMessagesToStdoutAPPLE; }) {
        if constexpr (IsRefusable<decltype(table.clLogMessagesToStdoutAPPLE)>::value) {
            if (table.clLogMessagesToStdoutAPPLE == nullptr)
                table.clLogMessagesToStdoutAPPLE = &Refuse<decltype(table.clLogMessagesToStdoutAPPLE)>::call;
        }
    }
    if constexpr (requires { table.clLogMessagesToStderrAPPLE; }) {
        if constexpr (IsRefusable<decltype(table.clLogMessagesToStderrAPPLE)>::value) {
            if (table.clLogMessagesToStderrAPPLE == nullptr)
                table.clLogMessagesToStderrAPPLE = &Refuse<decltype(table.clLogMessagesToStderrAPPLE)>::call;
        }
    }
    if constexpr (requires { table.clIcdGetPlatformIDsKHR; }) {
        if constexpr (IsRefusable<decltype(table.clIcdGetPlatformIDsKHR)>::value) {
            if (table.clIcdGetPlatformIDsKHR == nullptr)
                table.clIcdGetPlatformIDsKHR = &Refuse<decltype(table.clIcdGetPlatformIDsKHR)>::call;
        }
    }
    if constexpr (requires { table.clIcdGetFunctionAddressForPlatformKHR; }) {
        if constexpr (IsRefusable<decltype(table.clIcdGetFunctionAddressForPlatformKHR)>::value) {
            if (table.clIcdGetFunctionAddressForPlatformKHR == nullptr)
                table.clIcdGetFunctionAddressForPlatformKHR = &Refuse<decltype(table.clIcdGetFunctionAddressForPlatformKHR)>::call;
        }
    }
    if constexpr (requires { table.clIcdSetPlatformDispatchDataKHR; }) {
        if constexpr (IsRefusable<decltype(table.clIcdSetPlatformDispatchDataKHR)>::value) {
            if (table.clIcdSetPlatformDispatchDataKHR == nullptr)
                table.clIcdSetPlatformDispatchDataKHR = &Refuse<decltype(table.clIcdSetPlatformDispatchDataKHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateProgramWithILKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateProgramWithILKHR)>::value) {
            if (table.clCreateProgramWithILKHR == nullptr)
                table.clCreateProgramWithILKHR = &Refuse<decltype(table.clCreateProgramWithILKHR)>::call;
        }
    }
    if constexpr (requires { table.clTerminateContextKHR; }) {
        if constexpr (IsRefusable<decltype(table.clTerminateContextKHR)>::value) {
            if (table.clTerminateContextKHR == nullptr)
                table.clTerminateContextKHR = &Refuse<decltype(table.clTerminateContextKHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateCommandQueueWithPropertiesKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateCommandQueueWithPropertiesKHR)>::value) {
            if (table.clCreateCommandQueueWithPropertiesKHR == nullptr)
                table.clCreateCommandQueueWithPropertiesKHR = &Refuse<decltype(table.clCreateCommandQueueWithPropertiesKHR)>::call;
        }
    }
    if constexpr (requires { table.clReleaseDeviceEXT; }) {
        if constexpr (IsRefusable<decltype(table.clReleaseDeviceEXT)>::value) {
            if (table.clReleaseDeviceEXT == nullptr)
                table.clReleaseDeviceEXT = &Refuse<decltype(table.clReleaseDeviceEXT)>::call;
        }
    }
    if constexpr (requires { table.clRetainDeviceEXT; }) {
        if constexpr (IsRefusable<decltype(table.clRetainDeviceEXT)>::value) {
            if (table.clRetainDeviceEXT == nullptr)
                table.clRetainDeviceEXT = &Refuse<decltype(table.clRetainDeviceEXT)>::call;
        }
    }
    if constexpr (requires { table.clCreateSubDevicesEXT; }) {
        if constexpr (IsRefusable<decltype(table.clCreateSubDevicesEXT)>::value) {
            if (table.clCreateSubDevicesEXT == nullptr)
                table.clCreateSubDevicesEXT = &Refuse<decltype(table.clCreateSubDevicesEXT)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueMigrateMemObjectEXT; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueMigrateMemObjectEXT)>::value) {
            if (table.clEnqueueMigrateMemObjectEXT == nullptr)
                table.clEnqueueMigrateMemObjectEXT = &Refuse<decltype(table.clEnqueueMigrateMemObjectEXT)>::call;
        }
    }
    if constexpr (requires { table.clGetDeviceImageInfoQCOM; }) {
        if constexpr (IsRefusable<decltype(table.clGetDeviceImageInfoQCOM)>::value) {
            if (table.clGetDeviceImageInfoQCOM == nullptr)
                table.clGetDeviceImageInfoQCOM = &Refuse<decltype(table.clGetDeviceImageInfoQCOM)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueAcquireGrallocObjectsIMG; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueAcquireGrallocObjectsIMG)>::value) {
            if (table.clEnqueueAcquireGrallocObjectsIMG == nullptr)
                table.clEnqueueAcquireGrallocObjectsIMG = &Refuse<decltype(table.clEnqueueAcquireGrallocObjectsIMG)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueGenerateMipmapIMG; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueGenerateMipmapIMG)>::value) {
            if (table.clEnqueueGenerateMipmapIMG == nullptr)
                table.clEnqueueGenerateMipmapIMG = &Refuse<decltype(table.clEnqueueGenerateMipmapIMG)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueReleaseGrallocObjectsIMG; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueReleaseGrallocObjectsIMG)>::value) {
            if (table.clEnqueueReleaseGrallocObjectsIMG == nullptr)
                table.clEnqueueReleaseGrallocObjectsIMG = &Refuse<decltype(table.clEnqueueReleaseGrallocObjectsIMG)>::call;
        }
    }
    if constexpr (requires { table.clGetKernelSubGroupInfoKHR; }) {
        if constexpr (IsRefusable<decltype(table.clGetKernelSubGroupInfoKHR)>::value) {
            if (table.clGetKernelSubGroupInfoKHR == nullptr)
                table.clGetKernelSubGroupInfoKHR = &Refuse<decltype(table.clGetKernelSubGroupInfoKHR)>::call;
        }
    }
    if constexpr (requires { table.clGetKernelSuggestedLocalWorkSizeKHR; }) {
        if constexpr (IsRefusable<decltype(table.clGetKernelSuggestedLocalWorkSizeKHR)>::value) {
            if (table.clGetKernelSuggestedLocalWorkSizeKHR == nullptr)
                table.clGetKernelSuggestedLocalWorkSizeKHR = &Refuse<decltype(table.clGetKernelSuggestedLocalWorkSizeKHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateSemaphoreWithPropertiesKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateSemaphoreWithPropertiesKHR)>::value) {
            if (table.clCreateSemaphoreWithPropertiesKHR == nullptr)
                table.clCreateSemaphoreWithPropertiesKHR = &Refuse<decltype(table.clCreateSemaphoreWithPropertiesKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueWaitSemaphoresKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueWaitSemaphoresKHR)>::value) {
            if (table.clEnqueueWaitSemaphoresKHR == nullptr)
                table.clEnqueueWaitSemaphoresKHR = &Refuse<decltype(table.clEnqueueWaitSemaphoresKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueSignalSemaphoresKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueSignalSemaphoresKHR)>::value) {
            if (table.clEnqueueSignalSemaphoresKHR == nullptr)
                table.clEnqueueSignalSemaphoresKHR = &Refuse<decltype(table.clEnqueueSignalSemaphoresKHR)>::call;
        }
    }
    if constexpr (requires { table.clGetSemaphoreInfoKHR; }) {
        if constexpr (IsRefusable<decltype(table.clGetSemaphoreInfoKHR)>::value) {
            if (table.clGetSemaphoreInfoKHR == nullptr)
                table.clGetSemaphoreInfoKHR = &Refuse<decltype(table.clGetSemaphoreInfoKHR)>::call;
        }
    }
    if constexpr (requires { table.clReleaseSemaphoreKHR; }) {
        if constexpr (IsRefusable<decltype(table.clReleaseSemaphoreKHR)>::value) {
            if (table.clReleaseSemaphoreKHR == nullptr)
                table.clReleaseSemaphoreKHR = &Refuse<decltype(table.clReleaseSemaphoreKHR)>::call;
        }
    }
    if constexpr (requires { table.clRetainSemaphoreKHR; }) {
        if constexpr (IsRefusable<decltype(table.clRetainSemaphoreKHR)>::value) {
            if (table.clRetainSemaphoreKHR == nullptr)
                table.clRetainSemaphoreKHR = &Refuse<decltype(table.clRetainSemaphoreKHR)>::call;
        }
    }
    if constexpr (requires { table.clGetSemaphoreHandleForTypeKHR; }) {
        if constexpr (IsRefusable<decltype(table.clGetSemaphoreHandleForTypeKHR)>::value) {
            if (table.clGetSemaphoreHandleForTypeKHR == nullptr)
                table.clGetSemaphoreHandleForTypeKHR = &Refuse<decltype(table.clGetSemaphoreHandleForTypeKHR)>::call;
        }
    }
    if constexpr (requires { table.clReImportSemaphoreSyncFdKHR; }) {
        if constexpr (IsRefusable<decltype(table.clReImportSemaphoreSyncFdKHR)>::value) {
            if (table.clReImportSemaphoreSyncFdKHR == nullptr)
                table.clReImportSemaphoreSyncFdKHR = &Refuse<decltype(table.clReImportSemaphoreSyncFdKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueAcquireExternalMemObjectsKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueAcquireExternalMemObjectsKHR)>::value) {
            if (table.clEnqueueAcquireExternalMemObjectsKHR == nullptr)
                table.clEnqueueAcquireExternalMemObjectsKHR = &Refuse<decltype(table.clEnqueueAcquireExternalMemObjectsKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueReleaseExternalMemObjectsKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueReleaseExternalMemObjectsKHR)>::value) {
            if (table.clEnqueueReleaseExternalMemObjectsKHR == nullptr)
                table.clEnqueueReleaseExternalMemObjectsKHR = &Refuse<decltype(table.clEnqueueReleaseExternalMemObjectsKHR)>::call;
        }
    }
    if constexpr (requires { table.clImportMemoryARM; }) {
        if constexpr (IsRefusable<decltype(table.clImportMemoryARM)>::value) {
            if (table.clImportMemoryARM == nullptr)
                table.clImportMemoryARM = &Refuse<decltype(table.clImportMemoryARM)>::call;
        }
    }
    if constexpr (requires { table.clSVMAllocARM; }) {
        if constexpr (IsRefusable<decltype(table.clSVMAllocARM)>::value) {
            if (table.clSVMAllocARM == nullptr)
                table.clSVMAllocARM = &Refuse<decltype(table.clSVMAllocARM)>::call;
        }
    }
    if constexpr (requires { table.clSVMFreeARM; }) {
        if constexpr (IsRefusable<decltype(table.clSVMFreeARM)>::value) {
            if (table.clSVMFreeARM == nullptr)
                table.clSVMFreeARM = &Refuse<decltype(table.clSVMFreeARM)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueSVMFreeARM; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueSVMFreeARM)>::value) {
            if (table.clEnqueueSVMFreeARM == nullptr)
                table.clEnqueueSVMFreeARM = &Refuse<decltype(table.clEnqueueSVMFreeARM)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueSVMMemcpyARM; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueSVMMemcpyARM)>::value) {
            if (table.clEnqueueSVMMemcpyARM == nullptr)
                table.clEnqueueSVMMemcpyARM = &Refuse<decltype(table.clEnqueueSVMMemcpyARM)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueSVMMemFillARM; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueSVMMemFillARM)>::value) {
            if (table.clEnqueueSVMMemFillARM == nullptr)
                table.clEnqueueSVMMemFillARM = &Refuse<decltype(table.clEnqueueSVMMemFillARM)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueSVMMapARM; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueSVMMapARM)>::value) {
            if (table.clEnqueueSVMMapARM == nullptr)
                table.clEnqueueSVMMapARM = &Refuse<decltype(table.clEnqueueSVMMapARM)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueSVMUnmapARM; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueSVMUnmapARM)>::value) {
            if (table.clEnqueueSVMUnmapARM == nullptr)
                table.clEnqueueSVMUnmapARM = &Refuse<decltype(table.clEnqueueSVMUnmapARM)>::call;
        }
    }
    if constexpr (requires { table.clSetKernelArgSVMPointerARM; }) {
        if constexpr (IsRefusable<decltype(table.clSetKernelArgSVMPointerARM)>::value) {
            if (table.clSetKernelArgSVMPointerARM == nullptr)
                table.clSetKernelArgSVMPointerARM = &Refuse<decltype(table.clSetKernelArgSVMPointerARM)>::call;
        }
    }
    if constexpr (requires { table.clSetKernelExecInfoARM; }) {
        if constexpr (IsRefusable<decltype(table.clSetKernelExecInfoARM)>::value) {
            if (table.clSetKernelExecInfoARM == nullptr)
                table.clSetKernelExecInfoARM = &Refuse<decltype(table.clSetKernelExecInfoARM)>::call;
        }
    }
    if constexpr (requires { table.clCreateAcceleratorINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clCreateAcceleratorINTEL)>::value) {
            if (table.clCreateAcceleratorINTEL == nullptr)
                table.clCreateAcceleratorINTEL = &Refuse<decltype(table.clCreateAcceleratorINTEL)>::call;
        }
    }
    if constexpr (requires { table.clGetAcceleratorInfoINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clGetAcceleratorInfoINTEL)>::value) {
            if (table.clGetAcceleratorInfoINTEL == nullptr)
                table.clGetAcceleratorInfoINTEL = &Refuse<decltype(table.clGetAcceleratorInfoINTEL)>::call;
        }
    }
    if constexpr (requires { table.clRetainAcceleratorINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clRetainAcceleratorINTEL)>::value) {
            if (table.clRetainAcceleratorINTEL == nullptr)
                table.clRetainAcceleratorINTEL = &Refuse<decltype(table.clRetainAcceleratorINTEL)>::call;
        }
    }
    if constexpr (requires { table.clReleaseAcceleratorINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clReleaseAcceleratorINTEL)>::value) {
            if (table.clReleaseAcceleratorINTEL == nullptr)
                table.clReleaseAcceleratorINTEL = &Refuse<decltype(table.clReleaseAcceleratorINTEL)>::call;
        }
    }
    if constexpr (requires { table.clCreateEventFromGLsyncKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateEventFromGLsyncKHR)>::value) {
            if (table.clCreateEventFromGLsyncKHR == nullptr)
                table.clCreateEventFromGLsyncKHR = &Refuse<decltype(table.clCreateEventFromGLsyncKHR)>::call;
        }
    }
    if constexpr (requires { table.clGetGLContextInfoKHR; }) {
        if constexpr (IsRefusable<decltype(table.clGetGLContextInfoKHR)>::value) {
            if (table.clGetGLContextInfoKHR == nullptr)
                table.clGetGLContextInfoKHR = &Refuse<decltype(table.clGetGLContextInfoKHR)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromGLBuffer; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromGLBuffer)>::value) {
            if (table.clCreateFromGLBuffer == nullptr)
                table.clCreateFromGLBuffer = &Refuse<decltype(table.clCreateFromGLBuffer)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromGLTexture; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromGLTexture)>::value) {
            if (table.clCreateFromGLTexture == nullptr)
                table.clCreateFromGLTexture = &Refuse<decltype(table.clCreateFromGLTexture)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromGLRenderbuffer; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromGLRenderbuffer)>::value) {
            if (table.clCreateFromGLRenderbuffer == nullptr)
                table.clCreateFromGLRenderbuffer = &Refuse<decltype(table.clCreateFromGLRenderbuffer)>::call;
        }
    }
    if constexpr (requires { table.clGetGLObjectInfo; }) {
        if constexpr (IsRefusable<decltype(table.clGetGLObjectInfo)>::value) {
            if (table.clGetGLObjectInfo == nullptr)
                table.clGetGLObjectInfo = &Refuse<decltype(table.clGetGLObjectInfo)>::call;
        }
    }
    if constexpr (requires { table.clGetGLTextureInfo; }) {
        if constexpr (IsRefusable<decltype(table.clGetGLTextureInfo)>::value) {
            if (table.clGetGLTextureInfo == nullptr)
                table.clGetGLTextureInfo = &Refuse<decltype(table.clGetGLTextureInfo)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueAcquireGLObjects; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueAcquireGLObjects)>::value) {
            if (table.clEnqueueAcquireGLObjects == nullptr)
                table.clEnqueueAcquireGLObjects = &Refuse<decltype(table.clEnqueueAcquireGLObjects)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueReleaseGLObjects; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueReleaseGLObjects)>::value) {
            if (table.clEnqueueReleaseGLObjects == nullptr)
                table.clEnqueueReleaseGLObjects = &Refuse<decltype(table.clEnqueueReleaseGLObjects)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromGLTexture2D; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromGLTexture2D)>::value) {
            if (table.clCreateFromGLTexture2D == nullptr)
                table.clCreateFromGLTexture2D = &Refuse<decltype(table.clCreateFromGLTexture2D)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromGLTexture3D; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromGLTexture3D)>::value) {
            if (table.clCreateFromGLTexture3D == nullptr)
                table.clCreateFromGLTexture3D = &Refuse<decltype(table.clCreateFromGLTexture3D)>::call;
        }
    }
    if constexpr (requires { table.clGetDeviceIDsFromVA_APIMediaAdapterINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clGetDeviceIDsFromVA_APIMediaAdapterINTEL)>::value) {
            if (table.clGetDeviceIDsFromVA_APIMediaAdapterINTEL == nullptr)
                table.clGetDeviceIDsFromVA_APIMediaAdapterINTEL = &Refuse<decltype(table.clGetDeviceIDsFromVA_APIMediaAdapterINTEL)>::call;
        }
    }
    if constexpr (requires { table.clCreateFromVA_APIMediaSurfaceINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clCreateFromVA_APIMediaSurfaceINTEL)>::value) {
            if (table.clCreateFromVA_APIMediaSurfaceINTEL == nullptr)
                table.clCreateFromVA_APIMediaSurfaceINTEL = &Refuse<decltype(table.clCreateFromVA_APIMediaSurfaceINTEL)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueAcquireVA_APIMediaSurfacesINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueAcquireVA_APIMediaSurfacesINTEL)>::value) {
            if (table.clEnqueueAcquireVA_APIMediaSurfacesINTEL == nullptr)
                table.clEnqueueAcquireVA_APIMediaSurfacesINTEL = &Refuse<decltype(table.clEnqueueAcquireVA_APIMediaSurfacesINTEL)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueReleaseVA_APIMediaSurfacesINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueReleaseVA_APIMediaSurfacesINTEL)>::value) {
            if (table.clEnqueueReleaseVA_APIMediaSurfacesINTEL == nullptr)
                table.clEnqueueReleaseVA_APIMediaSurfacesINTEL = &Refuse<decltype(table.clEnqueueReleaseVA_APIMediaSurfacesINTEL)>::call;
        }
    }
    if constexpr (requires { table.clHostMemAllocINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clHostMemAllocINTEL)>::value) {
            if (table.clHostMemAllocINTEL == nullptr)
                table.clHostMemAllocINTEL = &Refuse<decltype(table.clHostMemAllocINTEL)>::call;
        }
    }
    if constexpr (requires { table.clDeviceMemAllocINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clDeviceMemAllocINTEL)>::value) {
            if (table.clDeviceMemAllocINTEL == nullptr)
                table.clDeviceMemAllocINTEL = &Refuse<decltype(table.clDeviceMemAllocINTEL)>::call;
        }
    }
    if constexpr (requires { table.clSharedMemAllocINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clSharedMemAllocINTEL)>::value) {
            if (table.clSharedMemAllocINTEL == nullptr)
                table.clSharedMemAllocINTEL = &Refuse<decltype(table.clSharedMemAllocINTEL)>::call;
        }
    }
    if constexpr (requires { table.clMemFreeINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clMemFreeINTEL)>::value) {
            if (table.clMemFreeINTEL == nullptr)
                table.clMemFreeINTEL = &Refuse<decltype(table.clMemFreeINTEL)>::call;
        }
    }
    if constexpr (requires { table.clMemBlockingFreeINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clMemBlockingFreeINTEL)>::value) {
            if (table.clMemBlockingFreeINTEL == nullptr)
                table.clMemBlockingFreeINTEL = &Refuse<decltype(table.clMemBlockingFreeINTEL)>::call;
        }
    }
    if constexpr (requires { table.clGetMemAllocInfoINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clGetMemAllocInfoINTEL)>::value) {
            if (table.clGetMemAllocInfoINTEL == nullptr)
                table.clGetMemAllocInfoINTEL = &Refuse<decltype(table.clGetMemAllocInfoINTEL)>::call;
        }
    }
    if constexpr (requires { table.clSetKernelArgMemPointerINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clSetKernelArgMemPointerINTEL)>::value) {
            if (table.clSetKernelArgMemPointerINTEL == nullptr)
                table.clSetKernelArgMemPointerINTEL = &Refuse<decltype(table.clSetKernelArgMemPointerINTEL)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueMemsetINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueMemsetINTEL)>::value) {
            if (table.clEnqueueMemsetINTEL == nullptr)
                table.clEnqueueMemsetINTEL = &Refuse<decltype(table.clEnqueueMemsetINTEL)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueMemFillINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueMemFillINTEL)>::value) {
            if (table.clEnqueueMemFillINTEL == nullptr)
                table.clEnqueueMemFillINTEL = &Refuse<decltype(table.clEnqueueMemFillINTEL)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueMemcpyINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueMemcpyINTEL)>::value) {
            if (table.clEnqueueMemcpyINTEL == nullptr)
                table.clEnqueueMemcpyINTEL = &Refuse<decltype(table.clEnqueueMemcpyINTEL)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueMigrateMemINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueMigrateMemINTEL)>::value) {
            if (table.clEnqueueMigrateMemINTEL == nullptr)
                table.clEnqueueMigrateMemINTEL = &Refuse<decltype(table.clEnqueueMigrateMemINTEL)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueMemAdviseINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueMemAdviseINTEL)>::value) {
            if (table.clEnqueueMemAdviseINTEL == nullptr)
                table.clEnqueueMemAdviseINTEL = &Refuse<decltype(table.clEnqueueMemAdviseINTEL)>::call;
        }
    }
    if constexpr (requires { table.clCreateBufferWithPropertiesINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clCreateBufferWithPropertiesINTEL)>::value) {
            if (table.clCreateBufferWithPropertiesINTEL == nullptr)
                table.clCreateBufferWithPropertiesINTEL = &Refuse<decltype(table.clCreateBufferWithPropertiesINTEL)>::call;
        }
    }
    if constexpr (requires { table.clCreateCommandBufferKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCreateCommandBufferKHR)>::value) {
            if (table.clCreateCommandBufferKHR == nullptr)
                table.clCreateCommandBufferKHR = &Refuse<decltype(table.clCreateCommandBufferKHR)>::call;
        }
    }
    if constexpr (requires { table.clFinalizeCommandBufferKHR; }) {
        if constexpr (IsRefusable<decltype(table.clFinalizeCommandBufferKHR)>::value) {
            if (table.clFinalizeCommandBufferKHR == nullptr)
                table.clFinalizeCommandBufferKHR = &Refuse<decltype(table.clFinalizeCommandBufferKHR)>::call;
        }
    }
    if constexpr (requires { table.clRetainCommandBufferKHR; }) {
        if constexpr (IsRefusable<decltype(table.clRetainCommandBufferKHR)>::value) {
            if (table.clRetainCommandBufferKHR == nullptr)
                table.clRetainCommandBufferKHR = &Refuse<decltype(table.clRetainCommandBufferKHR)>::call;
        }
    }
    if constexpr (requires { table.clReleaseCommandBufferKHR; }) {
        if constexpr (IsRefusable<decltype(table.clReleaseCommandBufferKHR)>::value) {
            if (table.clReleaseCommandBufferKHR == nullptr)
                table.clReleaseCommandBufferKHR = &Refuse<decltype(table.clReleaseCommandBufferKHR)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueCommandBufferKHR; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueCommandBufferKHR)>::value) {
            if (table.clEnqueueCommandBufferKHR == nullptr)
                table.clEnqueueCommandBufferKHR = &Refuse<decltype(table.clEnqueueCommandBufferKHR)>::call;
        }
    }
    if constexpr (requires { table.clCommandBarrierWithWaitListKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCommandBarrierWithWaitListKHR)>::value) {
            if (table.clCommandBarrierWithWaitListKHR == nullptr)
                table.clCommandBarrierWithWaitListKHR = &Refuse<decltype(table.clCommandBarrierWithWaitListKHR)>::call;
        }
    }
    if constexpr (requires { table.clCommandCopyBufferKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCommandCopyBufferKHR)>::value) {
            if (table.clCommandCopyBufferKHR == nullptr)
                table.clCommandCopyBufferKHR = &Refuse<decltype(table.clCommandCopyBufferKHR)>::call;
        }
    }
    if constexpr (requires { table.clCommandCopyBufferRectKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCommandCopyBufferRectKHR)>::value) {
            if (table.clCommandCopyBufferRectKHR == nullptr)
                table.clCommandCopyBufferRectKHR = &Refuse<decltype(table.clCommandCopyBufferRectKHR)>::call;
        }
    }
    if constexpr (requires { table.clCommandCopyBufferToImageKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCommandCopyBufferToImageKHR)>::value) {
            if (table.clCommandCopyBufferToImageKHR == nullptr)
                table.clCommandCopyBufferToImageKHR = &Refuse<decltype(table.clCommandCopyBufferToImageKHR)>::call;
        }
    }
    if constexpr (requires { table.clCommandCopyImageKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCommandCopyImageKHR)>::value) {
            if (table.clCommandCopyImageKHR == nullptr)
                table.clCommandCopyImageKHR = &Refuse<decltype(table.clCommandCopyImageKHR)>::call;
        }
    }
    if constexpr (requires { table.clCommandCopyImageToBufferKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCommandCopyImageToBufferKHR)>::value) {
            if (table.clCommandCopyImageToBufferKHR == nullptr)
                table.clCommandCopyImageToBufferKHR = &Refuse<decltype(table.clCommandCopyImageToBufferKHR)>::call;
        }
    }
    if constexpr (requires { table.clCommandFillBufferKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCommandFillBufferKHR)>::value) {
            if (table.clCommandFillBufferKHR == nullptr)
                table.clCommandFillBufferKHR = &Refuse<decltype(table.clCommandFillBufferKHR)>::call;
        }
    }
    if constexpr (requires { table.clCommandFillImageKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCommandFillImageKHR)>::value) {
            if (table.clCommandFillImageKHR == nullptr)
                table.clCommandFillImageKHR = &Refuse<decltype(table.clCommandFillImageKHR)>::call;
        }
    }
    if constexpr (requires { table.clCommandNDRangeKernelKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCommandNDRangeKernelKHR)>::value) {
            if (table.clCommandNDRangeKernelKHR == nullptr)
                table.clCommandNDRangeKernelKHR = &Refuse<decltype(table.clCommandNDRangeKernelKHR)>::call;
        }
    }
    if constexpr (requires { table.clCommandSVMMemcpyKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCommandSVMMemcpyKHR)>::value) {
            if (table.clCommandSVMMemcpyKHR == nullptr)
                table.clCommandSVMMemcpyKHR = &Refuse<decltype(table.clCommandSVMMemcpyKHR)>::call;
        }
    }
    if constexpr (requires { table.clCommandSVMMemFillKHR; }) {
        if constexpr (IsRefusable<decltype(table.clCommandSVMMemFillKHR)>::value) {
            if (table.clCommandSVMMemFillKHR == nullptr)
                table.clCommandSVMMemFillKHR = &Refuse<decltype(table.clCommandSVMMemFillKHR)>::call;
        }
    }
    if constexpr (requires { table.clGetCommandBufferInfoKHR; }) {
        if constexpr (IsRefusable<decltype(table.clGetCommandBufferInfoKHR)>::value) {
            if (table.clGetCommandBufferInfoKHR == nullptr)
                table.clGetCommandBufferInfoKHR = &Refuse<decltype(table.clGetCommandBufferInfoKHR)>::call;
        }
    }
    if constexpr (requires { table.clUpdateMutableCommandsKHR; }) {
        if constexpr (IsRefusable<decltype(table.clUpdateMutableCommandsKHR)>::value) {
            if (table.clUpdateMutableCommandsKHR == nullptr)
                table.clUpdateMutableCommandsKHR = &Refuse<decltype(table.clUpdateMutableCommandsKHR)>::call;
        }
    }
    if constexpr (requires { table.clGetMutableCommandInfoKHR; }) {
        if constexpr (IsRefusable<decltype(table.clGetMutableCommandInfoKHR)>::value) {
            if (table.clGetMutableCommandInfoKHR == nullptr)
                table.clGetMutableCommandInfoKHR = &Refuse<decltype(table.clGetMutableCommandInfoKHR)>::call;
        }
    }
    if constexpr (requires { table.clRemapCommandBufferKHR; }) {
        if constexpr (IsRefusable<decltype(table.clRemapCommandBufferKHR)>::value) {
            if (table.clRemapCommandBufferKHR == nullptr)
                table.clRemapCommandBufferKHR = &Refuse<decltype(table.clRemapCommandBufferKHR)>::call;
        }
    }
    if constexpr (requires { table.clSetContentSizeBufferPoCL; }) {
        if constexpr (IsRefusable<decltype(table.clSetContentSizeBufferPoCL)>::value) {
            if (table.clSetContentSizeBufferPoCL == nullptr)
                table.clSetContentSizeBufferPoCL = &Refuse<decltype(table.clSetContentSizeBufferPoCL)>::call;
        }
    }
    if constexpr (requires { table.clGetPlatformIDs; }) {
        if constexpr (std::is_same_v<decltype(table.clGetPlatformIDs),
                                     decltype(&stubs::clGetPlatformIDs)>) {
            if (table.clGetPlatformIDs == nullptr) table.clGetPlatformIDs = &stubs::clGetPlatformIDs;
        } else if constexpr (IsRefusable<decltype(table.clGetPlatformIDs)>::value) {
            if (table.clGetPlatformIDs == nullptr)
                table.clGetPlatformIDs = &Refuse<decltype(table.clGetPlatformIDs)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetPlatformIDs), void*>) {
            if (table.clGetPlatformIDs == nullptr)
                table.clGetPlatformIDs = reinterpret_cast<void*>(&stubs::clGetPlatformIDs);
        }
    }
    if constexpr (requires { table.clGetPlatformInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetPlatformInfo),
                                     decltype(&stubs::clGetPlatformInfo)>) {
            if (table.clGetPlatformInfo == nullptr) table.clGetPlatformInfo = &stubs::clGetPlatformInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetPlatformInfo)>::value) {
            if (table.clGetPlatformInfo == nullptr)
                table.clGetPlatformInfo = &Refuse<decltype(table.clGetPlatformInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetPlatformInfo), void*>) {
            if (table.clGetPlatformInfo == nullptr)
                table.clGetPlatformInfo = reinterpret_cast<void*>(&stubs::clGetPlatformInfo);
        }
    }
    if constexpr (requires { table.clGetDeviceIDs; }) {
        if constexpr (std::is_same_v<decltype(table.clGetDeviceIDs),
                                     decltype(&stubs::clGetDeviceIDs)>) {
            if (table.clGetDeviceIDs == nullptr) table.clGetDeviceIDs = &stubs::clGetDeviceIDs;
        } else if constexpr (IsRefusable<decltype(table.clGetDeviceIDs)>::value) {
            if (table.clGetDeviceIDs == nullptr)
                table.clGetDeviceIDs = &Refuse<decltype(table.clGetDeviceIDs)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetDeviceIDs), void*>) {
            if (table.clGetDeviceIDs == nullptr)
                table.clGetDeviceIDs = reinterpret_cast<void*>(&stubs::clGetDeviceIDs);
        }
    }
    if constexpr (requires { table.clGetDeviceInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetDeviceInfo),
                                     decltype(&stubs::clGetDeviceInfo)>) {
            if (table.clGetDeviceInfo == nullptr) table.clGetDeviceInfo = &stubs::clGetDeviceInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetDeviceInfo)>::value) {
            if (table.clGetDeviceInfo == nullptr)
                table.clGetDeviceInfo = &Refuse<decltype(table.clGetDeviceInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetDeviceInfo), void*>) {
            if (table.clGetDeviceInfo == nullptr)
                table.clGetDeviceInfo = reinterpret_cast<void*>(&stubs::clGetDeviceInfo);
        }
    }
    if constexpr (requires { table.clCreateSubDevices; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateSubDevices),
                                     decltype(&stubs::clCreateSubDevices)>) {
            if (table.clCreateSubDevices == nullptr) table.clCreateSubDevices = &stubs::clCreateSubDevices;
        } else if constexpr (IsRefusable<decltype(table.clCreateSubDevices)>::value) {
            if (table.clCreateSubDevices == nullptr)
                table.clCreateSubDevices = &Refuse<decltype(table.clCreateSubDevices)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateSubDevices), void*>) {
            if (table.clCreateSubDevices == nullptr)
                table.clCreateSubDevices = reinterpret_cast<void*>(&stubs::clCreateSubDevices);
        }
    }
    if constexpr (requires { table.clRetainDevice; }) {
        if constexpr (std::is_same_v<decltype(table.clRetainDevice),
                                     decltype(&stubs::clRetainDevice)>) {
            if (table.clRetainDevice == nullptr) table.clRetainDevice = &stubs::clRetainDevice;
        } else if constexpr (IsRefusable<decltype(table.clRetainDevice)>::value) {
            if (table.clRetainDevice == nullptr)
                table.clRetainDevice = &Refuse<decltype(table.clRetainDevice)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clRetainDevice), void*>) {
            if (table.clRetainDevice == nullptr)
                table.clRetainDevice = reinterpret_cast<void*>(&stubs::clRetainDevice);
        }
    }
    if constexpr (requires { table.clReleaseDevice; }) {
        if constexpr (std::is_same_v<decltype(table.clReleaseDevice),
                                     decltype(&stubs::clReleaseDevice)>) {
            if (table.clReleaseDevice == nullptr) table.clReleaseDevice = &stubs::clReleaseDevice;
        } else if constexpr (IsRefusable<decltype(table.clReleaseDevice)>::value) {
            if (table.clReleaseDevice == nullptr)
                table.clReleaseDevice = &Refuse<decltype(table.clReleaseDevice)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clReleaseDevice), void*>) {
            if (table.clReleaseDevice == nullptr)
                table.clReleaseDevice = reinterpret_cast<void*>(&stubs::clReleaseDevice);
        }
    }
    if constexpr (requires { table.clSetDefaultDeviceCommandQueue; }) {
        if constexpr (std::is_same_v<decltype(table.clSetDefaultDeviceCommandQueue),
                                     decltype(&stubs::clSetDefaultDeviceCommandQueue)>) {
            if (table.clSetDefaultDeviceCommandQueue == nullptr) table.clSetDefaultDeviceCommandQueue = &stubs::clSetDefaultDeviceCommandQueue;
        } else if constexpr (IsRefusable<decltype(table.clSetDefaultDeviceCommandQueue)>::value) {
            if (table.clSetDefaultDeviceCommandQueue == nullptr)
                table.clSetDefaultDeviceCommandQueue = &Refuse<decltype(table.clSetDefaultDeviceCommandQueue)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSetDefaultDeviceCommandQueue), void*>) {
            if (table.clSetDefaultDeviceCommandQueue == nullptr)
                table.clSetDefaultDeviceCommandQueue = reinterpret_cast<void*>(&stubs::clSetDefaultDeviceCommandQueue);
        }
    }
    if constexpr (requires { table.clGetDeviceAndHostTimer; }) {
        if constexpr (std::is_same_v<decltype(table.clGetDeviceAndHostTimer),
                                     decltype(&stubs::clGetDeviceAndHostTimer)>) {
            if (table.clGetDeviceAndHostTimer == nullptr) table.clGetDeviceAndHostTimer = &stubs::clGetDeviceAndHostTimer;
        } else if constexpr (IsRefusable<decltype(table.clGetDeviceAndHostTimer)>::value) {
            if (table.clGetDeviceAndHostTimer == nullptr)
                table.clGetDeviceAndHostTimer = &Refuse<decltype(table.clGetDeviceAndHostTimer)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetDeviceAndHostTimer), void*>) {
            if (table.clGetDeviceAndHostTimer == nullptr)
                table.clGetDeviceAndHostTimer = reinterpret_cast<void*>(&stubs::clGetDeviceAndHostTimer);
        }
    }
    if constexpr (requires { table.clGetHostTimer; }) {
        if constexpr (std::is_same_v<decltype(table.clGetHostTimer),
                                     decltype(&stubs::clGetHostTimer)>) {
            if (table.clGetHostTimer == nullptr) table.clGetHostTimer = &stubs::clGetHostTimer;
        } else if constexpr (IsRefusable<decltype(table.clGetHostTimer)>::value) {
            if (table.clGetHostTimer == nullptr)
                table.clGetHostTimer = &Refuse<decltype(table.clGetHostTimer)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetHostTimer), void*>) {
            if (table.clGetHostTimer == nullptr)
                table.clGetHostTimer = reinterpret_cast<void*>(&stubs::clGetHostTimer);
        }
    }
    if constexpr (requires { table.clCreateContext; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateContext),
                                     decltype(&stubs::clCreateContext)>) {
            if (table.clCreateContext == nullptr) table.clCreateContext = &stubs::clCreateContext;
        } else if constexpr (IsRefusable<decltype(table.clCreateContext)>::value) {
            if (table.clCreateContext == nullptr)
                table.clCreateContext = &Refuse<decltype(table.clCreateContext)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateContext), void*>) {
            if (table.clCreateContext == nullptr)
                table.clCreateContext = reinterpret_cast<void*>(&stubs::clCreateContext);
        }
    }
    if constexpr (requires { table.clCreateContextFromType; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateContextFromType),
                                     decltype(&stubs::clCreateContextFromType)>) {
            if (table.clCreateContextFromType == nullptr) table.clCreateContextFromType = &stubs::clCreateContextFromType;
        } else if constexpr (IsRefusable<decltype(table.clCreateContextFromType)>::value) {
            if (table.clCreateContextFromType == nullptr)
                table.clCreateContextFromType = &Refuse<decltype(table.clCreateContextFromType)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateContextFromType), void*>) {
            if (table.clCreateContextFromType == nullptr)
                table.clCreateContextFromType = reinterpret_cast<void*>(&stubs::clCreateContextFromType);
        }
    }
    if constexpr (requires { table.clRetainContext; }) {
        if constexpr (std::is_same_v<decltype(table.clRetainContext),
                                     decltype(&stubs::clRetainContext)>) {
            if (table.clRetainContext == nullptr) table.clRetainContext = &stubs::clRetainContext;
        } else if constexpr (IsRefusable<decltype(table.clRetainContext)>::value) {
            if (table.clRetainContext == nullptr)
                table.clRetainContext = &Refuse<decltype(table.clRetainContext)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clRetainContext), void*>) {
            if (table.clRetainContext == nullptr)
                table.clRetainContext = reinterpret_cast<void*>(&stubs::clRetainContext);
        }
    }
    if constexpr (requires { table.clReleaseContext; }) {
        if constexpr (std::is_same_v<decltype(table.clReleaseContext),
                                     decltype(&stubs::clReleaseContext)>) {
            if (table.clReleaseContext == nullptr) table.clReleaseContext = &stubs::clReleaseContext;
        } else if constexpr (IsRefusable<decltype(table.clReleaseContext)>::value) {
            if (table.clReleaseContext == nullptr)
                table.clReleaseContext = &Refuse<decltype(table.clReleaseContext)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clReleaseContext), void*>) {
            if (table.clReleaseContext == nullptr)
                table.clReleaseContext = reinterpret_cast<void*>(&stubs::clReleaseContext);
        }
    }
    if constexpr (requires { table.clGetContextInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetContextInfo),
                                     decltype(&stubs::clGetContextInfo)>) {
            if (table.clGetContextInfo == nullptr) table.clGetContextInfo = &stubs::clGetContextInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetContextInfo)>::value) {
            if (table.clGetContextInfo == nullptr)
                table.clGetContextInfo = &Refuse<decltype(table.clGetContextInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetContextInfo), void*>) {
            if (table.clGetContextInfo == nullptr)
                table.clGetContextInfo = reinterpret_cast<void*>(&stubs::clGetContextInfo);
        }
    }
    if constexpr (requires { table.clSetContextDestructorCallback; }) {
        if constexpr (std::is_same_v<decltype(table.clSetContextDestructorCallback),
                                     decltype(&stubs::clSetContextDestructorCallback)>) {
            if (table.clSetContextDestructorCallback == nullptr) table.clSetContextDestructorCallback = &stubs::clSetContextDestructorCallback;
        } else if constexpr (IsRefusable<decltype(table.clSetContextDestructorCallback)>::value) {
            if (table.clSetContextDestructorCallback == nullptr)
                table.clSetContextDestructorCallback = &Refuse<decltype(table.clSetContextDestructorCallback)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSetContextDestructorCallback), void*>) {
            if (table.clSetContextDestructorCallback == nullptr)
                table.clSetContextDestructorCallback = reinterpret_cast<void*>(&stubs::clSetContextDestructorCallback);
        }
    }
    if constexpr (requires { table.clCreateCommandQueueWithProperties; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateCommandQueueWithProperties),
                                     decltype(&stubs::clCreateCommandQueueWithProperties)>) {
            if (table.clCreateCommandQueueWithProperties == nullptr) table.clCreateCommandQueueWithProperties = &stubs::clCreateCommandQueueWithProperties;
        } else if constexpr (IsRefusable<decltype(table.clCreateCommandQueueWithProperties)>::value) {
            if (table.clCreateCommandQueueWithProperties == nullptr)
                table.clCreateCommandQueueWithProperties = &Refuse<decltype(table.clCreateCommandQueueWithProperties)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateCommandQueueWithProperties), void*>) {
            if (table.clCreateCommandQueueWithProperties == nullptr)
                table.clCreateCommandQueueWithProperties = reinterpret_cast<void*>(&stubs::clCreateCommandQueueWithProperties);
        }
    }
    if constexpr (requires { table.clRetainCommandQueue; }) {
        if constexpr (std::is_same_v<decltype(table.clRetainCommandQueue),
                                     decltype(&stubs::clRetainCommandQueue)>) {
            if (table.clRetainCommandQueue == nullptr) table.clRetainCommandQueue = &stubs::clRetainCommandQueue;
        } else if constexpr (IsRefusable<decltype(table.clRetainCommandQueue)>::value) {
            if (table.clRetainCommandQueue == nullptr)
                table.clRetainCommandQueue = &Refuse<decltype(table.clRetainCommandQueue)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clRetainCommandQueue), void*>) {
            if (table.clRetainCommandQueue == nullptr)
                table.clRetainCommandQueue = reinterpret_cast<void*>(&stubs::clRetainCommandQueue);
        }
    }
    if constexpr (requires { table.clReleaseCommandQueue; }) {
        if constexpr (std::is_same_v<decltype(table.clReleaseCommandQueue),
                                     decltype(&stubs::clReleaseCommandQueue)>) {
            if (table.clReleaseCommandQueue == nullptr) table.clReleaseCommandQueue = &stubs::clReleaseCommandQueue;
        } else if constexpr (IsRefusable<decltype(table.clReleaseCommandQueue)>::value) {
            if (table.clReleaseCommandQueue == nullptr)
                table.clReleaseCommandQueue = &Refuse<decltype(table.clReleaseCommandQueue)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clReleaseCommandQueue), void*>) {
            if (table.clReleaseCommandQueue == nullptr)
                table.clReleaseCommandQueue = reinterpret_cast<void*>(&stubs::clReleaseCommandQueue);
        }
    }
    if constexpr (requires { table.clGetCommandQueueInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetCommandQueueInfo),
                                     decltype(&stubs::clGetCommandQueueInfo)>) {
            if (table.clGetCommandQueueInfo == nullptr) table.clGetCommandQueueInfo = &stubs::clGetCommandQueueInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetCommandQueueInfo)>::value) {
            if (table.clGetCommandQueueInfo == nullptr)
                table.clGetCommandQueueInfo = &Refuse<decltype(table.clGetCommandQueueInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetCommandQueueInfo), void*>) {
            if (table.clGetCommandQueueInfo == nullptr)
                table.clGetCommandQueueInfo = reinterpret_cast<void*>(&stubs::clGetCommandQueueInfo);
        }
    }
    if constexpr (requires { table.clCreateBuffer; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateBuffer),
                                     decltype(&stubs::clCreateBuffer)>) {
            if (table.clCreateBuffer == nullptr) table.clCreateBuffer = &stubs::clCreateBuffer;
        } else if constexpr (IsRefusable<decltype(table.clCreateBuffer)>::value) {
            if (table.clCreateBuffer == nullptr)
                table.clCreateBuffer = &Refuse<decltype(table.clCreateBuffer)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateBuffer), void*>) {
            if (table.clCreateBuffer == nullptr)
                table.clCreateBuffer = reinterpret_cast<void*>(&stubs::clCreateBuffer);
        }
    }
    if constexpr (requires { table.clCreateBufferWithProperties; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateBufferWithProperties),
                                     decltype(&stubs::clCreateBufferWithProperties)>) {
            if (table.clCreateBufferWithProperties == nullptr) table.clCreateBufferWithProperties = &stubs::clCreateBufferWithProperties;
        } else if constexpr (IsRefusable<decltype(table.clCreateBufferWithProperties)>::value) {
            if (table.clCreateBufferWithProperties == nullptr)
                table.clCreateBufferWithProperties = &Refuse<decltype(table.clCreateBufferWithProperties)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateBufferWithProperties), void*>) {
            if (table.clCreateBufferWithProperties == nullptr)
                table.clCreateBufferWithProperties = reinterpret_cast<void*>(&stubs::clCreateBufferWithProperties);
        }
    }
    if constexpr (requires { table.clCreateSubBuffer; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateSubBuffer),
                                     decltype(&stubs::clCreateSubBuffer)>) {
            if (table.clCreateSubBuffer == nullptr) table.clCreateSubBuffer = &stubs::clCreateSubBuffer;
        } else if constexpr (IsRefusable<decltype(table.clCreateSubBuffer)>::value) {
            if (table.clCreateSubBuffer == nullptr)
                table.clCreateSubBuffer = &Refuse<decltype(table.clCreateSubBuffer)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateSubBuffer), void*>) {
            if (table.clCreateSubBuffer == nullptr)
                table.clCreateSubBuffer = reinterpret_cast<void*>(&stubs::clCreateSubBuffer);
        }
    }
    if constexpr (requires { table.clCreateImage; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateImage),
                                     decltype(&stubs::clCreateImage)>) {
            if (table.clCreateImage == nullptr) table.clCreateImage = &stubs::clCreateImage;
        } else if constexpr (IsRefusable<decltype(table.clCreateImage)>::value) {
            if (table.clCreateImage == nullptr)
                table.clCreateImage = &Refuse<decltype(table.clCreateImage)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateImage), void*>) {
            if (table.clCreateImage == nullptr)
                table.clCreateImage = reinterpret_cast<void*>(&stubs::clCreateImage);
        }
    }
    if constexpr (requires { table.clCreateImageWithProperties; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateImageWithProperties),
                                     decltype(&stubs::clCreateImageWithProperties)>) {
            if (table.clCreateImageWithProperties == nullptr) table.clCreateImageWithProperties = &stubs::clCreateImageWithProperties;
        } else if constexpr (IsRefusable<decltype(table.clCreateImageWithProperties)>::value) {
            if (table.clCreateImageWithProperties == nullptr)
                table.clCreateImageWithProperties = &Refuse<decltype(table.clCreateImageWithProperties)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateImageWithProperties), void*>) {
            if (table.clCreateImageWithProperties == nullptr)
                table.clCreateImageWithProperties = reinterpret_cast<void*>(&stubs::clCreateImageWithProperties);
        }
    }
    if constexpr (requires { table.clCreatePipe; }) {
        if constexpr (std::is_same_v<decltype(table.clCreatePipe),
                                     decltype(&stubs::clCreatePipe)>) {
            if (table.clCreatePipe == nullptr) table.clCreatePipe = &stubs::clCreatePipe;
        } else if constexpr (IsRefusable<decltype(table.clCreatePipe)>::value) {
            if (table.clCreatePipe == nullptr)
                table.clCreatePipe = &Refuse<decltype(table.clCreatePipe)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreatePipe), void*>) {
            if (table.clCreatePipe == nullptr)
                table.clCreatePipe = reinterpret_cast<void*>(&stubs::clCreatePipe);
        }
    }
    if constexpr (requires { table.clRetainMemObject; }) {
        if constexpr (std::is_same_v<decltype(table.clRetainMemObject),
                                     decltype(&stubs::clRetainMemObject)>) {
            if (table.clRetainMemObject == nullptr) table.clRetainMemObject = &stubs::clRetainMemObject;
        } else if constexpr (IsRefusable<decltype(table.clRetainMemObject)>::value) {
            if (table.clRetainMemObject == nullptr)
                table.clRetainMemObject = &Refuse<decltype(table.clRetainMemObject)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clRetainMemObject), void*>) {
            if (table.clRetainMemObject == nullptr)
                table.clRetainMemObject = reinterpret_cast<void*>(&stubs::clRetainMemObject);
        }
    }
    if constexpr (requires { table.clReleaseMemObject; }) {
        if constexpr (std::is_same_v<decltype(table.clReleaseMemObject),
                                     decltype(&stubs::clReleaseMemObject)>) {
            if (table.clReleaseMemObject == nullptr) table.clReleaseMemObject = &stubs::clReleaseMemObject;
        } else if constexpr (IsRefusable<decltype(table.clReleaseMemObject)>::value) {
            if (table.clReleaseMemObject == nullptr)
                table.clReleaseMemObject = &Refuse<decltype(table.clReleaseMemObject)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clReleaseMemObject), void*>) {
            if (table.clReleaseMemObject == nullptr)
                table.clReleaseMemObject = reinterpret_cast<void*>(&stubs::clReleaseMemObject);
        }
    }
    if constexpr (requires { table.clGetSupportedImageFormats; }) {
        if constexpr (std::is_same_v<decltype(table.clGetSupportedImageFormats),
                                     decltype(&stubs::clGetSupportedImageFormats)>) {
            if (table.clGetSupportedImageFormats == nullptr) table.clGetSupportedImageFormats = &stubs::clGetSupportedImageFormats;
        } else if constexpr (IsRefusable<decltype(table.clGetSupportedImageFormats)>::value) {
            if (table.clGetSupportedImageFormats == nullptr)
                table.clGetSupportedImageFormats = &Refuse<decltype(table.clGetSupportedImageFormats)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetSupportedImageFormats), void*>) {
            if (table.clGetSupportedImageFormats == nullptr)
                table.clGetSupportedImageFormats = reinterpret_cast<void*>(&stubs::clGetSupportedImageFormats);
        }
    }
    if constexpr (requires { table.clGetMemObjectInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetMemObjectInfo),
                                     decltype(&stubs::clGetMemObjectInfo)>) {
            if (table.clGetMemObjectInfo == nullptr) table.clGetMemObjectInfo = &stubs::clGetMemObjectInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetMemObjectInfo)>::value) {
            if (table.clGetMemObjectInfo == nullptr)
                table.clGetMemObjectInfo = &Refuse<decltype(table.clGetMemObjectInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetMemObjectInfo), void*>) {
            if (table.clGetMemObjectInfo == nullptr)
                table.clGetMemObjectInfo = reinterpret_cast<void*>(&stubs::clGetMemObjectInfo);
        }
    }
    if constexpr (requires { table.clGetImageInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetImageInfo),
                                     decltype(&stubs::clGetImageInfo)>) {
            if (table.clGetImageInfo == nullptr) table.clGetImageInfo = &stubs::clGetImageInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetImageInfo)>::value) {
            if (table.clGetImageInfo == nullptr)
                table.clGetImageInfo = &Refuse<decltype(table.clGetImageInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetImageInfo), void*>) {
            if (table.clGetImageInfo == nullptr)
                table.clGetImageInfo = reinterpret_cast<void*>(&stubs::clGetImageInfo);
        }
    }
    if constexpr (requires { table.clGetPipeInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetPipeInfo),
                                     decltype(&stubs::clGetPipeInfo)>) {
            if (table.clGetPipeInfo == nullptr) table.clGetPipeInfo = &stubs::clGetPipeInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetPipeInfo)>::value) {
            if (table.clGetPipeInfo == nullptr)
                table.clGetPipeInfo = &Refuse<decltype(table.clGetPipeInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetPipeInfo), void*>) {
            if (table.clGetPipeInfo == nullptr)
                table.clGetPipeInfo = reinterpret_cast<void*>(&stubs::clGetPipeInfo);
        }
    }
    if constexpr (requires { table.clSetMemObjectDestructorCallback; }) {
        if constexpr (std::is_same_v<decltype(table.clSetMemObjectDestructorCallback),
                                     decltype(&stubs::clSetMemObjectDestructorCallback)>) {
            if (table.clSetMemObjectDestructorCallback == nullptr) table.clSetMemObjectDestructorCallback = &stubs::clSetMemObjectDestructorCallback;
        } else if constexpr (IsRefusable<decltype(table.clSetMemObjectDestructorCallback)>::value) {
            if (table.clSetMemObjectDestructorCallback == nullptr)
                table.clSetMemObjectDestructorCallback = &Refuse<decltype(table.clSetMemObjectDestructorCallback)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSetMemObjectDestructorCallback), void*>) {
            if (table.clSetMemObjectDestructorCallback == nullptr)
                table.clSetMemObjectDestructorCallback = reinterpret_cast<void*>(&stubs::clSetMemObjectDestructorCallback);
        }
    }
    if constexpr (requires { table.clSetMemObjectDestructorAPPLE; }) {
        if constexpr (IsRefusable<decltype(table.clSetMemObjectDestructorAPPLE)>::value) {
            if (table.clSetMemObjectDestructorAPPLE == nullptr)
                table.clSetMemObjectDestructorAPPLE = &Refuse<decltype(table.clSetMemObjectDestructorAPPLE)>::call;
        }
    }
    if constexpr (requires { table.clSVMAlloc; }) {
        if constexpr (std::is_same_v<decltype(table.clSVMAlloc),
                                     decltype(&stubs::clSVMAlloc)>) {
            if (table.clSVMAlloc == nullptr) table.clSVMAlloc = &stubs::clSVMAlloc;
        } else if constexpr (IsRefusable<decltype(table.clSVMAlloc)>::value) {
            if (table.clSVMAlloc == nullptr)
                table.clSVMAlloc = &Refuse<decltype(table.clSVMAlloc)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSVMAlloc), void*>) {
            if (table.clSVMAlloc == nullptr)
                table.clSVMAlloc = reinterpret_cast<void*>(&stubs::clSVMAlloc);
        }
    }
    if constexpr (requires { table.clSVMFree; }) {
        if constexpr (std::is_same_v<decltype(table.clSVMFree),
                                     decltype(&stubs::clSVMFree)>) {
            if (table.clSVMFree == nullptr) table.clSVMFree = &stubs::clSVMFree;
        } else if constexpr (IsRefusable<decltype(table.clSVMFree)>::value) {
            if (table.clSVMFree == nullptr)
                table.clSVMFree = &Refuse<decltype(table.clSVMFree)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSVMFree), void*>) {
            if (table.clSVMFree == nullptr)
                table.clSVMFree = reinterpret_cast<void*>(&stubs::clSVMFree);
        }
    }
    if constexpr (requires { table.clCreateSamplerWithProperties; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateSamplerWithProperties),
                                     decltype(&stubs::clCreateSamplerWithProperties)>) {
            if (table.clCreateSamplerWithProperties == nullptr) table.clCreateSamplerWithProperties = &stubs::clCreateSamplerWithProperties;
        } else if constexpr (IsRefusable<decltype(table.clCreateSamplerWithProperties)>::value) {
            if (table.clCreateSamplerWithProperties == nullptr)
                table.clCreateSamplerWithProperties = &Refuse<decltype(table.clCreateSamplerWithProperties)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateSamplerWithProperties), void*>) {
            if (table.clCreateSamplerWithProperties == nullptr)
                table.clCreateSamplerWithProperties = reinterpret_cast<void*>(&stubs::clCreateSamplerWithProperties);
        }
    }
    if constexpr (requires { table.clRetainSampler; }) {
        if constexpr (std::is_same_v<decltype(table.clRetainSampler),
                                     decltype(&stubs::clRetainSampler)>) {
            if (table.clRetainSampler == nullptr) table.clRetainSampler = &stubs::clRetainSampler;
        } else if constexpr (IsRefusable<decltype(table.clRetainSampler)>::value) {
            if (table.clRetainSampler == nullptr)
                table.clRetainSampler = &Refuse<decltype(table.clRetainSampler)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clRetainSampler), void*>) {
            if (table.clRetainSampler == nullptr)
                table.clRetainSampler = reinterpret_cast<void*>(&stubs::clRetainSampler);
        }
    }
    if constexpr (requires { table.clReleaseSampler; }) {
        if constexpr (std::is_same_v<decltype(table.clReleaseSampler),
                                     decltype(&stubs::clReleaseSampler)>) {
            if (table.clReleaseSampler == nullptr) table.clReleaseSampler = &stubs::clReleaseSampler;
        } else if constexpr (IsRefusable<decltype(table.clReleaseSampler)>::value) {
            if (table.clReleaseSampler == nullptr)
                table.clReleaseSampler = &Refuse<decltype(table.clReleaseSampler)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clReleaseSampler), void*>) {
            if (table.clReleaseSampler == nullptr)
                table.clReleaseSampler = reinterpret_cast<void*>(&stubs::clReleaseSampler);
        }
    }
    if constexpr (requires { table.clGetSamplerInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetSamplerInfo),
                                     decltype(&stubs::clGetSamplerInfo)>) {
            if (table.clGetSamplerInfo == nullptr) table.clGetSamplerInfo = &stubs::clGetSamplerInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetSamplerInfo)>::value) {
            if (table.clGetSamplerInfo == nullptr)
                table.clGetSamplerInfo = &Refuse<decltype(table.clGetSamplerInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetSamplerInfo), void*>) {
            if (table.clGetSamplerInfo == nullptr)
                table.clGetSamplerInfo = reinterpret_cast<void*>(&stubs::clGetSamplerInfo);
        }
    }
    if constexpr (requires { table.clCreateProgramWithSource; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateProgramWithSource),
                                     decltype(&stubs::clCreateProgramWithSource)>) {
            if (table.clCreateProgramWithSource == nullptr) table.clCreateProgramWithSource = &stubs::clCreateProgramWithSource;
        } else if constexpr (IsRefusable<decltype(table.clCreateProgramWithSource)>::value) {
            if (table.clCreateProgramWithSource == nullptr)
                table.clCreateProgramWithSource = &Refuse<decltype(table.clCreateProgramWithSource)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateProgramWithSource), void*>) {
            if (table.clCreateProgramWithSource == nullptr)
                table.clCreateProgramWithSource = reinterpret_cast<void*>(&stubs::clCreateProgramWithSource);
        }
    }
    if constexpr (requires { table.clCreateProgramWithBinary; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateProgramWithBinary),
                                     decltype(&stubs::clCreateProgramWithBinary)>) {
            if (table.clCreateProgramWithBinary == nullptr) table.clCreateProgramWithBinary = &stubs::clCreateProgramWithBinary;
        } else if constexpr (IsRefusable<decltype(table.clCreateProgramWithBinary)>::value) {
            if (table.clCreateProgramWithBinary == nullptr)
                table.clCreateProgramWithBinary = &Refuse<decltype(table.clCreateProgramWithBinary)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateProgramWithBinary), void*>) {
            if (table.clCreateProgramWithBinary == nullptr)
                table.clCreateProgramWithBinary = reinterpret_cast<void*>(&stubs::clCreateProgramWithBinary);
        }
    }
    if constexpr (requires { table.clCreateProgramWithBuiltInKernels; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateProgramWithBuiltInKernels),
                                     decltype(&stubs::clCreateProgramWithBuiltInKernels)>) {
            if (table.clCreateProgramWithBuiltInKernels == nullptr) table.clCreateProgramWithBuiltInKernels = &stubs::clCreateProgramWithBuiltInKernels;
        } else if constexpr (IsRefusable<decltype(table.clCreateProgramWithBuiltInKernels)>::value) {
            if (table.clCreateProgramWithBuiltInKernels == nullptr)
                table.clCreateProgramWithBuiltInKernels = &Refuse<decltype(table.clCreateProgramWithBuiltInKernels)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateProgramWithBuiltInKernels), void*>) {
            if (table.clCreateProgramWithBuiltInKernels == nullptr)
                table.clCreateProgramWithBuiltInKernels = reinterpret_cast<void*>(&stubs::clCreateProgramWithBuiltInKernels);
        }
    }
    if constexpr (requires { table.clCreateProgramWithIL; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateProgramWithIL),
                                     decltype(&stubs::clCreateProgramWithIL)>) {
            if (table.clCreateProgramWithIL == nullptr) table.clCreateProgramWithIL = &stubs::clCreateProgramWithIL;
        } else if constexpr (IsRefusable<decltype(table.clCreateProgramWithIL)>::value) {
            if (table.clCreateProgramWithIL == nullptr)
                table.clCreateProgramWithIL = &Refuse<decltype(table.clCreateProgramWithIL)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateProgramWithIL), void*>) {
            if (table.clCreateProgramWithIL == nullptr)
                table.clCreateProgramWithIL = reinterpret_cast<void*>(&stubs::clCreateProgramWithIL);
        }
    }
    if constexpr (requires { table.clRetainProgram; }) {
        if constexpr (std::is_same_v<decltype(table.clRetainProgram),
                                     decltype(&stubs::clRetainProgram)>) {
            if (table.clRetainProgram == nullptr) table.clRetainProgram = &stubs::clRetainProgram;
        } else if constexpr (IsRefusable<decltype(table.clRetainProgram)>::value) {
            if (table.clRetainProgram == nullptr)
                table.clRetainProgram = &Refuse<decltype(table.clRetainProgram)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clRetainProgram), void*>) {
            if (table.clRetainProgram == nullptr)
                table.clRetainProgram = reinterpret_cast<void*>(&stubs::clRetainProgram);
        }
    }
    if constexpr (requires { table.clReleaseProgram; }) {
        if constexpr (std::is_same_v<decltype(table.clReleaseProgram),
                                     decltype(&stubs::clReleaseProgram)>) {
            if (table.clReleaseProgram == nullptr) table.clReleaseProgram = &stubs::clReleaseProgram;
        } else if constexpr (IsRefusable<decltype(table.clReleaseProgram)>::value) {
            if (table.clReleaseProgram == nullptr)
                table.clReleaseProgram = &Refuse<decltype(table.clReleaseProgram)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clReleaseProgram), void*>) {
            if (table.clReleaseProgram == nullptr)
                table.clReleaseProgram = reinterpret_cast<void*>(&stubs::clReleaseProgram);
        }
    }
    if constexpr (requires { table.clBuildProgram; }) {
        if constexpr (std::is_same_v<decltype(table.clBuildProgram),
                                     decltype(&stubs::clBuildProgram)>) {
            if (table.clBuildProgram == nullptr) table.clBuildProgram = &stubs::clBuildProgram;
        } else if constexpr (IsRefusable<decltype(table.clBuildProgram)>::value) {
            if (table.clBuildProgram == nullptr)
                table.clBuildProgram = &Refuse<decltype(table.clBuildProgram)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clBuildProgram), void*>) {
            if (table.clBuildProgram == nullptr)
                table.clBuildProgram = reinterpret_cast<void*>(&stubs::clBuildProgram);
        }
    }
    if constexpr (requires { table.clCompileProgram; }) {
        if constexpr (std::is_same_v<decltype(table.clCompileProgram),
                                     decltype(&stubs::clCompileProgram)>) {
            if (table.clCompileProgram == nullptr) table.clCompileProgram = &stubs::clCompileProgram;
        } else if constexpr (IsRefusable<decltype(table.clCompileProgram)>::value) {
            if (table.clCompileProgram == nullptr)
                table.clCompileProgram = &Refuse<decltype(table.clCompileProgram)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCompileProgram), void*>) {
            if (table.clCompileProgram == nullptr)
                table.clCompileProgram = reinterpret_cast<void*>(&stubs::clCompileProgram);
        }
    }
    if constexpr (requires { table.clLinkProgram; }) {
        if constexpr (std::is_same_v<decltype(table.clLinkProgram),
                                     decltype(&stubs::clLinkProgram)>) {
            if (table.clLinkProgram == nullptr) table.clLinkProgram = &stubs::clLinkProgram;
        } else if constexpr (IsRefusable<decltype(table.clLinkProgram)>::value) {
            if (table.clLinkProgram == nullptr)
                table.clLinkProgram = &Refuse<decltype(table.clLinkProgram)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clLinkProgram), void*>) {
            if (table.clLinkProgram == nullptr)
                table.clLinkProgram = reinterpret_cast<void*>(&stubs::clLinkProgram);
        }
    }
    if constexpr (requires { table.clSetProgramReleaseCallback; }) {
        if constexpr (std::is_same_v<decltype(table.clSetProgramReleaseCallback),
                                     decltype(&stubs::clSetProgramReleaseCallback)>) {
            if (table.clSetProgramReleaseCallback == nullptr) table.clSetProgramReleaseCallback = &stubs::clSetProgramReleaseCallback;
        } else if constexpr (IsRefusable<decltype(table.clSetProgramReleaseCallback)>::value) {
            if (table.clSetProgramReleaseCallback == nullptr)
                table.clSetProgramReleaseCallback = &Refuse<decltype(table.clSetProgramReleaseCallback)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSetProgramReleaseCallback), void*>) {
            if (table.clSetProgramReleaseCallback == nullptr)
                table.clSetProgramReleaseCallback = reinterpret_cast<void*>(&stubs::clSetProgramReleaseCallback);
        }
    }
    if constexpr (requires { table.clSetProgramSpecializationConstant; }) {
        if constexpr (std::is_same_v<decltype(table.clSetProgramSpecializationConstant),
                                     decltype(&stubs::clSetProgramSpecializationConstant)>) {
            if (table.clSetProgramSpecializationConstant == nullptr) table.clSetProgramSpecializationConstant = &stubs::clSetProgramSpecializationConstant;
        } else if constexpr (IsRefusable<decltype(table.clSetProgramSpecializationConstant)>::value) {
            if (table.clSetProgramSpecializationConstant == nullptr)
                table.clSetProgramSpecializationConstant = &Refuse<decltype(table.clSetProgramSpecializationConstant)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSetProgramSpecializationConstant), void*>) {
            if (table.clSetProgramSpecializationConstant == nullptr)
                table.clSetProgramSpecializationConstant = reinterpret_cast<void*>(&stubs::clSetProgramSpecializationConstant);
        }
    }
    if constexpr (requires { table.clUnloadPlatformCompiler; }) {
        if constexpr (std::is_same_v<decltype(table.clUnloadPlatformCompiler),
                                     decltype(&stubs::clUnloadPlatformCompiler)>) {
            if (table.clUnloadPlatformCompiler == nullptr) table.clUnloadPlatformCompiler = &stubs::clUnloadPlatformCompiler;
        } else if constexpr (IsRefusable<decltype(table.clUnloadPlatformCompiler)>::value) {
            if (table.clUnloadPlatformCompiler == nullptr)
                table.clUnloadPlatformCompiler = &Refuse<decltype(table.clUnloadPlatformCompiler)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clUnloadPlatformCompiler), void*>) {
            if (table.clUnloadPlatformCompiler == nullptr)
                table.clUnloadPlatformCompiler = reinterpret_cast<void*>(&stubs::clUnloadPlatformCompiler);
        }
    }
    if constexpr (requires { table.clGetProgramInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetProgramInfo),
                                     decltype(&stubs::clGetProgramInfo)>) {
            if (table.clGetProgramInfo == nullptr) table.clGetProgramInfo = &stubs::clGetProgramInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetProgramInfo)>::value) {
            if (table.clGetProgramInfo == nullptr)
                table.clGetProgramInfo = &Refuse<decltype(table.clGetProgramInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetProgramInfo), void*>) {
            if (table.clGetProgramInfo == nullptr)
                table.clGetProgramInfo = reinterpret_cast<void*>(&stubs::clGetProgramInfo);
        }
    }
    if constexpr (requires { table.clGetProgramBuildInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetProgramBuildInfo),
                                     decltype(&stubs::clGetProgramBuildInfo)>) {
            if (table.clGetProgramBuildInfo == nullptr) table.clGetProgramBuildInfo = &stubs::clGetProgramBuildInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetProgramBuildInfo)>::value) {
            if (table.clGetProgramBuildInfo == nullptr)
                table.clGetProgramBuildInfo = &Refuse<decltype(table.clGetProgramBuildInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetProgramBuildInfo), void*>) {
            if (table.clGetProgramBuildInfo == nullptr)
                table.clGetProgramBuildInfo = reinterpret_cast<void*>(&stubs::clGetProgramBuildInfo);
        }
    }
    if constexpr (requires { table.clCreateKernel; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateKernel),
                                     decltype(&stubs::clCreateKernel)>) {
            if (table.clCreateKernel == nullptr) table.clCreateKernel = &stubs::clCreateKernel;
        } else if constexpr (IsRefusable<decltype(table.clCreateKernel)>::value) {
            if (table.clCreateKernel == nullptr)
                table.clCreateKernel = &Refuse<decltype(table.clCreateKernel)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateKernel), void*>) {
            if (table.clCreateKernel == nullptr)
                table.clCreateKernel = reinterpret_cast<void*>(&stubs::clCreateKernel);
        }
    }
    if constexpr (requires { table.clCreateKernelsInProgram; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateKernelsInProgram),
                                     decltype(&stubs::clCreateKernelsInProgram)>) {
            if (table.clCreateKernelsInProgram == nullptr) table.clCreateKernelsInProgram = &stubs::clCreateKernelsInProgram;
        } else if constexpr (IsRefusable<decltype(table.clCreateKernelsInProgram)>::value) {
            if (table.clCreateKernelsInProgram == nullptr)
                table.clCreateKernelsInProgram = &Refuse<decltype(table.clCreateKernelsInProgram)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateKernelsInProgram), void*>) {
            if (table.clCreateKernelsInProgram == nullptr)
                table.clCreateKernelsInProgram = reinterpret_cast<void*>(&stubs::clCreateKernelsInProgram);
        }
    }
    if constexpr (requires { table.clCloneKernel; }) {
        if constexpr (std::is_same_v<decltype(table.clCloneKernel),
                                     decltype(&stubs::clCloneKernel)>) {
            if (table.clCloneKernel == nullptr) table.clCloneKernel = &stubs::clCloneKernel;
        } else if constexpr (IsRefusable<decltype(table.clCloneKernel)>::value) {
            if (table.clCloneKernel == nullptr)
                table.clCloneKernel = &Refuse<decltype(table.clCloneKernel)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCloneKernel), void*>) {
            if (table.clCloneKernel == nullptr)
                table.clCloneKernel = reinterpret_cast<void*>(&stubs::clCloneKernel);
        }
    }
    if constexpr (requires { table.clRetainKernel; }) {
        if constexpr (std::is_same_v<decltype(table.clRetainKernel),
                                     decltype(&stubs::clRetainKernel)>) {
            if (table.clRetainKernel == nullptr) table.clRetainKernel = &stubs::clRetainKernel;
        } else if constexpr (IsRefusable<decltype(table.clRetainKernel)>::value) {
            if (table.clRetainKernel == nullptr)
                table.clRetainKernel = &Refuse<decltype(table.clRetainKernel)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clRetainKernel), void*>) {
            if (table.clRetainKernel == nullptr)
                table.clRetainKernel = reinterpret_cast<void*>(&stubs::clRetainKernel);
        }
    }
    if constexpr (requires { table.clReleaseKernel; }) {
        if constexpr (std::is_same_v<decltype(table.clReleaseKernel),
                                     decltype(&stubs::clReleaseKernel)>) {
            if (table.clReleaseKernel == nullptr) table.clReleaseKernel = &stubs::clReleaseKernel;
        } else if constexpr (IsRefusable<decltype(table.clReleaseKernel)>::value) {
            if (table.clReleaseKernel == nullptr)
                table.clReleaseKernel = &Refuse<decltype(table.clReleaseKernel)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clReleaseKernel), void*>) {
            if (table.clReleaseKernel == nullptr)
                table.clReleaseKernel = reinterpret_cast<void*>(&stubs::clReleaseKernel);
        }
    }
    if constexpr (requires { table.clSetKernelArg; }) {
        if constexpr (std::is_same_v<decltype(table.clSetKernelArg),
                                     decltype(&stubs::clSetKernelArg)>) {
            if (table.clSetKernelArg == nullptr) table.clSetKernelArg = &stubs::clSetKernelArg;
        } else if constexpr (IsRefusable<decltype(table.clSetKernelArg)>::value) {
            if (table.clSetKernelArg == nullptr)
                table.clSetKernelArg = &Refuse<decltype(table.clSetKernelArg)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSetKernelArg), void*>) {
            if (table.clSetKernelArg == nullptr)
                table.clSetKernelArg = reinterpret_cast<void*>(&stubs::clSetKernelArg);
        }
    }
    if constexpr (requires { table.clSetKernelArgSVMPointer; }) {
        if constexpr (std::is_same_v<decltype(table.clSetKernelArgSVMPointer),
                                     decltype(&stubs::clSetKernelArgSVMPointer)>) {
            if (table.clSetKernelArgSVMPointer == nullptr) table.clSetKernelArgSVMPointer = &stubs::clSetKernelArgSVMPointer;
        } else if constexpr (IsRefusable<decltype(table.clSetKernelArgSVMPointer)>::value) {
            if (table.clSetKernelArgSVMPointer == nullptr)
                table.clSetKernelArgSVMPointer = &Refuse<decltype(table.clSetKernelArgSVMPointer)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSetKernelArgSVMPointer), void*>) {
            if (table.clSetKernelArgSVMPointer == nullptr)
                table.clSetKernelArgSVMPointer = reinterpret_cast<void*>(&stubs::clSetKernelArgSVMPointer);
        }
    }
    if constexpr (requires { table.clSetKernelArgDevicePointerEXT; }) {
        if constexpr (IsRefusable<decltype(table.clSetKernelArgDevicePointerEXT)>::value) {
            if (table.clSetKernelArgDevicePointerEXT == nullptr)
                table.clSetKernelArgDevicePointerEXT = &Refuse<decltype(table.clSetKernelArgDevicePointerEXT)>::call;
        }
    }
    if constexpr (requires { table.clSetKernelExecInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clSetKernelExecInfo),
                                     decltype(&stubs::clSetKernelExecInfo)>) {
            if (table.clSetKernelExecInfo == nullptr) table.clSetKernelExecInfo = &stubs::clSetKernelExecInfo;
        } else if constexpr (IsRefusable<decltype(table.clSetKernelExecInfo)>::value) {
            if (table.clSetKernelExecInfo == nullptr)
                table.clSetKernelExecInfo = &Refuse<decltype(table.clSetKernelExecInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSetKernelExecInfo), void*>) {
            if (table.clSetKernelExecInfo == nullptr)
                table.clSetKernelExecInfo = reinterpret_cast<void*>(&stubs::clSetKernelExecInfo);
        }
    }
    if constexpr (requires { table.clGetKernelInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetKernelInfo),
                                     decltype(&stubs::clGetKernelInfo)>) {
            if (table.clGetKernelInfo == nullptr) table.clGetKernelInfo = &stubs::clGetKernelInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetKernelInfo)>::value) {
            if (table.clGetKernelInfo == nullptr)
                table.clGetKernelInfo = &Refuse<decltype(table.clGetKernelInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetKernelInfo), void*>) {
            if (table.clGetKernelInfo == nullptr)
                table.clGetKernelInfo = reinterpret_cast<void*>(&stubs::clGetKernelInfo);
        }
    }
    if constexpr (requires { table.clGetKernelArgInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetKernelArgInfo),
                                     decltype(&stubs::clGetKernelArgInfo)>) {
            if (table.clGetKernelArgInfo == nullptr) table.clGetKernelArgInfo = &stubs::clGetKernelArgInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetKernelArgInfo)>::value) {
            if (table.clGetKernelArgInfo == nullptr)
                table.clGetKernelArgInfo = &Refuse<decltype(table.clGetKernelArgInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetKernelArgInfo), void*>) {
            if (table.clGetKernelArgInfo == nullptr)
                table.clGetKernelArgInfo = reinterpret_cast<void*>(&stubs::clGetKernelArgInfo);
        }
    }
    if constexpr (requires { table.clGetKernelWorkGroupInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetKernelWorkGroupInfo),
                                     decltype(&stubs::clGetKernelWorkGroupInfo)>) {
            if (table.clGetKernelWorkGroupInfo == nullptr) table.clGetKernelWorkGroupInfo = &stubs::clGetKernelWorkGroupInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetKernelWorkGroupInfo)>::value) {
            if (table.clGetKernelWorkGroupInfo == nullptr)
                table.clGetKernelWorkGroupInfo = &Refuse<decltype(table.clGetKernelWorkGroupInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetKernelWorkGroupInfo), void*>) {
            if (table.clGetKernelWorkGroupInfo == nullptr)
                table.clGetKernelWorkGroupInfo = reinterpret_cast<void*>(&stubs::clGetKernelWorkGroupInfo);
        }
    }
    if constexpr (requires { table.clGetKernelSubGroupInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetKernelSubGroupInfo),
                                     decltype(&stubs::clGetKernelSubGroupInfo)>) {
            if (table.clGetKernelSubGroupInfo == nullptr) table.clGetKernelSubGroupInfo = &stubs::clGetKernelSubGroupInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetKernelSubGroupInfo)>::value) {
            if (table.clGetKernelSubGroupInfo == nullptr)
                table.clGetKernelSubGroupInfo = &Refuse<decltype(table.clGetKernelSubGroupInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetKernelSubGroupInfo), void*>) {
            if (table.clGetKernelSubGroupInfo == nullptr)
                table.clGetKernelSubGroupInfo = reinterpret_cast<void*>(&stubs::clGetKernelSubGroupInfo);
        }
    }
    if constexpr (requires { table.clWaitForEvents; }) {
        if constexpr (std::is_same_v<decltype(table.clWaitForEvents),
                                     decltype(&stubs::clWaitForEvents)>) {
            if (table.clWaitForEvents == nullptr) table.clWaitForEvents = &stubs::clWaitForEvents;
        } else if constexpr (IsRefusable<decltype(table.clWaitForEvents)>::value) {
            if (table.clWaitForEvents == nullptr)
                table.clWaitForEvents = &Refuse<decltype(table.clWaitForEvents)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clWaitForEvents), void*>) {
            if (table.clWaitForEvents == nullptr)
                table.clWaitForEvents = reinterpret_cast<void*>(&stubs::clWaitForEvents);
        }
    }
    if constexpr (requires { table.clGetEventInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetEventInfo),
                                     decltype(&stubs::clGetEventInfo)>) {
            if (table.clGetEventInfo == nullptr) table.clGetEventInfo = &stubs::clGetEventInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetEventInfo)>::value) {
            if (table.clGetEventInfo == nullptr)
                table.clGetEventInfo = &Refuse<decltype(table.clGetEventInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetEventInfo), void*>) {
            if (table.clGetEventInfo == nullptr)
                table.clGetEventInfo = reinterpret_cast<void*>(&stubs::clGetEventInfo);
        }
    }
    if constexpr (requires { table.clCreateUserEvent; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateUserEvent),
                                     decltype(&stubs::clCreateUserEvent)>) {
            if (table.clCreateUserEvent == nullptr) table.clCreateUserEvent = &stubs::clCreateUserEvent;
        } else if constexpr (IsRefusable<decltype(table.clCreateUserEvent)>::value) {
            if (table.clCreateUserEvent == nullptr)
                table.clCreateUserEvent = &Refuse<decltype(table.clCreateUserEvent)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateUserEvent), void*>) {
            if (table.clCreateUserEvent == nullptr)
                table.clCreateUserEvent = reinterpret_cast<void*>(&stubs::clCreateUserEvent);
        }
    }
    if constexpr (requires { table.clRetainEvent; }) {
        if constexpr (std::is_same_v<decltype(table.clRetainEvent),
                                     decltype(&stubs::clRetainEvent)>) {
            if (table.clRetainEvent == nullptr) table.clRetainEvent = &stubs::clRetainEvent;
        } else if constexpr (IsRefusable<decltype(table.clRetainEvent)>::value) {
            if (table.clRetainEvent == nullptr)
                table.clRetainEvent = &Refuse<decltype(table.clRetainEvent)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clRetainEvent), void*>) {
            if (table.clRetainEvent == nullptr)
                table.clRetainEvent = reinterpret_cast<void*>(&stubs::clRetainEvent);
        }
    }
    if constexpr (requires { table.clReleaseEvent; }) {
        if constexpr (std::is_same_v<decltype(table.clReleaseEvent),
                                     decltype(&stubs::clReleaseEvent)>) {
            if (table.clReleaseEvent == nullptr) table.clReleaseEvent = &stubs::clReleaseEvent;
        } else if constexpr (IsRefusable<decltype(table.clReleaseEvent)>::value) {
            if (table.clReleaseEvent == nullptr)
                table.clReleaseEvent = &Refuse<decltype(table.clReleaseEvent)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clReleaseEvent), void*>) {
            if (table.clReleaseEvent == nullptr)
                table.clReleaseEvent = reinterpret_cast<void*>(&stubs::clReleaseEvent);
        }
    }
    if constexpr (requires { table.clSetUserEventStatus; }) {
        if constexpr (std::is_same_v<decltype(table.clSetUserEventStatus),
                                     decltype(&stubs::clSetUserEventStatus)>) {
            if (table.clSetUserEventStatus == nullptr) table.clSetUserEventStatus = &stubs::clSetUserEventStatus;
        } else if constexpr (IsRefusable<decltype(table.clSetUserEventStatus)>::value) {
            if (table.clSetUserEventStatus == nullptr)
                table.clSetUserEventStatus = &Refuse<decltype(table.clSetUserEventStatus)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSetUserEventStatus), void*>) {
            if (table.clSetUserEventStatus == nullptr)
                table.clSetUserEventStatus = reinterpret_cast<void*>(&stubs::clSetUserEventStatus);
        }
    }
    if constexpr (requires { table.clSetEventCallback; }) {
        if constexpr (std::is_same_v<decltype(table.clSetEventCallback),
                                     decltype(&stubs::clSetEventCallback)>) {
            if (table.clSetEventCallback == nullptr) table.clSetEventCallback = &stubs::clSetEventCallback;
        } else if constexpr (IsRefusable<decltype(table.clSetEventCallback)>::value) {
            if (table.clSetEventCallback == nullptr)
                table.clSetEventCallback = &Refuse<decltype(table.clSetEventCallback)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSetEventCallback), void*>) {
            if (table.clSetEventCallback == nullptr)
                table.clSetEventCallback = reinterpret_cast<void*>(&stubs::clSetEventCallback);
        }
    }
    if constexpr (requires { table.clGetEventProfilingInfo; }) {
        if constexpr (std::is_same_v<decltype(table.clGetEventProfilingInfo),
                                     decltype(&stubs::clGetEventProfilingInfo)>) {
            if (table.clGetEventProfilingInfo == nullptr) table.clGetEventProfilingInfo = &stubs::clGetEventProfilingInfo;
        } else if constexpr (IsRefusable<decltype(table.clGetEventProfilingInfo)>::value) {
            if (table.clGetEventProfilingInfo == nullptr)
                table.clGetEventProfilingInfo = &Refuse<decltype(table.clGetEventProfilingInfo)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetEventProfilingInfo), void*>) {
            if (table.clGetEventProfilingInfo == nullptr)
                table.clGetEventProfilingInfo = reinterpret_cast<void*>(&stubs::clGetEventProfilingInfo);
        }
    }
    if constexpr (requires { table.clFlush; }) {
        if constexpr (std::is_same_v<decltype(table.clFlush),
                                     decltype(&stubs::clFlush)>) {
            if (table.clFlush == nullptr) table.clFlush = &stubs::clFlush;
        } else if constexpr (IsRefusable<decltype(table.clFlush)>::value) {
            if (table.clFlush == nullptr)
                table.clFlush = &Refuse<decltype(table.clFlush)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clFlush), void*>) {
            if (table.clFlush == nullptr)
                table.clFlush = reinterpret_cast<void*>(&stubs::clFlush);
        }
    }
    if constexpr (requires { table.clFinish; }) {
        if constexpr (std::is_same_v<decltype(table.clFinish),
                                     decltype(&stubs::clFinish)>) {
            if (table.clFinish == nullptr) table.clFinish = &stubs::clFinish;
        } else if constexpr (IsRefusable<decltype(table.clFinish)>::value) {
            if (table.clFinish == nullptr)
                table.clFinish = &Refuse<decltype(table.clFinish)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clFinish), void*>) {
            if (table.clFinish == nullptr)
                table.clFinish = reinterpret_cast<void*>(&stubs::clFinish);
        }
    }
    if constexpr (requires { table.clEnqueueReadBuffer; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueReadBuffer),
                                     decltype(&stubs::clEnqueueReadBuffer)>) {
            if (table.clEnqueueReadBuffer == nullptr) table.clEnqueueReadBuffer = &stubs::clEnqueueReadBuffer;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueReadBuffer)>::value) {
            if (table.clEnqueueReadBuffer == nullptr)
                table.clEnqueueReadBuffer = &Refuse<decltype(table.clEnqueueReadBuffer)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueReadBuffer), void*>) {
            if (table.clEnqueueReadBuffer == nullptr)
                table.clEnqueueReadBuffer = reinterpret_cast<void*>(&stubs::clEnqueueReadBuffer);
        }
    }
    if constexpr (requires { table.clEnqueueReadBufferRect; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueReadBufferRect),
                                     decltype(&stubs::clEnqueueReadBufferRect)>) {
            if (table.clEnqueueReadBufferRect == nullptr) table.clEnqueueReadBufferRect = &stubs::clEnqueueReadBufferRect;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueReadBufferRect)>::value) {
            if (table.clEnqueueReadBufferRect == nullptr)
                table.clEnqueueReadBufferRect = &Refuse<decltype(table.clEnqueueReadBufferRect)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueReadBufferRect), void*>) {
            if (table.clEnqueueReadBufferRect == nullptr)
                table.clEnqueueReadBufferRect = reinterpret_cast<void*>(&stubs::clEnqueueReadBufferRect);
        }
    }
    if constexpr (requires { table.clEnqueueWriteBuffer; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueWriteBuffer),
                                     decltype(&stubs::clEnqueueWriteBuffer)>) {
            if (table.clEnqueueWriteBuffer == nullptr) table.clEnqueueWriteBuffer = &stubs::clEnqueueWriteBuffer;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueWriteBuffer)>::value) {
            if (table.clEnqueueWriteBuffer == nullptr)
                table.clEnqueueWriteBuffer = &Refuse<decltype(table.clEnqueueWriteBuffer)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueWriteBuffer), void*>) {
            if (table.clEnqueueWriteBuffer == nullptr)
                table.clEnqueueWriteBuffer = reinterpret_cast<void*>(&stubs::clEnqueueWriteBuffer);
        }
    }
    if constexpr (requires { table.clEnqueueWriteBufferRect; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueWriteBufferRect),
                                     decltype(&stubs::clEnqueueWriteBufferRect)>) {
            if (table.clEnqueueWriteBufferRect == nullptr) table.clEnqueueWriteBufferRect = &stubs::clEnqueueWriteBufferRect;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueWriteBufferRect)>::value) {
            if (table.clEnqueueWriteBufferRect == nullptr)
                table.clEnqueueWriteBufferRect = &Refuse<decltype(table.clEnqueueWriteBufferRect)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueWriteBufferRect), void*>) {
            if (table.clEnqueueWriteBufferRect == nullptr)
                table.clEnqueueWriteBufferRect = reinterpret_cast<void*>(&stubs::clEnqueueWriteBufferRect);
        }
    }
    if constexpr (requires { table.clEnqueueFillBuffer; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueFillBuffer),
                                     decltype(&stubs::clEnqueueFillBuffer)>) {
            if (table.clEnqueueFillBuffer == nullptr) table.clEnqueueFillBuffer = &stubs::clEnqueueFillBuffer;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueFillBuffer)>::value) {
            if (table.clEnqueueFillBuffer == nullptr)
                table.clEnqueueFillBuffer = &Refuse<decltype(table.clEnqueueFillBuffer)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueFillBuffer), void*>) {
            if (table.clEnqueueFillBuffer == nullptr)
                table.clEnqueueFillBuffer = reinterpret_cast<void*>(&stubs::clEnqueueFillBuffer);
        }
    }
    if constexpr (requires { table.clEnqueueCopyBuffer; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueCopyBuffer),
                                     decltype(&stubs::clEnqueueCopyBuffer)>) {
            if (table.clEnqueueCopyBuffer == nullptr) table.clEnqueueCopyBuffer = &stubs::clEnqueueCopyBuffer;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueCopyBuffer)>::value) {
            if (table.clEnqueueCopyBuffer == nullptr)
                table.clEnqueueCopyBuffer = &Refuse<decltype(table.clEnqueueCopyBuffer)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueCopyBuffer), void*>) {
            if (table.clEnqueueCopyBuffer == nullptr)
                table.clEnqueueCopyBuffer = reinterpret_cast<void*>(&stubs::clEnqueueCopyBuffer);
        }
    }
    if constexpr (requires { table.clEnqueueCopyBufferRect; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueCopyBufferRect),
                                     decltype(&stubs::clEnqueueCopyBufferRect)>) {
            if (table.clEnqueueCopyBufferRect == nullptr) table.clEnqueueCopyBufferRect = &stubs::clEnqueueCopyBufferRect;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueCopyBufferRect)>::value) {
            if (table.clEnqueueCopyBufferRect == nullptr)
                table.clEnqueueCopyBufferRect = &Refuse<decltype(table.clEnqueueCopyBufferRect)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueCopyBufferRect), void*>) {
            if (table.clEnqueueCopyBufferRect == nullptr)
                table.clEnqueueCopyBufferRect = reinterpret_cast<void*>(&stubs::clEnqueueCopyBufferRect);
        }
    }
    if constexpr (requires { table.clEnqueueReadImage; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueReadImage),
                                     decltype(&stubs::clEnqueueReadImage)>) {
            if (table.clEnqueueReadImage == nullptr) table.clEnqueueReadImage = &stubs::clEnqueueReadImage;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueReadImage)>::value) {
            if (table.clEnqueueReadImage == nullptr)
                table.clEnqueueReadImage = &Refuse<decltype(table.clEnqueueReadImage)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueReadImage), void*>) {
            if (table.clEnqueueReadImage == nullptr)
                table.clEnqueueReadImage = reinterpret_cast<void*>(&stubs::clEnqueueReadImage);
        }
    }
    if constexpr (requires { table.clEnqueueWriteImage; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueWriteImage),
                                     decltype(&stubs::clEnqueueWriteImage)>) {
            if (table.clEnqueueWriteImage == nullptr) table.clEnqueueWriteImage = &stubs::clEnqueueWriteImage;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueWriteImage)>::value) {
            if (table.clEnqueueWriteImage == nullptr)
                table.clEnqueueWriteImage = &Refuse<decltype(table.clEnqueueWriteImage)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueWriteImage), void*>) {
            if (table.clEnqueueWriteImage == nullptr)
                table.clEnqueueWriteImage = reinterpret_cast<void*>(&stubs::clEnqueueWriteImage);
        }
    }
    if constexpr (requires { table.clEnqueueFillImage; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueFillImage),
                                     decltype(&stubs::clEnqueueFillImage)>) {
            if (table.clEnqueueFillImage == nullptr) table.clEnqueueFillImage = &stubs::clEnqueueFillImage;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueFillImage)>::value) {
            if (table.clEnqueueFillImage == nullptr)
                table.clEnqueueFillImage = &Refuse<decltype(table.clEnqueueFillImage)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueFillImage), void*>) {
            if (table.clEnqueueFillImage == nullptr)
                table.clEnqueueFillImage = reinterpret_cast<void*>(&stubs::clEnqueueFillImage);
        }
    }
    if constexpr (requires { table.clEnqueueCopyImage; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueCopyImage),
                                     decltype(&stubs::clEnqueueCopyImage)>) {
            if (table.clEnqueueCopyImage == nullptr) table.clEnqueueCopyImage = &stubs::clEnqueueCopyImage;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueCopyImage)>::value) {
            if (table.clEnqueueCopyImage == nullptr)
                table.clEnqueueCopyImage = &Refuse<decltype(table.clEnqueueCopyImage)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueCopyImage), void*>) {
            if (table.clEnqueueCopyImage == nullptr)
                table.clEnqueueCopyImage = reinterpret_cast<void*>(&stubs::clEnqueueCopyImage);
        }
    }
    if constexpr (requires { table.clEnqueueCopyImageToBuffer; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueCopyImageToBuffer),
                                     decltype(&stubs::clEnqueueCopyImageToBuffer)>) {
            if (table.clEnqueueCopyImageToBuffer == nullptr) table.clEnqueueCopyImageToBuffer = &stubs::clEnqueueCopyImageToBuffer;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueCopyImageToBuffer)>::value) {
            if (table.clEnqueueCopyImageToBuffer == nullptr)
                table.clEnqueueCopyImageToBuffer = &Refuse<decltype(table.clEnqueueCopyImageToBuffer)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueCopyImageToBuffer), void*>) {
            if (table.clEnqueueCopyImageToBuffer == nullptr)
                table.clEnqueueCopyImageToBuffer = reinterpret_cast<void*>(&stubs::clEnqueueCopyImageToBuffer);
        }
    }
    if constexpr (requires { table.clEnqueueCopyBufferToImage; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueCopyBufferToImage),
                                     decltype(&stubs::clEnqueueCopyBufferToImage)>) {
            if (table.clEnqueueCopyBufferToImage == nullptr) table.clEnqueueCopyBufferToImage = &stubs::clEnqueueCopyBufferToImage;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueCopyBufferToImage)>::value) {
            if (table.clEnqueueCopyBufferToImage == nullptr)
                table.clEnqueueCopyBufferToImage = &Refuse<decltype(table.clEnqueueCopyBufferToImage)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueCopyBufferToImage), void*>) {
            if (table.clEnqueueCopyBufferToImage == nullptr)
                table.clEnqueueCopyBufferToImage = reinterpret_cast<void*>(&stubs::clEnqueueCopyBufferToImage);
        }
    }
    if constexpr (requires { table.clEnqueueMapBuffer; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueMapBuffer),
                                     decltype(&stubs::clEnqueueMapBuffer)>) {
            if (table.clEnqueueMapBuffer == nullptr) table.clEnqueueMapBuffer = &stubs::clEnqueueMapBuffer;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueMapBuffer)>::value) {
            if (table.clEnqueueMapBuffer == nullptr)
                table.clEnqueueMapBuffer = &Refuse<decltype(table.clEnqueueMapBuffer)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueMapBuffer), void*>) {
            if (table.clEnqueueMapBuffer == nullptr)
                table.clEnqueueMapBuffer = reinterpret_cast<void*>(&stubs::clEnqueueMapBuffer);
        }
    }
    if constexpr (requires { table.clEnqueueMapImage; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueMapImage),
                                     decltype(&stubs::clEnqueueMapImage)>) {
            if (table.clEnqueueMapImage == nullptr) table.clEnqueueMapImage = &stubs::clEnqueueMapImage;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueMapImage)>::value) {
            if (table.clEnqueueMapImage == nullptr)
                table.clEnqueueMapImage = &Refuse<decltype(table.clEnqueueMapImage)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueMapImage), void*>) {
            if (table.clEnqueueMapImage == nullptr)
                table.clEnqueueMapImage = reinterpret_cast<void*>(&stubs::clEnqueueMapImage);
        }
    }
    if constexpr (requires { table.clEnqueueUnmapMemObject; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueUnmapMemObject),
                                     decltype(&stubs::clEnqueueUnmapMemObject)>) {
            if (table.clEnqueueUnmapMemObject == nullptr) table.clEnqueueUnmapMemObject = &stubs::clEnqueueUnmapMemObject;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueUnmapMemObject)>::value) {
            if (table.clEnqueueUnmapMemObject == nullptr)
                table.clEnqueueUnmapMemObject = &Refuse<decltype(table.clEnqueueUnmapMemObject)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueUnmapMemObject), void*>) {
            if (table.clEnqueueUnmapMemObject == nullptr)
                table.clEnqueueUnmapMemObject = reinterpret_cast<void*>(&stubs::clEnqueueUnmapMemObject);
        }
    }
    if constexpr (requires { table.clEnqueueMigrateMemObjects; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueMigrateMemObjects),
                                     decltype(&stubs::clEnqueueMigrateMemObjects)>) {
            if (table.clEnqueueMigrateMemObjects == nullptr) table.clEnqueueMigrateMemObjects = &stubs::clEnqueueMigrateMemObjects;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueMigrateMemObjects)>::value) {
            if (table.clEnqueueMigrateMemObjects == nullptr)
                table.clEnqueueMigrateMemObjects = &Refuse<decltype(table.clEnqueueMigrateMemObjects)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueMigrateMemObjects), void*>) {
            if (table.clEnqueueMigrateMemObjects == nullptr)
                table.clEnqueueMigrateMemObjects = reinterpret_cast<void*>(&stubs::clEnqueueMigrateMemObjects);
        }
    }
    if constexpr (requires { table.clEnqueueNDRangeKernel; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueNDRangeKernel),
                                     decltype(&stubs::clEnqueueNDRangeKernel)>) {
            if (table.clEnqueueNDRangeKernel == nullptr) table.clEnqueueNDRangeKernel = &stubs::clEnqueueNDRangeKernel;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueNDRangeKernel)>::value) {
            if (table.clEnqueueNDRangeKernel == nullptr)
                table.clEnqueueNDRangeKernel = &Refuse<decltype(table.clEnqueueNDRangeKernel)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueNDRangeKernel), void*>) {
            if (table.clEnqueueNDRangeKernel == nullptr)
                table.clEnqueueNDRangeKernel = reinterpret_cast<void*>(&stubs::clEnqueueNDRangeKernel);
        }
    }
    if constexpr (requires { table.clEnqueueNativeKernel; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueNativeKernel),
                                     decltype(&stubs::clEnqueueNativeKernel)>) {
            if (table.clEnqueueNativeKernel == nullptr) table.clEnqueueNativeKernel = &stubs::clEnqueueNativeKernel;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueNativeKernel)>::value) {
            if (table.clEnqueueNativeKernel == nullptr)
                table.clEnqueueNativeKernel = &Refuse<decltype(table.clEnqueueNativeKernel)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueNativeKernel), void*>) {
            if (table.clEnqueueNativeKernel == nullptr)
                table.clEnqueueNativeKernel = reinterpret_cast<void*>(&stubs::clEnqueueNativeKernel);
        }
    }
    if constexpr (requires { table.clEnqueueMarkerWithWaitList; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueMarkerWithWaitList),
                                     decltype(&stubs::clEnqueueMarkerWithWaitList)>) {
            if (table.clEnqueueMarkerWithWaitList == nullptr) table.clEnqueueMarkerWithWaitList = &stubs::clEnqueueMarkerWithWaitList;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueMarkerWithWaitList)>::value) {
            if (table.clEnqueueMarkerWithWaitList == nullptr)
                table.clEnqueueMarkerWithWaitList = &Refuse<decltype(table.clEnqueueMarkerWithWaitList)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueMarkerWithWaitList), void*>) {
            if (table.clEnqueueMarkerWithWaitList == nullptr)
                table.clEnqueueMarkerWithWaitList = reinterpret_cast<void*>(&stubs::clEnqueueMarkerWithWaitList);
        }
    }
    if constexpr (requires { table.clEnqueueBarrierWithWaitList; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueBarrierWithWaitList),
                                     decltype(&stubs::clEnqueueBarrierWithWaitList)>) {
            if (table.clEnqueueBarrierWithWaitList == nullptr) table.clEnqueueBarrierWithWaitList = &stubs::clEnqueueBarrierWithWaitList;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueBarrierWithWaitList)>::value) {
            if (table.clEnqueueBarrierWithWaitList == nullptr)
                table.clEnqueueBarrierWithWaitList = &Refuse<decltype(table.clEnqueueBarrierWithWaitList)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueBarrierWithWaitList), void*>) {
            if (table.clEnqueueBarrierWithWaitList == nullptr)
                table.clEnqueueBarrierWithWaitList = reinterpret_cast<void*>(&stubs::clEnqueueBarrierWithWaitList);
        }
    }
    if constexpr (requires { table.clEnqueueSVMFree; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueSVMFree),
                                     decltype(&stubs::clEnqueueSVMFree)>) {
            if (table.clEnqueueSVMFree == nullptr) table.clEnqueueSVMFree = &stubs::clEnqueueSVMFree;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueSVMFree)>::value) {
            if (table.clEnqueueSVMFree == nullptr)
                table.clEnqueueSVMFree = &Refuse<decltype(table.clEnqueueSVMFree)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueSVMFree), void*>) {
            if (table.clEnqueueSVMFree == nullptr)
                table.clEnqueueSVMFree = reinterpret_cast<void*>(&stubs::clEnqueueSVMFree);
        }
    }
    if constexpr (requires { table.clEnqueueSVMMemcpy; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueSVMMemcpy),
                                     decltype(&stubs::clEnqueueSVMMemcpy)>) {
            if (table.clEnqueueSVMMemcpy == nullptr) table.clEnqueueSVMMemcpy = &stubs::clEnqueueSVMMemcpy;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueSVMMemcpy)>::value) {
            if (table.clEnqueueSVMMemcpy == nullptr)
                table.clEnqueueSVMMemcpy = &Refuse<decltype(table.clEnqueueSVMMemcpy)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueSVMMemcpy), void*>) {
            if (table.clEnqueueSVMMemcpy == nullptr)
                table.clEnqueueSVMMemcpy = reinterpret_cast<void*>(&stubs::clEnqueueSVMMemcpy);
        }
    }
    if constexpr (requires { table.clEnqueueSVMMemFill; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueSVMMemFill),
                                     decltype(&stubs::clEnqueueSVMMemFill)>) {
            if (table.clEnqueueSVMMemFill == nullptr) table.clEnqueueSVMMemFill = &stubs::clEnqueueSVMMemFill;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueSVMMemFill)>::value) {
            if (table.clEnqueueSVMMemFill == nullptr)
                table.clEnqueueSVMMemFill = &Refuse<decltype(table.clEnqueueSVMMemFill)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueSVMMemFill), void*>) {
            if (table.clEnqueueSVMMemFill == nullptr)
                table.clEnqueueSVMMemFill = reinterpret_cast<void*>(&stubs::clEnqueueSVMMemFill);
        }
    }
    if constexpr (requires { table.clEnqueueSVMMap; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueSVMMap),
                                     decltype(&stubs::clEnqueueSVMMap)>) {
            if (table.clEnqueueSVMMap == nullptr) table.clEnqueueSVMMap = &stubs::clEnqueueSVMMap;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueSVMMap)>::value) {
            if (table.clEnqueueSVMMap == nullptr)
                table.clEnqueueSVMMap = &Refuse<decltype(table.clEnqueueSVMMap)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueSVMMap), void*>) {
            if (table.clEnqueueSVMMap == nullptr)
                table.clEnqueueSVMMap = reinterpret_cast<void*>(&stubs::clEnqueueSVMMap);
        }
    }
    if constexpr (requires { table.clEnqueueSVMUnmap; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueSVMUnmap),
                                     decltype(&stubs::clEnqueueSVMUnmap)>) {
            if (table.clEnqueueSVMUnmap == nullptr) table.clEnqueueSVMUnmap = &stubs::clEnqueueSVMUnmap;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueSVMUnmap)>::value) {
            if (table.clEnqueueSVMUnmap == nullptr)
                table.clEnqueueSVMUnmap = &Refuse<decltype(table.clEnqueueSVMUnmap)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueSVMUnmap), void*>) {
            if (table.clEnqueueSVMUnmap == nullptr)
                table.clEnqueueSVMUnmap = reinterpret_cast<void*>(&stubs::clEnqueueSVMUnmap);
        }
    }
    if constexpr (requires { table.clEnqueueSVMMigrateMem; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueSVMMigrateMem),
                                     decltype(&stubs::clEnqueueSVMMigrateMem)>) {
            if (table.clEnqueueSVMMigrateMem == nullptr) table.clEnqueueSVMMigrateMem = &stubs::clEnqueueSVMMigrateMem;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueSVMMigrateMem)>::value) {
            if (table.clEnqueueSVMMigrateMem == nullptr)
                table.clEnqueueSVMMigrateMem = &Refuse<decltype(table.clEnqueueSVMMigrateMem)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueSVMMigrateMem), void*>) {
            if (table.clEnqueueSVMMigrateMem == nullptr)
                table.clEnqueueSVMMigrateMem = reinterpret_cast<void*>(&stubs::clEnqueueSVMMigrateMem);
        }
    }
    if constexpr (requires { table.clGetExtensionFunctionAddressForPlatform; }) {
        if constexpr (std::is_same_v<decltype(table.clGetExtensionFunctionAddressForPlatform),
                                     decltype(&stubs::clGetExtensionFunctionAddressForPlatform)>) {
            if (table.clGetExtensionFunctionAddressForPlatform == nullptr) table.clGetExtensionFunctionAddressForPlatform = &stubs::clGetExtensionFunctionAddressForPlatform;
        } else if constexpr (IsRefusable<decltype(table.clGetExtensionFunctionAddressForPlatform)>::value) {
            if (table.clGetExtensionFunctionAddressForPlatform == nullptr)
                table.clGetExtensionFunctionAddressForPlatform = &Refuse<decltype(table.clGetExtensionFunctionAddressForPlatform)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetExtensionFunctionAddressForPlatform), void*>) {
            if (table.clGetExtensionFunctionAddressForPlatform == nullptr)
                table.clGetExtensionFunctionAddressForPlatform = reinterpret_cast<void*>(&stubs::clGetExtensionFunctionAddressForPlatform);
        }
    }
    if constexpr (requires { table.clSetCommandQueueProperty; }) {
        if constexpr (std::is_same_v<decltype(table.clSetCommandQueueProperty),
                                     decltype(&stubs::clSetCommandQueueProperty)>) {
            if (table.clSetCommandQueueProperty == nullptr) table.clSetCommandQueueProperty = &stubs::clSetCommandQueueProperty;
        } else if constexpr (IsRefusable<decltype(table.clSetCommandQueueProperty)>::value) {
            if (table.clSetCommandQueueProperty == nullptr)
                table.clSetCommandQueueProperty = &Refuse<decltype(table.clSetCommandQueueProperty)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clSetCommandQueueProperty), void*>) {
            if (table.clSetCommandQueueProperty == nullptr)
                table.clSetCommandQueueProperty = reinterpret_cast<void*>(&stubs::clSetCommandQueueProperty);
        }
    }
    if constexpr (requires { table.clCreateImage2D; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateImage2D),
                                     decltype(&stubs::clCreateImage2D)>) {
            if (table.clCreateImage2D == nullptr) table.clCreateImage2D = &stubs::clCreateImage2D;
        } else if constexpr (IsRefusable<decltype(table.clCreateImage2D)>::value) {
            if (table.clCreateImage2D == nullptr)
                table.clCreateImage2D = &Refuse<decltype(table.clCreateImage2D)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateImage2D), void*>) {
            if (table.clCreateImage2D == nullptr)
                table.clCreateImage2D = reinterpret_cast<void*>(&stubs::clCreateImage2D);
        }
    }
    if constexpr (requires { table.clCreateImage3D; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateImage3D),
                                     decltype(&stubs::clCreateImage3D)>) {
            if (table.clCreateImage3D == nullptr) table.clCreateImage3D = &stubs::clCreateImage3D;
        } else if constexpr (IsRefusable<decltype(table.clCreateImage3D)>::value) {
            if (table.clCreateImage3D == nullptr)
                table.clCreateImage3D = &Refuse<decltype(table.clCreateImage3D)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateImage3D), void*>) {
            if (table.clCreateImage3D == nullptr)
                table.clCreateImage3D = reinterpret_cast<void*>(&stubs::clCreateImage3D);
        }
    }
    if constexpr (requires { table.clEnqueueMarker; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueMarker),
                                     decltype(&stubs::clEnqueueMarker)>) {
            if (table.clEnqueueMarker == nullptr) table.clEnqueueMarker = &stubs::clEnqueueMarker;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueMarker)>::value) {
            if (table.clEnqueueMarker == nullptr)
                table.clEnqueueMarker = &Refuse<decltype(table.clEnqueueMarker)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueMarker), void*>) {
            if (table.clEnqueueMarker == nullptr)
                table.clEnqueueMarker = reinterpret_cast<void*>(&stubs::clEnqueueMarker);
        }
    }
    if constexpr (requires { table.clEnqueueWaitForEvents; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueWaitForEvents),
                                     decltype(&stubs::clEnqueueWaitForEvents)>) {
            if (table.clEnqueueWaitForEvents == nullptr) table.clEnqueueWaitForEvents = &stubs::clEnqueueWaitForEvents;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueWaitForEvents)>::value) {
            if (table.clEnqueueWaitForEvents == nullptr)
                table.clEnqueueWaitForEvents = &Refuse<decltype(table.clEnqueueWaitForEvents)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueWaitForEvents), void*>) {
            if (table.clEnqueueWaitForEvents == nullptr)
                table.clEnqueueWaitForEvents = reinterpret_cast<void*>(&stubs::clEnqueueWaitForEvents);
        }
    }
    if constexpr (requires { table.clEnqueueBarrier; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueBarrier),
                                     decltype(&stubs::clEnqueueBarrier)>) {
            if (table.clEnqueueBarrier == nullptr) table.clEnqueueBarrier = &stubs::clEnqueueBarrier;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueBarrier)>::value) {
            if (table.clEnqueueBarrier == nullptr)
                table.clEnqueueBarrier = &Refuse<decltype(table.clEnqueueBarrier)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueBarrier), void*>) {
            if (table.clEnqueueBarrier == nullptr)
                table.clEnqueueBarrier = reinterpret_cast<void*>(&stubs::clEnqueueBarrier);
        }
    }
    if constexpr (requires { table.clUnloadCompiler; }) {
        if constexpr (std::is_same_v<decltype(table.clUnloadCompiler),
                                     decltype(&stubs::clUnloadCompiler)>) {
            if (table.clUnloadCompiler == nullptr) table.clUnloadCompiler = &stubs::clUnloadCompiler;
        } else if constexpr (IsRefusable<decltype(table.clUnloadCompiler)>::value) {
            if (table.clUnloadCompiler == nullptr)
                table.clUnloadCompiler = &Refuse<decltype(table.clUnloadCompiler)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clUnloadCompiler), void*>) {
            if (table.clUnloadCompiler == nullptr)
                table.clUnloadCompiler = reinterpret_cast<void*>(&stubs::clUnloadCompiler);
        }
    }
    if constexpr (requires { table.clGetExtensionFunctionAddress; }) {
        if constexpr (std::is_same_v<decltype(table.clGetExtensionFunctionAddress),
                                     decltype(&stubs::clGetExtensionFunctionAddress)>) {
            if (table.clGetExtensionFunctionAddress == nullptr) table.clGetExtensionFunctionAddress = &stubs::clGetExtensionFunctionAddress;
        } else if constexpr (IsRefusable<decltype(table.clGetExtensionFunctionAddress)>::value) {
            if (table.clGetExtensionFunctionAddress == nullptr)
                table.clGetExtensionFunctionAddress = &Refuse<decltype(table.clGetExtensionFunctionAddress)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clGetExtensionFunctionAddress), void*>) {
            if (table.clGetExtensionFunctionAddress == nullptr)
                table.clGetExtensionFunctionAddress = reinterpret_cast<void*>(&stubs::clGetExtensionFunctionAddress);
        }
    }
    if constexpr (requires { table.clCreateCommandQueue; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateCommandQueue),
                                     decltype(&stubs::clCreateCommandQueue)>) {
            if (table.clCreateCommandQueue == nullptr) table.clCreateCommandQueue = &stubs::clCreateCommandQueue;
        } else if constexpr (IsRefusable<decltype(table.clCreateCommandQueue)>::value) {
            if (table.clCreateCommandQueue == nullptr)
                table.clCreateCommandQueue = &Refuse<decltype(table.clCreateCommandQueue)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateCommandQueue), void*>) {
            if (table.clCreateCommandQueue == nullptr)
                table.clCreateCommandQueue = reinterpret_cast<void*>(&stubs::clCreateCommandQueue);
        }
    }
    if constexpr (requires { table.clCreateSampler; }) {
        if constexpr (std::is_same_v<decltype(table.clCreateSampler),
                                     decltype(&stubs::clCreateSampler)>) {
            if (table.clCreateSampler == nullptr) table.clCreateSampler = &stubs::clCreateSampler;
        } else if constexpr (IsRefusable<decltype(table.clCreateSampler)>::value) {
            if (table.clCreateSampler == nullptr)
                table.clCreateSampler = &Refuse<decltype(table.clCreateSampler)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clCreateSampler), void*>) {
            if (table.clCreateSampler == nullptr)
                table.clCreateSampler = reinterpret_cast<void*>(&stubs::clCreateSampler);
        }
    }
    if constexpr (requires { table.clEnqueueTask; }) {
        if constexpr (std::is_same_v<decltype(table.clEnqueueTask),
                                     decltype(&stubs::clEnqueueTask)>) {
            if (table.clEnqueueTask == nullptr) table.clEnqueueTask = &stubs::clEnqueueTask;
        } else if constexpr (IsRefusable<decltype(table.clEnqueueTask)>::value) {
            if (table.clEnqueueTask == nullptr)
                table.clEnqueueTask = &Refuse<decltype(table.clEnqueueTask)>::call;
        } else if constexpr (std::is_same_v<decltype(table.clEnqueueTask), void*>) {
            if (table.clEnqueueTask == nullptr)
                table.clEnqueueTask = reinterpret_cast<void*>(&stubs::clEnqueueTask);
        }
    }
    if constexpr (requires { table.clGetLayerInfo; }) {
        if constexpr (IsRefusable<decltype(table.clGetLayerInfo)>::value) {
            if (table.clGetLayerInfo == nullptr)
                table.clGetLayerInfo = &Refuse<decltype(table.clGetLayerInfo)>::call;
        }
    }
    if constexpr (requires { table.clInitLayer; }) {
        if constexpr (IsRefusable<decltype(table.clInitLayer)>::value) {
            if (table.clInitLayer == nullptr)
                table.clInitLayer = &Refuse<decltype(table.clInitLayer)>::call;
        }
    }
    if constexpr (requires { table.clGetICDLoaderInfoOCLICD; }) {
        if constexpr (IsRefusable<decltype(table.clGetICDLoaderInfoOCLICD)>::value) {
            if (table.clGetICDLoaderInfoOCLICD == nullptr)
                table.clGetICDLoaderInfoOCLICD = &Refuse<decltype(table.clGetICDLoaderInfoOCLICD)>::call;
        }
    }
    if constexpr (requires { table.clGetSupportedGLTextureFormatsINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clGetSupportedGLTextureFormatsINTEL)>::value) {
            if (table.clGetSupportedGLTextureFormatsINTEL == nullptr)
                table.clGetSupportedGLTextureFormatsINTEL = &Refuse<decltype(table.clGetSupportedGLTextureFormatsINTEL)>::call;
        }
    }
    if constexpr (requires { table.clGetSupportedDX9MediaSurfaceFormatsINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clGetSupportedDX9MediaSurfaceFormatsINTEL)>::value) {
            if (table.clGetSupportedDX9MediaSurfaceFormatsINTEL == nullptr)
                table.clGetSupportedDX9MediaSurfaceFormatsINTEL = &Refuse<decltype(table.clGetSupportedDX9MediaSurfaceFormatsINTEL)>::call;
        }
    }
    if constexpr (requires { table.clGetSupportedD3D10TextureFormatsINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clGetSupportedD3D10TextureFormatsINTEL)>::value) {
            if (table.clGetSupportedD3D10TextureFormatsINTEL == nullptr)
                table.clGetSupportedD3D10TextureFormatsINTEL = &Refuse<decltype(table.clGetSupportedD3D10TextureFormatsINTEL)>::call;
        }
    }
    if constexpr (requires { table.clGetSupportedD3D11TextureFormatsINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clGetSupportedD3D11TextureFormatsINTEL)>::value) {
            if (table.clGetSupportedD3D11TextureFormatsINTEL == nullptr)
                table.clGetSupportedD3D11TextureFormatsINTEL = &Refuse<decltype(table.clGetSupportedD3D11TextureFormatsINTEL)>::call;
        }
    }
    if constexpr (requires { table.clGetSupportedVA_APIMediaSurfaceFormatsINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clGetSupportedVA_APIMediaSurfaceFormatsINTEL)>::value) {
            if (table.clGetSupportedVA_APIMediaSurfaceFormatsINTEL == nullptr)
                table.clGetSupportedVA_APIMediaSurfaceFormatsINTEL = &Refuse<decltype(table.clGetSupportedVA_APIMediaSurfaceFormatsINTEL)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueReadHostPipeINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueReadHostPipeINTEL)>::value) {
            if (table.clEnqueueReadHostPipeINTEL == nullptr)
                table.clEnqueueReadHostPipeINTEL = &Refuse<decltype(table.clEnqueueReadHostPipeINTEL)>::call;
        }
    }
    if constexpr (requires { table.clEnqueueWriteHostPipeINTEL; }) {
        if constexpr (IsRefusable<decltype(table.clEnqueueWriteHostPipeINTEL)>::value) {
            if (table.clEnqueueWriteHostPipeINTEL == nullptr)
                table.clEnqueueWriteHostPipeINTEL = &Refuse<decltype(table.clEnqueueWriteHostPipeINTEL)>::call;
        }
    }
    if constexpr (requires { table.clGetImageRequirementsInfoEXT; }) {
        if constexpr (IsRefusable<decltype(table.clGetImageRequirementsInfoEXT)>::value) {
            if (table.clGetImageRequirementsInfoEXT == nullptr)
                table.clGetImageRequirementsInfoEXT = &Refuse<decltype(table.clGetImageRequirementsInfoEXT)>::call;
        }
    }
    if constexpr (requires { table.clCancelCommandsIMG; }) {
        if constexpr (IsRefusable<decltype(table.clCancelCommandsIMG)>::value) {
            if (table.clCancelCommandsIMG == nullptr)
                table.clCancelCommandsIMG = &Refuse<decltype(table.clCancelCommandsIMG)>::call;
        }
    }
}

}  // namespace av::clproto
