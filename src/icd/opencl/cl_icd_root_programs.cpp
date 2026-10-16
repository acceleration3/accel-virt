/* ClIcdRoot's programs and kernels beyond the one-source build: binaries, IL,
 * built-in kernels, separate compilation and linking, and what a program and
 * a kernel say about themselves. A context here has one device, so every
 * device list judged here names that device or none. */
#include "icd/opencl/cl_icd_root.hpp"

#include "core/log/log_sink.hpp"
#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/opencl/cl_info_request.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

namespace {

void report_program_error(cl_int *error, cl_int value)
{
    AV_SCOPE("report_program_error");
    if (error != nullptr) *error = value;
}

}  // namespace

cl_int ClIcdRoot::program_devices(ClObject &context, cl_uint num_devices, const cl_device_id *device_list)
{
    AV_SCOPE("ClIcdRoot::program_devices");
    if ((device_list == nullptr) != (num_devices == 0u)) return CL_INVALID_VALUE;
    GuestClContext *const owner = context.context();
    ClDevice *const       running = owner != nullptr ? devices_.running(*owner) : nullptr;
    for (cl_uint index = 0u; index < num_devices; ++index) {
        ClObject *const named = judged(device_list[index], ClObject::Kind::device);
        if (named == nullptr || running == nullptr || devices_.of(*named) != running) return CL_INVALID_DEVICE;
    }
    return CL_SUCCESS;
}

cl_int ClIcdRoot::program_list(ClObject &context, cl_uint count, const cl_program *programs, ClHostNumbers &room,
                               uint64_t *&into)
{
    AV_SCOPE("ClIcdRoot::program_list");
    if ((programs == nullptr) != (count == 0u)) return CL_INVALID_VALUE;
    into = room.room(count);
    if (into == nullptr) return CL_OUT_OF_HOST_MEMORY;
    for (cl_uint index = 0u; index < count; ++index) {
        ClObject *const named = judged(programs[index], ClObject::Kind::program);
        if (named == nullptr || named->context() != context.context()) return CL_INVALID_PROGRAM;
        into[index] = named->wire();
    }
    return CL_SUCCESS;
}

cl_int ClIcdRoot::build_program(cl_program program, cl_uint num_devices, const cl_device_id *device_list,
                                const char *options, const ClProgramNotify &notify)
{
    AV_SCOPE("ClIcdRoot::build_program");
    ClObject *const held = judged(program, ClObject::Kind::program);
    if (held == nullptr) return CL_INVALID_PROGRAM;
    if (!notify.valid()) return CL_INVALID_VALUE;
    const cl_int devices = program_devices(*held, num_devices, device_list);
    if (devices != CL_SUCCESS) return devices;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    /* Built for the context's one device, which a list of none names too. */
    const cl_int built = av::clproto::av_clBuildProgram(*wire, held->wire(), 0u, nullptr, options);
    if (built == CL_BUILD_PROGRAM_FAILURE) note_build_log(*wire, held->wire(), host_device_of(*held), options);
    if (built == CL_SUCCESS || built == CL_BUILD_PROGRAM_FAILURE) notify.tell(program);
    return built;
}

/* The host compiler's own words for a program that did not build: an
 * application that reports only "a problem" leaves them nowhere else. Its
 * first lines, since a large program's log runs long - however long, the
 * whole log crosses, since an answer longer than the reply slot comes in
 * parts. */
