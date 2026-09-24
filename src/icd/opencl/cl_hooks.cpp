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
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "proto/generated/opencl/cl_icd_stubs.h"
#include "proto/icd_stub_report.hpp"

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
    return root.platform_ids(num_entries, platforms, num_platforms);
}

cl_int CL_API_CALL get_platform_info(cl_platform_id platform, cl_platform_info name,
                                     size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("get_platform_info");
    AV_CL_ROOT_OR(CL_INVALID_PLATFORM);
    return root.platform_info(platform, name, value_bytes, value, size_ret);
}

cl_int CL_API_CALL get_device_ids(cl_platform_id platform, cl_device_type type, cl_uint num_entries,
                                  cl_device_id *devices, cl_uint *num_devices)
{
    AV_SCOPE("get_device_ids");
    AV_CL_ROOT_OR(CL_INVALID_PLATFORM);
    return root.device_ids(platform, type, num_entries, devices, num_devices);
}

cl_int CL_API_CALL get_device_info(cl_device_id device, cl_device_info name, size_t value_bytes,
                                   void *value, size_t *size_ret)
{
    AV_SCOPE("get_device_info");
    AV_CL_ROOT_OR(CL_INVALID_DEVICE);
    return root.device_info(device, name, value_bytes, value, size_ret);
}

cl_context CL_API_CALL create_context(const cl_context_properties *properties, cl_uint num_devices,
                                      const cl_device_id *devices,
                                      void(CL_CALLBACK *)(const char *, const void *, size_t, void *),
                                      void *, cl_int *error)
{
    AV_SCOPE("create_context");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_PLATFORM;
        return nullptr;
    }
    return cl_icd_binding.root().create_context(
        reinterpret_cast<const uint64_t *>(properties), devices, num_devices, error);
}

cl_int CL_API_CALL retain_context(cl_context context)
{
    AV_SCOPE("retain_context");
    AV_CL_ROOT_OR(CL_INVALID_CONTEXT);
    return root.retain_context(context);
}

cl_int CL_API_CALL release_context(cl_context context)
{
    AV_SCOPE("release_context");
    AV_CL_ROOT_OR(CL_INVALID_CONTEXT);
    return root.release_context(context);
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
    return cl_icd_binding.root().create_queue(context, device,
                                              reinterpret_cast<const uint64_t *>(properties),
                                              error);
}

cl_command_queue CL_API_CALL create_queue(cl_context context, cl_device_id device,
                                          cl_command_queue_properties, cl_int *error)
{
    AV_SCOPE("create_queue");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    /* The deprecated 1.2 call carries a bitfield rather than a list, and the
     * host is what judges its bits. */
    return cl_icd_binding.root().create_queue(context, device, nullptr, error);
}

cl_int CL_API_CALL retain_queue(cl_command_queue queue)
{
    AV_SCOPE("retain_queue");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.retain(queue, ClObject::Kind::command_queue);
}

cl_int CL_API_CALL release_queue(cl_command_queue queue)
{
    AV_SCOPE("release_queue");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.release(queue, ClObject::Kind::command_queue);
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
    return cl_icd_binding.root().create_buffer(context, flags, bytes, host_pointer, nullptr, error);
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
    return cl_icd_binding.root().create_buffer(context, flags, bytes, host_pointer,
                                               reinterpret_cast<const uint64_t *>(properties), error);
}

cl_int CL_API_CALL retain_mem(cl_mem buffer)
{
    AV_SCOPE("retain_mem");
    AV_CL_ROOT_OR(CL_INVALID_MEM_OBJECT);
    return root.retain(buffer, ClObject::Kind::mem);
}

cl_int CL_API_CALL release_mem(cl_mem buffer)
{
    AV_SCOPE("release_mem");
    AV_CL_ROOT_OR(CL_INVALID_MEM_OBJECT);
    return root.release(buffer, ClObject::Kind::mem);
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
    return cl_icd_binding.root().create_program(context, count, sources, lengths, error);
}

