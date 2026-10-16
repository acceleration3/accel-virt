#include "executor/opengl/opengl_backend.hpp"

#include <unistd.h>
#include "executor/sharing/shared_image_shape.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/source_memory.hpp"
#include "executor/opengl/gl_context_attributes.hpp"
#include "executor/source_memory.hpp"
#include "proto/generated/opengl/gl_command_names.h"
#include "proto/generated/opengl/gl_dispatch_server.h"
#include "proto/submission/published_slot_layout.hpp"

#include <stdio.h>
#include <string.h>

namespace av {

namespace {

constexpr uint32_t gl_context_profile_mask = 0x9126u, gl_context_core_profile_bit = 0x1u;
/* GL's error flags are one per kind: past this many, what is left is a lost
 * context answering GL_CONTEXT_LOST to every look. */
constexpr uint32_t error_flags = 8u;
constexpr uint32_t gl_context_lost = 0x0507u;
constexpr uint32_t gl_sync_flush_commands_bit = 0x1u;
constexpr uint32_t gl_already_signaled = 0x911Au, gl_condition_satisfied = 0x911Cu;
constexpr uint32_t gl_out_of_memory = 0x0505u;

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

void *forget_sync(void *user, uint64_t token)
{
    AV_SCOPE("forget_sync");
    return static_cast<OpenGlBackend *>(user)->forget_fence(token);
}

uint8_t *blob_at(void *user, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("blob_at");
    return static_cast<OpenGlBackend *>(user)->guest_bytes(offset, bytes);
}

bool pack_blob(void *user, uint64_t offset, uint64_t bytes, void **packed)
{
    AV_SCOPE("pack_blob");
    return packed != nullptr && static_cast<OpenGlBackend *>(user)->pack(offset, bytes, *packed);
}

void unpack_blob(void *user)
{
    AV_SCOPE("unpack_blob");
    static_cast<OpenGlBackend *>(user)->unpack();
}

}  // namespace

OpenGlBackend::OpenGlBackend() : OpenGlBackend(heap_)
{
    AV_SCOPE("OpenGlBackend::OpenGlBackend");
}

/* heap_ is constructed before blocks are bound to it, whichever constructor
 * runs: it is declared first. */
OpenGlBackend::OpenGlBackend(BlockSource &blocks)
    : heap_(), context_(), gl_(), import_(), buffers_(), pack_(gl_, buffers_), fences_(blocks), region_(), errors_(),
      resolved_(0u), ran_(0u),
      lost_(false), current_(false), sink_(stderr), lines_(sink_, "virtio-gpu-av: opengl: ")
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
    install_hooks();
    gl_.av_fixed_function = granted_fixed_function();
    import_.configure(context_);
    buffers_.configure(context_, import_);
    /* Opened is served. A driver that resolves fewer entry points than the
     * table has is not refused here: the decoder refuses each command whose
     * entry point is null, one command at a time, which is the degradation a
     * guest can live with rather than a context it cannot have at all. */
    return true;
}

void OpenGlBackend::install_hooks()
{
    AV_SCOPE("OpenGlBackend::install_hooks");
    gl_.av_bind_sync = bind_sync;
    gl_.av_sync_for = sync_for;
    gl_.av_forget_sync = forget_sync;
    gl_.av_sync_user = this;
    gl_.av_blob = blob_at;
    gl_.av_pack_blob = pack_blob;
    gl_.av_unpack_blob = unpack_blob;
    gl_.av_blob_user = this;
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
    /* What this context imported, and every GLsync it made, is deleted while
     * the context is still open: after the close there is nothing left to
     * delete it with, and a sync of a context sharing objects with another
     * would outlive this one. */
    pack_.release();
    pack_.abandon();
    buffers_.give_back();
    import_.give_back();
    void *fence = nullptr;
    while (fences_.take_any(fence))
        if (gl_.glDeleteSync != nullptr) gl_.glDeleteSync(static_cast<GLsync>(fence));
    context_.close();
    gl_ = glproto::RealGl{};
    resolved_ = 0u;
    ran_ = 0u;
    lost_ = false;
}

bool OpenGlBackend::knows(uint32_t command_id) const
{
    AV_SCOPE("OpenGlBackend::knows");
    return glproto::knows_command(command_id);
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
        lines_.name("the context could not be made current for", glproto::cmd_id_to_string(command_id));
        return false;
    }
    current_ = true;
    if (!glproto::dispatch_command(gl_, command_id, payload, reply, arena)) {
        LogLine(lines_).text(glproto::cmd_id_to_string(command_id)).text(" could not run").write();
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

void OpenGlBackend::bind_signal(void (*)(void *context), void *)
{
    AV_SCOPE("OpenGlBackend::bind_signal");
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
        /* A lost context is the device lost, as Vulkan's and OpenCL's are:
         * the session faults and the guest's context learns it from that. */
        if (error == gl_context_lost) lost_ = true;
        errors_.record(error);
    }
    /* A region without the word has nobody reading it. */
    const PublishedSlotLayout tail(region_.length());
    if (tail.holds_tail()) static_cast<void>(region_.publish(tail.error_word(), errors_.word()));
}

