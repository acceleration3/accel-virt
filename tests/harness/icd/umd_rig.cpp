#include "harness/icd/umd_rig.hpp"

#include "guest/kmt/memory_segment_group.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "icd/d3d/umd_binding.hpp"

namespace av {

namespace {

const PciIdentity avgpu(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                         PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);

}

UmdRig::UmdRig(d3d::UmdHostDriver *builder)
    : platform_(), kmt_(), trace_(), lock_(trace_, 'U'), log_(), lines_(log_, "avumd: "),
      root_(platform_, kmt_, lock_, lines_, d3d::umd_binding, builder)
{
    kmt_.add_adapter(avgpu, AdapterScenario::wddm_3_2);
    kmt_.open_host_window(MockKmt::rig_window_bytes);
    kmt_.set_adapter_luid(0u, adapter_luid);
    kmt_.set_video_memory(MemorySegmentGroup::local, local_budget, 0u);
    kmt_.set_video_memory(MemorySegmentGroup::non_local, non_local_budget, 0u);
}

MockPlatform        &UmdRig::platform() { return platform_; }
MockKmt             &UmdRig::kmt() { return kmt_; }
LockTrace           &UmdRig::trace() { return trace_; }
RecordingThreadLock &UmdRig::lock() { return lock_; }
RecordingLog        &UmdRig::log() { return log_; }
d3d::UmdRoot        &UmdRig::root() { return root_; }

}
