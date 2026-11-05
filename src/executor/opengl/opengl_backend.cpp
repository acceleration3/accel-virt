#include "executor/opengl/opengl_backend.hpp"

#include "executor/sharing/shared_image_shape.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/source_memory.hpp"
#include "executor/opengl/gl_context_attributes.hpp"
#include "proto/generated/glmem/glmem_command_names.h"
#include "proto/generated/opengl/gl_command_names.h"
#include "proto/generated/opengl/gl_dispatch_server.h"

#include <atomic>
#include <stdio.h>
#include <string.h>

namespace av {

namespace {

constexpr uint32_t gl_context_profile_mask = 0x9126u, gl_context_core_profile_bit = 0x1u;
constexpr uint32_t error_flags = 8u;
constexpr uint32_t gl_context_lost = 0x0507u;
constexpr uint32_t gl_sync_flush_commands_bit = 0x1u;
constexpr uint32_t gl_already_signaled = 0x911Au, gl_condition_satisfied = 0x911Cu;
constexpr uint32_t gl_out_of_memory = 0x0505u;
constexpr uint32_t gl_invalid_value = 0x0501u;
constexpr uint32_t gl_invalid_operation = 0x0502u;

constexpr GLenum gl_device_node_mask = 0x959Au;

template <typename Value>
class GuardedQuery {
public:
    using Query = void (*)(GLenum, Value *);

    static void *wrap(void *found)
    {
        Query expected = nullptr;
        const Query query = reinterpret_cast<Query>(found);
        if (!real_.compare_exchange_strong(expected, query) && expected != query) return found;
        return reinterpret_cast<void *>(&guarded);
    }

private:
    static void guarded(GLenum pname, Value *data)
    {
        if (pname == gl_device_node_mask) {
            if (data != nullptr) *data = static_cast<Value>(1);
            return;
        }
        real_.load(std::memory_order_relaxed)(pname, data);
    }

    static inline std::atomic<Query> real_{nullptr};
};

void *guard_query(const char *name, void *found)
{
    if (found == nullptr) return nullptr;
    if (strcmp(name, "glGetIntegerv") == 0) return GuardedQuery<GLint>::wrap(found);
    if (strcmp(name, "glGetInteger64v") == 0) return GuardedQuery<GLint64>::wrap(found);
    if (strcmp(name, "glGetBooleanv") == 0) return GuardedQuery<GLboolean>::wrap(found);
    if (strcmp(name, "glGetFloatv") == 0) return GuardedQuery<GLfloat>::wrap(found);
    if (strcmp(name, "glGetDoublev") == 0) return GuardedQuery<GLdouble>::wrap(found);
    return found;
}

void *resolve_through(const char *name, void *user)
{
    AV_SCOPE("resolve_through");
    return guard_query(name, static_cast<GlContext *>(user)->resolve(name));
}

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

}

OpenGlBackend::OpenGlBackend(DriverLog &lines) : OpenGlBackend(heap_, lines)
{
    AV_SCOPE("OpenGlBackend::OpenGlBackend");
}

OpenGlBackend::OpenGlBackend(BlockSource &blocks, DriverLog &lines)
    : glmemproto::ServerDispatch(guest_), heap_(), context_(), gl_(), import_(), buffers_(), natives_(), wake_(), pack_(gl_, buffers_), fences_(blocks), progress_(gl_, natives_, wake_), memories_(blocks, progress_), guest_(), region_(guest_), published_(guest_), errors_(),
      resolved_(0u), ran_(0u),
      lost_(false), current_(false), lines_(lines)
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
    natives_.configure(context_);
    buffers_.configure(context_, import_);
    return true;
}

