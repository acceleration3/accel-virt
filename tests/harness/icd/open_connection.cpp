#include "harness/icd/open_connection.hpp"

#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"

namespace av {

OpenConnection::OpenConnection()
    : kmt_(), platform_(), host_(kmt_), trace_(), lock_(trace_, 'S'), connection_(kmt_, platform_, lock_)
{
    kmt_.add_adapter(PciIdentity(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                                 AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem),
                     AdapterScenario::wddm_3_2);
    CHECK_STATUS(connection_.open(), Status::ok);
}

MockKmt             &OpenConnection::kmt() { return kmt_; }
MockPlatform        &OpenConnection::platform() { return platform_; }
ScriptedHost        &OpenConnection::host() { return host_; }
LockTrace           &OpenConnection::trace() { return trace_; }
RecordingThreadLock &OpenConnection::lock() { return lock_; }
icd::Connection     &OpenConnection::connection() { return connection_; }

}  // namespace av
