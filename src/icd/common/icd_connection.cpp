#include "icd/common/icd_connection.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::icd {

IcdConnection::IcdConnection(BlockSource &memory, Kmt &kmt, uint64_t luid)
    : kmt_(kmt), luid_(luid), search_(kmt), links_(memory)
{
    AV_SCOPE("IcdConnection::IcdConnection");
}

Status IcdConnection::acquire(DeviceLink *&link)
{
    AV_SCOPE("IcdConnection::acquire");
    link = nullptr;
    if (!search_.found() && search_.find_avgpu(luid_) != Status::ok) return Status::not_supported;
    bool        left_lost = false;
    DeviceLink *joined = links_.join(left_lost, kmt_);
    if (joined == nullptr) {
        close_adapter_if_idle();
        return Status::no_memory;
    }
    if (joined->users() == 0u && joined->open(search_.adapter()) != Status::ok) {
        links_.drop(*joined);
        close_adapter_if_idle();
        return Status::not_supported;
    }
    joined->hold();
    link = joined;
    return Status::ok;
}

void IcdConnection::release(DeviceLink &link)
{
    AV_SCOPE("IcdConnection::release");
    if (!link.let_go()) return;
    links_.drop(link);
    close_adapter_if_idle();
}

uint32_t IcdConnection::adapter() const
{
    AV_SCOPE("IcdConnection::adapter");
    return search_.adapter();
}

uint32_t IcdConnection::users() const
{
    AV_SCOPE("IcdConnection::users");
    uint32_t total = 0u;
    links_.for_each([&total](const DeviceLink &link, bool) { total += link.users(); });
    return total;
}

void IcdConnection::report(StateWriter &writer) const
{
    AV_SCOPE("IcdConnection::report");
    writer.open("connection", 0u);
    writer.number("users", users());
    writer.number("adapter", search_.adapter());
    writer.number("luid", luid_);
    links_.for_each([&writer](const DeviceLink &link, bool) { link.report(writer); });
    writer.close();
}

/* Every link is held by a context and its device open; the adapter is kept
 * exactly while some link is. */
bool IcdConnection::invariants_hold() const
{
    AV_SCOPE("IcdConnection::invariants_hold");
    bool sound = links_.invariants_hold();
    links_.for_each([&sound](const DeviceLink &link, bool) {
        sound = sound && link.users() != 0u && link.invariants_hold();
    });
    return sound && (links_.count() != 0u) == search_.found();
}

void IcdConnection::close_adapter_if_idle()
{
    AV_SCOPE("IcdConnection::close_adapter_if_idle");
    if (links_.count() != 0u) return;
    /* The adapter handle is ours alone; a failed close leaves nothing to redo. */
    static_cast<void>(search_.close());
}

}  // namespace av::icd
