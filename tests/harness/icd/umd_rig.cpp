#include "harness/icd/umd_rig.hpp"

#include "guest/kmt/memory_segment_group.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "icd/d3d/umd_binding.hpp"

namespace av {

namespace {

const PciIdentity avgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                         AdapterScenario::virtio_vendor, AdapterScenario::av_subsystem);

}  // namespace

UmdRig::UmdRig(d3d::UmdHostDriver *builder)
    : platform_(), kmt_(), trace_(), lock_(trace_, 'U'), log_(),
      root_(platform_, kmt_, lock_, log_, d3d::umd_binding, builder)
{
    kmt_.add_adapter(avgpu, AdapterScenario::wddm_3_2);
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

}  // namespace av
