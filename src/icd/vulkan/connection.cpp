#include "icd/vulkan/connection.hpp"

#include "core/support/scope_exit.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "icd/vulkan/adapter_luids.hpp"
#include "proto/context/sub_api.hpp"

namespace av::icd {

Connection::Connection(Kmt &kmt, AdapterLuids &luids, ThreadLock &lock)
    : luids_(luids), search_(kmt), connection_(kmt), transport_(connection_, SubApi::vulkan, ring_order),
      locked_(transport_, lock), luid_(0u) {}

Connection::~Connection() { close(); }

Status Connection::open()
{
    if (opened()) return Status::invalid_state;
    ScopeExit undo([&] { close(); });
    const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                             AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
    if (search_.find(rvkgpu) != Status::ok) return Status::not_supported;
    if (luids_.adapter_luid(search_.adapter(), luid_) != Status::ok) return Status::not_supported;
    if (connection_.open(search_.adapter()) != Status::ok) return Status::not_supported;
    if (transport_.initialize() != Status::ok) return Status::not_supported;
    undo.dismiss();
    return Status::ok;
}

void Connection::close()
{
    transport_.teardown();
    connection_.close();
    /* Nothing is left to report a failed close to: the instance is gone. */
    static_cast<void>(search_.close());
    luid_ = 0u;
}

bool             Connection::opened() const { return transport_.established(); }
bool             Connection::faulted() const { return transport_.faulted(); }
uint64_t         Connection::luid() const { return luid_; }
ClientTransport &Connection::transport() { return locked_; }

bool Connection::invariants_hold() const
{
    if (!connection_.invariants_hold() || !transport_.invariants_hold()) return false;
    if (!opened()) return !connection_.opened() && !search_.found();
    return connection_.opened() && search_.found() && connection_.adapter() == search_.adapter();
}

}  // namespace av::icd
