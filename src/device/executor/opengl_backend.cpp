#include "device/executor/opengl_backend.hpp"

#include "core/trace/function_scope.hpp"
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

bool OpenGlBackend::initialize(const GlDisplay *display)
{
    AV_SCOPE("OpenGlBackend::initialize");
    if (display == nullptr || !context_.open(*display)) return false;
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

bool OpenGlBackend::adopt(ImportedBlob &)
{
    AV_SCOPE("OpenGlBackend::adopt");
    /* GL_EXT_memory_object_fd takes a dma-buf on Mesa and refuses one on
     * NVIDIA, and the region a command reads from is not an imported object
     * anyway - it is a mapping the device already made. Nothing to adopt yet;
     * import_kinds says so for OpenGL. */
    return false;
}

void OpenGlBackend::abandon(ImportedBlob &)
{
    AV_SCOPE("OpenGlBackend::abandon");
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

void OpenGlBackend::take_region(uint8_t *base, uint64_t length)
{
    AV_SCOPE("OpenGlBackend::take_region");
    region_.take(base, length);
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