bool OpenGlBackend::lose_device()
{
    AV_SCOPE("OpenGlBackend::lose_device");
    return false;
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
 * this context can share while it is held. One handed over as a dma-buf is
 * mapped here, and is also what a pixel transfer packs into on the GPU
 * (GlPackBuffer). */
/* The context's arena is its first import; every one after is pages the
 * application lent (GL_AMD_pinned_memory, docs/host-memory.md, "Application
 * memory"), which become a memory object of the context: its name is what
 * the guest gives glBufferStorageMemEXT. */
bool OpenGlBackend::adopt(const SourceMemory &memory, uint64_t &handle)
{
    AV_SCOPE("OpenGlBackend::adopt");
    if (region_.memory() != nullptr) return lend(memory, handle);
    if (!region_.adopt(memory, handle)) return false;
    pack_.adopt(memory.share(), memory.size());
    return true;
}

bool OpenGlBackend::lend(const SourceMemory &memory, uint64_t &handle)
{
    AV_SCOPE("OpenGlBackend::lend");
    handle = 0u;
    const int fd = memory.share();
    if (fd < 0) return false;
    uint32_t     name = 0u;
    const bool   imported = import_.imports_memory() && import_.import_memory(fd, memory.size(), name);
    close(fd);
    if (!imported) {
        LogLine(lines_).text("no memory object over ").number(memory.size()).text(" lent bytes").write();
        return false;
    }
    handle = name;
    return true;
}

bool OpenGlBackend::pack(uint64_t offset, uint64_t bytes, void *&packed)
{
    AV_SCOPE("OpenGlBackend::pack");
    return pack_.bind(offset, bytes, packed);
}

void OpenGlBackend::unpack()
{
    AV_SCOPE("OpenGlBackend::unpack");
    pack_.unbind();
}

void OpenGlBackend::abandon(const SourceMemory &memory, uint64_t handle)
{
    AV_SCOPE("OpenGlBackend::abandon");
    if (!region_.is(memory)) {
        import_.delete_memory(static_cast<uint32_t>(handle));
        return;
    }
    region_.abandon(memory);
    pack_.abandon();
}

bool OpenGlBackend::bind_fence(uint64_t token, void *fence)
{
    AV_SCOPE("OpenGlBackend::bind_fence");
    if (fences_.crowded()) retire_passed_fence();
    const Status bound = fences_.bind(token, fence);
    if (bound == Status::ok) return true;
    if (fence == nullptr) {
        LogLine(lines_).text("glFenceSync made no GLsync for token ").number(token).write();
        return false;
    }
    /* A sync no token names is one nothing would ever delete: a token reused
     * or out of order, or one with no room to be held, gets none rather than
     * the host keeping one for good. The guest is told of the last as GL is. */
    if (bound == Status::no_memory) errors_.record(gl_out_of_memory);
    if (gl_.glDeleteSync != nullptr) gl_.glDeleteSync(static_cast<GLsync>(fence));
    LogLine(lines_).text("the GLsync of token ").number(token).text(" was deleted as it was made").write();
    return false;
}

/* Fences on one context signal in the order they were made, so the oldest is
 * the first the GPU passes. Asked without waiting: one the GPU has not yet
 * passed stays, and the table grows instead. */
void OpenGlBackend::retire_passed_fence()
{
    AV_SCOPE("OpenGlBackend::retire_passed_fence");
    if (gl_.glClientWaitSync == nullptr) return;
    const uint32_t reached =
        gl_.glClientWaitSync(static_cast<GLsync>(fences_.oldest()), gl_sync_flush_commands_bit, 0u);
    if (reached != gl_already_signaled && reached != gl_condition_satisfied) return;
    void *deleted = nullptr;
    if (fences_.retire_oldest(deleted) != Status::ok) return;
    if (deleted != nullptr && gl_.glDeleteSync != nullptr) gl_.glDeleteSync(static_cast<GLsync>(deleted));
}

void *OpenGlBackend::fence_for(uint64_t token) const
{
    AV_SCOPE("OpenGlBackend::fence_for");
    return fences_.fence_for(token);
}

void *OpenGlBackend::forget_fence(uint64_t token)
{
    AV_SCOPE("OpenGlBackend::forget_fence");
    return fences_.forget(token);
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
                                  const SharedImageShape &shape,
                                  uint64_t &wire_handle, uint64_t &detail)
{
    AV_SCOPE("OpenGlBackend::import_object");
    static_cast<void>(shape);   /* memory alone: no image of this API is made over it */
    wire_handle = 0u;
    detail = 0u;
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
    wire_handle = name;
    return true;
}

/* The GL name is the handle: memory objects and semaphores are two name
 * spaces, and `kind` says which, as the import recorded it. */
void OpenGlBackend::release_object(uint32_t kind, uint64_t wire_handle, uint64_t)
{
    AV_SCOPE("OpenGlBackend::release_object");
    destroy_made(kind, static_cast<uint32_t>(wire_handle));
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
