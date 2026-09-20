#include "icd/d3d/umd_connection.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"

namespace av::d3d {
namespace {

const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                         AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);

}  // namespace

UmdConnection::UmdConnection(Kmt &kmt) : search_(kmt), opens_(0u)
{
    AV_SCOPE("UmdConnection::UmdConnection");
}

UmdConnection::~UmdConnection()
{
    AV_SCOPE("UmdConnection::~UmdConnection");
}

Status UmdConnection::acquire()
{
    AV_SCOPE("UmdConnection::acquire");
    if (search_.found()) {
        opens_++;
        return Status::ok;
    }
    const Status found = search_.find(rvkgpu);
    if (found != Status::ok) return found == Status::not_found ? Status::not_supported : found;
    opens_ = 1u;
    return Status::ok;
}

Status UmdConnection::release()
{
    AV_SCOPE("UmdConnection::release");
    if (opens_ == 0u) return Status::invalid_state;
    opens_--;
    if (opens_ != 0u) return Status::ok;
    return search_.close();
}

bool UmdConnection::open() const
{
    AV_SCOPE("UmdConnection::open");
    return search_.found();
}

uint32_t UmdConnection::adapter() const
{
    AV_SCOPE("UmdConnection::adapter");
    return search_.adapter();
}

uint32_t UmdConnection::opens() const
{
    AV_SCOPE("UmdConnection::opens");
    return opens_;
}

void UmdConnection::report(StateWriter &writer) const
{
    AV_SCOPE("UmdConnection::report");
    writer.flag("open", search_.found());
    writer.number("adapter", search_.adapter());
    writer.number("opens", opens_);
}

bool UmdConnection::invariants_hold() const
{
    AV_SCOPE("UmdConnection::invariants_hold");
    return search_.found() == (opens_ != 0u);
}

}  // namespace av::d3d
