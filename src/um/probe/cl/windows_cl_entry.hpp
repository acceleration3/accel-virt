/* The CL probe's ClEntry in the guest (feature 052, T030): the Khronos loader
 * beside the probe, called the way any OpenCL application calls it - through
 * the import library src/um/imports/OpenCL.def names, with the API's own
 * types. The user-event pair is not in that import list, so it is resolved
 * from the loaded module by name, as the GL probe resolves what opengl32 does
 * not export. Translation only. */
#ifndef AV_UM_WINDOWS_CL_ENTRY_HPP
#define AV_UM_WINDOWS_CL_ENTRY_HPP

#include "guest/suite/cl_entry.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WindowsClEntry final : public ClEntry {
public:
    WindowsClEntry();
    WindowsClEntry(const WindowsClEntry &) = delete;
    WindowsClEntry &operator=(const WindowsClEntry &) = delete;

    int32_t platform_count(uint32_t &count) override;
    int32_t platforms(void **into, uint32_t room, uint32_t &written) override;
    int32_t platform_text(void *platform, uint32_t name, char *into, size_t room,
                          size_t &bytes) override;
    int32_t devices(void *platform, void **into, uint32_t room, uint32_t &written) override;
    int32_t device_text(void *device, uint32_t name, char *into, size_t room,
                        size_t &bytes) override;
    int32_t device_number(void *device, uint32_t name, uint64_t &value) override;
    void   *create_buffer_with_properties(void *context, const uint64_t *properties, uint64_t flags,
                                          size_t bytes, int32_t &error) override;

    void   *create_context(void *device, int32_t &error) override;
    void   *create_queue(void *context, void *device, int32_t &error) override;
    void   *create_buffer(void *context, uint64_t flags, size_t bytes, int32_t &error) override;
    void   *create_program(void *context, const char *source, int32_t &error) override;
    int32_t build_program(void *program, void *device, const char *options) override;
    int32_t program_build_text(void *program, void *device, char *into, size_t room,
                               size_t &bytes) override;
    void   *create_kernel(void *program, const char *name, int32_t &error) override;

    int32_t set_kernel_buffer(void *kernel, uint32_t index, void *buffer) override;
    int32_t write_buffer(void *queue, void *buffer, size_t bytes, const void *from) override;
    int32_t read_buffer(void *queue, void *buffer, size_t bytes, void *into) override;
    int32_t run_kernel(void *queue, void *kernel, size_t elements) override;
    int32_t finish(void *queue) override;

    void   *map_buffer(void *queue, void *buffer, uint64_t flags, size_t bytes,
                       int32_t &error) override;
    int32_t unmap(void *queue, void *buffer, void *pointer) override;

    void   *svm_alloc(void *context, size_t bytes) override;
    void    svm_free(void *context, void *pointer) override;
    int32_t set_kernel_svm(void *kernel, uint32_t index, void *pointer) override;

    void   *create_user_event(void *context, int32_t &error) override;
    int32_t set_user_event_status(void *event, int32_t status) override;
    int32_t watch_event(void *event, int32_t status,
                        void (*notify)(void *event, int32_t status, void *user_data),
                        void *user_data) override;
    int32_t wait_for_event(void *event) override;

    void release_event(void *event) override;
    void release_kernel(void *kernel) override;
    void release_program(void *program) override;
    void release_buffer(void *buffer) override;
    void release_queue(void *queue) override;
    void release_context(void *context) override;

private:
    /* An entry of the loaded OpenCL.dll the import library does not name; null
     * when the loader beside the probe has no such export. */
    template <typename Function>
    Function named(const char *name) const;

    /* The loader this process already bound to, which is where the names the
     * import library leaves out are found. */
    void *loader_;
};

}  // namespace av

#endif  // AV_UM_WINDOWS_CL_ENTRY_HPP
