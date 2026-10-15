#include "harness/icd/fake_cl_loader.hpp"

#include "icd/opencl/cl_icd_root.hpp"

#include <CL/cl_ext.h>

namespace av {

namespace {

/* The loader holds a vendor's platforms by the dispatch table in their first
 * word; this is that read, spelled once. */
const cl_icd_dispatch *first_word(const void *object)
{
    return object != nullptr ? *reinterpret_cast<const cl_icd_dispatch *const *>(object) : nullptr;
}

using PlatformIds = cl_int(CL_API_CALL *)(cl_uint, cl_platform_id *, cl_uint *);

}  // namespace

FakeClLoader::FakeClLoader(clicd::ClIcdRoot &root)
    : root_(&root), hidden_(false), state_(Load::kept), platforms_{}, count_(0u), suffix_{},
      extensions_{}
{
}

void FakeClLoader::hide_export() { hidden_ = true; }

FakeClLoader::Load FakeClLoader::state() const { return state_; }
std::uint32_t      FakeClLoader::platforms() const { return count_; }
cl_platform_id     FakeClLoader::platform(std::uint32_t index) const
{
    return index < count_ ? platforms_[index] : nullptr;
}
const char *FakeClLoader::suffix() const { return suffix_; }
const char *FakeClLoader::extensions() const { return extensions_; }

const cl_icd_dispatch *FakeClLoader::dispatch_of(const void *object) const
{
    return first_word(object);
}

FakeClLoader::Load FakeClLoader::load()
{
    count_ = 0u;
    /* 1. The one symbol a vendor library is resolved by. */
    if (hidden_) {
        state_ = Load::no_extension_function;
        return state_;
    }
    /* 2. The enumeration, asked for through it by name. */
    void *const asked = root_->extension_function("clIcdGetPlatformIDsKHR");
    if (asked == nullptr) {
        state_ = Load::no_enumeration;
        return state_;
    }
    const PlatformIds enumerate = reinterpret_cast<PlatformIds>(asked);

    /* 3 and 4. The count, then the platforms. */
    cl_uint available = 0u;
    if (enumerate(0u, nullptr, &available) != CL_SUCCESS) {
        state_ = Load::enumeration_failed;
        return state_;
    }
    if (available == 0u) {
        state_ = Load::no_platform;
        return state_;
    }
    if (available > max_platforms) available = max_platforms;
    if (enumerate(available, platforms_, nullptr) != CL_SUCCESS) {
        state_ = Load::enumeration_failed;
        return state_;
    }

    /* 5. A platform with no dispatch table is skipped before it is asked
     * anything at all. */
    if (first_word(platforms_[0]) == nullptr) {
        state_ = Load::not_dispatchable;
        return state_;
    }
    const cl_icd_dispatch *const table = first_word(platforms_[0]);

    /* 6. The extensions, size then bytes. */
    std::size_t bytes = 0u;
    if (table->clGetPlatformInfo(platforms_[0], CL_PLATFORM_EXTENSIONS, 0u, nullptr, &bytes) !=
            CL_SUCCESS ||
        bytes > sizeof extensions_ ||
        table->clGetPlatformInfo(platforms_[0], CL_PLATFORM_EXTENSIONS, bytes, extensions_,
                                 nullptr) != CL_SUCCESS) {
        state_ = Load::no_extensions;
        return state_;
    }

    /* 7. The suffix, which a platform cannot decline: one that does is
     * dropped, and the driver with it. */
    if (table->clGetPlatformInfo(platforms_[0], CL_PLATFORM_ICD_SUFFIX_KHR, 0u, nullptr, &bytes) !=
            CL_SUCCESS ||
        bytes > sizeof suffix_ ||
        table->clGetPlatformInfo(platforms_[0], CL_PLATFORM_ICD_SUFFIX_KHR, bytes, suffix_,
                                 nullptr) != CL_SUCCESS) {
        state_ = Load::no_suffix;
        return state_;
    }
    count_ = available;
    state_ = Load::kept;
    return state_;
}

cl_int FakeClLoader::device_ids(cl_device_type type, cl_uint num_entries, cl_device_id *devices,
                                cl_uint *num_devices)
{
    if (count_ == 0u) return CL_INVALID_PLATFORM;
    return first_word(platforms_[0])
        ->clGetDeviceIDs(platforms_[0], type, num_entries, devices, num_devices);
}

cl_int FakeClLoader::device_info(cl_device_id device, cl_device_info name, std::size_t bytes,
                                 void *value, std::size_t *size_ret)
{
    const cl_icd_dispatch *const table = first_word(device);
    if (table == nullptr) return CL_INVALID_DEVICE;
    return table->clGetDeviceInfo(device, name, bytes, value, size_ret);
}

cl_context FakeClLoader::create_context(cl_device_id device, cl_int *error)
{
    const cl_icd_dispatch *const table = first_word(device);
    if (table == nullptr) {
        if (error != nullptr) *error = CL_INVALID_DEVICE;
        return nullptr;
    }
    return table->clCreateContext(nullptr, 1u, &device, nullptr, nullptr, error);
}

cl_context FakeClLoader::create_context(cl_device_id device,
                                        void(CL_CALLBACK *notify)(const char *, const void *, size_t, void *),
                                        void *user_data, cl_int *error)
{
    const cl_icd_dispatch *const table = first_word(device);
    if (table == nullptr) {
        if (error != nullptr) *error = CL_INVALID_DEVICE;
        return nullptr;
    }
    return table->clCreateContext(nullptr, 1u, &device, notify, user_data, error);
}

cl_int FakeClLoader::release_context(cl_context context)
{
    const cl_icd_dispatch *const table = first_word(context);
    return table != nullptr ? table->clReleaseContext(context) : CL_INVALID_CONTEXT;
}

cl_command_queue FakeClLoader::create_queue(cl_context context, cl_device_id device, cl_int *error)
{
    const cl_icd_dispatch *const table = first_word(context);
    if (table == nullptr) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    return table->clCreateCommandQueueWithProperties(context, device, nullptr, error);
}

cl_mem FakeClLoader::create_buffer(cl_context context, cl_mem_flags flags, std::size_t bytes,
                                   cl_int *error)
{
    const cl_icd_dispatch *const table = first_word(context);
    if (table == nullptr) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    return table->clCreateBuffer(context, flags, bytes, nullptr, error);
}

cl_mem FakeClLoader::create_buffer_with_properties(cl_context context,
                                                   const cl_mem_properties *properties,
                                                   cl_mem_flags flags, std::size_t bytes, cl_int *error)
{
    const cl_icd_dispatch *const table = first_word(context);
    if (table == nullptr) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    return table->clCreateBufferWithProperties(context, properties, flags, bytes, nullptr, error);
}

cl_program FakeClLoader::create_program(cl_context context, const char *source, cl_int *error)
{
    const cl_icd_dispatch *const table = first_word(context);
    if (table == nullptr) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    return table->clCreateProgramWithSource(context, 1u, &source, nullptr, error);
}

cl_int FakeClLoader::build_program(cl_program program, const char *options)
{
    const cl_icd_dispatch *const table = first_word(program);
    return table != nullptr ? table->clBuildProgram(program, 0u, nullptr, options, nullptr, nullptr)
                            : CL_INVALID_PROGRAM;
}

cl_kernel FakeClLoader::create_kernel(cl_program program, const char *name, cl_int *error)
{
    const cl_icd_dispatch *const table = first_word(program);
    if (table == nullptr) {
        if (error != nullptr) *error = CL_INVALID_PROGRAM;
        return nullptr;
    }
    return table->clCreateKernel(program, name, error);
}


cl_event FakeClLoader::create_user_event(cl_context context, cl_int *error)
{
    const cl_icd_dispatch *const table = first_word(context);
    if (table == nullptr) {
        if (error != nullptr) *error = CL_INVALID_CONTEXT;
        return nullptr;
    }
    return table->clCreateUserEvent(context, error);
}

cl_int FakeClLoader::set_user_event_status(cl_event event, cl_int status)
{
    const cl_icd_dispatch *const table = first_word(event);
    return table != nullptr ? table->clSetUserEventStatus(event, status) : CL_INVALID_EVENT;
}

cl_int FakeClLoader::wait_for_events(cl_uint count, const cl_event *events)
{
    /* The real loader dispatches a wait through the first event's table, so a
     * wait of none has nothing to dispatch through. */
    if (count == 0u || events == nullptr) return CL_INVALID_VALUE;
    const cl_icd_dispatch *const table = first_word(events[0]);
    return table != nullptr ? table->clWaitForEvents(count, events) : CL_INVALID_EVENT;
}

cl_int FakeClLoader::event_info(cl_event event, cl_event_info name, std::size_t bytes, void *value,
                                std::size_t *size_ret)
{
    const cl_icd_dispatch *const table = first_word(event);
    return table != nullptr ? table->clGetEventInfo(event, name, bytes, value, size_ret)
                            : CL_INVALID_EVENT;
}

cl_int FakeClLoader::set_event_callback(cl_event event, cl_int status,
                                        void(CL_CALLBACK *notify)(cl_event, cl_int, void *),
                                        void *user_data)
{
    const cl_icd_dispatch *const table = first_word(event);
    return table != nullptr ? table->clSetEventCallback(event, status, notify, user_data)
                            : CL_INVALID_EVENT;
}

cl_int FakeClLoader::release_event(cl_event event)
{
    const cl_icd_dispatch *const table = first_word(event);
    return table != nullptr ? table->clReleaseEvent(event) : CL_INVALID_EVENT;
}

cl_int FakeClLoader::finish(cl_command_queue queue)
{
    const cl_icd_dispatch *const table = first_word(queue);
    return table != nullptr ? table->clFinish(queue) : CL_INVALID_COMMAND_QUEUE;
}

cl_int FakeClLoader::release_queue(cl_command_queue queue)
{
    const cl_icd_dispatch *const table = first_word(queue);
    return table != nullptr ? table->clReleaseCommandQueue(queue) : CL_INVALID_COMMAND_QUEUE;
}

cl_int FakeClLoader::release_mem(cl_mem buffer)
{
    const cl_icd_dispatch *const table = first_word(buffer);
    return table != nullptr ? table->clReleaseMemObject(buffer) : CL_INVALID_MEM_OBJECT;
}

}  // namespace av
