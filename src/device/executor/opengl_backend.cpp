#include "device/executor/opengl_backend.hpp"

#include "core/trace/function_scope.hpp"
#include "device/executor/gl_context_attributes.hpp"
#include "device/executor/imported_blob.hpp"
#include "proto/generated/opengl/gl_command_names.h"
#include "proto/generated/opengl/gl_dispatch_server.h"

#include <stdio.h>
#include <string.h>

namespace av {

namespace {

constexpr uint32_t gl_context_profile_mask = 0x9126u, gl_context_core_profile_bit = 0x1u;
/* GL's error flags are one per kind: past this many, what is left is a lost
 * context answering GL_CONTEXT_LOST to every look. */
constexpr uint32_t error_flags = 8u;

/* The generated table's loader signature: a name and whatever the caller wants
 * to reach, which here is the context the entry points come from. */
void *resolve_through(const char *name, void *user)
{
    AV_SCOPE("resolve_through");
    return static_cast<GlContext *>(user)->resolve(name);
}

/* The three sync hooks and the blob resolver, as the generated decoder calls
 * them: through the table, with the backend as the user pointer, because a
 * parameterless hook could not reach one context's state. */
void bind_sync(void *user, uint64_t token, void *fence)
{
    AV_SCOPE("bind_sync");
    (void)static_cast<OpenGlBackend *>(user)->bind_fence(token, fence);
}

void *sync_for(void *user, uint64_t token)
{
    AV_SCOPE("sync_for");
    return static_cast<OpenGlBackend *>(user)->fence_for(token);
}

void forget_sync(void *user, uint64_t token)
{
    AV_SCOPE("forget_sync");
    static_cast<OpenGlBackend *>(user)->forget_fence(token);
}

uint8_t *blob_at(void *user, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("blob_at");
    return static_cast<OpenGlBackend *>(user)->guest_bytes(offset, bytes);
}

}  // namespace

OpenGlBackend::OpenGlBackend()
    : context_(), gl_(), import_(), objects_(), fences_(), region_(), errors_(), resolved_(0u), ran_(0u),
      lost_(false), current_(false)
{
    AV_SCOPE("OpenGlBackend::OpenGlBackend");
}

const GlContext &OpenGlBackend::context() const
{
    AV_SCOPE("OpenGlBackend::context");
    return context_;
}

bool OpenGlBackend::initialize(const GlDisplay *display, const AvContextOptions *options, const OpenGlBackend *share)
{
    AV_SCOPE("OpenGlBackend::initialize");
    if (display == nullptr ||
        !context_.open(*display, GlContextAttributes(options), share != nullptr ? &share->context_ : nullptr))
        return false;
    resolved_ = static_cast<uint32_t>(load_real_gl(gl_, resolve_through, &context_));
    gl_.av_bind_sync = bind_sync;
    gl_.av_sync_for = sync_for;
    gl_.av_forget_sync = forget_sync;
    gl_.av_sync_user = this;
    gl_.av_blob = blob_at;
    gl_.av_blob_user = this;
    gl_.av_fixed_function = granted_fixed_function();
    import_.configure(context_);
    /* Opened is served. A driver that resolves fewer entry points than the
     * table has is not refused here: the decoder refuses each command whose
     * entry point is null, one command at a time, which is the degradation a
     * guest can live with rather than a context it cannot have at all. */
    return true;
}

/* A context from before profiles has no mask to ask for, and the asking is
 * an error: that context has the fixed-function arrays. */
bool OpenGlBackend::granted_fixed_function()
{
    AV_SCOPE("OpenGlBackend::granted_fixed_function");
    if (gl_.glGetIntegerv == nullptr || gl_.glGetError == nullptr) return true;
    GLint mask = 0;
    gl_.glGetIntegerv(gl_context_profile_mask, &mask);
    for (uint32_t flag = 0u; flag < error_flags && gl_.glGetError() != 0u; flag++) {
    }
    return (static_cast<uint32_t>(mask) & gl_context_core_profile_bit) == 0u;
}

void OpenGlBackend::shut_down()
{
    AV_SCOPE("OpenGlBackend::shut_down");
    /* What this context imported is deleted while the context is still open:
     * after the close there is nothing left to delete it with. */
    uint32_t kind = 0u;
    uint32_t name = 0u;
    while (objects_.take_any(kind, name)) destroy_made(kind, name);
    import_.give_back();
    fences_.forget_all();
    region_.give_back();
    context_.close();
    gl_ = glproto::RealGl{};
    resolved_ = 0u;
    ran_ = 0u;
    lost_ = false;
}

bool OpenGlBackend::knows(uint32_t command_id) const
{
    AV_SCOPE("OpenGlBackend::knows");
    if (command_id >> 16u != opengl_api) return false;
    /* Every id the table has not got answers the one literal id 0 does. */
    return glproto::cmd_id_to_string(command_id) != glproto::cmd_id_to_string(0u);
}

bool OpenGlBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                        WireArena &arena)
{
    AV_SCOPE("OpenGlBackend::run");
    /* One context belongs to one worker, but a worker runs many contexts in
     * turn, a batch at a time: the current one is set at a batch's first
     * command. Not per command - libglvnd's eglMakeCurrent, and even its
     * eglGetCurrentContext, is a getpid() system call each time. */
    /* Named here, because all the guest learns is that its batch could not
     * be decoded, and the device's fault names only the fence. */
    if (!current_ && !context_.make_current()) {
        fprintf(stderr, "virtio-gpu-av: opengl: the context could not be made current for %s\n",
                glproto::cmd_id_to_string(command_id));
        return false;
    }
    current_ = true;
    if (!glproto::dispatch_command(gl_, command_id, payload, reply, arena)) {
        fprintf(stderr, "virtio-gpu-av: opengl: %s could not run\n", glproto::cmd_id_to_string(command_id));
        return false;
    }
    ran_++;
    return true;
}

