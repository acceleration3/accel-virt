/* The OpenCL the CL probe calls (feature 052): one method per call it makes,
 * with the API's own types.
 *
 * In the guest this is the Khronos loader the runner pushes beside the probe -
 * which is the whole point of the probe, because it is the loader's discovery
 * of OpenCLDriverName on the avgpu adapter that SC-001 is about. On the
 * full-stack rig it is the ICD's dispatch table reached the way a loader
 * reaches it; in the probe's own unit tests, a scripted OpenCL. Virtual for
 * the reason Platform is. */
#ifndef AV_CL_ENTRY_HPP
#define AV_CL_ENTRY_HPP

#include "core/support/runtime_call.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClEntry {
public:
    ClEntry(const ClEntry &) = delete;
    ClEntry &operator=(const ClEntry &) = delete;

    /* clGetPlatformIDs in its two-call shape: how many, then which. */
    virtual int32_t platform_count(uint32_t &count) = 0;
    virtual int32_t platforms(void **into, uint32_t room, uint32_t &written) = 0;
    /* clGetPlatformInfo, into `into`; the bytes the answer needs. */
    virtual int32_t platform_text(void *platform, uint32_t name, char *into, size_t room,
                                  size_t &bytes) = 0;
    /* clGetDeviceIDs for GPUs of `platform`. */
    virtual int32_t devices(void *platform, void **into, uint32_t room, uint32_t &written) = 0;
    virtual int32_t device_text(void *device, uint32_t name, char *into, size_t room,
                                size_t &bytes) = 0;
    virtual int32_t device_number(void *device, uint32_t name, uint64_t &value) = 0;

    /* The objects a run makes, each null on failure with the error in
     * `error`. */
    virtual void *create_context(void *device, int32_t &error) = 0;
    virtual void *create_queue(void *context, void *device, int32_t &error) = 0;
    virtual void *create_buffer(void *context, uint64_t flags, size_t bytes, int32_t &error) = 0;
    /* clCreateBufferWithProperties, which is how a buffer is made over memory
     * another API allocated: the Win32 handle travels in the property list
     * (feature 053). `properties` is a terminated list of name/value pairs. */
    virtual void *create_buffer_with_properties(void *context, const uint64_t *properties, uint64_t flags,
                                                size_t bytes, int32_t &error) = 0;
    /* clCreateProgramWithSource of `count` NUL-terminated strings. */
    virtual void *create_program(void *context, const char *const *sources, uint32_t count, int32_t &error) = 0;
    virtual int32_t build_program(void *program, void *device, const char *options) = 0;
    virtual int32_t program_build_text(void *program, void *device, char *into, size_t room,
                                       size_t &bytes) = 0;
    virtual void *create_kernel(void *program, const char *name, int32_t &error) = 0;

    /* What a run does with them. */
    virtual int32_t set_kernel_buffer(void *kernel, uint32_t index, void *buffer) = 0;
    virtual int32_t write_buffer(void *queue, void *buffer, size_t bytes, const void *from) = 0;
    virtual int32_t read_buffer(void *queue, void *buffer, size_t bytes, void *into) = 0;
    virtual int32_t run_kernel(void *queue, void *kernel, size_t elements) = 0;
    virtual int32_t finish(void *queue) = 0;

    /* A mapping of a buffer, and what gives it back. */
    virtual void   *map_buffer(void *queue, void *buffer, uint64_t flags, size_t bytes,
                               int32_t &error) = 0;
    virtual int32_t unmap(void *queue, void *buffer, void *pointer) = 0;

    /* Shared virtual memory, which a device need not have. */
    virtual void   *svm_alloc(void *context, size_t bytes) = 0;
    virtual void    svm_free(void *context, void *pointer) = 0;
    virtual int32_t set_kernel_svm(void *kernel, uint32_t index, void *pointer) = 0;

    /* An event, and a callback on it. */
    virtual void   *create_user_event(void *context, int32_t &error) = 0;
    virtual int32_t set_user_event_status(void *event, int32_t status) = 0;
    virtual int32_t watch_event(void *event, int32_t status,
                                void(AV_RUNTIME_CALL *notify)(void *event, int32_t status, void *user_data),
                                void *user_data) = 0;
    virtual int32_t wait_for_event(void *event) = 0;

    /* Releases, in the order a run unwinds them. */
    virtual void release_event(void *event) = 0;
    virtual void release_kernel(void *kernel) = 0;
    virtual void release_program(void *program) = 0;
    virtual void release_buffer(void *buffer) = 0;
    virtual void release_queue(void *queue) = 0;
    virtual void release_context(void *context) = 0;

protected:
    ClEntry() = default;
    /* Non-virtual: nothing is destroyed through this pointer (CLAUDE.md,
     * Adaptations). */
    ~ClEntry() = default;
};

}  // namespace av

#endif  // AV_CL_ENTRY_HPP