void OpenGlBackend::install_hooks()
{
    AV_SCOPE("OpenGlBackend::install_hooks");
    gl_.av_bind_sync = bind_sync;
    gl_.av_sync_for = sync_for;
    gl_.av_forget_sync = forget_sync;
    gl_.av_sync_user = this;
    gl_.av_guest = &guest_;
    gl_.av_pack_blob = pack_blob;
    gl_.av_unpack_blob = unpack_blob;
    gl_.av_blob_user = this;
}

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
    if (context_.make_current()) {
        pack_.release();
        pack_.abandon();
        buffers_.give_back();
        memories_.destroy_all(import_);
        import_.give_back();
        progress_.release();
        natives_.give_back();
        void *fence = nullptr;
        while (fences_.take_any(fence))
            if (gl_.glDeleteSync != nullptr) gl_.glDeleteSync(static_cast<GLsync>(fence));
    }
    wake_.unbind();
    context_.close();
    gl_ = glproto::RealGl{};
    resolved_ = 0u;
    ran_ = 0u;
    lost_ = false;
}

bool OpenGlBackend::knows(uint32_t command_id) const
{
    AV_SCOPE("OpenGlBackend::knows");
    return glproto::knows_command(command_id) || glmemproto::knows_command(command_id);
}

bool OpenGlBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                        WireArena &arena)
{
    AV_SCOPE("OpenGlBackend::run");
    if (!context_.make_current()) {
        lines_.name("the context could not be made current for", glproto::cmd_id_to_string(command_id));
        return false;
    }
    current_ = true;
    if (glmemproto::knows_command(command_id)) {
        if (glmemproto::dispatch_command(*this, command_id, payload, reply, arena)) return true;
        LogLine(lines_).text(glmemproto::command_name(command_id)).text(" could not run").write();
        return false;
    }
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
    if (!progress_.pending()) {
        wake_.settled();
        return false;
    }
    if (!context_.make_current()) return false;
    static_cast<void>(progress_.settle());
    memories_.reclaim(import_);
    const bool unwatched = progress_.polled();
    if (unwatched) wake_.retry_later();
    else wake_.settled();
    return unwatched;
}

void OpenGlBackend::bind_signal(void (*signal)(void *context), void *context)
{
    AV_SCOPE("OpenGlBackend::bind_signal");
    wake_.bind(signal, context);
}

void OpenGlBackend::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("OpenGlBackend::gpu_progress");
    marked = progress_.marked();
    reached = progress_.reached();
}

void OpenGlBackend::end_batch()
{
    AV_SCOPE("OpenGlBackend::end_batch");
    if (current_ && context_.make_current()) {
        publish_errors();
        progress_.fence();
        memories_.reclaim(import_);
    }
    current_ = false;
}

