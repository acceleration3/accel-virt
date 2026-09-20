/* The OpenCL server dispatch with no OpenCL behind it.
 *
 * Holds handles the way OpenClBackend does - a ClHandleRegistry answering the
 * decoder's lookups - and makes objects out of its own storage, so a test or a
 * fuzz target can drive the generated decoder with a real object chain and no
 * GPU. What it records is what the decoder must never do: hand a call an
 * object this dispatch did not make (a stranger), or a span that runs outside
 * what the decode arena gave out (a stray).
 *
 * Its kernels declare their arguments by index, so a test or a fuzz input
 * reaches every kind the decoder tells apart by choosing an index: see
 * argument_declared. Setting an argument the kernel reads as an object with
 * bytes that are not one this dispatch made is a stranger too.
 *
 * And it can answer the way a careless ICD does: see answer_next_with. */
#ifndef AV_RECORDING_CL_DISPATCH_HPP
#define AV_RECORDING_CL_DISPATCH_HPP

#include "device/executor/cl_handle_registry.hpp"
#include "device/executor/host_svm_ranges.hpp"
#include "proto/generated/opencl/cl_default_dispatch.h"

#include <cstddef>
#include <cstdint>

namespace av {

class WireArena;

class RecordingClDispatch final : public clproto::DefaultDispatch {
public:
    /* Enough for any test, and few enough that a fuzz input exhausts them. */
    static constexpr std::size_t max_objects = 256u;
    /* The same, for shared virtual memory. */
    static constexpr std::size_t max_shared = 4096u;

    /* `block` is the start of the memory `arena` hands out from. */
    RecordingClDispatch(const std::uint8_t *block, const WireArena &arena);
    RecordingClDispatch(const RecordingClDispatch &) = delete;
    RecordingClDispatch &operator=(const RecordingClDispatch &) = delete;

    /* What the kernel at an index declares, repeating every
     * `declared_arguments` indices. */
    enum class Declared : std::uint8_t { buffer, image, sampler, scalar, local, undeclared, pipe,
                                         constant };
    static constexpr std::uint32_t declared_arguments = 8u;

    /* What a careless ICD leaves in an output it did not write. */
    static constexpr std::uint8_t garbage = 0xA5u;
    /* What an info query that answers writes into the bytes it reports. */
    static constexpr std::uint8_t answered = 0x6Bu;

    /* The next ID query, device info query or buffer read answers `result`,
     * as an ICD that failed or wrote less than its room and cleared nothing:
     * a query reports `reported` ids or bytes and leaves every output past
     * those it wrote as garbage - all of them, for a result that is not
     * CL_SUCCESS - and a read leaves its bytes and its event as garbage. */
    void answer_next_with(cl_int result, std::size_t reported);

    void *handle_for(clproto::HandleKind kind, std::uint64_t wire) const override;
    /* As handle_for, for an address: a pointer only for one inside something
     * this dispatch allocated, with room for `bytes` after it, and null
     * otherwise - which the decoder reads as a refusal. An address that
     * reaches a call without being one of these is the stranger, and each
     * call below is where that is judged. */
    void *svm_for(std::uint64_t address, std::uint64_t bytes) const override;

    /* Makes `bytes` of shared virtual memory a guest may then name, and
     * answers the address of it, or 0 when there is no room left. */
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
    /* Every call the decoder hands a resolved address: the pointer it got is
     * one this dispatch allocated, or a decoder let a guest name host memory. */
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
    cl_int on_clSetKernelArg(cl_kernel kernel, cl_uint arg_index, std::size_t arg_size,
                             const void *arg_value) override;
    cl_int on_clBuildProgram(cl_program program, cl_uint num_devices, const cl_device_id *device_list,
                             const char *options) override;
    cl_int on_clCompileProgram(cl_program program, cl_uint num_devices,
                               const cl_device_id *device_list, const char *options,
                               cl_uint num_input_headers, const cl_program *input_headers,
                               const char **header_include_names) override;
    void  made(clproto::HandleKind kind, void *object) override;
    void  retained(clproto::HandleKind kind, void *object) override;
    void  released(clproto::HandleKind kind, void *object) override;

    /* The number a guest names `object` by. */
    std::uint64_t wire_of(const void *object) const;
    /* The number of the `index`th object this dispatch made, or 0 for one it
     * has not made yet: what a fuzz target writes where a guest would have
     * written a number it was answered. */
    std::uint64_t wire_of_made(std::size_t index) const;

    std::uint32_t calls() const;
    std::uint32_t last_called() const;
    std::uint32_t strangers() const;
    std::uint32_t strays() const;
    /* Objects made that the registry would not hold. */
    std::uint32_t refused() const;
    /* Builds whose options did not ask the ICD to keep argument information. */
    std::uint32_t builds_without_argument_info() const;
    Declared      argument_declared(cl_uint index) const;
    const ClHandleRegistry &handles() const;
    /* The id of the `index`th call, or 0 past the calls made or the log. */
    std::uint32_t called_at(std::size_t index) const;

protected:
    void  called(std::uint32_t command_id) override;
    void  handed(clproto::HandleKind kind, void *object) override;
    void  touched(const void *bytes, std::size_t length) override;
    void *produce(clproto::HandleKind kind) override;

private:
    /* Counts a build whose options lack -cl-kernel-arg-info. */
    void built(const char *options);
    /* Counts a stranger unless `pointer` is inside something this dispatch
     * allocated, with room for `bytes` after it. Null is no address at all,
     * which is what the guest asked for. */
    void from_svm(const void *pointer, std::uint64_t bytes) const;
    /* Whether answer_next_with is waiting for a call; the call takes it. */
    bool take_answer();
    /* An ID query's answer: garbage past the ids reported, and the count. */
    cl_int scribble_ids(void *ids, cl_uint room, cl_uint *count);

    const std::uint8_t *block_;
    const WireArena    &arena_;
    ClHandleRegistry    handles_;
    HostSvmRanges       svm_;
    /* What make_svm hands out of: real storage, so a decoder that writes
     * through a resolved address writes somewhere it may. */
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

}  // namespace av

#endif  // AV_RECORDING_CL_DISPATCH_HPP
