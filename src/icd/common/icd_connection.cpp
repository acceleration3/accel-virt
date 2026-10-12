#include "icd/common/icd_connection.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::icd {

IcdConnection::IcdConnection(Kmt &kmt, uint64_t luid)
    : kmt_(kmt), luid_(luid), search_(kmt), devices_{}, users_{}, lost_{}, current_(no_link)
{
    AV_SCOPE("IcdConnection::IcdConnection");
}

IcdConnection::~IcdConnection()
{
    AV_SCOPE("IcdConnection::~IcdConnection");
    for (uint32_t link = 0u; link < max_links; link++) {
        if (!held(link)) continue;
        users_[link] = 1u;
        release(link);
    }
}

Status IcdConnection::acquire(uint32_t &link)
{
    AV_SCOPE("IcdConnection::acquire");
    link = no_link;
    if (current_ != no_link) {
        users_[current_]++;
        link = current_;
        return Status::ok;
    }
    uint32_t free = 0u;
    while (free < max_links && held(free)) free++;
    if (free == max_links) return Status::no_space;
    const Status opened = open_link(free);
    if (opened != Status::ok) return opened;
    link = free;
    return Status::ok;
}

void IcdConnection::release(uint32_t link)
{
    AV_SCOPE("IcdConnection::release");
    if (!held(link) || --users_[link] != 0u) return;
    devices_[link].object().close();
    devices_[link].destroy();
    lost_[link] = false;
    if (current_ == link) current_ = no_link;
    close_adapter_if_idle();
}

void IcdConnection::lose(uint32_t link)
{
    AV_SCOPE("IcdConnection::lose");
    if (!held(link)) return;
    lost_[link] = true;
    if (current_ == link) current_ = no_link;
}

bool IcdConnection::lost(uint32_t link) const
{
    AV_SCOPE("IcdConnection::lost");
    return link < max_links && lost_[link];
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
    for (const uint32_t count : users_) total += count;
    return total;
}

GuestConnection &IcdConnection::device(uint32_t link)
{
    AV_SCOPE("IcdConnection::device");
    return devices_[link].object();
}

void IcdConnection::report(StateWriter &writer) const
{
    AV_SCOPE("IcdConnection::report");
    writer.open("connection", 0u);
    writer.number("users", users());
    writer.number("adapter", search_.adapter());
    writer.number("luid", luid_);
    for (uint32_t link = 0u; link < max_links; link++) {
        if (!held(link)) continue;
        writer.open("link", link);
        writer.number("users", users_[link]);
        writer.flag("lost", lost_[link]);
        writer.number("device", devices_[link].object().device());
        writer.close();
    }
    writer.close();
}

/* A link is held exactly while its device is open; only a held link can be
 * lost; the current link is held and not lost, and it is the only such one;
 * the adapter is kept exactly while some link is held. */
bool IcdConnection::invariants_hold() const
{
    AV_SCOPE("IcdConnection::invariants_hold");
    bool     any = false;
    uint32_t live = 0u;
    for (uint32_t link = 0u; link < max_links; link++) {
        const bool constructed = devices_[link].constructed();
        if (constructed != (users_[link] != 0u)) return false;
        if (lost_[link] && !constructed) return false;
        if (!constructed) continue;
        if (!devices_[link].object().opened() || !devices_[link].object().invariants_hold()) return false;
        any = true;
        if (!lost_[link]) live++;
    }
    if (current_ != no_link && (!held(current_) || lost_[current_])) return false;
    if (live != (current_ != no_link ? 1u : 0u)) return false;
    return any == search_.found();
}

bool IcdConnection::held(uint32_t link) const
{
    AV_SCOPE("IcdConnection::held");
    return link < max_links && users_[link] != 0u;
}

Status IcdConnection::open_link(uint32_t link)
{
    AV_SCOPE("IcdConnection::open_link");
    if (!search_.found() && search_.find_avgpu(luid_) != Status::ok) return Status::not_supported;
    /* The link is not held, so its storage is empty. */
    static_cast<void>(devices_[link].construct(kmt_));
    if (devices_[link].object().open(search_.adapter()) != Status::ok) {
        devices_[link].destroy();
        close_adapter_if_idle();
        return Status::not_supported;
    }
    users_[link] = 1u;
    lost_[link] = false;
    current_ = link;
    return Status::ok;
}

void IcdConnection::close_adapter_if_idle()
{
    AV_SCOPE("IcdConnection::close_adapter_if_idle");
    for (uint32_t link = 0u; link < max_links; link++)
        if (held(link)) return;
    /* The adapter handle is ours alone; a failed close leaves nothing to redo. */
    static_cast<void>(search_.close());
}

}  // namespace av::icd
