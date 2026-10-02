#include "harness/icd/gl_context_rig.hpp"

#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"

namespace av {

GlContextRig::GlContextRig()
    : platform_(), kmt_(), host_(kmt_), trace_(), send_lock_(trace_, 'S'), log_(), connection_(kmt_),
      extensions_(nullptr, 0u, nullptr, 0u), handles_lock_(trace_, 'H'), handles_(kmt_, handles_lock_)
{
    const PciIdentity avgpu(AdapterSearch::virtio_vendor, AdapterSearch::virtio_gpu_device,
                             AdapterSearch::virtio_vendor, AdapterSearch::av_subsystem);
    kmt_.add_adapter(avgpu, AdapterScenario::wddm_3_2);
    /* The transport watches the reply slot for a budget of the guest's clock
     * before it asks the kernel to wait, and a command no answer was queued
     * for is one it would watch for ever on a clock that never moves. */
    kmt_.reads_clock(nullptr, transport_clock_step);
}

MockPlatform        &GlContextRig::platform() { return platform_; }
MockKmt             &GlContextRig::kmt() { return kmt_; }
ScriptedHost        &GlContextRig::host() { return host_; }
LockTrace           &GlContextRig::trace() { return trace_; }
RecordingThreadLock &GlContextRig::send_lock() { return send_lock_; }
RecordingLog        &GlContextRig::log() { return log_; }
av::icd::IcdConnection &GlContextRig::connection() { return connection_; }
const glicd::GlExtensionPolicy &GlContextRig::extensions() const { return extensions_; }

av::icd::SharedHandles &GlContextRig::handles()
{
    return handles_;
}

}  // namespace av
