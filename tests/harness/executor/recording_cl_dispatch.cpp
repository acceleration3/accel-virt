#include "harness/executor/recording_cl_dispatch.hpp"

#include "proto/wire/wire_arena.hpp"

#include <cstring>

namespace av {

namespace {

constexpr const char kernel_arg_info_flag[] = "-cl-kernel-arg-info";

/* One declared argument, as clGetKernelArgInfo answers it. */
struct Declaration {
    cl_kernel_arg_address_qualifier address;
    cl_kernel_arg_type_qualifier    qualifier;
    const char                     *type_name;
};

/* In RecordingClDispatch::Declared order. A typedef'd pointer in constant
 * space is named without its star, so only the address says it is one. */
constexpr Declaration declarations[RecordingClDispatch::declared_arguments] = {
    {CL_KERNEL_ARG_ADDRESS_GLOBAL, CL_KERNEL_ARG_TYPE_NONE, "float*"},
    {CL_KERNEL_ARG_ADDRESS_PRIVATE, CL_KERNEL_ARG_TYPE_NONE, "image2d_array_t"},
    {CL_KERNEL_ARG_ADDRESS_PRIVATE, CL_KERNEL_ARG_TYPE_NONE, "sampler_t"},
    {CL_KERNEL_ARG_ADDRESS_PRIVATE, CL_KERNEL_ARG_TYPE_NONE, "ulong"},
    {CL_KERNEL_ARG_ADDRESS_LOCAL, CL_KERNEL_ARG_TYPE_NONE, "float*"},
    {CL_KERNEL_ARG_ADDRESS_PRIVATE, CL_KERNEL_ARG_TYPE_NONE, ""},
    {CL_KERNEL_ARG_ADDRESS_PRIVATE, CL_KERNEL_ARG_TYPE_PIPE, "int"},
    {CL_KERNEL_ARG_ADDRESS_CONSTANT, CL_KERNEL_ARG_TYPE_CONST, "table_t"},
};

/* The ICD's answer to an info query: `bytes` of `answer`, or the size alone. */
cl_int answer_info(const void *answer, std::size_t bytes, std::size_t room, void *destination,
                   std::size_t *written)
{
    if (destination != nullptr && room < bytes) return CL_INVALID_VALUE;
    if (destination != nullptr) std::memcpy(destination, answer, bytes);
    if (written != nullptr) *written = bytes;
    return CL_SUCCESS;
}

}  // namespace

RecordingClDispatch::RecordingClDispatch(const std::uint8_t *block, const WireArena &arena)
    : block_(block), arena_(arena), handles_(), objects_{}, produced_(0u), calls_(0u),
      last_called_(0u), strangers_(0u), strays_(0u), refused_(0u), builds_without_argument_info_(0u),
      logged_{}
{}

void *RecordingClDispatch::handle_for(clproto::HandleKind kind, std::uint64_t wire) const
{
    return handles_.resolve(kind, wire);
}

cl_int RecordingClDispatch::on_clGetKernelArgInfo(cl_kernel kernel, cl_uint arg_index,
                                                  cl_kernel_arg_info param_name,
                                                  std::size_t param_value_size, void *param_value,
                                                  std::size_t *param_value_size_ret)
{
    (void)DefaultDispatch::on_clGetKernelArgInfo(kernel, arg_index, param_name, param_value_size,
                                                 param_value, param_value_size_ret);
    if (argument_declared(arg_index) == Declared::undeclared) return CL_KERNEL_ARG_INFO_NOT_AVAILABLE;
    const Declaration &declaration = declarations[arg_index % declared_arguments];
    switch (param_name) {
    case CL_KERNEL_ARG_ADDRESS_QUALIFIER:
        return answer_info(&declaration.address, sizeof declaration.address, param_value_size,
                           param_value, param_value_size_ret);
    case CL_KERNEL_ARG_TYPE_QUALIFIER:
        return answer_info(&declaration.qualifier, sizeof declaration.qualifier, param_value_size,
                           param_value, param_value_size_ret);
    case CL_KERNEL_ARG_TYPE_NAME:
        return answer_info(declaration.type_name, std::strlen(declaration.type_name) + 1u,
                           param_value_size, param_value, param_value_size_ret);
    default:
        return CL_INVALID_VALUE;
    }
}

cl_int RecordingClDispatch::on_clSetKernelArg(cl_kernel kernel, cl_uint arg_index,
                                              std::size_t arg_size, const void *arg_value)
{
    (void)DefaultDispatch::on_clSetKernelArg(kernel, arg_index, arg_size, arg_value);
    const Declared declared = argument_declared(arg_index);
    if (declared == Declared::scalar || declared == Declared::local) return CL_SUCCESS;
    if (arg_value == nullptr || arg_size != sizeof(cl_mem)) return CL_SUCCESS;
    std::uint64_t object = 0u;
    std::memcpy(&object, arg_value, sizeof object);
    /* An ICD with no declaration to go by reads eight bytes as whatever the
     * kernel wanted, so none may reach it. */
    if (declared == Declared::undeclared) {
        strangers_++;
        return CL_SUCCESS;
    }
    if (object == 0u) return CL_SUCCESS;
    const clproto::HandleKind kind =
        declared == Declared::sampler ? clproto::HandleKind::sampler : clproto::HandleKind::mem;
    if (handles_.resolve(kind, object) == nullptr) strangers_++;
    return CL_SUCCESS;
}

cl_int RecordingClDispatch::on_clBuildProgram(cl_program program, cl_uint num_devices,
                                              const cl_device_id *device_list, const char *options)
{
    built(options);
    return DefaultDispatch::on_clBuildProgram(program, num_devices, device_list, options);
}

cl_int RecordingClDispatch::on_clCompileProgram(cl_program program, cl_uint num_devices,
                                                const cl_device_id *device_list,
                                                const char *options, cl_uint num_input_headers,
                                                const cl_program *input_headers,
                                                const char **header_include_names)
{
    built(options);
    return DefaultDispatch::on_clCompileProgram(program, num_devices, device_list, options,
                                                num_input_headers, input_headers,
                                                header_include_names);
}

void RecordingClDispatch::built(const char *options)
{
    if (options == nullptr || std::strstr(options, kernel_arg_info_flag) == nullptr)
        builds_without_argument_info_++;
}

RecordingClDispatch::Declared RecordingClDispatch::argument_declared(cl_uint index) const
{
    return static_cast<Declared>(index % declared_arguments);
}

void RecordingClDispatch::made(clproto::HandleKind kind, void *object)
{
    if (!handles_.remember(kind, object)) refused_++;
}

void RecordingClDispatch::retained(clproto::HandleKind kind, void *object)
{
    handles_.retain(kind, object);
}

void RecordingClDispatch::released(clproto::HandleKind kind, void *object)
{
    handles_.release(kind, object);
}

std::uint64_t RecordingClDispatch::wire_of(const void *object) const
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(object));
}

