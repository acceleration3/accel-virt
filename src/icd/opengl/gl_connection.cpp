#include "icd/opengl/gl_connection.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"

namespace av::glicd {

GlConnection::GlConnection(Kmt &kmt) : kmt_(kmt), search_(kmt), devices_{}, users_{}, lost_{}, current_(no_link)
{
    AV_SCOPE("GlConnection::GlConnection");
}

GlConnection::~GlConnection()
{
    AV_SCOPE("GlConnection::~GlConnection");
    for (uint32_t link = 0u; link < max_links; link++) {
        if (!held(link)) continue;
        users_[link] = 1u;
        release(link);
    }
}

Status GlConnection::acquire(uint32_t &link)
{
    AV_SCOPE("GlConnection::acquire");
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

void GlConnection::release(uint32_t link)
{
    AV_SCOPE("GlConnection::release");
    if (!held(link) || --users_[link] != 0u) return;
    devices_[link].object().close();
    devices_[link].destroy();
    lost_[link] = false;
    if (current_ == link) current_ = no_link;
    close_adapter_if_idle();
}

void GlConnection::lose(uint32_t link)
{
    AV_SCOPE("GlConnection::lose");
    if (!held(link)) return;
    lost_[link] = true;
    if (current_ == link) current_ = no_link;
}

bool GlConnection::lost(uint32_t link) const
{
    AV_SCOPE("GlConnection::lost");
    return link < max_links && lost_[link];
}

uint32_t GlConnection::users() const
{
    AV_SCOPE("GlConnection::users");
    uint32_t total = 0u;
    for (const uint32_t count : users_) total += count;
    return total;
}

GuestConnection &GlConnection::device(uint32_t link)
{
    AV_SCOPE("GlConnection::device");
    return devices_[link].object();
}

void GlConnection::report(StateWriter &writer) const
{
    AV_SCOPE("GlConnection::report");
    writer.open("connection", 0u);
    writer.number("users", users());
    writer.number("adapter", search_.adapter());
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
bool GlConnection::invariants_hold() const
{
    AV_SCOPE("GlConnection::invariants_hold");
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

bool GlConnection::held(uint32_t link) const
{
    AV_SCOPE("GlConnection::held");
    return link < max_links && users_[link] != 0u;
}

Status GlConnection::open_link(uint32_t link)
{
    AV_SCOPE("GlConnection::open_link");
    const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                             AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
    if (!search_.found() && search_.find(rvkgpu) != Status::ok) return Status::not_supported;
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

void GlConnection::close_adapter_if_idle()
{
    AV_SCOPE("GlConnection::close_adapter_if_idle");
    for (uint32_t link = 0u; link < max_links; link++)
        if (held(link)) return;
    /* The adapter handle is ours alone; a failed close leaves nothing to redo. */
    static_cast<void>(search_.close());
}

}  // namespace av::glicd
