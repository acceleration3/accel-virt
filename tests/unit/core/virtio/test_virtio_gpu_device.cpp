#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/pci/bar_mappings.hpp"
#include "core/pci/pci_base_addresses.hpp"
#include "core/pci/resource_list.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virt_queue.hpp"
#include "core/virtio/virtio_capability_walk.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "core/virtio/virtio_pci_layout.hpp"
#include "core/virtio/virtq_segment.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

namespace av {

struct VirtioGpuDeviceProbe {
    explicit VirtioGpuDeviceProbe(VirtioGpuDevice &target) : device(target) {}
    GpuCommandQueue &control() { return device.control_; }
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

using MappedFunction = av::MappedVirtioFunction;

constexpr std::uint32_t ok_nodata = 0x1100u;
/* A TRANSFER_TO_HOST_2D request's type and padding: what the device reads first. */
const std::uint8_t request_bytes[32] = {0x05, 0x01};

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
  CHECK_STATUS(device.send_control(request_bytes, sizeof request_bytes), Status::ok);
  CHECK_STATUS(device.send_control(request_bytes, sizeof request_bytes), Status::ok);
  FakeVirtqueueDevice fake = mapped.queue_device(0u);
  CHECK_EQ(fake.respond_all(ok_nodata), 2u);
  CHECK_EQ(fake.request_type(1u), 0x0105u);
  mapped.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(device.harvest_control(completed), Status::ok);
  mapped.platform().set_irql(Irql::passive);
  CHECK_EQ(completed, 2u);
  CHECK_EQ(mapped.platform().logged(), 0u);
  CHECK_EQ(mapped.platform().unclaimed_violations(), 0u); }

TEST(virtio_gpu_device_harvest_control, a_refused_request_is_logged_not_failed)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(3u), Status::ok);
  CHECK_STATUS(device.send_control(request_bytes, sizeof request_bytes), Status::ok);
  FakeVirtqueueDevice fake = mapped.queue_device(0u);
  CHECK_EQ(fake.respond_all(0x1203u), 1u);
  std::uint32_t completed = 0u;
  CHECK_STATUS(device.harvest_control(completed), Status::ok);
  CHECK_EQ(completed, 1u);
  CHECK_EQ(mapped.platform().logged(), 1u);
  CHECK_EQ(mapped.common().status(), driver_ok_status); }

TEST(virtio_gpu_device_harvest_cursor, collects_cursor_requests_that_carry_no_response)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  std::uint32_t completed = 9u;
  CHECK_STATUS(device.send_cursor(request_bytes, sizeof request_bytes), Status::invalid_state);
  CHECK_STATUS(device.harvest_cursor(completed), Status::invalid_state);
  CHECK_STATUS(device.start(3u), Status::ok);
  CHECK_STATUS(device.send_cursor(request_bytes, sizeof request_bytes), Status::ok);
  FakeVirtqueueDevice fake = mapped.queue_device(1u);
  CHECK_EQ(fake.complete_all(0u), 1u);
  CHECK_EQ(fake.seen_count(), 1u);
  mapped.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(device.harvest_cursor(completed), Status::ok);
  mapped.platform().set_irql(Irql::passive);
  CHECK_EQ(completed, 1u);
  CHECK_STATUS(device.stop(), Status::ok);
  CHECK_STATUS(device.send_control(request_bytes, sizeof request_bytes), Status::invalid_state); }

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
  CHECK((mapped.common().status() & failed_bit) != 0u);
  CHECK_STATUS(device.harvest_cursor(completed), Status::invalid_state); }

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

/* A device that is not running has nothing to ask. */
TEST(virtio_gpu_device_ask_control, needs_a_running_device)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  std::uint8_t  request[24] = {};
  std::uint8_t  answer[64] = {};
  std::uint32_t written = 0u;
  CHECK_STATUS(device.ask_control(request, sizeof request, answer, sizeof answer, written), Status::invalid_state);
  CHECK_STATUS(device.start(3u), Status::ok);
  /* Running, but the device answers nothing. */
  CHECK_STATUS(device.ask_control(request, sizeof request, answer, sizeof answer, written), Status::not_found); }

/* The displays the device has, from its configuration: what decides how many
 * VidPn sources the driver offers. */
TEST(virtio_gpu_device_scanouts, are_what_the_device_configuration_says)
{ MappedFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  mapped.function().device_config().set_u32(8u, 2u);   /* num_scanouts */
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_EQ(device.scanouts(), 0u);
  CHECK_STATUS(device.start(3u), Status::ok);
  CHECK_EQ(device.scanouts(), 2u);
  CHECK_STATUS(device.stop(), Status::ok);
  CHECK_EQ(device.scanouts(), 0u); }

/* Feature 043: each context's fence queue gets a message of its own while the
 * grant lasts, and shares the granted ones round the doorbells past it. */
TEST(virtio_gpu_device_connect_fence_queue, gives_each_doorbell_a_message_past_the_sources)
{ MappedFunction mapped;
  mapped.function().add_doorbell_region();
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  std::uint32_t message = 99u;
  CHECK_STATUS(device.connect_fence_queue(0u, message), Status::invalid_state);   /* not running */
  CHECK_EQ(message, 0u);
  CHECK_STATUS(device.start(6u), Status::ok);
  const std::uint32_t expected[] = {3u, 4u, 5u, 3u};   /* three granted past the sources */
  for (std::uint32_t doorbell = 0u; doorbell < 4u; doorbell++) {
      CHECK_STATUS(device.connect_fence_queue(doorbell, message), Status::ok);
      CHECK_EQ(message, expected[doorbell]);
      CHECK_EQ(mapped.common().queue(static_cast<std::uint16_t>(2u + doorbell)).vector, expected[doorbell]);
  }
  CHECK_STATUS(device.connect_fence_queue(64u, message), Status::not_found);   /* queue 66 */
  CHECK_STATUS(device.connect_fence_queue(0xFFFFu, message), Status::not_found);
  CHECK(device.invariants_hold()); }

TEST(virtio_gpu_device_connect_fence_queue, shares_vector_zero_or_assigns_nothing_when_little_was_granted)
{ const std::uint32_t grants[] = {3u, 1u, 0u};
  for (const std::uint32_t messages : grants) {
      MappedFunction mapped;
      mapped.function().add_doorbell_region();
      CHECK_STATUS(mapped.map(), Status::ok);
      VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
      CHECK_STATUS(device.start(messages), Status::ok);
      std::uint32_t message = 99u;
      CHECK_STATUS(device.connect_fence_queue(5u, message), Status::ok);
      CHECK_EQ(message, 0u);
      if (messages != 0u) CHECK_EQ(mapped.common().queue(7u).vector, 0u);
      CHECK_STATUS(device.connect_fence_queue(64u, message), Status::not_found);
  } }

TEST(virtio_gpu_device_connect_fence_queue, a_refused_vector_is_no_space)
{ MappedFunction mapped;
  mapped.function().add_doorbell_region();
  CHECK_STATUS(mapped.map(), Status::ok);
  VirtioGpuDevice device(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device.start(8u), Status::ok);
  mapped.common().refuse_vectors();
  std::uint32_t message = 0u;
  CHECK_STATUS(device.connect_fence_queue(1u, message), Status::no_space); }
