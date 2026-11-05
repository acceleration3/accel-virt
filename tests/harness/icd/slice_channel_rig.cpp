#include "harness/icd/slice_channel_rig.hpp"

#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/blob_import_answer.hpp"

namespace av {

namespace {

constexpr std::uint64_t first_memory = 0x0000700000000001ull;
constexpr std::uint64_t clock_start = 1000000000ull;

icd::ChannelShape slice_shape() { return icd::ChannelShape(SubApi::vulkan, 12u, 4096u, 4096u, 0u, 0u); }

}

SliceChannelRig::SliceChannelRig()
    : platform_(), kmt_(), connection_(kmt_), host_(kmt_), sink_(), lines_(sink_, "avtest: "), trace_(),
      lock_(trace_, 'M'), stream_lock_(trace_, 'S'), clock_(clock_start, 0u), channel_(platform_, lines_, slice_shape(), lock_),
      next_memory_(first_memory), opened_(0u)
{
    kmt_.add_adapter(PciIdentity(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                                 PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem),
                     AdapterScenario::wddm_3_2);
    kmt_.open_host_window(MockKmt::rig_window_bytes);
    std::uint32_t adapter = 0u, count = 0u;
    CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
    CHECK_STATUS(connection_.open(adapter), Status::ok);
    CHECK_STATUS(channel_.open(connection_), Status::ok);
}

SliceChannelRig::~SliceChannelRig()
{
    channel_.close(true);
    CHECK_EQ(kmt_.misuses(), 0u);
}

void SliceChannelRig::will_open()
{
    host_.blob_import(0u, next_memory_++);
    opened_++;
}

void SliceChannelRig::will_close() { host_.nop(); }

void SliceChannelRig::will_refuse_open() { host_.blob_import(BlobImportAnswer::executor_refused, 0u); }

MockPlatform        &SliceChannelRig::platform() { return platform_; }
MockKmt             &SliceChannelRig::kmt() { return kmt_; }
ScriptedHost        &SliceChannelRig::host() { return host_; }
ScriptedClock       &SliceChannelRig::clock() { return clock_; }
RecordingLog        &SliceChannelRig::sink() { return sink_; }
DriverLog           &SliceChannelRig::lines() { return lines_; }
RecordingThreadLock &SliceChannelRig::lock() { return lock_; }
RecordingThreadLock &SliceChannelRig::stream_lock() { return stream_lock_; }
icd::HostChannel    &SliceChannelRig::channel() { return channel_; }
std::uint32_t        SliceChannelRig::opened() const { return opened_; }

}
