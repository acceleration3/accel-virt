#include "harness/executor/recording_cl_dispatch.hpp"

#include "proto/wire/wire_arena.hpp"

#include <cstring>

namespace av {

namespace {

constexpr const char kernel_arg_info_flag[] = "-cl-kernel-arg-info";

struct Declaration {
    cl_kernel_arg_address_qualifier address;
    cl_kernel_arg_type_qualifier    qualifier;
    const char                     *type_name;
};

constexpr Declaration declarations[RecordingClDispatch::declared_arguments] = {
    {CL_KERNEL_ARG_ADDRESS_GLOBAL, CL_KERNEL_ARG_TYPE_NONE, "float*"},
    {CL_KERNEL_ARG_ADDRESS_PRIVATE, CL_KERNEL_ARG_TYPE_NONE, "image2d_array_t"},
    {CL_KERNEL_ARG_ADDRESS_PRIVATE, CL_KERNEL_ARG_TYPE_NONE, "sampler_t"},
    {CL_KERNEL_ARG_ADDRESS_PRIVATE, CL_KERNEL_ARG_TYPE_NONE, "ulong"},
    {CL_KERNEL_ARG_ADDRESS_LOCAL, CL_KERNEL_ARG_TYPE_NONE, "float*"},
    {CL_KERNEL_ARG_ADDRESS_PRIVATE, CL_KERNEL_ARG_TYPE_NONE, ""},
    {CL_KERNEL_ARG_ADDRESS_PRIVATE, CL_KERNEL_ARG_TYPE_PIPE, "int"},
    {CL_KERNEL_ARG_ADDRESS_CONSTANT, CL_KERNEL_ARG_TYPE_CONST, "table_t"},
    {CL_KERNEL_ARG_ADDRESS_GLOBAL, CL_KERNEL_ARG_TYPE_NONE, ""},
    {CL_KERNEL_ARG_ADDRESS_PRIVATE, CL_KERNEL_ARG_TYPE_NONE, ""},
};

cl_int answer_info(const void *answer, std::size_t bytes, std::size_t room, void *destination,
                   std::size_t *written)
{
    if (destination != nullptr && room < bytes) return CL_INVALID_VALUE;
    if (destination != nullptr) std::memcpy(destination, answer, bytes);
    if (written != nullptr) *written = bytes;
    return CL_SUCCESS;
}

}

RecordingClDispatch::RecordingClDispatch(const std::uint8_t *block, const WireArena &arena)
    : platform_(), block_(block), arena_(arena), lent_(nullptr), lent_bytes_(0u), handles_(platform_), svm_(platform_), shared_(), shared_used_(0u), objects_{}, produced_(0u), calls_(0u),
      last_called_(0u), strangers_(0u), strays_(0u), refused_(0u), builds_without_argument_info_(0u),
      answer_pending_(false), answer_result_(CL_SUCCESS), answer_reported_(0u), logged_{}
{}

void RecordingClDispatch::answer_next_with(cl_int result, std::size_t reported)
{
    answer_pending_ = true;
    answer_result_ = result;
    answer_reported_ = reported;
}

bool RecordingClDispatch::take_answer()
{
    const bool pending = answer_pending_;
    answer_pending_ = false;
    return pending;
}

cl_int RecordingClDispatch::scribble_ids(void *ids, cl_uint room, cl_uint *count)
{
    std::size_t kept = 0u;
    if (answer_result_ == CL_SUCCESS) kept = answer_reported_ < room ? answer_reported_ : room;
    if (ids != nullptr && kept < room)
        std::memset(static_cast<char *>(ids) + kept * sizeof(void *), garbage, (room - kept) * sizeof(void *));
    if (count != nullptr) *count = static_cast<cl_uint>(answer_reported_);
    return answer_result_;
}

cl_int RecordingClDispatch::on_clGetPlatformIDs(cl_uint num_entries, cl_platform_id *platforms,
                                                cl_uint *num_platforms)
{
    const cl_int listed = DefaultDispatch::on_clGetPlatformIDs(num_entries, platforms, num_platforms);
    return take_answer() ? scribble_ids(platforms, num_entries, num_platforms) : listed;
}

cl_int RecordingClDispatch::on_clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type,
                                              cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices)
{
    const cl_int listed = DefaultDispatch::on_clGetDeviceIDs(platform, device_type, num_entries, devices, num_devices);
    return take_answer() ? scribble_ids(devices, num_entries, num_devices) : listed;
}

