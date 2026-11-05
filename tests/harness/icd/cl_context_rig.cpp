#include "harness/icd/cl_context_rig.hpp"
#include "harness/icd/cl_icd_rig.hpp"

#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"

namespace av {

ClContextRig::ClContextRig()
    : platform_(), kmt_(), host_(kmt_), trace_(), send_lock_(trace_, 'S'), blocks_lock_(trace_, 'B'), log_(), connection_(platform_, kmt_, 0u),
      handles_lock_(trace_, 'H'), handles_(kmt_, handles_lock_, platform_)
{
    const PciIdentity avgpu(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                             PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);
    kmt_.add_adapter(avgpu, AdapterScenario::wddm_3_2);
    kmt_.open_host_window(MockKmt::rig_window_bytes);
    kmt_.reads_clock(nullptr, ClIcdRig::transport_clock_step);
}

av::icd::SharedHandles &ClContextRig::handles() { return handles_; }
MockPlatform           &ClContextRig::platform() { return platform_; }
MockKmt                &ClContextRig::kmt() { return kmt_; }
ScriptedHost           &ClContextRig::host() { return host_; }
LockTrace              &ClContextRig::trace() { return trace_; }
RecordingThreadLock    &ClContextRig::send_lock() { return send_lock_; }
RecordingThreadLock    &ClContextRig::blocks_lock() { return blocks_lock_; }
RecordingLog           &ClContextRig::log() { return log_; }
av::icd::IcdConnection &ClContextRig::connection() { return connection_; }

}
