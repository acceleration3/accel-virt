#include "icd/opengl/gl_context_table.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::glicd {

GlContextTable::GlContextTable(BlockSource &memory, icd::IcdConnection &connection, ThreadLock &send_lock,
                               KeyedLog &lines, const GlExtensionPolicy &extensions,
                               icd::SharedHandles &handles)
    : memory_(memory), connection_(connection), send_lock_(send_lock), lines_(lines), extensions_(extensions),
      handles_(handles), pool_(memory), contexts_(memory, Pool::paged), next_(1u)
{
    AV_SCOPE("GlContextTable::GlContextTable");
}

uint64_t GlContextTable::mint()
{
    AV_SCOPE("GlContextTable::mint");
    uint64_t handle = next_;
    while (contexts_.find(handle) != nullptr) handle = handle == last_handle ? 1u : handle + 1u;
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
    const Status status = pool_.create(made, connection_, memory_, send_lock_, lines_, extensions_, handles_,
                                       pixel_format, options);
    if (status != Status::ok) return status;
    const uint64_t minted = mint();
    uint32_t       slot = 0u;
    static_cast<void>(contexts_.insert(minted, made, slot));
    handle = minted;
    return Status::ok;
}

GuestGlContext *GlContextTable::resolve(uint64_t handle) const
{
    AV_SCOPE("GlContextTable::resolve");
    GuestGlContext *const *const found = contexts_.find(handle);
    return found != nullptr ? *found : nullptr;
}

Status GlContextTable::destroy(uint64_t handle)
{
    AV_SCOPE("GlContextTable::destroy");
    GuestGlContext *gone = nullptr;
    if (!contexts_.take(handle, gone)) return Status::invalid_parameter;
    if (contexts_.count() == 0u) contexts_.clear();
    return pool_.destroy(gone);
}

uint32_t GlContextTable::live() const
{
    AV_SCOPE("GlContextTable::live");
    return contexts_.count();
}

void GlContextTable::report(StateWriter &writer) const
{
    AV_SCOPE("GlContextTable::report");
    writer.open("contexts", 0u);
    writer.number("live", contexts_.count());
    contexts_.for_each([&writer](uint64_t handle, GuestGlContext *const &context) { context->report(writer, handle); });
    writer.close();
}

bool GlContextTable::invariants_hold() const
{
    AV_SCOPE("GlContextTable::invariants_hold");
    bool sound = next_ != 0u;
    contexts_.for_each([&sound](uint64_t, GuestGlContext *const &context) {
        sound = sound && context != nullptr && context->invariants_hold();
    });
    return sound && contexts_.count() == pool_.live() && contexts_.invariants_hold();
}

}