cl_int CL_API_CALL build_program(cl_program program, cl_uint, const cl_device_id *,
                                 const char *options,
                                 void(CL_CALLBACK *)(cl_program, void *), void *)
{
    AV_SCOPE("build_program");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.build_program(program, options);
}

cl_int CL_API_CALL retain_program(cl_program program)
{
    AV_SCOPE("retain_program");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.retain(program, ClObject::Kind::program);
}

cl_int CL_API_CALL release_program(cl_program program)
{
    AV_SCOPE("release_program");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.release(program, ClObject::Kind::program);
}

cl_kernel CL_API_CALL create_kernel(cl_program program, const char *name, cl_int *error)
{
    AV_SCOPE("create_kernel");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_PROGRAM;
        return nullptr;
    }
    return cl_icd_binding.root().create_kernel(program, name, error);
}

cl_int CL_API_CALL set_kernel_arg(cl_kernel kernel, cl_uint index, size_t bytes, const void *value)
{
    AV_SCOPE("set_kernel_arg");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.set_kernel_argument(kernel, index, bytes, value);
}

cl_int CL_API_CALL retain_kernel(cl_kernel kernel)
{
    AV_SCOPE("retain_kernel");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.retain(kernel, ClObject::Kind::kernel);
}

cl_int CL_API_CALL release_kernel(cl_kernel kernel)
{
    AV_SCOPE("release_kernel");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.release(kernel, ClObject::Kind::kernel);
}

cl_int CL_API_CALL enqueue_ndrange(cl_command_queue queue, cl_kernel kernel, cl_uint dimensions,
                                   const size_t *offset, const size_t *global, const size_t *local,
                                   cl_uint, const cl_event *, cl_event *event)
{
    AV_SCOPE("enqueue_ndrange");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_kernel(queue, kernel, dimensions, offset, global, local, event);
}

cl_int CL_API_CALL enqueue_read(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                size_t offset, size_t bytes, void *into, cl_uint, const cl_event *,
                                cl_event *event)
{
    AV_SCOPE("enqueue_read");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_read(queue, buffer, blocking, offset, bytes, into, event);
}

cl_int CL_API_CALL enqueue_write(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                 size_t offset, size_t bytes, const void *from, cl_uint,
                                 const cl_event *, cl_event *event)
{
    AV_SCOPE("enqueue_write");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_write(queue, buffer, blocking, offset, bytes, from, event);
}

cl_int CL_API_CALL finish_queue(cl_command_queue queue)
{
    AV_SCOPE("finish_queue");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.finish(queue);
}

cl_int CL_API_CALL flush_queue(cl_command_queue queue)
{
    AV_SCOPE("flush_queue");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.flush(queue);
}

cl_int CL_API_CALL retain_event(cl_event event)
{
    AV_SCOPE("retain_event");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.retain(event, ClObject::Kind::event);
}

void *CL_API_CALL map_buffer(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                             cl_map_flags flags, size_t offset, size_t bytes, cl_uint,
                             const cl_event *, cl_event *event, cl_int *error)
{
    AV_SCOPE("map_buffer");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_COMMAND_QUEUE;
        return nullptr;
    }
    return cl_icd_binding.root().map_buffer(queue, buffer, blocking, flags, offset, bytes, event,
                                            error);
}

cl_int CL_API_CALL unmap_mem_object(cl_command_queue queue, cl_mem buffer, void *pointer, cl_uint,
                                    const cl_event *, cl_event *event)
{
    AV_SCOPE("unmap_mem_object");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.unmap(queue, buffer, pointer, event);
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
                                   void *pointer, size_t bytes, cl_uint, const cl_event *,
                                   cl_event *event)
{
    AV_SCOPE("enqueue_svm_map");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.svm_map(queue, blocking, flags, pointer, bytes, event);
}