std::uint64_t RecordingClDispatch::wire_of_made(std::size_t index) const
{
    return index < produced_ ? wire_of(&objects_[index]) : 0u;
}

std::uint32_t RecordingClDispatch::calls() const { return calls_; }
std::uint32_t RecordingClDispatch::last_called() const { return last_called_; }
std::uint32_t RecordingClDispatch::strangers() const { return strangers_; }
std::uint32_t RecordingClDispatch::strays() const { return strays_; }
std::uint32_t RecordingClDispatch::refused() const { return refused_; }
std::uint32_t RecordingClDispatch::builds_without_argument_info() const
{
    return builds_without_argument_info_;
}
const ClHandleRegistry &RecordingClDispatch::handles() const { return handles_; }

void RecordingClDispatch::called(std::uint32_t command_id)
{
    if (calls_ < max_logged) logged_[calls_] = command_id;
    calls_++;
    last_called_ = command_id;
}

void RecordingClDispatch::handed(clproto::HandleKind kind, void *object)
{
    /* Null is the API's own and the ICD refuses it; anything else has to be
     * an object this dispatch holds as exactly this kind. */
    if (object == nullptr) return;
    if (handles_.resolve(kind, wire_of(object)) != object) strangers_++;
}

void RecordingClDispatch::touched(const void *bytes, std::size_t length)
{
    if (length == 0u) return;
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(bytes);
    const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(block_);
    const std::uintptr_t end = first + arena_.used();
    if (start < first || start > end || length > end - start) strays_++;
}

void *RecordingClDispatch::produce(clproto::HandleKind)
{
    if (produced_ == max_objects) return nullptr;
    return &objects_[produced_++];
}

std::uint32_t RecordingClDispatch::called_at(std::size_t index) const
{
    return index < calls_ && index < max_logged ? logged_[index] : 0u;
}

}  // namespace av
