/* The Khronos loader's side of the ICD ABI, as a test drives it.
 *
 * The point of this class is that it dispatches the way the real loader does:
 * it reads the `cl_icd_dispatch *` out of the first word of whatever object it
 * is handed and calls through the slot, rather than calling the ICD's methods
 * by name. A slot the ICD left null, or filled with an entry of the wrong
 * arity, fails a test here instead of a guest there.
 *
 * It follows the pinned loader's own sequence (research §1): resolve
 * clGetExtensionFunctionAddress, ask it for clIcdGetPlatformIDsKHR, enumerate
 * in two calls, skip a platform whose dispatch is null, then ask for
 * CL_PLATFORM_EXTENSIONS and CL_PLATFORM_ICD_SUFFIX_KHR - the second of which
 * drops the platform if it fails. */
#ifndef AV_FAKE_CL_LOADER_HPP
#define AV_FAKE_CL_LOADER_HPP

#include <CL/cl.h>
#include <CL/cl_icd.h>

#include <cstdint>

namespace av::clicd {
class ClIcdRoot;
}

namespace av {

class FakeClLoader {
public:
    /* Why a driver was not kept, in the order the loader decides it. */
    enum class Load : std::uint8_t {
        kept,
        no_extension_function,   /* the one exported symbol answered nothing */
        no_enumeration,          /* clGetExtensionFunctionAddress had no clIcdGetPlatformIDsKHR */
        enumeration_failed,      /* the count call refused */
        no_platform,             /* it answered none */
        not_dispatchable,        /* the platform's first word was null */
        no_extensions,           /* CL_PLATFORM_EXTENSIONS refused */
        no_suffix,               /* CL_PLATFORM_ICD_SUFFIX_KHR refused: dropped */
    };

    static constexpr std::uint32_t max_platforms = 4u;

    explicit FakeClLoader(clicd::ClIcdRoot &root);
    FakeClLoader(const FakeClLoader &) = delete;
    FakeClLoader &operator=(const FakeClLoader &) = delete;

    /* Walks the loader's own sequence. Keeps the platforms it accepted. */
    Load load();
    Load state() const;

    /* Hides the one symbol a vendor library exports, so a test can play a DLL
     * that is not an ICD at all. */
    void hide_export();

    std::uint32_t platforms() const;
    cl_platform_id platform(std::uint32_t index) const;

    /* What the loader read while deciding. */
    const char *suffix() const;
    const char *extensions() const;

    /* Dispatching through the table, as every loader entry point does: the
     * object's first word, the slot, the call. */
    const cl_icd_dispatch *dispatch_of(const void *object) const;

    cl_int         device_ids(cl_device_type type, cl_uint num_entries, cl_device_id *devices,
                              cl_uint *num_devices);
    cl_int         device_info(cl_device_id device, cl_device_info name, std::size_t bytes,
                               void *value, std::size_t *size_ret);
    cl_context     create_context(cl_device_id device, cl_int *error);
    /* The same, with the callback the application reports the context's
     * errors to, and its data. */
    cl_context     create_context(cl_device_id device, void(CL_CALLBACK *notify)(const char *, const void *, size_t, void *),
                                  void *user_data, cl_int *error);
    cl_int         release_context(cl_context context);
    cl_command_queue create_queue(cl_context context, cl_device_id device, cl_int *error);
    cl_mem         create_buffer(cl_context context, cl_mem_flags flags, std::size_t bytes,
                                 cl_int *error);
    /* The same slot's neighbour, which carries the list an external-memory
     * create needs (feature 053). */
    cl_mem         create_buffer_with_properties(cl_context context, const cl_mem_properties *properties,
                                                 cl_mem_flags flags, std::size_t bytes, cl_int *error);
    cl_program     create_program(cl_context context, const char *source, cl_int *error);
    cl_int         build_program(cl_program program, const char *options);
    cl_kernel      create_kernel(cl_program program, const char *name, cl_int *error);
    cl_int         finish(cl_command_queue queue);
    cl_int         release_queue(cl_command_queue queue);
    cl_int         release_mem(cl_mem buffer);
    /* Events, through the same table: a slot the ICD left null would be a
     * null call here rather than a test that quietly passed. */
    cl_event       create_user_event(cl_context context, cl_int *error);
    cl_int         set_user_event_status(cl_event event, cl_int status);
    cl_int         wait_for_events(cl_uint count, const cl_event *events);
    cl_int         event_info(cl_event event, cl_event_info name, std::size_t bytes, void *value,
                              std::size_t *size_ret);
    cl_int         set_event_callback(cl_event event, cl_int status,
                                      void(CL_CALLBACK *notify)(cl_event, cl_int, void *),
                                      void *user_data);
    cl_int         release_event(cl_event event);

private:
    clicd::ClIcdRoot *root_;
    bool              hidden_;
    Load              state_;
    cl_platform_id    platforms_[max_platforms];
    std::uint32_t     count_;
    char              suffix_[32];
    char              extensions_[256];
};

}  // namespace av

#endif  // AV_FAKE_CL_LOADER_HPP
