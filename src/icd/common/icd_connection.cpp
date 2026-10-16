#include "icd/common/icd_connection.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::icd {

IcdConnection::IcdConnection(BlockSource &memory, Kmt &kmt, uint64_t luid)
    : kmt_(kmt), luid_(luid), search_(kmt), devices_(memory), links_(memory, Pool::paged), next_(0u),
      current_(no_link)
{
    AV_SCOPE("IcdConnection::IcdConnection");
}

IcdConnection::~IcdConnection()
{
    AV_SCOPE("IcdConnection::~IcdConnection");
    links_.erase_if([this](uint32_t, Link &link) {
        link.device_->close();
        static_cast<void>(devices_.destroy(link.device_));
        return true;
    });
    current_ = no_link;
    close_adapter_if_idle();
}

Status IcdConnection::acquire(uint32_t &link)
{
    AV_SCOPE("IcdConnection::acquire");
    link = no_link;
    if (current_ != no_link) {
        links_.find(current_)->users_++;
        link = current_;
        return Status::ok;
    }
    return open_link(link);
}

void IcdConnection::release(uint32_t link)
{
    AV_SCOPE("IcdConnection::release");
    Link *const held_link = links_.find(link);
    if (held_link == nullptr || --held_link->users_ != 0u) return;
    held_link->device_->close();
    static_cast<void>(devices_.destroy(held_link->device_));
    static_cast<void>(links_.erase(link));
    if (current_ == link) current_ = no_link;
    close_adapter_if_idle();
}

void IcdConnection::lose(uint32_t link)
{
    AV_SCOPE("IcdConnection::lose");
    Link *const held_link = links_.find(link);
    if (held_link == nullptr) return;
    held_link->lost_ = true;
    if (current_ == link) current_ = no_link;
}

bool IcdConnection::lost(uint32_t link) const
{
    AV_SCOPE("IcdConnection::lost");
    const Link *const held_link = links_.find(link);
    return held_link != nullptr && held_link->lost_;
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
    links_.for_each([&total](uint32_t, const Link &link) { total += link.users_; });
    return total;
}

GuestConnection &IcdConnection::device(uint32_t link)
{
    AV_SCOPE("IcdConnection::device");
    return *links_.find(link)->device_;
}

void IcdConnection::report(StateWriter &writer) const
{
    AV_SCOPE("IcdConnection::report");
    writer.open("connection", 0u);
    writer.number("users", users());
    writer.number("adapter", search_.adapter());
    writer.number("luid", luid_);
    links_.for_each([&writer](uint32_t number, const Link &link) {
        writer.open("link", number);
        writer.number("users", link.users_);
        writer.flag("lost", link.lost_);
        writer.number("device", link.device_->device());
        writer.close();
    });
    writer.close();
}

/* A link is held exactly while its device is open and used; the current link
 * is held and not lost, and it is the only such one; the adapter is kept
 * exactly while some link is held. */
bool IcdConnection::invariants_hold() const
{
    AV_SCOPE("IcdConnection::invariants_hold");
    bool     sound = links_.count() == devices_.live();
    uint32_t live = 0u;
    links_.for_each([&sound, &live](uint32_t number, const Link &link) {
        sound = sound && number != no_link && link.users_ != 0u && link.device_ != nullptr &&
                link.device_->opened() && link.device_->invariants_hold();
        if (!link.lost_) live++;
    });
    if (!sound || !links_.invariants_hold()) return false;
    if (current_ != no_link && (!held(current_) || lost(current_))) return false;
    if (live != (current_ != no_link ? 1u : 0u)) return false;
    return (links_.count() != 0u) == search_.found();
}

bool IcdConnection::held(uint32_t link) const
{
    AV_SCOPE("IcdConnection::held");
    return links_.find(link) != nullptr;
}

uint32_t IcdConnection::mint()
{
    AV_SCOPE("IcdConnection::mint");
    uint32_t number = next_;
    while (number == no_link || held(number)) number++;
    next_ = number + 1u;
    return number;
}

Status IcdConnection::open_link(uint32_t &link)
{
    AV_SCOPE("IcdConnection::open_link");
    if (!search_.found() && search_.find_avgpu(luid_) != Status::ok) return Status::not_supported;
    GuestConnection *device = nullptr;
    if (links_.reserve(links_.count() + 1u) != Status::ok || devices_.create(device, kmt_) != Status::ok) {
        close_adapter_if_idle();
        return Status::no_memory;
    }
    if (device->open(search_.adapter()) != Status::ok) {
        static_cast<void>(devices_.destroy(device));
        close_adapter_if_idle();
        return Status::not_supported;
    }
    const uint32_t number = mint();
    uint32_t       slot = 0u;
    static_cast<void>(links_.insert(number, Link{device, 1u, false}, slot));
    current_ = number;
    link = number;
    return Status::ok;
}

void IcdConnection::close_adapter_if_idle()
{
    AV_SCOPE("IcdConnection::close_adapter_if_idle");
    if (links_.count() != 0u) return;
    /* The adapter handle is ours alone; a failed close leaves nothing to redo. */
    static_cast<void>(search_.close());
}

}  // namespace av::icd
