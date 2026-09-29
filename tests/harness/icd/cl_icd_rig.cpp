#include "harness/icd/cl_icd_rig.hpp"

#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"

namespace av {

ClIcdRig::ClIcdRig()
    : platform_(), kmt_(), host_(kmt_), trace_(), lock_(trace_, 'H'), send_lock_(trace_, 'S'),
      log_(), watcher_(), gl_share_(),
      root_(platform_, kmt_, lock_, send_lock_, log_, watcher_, gl_share_, clicd::cl_icd_binding),
      loader_(root_)
{
    const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                             AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
    kmt_.add_adapter(rvkgpu, AdapterScenario::wddm_3_2);
    kmt_.set_adapter_luid(0u, adapter_luid);
    /* The transport watches the reply slot for a budget of the guest's clock
     * before it asks the kernel to wait, and a command no answer was queued
     * for is one it would watch for ever on a clock that never moves. */
    kmt_.reads_clock(nullptr, transport_clock_step);
}

MockPlatform        &ClIcdRig::platform() { return platform_; }
MockKmt             &ClIcdRig::kmt() { return kmt_; }
ScriptedHost        &ClIcdRig::host() { return host_; }
LockTrace           &ClIcdRig::trace() { return trace_; }
RecordingThreadLock &ClIcdRig::lock() { return lock_; }
RecordingThreadLock &ClIcdRig::send_lock() { return send_lock_; }
RecordingLog        &ClIcdRig::log() { return log_; }
ScriptedThread      &ClIcdRig::watcher() { return watcher_; }
ScriptedGlShareSource &ClIcdRig::gl_share() { return gl_share_; }
clicd::ClIcdRoot    &ClIcdRig::root() { return root_; }
FakeClLoader        &ClIcdRig::loader() { return loader_; }

}  // namespace av
