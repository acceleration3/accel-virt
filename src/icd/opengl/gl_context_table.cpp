#include "icd/opengl/gl_context_table.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/log/keyed_log.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::glicd {

GlContextTable::GlContextTable(BlockSource &memory, icd::IcdConnection &connection, ThreadLock &lock,
                               ThreadLock &send_lock, ThreadLock &slice_lock, KeyedLog &lines,
                               const GlExtensionPolicy &extensions, icd::SharedHandles &handles)
    : memory_(memory), connection_(connection), lock_(lock), send_lock_(send_lock), lines_(lines),
      extensions_(extensions), handles_(handles), groups_(memory, slice_lock), pool_(memory),
      contexts_(memory, Pool::paged), retired_(memory, Pool::paged), retired_count_(0u), next_(1u)
{
    AV_SCOPE("GlContextTable::GlContextTable");
}

uint64_t GlContextTable::mint()
{
    AV_SCOPE("GlContextTable::mint");
    uint64_t handle = next_;
    while (contexts_.find(handle) != nullptr || retired_.find(handle) != nullptr)
        handle = handle == last_handle ? 1u : handle + 1u;
    next_ = handle == last_handle ? 1u : static_cast<uint32_t>(handle) + 1u;
    return handle;
}

Status GlContextTable::create(int32_t pixel_format, const GlContextOptions &options, uint64_t &handle)
{
    AV_SCOPE("GlContextTable::create");
    handle = 0u;
    const Status room = contexts_.reserve(contexts_.count() + 1u);
    if (room != Status::ok) return room;
    GuestGlContext *made = nullptr;
    const Status status = pool_.create(made, connection_, memory_, send_lock_, groups_, lines_, extensions_, handles_,
                                       pixel_format, options);
    if (status != Status::ok) return status;
    const uint64_t minted = mint();
    uint32_t       slot = 0u;
    static_cast<void>(contexts_.insert(minted, made, slot));
    handle = minted;
    return Status::ok;
}

Status GlContextTable::join(uint64_t joiner, uint64_t target)
{
    AV_SCOPE("GlContextTable::join");
    GuestGlContext *const sharer = resolve(joiner);
    GuestGlContext *const lender = resolve(target);
    if (sharer == nullptr || lender == nullptr) return Status::invalid_parameter;
    const Status admitted = groups_.admit_join(sharer->slice_group());
    if (admitted != Status::ok) return admitted;
    const Status shared = sharer->share_after(target);
    if (shared != Status::ok) return shared;
    const Status grouped = groups_.join(sharer->slice_group(), lender->slice_group());
    if (grouped != Status::ok) return grouped;
    lender->lend();
    return Status::ok;
}

GuestGlContext *GlContextTable::resolve(uint64_t handle) const
{
    AV_SCOPE("GlContextTable::resolve");
    GuestGlContext *const *const found = contexts_.find(handle);
    return found != nullptr ? *found : nullptr;
}

bool GlContextTable::shares_live(const GuestGlContext &context) const
{
    AV_SCOPE("GlContextTable::shares_live");
    bool shared = false;
    contexts_.for_each([this, &context, &shared](uint64_t, GuestGlContext *const &other) {
        shared = shared || (other != &context && groups_.same(other->slice_group(), context.slice_group()));
    });
    return shared;
}

bool GlContextTable::retires(const GuestGlContext &context) const
{
    AV_SCOPE("GlContextTable::retires");
    return context.slices_busy() != 0u && shares_live(context);
}

bool GlContextTable::reapable(GuestGlContext &context) const
{
    AV_SCOPE("GlContextTable::reapable");
    context.slices_reclaim();
    const uint32_t held = context.slices_owned() + context.slices_deferred();
    return context.slices_pinned() == 0u && (held == 0u || !shares_live(context));
}

Status GlContextTable::destroy(uint64_t handle)
{
    AV_SCOPE("GlContextTable::destroy");
    GuestGlContext *gone = nullptr;
    if (!contexts_.take(handle, gone)) return Status::invalid_parameter;
    if (contexts_.count() == 0u) contexts_.clear();
    Status status = Status::ok;
    uint32_t slot = 0u;
    if (retires(*gone) && retired_.insert(handle, gone, slot) == Status::ok) {
        gone->flush();
        __atomic_store_n(&retired_count_, retired_.count(), __ATOMIC_RELEASE);
    } else {
        status = pool_.destroy(gone);
    }
    reap_locked();
    return status;
}

void GlContextTable::reap()
{
    AV_SCOPE("GlContextTable::reap");
    if (__atomic_load_n(&retired_count_, __ATOMIC_ACQUIRE) == 0u) return;
    const ThreadLockHold hold(lock_);
    reap_locked();
}

void GlContextTable::reap_locked()
{
    AV_SCOPE("GlContextTable::reap_locked");
    for (;;) {
        uint64_t       found = 0u;
        GuestGlContext *zombie = nullptr;
        retired_.for_each([this, &found, &zombie](uint64_t handle, GuestGlContext *const &context) {
            if (zombie == nullptr && reapable(*context)) {
                found = handle;
                zombie = context;
            }
        });
        if (zombie == nullptr) break;
        static_cast<void>(retired_.erase(found));
        if (retired_.count() == 0u) retired_.clear();
        __atomic_store_n(&retired_count_, retired_.count(), __ATOMIC_RELEASE);
        if (pool_.destroy(zombie) != Status::ok) lines_.write("a retired OpenGL context could not be destroyed");
    }
}

uint32_t GlContextTable::live() const
{
    AV_SCOPE("GlContextTable::live");
    return contexts_.count();
}

uint32_t GlContextTable::retired() const
{
    AV_SCOPE("GlContextTable::retired");
    return retired_.count();
}

void GlContextTable::report(StateWriter &writer) const
{
    AV_SCOPE("GlContextTable::report");
    writer.open("contexts", 0u);
    writer.number("live", contexts_.count());
    writer.number("retired", retired_.count());
    contexts_.for_each([&writer](uint64_t handle, GuestGlContext *const &context) { context->report(writer, handle); });
    retired_.for_each([&writer](uint64_t handle, GuestGlContext *const &context) { context->report(writer, handle); });
    writer.close();
}

bool GlContextTable::invariants_hold() const
{
    AV_SCOPE("GlContextTable::invariants_hold");
    bool sound = next_ != 0u;
    contexts_.for_each([&sound](uint64_t, GuestGlContext *const &context) {
        sound = sound && context != nullptr && context->invariants_hold();
    });
    retired_.for_each([&sound](uint64_t, GuestGlContext *const &context) {
        sound = sound && context != nullptr && context->invariants_hold();
    });
    return sound && contexts_.count() + retired_.count() == pool_.live() && contexts_.invariants_hold() &&
           retired_.invariants_hold() && groups_.invariants_hold();
}

}