cl_int CL_API_CALL enqueue_svm_unmap(cl_command_queue queue, void *pointer, cl_uint,
                                     const cl_event *, cl_event *event)
{
    AV_SCOPE("enqueue_svm_unmap");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.svm_unmap(queue, pointer, event);
}

cl_int CL_API_CALL set_kernel_arg_svm_pointer(cl_kernel kernel, cl_uint index, const void *pointer)
{
    AV_SCOPE("set_kernel_arg_svm_pointer");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.set_kernel_svm_argument(kernel, index, pointer);
}

cl_int CL_API_CALL release_event(cl_event event)
{
    AV_SCOPE("release_event");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.release(event, ClObject::Kind::event);
}

cl_event CL_API_CALL create_user_event(cl_context context, cl_int *error)
{
    AV_SCOPE("create_user_event");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    return cl_icd_binding.root().create_user_event(context, error);
}

cl_int CL_API_CALL set_user_event_status(cl_event event, cl_int status)
{
    AV_SCOPE("set_user_event_status");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.set_user_event_status(event, status);
}

cl_int CL_API_CALL wait_for_events(cl_uint count, const cl_event *events)
{
    AV_SCOPE("wait_for_events");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.wait_for_events(count, events);
}

cl_int CL_API_CALL get_event_info(cl_event event, cl_event_info name, size_t value_bytes,
                                  void *value, size_t *size_ret)
{
    AV_SCOPE("get_event_info");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.event_info(event, name, value_bytes, value, size_ret);
}

cl_int CL_API_CALL set_event_callback(cl_event event, cl_int status,
                                      void(CL_CALLBACK *notify)(cl_event, cl_int, void *),
                                      void *user_data)
{
    AV_SCOPE("set_event_callback");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    if (notify == nullptr) return CL_INVALID_VALUE;
    return root.watch_event(event, status, notify, user_data);
}

cl_int CL_API_CALL get_program_build_info(cl_program program, cl_device_id device,
                                          cl_program_build_info name, size_t value_bytes,
                                          void *value, size_t *size_ret)
{
    AV_SCOPE("get_program_build_info");
    AV_CL_ROOT_OR(CL_INVALID_PROGRAM);
    return root.program_build_info(program, device, name, value_bytes, value, size_ret);
}

cl_int CL_API_CALL get_kernel_work_group_info(cl_kernel kernel, cl_device_id device,
                                              cl_kernel_work_group_info name, size_t value_bytes,
                                              void *value, size_t *size_ret)
{
    AV_SCOPE("get_kernel_work_group_info");
    AV_CL_ROOT_OR(CL_INVALID_KERNEL);
    return root.kernel_work_group_info(kernel, device, name, value_bytes, value, size_ret);
}

cl_int CL_API_CALL enqueue_copy_buffer(cl_command_queue queue, cl_mem from, cl_mem to,
                                       size_t from_offset, size_t to_offset, size_t bytes, cl_uint,
                                       const cl_event *, cl_event *event)
{
    AV_SCOPE("enqueue_copy_buffer");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_copy_buffer(queue, from, to, from_offset, to_offset, bytes, event);
}

cl_int CL_API_CALL enqueue_fill_buffer(cl_command_queue queue, cl_mem buffer, const void *pattern,
                                       size_t pattern_bytes, size_t offset, size_t bytes, cl_uint,
                                       const cl_event *, cl_event *event)
{
    AV_SCOPE("enqueue_fill_buffer");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_fill_buffer(queue, buffer, pattern, pattern_bytes, offset, bytes, event);
}

cl_int CL_API_CALL enqueue_copy_image(cl_command_queue queue, cl_mem from, cl_mem to,
                                      const size_t *from_origin, const size_t *to_origin,
                                      const size_t *region, cl_uint, const cl_event *,
                                      cl_event *event)
{
    AV_SCOPE("enqueue_copy_image");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_copy_image(queue, from, to, from_origin, to_origin, region, event);
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
    return cl_icd_binding.root().create_image(context, flags, format, desc, host_pointer, error);
}

