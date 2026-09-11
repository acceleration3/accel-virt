#include "../../src/core/bar_mappings.hpp"
#include "../../src/core/dxgkrnl_interface.hpp"
#include "../../src/core/pci_base_addresses.hpp"
#include "../../src/core/resource_list.hpp"
#include "../../src/core/status.hpp"
#include "../../src/core/virt_queue.hpp"
#include "../../src/core/virtio_capability_walk.hpp"
#include "../../src/core/virtio_gpu_device.hpp"
#include "../../src/core/virtio_pci_layout.hpp"
#include "../../src/core/virtq_segment.hpp"
#include "../harness/fake_dxgkrnl_adapter.hpp"
#include "../harness/fake_virtio_gpu_function.hpp"
#include "../harness/fake_virtqueue_device.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstdint>

namespace av {

struct VirtioGpuDeviceProbe {
    explicit VirtioGpuDeviceProbe(VirtioGpuDevice &target) : device(target) {}
    VirtQueue &control() { return device.control_; }
    void set_running() { device.running_ = true; }
    VirtioGpuDevice &device;
};

}  // namespace av

using av::BarMappings;
using av::DxgkDeviceInfoRecord;
using av::DxgkrnlInterface;
using av::FakeCommonConfig;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::FakeVirtqueueDevice;
using av::Irql;
using av::MockPlatform;
using av::PciBaseAddresses;
using av::ResourceList;
using av::SegmentDirection;
using av::Status;
using av::Violation;
using av::VirtioCapabilityWalk;
using av::VirtioGpuDevice;
using av::VirtioGpuDeviceProbe;
using av::VirtioPciLayout;
using av::VirtqSegment;

namespace {

constexpr std::uint64_t all_features = FakeVirtioGpuFunction::feature_version_1 |
                                       FakeVirtioGpuFunction::feature_edid |
                                       FakeVirtioGpuFunction::feature_resource_blob |
                                       FakeVirtioGpuFunction::feature_context_init;
constexpr std::uint16_t no_vector = 0xFFFFu;
constexpr std::uint8_t  driver_ok_status = 0x0Fu;
constexpr std::uint8_t  failed_bit = 0x80u;

/* The function mapped as a started adapter maps it. */
class MappedFunction {
public:
    MappedFunction() : dxgkrnl_(platform_, function_), interface_(platform_), mappings_(platform_, interface_) {}

    Status map()
    {
        ResourceList resources(platform_);
        DxgkDeviceInfoRecord info;
        Status status = interface_.initialize(&dxgkrnl_.record());
        if (status == Status::ok) status = interface_.get_device_information(info);
        if (status == Status::ok) status = resources.initialize(info.TranslatedResourceList, ResourceList::max_bytes);
        if (status == Status::ok) status = VirtioCapabilityWalk(function_.config_space(), 256u).walk(layout_);
        const PciBaseAddresses bars(function_.config_space(), 256u);
        if (status == Status::ok) status = mappings_.map(bars, resources, layout_);
        return status;
    }

    MockPlatform          &platform() { return platform_; }
    FakeVirtioGpuFunction &function() { return function_; }
    FakeCommonConfig      &common() { return function_.common(); }
    BarMappings           &mappings() { return mappings_; }
    const VirtioPciLayout &layout() const { return layout_; }

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    BarMappings           mappings_;
    VirtioPciLayout       layout_;
};

const VirtqSegment request[] = {VirtqSegment(0x5000u, 24u, SegmentDirection::device_reads)};

}  // namespace

TEST(virtio_gpu_device_start, brings_the_device_to_driver_ok_with_one_vector_per_source)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(3u), Status::ok);
  CHECK(device.invariants_hold());
  CHECK(device.features() == all_features);
  CHECK_EQ(mapped.common().status(), driver_ok_status);
  CHECK_EQ(mapped.common().config_vector(), 0u);
  CHECK_EQ(mapped.common().queue(0u).vector, 1u);
  CHECK_EQ(mapped.common().queue(1u).vector, 2u);
  CHECK_EQ(mapped.common().queue(0u).enable, 1u);
  CHECK_EQ(mapped.common().queue(1u).enable, 1u); }

TEST(virtio_gpu_device_start, fewer_messages_than_sources_share_vector_zero)
{ const std::uint32_t counts[] = {1u, 2u};
  for (const std::uint32_t messages : counts) {
      MappedFunction mapped;
      CHECK_STATUS(mapped.map(), Status::ok);
      VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
      CHECK_STATUS(device.start(messages), Status::ok);
      CHECK_EQ(mapped.common().config_vector(), 0u);
      CHECK_EQ(mapped.common().queue(0u).vector, 0u);
      CHECK_EQ(mapped.common().queue(1u).vector, 0u);
  } }

TEST(virtio_gpu_device_start, a_line_interrupt_assigns_no_vector)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(0u), Status::ok);
  CHECK_EQ(mapped.common().config_vector(), no_vector);
  CHECK_EQ(mapped.common().queue(0u).vector, no_vector);
  CHECK_EQ(mapped.common().status(), driver_ok_status); }

TEST(virtio_gpu_device_start, a_device_without_version_1_is_not_supported_and_failed)
{ MappedFunction mapped;
  mapped.common().offer_features(FakeVirtioGpuFunction::feature_edid);
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(3u), Status::not_supported);
  CHECK((mapped.common().status() & failed_bit) != 0u); }

TEST(virtio_gpu_device_start, a_refused_vector_is_no_space_and_failed)
{ const std::uint32_t counts[] = {1u, 3u};
  for (const std::uint32_t messages : counts) {
      MappedFunction mapped;
      mapped.common().refuse_vectors();
      CHECK_STATUS(mapped.map(), Status::ok);
      VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
      CHECK_STATUS(device.start(messages), Status::no_space);
      CHECK((mapped.common().status() & failed_bit) != 0u);
  } }