cl_int RecordingClDispatch::on_clGetDeviceInfo(cl_device_id device, cl_device_info param_name,
                                               std::size_t param_value_size, void *param_value,
                                               std::size_t *param_value_size_ret)
{
    const cl_int described = DefaultDispatch::on_clGetDeviceInfo(device, param_name, param_value_size,
                                                                 param_value, param_value_size_ret);
    if (!take_answer()) return described;
    const std::size_t written =
        answer_result_ == CL_SUCCESS && answer_reported_ < param_value_size ? answer_reported_ : 0u;
    if (param_value != nullptr) {
        std::memset(param_value, garbage, param_value_size);
        std::memset(param_value, answered, written);
    }
    if (param_value_size_ret != nullptr) *param_value_size_ret = answer_reported_;
    return answer_result_;
}

cl_int RecordingClDispatch::on_clEnqueueReadBuffer(cl_command_queue command_queue, cl_mem buffer,
                                                   cl_bool blocking_read, std::size_t offset, std::size_t size,
                                                   void *ptr, cl_uint num_events_in_wait_list,
                                                   const cl_event *event_wait_list, cl_event *event)
{
    const cl_int read = DefaultDispatch::on_clEnqueueReadBuffer(command_queue, buffer, blocking_read, offset, size,
                                                                ptr, num_events_in_wait_list, event_wait_list, event);
    if (!take_answer()) return read;
    if (ptr != nullptr) std::memset(ptr, garbage, size);
    if (event != nullptr) std::memset(static_cast<void *>(event), garbage, sizeof *event);
    return answer_result_;
}

void *RecordingClDispatch::handle_for(clproto::HandleKind kind, std::uint64_t wire) const
{
    return handles_.resolve(kind, wire);
}

void *RecordingClDispatch::svm_for(std::uint64_t address, std::uint64_t bytes) const
{
    return svm_.resolve(address, bytes);
}

void *RecordingClDispatch::lent_for(std::uint64_t offset, std::uint64_t bytes) const
{
    if (lent_ == nullptr || offset > lent_bytes_ || bytes > lent_bytes_ - offset) return nullptr;
    return lent_ + offset;
}

void RecordingClDispatch::lend(std::uint8_t *block, std::size_t bytes)
{
    lent_ = block;
    lent_bytes_ = block == nullptr ? 0u : bytes;
}

bool RecordingClDispatch::inside_lent(const void *bytes, std::size_t length) const
{
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(bytes);
    const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(lent_);
    return lent_ != nullptr && start >= first && start - first <= lent_bytes_ &&
           length <= lent_bytes_ - (start - first);
}

void RecordingClDispatch::from_svm(const void *pointer, std::uint64_t bytes) const
{
    if (pointer == nullptr) return;
    if (svm_.resolve(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)), bytes) == nullptr)
        ++strangers_;
}

cl_int RecordingClDispatch::on_clEnqueueUnmapMemObject(cl_command_queue command_queue, cl_mem memobj,
                                                       void *mapped_ptr, cl_uint num_events_in_wait_list,
                                                       const cl_event *event_wait_list, cl_event *event)
{
    from_svm(mapped_ptr, 0u);
    return DefaultDispatch::on_clEnqueueUnmapMemObject(command_queue, memobj, mapped_ptr,
                                                       num_events_in_wait_list, event_wait_list, event);
}

void RecordingClDispatch::on_clSVMFree(cl_context context, void *svm_pointer)
{
    from_svm(svm_pointer, 0u);
    DefaultDispatch::on_clSVMFree(context, svm_pointer);
}

cl_int RecordingClDispatch::on_clEnqueueSVMMap(cl_command_queue command_queue, cl_bool blocking_map,
                                               cl_map_flags flags, void *svm_ptr, std::size_t size,
                                               cl_uint num_events_in_wait_list,
                                               const cl_event *event_wait_list, cl_event *event)
{
    from_svm(svm_ptr, static_cast<std::uint64_t>(size));
    return DefaultDispatch::on_clEnqueueSVMMap(command_queue, blocking_map, flags, svm_ptr, size,
                                               num_events_in_wait_list, event_wait_list, event);
}

cl_int RecordingClDispatch::on_clEnqueueSVMUnmap(cl_command_queue command_queue, void *svm_ptr,
                                                 cl_uint num_events_in_wait_list,
                                                 const cl_event *event_wait_list, cl_event *event)
{
    from_svm(svm_ptr, 0u);
    return DefaultDispatch::on_clEnqueueSVMUnmap(command_queue, svm_ptr, num_events_in_wait_list,
                                                 event_wait_list, event);
}

