#include "device/executor/opengl_backend.hpp"

#include "core/trace/function_scope.hpp"
#include "device/executor/gl_context_attributes.hpp"
#include "device/executor/imported_blob.hpp"
#include "proto/generated/opengl/gl_command_names.h"
#include "proto/generated/opengl/gl_dispatch_server.h"

#include <string.h>

namespace av {

namespace {

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
    : context_(), gl_(), fences_(), region_(), resolved_(0u), ran_(0u), lost_(false)
{
    AV_SCOPE("OpenGlBackend::OpenGlBackend");
}

bool OpenGlBackend::initialize(const GlDisplay *display, const RvkContextOptions *options, const OpenGlBackend *share)
{
    AV_SCOPE("OpenGlBackend::initialize");
    if (display == nullptr ||
        !context_.open(*display, GlContextAttributes(options), share != nullptr ? &share->context_ : nullptr))
        return false;
    resolved_ = static_cast<uint32_t>(load_real_gl(gl_, resolve_through, &context_));
    gl_.rvk_bind_sync = bind_sync;
    gl_.rvk_sync_for = sync_for;
    gl_.rvk_forget_sync = forget_sync;
    gl_.rvk_sync_user = this;
    gl_.rvk_blob = blob_at;
    gl_.rvk_blob_user = this;
    /* Opened is served. A driver that resolves fewer entry points than the
     * table has is not refused here: the decoder refuses each command whose
     * entry point is null, one command at a time, which is the degradation a
     * guest can live with rather than a context it cannot have at all. */
    return true;
}

void OpenGlBackend::shut_down()
{
    AV_SCOPE("OpenGlBackend::shut_down");
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
    return strcmp(glproto::cmd_id_to_string(command_id), "unknown") != 0;
}

bool OpenGlBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                        WireArena &arena)
{
    AV_SCOPE("OpenGlBackend::run");
    /* One context belongs to one worker, but a worker runs many contexts in
     * turn, so the current one is set per command rather than once. */
    if (!context_.make_current()) return false;
    if (!glproto::dispatch_command(gl_, command_id, payload, reply, arena)) return false;
    ran_++;
    return true;
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
    if (blob.kind() != RVK_BLOB_HOST_POINTER || region_.length() != 0u) return false;
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

}  // namespace av
