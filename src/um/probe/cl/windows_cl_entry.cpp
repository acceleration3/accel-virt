#include "um/probe/cl/windows_cl_entry.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

#include <CL/cl.h>

namespace av {

namespace {

/* The module the linker already bound this program to: the loader the runner
 * pushed beside the probe, never a second copy loaded by name. */
constexpr const wchar_t *loader_module = L"OpenCL.dll";

}  // namespace

WindowsClEntry::WindowsClEntry() : loader_(GetModuleHandleW(loader_module))
{
    AV_SCOPE("WindowsClEntry::WindowsClEntry");
}

template <typename Function>
Function WindowsClEntry::named(const char *name) const
{
    AV_SCOPE("WindowsClEntry::named");
    return reinterpret_cast<Function>(GetProcAddress(static_cast<HMODULE>(loader_), name));
}

int32_t WindowsClEntry::platform_count(uint32_t &count)
{
    AV_SCOPE("WindowsClEntry::platform_count");
    return clGetPlatformIDs(0u, nullptr, &count);
}

int32_t WindowsClEntry::platforms(void **into, uint32_t room, uint32_t &written)
{
    AV_SCOPE("WindowsClEntry::platforms");
    return clGetPlatformIDs(room, reinterpret_cast<cl_platform_id *>(into), &written);
}

int32_t WindowsClEntry::platform_text(void *platform, uint32_t name, char *into, size_t room,
                                      size_t &bytes)
{
    AV_SCOPE("WindowsClEntry::platform_text");
    return clGetPlatformInfo(static_cast<cl_platform_id>(platform), name, room, into, &bytes);
}

int32_t WindowsClEntry::devices(void *platform, void **into, uint32_t room, uint32_t &written)
{
    AV_SCOPE("WindowsClEntry::devices");
    return clGetDeviceIDs(static_cast<cl_platform_id>(platform), CL_DEVICE_TYPE_GPU, room,
                          reinterpret_cast<cl_device_id *>(into), &written);
}

int32_t WindowsClEntry::device_text(void *device, uint32_t name, char *into, size_t room,
                                    size_t &bytes)
{
    AV_SCOPE("WindowsClEntry::device_text");
    return clGetDeviceInfo(static_cast<cl_device_id>(device), name, room, into, &bytes);
}

/* The two-call shape, because the names the probe asks for are four bytes
 * (CL_DEVICE_VENDOR_ID) or eight (the SVM capabilities, the LUID) and the
 * caller wants one number either way; anything wider is not a number. */
int32_t WindowsClEntry::device_number(void *device, uint32_t name, uint64_t &value)
{
    AV_SCOPE("WindowsClEntry::device_number");
    size_t        bytes = 0u;
    const int32_t asked =
        clGetDeviceInfo(static_cast<cl_device_id>(device), name, 0u, nullptr, &bytes);
    if (asked != CL_SUCCESS) return asked;
    if (bytes == 0u || bytes > sizeof value) return CL_INVALID_VALUE;
    uint64_t      held = 0u;
    const int32_t read = clGetDeviceInfo(static_cast<cl_device_id>(device), name, bytes, &held, nullptr);
    value = held;
    return read;
}

void *WindowsClEntry::create_context(void *device, int32_t &error)
{
    AV_SCOPE("WindowsClEntry::create_context");
    const cl_device_id one = static_cast<cl_device_id>(device);
    return clCreateContext(nullptr, 1u, &one, nullptr, nullptr, &error);
}

void *WindowsClEntry::create_queue(void *context, void *device, int32_t &error)
{
    AV_SCOPE("WindowsClEntry::create_queue");
    return clCreateCommandQueueWithProperties(static_cast<cl_context>(context),
                                              static_cast<cl_device_id>(device), nullptr, &error);
}

void *WindowsClEntry::create_buffer(void *context, uint64_t flags, size_t bytes, int32_t &error)
{
    AV_SCOPE("WindowsClEntry::create_buffer");
    return clCreateBuffer(static_cast<cl_context>(context), flags, bytes, nullptr, &error);
}

void *WindowsClEntry::create_program(void *context, const char *source, int32_t &error)
{
    AV_SCOPE("WindowsClEntry::create_program");
    return clCreateProgramWithSource(static_cast<cl_context>(context), 1u, &source, nullptr, &error);
}

int32_t WindowsClEntry::build_program(void *program, void *device, const char *options)
{
    AV_SCOPE("WindowsClEntry::build_program");
    const cl_device_id one = static_cast<cl_device_id>(device);
    return clBuildProgram(static_cast<cl_program>(program), 1u, &one, options, nullptr, nullptr);
}

int32_t WindowsClEntry::program_build_text(void *program, void *device, char *into, size_t room,
                                           size_t &bytes)
{
    AV_SCOPE("WindowsClEntry::program_build_text");
    return clGetProgramBuildInfo(static_cast<cl_program>(program), static_cast<cl_device_id>(device),
                                 CL_PROGRAM_BUILD_LOG, room, into, &bytes);
}

void *WindowsClEntry::create_kernel(void *program, const char *name, int32_t &error)
{
    AV_SCOPE("WindowsClEntry::create_kernel");
    return clCreateKernel(static_cast<cl_program>(program), name, &error);
}

int32_t WindowsClEntry::set_kernel_buffer(void *kernel, uint32_t index, void *buffer)
{
    AV_SCOPE("WindowsClEntry::set_kernel_buffer");
    const cl_mem object = static_cast<cl_mem>(buffer);
    return clSetKernelArg(static_cast<cl_kernel>(kernel), index, sizeof object, &object);
}

int32_t WindowsClEntry::write_buffer(void *queue, void *buffer, size_t bytes, const void *from)
{
    AV_SCOPE("WindowsClEntry::write_buffer");
    return clEnqueueWriteBuffer(static_cast<cl_command_queue>(queue), static_cast<cl_mem>(buffer),
                                CL_TRUE, 0u, bytes, from, 0u, nullptr, nullptr);
}

int32_t WindowsClEntry::read_buffer(void *queue, void *buffer, size_t bytes, void *into)
{
    AV_SCOPE("WindowsClEntry::read_buffer");
    return clEnqueueReadBuffer(static_cast<cl_command_queue>(queue), static_cast<cl_mem>(buffer),
                               CL_TRUE, 0u, bytes, into, 0u, nullptr, nullptr);
}

int32_t WindowsClEntry::run_kernel(void *queue, void *kernel, size_t elements)
{
    AV_SCOPE("WindowsClEntry::run_kernel");
    return clEnqueueNDRangeKernel(static_cast<cl_command_queue>(queue), static_cast<cl_kernel>(kernel),
                                  1u, nullptr, &elements, nullptr, 0u, nullptr, nullptr);
}

int32_t WindowsClEntry::finish(void *queue)
{
    AV_SCOPE("WindowsClEntry::finish");
    return clFinish(static_cast<cl_command_queue>(queue));
}

void *WindowsClEntry::map_buffer(void *queue, void *buffer, uint64_t flags, size_t bytes,
                                 int32_t &error)
{
    AV_SCOPE("WindowsClEntry::map_buffer");
    return clEnqueueMapBuffer(static_cast<cl_command_queue>(queue), static_cast<cl_mem>(buffer),
                              CL_TRUE, flags, 0u, bytes, 0u, nullptr, nullptr, &error);
}

int32_t WindowsClEntry::unmap(void *queue, void *buffer, void *pointer)
{
    AV_SCOPE("WindowsClEntry::unmap");
    return clEnqueueUnmapMemObject(static_cast<cl_command_queue>(queue), static_cast<cl_mem>(buffer),
                                   pointer, 0u, nullptr, nullptr);
}

void *WindowsClEntry::svm_alloc(void *context, size_t bytes)
{
    AV_SCOPE("WindowsClEntry::svm_alloc");
    return clSVMAlloc(static_cast<cl_context>(context), CL_MEM_READ_WRITE, bytes, 0u);
}

void WindowsClEntry::svm_free(void *context, void *pointer)
{
    AV_SCOPE("WindowsClEntry::svm_free");
    clSVMFree(static_cast<cl_context>(context), pointer);
}

int32_t WindowsClEntry::set_kernel_svm(void *kernel, uint32_t index, void *pointer)
{
    AV_SCOPE("WindowsClEntry::set_kernel_svm");
    return clSetKernelArgSVMPointer(static_cast<cl_kernel>(kernel), index, pointer);
}

/* clCreateUserEvent and clSetUserEventStatus are not in the probe's import
 * library, so they come from the loaded loader by name; a loader without them
 * is one this step cannot run on rather than a crash. */
void *WindowsClEntry::create_user_event(void *context, int32_t &error)
{
    AV_SCOPE("WindowsClEntry::create_user_event");
    using Create = cl_event(CL_API_CALL *)(cl_context, cl_int *);
    const Create create = named<Create>("clCreateUserEvent");
    if (create == nullptr) {
        error = CL_INVALID_OPERATION;
        return nullptr;
    }
    return create(static_cast<cl_context>(context), &error);
}

int32_t WindowsClEntry::set_user_event_status(void *event, int32_t status)
{
    AV_SCOPE("WindowsClEntry::set_user_event_status");
    using Complete = cl_int(CL_API_CALL *)(cl_event, cl_int);
    const Complete complete = named<Complete>("clSetUserEventStatus");
    if (complete == nullptr) return CL_INVALID_OPERATION;
    return complete(static_cast<cl_event>(event), status);
}

int32_t WindowsClEntry::watch_event(void *event, int32_t status,
                                    void (*notify)(void *event, int32_t status, void *user_data),
                                    void *user_data)
{
    AV_SCOPE("WindowsClEntry::watch_event");
    /* The probe's callback takes the event as a void pointer; on x64 Windows
     * CL_CALLBACK is the one calling convention there is, so only the handle's
     * type differs and the cast is a rename. */
    using Notify = void(CL_CALLBACK *)(cl_event, cl_int, void *);
    return clSetEventCallback(static_cast<cl_event>(event), status,
                              reinterpret_cast<Notify>(notify), user_data);
}

int32_t WindowsClEntry::wait_for_event(void *event)
{
    AV_SCOPE("WindowsClEntry::wait_for_event");
    const cl_event one = static_cast<cl_event>(event);
    return clWaitForEvents(1u, &one);
}

void WindowsClEntry::release_event(void *event)
{
    AV_SCOPE("WindowsClEntry::release_event");
    static_cast<void>(clReleaseEvent(static_cast<cl_event>(event)));
}

void WindowsClEntry::release_kernel(void *kernel)
{
    AV_SCOPE("WindowsClEntry::release_kernel");
    static_cast<void>(clReleaseKernel(static_cast<cl_kernel>(kernel)));
}

void WindowsClEntry::release_program(void *program)
{
    AV_SCOPE("WindowsClEntry::release_program");
    static_cast<void>(clReleaseProgram(static_cast<cl_program>(program)));
}

void WindowsClEntry::release_buffer(void *buffer)
{
    AV_SCOPE("WindowsClEntry::release_buffer");
    static_cast<void>(clReleaseMemObject(static_cast<cl_mem>(buffer)));
}

void WindowsClEntry::release_queue(void *queue)
{
    AV_SCOPE("WindowsClEntry::release_queue");
    static_cast<void>(clReleaseCommandQueue(static_cast<cl_command_queue>(queue)));
}

void WindowsClEntry::release_context(void *context)
{
    AV_SCOPE("WindowsClEntry::release_context");
    static_cast<void>(clReleaseContext(static_cast<cl_context>(context)));
}

}  // namespace av
