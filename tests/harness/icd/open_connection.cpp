#include "harness/icd/open_connection.hpp"

#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "harness/framework/test_registry.hpp"

namespace av {

OpenConnection::OpenConnection()
    : kmt_(), platform_(), host_(kmt_), trace_(), lock_(trace_, 'S'), log_(), connection_(kmt_, platform_, lock_, log_)
{
    kmt_.add_adapter(PciIdentity(AdapterSearch::virtio_vendor, AdapterSearch::virtio_gpu_device,
                                 AdapterSearch::virtio_vendor, AdapterSearch::av_subsystem),
                     AdapterScenario::wddm_3_2);
    CHECK_STATUS(connection_.open(), Status::ok);
}

MockKmt             &OpenConnection::kmt() { return kmt_; }
MockPlatform        &OpenConnection::platform() { return platform_; }
ScriptedHost        &OpenConnection::host() { return host_; }
LockTrace           &OpenConnection::trace() { return trace_; }
RecordingThreadLock &OpenConnection::lock() { return lock_; }
RecordingLog        &OpenConnection::log() { return log_; }
icd::Connection     &OpenConnection::connection() { return connection_; }

}  // namespace av
