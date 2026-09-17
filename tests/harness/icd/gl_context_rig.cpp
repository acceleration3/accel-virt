#include "harness/icd/gl_context_rig.hpp"

#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"

namespace av {

GlContextRig::GlContextRig()
    : platform_(), kmt_(), host_(kmt_), trace_(), send_lock_(trace_, 'S'), log_(), connection_(kmt_),
      extensions_(nullptr, 0u, nullptr, 0u)
{
    const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                             AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
    kmt_.add_adapter(rvkgpu, AdapterScenario::wddm_3_2);
}

MockPlatform        &GlContextRig::platform() { return platform_; }
MockKmt             &GlContextRig::kmt() { return kmt_; }
ScriptedHost        &GlContextRig::host() { return host_; }
LockTrace           &GlContextRig::trace() { return trace_; }
RecordingThreadLock &GlContextRig::send_lock() { return send_lock_; }
RecordingLog        &GlContextRig::log() { return log_; }
glicd::GlConnection &GlContextRig::connection() { return connection_; }
const glicd::GlExtensionPolicy &GlContextRig::extensions() const { return extensions_; }

}  // namespace av