bool OpenGlBackend::poll()
{
    AV_SCOPE("OpenGlBackend::poll");
    return false;
}

void OpenGlBackend::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("OpenGlBackend::gpu_progress");
    marked = 0u;
    reached = 0u;
}

/* A batch that never made the context current raised nothing. */
void OpenGlBackend::end_batch()
{
    AV_SCOPE("OpenGlBackend::end_batch");
    if (current_) publish_errors();
    current_ = false;
}

/* glGetError was a round trip, and an application asks after everything:
 * Minecraft clears the flags before most calls, which was two thirds of its
 * render thread's time in the driver. The host is the only side that sees an
 * error and reading one clears it, so the batch's errors are read here and
 * counted into the word the guest reads without asking (docs/protocol.md,
 * "Error word"). It is stored before the device publishes the batch's head,
 * so a guest that sees the batch done sees its errors. */
void OpenGlBackend::publish_errors()
{
    AV_SCOPE("OpenGlBackend::publish_errors");
    if (gl_.glGetError == nullptr) return;
    for (uint32_t flag = 0u; flag < error_flags; flag++) {
        const uint32_t error = gl_.glGetError();
        if (error == 0u) break;
        errors_.record(error);
    }
    if (region_.length() < sizeof(uint64_t)) return;
    uint8_t *const word = region_.at(region_.length() - sizeof(uint64_t), sizeof(uint64_t));
    if (word != nullptr) __atomic_store_n(reinterpret_cast<uint64_t *>(word), errors_.word(), __ATOMIC_RELEASE);
}

bool OpenGlBackend::lost() const
{
    AV_SCOPE("OpenGlBackend::lost");
    return lost_;
}

/* The context's arena: guest memory the device already mapped, which its
 * bulk-data commands read and write in place. GL imports no host pointer on
 * both host drivers, so the pointer is not a GL object - it is the region
 * (research.md §5). Its address names it on the wire, which no other import of
 * this context can share while it is held. */
bool OpenGlBackend::adopt(ImportedBlob &blob)
{
    AV_SCOPE("OpenGlBackend::adopt");
    if (blob.kind() != AV_BLOB_HOST_POINTER || region_.length() != 0u) return false;
    region_.take(static_cast<uint8_t *>(blob.pointer()), blob.size());
    const uint64_t address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(blob.pointer()));
    blob.record(address, address);
    return true;
}

void OpenGlBackend::abandon(ImportedBlob &blob)
{
    AV_SCOPE("OpenGlBackend::abandon");
    if (blob.object() == 0u) return;
    region_.give_back();
    blob.record(0u, 0u);
}

bool     OpenGlBackend::bind_fence(uint64_t token, void *fence)
{
    AV_SCOPE("OpenGlBackend::bind_fence");
    return fences_.bind(token, fence);
}
void    *OpenGlBackend::fence_for(uint64_t token) const
{
    AV_SCOPE("OpenGlBackend::fence_for");
    return fences_.fence_for(token);
}
void     OpenGlBackend::forget_fence(uint64_t token)
{
    AV_SCOPE("OpenGlBackend::forget_fence");
    fences_.forget(token);
}
uint8_t *OpenGlBackend::guest_bytes(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("OpenGlBackend::guest_bytes");
    return region_.at(offset, bytes);
}


uint32_t OpenGlBackend::resolved() const
{
    AV_SCOPE("OpenGlBackend::resolved");
    return resolved_;
}
uint64_t OpenGlBackend::ran() const
{
    AV_SCOPE("OpenGlBackend::ran");
    return ran_;
}

bool OpenGlBackend::export_object(uint32_t, uint64_t, uint32_t &handle_kind, int &fd)
{
    AV_SCOPE("OpenGlBackend::export_object");
    handle_kind = 0u;
    fd = -1;
    return false;
}

bool OpenGlBackend::import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                  uint64_t &wire_handle)
{
    AV_SCOPE("OpenGlBackend::import_object");
    wire_handle = 0u;
    if (handle_kind != AV_SHARED_HANDLE_DMA_BUF || fd < 0) return false;
    uint32_t name = 0u;
    if (kind == AV_SHARED_MEMORY) {
        if (size == 0u || !import_.import_memory(fd, size, name)) return false;
    } else if (kind == AV_SHARED_MONITORED_FENCE || kind == AV_SHARED_NATIVE_FENCE) {
        /* The size is the memory kind's and is judged where the object is
         * published; a semaphore's payload carries its own. */
        if (!import_.import_semaphore(fd, name)) return false;
    } else {
        return false;
    }
    if (objects_.remember(kind, name)) {
        wire_handle = name;
        return true;
    }
    /* A row this context has no room for is an object nothing could name and
     * nothing would free. */
    destroy_made(kind, name);
    return false;
}

void OpenGlBackend::release_object(uint32_t kind, uint64_t wire_handle)
{
    AV_SCOPE("OpenGlBackend::release_object");
    uint32_t name = 0u;
    if (!objects_.forget(kind, wire_handle, name)) return;
    destroy_made(kind, name);
}

void OpenGlBackend::destroy_made(uint32_t kind, uint32_t name)
{
    AV_SCOPE("OpenGlBackend::destroy_made");
    if (kind == AV_SHARED_MEMORY) {
        import_.delete_memory(name);
        return;
    }
    import_.delete_semaphore(name);
}

}  // namespace av