void ClIcdRoot::note_build_log(ClientTransport &wire, uint64_t program, uint64_t device, const char *options)
{
    AV_SCOPE("ClIcdRoot::note_build_log");
    constexpr uint32_t most_lines = 24u;
    char       bytes[256];
    TextBuffer line(bytes, sizeof bytes);
    line.append("avcl: a program did not build; its options: ");
    line.append(options != nullptr ? options : "");
    log_.write(line.text());
    size_t size = 0u;
    if (device == 0u ||
        av::clproto::av_clGetProgramBuildInfo(wire, program, device, CL_PROGRAM_BUILD_LOG, 0u,
                                               nullptr, &size) != CL_SUCCESS ||
        size == 0u)
        return;
    char *const text = static_cast<char *>(memory_.allocate(size + 1u, Pool::paged));
    if (text == nullptr) return;
    ScopeExit given_back([&] { memory_.release(text); });
    if (av::clproto::av_clGetProgramBuildInfo(wire, program, device, CL_PROGRAM_BUILD_LOG, size,
                                               text, nullptr) != CL_SUCCESS)
        return;
    text[size] = '\0';
    uint32_t written = 0u;
    for (char *start = text; *start != '\0' && written < most_lines;) {
        char *end = start;
        while (*end != '\0' && *end != '\n') end++;
        const bool last = *end == '\0';
        *end = '\0';
        if (end != start) {
            TextBuffer said(bytes, sizeof bytes);
            said.append("avcl: build: ");
            said.append(start);
            log_.write(said.text());
            written++;
        }
        if (last) break;
        start = end + 1;
    }
}

