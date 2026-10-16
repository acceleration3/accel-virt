/* One guest context's OpenGL, run on the host's own.
 *
 * Not the same shape as the other two. The generator gives Vulkan and OpenCL an
 * abstract dispatch to derive from; OpenGL gets a table of 1375 function
 * pointers and a decoder that reads it, so this owns the table, the context it
 * was resolved against, the fences a guest's tokens name and the region its
 * bulk-data commands read from. */
#ifndef AV_OPENGL_BACKEND_HPP
#define AV_OPENGL_BACKEND_HPP

#include "core/log/driver_log.hpp"
#include "device/abi/av_executor.h"
#include "executor/guest_region.hpp"
#include "executor/opengl/gl_error_tally.hpp"
#include "executor/session_backend.hpp"
#include "executor/opengl/sync_tokens.hpp"
#include "device/host/stdio_log.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/opengl/gl_pack_buffer.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"

#include <stdint.h>

namespace av {

class SharedImageShape;

class GlDisplay;

class OpenGlBackend final : public SessionBackend {
public:
    OpenGlBackend();

    /* Opens a context on the display the host keeps for this context's GPU,
     * which must outlive it, made with `options` (null: the defaults) and
     * sharing `share`'s objects (null: none), and fills the table from it.
     * False for a null display - a GPU the host serves no OpenGL on - options
     * no OpenGL has, a share with no context on this display, or a context the
     * driver will not make. */
    [[nodiscard]] bool initialize(const GlDisplay *display, const AvContextOptions *options,
                                  const OpenGlBackend *share);
    /* The context whose objects another shares: an OpenGL context's share
     * lists, or an OpenCL context's vehicle under cl_khr_gl_sharing. */
    const GlContext   &context() const;
    void               shut_down() override;

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    /* OpenGL has no Vulkan device underneath to lose: a lost context is the host GL's own, GL_CONTEXT_LOST. */
    [[nodiscard]] bool lose_device() override;
    void end_batch() override;
    bool poll() override;
    /* Its commands finish as they run: nothing to wake the worker for. */
    void bind_signal(void (*signal)(void *context), void *context) override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;
    [[nodiscard]] bool adopt(ImportedBlob &blob) override;
    void abandon(ImportedBlob &blob) override;

    /* This API exports no descriptor (feature 053,
     * contracts/wire_additions.md): nothing in OpenGL hands one out, so a
     * guest sharing through this API is refused by the device with a status
     * rather than handed an object nothing made. */
    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                     int &fd) override;
    /* It does import one, as a memory object or a semaphore of this context,
     * answered as the GL name the guest names it by from then on. Only a
     * dma-buf: what Vulkan exports as an opaque fd is a handle private to that
     * driver, which glImportMemoryFdEXT would read as one of its own. A host
     * without the matching GL_EXT_*_fd extension imports nothing. */
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     const SharedImageShape &shape,
                                     uint64_t &wire_handle, uint64_t &detail) override;
    /* What an import made, destroyed; nothing for a name this context did not
     * import, which is what stops a guest deleting an object of another's. */
    void               release_object(uint32_t kind, uint64_t wire_handle, uint64_t detail) override;

    /* How many of the table's entry points this host actually has. A driver
     * short of some is not a failure: the decoder refuses each command whose
     * entry point is null, one command at a time. */
    /* What the generated table's hooks call, which is why they are fields on
     * it: a parameterless hook could not reach one context's state. */
    [[nodiscard]] bool bind_fence(uint64_t token, void *fence);
    void              *fence_for(uint64_t token) const;
    /* What glDeleteSync deletes for `token` (SyncTokens::forget). */
    void              *forget_fence(uint64_t token);
    /* The bytes of the imported region its bulk-data commands name. */
    uint8_t           *guest_bytes(uint64_t offset, uint64_t bytes) const;
    /* A pixel transfer into the region packed into it by the GPU
     * (GlPackBuffer): the offset it names instead of a pointer, and once made,
     * the write finished. */
    [[nodiscard]] bool pack(uint64_t offset, uint64_t bytes, void *&packed);
    void               unpack();

    uint32_t resolved() const;
    uint64_t ran() const;

private:
    /* The table's hooks into this backend: its fences and its region. */
    void install_hooks();
    /* Room in a full table of fences, made once the GPU passes its oldest. */
    void retire_oldest_fence();
    /* The entry point a kind is destroyed through. */
    void destroy_made(uint32_t kind, uint32_t name);
    /* Whether the context the host made has the fixed-function vertex arrays
     * a draw's check asks after - a core context has none - asked once, with
     * whatever error the asking left cleared before the guest can read it. */
    bool granted_fixed_function();
    /* What the batch raised, drained from the context while it is current
     * and published as the error word at the end of the imported region. */
    void publish_errors();

    GlContext   context_;
    glproto::RealGl gl_;
    GlHostImport    import_;
    GlPackBuffer    pack_;
    SyncTokens  fences_;
    GuestRegion region_;
    GlErrorTally errors_;
    uint32_t    resolved_;
    uint64_t    ran_;
    bool        lost_;
    /* Whether this batch has made the context current yet. */
    bool        current_;
    /* QEMU's stderr, where the host log is read when a guest's batch fails. */
    StdioLog    sink_;
    DriverLog   lines_;

    friend struct OpenGlBackendProbe;
};

}  // namespace av

#endif  // AV_OPENGL_BACKEND_HPP
