#include "harness/icd/cl_context_rig.hpp"

#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"

namespace av {

ClContextRig::ClContextRig()
    : platform_(), kmt_(), host_(kmt_), trace_(), send_lock_(trace_, 'S'), log_(), connection_(kmt_)
{
    const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                             AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
    kmt_.add_adapter(rvkgpu, AdapterScenario::wddm_3_2);
}

MockPlatform           &ClContextRig::platform() { return platform_; }
MockKmt                &ClContextRig::kmt() { return kmt_; }
ScriptedHost           &ClContextRig::host() { return host_; }
LockTrace              &ClContextRig::trace() { return trace_; }
RecordingThreadLock    &ClContextRig::send_lock() { return send_lock_; }
RecordingLog           &ClContextRig::log() { return log_; }
av::icd::IcdConnection &ClContextRig::connection() { return connection_; }

}  // namespace av
