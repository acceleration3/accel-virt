/* The extension list a guest is shown for its device: the host's, with the
 * names this executor answers differently from the host struck out or added
 * (OpenClBackend::on_clGetDeviceInfo).
 *
 * Two names are the executor's to say. `cl_khr_gl_sharing` is said where a
 * context made from a GL context of the guest's can open its objects - through
 * the host's own sharing or by copies - and struck where no GL context can be
 * made on the GPU at all. `cl_khr_external_memory` is said where a descriptor
 * another API exported can be opened as a cl_mem (OpenClBackend::import_object)
 * and struck where it cannot, whatever the host's own OpenCL lists: the guest's
 * ICD answers that extension itself, over this one ability (its policy). */
#ifndef AV_CL_EXTENSION_LIST_HPP
#define AV_CL_EXTENSION_LIST_HPP

#include <CL/cl.h>

#include <stddef.h>

namespace av {

struct ClExtensionListProbe;

class ClExtensionList {
public:
    /* The longest host list held, terminator included - NVIDIA's is some 600
     * bytes and Intel's 1.7 KiB - and the room beyond it for names added. */
    static constexpr size_t host_bytes = 8192u;
    static constexpr size_t added_bytes = 256u;

    ClExtensionList();

    /* The host's list, read no further than `bytes` nor past host_bytes - 1
     * letters, its words kept one space apart. */
    void   hold(const char *host, size_t bytes);
    /* `name` gone from the list, as a whole word. */
    void   strike(const char *name);
    /* `name` at the end of the list unless it is there already, or there is
     * no room for it. */
    void   add(const char *name);
    /* Whether `name` is a whole word of the list. */
    bool   names(const char *name) const;
    /* clGetDeviceInfo's answer from the list: CL_INVALID_VALUE, nothing
     * written, for room it does not fit; the bytes it needs, terminator
     * included, in `value_bytes_ret` either way. */
    cl_int answer(size_t value_bytes, void *value, size_t *value_bytes_ret) const;

    bool invariants_hold() const;

private:
    friend struct ClExtensionListProbe;

    /* Whether the `length` letters at `word` are the whole of `name`. */
    bool same(const char *word, size_t length, const char *name) const;

    char   text_[host_bytes + added_bytes];
    size_t length_;
};

}  // namespace av

#endif  // AV_CL_EXTENSION_LIST_HPP
