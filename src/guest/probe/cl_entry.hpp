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

    virtual int32_t platform_count(uint32_t &count) = 0;
    virtual int32_t platforms(void **into, uint32_t room, uint32_t &written) = 0;
    virtual int32_t platform_text(void *platform, uint32_t name, char *into, size_t room,
                                  size_t &bytes) = 0;
    virtual int32_t devices(void *platform, void **into, uint32_t room, uint32_t &written) = 0;
    virtual int32_t device_text(void *device, uint32_t name, char *into, size_t room,
                                size_t &bytes) = 0;
    virtual int32_t device_number(void *device, uint32_t name, uint64_t &value) = 0;

    virtual void *create_context(void *device, int32_t &error) = 0;
    virtual void *create_queue(void *context, void *device, int32_t &error) = 0;
    virtual void *create_buffer(void *context, uint64_t flags, size_t bytes, int32_t &error) = 0;
    virtual void *create_buffer_with_properties(void *context, const uint64_t *properties, uint64_t flags,
                                                size_t bytes, int32_t &error) = 0;
    virtual void *create_program(void *context, const char *const *sources, uint32_t count, int32_t &error) = 0;
    virtual int32_t build_program(void *program, void *device, const char *options) = 0;
    virtual int32_t program_build_text(void *program, void *device, char *into, size_t room,
                                       size_t &bytes) = 0;
    virtual void *create_kernel(void *program, const char *name, int32_t &error) = 0;
    virtual uint8_t *program_binary(void *program, size_t &bytes, int32_t &error) = 0;
    virtual void *create_program_with_binary(void *context, void *device, const uint8_t *binary, size_t bytes,
                                             int32_t &status, int32_t &error) = 0;
    virtual void *create_program_with_il(void *context, const void *il, size_t bytes, int32_t &error) = 0;
    virtual int32_t compile_program(void *program, void *device, void *header, const char *header_name) = 0;
    virtual void *link_program(void *context, void *const *programs, uint32_t count, int32_t &error) = 0;

    virtual int32_t set_kernel_buffer(void *kernel, uint32_t index, void *buffer) = 0;
    virtual int32_t write_buffer(void *queue, void *buffer, size_t bytes, const void *from) = 0;
    virtual int32_t read_buffer(void *queue, void *buffer, size_t bytes, void *into) = 0;
    virtual int32_t run_kernel(void *queue, void *kernel, size_t elements) = 0;
    virtual int32_t finish(void *queue) = 0;

    virtual void   *map_buffer(void *queue, void *buffer, uint64_t flags, size_t bytes,
                               int32_t &error) = 0;
    virtual int32_t unmap(void *queue, void *buffer, void *pointer) = 0;

    virtual void   *svm_alloc(void *context, size_t bytes) = 0;
    virtual void    svm_free(void *context, void *pointer) = 0;
    virtual int32_t set_kernel_svm(void *kernel, uint32_t index, void *pointer) = 0;

    virtual void   *create_user_event(void *context, int32_t &error) = 0;
    virtual int32_t set_user_event_status(void *event, int32_t status) = 0;
    virtual int32_t watch_event(void *event, int32_t status,
                                void(AV_RUNTIME_CALL *notify)(void *event, int32_t status, void *user_data),
                                void *user_data) = 0;
    virtual int32_t wait_for_event(void *event) = 0;

    virtual void release_event(void *event) = 0;
    virtual void release_kernel(void *kernel) = 0;
    virtual void release_program(void *program) = 0;
    virtual void release_buffer(void *buffer) = 0;
    virtual void release_queue(void *queue) = 0;
    virtual void release_context(void *context) = 0;

protected:
    ClEntry() = default;
    ~ClEntry() = default;
};

}

#endif