cl_program ClIcdRoot::create_program_with_binary(cl_context context, cl_uint num_devices,
                                                 const cl_device_id *device_list, const size_t *lengths,
                                                 const unsigned char **binaries, cl_int *binary_status,
                                                 cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_program_with_binary");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) {
        report_program_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
    if (num_devices == 0u || device_list == nullptr || lengths == nullptr || binaries == nullptr) {
        report_program_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    cl_int judged_binaries = CL_SUCCESS;
    for (cl_uint index = 0u; index < num_devices; ++index) {
        const bool empty = lengths[index] == 0u || binaries[index] == nullptr;
        if (binary_status != nullptr) binary_status[index] = empty ? CL_INVALID_VALUE : CL_SUCCESS;
        if (empty) judged_binaries = CL_INVALID_VALUE;
    }
    /* The context's one device, once: a list naming it twice names a device
     * the context has only one binary for. */
    cl_int judged_devices = program_devices(*held, num_devices, device_list);
    if (judged_devices == CL_SUCCESS && num_devices != 1u) judged_devices = CL_INVALID_DEVICE;
    if (judged_binaries != CL_SUCCESS || judged_devices != CL_SUCCESS) {
        report_program_error(error, judged_binaries != CL_SUCCESS ? judged_binaries : judged_devices);
        return nullptr;
    }
    ClientTransport *const wire = wire_of(*held);
    uint64_t               host_device = host_device_of(*held);
    if (wire == nullptr || host_device == 0u) {
        report_program_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_int         status = CL_SUCCESS;
    cl_int         host_error = CL_SUCCESS;
    const uint64_t program = av::clproto::av_clCreateProgramWithBinary(*wire, held->wire(), 1u, &host_device, lengths,
                                                                       binaries, &status, &host_error);
    if (binary_status != nullptr) binary_status[0] = status;
    if (program == 0u) {
        report_program_error(error, host_error != CL_SUCCESS ? host_error : CL_INVALID_BINARY);
        return nullptr;
    }
    return static_cast<cl_program>(wrap(*wire, ClObject::Kind::program, program, held->context(), held, error));
}

cl_program ClIcdRoot::create_program_with_il(cl_context context, const void *il, size_t length, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_program_with_il");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) {
        report_program_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
    if (il == nullptr || length == 0u) {
        report_program_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_program_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t program = av::clproto::av_clCreateProgramWithIL(*wire, held->wire(), il, length, &host_error);
    if (program == 0u) {
        report_program_error(error, host_error != CL_SUCCESS ? host_error : CL_INVALID_VALUE);
        return nullptr;
    }
    return static_cast<cl_program>(wrap(*wire, ClObject::Kind::program, program, held->context(), held, error));
}

cl_program ClIcdRoot::create_program_with_built_in_kernels(cl_context context, cl_uint num_devices,
                                                           const cl_device_id *device_list, const char *kernel_names,
                                                           cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_program_with_built_in_kernels");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) {
        report_program_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
    if (num_devices == 0u || device_list == nullptr || kernel_names == nullptr) {
        report_program_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    const cl_int devices = program_devices(*held, num_devices, device_list);
    if (devices != CL_SUCCESS) {
        report_program_error(error, devices);
        return nullptr;
    }
    ClientTransport *const wire = wire_of(*held);
    uint64_t               host_device = host_device_of(*held);
    if (wire == nullptr || host_device == 0u) {
        report_program_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t program = av::clproto::av_clCreateProgramWithBuiltInKernels(*wire, held->wire(), 1u, &host_device,
                                                                               kernel_names, &host_error);
    if (program == 0u) {
        report_program_error(error, host_error != CL_SUCCESS ? host_error : CL_INVALID_VALUE);
        return nullptr;
    }
    return static_cast<cl_program>(wrap(*wire, ClObject::Kind::program, program, held->context(), held, error));
}

cl_int ClIcdRoot::compile_program(cl_program program, cl_uint num_devices, const cl_device_id *device_list,
                                  const char *options, cl_uint num_headers, const cl_program *headers,
                                  const char **header_names, const ClProgramNotify &notify)
{
    AV_SCOPE("ClIcdRoot::compile_program");
    ClObject *const held = judged(program, ClObject::Kind::program);
    if (held == nullptr) return CL_INVALID_PROGRAM;
    if (!notify.valid() || (num_headers != 0u && header_names == nullptr)) return CL_INVALID_VALUE;
    const cl_int devices = program_devices(*held, num_devices, device_list);
    if (devices != CL_SUCCESS) return devices;
    ClHostNumbers host_headers_room(memory_);
    uint64_t     *host_headers = nullptr;
    const cl_int listed = program_list(*held, num_headers, headers, host_headers_room, host_headers);
    if (listed != CL_SUCCESS) return listed;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_int compiled = av::clproto::av_clCompileProgram(*wire, held->wire(), 0u, nullptr, options, num_headers,
                                                             num_headers != 0u ? host_headers : nullptr, header_names);
    if (compiled == CL_COMPILE_PROGRAM_FAILURE) note_build_log(*wire, held->wire(), host_device_of(*held), options);
    if (compiled == CL_SUCCESS || compiled == CL_COMPILE_PROGRAM_FAILURE) notify.tell(program);
    return compiled;
}

cl_program ClIcdRoot::link_program(cl_context context, cl_uint num_devices, const cl_device_id *device_list,
                                   const char *options, cl_uint num_inputs, const cl_program *inputs,
                                   const ClProgramNotify &notify, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::link_program");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) {
        report_program_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
    cl_int judgement = !notify.valid() || num_inputs == 0u ? CL_INVALID_VALUE : CL_SUCCESS;
    if (judgement == CL_SUCCESS) judgement = program_devices(*held, num_devices, device_list);
    ClHostNumbers host_inputs_room(memory_);
    uint64_t     *host_inputs = nullptr;
    if (judgement == CL_SUCCESS) judgement = program_list(*held, num_inputs, inputs, host_inputs_room, host_inputs);
    ClientTransport *const wire = wire_of(*held);
    if (judgement == CL_SUCCESS && wire == nullptr) judgement = CL_OUT_OF_RESOURCES;
    if (judgement != CL_SUCCESS) {
        report_program_error(error, judgement);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t linked = av::clproto::av_clLinkProgram(*wire, held->wire(), 0u, nullptr, options, num_inputs,
                                                          host_inputs, &host_error);
    if (linked == 0u) {
        report_program_error(error, host_error != CL_SUCCESS ? host_error : CL_LINK_PROGRAM_FAILURE);
        return nullptr;
    }
    if (host_error == CL_LINK_PROGRAM_FAILURE) note_build_log(*wire, linked, host_device_of(*held), options);
    cl_int     kept = CL_SUCCESS;
    cl_program made = static_cast<cl_program>(wrap(*wire, ClObject::Kind::program, linked, held->context(), held, &kept));
    report_program_error(error, kept != CL_SUCCESS ? kept : host_error);
    notify.tell(made);
    return made;
}

cl_int ClIcdRoot::program_info(cl_program program, cl_program_info name, size_t value_bytes, void *value,
                               size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::program_info");
    ClObject *const held = judged(program, ClObject::Kind::program);
    if (held == nullptr) return CL_INVALID_PROGRAM;
    const ClInfoRequest asked(value_bytes, value, size_ret);
    switch (name) {
    case CL_PROGRAM_REFERENCE_COUNT: {
        lock_.acquire();
        const cl_uint count = held->references();
        lock_.release();
        return asked.give(&count, sizeof count);
    }
    case CL_PROGRAM_CONTEXT: {
        const cl_context owner = reinterpret_cast<cl_context>(held->nearest(ClObject::Kind::context));
        return asked.give(&owner, sizeof owner);
    }
    case CL_PROGRAM_NUM_DEVICES: {
        const cl_uint one = 1u;
        return asked.give(&one, sizeof one);
    }
    case CL_PROGRAM_DEVICES: {
        ClDevice *const    on = held->context() != nullptr ? devices_.running(*held->context()) : nullptr;
        const cl_device_id device = on != nullptr ? reinterpret_cast<cl_device_id>(on->object()) : nullptr;
        return asked.give(&device, sizeof device);
    }
    default:
        break;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    if (name == CL_PROGRAM_BINARIES) return program_binaries(*wire, *held, value_bytes, value, size_ret);
    return av::clproto::av_clGetProgramInfo(*wire, held->wire(), name, value_bytes, value, size_ret);
}

/* The application's value is an array of pointers, one per device - one here
 * - and on the wire CL_PROGRAM_BINARIES is the bytes themselves, which the
 * host answers into the application's own memory: the size it was told by
 * CL_PROGRAM_BINARY_SIZES is the room it gave. */
cl_int ClIcdRoot::program_binaries(ClientTransport &wire, ClObject &program, size_t value_bytes, void *value,
                                   size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::program_binaries");
    unsigned char *into = nullptr;
    if (size_ret != nullptr) *size_ret = sizeof into;
    if (value == nullptr) return CL_SUCCESS;
    if (value_bytes < sizeof into) return CL_INVALID_VALUE;
    __builtin_memcpy(&into, value, sizeof into);
    /* A null entry is one the application does not want, which the API
     * skips. */
    if (into == nullptr) return CL_SUCCESS;
    size_t       size = 0u;
    size_t       answered = 0u;
    const cl_int sized =
        av::clproto::av_clGetProgramInfo(wire, program.wire(), CL_PROGRAM_BINARY_SIZES, sizeof size, &size, &answered);
    if (sized != CL_SUCCESS) return sized;
    if (answered != sizeof size) return CL_OUT_OF_RESOURCES;
    if (size == 0u) return CL_SUCCESS;
    return av::clproto::av_clGetProgramInfo(wire, program.wire(), CL_PROGRAM_BINARIES, size, into, nullptr);
}

cl_int ClIcdRoot::set_specialization_constant(cl_program program, cl_uint id, size_t bytes, const void *value)
{
    AV_SCOPE("ClIcdRoot::set_specialization_constant");
    ClObject *const held = judged(program, ClObject::Kind::program);
    if (held == nullptr) return CL_INVALID_PROGRAM;
    if (value == nullptr || bytes == 0u) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clSetProgramSpecializationConstant(*wire, held->wire(), id, bytes, value);
}

cl_int ClIcdRoot::set_program_release_callback(cl_program program, void(CL_CALLBACK *notify)(cl_program, void *))
{
    AV_SCOPE("ClIcdRoot::set_program_release_callback");
    if (judged(program, ClObject::Kind::program) == nullptr) return CL_INVALID_PROGRAM;
    return notify == nullptr ? CL_INVALID_VALUE : CL_INVALID_OPERATION;
}

cl_int ClIcdRoot::unload_platform_compiler(cl_platform_id platform)
{
    AV_SCOPE("ClIcdRoot::unload_platform_compiler");
    return platform_.names_this(platform) ? CL_SUCCESS : CL_INVALID_PLATFORM;
}

cl_int ClIcdRoot::create_kernels_in_program(cl_program program, cl_uint num_kernels, cl_kernel *kernels,
                                            cl_uint *num_kernels_ret)
{
    AV_SCOPE("ClIcdRoot::create_kernels_in_program");
    ClObject *const held = judged(program, ClObject::Kind::program);
    if (held == nullptr) return CL_INVALID_PROGRAM;
    if (kernels != nullptr && num_kernels == 0u) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_uint   room = kernels != nullptr ? num_kernels : 0u;
    uint64_t *const handles =
        room != 0u ? static_cast<uint64_t *>(memory_.allocate(room * sizeof(uint64_t), Pool::paged)) : nullptr;
    if (room != 0u && handles == nullptr) return CL_OUT_OF_HOST_MEMORY;
    ScopeExit    given_back([&] { if (handles != nullptr) memory_.release(handles); });
    cl_uint      made = 0u;
    const cl_int created = av::clproto::av_clCreateKernelsInProgram(*wire, held->wire(), room, handles, &made);
    if (created != CL_SUCCESS) return created;
    if (num_kernels_ret != nullptr) *num_kernels_ret = made;
    if (kernels == nullptr) return CL_SUCCESS;
    for (cl_uint index = 0u; index < made && index < room; ++index) {
        cl_int kept = CL_SUCCESS;
        kernels[index] = static_cast<cl_kernel>(wrap(*wire, ClObject::Kind::kernel, handles[index], held->context(),
                                                     held, &kept));
        if (kept == CL_SUCCESS) continue;
        /* All or none: the kernels already handed over go back, and the
         * host's for the rest with them. */
        for (cl_uint made_before = 0u; made_before < index; ++made_before)
            static_cast<void>(release(kernels[made_before], ClObject::Kind::kernel));
        for (cl_uint rest = index + 1u; rest < made && rest < room; ++rest)
            give_back_to_host(*wire, ClObject::Kind::kernel, handles[rest]);
        return kept;
    }
    return CL_SUCCESS;
}

cl_kernel ClIcdRoot::clone_kernel(cl_kernel kernel, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::clone_kernel");
    ClObject *const held = judged(kernel, ClObject::Kind::kernel);
    if (held == nullptr) {
        report_program_error(error, CL_INVALID_KERNEL);
        return nullptr;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_program_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t clone = av::clproto::av_clCloneKernel(*wire, held->wire(), &host_error);
    if (clone == 0u) {
        report_program_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    return static_cast<cl_kernel>(wrap(*wire, ClObject::Kind::kernel, clone, held->context(), held->parent(), error));
}

cl_int ClIcdRoot::kernel_info(cl_kernel kernel, cl_kernel_info name, size_t value_bytes, void *value,
                              size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::kernel_info");
    ClObject *const held = judged(kernel, ClObject::Kind::kernel);
    if (held == nullptr) return CL_INVALID_KERNEL;
    const ClInfoRequest asked(value_bytes, value, size_ret);
    switch (name) {
    case CL_KERNEL_REFERENCE_COUNT: {
        lock_.acquire();
        const cl_uint count = held->references();
        lock_.release();
        return asked.give(&count, sizeof count);
    }
    case CL_KERNEL_CONTEXT: {
        const cl_context owner = reinterpret_cast<cl_context>(held->nearest(ClObject::Kind::context));
        return asked.give(&owner, sizeof owner);
    }
    case CL_KERNEL_PROGRAM: {
        const cl_program owner = reinterpret_cast<cl_program>(held->nearest(ClObject::Kind::program));
        return asked.give(&owner, sizeof owner);
    }
    default:
        break;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetKernelInfo(*wire, held->wire(), name, value_bytes, value, size_ret);
}

cl_int ClIcdRoot::kernel_arg_info(cl_kernel kernel, cl_uint index, cl_kernel_arg_info name, size_t value_bytes,
                                  void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::kernel_arg_info");
    ClObject *const held = judged(kernel, ClObject::Kind::kernel);
    if (held == nullptr) return CL_INVALID_KERNEL;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetKernelArgInfo(*wire, held->wire(), index, name, value_bytes, value, size_ret);
}

cl_int ClIcdRoot::kernel_sub_group_info(cl_kernel kernel, cl_device_id device, cl_kernel_sub_group_info name,
                                        size_t input_bytes, const void *input, size_t value_bytes, void *value,
                                        size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::kernel_sub_group_info");
    ClObject *const held = judged(kernel, ClObject::Kind::kernel);
    if (held == nullptr) return CL_INVALID_KERNEL;
    /* A null device is the kernel's only one, which the API allows. */
    if (device != nullptr && program_devices(*held, 1u, &device) != CL_SUCCESS) return CL_INVALID_DEVICE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetKernelSubGroupInfo(*wire, held->wire(), host_device_of(*held), name, input_bytes,
                                                    input, value_bytes, value, size_ret);
}

cl_int ClIcdRoot::set_kernel_exec_info(cl_kernel kernel, cl_kernel_exec_info name, size_t bytes, const void *value)
{
    AV_SCOPE("ClIcdRoot::set_kernel_exec_info");
    ClObject *const held = judged(kernel, ClObject::Kind::kernel);
    if (held == nullptr) return CL_INVALID_KERNEL;
    GuestClContext *const context = held->context();
    ClientTransport *const wire = wire_of(*held);
    if (context == nullptr || wire == nullptr) return CL_OUT_OF_RESOURCES;
    if (name == CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM) {
        cl_bool wanted = CL_FALSE;
        if (value == nullptr || bytes != sizeof wanted) return CL_INVALID_VALUE;
        __builtin_memcpy(&wanted, value, sizeof wanted);
        if (wanted != CL_FALSE) return CL_INVALID_OPERATION;
        return av::clproto::av_clSetKernelExecInfo(*wire, held->wire(), name, sizeof wanted, &wanted);
    }
    if (name != CL_KERNEL_EXEC_INFO_SVM_PTRS) return CL_INVALID_VALUE;
    if (value == nullptr || bytes == 0u || bytes % sizeof(void *) != 0u) return CL_INVALID_VALUE;
    const size_t count = bytes / sizeof(void *);
    /* No list of pointers a process could hold is longer. */
    if (count > 0x3FFFFFFFu) return CL_INVALID_VALUE;
    /* Each the host address of the allocation it is in, as a pointer
     * argument crosses; one inside none is the application's mistake. */
    ClHostNumbers   hosts_room(memory_);
    uint64_t *const hosts = hosts_room.room(static_cast<uint32_t>(count));
    if (hosts == nullptr) return CL_OUT_OF_HOST_MEMORY;
    for (size_t index = 0u; index < count; ++index) {
        const void *pointer = nullptr;
        __builtin_memcpy(&pointer, static_cast<const uint8_t *>(value) + index * sizeof pointer, sizeof pointer);
        hosts[index] = context->svm().host_address_of(pointer);
        if (hosts[index] == 0u) return CL_INVALID_VALUE;
    }
    return av::clproto::av_clSetKernelExecInfo(*wire, held->wire(), name, count * sizeof(uint64_t), hosts);
}

}  // namespace av::clicd
