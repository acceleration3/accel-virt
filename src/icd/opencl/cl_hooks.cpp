/* The ABI boundary the Khronos loader calls across.
 *
 * Every function here is a bare pointer in a cl_icd_dispatch, called with
 * OpenCL's own arguments and nowhere to put a `this`, so each is one line
 * through ClIcdBinding and holds no logic of its own (constitution III). The
 * one exported symbol is clGetExtensionFunctionAddress; everything else the
 * loader wants it reaches through the table.
 *
 * ClDispatchTable::fill is defined here rather than beside its class, because
 * what it installs is exactly these entries and nothing else may see them. */
#include "icd/opencl/cl_dispatch_table.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_buffer_rect.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/cl_noted_error.hpp"
#include "icd/opencl/cl_program_notify.hpp"
#include "proto/generated/opencl/cl_icd_stubs.h"
#include "proto/client/icd_stub_report.hpp"

namespace av::clicd {

constinit ClIcdBinding cl_icd_binding;

namespace {

/* An entry reached before the root exists answers the API's own refusal.
 * Nothing in this process can have a valid object yet, so every one of these
 * is an application calling into a driver that failed to start. */
#define AV_CL_ROOT_OR(refusal) \
    if (!cl_icd_binding.bound()) return refusal; \
    ClIcdRoot &root = cl_icd_binding.root()

cl_int CL_API_CALL icd_get_platform_ids_entry(cl_uint num_entries, cl_platform_id *platforms,
                                              cl_uint *num_platforms)
{
    AV_SCOPE("icd_get_platform_ids_entry");
    AV_CL_ROOT_OR(CL_PLATFORM_NOT_FOUND_KHR);
    return root.platform_ids(num_entries, platforms, num_platforms);
}

cl_int CL_API_CALL get_platform_ids(cl_uint num_entries, cl_platform_id *platforms,
                                    cl_uint *num_platforms)
{
    AV_SCOPE("get_platform_ids");
    AV_CL_ROOT_OR(CL_PLATFORM_NOT_FOUND_KHR);
    return root.noted("clGetPlatformIDs", root.platform_ids(num_entries, platforms, num_platforms));
}

cl_int CL_API_CALL get_platform_info(cl_platform_id platform, cl_platform_info name,
                                     size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("get_platform_info");
    AV_CL_ROOT_OR(CL_INVALID_PLATFORM);
    return root.noted("clGetPlatformInfo", root.platform_info(platform, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL get_device_ids(cl_platform_id platform, cl_device_type type, cl_uint num_entries,
                                  cl_device_id *devices, cl_uint *num_devices)
{
    AV_SCOPE("get_device_ids");
    AV_CL_ROOT_OR(CL_INVALID_PLATFORM);
    return root.noted("clGetDeviceIDs", root.device_ids(platform, type, num_entries, devices, num_devices));
}

cl_int CL_API_CALL get_command_queue_info(cl_command_queue queue, cl_command_queue_info name, size_t value_bytes,
                                          void *value, size_t *size_ret)
{
    AV_SCOPE("get_command_queue_info");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted_query("clGetCommandQueueInfo", name, root.queue_info(queue, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL get_device_info(cl_device_id device, cl_device_info name, size_t value_bytes,
                                   void *value, size_t *size_ret)
{
    AV_SCOPE("get_device_info");
    AV_CL_ROOT_OR(CL_INVALID_DEVICE);
    return root.noted_query("clGetDeviceInfo", name, root.device_info(device, name, value_bytes, value, size_ret));
}

cl_context CL_API_CALL create_context(const cl_context_properties *properties, cl_uint num_devices,
                                      const cl_device_id *devices,
                                      void(CL_CALLBACK *notify)(const char *, const void *, size_t, void *),
                                      void *user_data, cl_int *error)
{
    AV_SCOPE("create_context");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_PLATFORM;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateContext", error);
    return cl_icd_binding.root().create_context(properties, devices, num_devices, noted.pointer(),
                                                clicd::ClContextNotify(notify, user_data));
}

cl_int CL_API_CALL retain_context(cl_context context)
{
    AV_SCOPE("retain_context");
    AV_CL_ROOT_OR(CL_INVALID_CONTEXT);
    return root.noted("clRetainContext", root.retain_context(context));
}

cl_int CL_API_CALL release_context(cl_context context)
{
    AV_SCOPE("release_context");
    AV_CL_ROOT_OR(CL_INVALID_CONTEXT);
    return root.noted("clReleaseContext", root.release_context(context));
}

cl_command_queue CL_API_CALL create_queue_with_properties(cl_context context, cl_device_id device,
                                                          const cl_queue_properties *properties,
                                                          cl_int *error)
{
    AV_SCOPE("create_queue_with_properties");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateCommandQueueWithProperties", error);
    return cl_icd_binding.root().create_queue(context, device,
                                              reinterpret_cast<const uint64_t *>(properties),
                                              noted.pointer());
}

cl_command_queue CL_API_CALL create_queue(cl_context context, cl_device_id device,
                                          cl_command_queue_properties properties, cl_int *error)
{
    AV_SCOPE("create_queue");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    /* The deprecated 1.2 call carries a bitfield rather than a list: the same
     * bits, which a list names as its one CL_QUEUE_PROPERTIES. */
    const uint64_t listed[] = {static_cast<uint64_t>(CL_QUEUE_PROPERTIES), properties, 0u};
    ClNotedError noted(cl_icd_binding.root(), "clCreateCommandQueue", error);
    return cl_icd_binding.root().create_queue(context, device, properties != 0u ? listed : nullptr, noted.pointer());
}

cl_int CL_API_CALL retain_queue(cl_command_queue queue)
{
    AV_SCOPE("retain_queue");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clRetainCommandQueue", root.retain(queue, ClObject::Kind::command_queue));
}

cl_int CL_API_CALL release_queue(cl_command_queue queue)
{
    AV_SCOPE("release_queue");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clReleaseCommandQueue", root.release(queue, ClObject::Kind::command_queue));
}

cl_mem CL_API_CALL create_buffer(cl_context context, cl_mem_flags flags, size_t bytes,
                                 void *host_pointer, cl_int *error)
{
    AV_SCOPE("create_buffer");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    /* clCreateBuffer carries no list; clCreateBufferWithProperties is not a
     * slot this ICD claims, so the only list a buffer could have is none. */
    ClNotedError noted(cl_icd_binding.root(), "clCreateBuffer", error);
    return cl_icd_binding.root().create_buffer(context, flags, bytes, host_pointer, nullptr, noted.pointer());
}

/* clCreateBufferWithProperties: the same create, with the list the
 * application wrote. The only names it may carry are the external-memory
 * ones, and only while that row is reported (feature 053). */
cl_mem CL_API_CALL create_buffer_with_properties(cl_context context, const cl_mem_properties *properties,
                                                 cl_mem_flags flags, size_t bytes, void *host_pointer,
                                                 cl_int *error)
{
    AV_SCOPE("create_buffer_with_properties");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateBufferWithProperties", error);
    return cl_icd_binding.root().create_buffer(context, flags, bytes, host_pointer,
                                               reinterpret_cast<const uint64_t *>(properties), noted.pointer());
}

cl_int CL_API_CALL retain_mem(cl_mem buffer)
{
    AV_SCOPE("retain_mem");
    AV_CL_ROOT_OR(CL_INVALID_MEM_OBJECT);
    return root.noted("clRetainMemObject", root.retain(buffer, ClObject::Kind::mem));
}

cl_int CL_API_CALL release_mem(cl_mem buffer)
{
    AV_SCOPE("release_mem");
    AV_CL_ROOT_OR(CL_INVALID_MEM_OBJECT);
    return root.noted("clReleaseMemObject", root.release(buffer, ClObject::Kind::mem));
}

cl_program CL_API_CALL create_program_with_source(cl_context context, cl_uint count,
                                                  const char **sources, const size_t *lengths,
                                                  cl_int *error)
{
    AV_SCOPE("create_program_with_source");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateProgramWithSource", error);
    return cl_icd_binding.root().create_program(context, count, sources, lengths, noted.pointer());
}

cl_int CL_API_CALL build_program(cl_program program, cl_uint num_devices, const cl_device_id *device_list,
                                 const char *options, void(CL_CALLBACK *notify)(cl_program, void *), void *user_data)
{
    AV_SCOPE("build_program");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.noted("clBuildProgram", root.build_program(program, num_devices, device_list, options,
                                                           ClProgramNotify(notify, user_data)));
}

cl_int CL_API_CALL retain_program(cl_program program)
{
    AV_SCOPE("retain_program");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.noted("clRetainProgram", root.retain(program, ClObject::Kind::program));
}

cl_int CL_API_CALL release_program(cl_program program)
{
    AV_SCOPE("release_program");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.noted("clReleaseProgram", root.release(program, ClObject::Kind::program));
}

cl_kernel CL_API_CALL create_kernel(cl_program program, const char *name, cl_int *error)
{
    AV_SCOPE("create_kernel");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_PROGRAM;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateKernel", error);
    return cl_icd_binding.root().create_kernel(program, name, noted.pointer());
}

cl_int CL_API_CALL set_kernel_arg(cl_kernel kernel, cl_uint index, size_t bytes, const void *value)
{
    AV_SCOPE("set_kernel_arg");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.noted("clSetKernelArg", root.set_kernel_argument(kernel, index, bytes, value));
}

cl_int CL_API_CALL retain_kernel(cl_kernel kernel)
{
    AV_SCOPE("retain_kernel");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.noted("clRetainKernel", root.retain(kernel, ClObject::Kind::kernel));
}

cl_int CL_API_CALL release_kernel(cl_kernel kernel)
{
    AV_SCOPE("release_kernel");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.noted("clReleaseKernel", root.release(kernel, ClObject::Kind::kernel));
}

cl_int CL_API_CALL enqueue_ndrange(cl_command_queue queue, cl_kernel kernel, cl_uint dimensions,
                                   const size_t *offset, const size_t *global, const size_t *local,
                                   cl_uint waits, const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_ndrange");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueNDRangeKernel", root.enqueue_kernel(queue, kernel, dimensions, offset, global, local, event,
                                                                    ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_read(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                size_t offset, size_t bytes, void *into, cl_uint waits, const cl_event *events,
                                cl_event *event)
{
    AV_SCOPE("enqueue_read");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueReadBuffer", root.enqueue_read(queue, buffer, blocking, offset, bytes, into, event,
                                                               ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_write(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                 size_t offset, size_t bytes, const void *from, cl_uint waits,
                                 const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_write");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueWriteBuffer", root.enqueue_write(queue, buffer, blocking, offset, bytes, from, event,
                                                                 ClWaitList(waits, events)));
}

cl_int CL_API_CALL finish_queue(cl_command_queue queue)
{
    AV_SCOPE("finish_queue");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clFinish", root.finish(queue));
}

cl_int CL_API_CALL flush_queue(cl_command_queue queue)
{
    AV_SCOPE("flush_queue");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clFlush", root.flush(queue));
}

cl_int CL_API_CALL retain_event(cl_event event)
{
    AV_SCOPE("retain_event");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.noted("clRetainEvent", root.retain(event, ClObject::Kind::event));
}

void *CL_API_CALL map_buffer(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                             cl_map_flags flags, size_t offset, size_t bytes, cl_uint waits,
                             const cl_event *events, cl_event *event, cl_int *error)
{
    AV_SCOPE("map_buffer");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_COMMAND_QUEUE;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clEnqueueMapBuffer", error);
    return cl_icd_binding.root().map_buffer(queue, buffer, blocking, flags, offset, bytes, event, noted.pointer(),
                                            ClWaitList(waits, events));
}

cl_int CL_API_CALL unmap_mem_object(cl_command_queue queue, cl_mem buffer, void *pointer, cl_uint waits,
                                    const cl_event *events, cl_event *event)
{
    AV_SCOPE("unmap_mem_object");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueUnmapMemObject", root.unmap(queue, buffer, pointer, event, ClWaitList(waits, events)));
}

void *CL_API_CALL svm_alloc(cl_context context, cl_svm_mem_flags flags, size_t bytes,
                            cl_uint alignment)
{
    AV_SCOPE("svm_alloc");
    if (!cl_icd_binding.bound()) return nullptr;
    return cl_icd_binding.root().svm_alloc(context, flags, bytes, alignment);
}

void CL_API_CALL svm_free(cl_context context, void *pointer)
{
    AV_SCOPE("svm_free");
    if (!cl_icd_binding.bound()) return;
    cl_icd_binding.root().svm_free(context, pointer);
}

cl_int CL_API_CALL enqueue_svm_map(cl_command_queue queue, cl_bool blocking, cl_map_flags flags,
                                   void *pointer, size_t bytes, cl_uint waits, const cl_event *events,
                                   cl_event *event)
{
    AV_SCOPE("enqueue_svm_map");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueSVMMap", root.svm_map(queue, blocking, flags, pointer, bytes, event,
                                                      ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_svm_unmap(cl_command_queue queue, void *pointer, cl_uint waits,
                                     const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_svm_unmap");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueSVMUnmap", root.svm_unmap(queue, pointer, event, ClWaitList(waits, events)));
}

cl_int CL_API_CALL set_kernel_arg_svm_pointer(cl_kernel kernel, cl_uint index, const void *pointer)
{
    AV_SCOPE("set_kernel_arg_svm_pointer");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.noted("clSetKernelArgSVMPointer", root.set_kernel_svm_argument(kernel, index, pointer));
}

cl_int CL_API_CALL release_event(cl_event event)
{
    AV_SCOPE("release_event");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.noted("clReleaseEvent", root.release(event, ClObject::Kind::event));
}

cl_event CL_API_CALL create_user_event(cl_context context, cl_int *error)
{
    AV_SCOPE("create_user_event");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateUserEvent", error);
    return cl_icd_binding.root().create_user_event(context, noted.pointer());
}

cl_int CL_API_CALL set_user_event_status(cl_event event, cl_int status)
{
    AV_SCOPE("set_user_event_status");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.noted("clSetUserEventStatus", root.set_user_event_status(event, status));
}

cl_int CL_API_CALL wait_for_events(cl_uint count, const cl_event *events)
{
    AV_SCOPE("wait_for_events");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.noted("clWaitForEvents", root.wait_for_events(count, events));
}

cl_int CL_API_CALL get_event_info(cl_event event, cl_event_info name, size_t value_bytes,
                                  void *value, size_t *size_ret)
{
    AV_SCOPE("get_event_info");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.noted("clGetEventInfo", root.event_info(event, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL get_context_info(cl_context context, cl_context_info name, size_t value_bytes, void *value,
                                    size_t *size_ret)
{
    AV_SCOPE("get_context_info");
    AV_CL_ROOT_OR(CL_INVALID_CONTEXT);
    return root.noted("clGetContextInfo", root.context_info(context, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL get_mem_object_info(cl_mem object, cl_mem_info name, size_t value_bytes, void *value,
                                       size_t *size_ret)
{
    AV_SCOPE("get_mem_object_info");
    AV_CL_ROOT_OR(CL_INVALID_MEM_OBJECT);
    return root.noted("clGetMemObjectInfo", root.mem_object_info(object, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL get_image_info(cl_mem image, cl_image_info name, size_t value_bytes, void *value,
                                  size_t *size_ret)
{
    AV_SCOPE("get_image_info");
    AV_CL_ROOT_OR(CL_INVALID_MEM_OBJECT);
    return root.noted("clGetImageInfo", root.image_info(image, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL get_event_profiling_info(cl_event event, cl_profiling_info name, size_t value_bytes,
                                            void *value, size_t *size_ret)
{
    AV_SCOPE("get_event_profiling_info");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.noted("clGetEventProfilingInfo", root.event_profiling(event, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL set_event_callback(cl_event event, cl_int status,
                                      void(CL_CALLBACK *notify)(cl_event, cl_int, void *),
                                      void *user_data)
{
    AV_SCOPE("set_event_callback");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    if (notify == nullptr) return CL_INVALID_VALUE;
    return root.noted("clSetEventCallback", root.watch_event(event, status, notify, user_data));
}

cl_int CL_API_CALL get_program_build_info(cl_program program, cl_device_id device,
                                          cl_program_build_info name, size_t value_bytes,
                                          void *value, size_t *size_ret)
{
    AV_SCOPE("get_program_build_info");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.noted("clGetProgramBuildInfo", root.program_build_info(program, device, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL get_kernel_work_group_info(cl_kernel kernel, cl_device_id device,
                                              cl_kernel_work_group_info name, size_t value_bytes,
                                              void *value, size_t *size_ret)
{
    AV_SCOPE("get_kernel_work_group_info");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.noted("clGetKernelWorkGroupInfo", root.kernel_work_group_info(kernel, device, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL enqueue_copy_buffer(cl_command_queue queue, cl_mem from, cl_mem to,
                                       size_t from_offset, size_t to_offset, size_t bytes, cl_uint waits,
                                       const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_copy_buffer");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueCopyBuffer", root.enqueue_copy_buffer(queue, from, to, from_offset, to_offset, bytes,
                                                                      event, ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_fill_buffer(cl_command_queue queue, cl_mem buffer, const void *pattern,
                                       size_t pattern_bytes, size_t offset, size_t bytes, cl_uint waits,
                                       const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_fill_buffer");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueFillBuffer", root.enqueue_fill_buffer(queue, buffer, pattern, pattern_bytes, offset,
                                                                      bytes, event, ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_copy_image(cl_command_queue queue, cl_mem from, cl_mem to,
                                      const size_t *from_origin, const size_t *to_origin,
                                      const size_t *region, cl_uint waits, const cl_event *events,
                                      cl_event *event)
{
    AV_SCOPE("enqueue_copy_image");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueCopyImage", root.enqueue_copy_image(queue, from, to, from_origin, to_origin, region,
                                                                    event, ClWaitList(waits, events)));
}

cl_mem CL_API_CALL create_image(cl_context context, cl_mem_flags flags,
                                const cl_image_format *format, const cl_image_desc *desc,
                                void *host_pointer, cl_int *error)
{
    AV_SCOPE("create_image");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateImage", error);
    return cl_icd_binding.root().create_image(context, flags, format, desc, host_pointer, noted.pointer());
}

cl_mem CL_API_CALL create_image_2d(cl_context context, cl_mem_flags flags, const cl_image_format *format,
                                   size_t width, size_t height, size_t row_pitch, void *host_pointer, cl_int *error)
{
    AV_SCOPE("create_image_2d");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateImage2D", error);
    return cl_icd_binding.root().create_image_2d(context, flags, format, width, height, row_pitch, host_pointer,
                                                 noted.pointer());
}

cl_mem CL_API_CALL create_image_3d(cl_context context, cl_mem_flags flags, const cl_image_format *format,
                                   size_t width, size_t height, size_t depth, size_t row_pitch, size_t slice_pitch,
                                   void *host_pointer, cl_int *error)
{
    AV_SCOPE("create_image_3d");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateImage3D", error);
    return cl_icd_binding.root().create_image_3d(context, flags, format, width, height, depth, row_pitch,
                                                 slice_pitch, host_pointer, noted.pointer());
}

cl_int CL_API_CALL enqueue_read_image(cl_command_queue queue, cl_mem image, cl_bool,
                                      const size_t *origin, const size_t *region, size_t row_pitch,
                                      size_t slice_pitch, void *into, cl_uint waits, const cl_event *events,
                                      cl_event *event)
{
    AV_SCOPE("enqueue_read_image");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueReadImage", root.enqueue_read_image(queue, image, origin, region, row_pitch, slice_pitch,
                                                                    into, event, ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_read_buffer_rect(cl_command_queue queue, cl_mem buffer, cl_bool,
                                            const size_t *buffer_origin, const size_t *host_origin,
                                            const size_t *region, size_t buffer_row_pitch, size_t buffer_slice_pitch,
                                            size_t host_row_pitch, size_t host_slice_pitch, void *into, cl_uint waits,
                                            const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_read_buffer_rect");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueReadBufferRect",
                      root.enqueue_read_buffer_rect(queue, buffer,
                                                    ClBufferRect(buffer_origin, host_origin, region, buffer_row_pitch,
                                                                 buffer_slice_pitch, host_row_pitch, host_slice_pitch),
                                                    into, event, ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_write_buffer_rect(cl_command_queue queue, cl_mem buffer, cl_bool,
                                             const size_t *buffer_origin, const size_t *host_origin,
                                             const size_t *region, size_t buffer_row_pitch,
                                             size_t buffer_slice_pitch, size_t host_row_pitch,
                                             size_t host_slice_pitch, const void *from, cl_uint waits,
                                             const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_write_buffer_rect");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueWriteBufferRect",
                      root.enqueue_write_buffer_rect(queue, buffer,
                                                     ClBufferRect(buffer_origin, host_origin, region, buffer_row_pitch,
                                                                  buffer_slice_pitch, host_row_pitch, host_slice_pitch),
                                                     from, event, ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_copy_buffer_rect(cl_command_queue queue, cl_mem from, cl_mem to,
                                            const size_t *from_origin, const size_t *to_origin, const size_t *region,
                                            size_t from_row_pitch, size_t from_slice_pitch, size_t to_row_pitch,
                                            size_t to_slice_pitch, cl_uint waits, const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_copy_buffer_rect");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueCopyBufferRect",
                      root.enqueue_copy_buffer_rect(queue, from, to, from_origin, to_origin, region, from_row_pitch,
                                                    from_slice_pitch, to_row_pitch, to_slice_pitch, event,
                                                    ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_write_image(cl_command_queue queue, cl_mem image, cl_bool,
                                       const size_t *origin, const size_t *region, size_t row_pitch,
                                       size_t slice_pitch, const void *from, cl_uint waits,
                                       const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_write_image");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueWriteImage", root.enqueue_write_image(queue, image, origin, region, row_pitch,
                                                                      slice_pitch, from, event, ClWaitList(waits, events)));
}

cl_sampler CL_API_CALL create_sampler_with_properties(cl_context context,
                                                      const cl_sampler_properties *properties,
                                                      cl_int *error)
{
    AV_SCOPE("create_sampler_with_properties");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateSamplerWithProperties", error);
    return cl_icd_binding.root().create_sampler(
        context, reinterpret_cast<const uint64_t *>(properties), noted.pointer());
}

cl_sampler CL_API_CALL create_sampler(cl_context context, cl_bool normalized,
                                      cl_addressing_mode addressing, cl_filter_mode filter,
                                      cl_int *error)
{
    AV_SCOPE("create_sampler");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    /* The 1.2 call is the property list written out in its arguments. */
    const uint64_t properties[] = {CL_SAMPLER_NORMALIZED_COORDS, normalized,
                                   CL_SAMPLER_ADDRESSING_MODE,   addressing,
                                   CL_SAMPLER_FILTER_MODE,       filter,
                                   0u};
    ClNotedError noted(cl_icd_binding.root(), "clCreateSampler", error);
    return cl_icd_binding.root().create_sampler(context, properties, noted.pointer());
}

cl_int CL_API_CALL retain_sampler(cl_sampler sampler)
{
    AV_SCOPE("retain_sampler");
    AV_CL_ROOT_OR(CL_INVALID_SAMPLER);
    return root.noted("clRetainSampler", root.retain(sampler, ClObject::Kind::sampler));
}

cl_int CL_API_CALL release_sampler(cl_sampler sampler)
{
    AV_SCOPE("release_sampler");
    AV_CL_ROOT_OR(CL_INVALID_SAMPLER);
    return root.noted("clReleaseSampler", root.release(sampler, ClObject::Kind::sampler));
}

cl_int CL_API_CALL get_supported_image_formats(cl_context context, cl_mem_flags flags,
                                               cl_mem_object_type type, cl_uint num_entries,
                                               cl_image_format *formats, cl_uint *num_formats)
{
    AV_SCOPE("get_supported_image_formats");
    AV_CL_ROOT_OR(CL_INVALID_CONTEXT);
    return root.noted("clGetSupportedImageFormats", root.supported_image_formats(context, flags, type, num_entries, formats, num_formats));
}

cl_int CL_API_CALL enqueue_marker(cl_command_queue queue, cl_uint count, const cl_event *events,
                                  cl_event *event)
{
    AV_SCOPE("enqueue_marker");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueMarkerWithWaitList", root.enqueue_marker(queue, count, events, event));
}

cl_int CL_API_CALL enqueue_barrier(cl_command_queue queue, cl_uint count, const cl_event *events,
                                   cl_event *event)
{
    AV_SCOPE("enqueue_barrier");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueBarrierWithWaitList", root.enqueue_barrier(queue, count, events, event));
}

/* cl_khr_d3d10_sharing: its six slots of cl_icd_dispatch, which the loader's
 * own entry points for the names dispatch through. Each takes the Direct3D
 * objects as the pointers they are; the slots' declared types name them by
 * interface only where d3d10.h is included, which portable code is not. */
cl_int CL_API_CALL get_device_ids_from_d3d10(cl_platform_id platform, cl_uint source, void *object, cl_uint set,
                                             cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices)
{
    AV_SCOPE("get_device_ids_from_d3d10");
    AV_CL_ROOT_OR(CL_INVALID_PLATFORM);
    return root.noted("clGetDeviceIDsFromD3D10KHR",
                      root.device_ids_from_d3d10(platform, source, object, set, num_entries, devices, num_devices));
}

cl_mem CL_API_CALL create_from_d3d10_buffer(cl_context context, cl_mem_flags, void *, cl_int *error)
{
    AV_SCOPE("create_from_d3d10_buffer");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateFromD3D10BufferKHR", error);
    return cl_icd_binding.root().create_from_d3d10(context, noted.pointer());
}

cl_mem CL_API_CALL create_from_d3d10_texture(cl_context context, cl_mem_flags, void *, cl_uint, cl_int *error)
{
    AV_SCOPE("create_from_d3d10_texture");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateFromD3D10TextureKHR", error);
    return cl_icd_binding.root().create_from_d3d10(context, noted.pointer());
}

cl_int CL_API_CALL enqueue_acquire_d3d10(cl_command_queue queue, cl_uint, const cl_mem *, cl_uint, const cl_event *,
                                         cl_event *)
{
    AV_SCOPE("enqueue_acquire_d3d10");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueAcquireD3D10ObjectsKHR", root.enqueue_d3d10_objects(queue));
}

cl_int CL_API_CALL enqueue_release_d3d10(cl_command_queue queue, cl_uint, const cl_mem *, cl_uint, const cl_event *,
                                         cl_event *)
{
    AV_SCOPE("enqueue_release_d3d10");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueReleaseD3D10ObjectsKHR", root.enqueue_d3d10_objects(queue));
}

/* cl_khr_d3d11_sharing's six (docs/d3d11-sharing.md), with the Direct3D
 * objects as the pointers they are, as for Direct3D 10. */
cl_int CL_API_CALL get_device_ids_from_d3d11(cl_platform_id platform, cl_uint source, void *object, cl_uint set,
                                             cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices)
{
    AV_SCOPE("get_device_ids_from_d3d11");
    AV_CL_ROOT_OR(CL_INVALID_PLATFORM);
    return root.noted("clGetDeviceIDsFromD3D11KHR",
                      root.device_ids_from_d3d11(platform, source, object, set, num_entries, devices, num_devices));
}

cl_mem CL_API_CALL create_from_d3d11_buffer(cl_context context, cl_mem_flags flags, void *resource, cl_int *error)
{
    AV_SCOPE("create_from_d3d11_buffer");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateFromD3D11BufferKHR", error);
    return cl_icd_binding.root().create_from_d3d11_buffer(context, flags, resource, noted.pointer());
}

cl_mem CL_API_CALL create_from_d3d11_texture2d(cl_context context, cl_mem_flags flags, void *resource,
                                               cl_uint subresource, cl_int *error)
{
    AV_SCOPE("create_from_d3d11_texture2d");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateFromD3D11Texture2DKHR", error);
    return cl_icd_binding.root().create_from_d3d11_texture(context, flags, resource, subresource,
                                                           D3d11Resource::Kind::texture2d, noted.pointer());
}

cl_mem CL_API_CALL create_from_d3d11_texture3d(cl_context context, cl_mem_flags flags, void *resource,
                                               cl_uint subresource, cl_int *error)
{
    AV_SCOPE("create_from_d3d11_texture3d");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateFromD3D11Texture3DKHR", error);
    return cl_icd_binding.root().create_from_d3d11_texture(context, flags, resource, subresource,
                                                           D3d11Resource::Kind::texture3d, noted.pointer());
}

cl_int CL_API_CALL enqueue_acquire_d3d11(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                         const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_acquire_d3d11");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueAcquireD3D11ObjectsKHR",
                      root.enqueue_acquire_d3d11(queue, count, objects, waits, events, event));
}

cl_int CL_API_CALL enqueue_release_d3d11(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                         const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_release_d3d11");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueReleaseD3D11ObjectsKHR",
                      root.enqueue_release_d3d11(queue, count, objects, waits, events, event));
}

/* The device's retain and release, which answer alike for the root device
 * this ICD shows; two entries so the log names the one an application called. */
cl_int CL_API_CALL retain_device(cl_device_id device)
{
    AV_SCOPE("retain_device");
    AV_CL_ROOT_OR(CL_INVALID_DEVICE);
    return root.noted("clRetainDevice", root.device_reference(device));
}

cl_int CL_API_CALL release_device(cl_device_id device)
{
    AV_SCOPE("release_device");
    AV_CL_ROOT_OR(CL_INVALID_DEVICE);
    return root.noted("clReleaseDevice", root.device_reference(device));
}

/* cl_khr_gl_sharing: each slot of cl_icd_dispatch, and clGetGLContextInfoKHR
 * also by name, since an application may ask the ICD for it directly. */
cl_mem CL_API_CALL create_from_gl_buffer(cl_context context, cl_mem_flags flags, cl_GLuint buffer, cl_int *error)
{
    AV_SCOPE("create_from_gl_buffer");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateFromGLBuffer", error);
    return cl_icd_binding.root().create_from_gl_buffer(context, flags, buffer, noted.pointer());
}

cl_mem CL_API_CALL create_from_gl_texture(cl_context context, cl_mem_flags flags, cl_GLenum target,
                                          cl_GLint level, cl_GLuint texture, cl_int *error)
{
    AV_SCOPE("create_from_gl_texture");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateFromGLTexture", error);
    return cl_icd_binding.root().create_from_gl_texture(context, flags, target, level, texture, noted.pointer());
}

cl_mem CL_API_CALL create_from_gl_renderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                               cl_int *error)
{
    AV_SCOPE("create_from_gl_renderbuffer");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateFromGLRenderbuffer", error);
    return cl_icd_binding.root().create_from_gl_renderbuffer(context, flags, renderbuffer, noted.pointer());
}

cl_int CL_API_CALL get_gl_object_info(cl_mem object, cl_gl_object_type *type, cl_GLuint *name)
{
    AV_SCOPE("get_gl_object_info");
    AV_CL_ROOT_OR(CL_INVALID_MEM_OBJECT);
    return root.noted("clGetGLObjectInfo", root.gl_object_info(object, type, name));
}

cl_int CL_API_CALL get_gl_texture_info(cl_mem object, cl_gl_texture_info name, size_t value_bytes, void *value,
                                       size_t *size_ret)
{
    AV_SCOPE("get_gl_texture_info");
    AV_CL_ROOT_OR(CL_INVALID_MEM_OBJECT);
    return root.noted("clGetGLTextureInfo", root.gl_texture_info(object, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL enqueue_acquire_gl(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                      const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_acquire_gl");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueAcquireGLObjects", root.enqueue_acquire_gl(queue, count, objects, waits, events, event));
}

cl_int CL_API_CALL enqueue_release_gl(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                      const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_release_gl");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueReleaseGLObjects", root.enqueue_release_gl(queue, count, objects, waits, events, event));
}

cl_int CL_API_CALL get_gl_context_info(const cl_context_properties *properties, cl_gl_context_info name,
                                       size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("get_gl_context_info");
    AV_CL_ROOT_OR(CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR);
    return root.noted("clGetGLContextInfoKHR", root.gl_context_info(properties, name, value_bytes, value, size_ret));
}

/* `cl_khr_external_memory`'s two, which the loader asks for by name: they are
 * not slots of cl_icd_dispatch, because the ICD ABI predates the extension. */
cl_int CL_API_CALL acquire_external_objects(cl_command_queue queue, cl_uint num_mem_objects,
                                           const cl_mem *mem_objects, cl_uint waits, const cl_event *events,
                                           cl_event *event)
{
    AV_SCOPE("acquire_external_objects");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_external(queue, num_mem_objects, mem_objects, event, ClWaitList(waits, events));
}

cl_int CL_API_CALL release_external_objects(cl_command_queue queue, cl_uint num_mem_objects,
                                           const cl_mem *mem_objects, cl_uint waits, const cl_event *events,
                                           cl_event *event)
{
    AV_SCOPE("release_external_objects");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_external(queue, num_mem_objects, mem_objects, event, ClWaitList(waits, events));
}

/* The rest of the enqueue surface and the memory objects it names: the 1.x
 * spellings, image-buffer copies, image fills and maps, migration,
 * sub-buffers, the properties spelling of an image, pipes, and the queue's
 * own shared-memory commands. */
cl_int CL_API_CALL enqueue_task(cl_command_queue queue, cl_kernel kernel, cl_uint waits, const cl_event *events,
                                cl_event *event)
{
    AV_SCOPE("enqueue_task");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueTask", root.enqueue_task(queue, kernel, event, ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_marker_1_1(cl_command_queue queue, cl_event *event)
{
    AV_SCOPE("enqueue_marker_1_1");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueMarker", root.enqueue_event_marker(queue, event));
}

cl_int CL_API_CALL enqueue_wait_for_events(cl_command_queue queue, cl_uint count, const cl_event *events)
{
    AV_SCOPE("enqueue_wait_for_events");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueWaitForEvents", root.enqueue_wait_for_events(queue, count, events));
}

cl_int CL_API_CALL enqueue_barrier_1_1(cl_command_queue queue)
{
    AV_SCOPE("enqueue_barrier_1_1");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueBarrier", root.enqueue_barrier(queue, 0u, nullptr, nullptr));
}

cl_int CL_API_CALL enqueue_copy_image_to_buffer(cl_command_queue queue, cl_mem image, cl_mem buffer,
                                                const size_t *origin, const size_t *region, size_t offset,
                                                cl_uint waits, const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_copy_image_to_buffer");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueCopyImageToBuffer",
                      root.enqueue_copy_image_to_buffer(queue, image, buffer, origin, region, offset, event,
                                                        ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_copy_buffer_to_image(cl_command_queue queue, cl_mem buffer, cl_mem image, size_t offset,
                                                const size_t *origin, const size_t *region, cl_uint waits,
                                                const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_copy_buffer_to_image");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueCopyBufferToImage",
                      root.enqueue_copy_buffer_to_image(queue, buffer, image, offset, origin, region, event,
                                                        ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_fill_image(cl_command_queue queue, cl_mem image, const void *colour, const size_t *origin,
                                      const size_t *region, cl_uint waits, const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_fill_image");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueFillImage",
                      root.enqueue_fill_image(queue, image, colour, origin, region, event, ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_migrate(cl_command_queue queue, cl_uint count, const cl_mem *objects,
                                   cl_mem_migration_flags flags, cl_uint waits, const cl_event *events,
                                   cl_event *event)
{
    AV_SCOPE("enqueue_migrate");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueMigrateMemObjects",
                      root.enqueue_migrate(queue, count, objects, flags, event, ClWaitList(waits, events)));
}

cl_mem CL_API_CALL create_sub_buffer(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type type, const void *info,
                                     cl_int *error)
{
    AV_SCOPE("create_sub_buffer");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_MEM_OBJECT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateSubBuffer", error);
    return cl_icd_binding.root().create_sub_buffer(buffer, flags, type, info, noted.pointer());
}

void *CL_API_CALL map_image(cl_command_queue queue, cl_mem image, cl_bool blocking, cl_map_flags flags,
                            const size_t *origin, const size_t *region, size_t *row_pitch, size_t *slice_pitch,
                            cl_uint waits, const cl_event *events, cl_event *event, cl_int *error)
{
    AV_SCOPE("map_image");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_COMMAND_QUEUE;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clEnqueueMapImage", error);
    return cl_icd_binding.root().map_image(queue, image, blocking, flags, origin, region, row_pitch, slice_pitch,
                                           event, noted.pointer(), ClWaitList(waits, events));
}

cl_mem CL_API_CALL create_image_with_properties(cl_context context, const cl_mem_properties *properties,
                                                cl_mem_flags flags, const cl_image_format *format,
                                                const cl_image_desc *desc, void *host_pointer, cl_int *error)
{
    AV_SCOPE("create_image_with_properties");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateImageWithProperties", error);
    return cl_icd_binding.root().create_image_with_properties(context, properties, flags, format, desc, host_pointer,
                                                              noted.pointer());
}

cl_mem CL_API_CALL create_pipe(cl_context context, cl_mem_flags flags, cl_uint packet_bytes, cl_uint packets,
                               const cl_pipe_properties *properties, cl_int *error)
{
    AV_SCOPE("create_pipe");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreatePipe", error);
    return cl_icd_binding.root().create_pipe(context, flags, packet_bytes, packets, properties, noted.pointer());
}

cl_int CL_API_CALL get_pipe_info(cl_mem pipe, cl_pipe_info name, size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("get_pipe_info");
    AV_CL_ROOT_OR(CL_INVALID_MEM_OBJECT);
    return root.noted("clGetPipeInfo", root.pipe_info(pipe, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL enqueue_svm_memcpy(cl_command_queue queue, cl_bool blocking, void *to, const void *from,
                                      size_t bytes, cl_uint waits, const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_svm_memcpy");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueSVMMemcpy",
                      root.svm_memcpy(queue, blocking, to, from, bytes, event, ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_svm_mem_fill(cl_command_queue queue, void *pointer, const void *pattern,
                                        size_t pattern_bytes, size_t bytes, cl_uint waits, const cl_event *events,
                                        cl_event *event)
{
    AV_SCOPE("enqueue_svm_mem_fill");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueSVMMemFill", root.svm_mem_fill(queue, pointer, pattern, pattern_bytes, bytes, event,
                                                               ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_svm_free(cl_command_queue queue, cl_uint count, void *pointers[],
                                    void(CL_CALLBACK *freed)(cl_command_queue, cl_uint, void *[], void *),
                                    void *user_data, cl_uint waits, const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_svm_free");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueSVMFree", root.enqueue_svm_free(queue, count, pointers, freed, user_data, event,
                                                                ClWaitList(waits, events)));
}

cl_int CL_API_CALL enqueue_svm_migrate(cl_command_queue queue, cl_uint count, const void **pointers, const size_t *,
                                       cl_mem_migration_flags, cl_uint waits, const cl_event *events, cl_event *event)
{
    AV_SCOPE("enqueue_svm_migrate");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.noted("clEnqueueSVMMigrateMem",
                      root.enqueue_svm_migrate(queue, count, pointers, event, ClWaitList(waits, events)));
}

/* Programs and kernels beyond the one-source build (cl_icd_root_programs.cpp). */
cl_program CL_API_CALL create_program_with_binary(cl_context context, cl_uint num_devices,
                                                  const cl_device_id *device_list, const size_t *lengths,
                                                  const unsigned char **binaries, cl_int *binary_status,
                                                  cl_int *error)
{
    AV_SCOPE("create_program_with_binary");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateProgramWithBinary", error);
    return cl_icd_binding.root().create_program_with_binary(context, num_devices, device_list, lengths, binaries,
                                                            binary_status, noted.pointer());
}

cl_program CL_API_CALL create_program_with_il(cl_context context, const void *il, size_t length, cl_int *error)
{
    AV_SCOPE("create_program_with_il");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateProgramWithIL", error);
    return cl_icd_binding.root().create_program_with_il(context, il, length, noted.pointer());
}

cl_program CL_API_CALL create_program_with_built_in_kernels(cl_context context, cl_uint num_devices,
                                                            const cl_device_id *device_list,
                                                            const char *kernel_names, cl_int *error)
{
    AV_SCOPE("create_program_with_built_in_kernels");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateProgramWithBuiltInKernels", error);
    return cl_icd_binding.root().create_program_with_built_in_kernels(context, num_devices, device_list, kernel_names,
                                                                      noted.pointer());
}

cl_int CL_API_CALL compile_program(cl_program program, cl_uint num_devices, const cl_device_id *device_list,
                                   const char *options, cl_uint num_headers, const cl_program *headers,
                                   const char **header_names, void(CL_CALLBACK *notify)(cl_program, void *),
                                   void *user_data)
{
    AV_SCOPE("compile_program");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.noted("clCompileProgram",
                      root.compile_program(program, num_devices, device_list, options, num_headers, headers,
                                           header_names, ClProgramNotify(notify, user_data)));
}

cl_program CL_API_CALL link_program(cl_context context, cl_uint num_devices, const cl_device_id *device_list,
                                    const char *options, cl_uint num_inputs, const cl_program *inputs,
                                    void(CL_CALLBACK *notify)(cl_program, void *), void *user_data, cl_int *error)
{
    AV_SCOPE("link_program");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clLinkProgram", error);
    return cl_icd_binding.root().link_program(context, num_devices, device_list, options, num_inputs, inputs,
                                              ClProgramNotify(notify, user_data), noted.pointer());
}

cl_int CL_API_CALL get_program_info(cl_program program, cl_program_info name, size_t value_bytes, void *value,
                                    size_t *size_ret)
{
    AV_SCOPE("get_program_info");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.noted_query("clGetProgramInfo", name, root.program_info(program, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL set_program_specialization_constant(cl_program program, cl_uint id, size_t bytes,
                                                       const void *value)
{
    AV_SCOPE("set_program_specialization_constant");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.noted("clSetProgramSpecializationConstant", root.set_specialization_constant(program, id, bytes, value));
}

cl_int CL_API_CALL set_program_release_callback(cl_program program, void(CL_CALLBACK *notify)(cl_program, void *),
                                                void *)
{
    AV_SCOPE("set_program_release_callback");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.noted("clSetProgramReleaseCallback", root.set_program_release_callback(program, notify));
}

cl_int CL_API_CALL unload_platform_compiler(cl_platform_id platform)
{
    AV_SCOPE("unload_platform_compiler");
    AV_CL_ROOT_OR(CL_INVALID_PLATFORM);
    return root.noted("clUnloadPlatformCompiler", root.unload_platform_compiler(platform));
}

/* The 1.1 spelling, of no platform: a hint, as the 1.2 one is. */
cl_int CL_API_CALL unload_compiler()
{
    AV_SCOPE("unload_compiler");
    return CL_SUCCESS;
}

cl_int CL_API_CALL create_kernels_in_program(cl_program program, cl_uint num_kernels, cl_kernel *kernels,
                                             cl_uint *num_kernels_ret)
{
    AV_SCOPE("create_kernels_in_program");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.noted("clCreateKernelsInProgram",
                      root.create_kernels_in_program(program, num_kernels, kernels, num_kernels_ret));
}

cl_kernel CL_API_CALL clone_kernel(cl_kernel kernel, cl_int *error)
{
    AV_SCOPE("clone_kernel");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_KERNEL;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCloneKernel", error);
    return cl_icd_binding.root().clone_kernel(kernel, noted.pointer());
}

cl_int CL_API_CALL get_kernel_info(cl_kernel kernel, cl_kernel_info name, size_t value_bytes, void *value,
                                   size_t *size_ret)
{
    AV_SCOPE("get_kernel_info");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.noted_query("clGetKernelInfo", name, root.kernel_info(kernel, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL get_kernel_arg_info(cl_kernel kernel, cl_uint index, cl_kernel_arg_info name, size_t value_bytes,
                                       void *value, size_t *size_ret)
{
    AV_SCOPE("get_kernel_arg_info");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.noted_query("clGetKernelArgInfo", name,
                            root.kernel_arg_info(kernel, index, name, value_bytes, value, size_ret));
}

/* The core call and cl_khr_subgroups' slot, which take the same arguments. */
cl_int CL_API_CALL get_kernel_sub_group_info(cl_kernel kernel, cl_device_id device, cl_kernel_sub_group_info name,
                                             size_t input_bytes, const void *input, size_t value_bytes, void *value,
                                             size_t *size_ret)
{
    AV_SCOPE("get_kernel_sub_group_info");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.noted_query("clGetKernelSubGroupInfo", name,
                            root.kernel_sub_group_info(kernel, device, name, input_bytes, input, value_bytes, value,
                                                       size_ret));
}

cl_int CL_API_CALL set_kernel_exec_info(cl_kernel kernel, cl_kernel_exec_info name, size_t bytes, const void *value)
{
    AV_SCOPE("set_kernel_exec_info");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.noted("clSetKernelExecInfo", root.set_kernel_exec_info(kernel, name, bytes, value));
}

/* A context from a device type, which carries its notify as clCreateContext's
 * does. */
cl_context CL_API_CALL create_context_from_type(const cl_context_properties *properties, cl_device_type type,
                                                void(CL_CALLBACK *notify)(const char *, const void *, size_t, void *),
                                                void *user_data, cl_int *error)
{
    AV_SCOPE("create_context_from_type");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_PLATFORM;
        return nullptr;
    }
    ClNotedError noted(cl_icd_binding.root(), "clCreateContextFromType", error);
    return cl_icd_binding.root().create_context_from_type(properties, type, noted.pointer(),
                                                          clicd::ClContextNotify(notify, user_data));
}

cl_int CL_API_CALL get_sampler_info(cl_sampler sampler, cl_sampler_info name, size_t value_bytes, void *value,
                                    size_t *size_ret)
{
    AV_SCOPE("get_sampler_info");
    AV_CL_ROOT_OR(CL_INVALID_SAMPLER);
    return root.noted_query("clGetSamplerInfo", name, root.sampler_info(sampler, name, value_bytes, value, size_ret));
}

cl_int CL_API_CALL set_mem_object_destructor_callback(cl_mem object, void(CL_CALLBACK *notify)(cl_mem, void *),
                                                      void *user_data)
{
    AV_SCOPE("set_mem_object_destructor_callback");
    AV_CL_ROOT_OR(CL_INVALID_MEM_OBJECT);
    return root.noted("clSetMemObjectDestructorCallback", root.watch_destruction(object, notify, user_data));
}

cl_int CL_API_CALL set_context_destructor_callback(cl_context context,
                                                   void(CL_CALLBACK *notify)(cl_context, void *), void *user_data)
{
    AV_SCOPE("set_context_destructor_callback");
    AV_CL_ROOT_OR(CL_INVALID_CONTEXT);
    return root.noted("clSetContextDestructorCallback", root.watch_destruction(context, notify, user_data));
}

cl_int CL_API_CALL create_sub_devices(cl_device_id device, const cl_device_partition_property *properties,
                                      cl_uint num_devices, cl_device_id *out_devices, cl_uint *num_devices_ret)
{
    AV_SCOPE("create_sub_devices");
    AV_CL_ROOT_OR(CL_INVALID_DEVICE);
    return root.noted("clCreateSubDevices",
                      root.create_sub_devices(device, properties, num_devices, out_devices, num_devices_ret));
}

/* The loader's own clGetExtensionFunctionAddressForPlatform dispatches here
 * through the platform. */
void *CL_API_CALL extension_function_for_platform(cl_platform_id platform, const char *name)
{
    AV_SCOPE("extension_function_for_platform");
    if (!cl_icd_binding.bound()) return nullptr;
    return cl_icd_binding.root().extension_function_for(platform, name);
}

#undef AV_CL_ROOT_OR

}  // namespace

ClDispatchTable::ClDispatchTable() : table_(), claimed_(0u), filled_(false)
{
    AV_SCOPE("ClDispatchTable::ClDispatchTable");
}

void ClDispatchTable::fill()
{
    AV_SCOPE("ClDispatchTable::fill");
    if (filled_) return;
    table_.clGetPlatformIDs = &get_platform_ids;
    table_.clGetPlatformInfo = &get_platform_info;
    table_.clGetDeviceIDs = &get_device_ids;
    table_.clGetDeviceInfo = &get_device_info;
    table_.clCreateContext = &create_context;
    table_.clRetainContext = &retain_context;
    table_.clReleaseContext = &release_context;
    table_.clCreateCommandQueue = &create_queue;
    table_.clCreateCommandQueueWithProperties = &create_queue_with_properties;
    table_.clRetainCommandQueue = &retain_queue;
    table_.clReleaseCommandQueue = &release_queue;
    table_.clCreateBuffer = &create_buffer;
    table_.clCreateBufferWithProperties = &create_buffer_with_properties;
    table_.clRetainMemObject = &retain_mem;
    table_.clReleaseMemObject = &release_mem;
    table_.clCreateProgramWithSource = &create_program_with_source;
    table_.clBuildProgram = &build_program;
    table_.clRetainProgram = &retain_program;
    table_.clReleaseProgram = &release_program;
    table_.clCreateKernel = &create_kernel;
    table_.clSetKernelArg = &set_kernel_arg;
    table_.clRetainKernel = &retain_kernel;
    table_.clReleaseKernel = &release_kernel;
    table_.clEnqueueNDRangeKernel = &enqueue_ndrange;
    table_.clEnqueueReadBuffer = &enqueue_read;
    table_.clEnqueueWriteBuffer = &enqueue_write;
    table_.clFinish = &finish_queue;
    table_.clFlush = &flush_queue;
    table_.clRetainEvent = &retain_event;
    table_.clReleaseEvent = &release_event;
    table_.clEnqueueMapBuffer = &map_buffer;
    table_.clEnqueueUnmapMemObject = &unmap_mem_object;
    table_.clSVMAlloc = &svm_alloc;
    table_.clSVMFree = &svm_free;
    table_.clEnqueueSVMMap = &enqueue_svm_map;
    table_.clEnqueueSVMUnmap = &enqueue_svm_unmap;
    table_.clSetKernelArgSVMPointer = &set_kernel_arg_svm_pointer;
    table_.clCreateUserEvent = &create_user_event;
    table_.clSetUserEventStatus = &set_user_event_status;
    table_.clWaitForEvents = &wait_for_events;
    table_.clGetEventInfo = &get_event_info;
    table_.clGetEventProfilingInfo = &get_event_profiling_info;
    table_.clGetContextInfo = &get_context_info;
    table_.clGetMemObjectInfo = &get_mem_object_info;
    table_.clGetImageInfo = &get_image_info;
    table_.clSetEventCallback = &set_event_callback;
    table_.clGetProgramBuildInfo = &get_program_build_info;
    table_.clGetKernelWorkGroupInfo = &get_kernel_work_group_info;
    table_.clEnqueueCopyBuffer = &enqueue_copy_buffer;
    table_.clEnqueueReadBufferRect = &enqueue_read_buffer_rect;
    table_.clEnqueueWriteBufferRect = &enqueue_write_buffer_rect;
    table_.clEnqueueCopyBufferRect = &enqueue_copy_buffer_rect;
    table_.clEnqueueFillBuffer = &enqueue_fill_buffer;
    table_.clEnqueueCopyImage = &enqueue_copy_image;
    table_.clCreateImage = &create_image;
    table_.clCreateImage2D = &create_image_2d;
    table_.clCreateImage3D = &create_image_3d;
    table_.clEnqueueReadImage = &enqueue_read_image;
    table_.clEnqueueWriteImage = &enqueue_write_image;
    table_.clCreateSamplerWithProperties = &create_sampler_with_properties;
    table_.clCreateSampler = &create_sampler;
    table_.clRetainSampler = &retain_sampler;
    table_.clReleaseSampler = &release_sampler;
    table_.clGetSupportedImageFormats = &get_supported_image_formats;
    table_.clEnqueueMarkerWithWaitList = &enqueue_marker;
    table_.clEnqueueBarrierWithWaitList = &enqueue_barrier;
    table_.clCreateFromGLBuffer = &create_from_gl_buffer;
    table_.clCreateFromGLTexture = &create_from_gl_texture;
    table_.clCreateFromGLTexture2D = &create_from_gl_texture;
    table_.clCreateFromGLTexture3D = &create_from_gl_texture;
    table_.clCreateFromGLRenderbuffer = &create_from_gl_renderbuffer;
    table_.clGetGLObjectInfo = &get_gl_object_info;
    table_.clGetGLTextureInfo = &get_gl_texture_info;
    table_.clEnqueueAcquireGLObjects = &enqueue_acquire_gl;
    table_.clEnqueueReleaseGLObjects = &enqueue_release_gl;
    table_.clGetGLContextInfoKHR = &get_gl_context_info;
    table_.clGetDeviceIDsFromD3D10KHR =
        reinterpret_cast<decltype(table_.clGetDeviceIDsFromD3D10KHR)>(&get_device_ids_from_d3d10);
    table_.clCreateFromD3D10BufferKHR =
        reinterpret_cast<decltype(table_.clCreateFromD3D10BufferKHR)>(&create_from_d3d10_buffer);
    table_.clCreateFromD3D10Texture2DKHR =
        reinterpret_cast<decltype(table_.clCreateFromD3D10Texture2DKHR)>(&create_from_d3d10_texture);
    table_.clCreateFromD3D10Texture3DKHR =
        reinterpret_cast<decltype(table_.clCreateFromD3D10Texture3DKHR)>(&create_from_d3d10_texture);
    table_.clEnqueueAcquireD3D10ObjectsKHR =
        reinterpret_cast<decltype(table_.clEnqueueAcquireD3D10ObjectsKHR)>(&enqueue_acquire_d3d10);
    table_.clEnqueueReleaseD3D10ObjectsKHR =
        reinterpret_cast<decltype(table_.clEnqueueReleaseD3D10ObjectsKHR)>(&enqueue_release_d3d10);
    table_.clGetDeviceIDsFromD3D11KHR =
        reinterpret_cast<decltype(table_.clGetDeviceIDsFromD3D11KHR)>(&get_device_ids_from_d3d11);
    table_.clCreateFromD3D11BufferKHR =
        reinterpret_cast<decltype(table_.clCreateFromD3D11BufferKHR)>(&create_from_d3d11_buffer);
    table_.clCreateFromD3D11Texture2DKHR =
        reinterpret_cast<decltype(table_.clCreateFromD3D11Texture2DKHR)>(&create_from_d3d11_texture2d);
    table_.clCreateFromD3D11Texture3DKHR =
        reinterpret_cast<decltype(table_.clCreateFromD3D11Texture3DKHR)>(&create_from_d3d11_texture3d);
    table_.clEnqueueAcquireD3D11ObjectsKHR =
        reinterpret_cast<decltype(table_.clEnqueueAcquireD3D11ObjectsKHR)>(&enqueue_acquire_d3d11);
    table_.clEnqueueReleaseD3D11ObjectsKHR =
        reinterpret_cast<decltype(table_.clEnqueueReleaseD3D11ObjectsKHR)>(&enqueue_release_d3d11);
    table_.clRetainDevice = &retain_device;
    table_.clReleaseDevice = &release_device;
    table_.clGetCommandQueueInfo = &get_command_queue_info;
    table_.clCreateProgramWithBinary = &create_program_with_binary;
    table_.clCreateProgramWithIL = &create_program_with_il;
    table_.clCreateProgramWithBuiltInKernels = &create_program_with_built_in_kernels;
    table_.clCompileProgram = &compile_program;
    table_.clLinkProgram = &link_program;
    table_.clGetProgramInfo = &get_program_info;
    table_.clSetProgramSpecializationConstant = &set_program_specialization_constant;
    table_.clSetProgramReleaseCallback = &set_program_release_callback;
    table_.clUnloadPlatformCompiler = &unload_platform_compiler;
    table_.clUnloadCompiler = &unload_compiler;
    table_.clCreateKernelsInProgram = &create_kernels_in_program;
    table_.clCloneKernel = &clone_kernel;
    table_.clGetKernelInfo = &get_kernel_info;
    table_.clGetKernelArgInfo = &get_kernel_arg_info;
    table_.clGetKernelSubGroupInfo = &get_kernel_sub_group_info;
    table_.clGetKernelSubGroupInfoKHR = &get_kernel_sub_group_info;
    table_.clSetKernelExecInfo = &set_kernel_exec_info;
    table_.clCreateContextFromType = &create_context_from_type;
    table_.clGetSamplerInfo = &get_sampler_info;
    table_.clSetMemObjectDestructorCallback = &set_mem_object_destructor_callback;
    table_.clSetContextDestructorCallback = &set_context_destructor_callback;
    table_.clCreateSubDevices = &create_sub_devices;
    table_.clGetExtensionFunctionAddressForPlatform = &extension_function_for_platform;
    table_.clEnqueueTask = &enqueue_task;
    table_.clEnqueueMarker = &enqueue_marker_1_1;
    table_.clEnqueueWaitForEvents = &enqueue_wait_for_events;
    table_.clEnqueueBarrier = &enqueue_barrier_1_1;
    table_.clEnqueueCopyImageToBuffer = &enqueue_copy_image_to_buffer;
    table_.clEnqueueCopyBufferToImage = &enqueue_copy_buffer_to_image;
    table_.clEnqueueFillImage = &enqueue_fill_image;
    table_.clEnqueueMigrateMemObjects = &enqueue_migrate;
    table_.clCreateSubBuffer = &create_sub_buffer;
    table_.clEnqueueMapImage = &map_image;
    table_.clCreateImageWithProperties = &create_image_with_properties;
    table_.clCreatePipe = &create_pipe;
    table_.clGetPipeInfo = &get_pipe_info;
    table_.clEnqueueSVMMemcpy = &enqueue_svm_memcpy;
    table_.clEnqueueSVMMemFill = &enqueue_svm_mem_fill;
    table_.clEnqueueSVMFree = &enqueue_svm_free;
    table_.clEnqueueSVMMigrateMem = &enqueue_svm_migrate;
    claimed_ = 131u;
    /* Last, and only into slots nothing above claimed. */
    av::clproto::fill_unimplemented(table_);
    filled_ = true;
}

bool ClDispatchTable::filled() const
{
    AV_SCOPE("ClDispatchTable::filled");
    return filled_;
}

const cl_icd_dispatch &ClDispatchTable::table() const
{
    AV_SCOPE("ClDispatchTable::table");
    return table_;
}

/* Not a slot of the table: cl_icd_dispatch has no clIcdGetPlatformIDsKHR, and
 * the loader asks clGetExtensionFunctionAddress for it by name before it has a
 * platform to dispatch through. */
void *ClDispatchTable::icd_get_platform_ids() const
{
    AV_SCOPE("ClDispatchTable::icd_get_platform_ids");
    return reinterpret_cast<void *>(&icd_get_platform_ids_entry);
}

void *ClDispatchTable::external_mem_acquire() const
{
    AV_SCOPE("ClDispatchTable::external_mem_acquire");
    return reinterpret_cast<void *>(&acquire_external_objects);
}

void *ClDispatchTable::gl_context_info() const
{
    AV_SCOPE("ClDispatchTable::gl_context_info");
    return reinterpret_cast<void *>(&get_gl_context_info);
}

void *ClDispatchTable::program_from_il() const
{
    AV_SCOPE("ClDispatchTable::program_from_il");
    return reinterpret_cast<void *>(&create_program_with_il);
}

void *ClDispatchTable::external_mem_release() const
{
    AV_SCOPE("ClDispatchTable::external_mem_release");
    return reinterpret_cast<void *>(&release_external_objects);
}

uint32_t ClDispatchTable::claimed() const
{
    AV_SCOPE("ClDispatchTable::claimed");
    return claimed_;
}

bool ClDispatchTable::invariants_hold() const
{
    AV_SCOPE("ClDispatchTable::invariants_hold");
    if (!filled_) return claimed_ == 0u;
    /* Every slot the loader could dispatch a carried command through holds
     * something; the ones this ICD claims hold its own. */
    return claimed_ != 0u && table_.clGetPlatformIDs == &get_platform_ids &&
           table_.clGetPlatformInfo == &get_platform_info && table_.clCreateContext != nullptr &&
           table_.clEnqueueMapBuffer == &map_buffer && table_.clCreateImage != nullptr;
}

}  // namespace av::clicd

namespace av::icd {

void stub_called(const char *api, const char *entry_point, const char *why) noexcept
{
    AV_SCOPE("stub_called");
    static_cast<void>(api);
    static_cast<void>(why);
    if (clicd::cl_icd_binding.bound()) clicd::cl_icd_binding.root().note_refused(entry_point);
}

}  // namespace av::icd