TEST(virtio_gpu_device_start, a_device_with_only_a_control_queue_is_not_found)
{ MappedFunction mapped;
  mapped.common().set_queues(1u, 256u);
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(3u), Status::not_found);
  CHECK((mapped.common().status() & failed_bit) != 0u); }

TEST(virtio_gpu_device_start, each_allocation_failing_in_turn_fails_cleanly)
{ for (std::uint32_t nth = 1u; nth <= 8u; nth++) {
      MappedFunction mapped;
      CHECK_STATUS(mapped.map(), Status::ok);
      { VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
        mapped.platform().fail_allocation_at(nth);
        const Status status = device.start(3u);
        CHECK(status == Status::ok || status == Status::no_memory);
        CHECK(device.invariants_hold()); }
      mapped.platform().fail_allocation_at(0u);
      CHECK_EQ(mapped.platform().live_allocations(), 0u);
  } }

TEST(virtio_gpu_device_start, a_second_start_is_invalid_state)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(3u), Status::ok);
  CHECK_STATUS(device.start(3u), Status::invalid_state);
  CHECK_EQ(mapped.common().status(), driver_ok_status); }

TEST(virtio_gpu_device_start, is_passive_only)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  mapped.platform().set_irql(Irql::dispatch);
  static_cast<void>(device.start(3u));
  mapped.platform().set_irql(Irql::passive);
  CHECK(mapped.platform().claim(Violation::irql_ceiling));
  while (mapped.platform().claim(Violation::irql_ceiling)) {}
  while (mapped.platform().claim(Violation::allocation_at_dispatch)) {} }

TEST(virtio_gpu_device_stop, resets_the_device)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(3u), Status::ok);
  CHECK_STATUS(device.stop(), Status::ok);
  CHECK_EQ(mapped.common().status(), 0u);
  CHECK(device.features() == 0u);
  CHECK(device.invariants_hold()); }

TEST(virtio_gpu_device_stop, a_device_that_ignores_reset_is_invalid_state)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(3u), Status::ok);
  mapped.common().delay_reset(100000u);
  CHECK_STATUS(device.stop(), Status::invalid_state); }

TEST(virtio_gpu_device, destruction_writes_reset_before_the_queues_are_released)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  { VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
    CHECK_STATUS(device.start(3u), Status::ok); }
  CHECK_EQ(mapped.common().status(), 0u);
  CHECK_EQ(mapped.platform().live_allocations(), 0u); }

TEST(virtio_gpu_device_reset_immediately, writes_reset_from_an_interrupt)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(3u), Status::ok);
  const std::uint32_t allocations = mapped.platform().allocations_made();
  mapped.platform().set_irql(Irql::isr);
  device.reset_immediately();
  mapped.platform().set_irql(Irql::passive);
  CHECK_EQ(mapped.common().status(), 0u);
  CHECK_EQ(mapped.platform().allocations_made(), allocations);
  CHECK_EQ(mapped.platform().unclaimed_violations(), 0u); }

TEST(virtio_gpu_device_read_isr, returns_and_clears_the_isr_status)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  mapped.function().isr().raise(0x1u);
  mapped.platform().set_irql(Irql::isr);
  CHECK_EQ(device.read_isr(), 1u);
  CHECK_EQ(device.read_isr(), 0u);
  mapped.platform().set_irql(Irql::passive); }

TEST(virtio_gpu_device_harvest_control, collects_every_completed_request)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(3u), Status::ok);
  std::uint32_t completed = 9u;
  mapped.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(device.harvest_control(completed), Status::ok);
  CHECK_EQ(completed, 0u);
  mapped.platform().set_irql(Irql::passive);
  VirtioGpuDeviceProbe probe(device);
  std::uint16_t head = 0u;
  CHECK_STATUS(probe.control().add(request, 1u, head), Status::ok);
  CHECK_STATUS(probe.control().add(request, 1u, head), Status::ok);
  const FakeCommonConfig::Queue &queue = mapped.common().queue(0u);
  FakeVirtqueueDevice fake(mapped.platform(), queue.desc, queue.driver, queue.device, queue.size);
  CHECK_EQ(fake.complete_all(0u), 2u);
  mapped.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(device.harvest_control(completed), Status::ok);
  mapped.platform().set_irql(Irql::passive);
  CHECK_EQ(completed, 2u);
  CHECK_EQ(mapped.platform().unclaimed_violations(), 0u); }

TEST(virtio_gpu_device_harvest_control, a_lying_device_is_failed)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(3u), Status::ok);
  const FakeCommonConfig::Queue &queue = mapped.common().queue(0u);
  FakeVirtqueueDevice fake(mapped.platform(), queue.desc, queue.driver, queue.device, queue.size);
  fake.write_used(0u, 3u, 0u);
  fake.set_used_index(1u);
  std::uint32_t completed = 9u;
  CHECK_STATUS(device.harvest_control(completed), Status::invalid_state);
  CHECK_EQ(completed, 0u);
  CHECK((mapped.common().status() & failed_bit) != 0u); }

TEST(virtio_gpu_device_harvest_control, before_start_is_invalid_state)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  std::uint32_t completed = 9u;
  CHECK_STATUS(device.harvest_control(completed), Status::invalid_state);
  CHECK_EQ(completed, 0u); }

TEST(virtio_gpu_device_invariants, running_without_negotiated_features_is_detected)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK(device.invariants_hold());
  VirtioGpuDeviceProbe(device).set_running();
  CHECK(!device.invariants_hold()); }
