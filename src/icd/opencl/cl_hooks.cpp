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

cl_context CL_API_CALL create_context(const cl_context_properties *, cl_uint num_devices,
                                      const cl_device_id *devices,
                                      void(CL_CALLBACK *)(const char *, const void *, size_t, void *),
                                      void *, cl_int *error)
{
    AV_SCOPE("create_context");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_PLATFORM;
        return nullptr;
    }
    return cl_icd_binding.root().create_context(devices, num_devices, error);
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
                                                          const cl_queue_properties *, cl_int *error)
{
    AV_SCOPE("create_queue_with_properties");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    return cl_icd_binding.root().create_queue(context, device, error);
}

cl_command_queue CL_API_CALL create_queue(cl_context context, cl_device_id device,
                                          cl_command_queue_properties, cl_int *error)
{
    AV_SCOPE("create_queue");
    if (!cl_icd_binding.bound()) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    return cl_icd_binding.root().create_queue(context, device, error);
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
    return cl_icd_binding.root().create_buffer(context, flags, bytes, host_pointer, error);
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

cl_int CL_API_CALL release_event(cl_event event)
{
    AV_SCOPE("release_event");
    AV_CL_ROOT_OR(CL_INVALID_EVENT);
    return root.release(event, ClObject::Kind::event);
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
    claimed_ = 29u;
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
           table_.clEnqueueMapBuffer != nullptr;
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
