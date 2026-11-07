#include "executor/opengl/opengl_backend.hpp"

#include "executor/vram_ledger.hpp"

#include "executor/opengl/gl_memory_group.hpp"

#include "executor/teardown_budget.hpp"

#include "executor/sharing/shared_image_shape.hpp"

#include "core/log/log_line.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/submission/blob_role.hpp"
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
constexpr uint32_t gl_sync_flush_commands_bit = 0x1u;
constexpr uint32_t gl_sync_gpu_commands_complete = 0x9117u;
constexpr uint32_t gl_already_signaled = 0x911Au, gl_condition_satisfied = 0x911Cu;
constexpr uint32_t gl_out_of_memory = 0x0505u;

constexpr GLenum gl_device_node_mask = 0x959Au;

template <typename Value>
class GuardedQuery {
public:
    using Query = void (*)(GLenum, Value *);

    static void *wrap(void *found)
    {
        AV_SCOPE("GuardedQuery::wrap");
        Query expected = nullptr;
        const Query query = reinterpret_cast<Query>(found);
        if (!real_.compare_exchange_strong(expected, query) && expected != query) return found;
        return reinterpret_cast<void *>(&guarded);
    }

private:
    static void guarded(GLenum pname, Value *data)
    {
        AV_SCOPE("GuardedQuery::guarded");
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
    AV_SCOPE("guard_query");
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

void record_invalid(void *user, uint32_t error)
{
    AV_SCOPE("record_invalid");
    static_cast<OpenGlBackend *>(user)->record_error(error);
}

}

OpenGlBackend::OpenGlBackend(DriverLog &lines) : OpenGlBackend(heap_, lines)
{
    AV_SCOPE("OpenGlBackend::OpenGlBackend");
}

OpenGlBackend::OpenGlBackend(BlockSource &blocks, DriverLog &lines)
    : heap_(), context_(), gl_(), import_(), buffers_(), natives_(), wake_(), pack_(gl_, buffers_), fences_(blocks), progress_(gl_, natives_, wake_), pending_(gl_, errors_), vram_guard_(blocks, gl_, pending_), memories_(blocks, progress_), images_(blocks), slices_(memories_, gl_, import_, progress_, pending_, guest_, lines),
      textures_(blocks, gl_, memories_, images_, pending_), guest_(), region_(guest_, lines), published_(guest_), errors_(),
      resolved_(0u), ran_(0u), submitted_(0u), fenced_at_(0u), drained_at_(0u), drain_fence_(nullptr),
      current_(false), poll_failure_logged_(false), lines_(lines)
{
    AV_SCOPE("OpenGlBackend::OpenGlBackend");
    import_.attach(images_);
}

const GlContext &OpenGlBackend::context() const
{
    AV_SCOPE("OpenGlBackend::context");
    return context_;
}

void OpenGlBackend::use_vram(VramLedger *ledger)
{
    AV_SCOPE("OpenGlBackend::use_vram");
    vram_guard_.bind(ledger);
}

bool OpenGlBackend::initialize(const GlDisplay *display, const AvContextOptions *options, const OpenGlBackend *share,
                               GlMemoryGroup *group)
{
    AV_SCOPE("OpenGlBackend::initialize");
    if (display == nullptr ||
        !context_.open(*display, GlContextAttributes(options), share != nullptr ? &share->context_ : nullptr))
        return false;
    if (group != nullptr && !memories_.join(*group, &import_)) {
        context_.close();
        return false;
    }
    resolved_ = static_cast<uint32_t>(load_real_gl(gl_, resolve_through, &context_));
    install_hooks();
    gl_.av_fixed_function = granted_fixed_function();
    import_.configure(context_);
    images_.configure(context_);
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
    gl_.av_record_error = record_invalid;
    gl_.av_error_user = this;
    slices_.install();
    textures_.install();
    vram_guard_.install();
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

bool OpenGlBackend::drain(TeardownBudget &budget)
{
    AV_SCOPE("OpenGlBackend::drain");
    if (gl_.glFinish == nullptr) return true;
    if (!context_.make_current()) return false;
    const bool syncs = gl_.glFenceSync != nullptr && gl_.glClientWaitSync != nullptr && gl_.glDeleteSync != nullptr;
    if (syncs && drain_fence_ != nullptr && fenced_at_ != submitted_) {
        gl_.glDeleteSync(static_cast<GLsync>(drain_fence_));
        drain_fence_ = nullptr;
    }
    if (syncs && drain_fence_ == nullptr) {
        drain_fence_ = gl_.glFenceSync(gl_sync_gpu_commands_complete, 0u);
        fenced_at_ = submitted_;
        if (drain_fence_ != nullptr && gl_.glFlush != nullptr) gl_.glFlush();
    }
    if (drain_fence_ == nullptr) {
        gl_.glFinish();
    } else {
        const uint32_t reached =
            gl_.glClientWaitSync(static_cast<GLsync>(drain_fence_), gl_sync_flush_commands_bit, budget.remaining());
        if (reached != gl_already_signaled && reached != gl_condition_satisfied) return false;
        gl_.glDeleteSync(static_cast<GLsync>(drain_fence_));
        drain_fence_ = nullptr;
    }
    drained_at_ = submitted_;
    static_cast<void>(progress_.settle());
    memories_.reclaim(import_);
    return true;
}

void OpenGlBackend::shut_down()
{
    AV_SCOPE("OpenGlBackend::shut_down");
    vram_guard_.clear();
    if (context_.make_current()) {
        if (drain_fence_ != nullptr && gl_.glDeleteSync != nullptr) gl_.glDeleteSync(static_cast<GLsync>(drain_fence_));
        if (gl_.glFinish != nullptr && drained_at_ != submitted_) gl_.glFinish();
        pack_.release();
        pack_.abandon_all();
        buffers_.give_back();
        memories_.destroy_all(import_);
        memories_.part();
        import_.give_back();
        progress_.release();
        void *fence = nullptr;
        while (fences_.take_any(fence))
            if (gl_.glDeleteSync != nullptr) gl_.glDeleteSync(static_cast<GLsync>(fence));
    } else {
        memories_.forget_all();
        memories_.part();
        progress_.forget();
        void *fence = nullptr;
        while (fences_.take_any(fence)) fence = nullptr;
    }
    drain_fence_ = nullptr;
    fenced_at_ = 0u;
    submitted_ = 0u;
    images_.give_back();
    natives_.give_back();
    wake_.unbind();
    context_.close();
    pending_.recover();
    gl_ = glproto::RealGl{};
    resolved_ = 0u;
    ran_ = 0u;
}

void OpenGlBackend::let_go_of_thread()
{
    AV_SCOPE("OpenGlBackend::let_go_of_thread");
    context_.let_go();
}

bool OpenGlBackend::knows(uint32_t command_id) const
{
    AV_SCOPE("OpenGlBackend::knows");
    return glproto::knows_command(command_id) || slices_.knows(command_id);
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
    submitted_++;
    if (slices_.knows(command_id)) return slices_.run(command_id, payload, reply, arena);
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
    if (!context_.make_current()) {
        if (!poll_failure_logged_)
            LogLine(lines_)
                .text("the context could not be made current to poll the host: status ")
                .number(static_cast<uint32_t>(Status::invalid_state))
                .write();
        poll_failure_logged_ = true;
        wake_.retry_later();
        return true;
    }
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
        pack_.sweep();
    }
    current_ = false;
}

void OpenGlBackend::publish_errors()
{
    AV_SCOPE("OpenGlBackend::publish_errors");
    if (gl_.glGetError == nullptr) return;
    pending_.collect();
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
    return pending_.lost();
}

bool OpenGlBackend::adopt(SourceMemory &memory, const BlobRole &role, uint64_t &handle)
{
    AV_SCOPE("OpenGlBackend::adopt");
    if (!role.is_arena()) return lend(memory, handle);
    if (!region_.adopt(memory, role, handle)) return false;
    OwnedFd packed;
    static_cast<void>(memory.share(packed));
    pack_.adopt(packed, role.base(), memory.size());
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

void OpenGlBackend::record_error(uint32_t error)
{
    AV_SCOPE("OpenGlBackend::record_error");
    pending_.record(error);
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

void OpenGlBackend::abandon(const SourceMemory &, uint64_t handle)
{
    AV_SCOPE("OpenGlBackend::abandon");
    memories_.retire(static_cast<uint32_t>(handle));
    memories_.reclaim(import_);
}

void OpenGlBackend::unbind_arena(const SourceMemory &memory)
{
    AV_SCOPE("OpenGlBackend::unbind_arena");
    if (!region_.is(memory)) return;
    pack_.abandon(region_.base_of(memory));
    region_.abandon(memory);
}

void OpenGlBackend::retire_arena(const SourceMemory &)
{
    AV_SCOPE("OpenGlBackend::retire_arena");
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
    wire_handle = 0u;
    detail = 0u;
    if (fd < 0 || into != 0u) return false;
    uint32_t name = 0u;
    if (kind == AV_SHARED_MEMORY) {
        if (handle_kind != AV_SHARED_HANDLE_DMA_BUF || size == 0u) return false;
        OwnedFd probe;
        if (!probe.hold_copy(fd)) return false;
        const uint64_t real = probe.extent();
        const uint64_t bytes = real < size ? real : size;
        if (bytes == 0u || !import_.import_dedicated_memory(fd, bytes, name)) return false;
        if (memories_.add(name, bytes) != Status::ok) {
            import_.delete_memory(name);
            return false;
        }
        if (images_.imports_images() && shape.described()) static_cast<void>(images_.import(name, fd, shape));
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