void OpenGlBackend::publish_errors()
{
    AV_SCOPE("OpenGlBackend::publish_errors");
    if (gl_.glGetError == nullptr) return;
    for (uint32_t flag = 0u; flag < error_flags; flag++) {
        const uint32_t error = gl_.glGetError();
        if (error == 0u) break;
        if (error == gl_context_lost) lost_ = true;
        errors_.record(error);
    }
    static_cast<void>(published_.publish_error_word(errors_.word()));
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

bool OpenGlBackend::adopt(SourceMemory &memory, uint64_t &handle)
{
    AV_SCOPE("OpenGlBackend::adopt");
    if (region_.held()) return lend(memory, handle);
    if (!region_.adopt(memory, handle)) return false;
    OwnedFd packed;
    static_cast<void>(memory.share(packed));
    pack_.adopt(packed, memory.size());
    return true;
}

bool OpenGlBackend::lend(const SourceMemory &memory, uint64_t &handle)
{
    AV_SCOPE("OpenGlBackend::lend");
    handle = 0u;
    OwnedFd lent;
    if (!memory.share(lent)) return false;
    uint32_t   name = 0u;
    const bool imported = import_.imports_memory() && import_.import_memory(lent.fd(), memory.size(), name);
    if (!imported) {
        LogLine(lines_).text("no memory object over ").number(memory.size()).text(" lent bytes").write();
        return false;
    }
    if (memories_.add(name, memory.size()) != Status::ok) {
        import_.delete_memory(name);
        LogLine(lines_).text("no room to track the memory object over ").number(memory.size()).text(" lent bytes").write();
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
        memories_.retire(static_cast<uint32_t>(handle));
        memories_.reclaim(import_);
        return;
    }
    region_.abandon(memory);
    pack_.abandon();
}

bool OpenGlBackend::bind_fence(uint64_t token, void *fence)
{
    AV_SCOPE("OpenGlBackend::bind_fence");
    if (!context_.make_current()) return false;
    if (fences_.crowded()) retire_passed_fence();
    const Status bound = fences_.bind(token, fence);
    if (bound == Status::ok) return true;
    if (fence == nullptr) {
        LogLine(lines_).text("glFenceSync made no GLsync for token ").number(token).write();
        return false;
    }
    if (bound == Status::no_memory) errors_.record(gl_out_of_memory);
    if (gl_.glDeleteSync != nullptr) gl_.glDeleteSync(static_cast<GLsync>(fence));
    LogLine(lines_).text("the GLsync of token ").number(token).text(" was deleted as it was made").write();
    return false;
}

void OpenGlBackend::retire_passed_fence()
{
    AV_SCOPE("OpenGlBackend::retire_passed_fence");
    if (gl_.glClientWaitSync == nullptr || !context_.make_current()) return;
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

bool OpenGlBackend::export_object(uint32_t, uint64_t, uint32_t &handle_kind, OwnedFd &)
{
    AV_SCOPE("OpenGlBackend::export_object");
    handle_kind = 0u;
    return false;
}

bool OpenGlBackend::import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                  const SharedImageShape &shape, uint64_t into,
                                  uint64_t &wire_handle, uint64_t &detail)
{
    AV_SCOPE("OpenGlBackend::import_object");
    static_cast<void>(shape);
    wire_handle = 0u;
    detail = 0u;
    if (fd < 0 || into != 0u) return false;
    uint32_t name = 0u;
    if (kind == AV_SHARED_MEMORY) {
        if (handle_kind != AV_SHARED_HANDLE_DMA_BUF || size == 0u || !import_.import_memory(fd, size, name))
            return false;
        if (memories_.add(name, size) != Status::ok) {
            import_.delete_memory(name);
            return false;
        }
    } else if (kind == AV_SHARED_MONITORED_FENCE || kind == AV_SHARED_NATIVE_FENCE) {
        if (handle_kind != AV_SHARED_HANDLE_OPAQUE_FD || !import_.import_semaphore(fd, name)) return false;
    } else {
        return false;
    }
    wire_handle = name;
    return true;
}

void OpenGlBackend::release_object(uint32_t kind, uint64_t wire_handle, uint64_t)
{
    AV_SCOPE("OpenGlBackend::release_object");
    destroy_made(kind, static_cast<uint32_t>(wire_handle));
}

bool OpenGlBackend::on_QueryBufferSlicesGL(uint32_t &supported)
{
    AV_SCOPE("OpenGlBackend::on_QueryBufferSlicesGL");
    supported = import_.places_buffers() && gl_.glNamedBufferStorageMemEXT != nullptr && progress_.fences() ? 1u : 0u;
    return true;
}

bool OpenGlBackend::on_PlaceBufferSlice(uint32_t buffer, uint64_t memory, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("OpenGlBackend::on_PlaceBufferSlice");
    if (!import_.places_buffers() || gl_.glNamedBufferStorageMemEXT == nullptr || memory > UINT32_MAX) {
        errors_.record(gl_invalid_operation);
        return true;
    }
    const Status placed = memories_.place(buffer, static_cast<uint32_t>(memory), offset, bytes);
    if (placed != Status::ok) {
        LogLine(lines_).text("a buffer slice was refused, status ").number(static_cast<uint32_t>(placed)).write();
        errors_.record(placed == Status::no_memory ? gl_out_of_memory : gl_invalid_value);
        return true;
    }
    gl_.glNamedBufferStorageMemEXT(buffer, static_cast<GLsizeiptr>(bytes), static_cast<GLuint>(memory), offset);
    return true;
}

bool OpenGlBackend::on_ForgetBufferSlice(uint32_t buffer)
{
    AV_SCOPE("OpenGlBackend::on_ForgetBufferSlice");
    memories_.leave(buffer);
    return true;
}

void OpenGlBackend::destroy_made(uint32_t kind, uint32_t name)
{
    AV_SCOPE("OpenGlBackend::destroy_made");
    if (kind == AV_SHARED_MEMORY) {
        memories_.retire(name);
        memories_.reclaim(import_);
        return;
    }
    import_.delete_semaphore(name);
}

}
