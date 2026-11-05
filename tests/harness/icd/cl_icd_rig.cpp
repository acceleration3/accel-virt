#include "harness/icd/cl_icd_rig.hpp"

#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"

namespace av {

ClIcdRig::ClIcdRig()
    : platform_(), kmt_(), host_(kmt_), trace_(), lock_(trace_, 'H'), send_lock_(trace_, 'S'),
      objects_lock_(trace_, 'O'), blocks_lock_(trace_, 'B'), log_(), lines_(platform_, log_, "avcl: "), watcher_(), gl_share_(), d3d_adapters_(), d3d11_(),
      root_(platform_, kmt_, lock_, send_lock_, objects_lock_, blocks_lock_, lines_, watcher_, gl_share_, d3d_adapters_, d3d11_,
            clicd::cl_icd_binding),
      loader_(root_)
{
    const PciIdentity avgpu(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                             PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);
    kmt_.add_adapter(avgpu, AdapterScenario::wddm_3_2);
    kmt_.open_host_window(MockKmt::rig_window_bytes);
    kmt_.set_adapter_luid(0u, adapter_luid);
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
ScriptedD3dAdapterSource &ClIcdRig::d3d_adapters() { return d3d_adapters_; }

ScriptedD3d11Bridge &ClIcdRig::d3d11() { return d3d11_; }
clicd::ClIcdRoot    &ClIcdRig::root() { return root_; }
FakeClLoader        &ClIcdRig::loader() { return loader_; }

}