cl_int RecordingClDispatch::on_clEnqueueSVMMemcpy(cl_command_queue command_queue, cl_bool blocking_copy,
                                                  void *dst_ptr, const void *src_ptr, std::size_t size,
                                                  cl_uint num_events_in_wait_list,
                                                  const cl_event *event_wait_list, cl_event *event)
{
    from_svm(dst_ptr, static_cast<std::uint64_t>(size));
    from_svm(src_ptr, static_cast<std::uint64_t>(size));
    return DefaultDispatch::on_clEnqueueSVMMemcpy(command_queue, blocking_copy, dst_ptr, src_ptr, size,
                                                  num_events_in_wait_list, event_wait_list, event);
}

cl_int RecordingClDispatch::on_clEnqueueSVMMemFill(cl_command_queue command_queue, void *svm_ptr,
                                                   const void *pattern, std::size_t pattern_size,
                                                   std::size_t size, cl_uint num_events_in_wait_list,
                                                   const cl_event *event_wait_list, cl_event *event)
{
    from_svm(svm_ptr, static_cast<std::uint64_t>(size));
    return DefaultDispatch::on_clEnqueueSVMMemFill(command_queue, svm_ptr, pattern, pattern_size, size,
                                                   num_events_in_wait_list, event_wait_list, event);
}

cl_int RecordingClDispatch::on_clSetKernelArgSVMPointer(cl_kernel kernel, cl_uint arg_index,
                                                        const void *arg_value)
{
    from_svm(arg_value, 0u);
    return DefaultDispatch::on_clSetKernelArgSVMPointer(kernel, arg_index, arg_value);
}

cl_int RecordingClDispatch::on_clSetKernelExecInfo(cl_kernel kernel, cl_kernel_exec_info param_name,
                                                   std::size_t param_value_size, const void *param_value)
{
    if (param_name == CL_KERNEL_EXEC_INFO_SVM_PTRS && param_value != nullptr) {
        const void *const *const pointers = static_cast<const void *const *>(param_value);
        for (std::size_t index = 0u; index < param_value_size / sizeof(void *); ++index) from_svm(pointers[index], 0u);
    }
    return DefaultDispatch::on_clSetKernelExecInfo(kernel, param_name, param_value_size, param_value);
}

std::uint64_t RecordingClDispatch::make_svm(std::size_t bytes)
{
    if (bytes == 0u || bytes > max_shared - shared_used_) return 0u;
    unsigned char *const allocated = &shared_[shared_used_];
    if (svm_.remember(allocated, static_cast<std::uint64_t>(bytes), 0u) != Status::ok) return 0u;
    shared_used_ += bytes;
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(allocated));
}

cl_int RecordingClDispatch::on_clGetKernelArgInfo(cl_kernel kernel, cl_uint arg_index,
                                                  cl_kernel_arg_info param_name,
                                                  std::size_t param_value_size, void *param_value,
                                                  std::size_t *param_value_size_ret)
{
    (void)DefaultDispatch::on_clGetKernelArgInfo(kernel, arg_index, param_name, param_value_size,
                                                 param_value, param_value_size_ret);
    const Declared declared = argument_declared(arg_index);
    if (declared == Declared::undeclared) return CL_KERNEL_ARG_INFO_NOT_AVAILABLE;
    const Declaration &declaration = declarations[arg_index % declared_arguments];
    const bool untyped = declared == Declared::untyped_global || declared == Declared::untyped_private;
    if (untyped && param_name != CL_KERNEL_ARG_ADDRESS_QUALIFIER) return CL_KERNEL_ARG_INFO_NOT_AVAILABLE;
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
    if (declared == Declared::undeclared) {
        strangers_++;
        return CL_SUCCESS;
    }
    if (object == 0u) return CL_SUCCESS;
    const clproto::HandleKind kind = declared == Declared::sampler || declared == Declared::untyped_private
                                         ? clproto::HandleKind::sampler
                                         : clproto::HandleKind::mem;
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

bool RecordingClDispatch::declared_argument(cl_kernel, cl_uint index, clproto::KernelArgument &argument)
{
    if (argument_declared(index) != Declared::untyped_private) return false;
    argument = clproto::KernelArgument::sampler;
    return true;
}

RecordingClDispatch::Declared RecordingClDispatch::argument_declared(cl_uint index) const
{
    return static_cast<Declared>(index % declared_arguments);
}

void RecordingClDispatch::made(clproto::HandleKind kind, void *object)
{
    if (handles_.remember(kind, object) != Status::ok) refused_++;
}

bool RecordingClDispatch::room_for(std::uint64_t objects) { return handles_.make_room(objects) == Status::ok; }

MockPlatform &RecordingClDispatch::platform() { return platform_; }

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
    if (object == nullptr) return;
    if (handles_.resolve(kind, wire_of(object)) != object) strangers_++;
}

void RecordingClDispatch::touched(const void *bytes, std::size_t length)
{
    if (length == 0u || inside_lent(bytes, length)) return;
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

}
