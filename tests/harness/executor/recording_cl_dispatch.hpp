#ifndef AV_RECORDING_CL_DISPATCH_HPP
#define AV_RECORDING_CL_DISPATCH_HPP

#include "executor/host_object_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "executor/opencl/host_svm_ranges.hpp"
#include "proto/generated/opencl/cl_default_dispatch.h"
#include "proto/wire/guest_bytes.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class WireArena;

class RecordingClDispatch final : public clproto::DefaultDispatch {
public:
    static constexpr std::size_t max_objects = 256u;
    static constexpr std::size_t max_shared = 4096u;

    RecordingClDispatch(const std::uint8_t *block, const WireArena &arena);
    RecordingClDispatch(const RecordingClDispatch &) = delete;
    RecordingClDispatch &operator=(const RecordingClDispatch &) = delete;

    enum class Declared : std::uint8_t { buffer, image, sampler, scalar, local, undeclared, pipe,
                                         constant, untyped_global, untyped_private };
    static constexpr std::uint32_t declared_arguments = 10u;

    static constexpr std::uint8_t garbage = 0xA5u;
    static constexpr std::uint8_t answered = 0x6Bu;

    void answer_next_with(cl_int result, std::size_t reported);

    void *handle_for(clproto::HandleKind kind, std::uint64_t wire) const override;
    bool  declared_argument(cl_kernel kernel, cl_uint index, clproto::KernelArgument &argument) override;
    void *svm_for(std::uint64_t address, std::uint64_t bytes) const override;
    void  lend(std::uint8_t *block, std::size_t bytes);

    std::uint64_t make_svm(std::size_t bytes);
    cl_int on_clGetPlatformIDs(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms) override;
    cl_int on_clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries,
                             cl_device_id *devices, cl_uint *num_devices) override;
    cl_int on_clGetDeviceInfo(cl_device_id device, cl_device_info param_name, std::size_t param_value_size,
                              void *param_value, std::size_t *param_value_size_ret) override;
    cl_int on_clEnqueueReadBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
                                  std::size_t offset, std::size_t size, void *ptr, cl_uint num_events_in_wait_list,
                                  const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clGetKernelArgInfo(cl_kernel kernel, cl_uint arg_index, cl_kernel_arg_info param_name,
                                 std::size_t param_value_size, void *param_value,
                                 std::size_t *param_value_size_ret) override;
    cl_int on_clEnqueueUnmapMemObject(cl_command_queue command_queue, cl_mem memobj, void *mapped_ptr,
                                      cl_uint num_events_in_wait_list, const cl_event *event_wait_list,
                                      cl_event *event) override;
    void   on_clSVMFree(cl_context context, void *svm_pointer) override;
    cl_int on_clEnqueueSVMMap(cl_command_queue command_queue, cl_bool blocking_map, cl_map_flags flags,
                              void *svm_ptr, std::size_t size, cl_uint num_events_in_wait_list,
                              const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clEnqueueSVMUnmap(cl_command_queue command_queue, void *svm_ptr,
                                cl_uint num_events_in_wait_list, const cl_event *event_wait_list,
                                cl_event *event) override;
    cl_int on_clEnqueueSVMMemcpy(cl_command_queue command_queue, cl_bool blocking_copy, void *dst_ptr,
                                 const void *src_ptr, std::size_t size, cl_uint num_events_in_wait_list,
                                 const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clEnqueueSVMMemFill(cl_command_queue command_queue, void *svm_ptr, const void *pattern,
                                  std::size_t pattern_size, std::size_t size,
                                  cl_uint num_events_in_wait_list, const cl_event *event_wait_list,
                                  cl_event *event) override;
    cl_int on_clSetKernelArgSVMPointer(cl_kernel kernel, cl_uint arg_index, const void *arg_value) override;
    cl_int on_clSetKernelExecInfo(cl_kernel kernel, cl_kernel_exec_info param_name, std::size_t param_value_size,
                                  const void *param_value) override;
    cl_int on_clSetKernelArg(cl_kernel kernel, cl_uint arg_index, std::size_t arg_size,
                             const void *arg_value) override;
    cl_int on_clBuildProgram(cl_program program, cl_uint num_devices, const cl_device_id *device_list,
                             const char *options) override;
    cl_int on_clCompileProgram(cl_program program, cl_uint num_devices,
                               const cl_device_id *device_list, const char *options,
                               cl_uint num_input_headers, const cl_program *input_headers,
                               const char **header_include_names) override;
    void  made(clproto::HandleKind kind, void *object) override;
    bool  room_for(std::uint64_t objects) override;
    void  retained(clproto::HandleKind kind, void *object) override;
    void  released(clproto::HandleKind kind, void *object) override;

    std::uint64_t wire_of(const void *object) const;
    std::uint64_t wire_of_made(std::size_t index) const;

    std::uint32_t calls() const;
    std::uint32_t last_called() const;
    std::uint32_t strangers() const;
    std::uint32_t strays() const;
    std::uint32_t refused() const;
    std::uint32_t builds_without_argument_info() const;
    Declared      argument_declared(cl_uint index) const;
    const HostObjectRegistry<std::uint64_t, clproto::HandleKind, void *> &handles() const;
    MockPlatform           &platform();
    std::uint32_t called_at(std::size_t index) const;

protected:
    void  called(std::uint32_t command_id) override;
    void  handed(clproto::HandleKind kind, void *object) override;
    void  touched(const void *bytes, std::size_t length) override;
    void *produce(clproto::HandleKind kind) override;

private:
    void built(const char *options);
    void from_svm(const void *pointer, std::uint64_t bytes) const;
    bool take_answer();
    cl_int scribble_ids(void *ids, cl_uint room, cl_uint *count);

    bool inside_lent(const void *bytes, std::size_t length) const;

    MockPlatform        platform_;
    const std::uint8_t *block_;
    const WireArena    &arena_;
    GuestBytes          guest_;
    std::uint8_t       *bound_;
    std::uint8_t       *lent_;
    std::size_t         lent_bytes_;
    HostObjectRegistry<std::uint64_t, clproto::HandleKind, void *> handles_;
    HostSvmRanges       svm_;
    unsigned char       shared_[max_shared];
    std::size_t         shared_used_;
    unsigned char       objects_[max_objects];
    std::size_t         produced_;
    std::uint32_t       calls_;
    std::uint32_t       last_called_;
    mutable std::uint32_t strangers_;
    std::uint32_t       strays_;
    std::uint32_t       refused_;
    std::uint32_t       builds_without_argument_info_;
    bool                answer_pending_;
    cl_int              answer_result_;
    std::size_t         answer_reported_;

    static constexpr std::size_t max_logged = 64u;
    std::uint32_t                logged_[max_logged];
};

}

#endif
