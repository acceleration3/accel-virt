#include "icd/opengl/gl_connection.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"

namespace av::glicd {

GlConnection::GlConnection(Kmt &kmt) : search_(kmt), device_(kmt), users_(0u)
{
    AV_SCOPE("GlConnection::GlConnection");
}

GlConnection::~GlConnection()
{
    AV_SCOPE("GlConnection::~GlConnection");
    if (users_ == 0u) return;
    users_ = 1u;
    release();
}

Status GlConnection::acquire()
{
    AV_SCOPE("GlConnection::acquire");
    if (users_ != 0u) {
        users_++;
        return Status::ok;
    }
    const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                             AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
    if (search_.find(rvkgpu) != Status::ok) return Status::not_supported;
    if (device_.open(search_.adapter()) != Status::ok) {
        /* Nothing is left to report a failed close to: the open failed first. */
        static_cast<void>(search_.close());
        return Status::not_supported;
    }
    users_ = 1u;
    return Status::ok;
}

void GlConnection::release()
{
    AV_SCOPE("GlConnection::release");
    if (users_ == 0u || --users_ != 0u) return;
    device_.close();
    /* The adapter handle is ours alone; a failed close leaves nothing to redo. */
    static_cast<void>(search_.close());
}

uint32_t         GlConnection::users() const
{
    AV_SCOPE("GlConnection::users");
    return users_;
}
GuestConnection &GlConnection::device()
{
    AV_SCOPE("GlConnection::device");
    return device_;
}

void GlConnection::report(StateWriter &writer) const
{
    AV_SCOPE("GlConnection::report");
    writer.open("connection", 0u);
    writer.number("users", users_);
    writer.number("adapter", device_.adapter());
    writer.number("device", device_.device());
    writer.close();
}

bool GlConnection::invariants_hold() const
{
    AV_SCOPE("GlConnection::invariants_hold");
    if ((users_ != 0u) != device_.opened()) return false;
    return (users_ != 0u) == search_.found() && device_.invariants_hold();
}

}  // namespace av::glicd