cl_int CL_API_CALL enqueue_read_image(cl_command_queue queue, cl_mem image, cl_bool,
                                      const size_t *origin, const size_t *region, size_t row_pitch,
                                      size_t slice_pitch, void *into, cl_uint, const cl_event *,
                                      cl_event *event)
{
    AV_SCOPE("enqueue_read_image");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_read_image(queue, image, origin, region, row_pitch, slice_pitch, into,
                                   event);
}

cl_int CL_API_CALL enqueue_write_image(cl_command_queue queue, cl_mem image, cl_bool,
                                       const size_t *origin, const size_t *region, size_t row_pitch,
                                       size_t slice_pitch, const void *from, cl_uint,
                                       const cl_event *, cl_event *event)
{
    AV_SCOPE("enqueue_write_image");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_write_image(queue, image, origin, region, row_pitch, slice_pitch, from,
                                    event);
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
    return cl_icd_binding.root().create_sampler(
        context, reinterpret_cast<const uint64_t *>(properties), error);
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
    return cl_icd_binding.root().create_sampler(context, properties, error);
}

cl_int CL_API_CALL retain_sampler(cl_sampler sampler)
{
    AV_SCOPE("retain_sampler");
    AV_CL_ROOT_OR(CL_INVALID_SAMPLER);
    return root.retain(sampler, ClObject::Kind::sampler);
}

cl_int CL_API_CALL release_sampler(cl_sampler sampler)
{
    AV_SCOPE("release_sampler");
    AV_CL_ROOT_OR(CL_INVALID_SAMPLER);
    return root.release(sampler, ClObject::Kind::sampler);
}

/* `cl_khr_external_memory`'s two, which the loader asks for by name: they are
 * not slots of cl_icd_dispatch, because the ICD ABI predates the extension. */
cl_int CL_API_CALL acquire_external_objects(cl_command_queue queue, cl_uint num_mem_objects,
                                           const cl_mem *mem_objects, cl_uint, const cl_event *,
                                           cl_event *event)
{
    AV_SCOPE("acquire_external_objects");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_external(queue, num_mem_objects, mem_objects, event);
}

cl_int CL_API_CALL release_external_objects(cl_command_queue queue, cl_uint num_mem_objects,
                                           const cl_mem *mem_objects, cl_uint, const cl_event *,
                                           cl_event *event)
{
    AV_SCOPE("release_external_objects");
    AV_CL_ROOT_OR(CL_INVALID_COMMAND_QUEUE);
    return root.enqueue_external(queue, num_mem_objects, mem_objects, event);
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
    table_.clSetEventCallback = &set_event_callback;
    table_.clGetProgramBuildInfo = &get_program_build_info;
    table_.clGetKernelWorkGroupInfo = &get_kernel_work_group_info;
    table_.clEnqueueCopyBuffer = &enqueue_copy_buffer;
    table_.clEnqueueFillBuffer = &enqueue_fill_buffer;
    table_.clEnqueueCopyImage = &enqueue_copy_image;
    table_.clCreateImage = &create_image;
    table_.clEnqueueReadImage = &enqueue_read_image;
    table_.clEnqueueWriteImage = &enqueue_write_image;
    table_.clCreateSamplerWithProperties = &create_sampler_with_properties;
    table_.clCreateSampler = &create_sampler;
    table_.clRetainSampler = &retain_sampler;
    table_.clReleaseSampler = &release_sampler;
    claimed_ = 54u;
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
    static_cast<void>(entry_point);
    static_cast<void>(why);
}

void payload_dropped(const char *api, const char *command, size_t written) noexcept
{
    AV_SCOPE("payload_dropped");
    static_cast<void>(api);
    static_cast<void>(command);
    static_cast<void>(written);
}

}  // namespace av::icd
