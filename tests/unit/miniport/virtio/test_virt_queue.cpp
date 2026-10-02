#include "core/support/status.hpp"
#include "miniport/virtio/virt_queue.hpp"
#include "miniport/virtio/virtio_pci_transport.hpp"
#include "miniport/virtio/virtq_segment.hpp"
#include "harness/virtio/fake_common_config.hpp"
#include "harness/virtio/fake_isr_register.hpp"
#include "harness/virtio/fake_register_bytes.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::FakeCommonConfig;
using av::FakeIsrRegister;
using av::FakeRegisterBytes;
using av::FakeVirtqueueDevice;
using av::Irql;
using av::MockPlatform;
using av::SegmentDirection;
using av::Status;
using av::Violation;
using av::VirtioPciTransport;
using av::VirtQueue;
using av::VirtqSegment;

namespace av {

struct VirtQueueProbe {
    explicit VirtQueueProbe(VirtQueue &target) : queue(target) {}
    void set_free_count(std::uint16_t count) { queue.free_count_ = count; }
    void set_avail_index(std::uint16_t index) { queue.avail_index_ = index; }
    void set_size(std::uint16_t size) { queue.size_ = size; }
    VirtQueue &queue;
};

}  // namespace av

using av::VirtQueueProbe;

namespace {

const VirtqSegment request_and_response[] = {
    VirtqSegment(0x5000u, 24u, SegmentDirection::device_reads),
    VirtqSegment(0x6000u, 64u, SegmentDirection::device_writes),
};

class Queue {
public:
    explicit Queue(std::uint16_t size) : queue_(platform_, 0u)
    {
        if (queue_.initialize(size) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "queue failed to initialize");
    }

    FakeVirtqueueDevice device()
    {
        return FakeVirtqueueDevice(platform_, queue_.descriptor_area(), queue_.driver_area(),
                                   queue_.device_area(), queue_.size());
    }

    MockPlatform &platform() { return platform_; }
    VirtQueue    &queue() { return queue_; }

private:
    MockPlatform platform_;
    VirtQueue    queue_;
};

}  // namespace

TEST(virt_queue_initialize, allocates_a_ring_of_the_device_size_capped_by_the_driver)
{ Queue small(64u);
  CHECK_EQ(small.queue().size(), 64u);
  CHECK_EQ(small.queue().free_descriptors(), 64u);
  CHECK(small.queue().invariants_hold());
  CHECK(small.queue().descriptor_area() != 0u);
  CHECK(small.queue().driver_area() == small.queue().descriptor_area() + 16u * 64u);
  CHECK((small.queue().device_area() & 3u) == 0u);
  CHECK(small.queue().device_area() >= small.queue().driver_area() + 6u + 2u * 64u);
  Queue huge(32768u);
  CHECK_EQ(huge.queue().size(), VirtQueue::max_size); }

TEST(virt_queue_initialize, rejects_sizes_that_are_not_powers_of_two)
{ MockPlatform platform;
  const std::uint16_t bad[] = {0u, 3u, 100u, 0xFFFFu};
  for (const std::uint16_t size : bad) {
      VirtQueue queue(platform, 1u);
      CHECK_STATUS(queue.initialize(size), Status::malformed);
      CHECK(queue.invariants_hold());
  } }

TEST(virt_queue_initialize, a_second_initialize_is_invalid_state)
{ Queue queue(8u);
  CHECK_STATUS(queue.queue().initialize(8u), Status::invalid_state); }

TEST(virt_queue_initialize, every_allocation_failure_is_clean)
{ for (std::uint32_t nth = 1u; nth <= 2u; nth++) {
      MockPlatform platform;
      { VirtQueue queue(platform, 0u);
        platform.fail_allocation_at(nth);
        CHECK_STATUS(queue.initialize(16u), Status::no_memory);
        CHECK(queue.invariants_hold()); }
      CHECK_EQ(platform.live_allocations(), 0u);
  } }

TEST(virt_queue_add, publishes_a_chain_the_device_can_follow)
{ Queue queue(8u);
  std::uint16_t head = 0xFFFFu;
  CHECK_STATUS(queue.queue().add(request_and_response, 2u, head), Status::ok);
  CHECK(queue.queue().invariants_hold());
  CHECK_EQ(queue.queue().free_descriptors(), 6u);
  FakeVirtqueueDevice device = queue.device();
  CHECK_EQ(device.available_index(), 1u);
  CHECK_EQ(device.head(0u), head);
  CHECK_EQ(device.complete_all(40u), 1u);
  CHECK_EQ(device.seen_count(), 2u);
  CHECK(device.seen(0u).address == 0x5000u);
  CHECK_EQ(device.seen(0u).length, 24u);
  CHECK_EQ(device.seen(0u).flags, 1u);    /* NEXT */
  CHECK(device.seen(1u).address == 0x6000u);
  CHECK_EQ(device.seen(1u).flags, 2u);    /* WRITE, last */ }

TEST(virt_queue_take_used, returns_the_head_and_written_length_and_frees_the_chain)
{ Queue queue(8u);
  std::uint16_t head = 0u;
  CHECK_STATUS(queue.queue().add(request_and_response, 2u, head), Status::ok);
  std::uint16_t done = 0u; std::uint32_t written = 0u;
  CHECK_STATUS(queue.queue().take_used(done, written), Status::not_found);
  FakeVirtqueueDevice device = queue.device();
  device.complete_all(40u);
  CHECK_STATUS(queue.queue().take_used(done, written), Status::ok);
  CHECK(queue.queue().invariants_hold());
  CHECK_EQ(done, head);
  CHECK_EQ(written, 40u);
  CHECK_EQ(queue.queue().free_descriptors(), 8u);
  CHECK_STATUS(queue.queue().take_used(done, written), Status::not_found); }

TEST(virt_queue_add, a_full_queue_is_no_space_until_completions_free_it)
{ Queue queue(4u);
  std::uint16_t head = 0u;
  CHECK_STATUS(queue.queue().add(request_and_response, 2u, head), Status::ok);
  CHECK_STATUS(queue.queue().add(request_and_response, 2u, head), Status::ok);
  CHECK_STATUS(queue.queue().add(request_and_response, 1u, head), Status::no_space);
  FakeVirtqueueDevice device = queue.device();
  CHECK_EQ(device.available_index(), 2u);
  device.complete_all(0u);
  std::uint16_t done = 0u; std::uint32_t written = 0u;
  CHECK_STATUS(queue.queue().take_used(done, written), Status::ok);
  CHECK_STATUS(queue.queue().add(request_and_response, 2u, head), Status::ok);
  CHECK(queue.queue().invariants_hold()); }

TEST(virt_queue_add, rejects_an_empty_chain_and_an_unrepresentable_writable_total)
{ Queue queue(8u);
  std::uint16_t head = 0u;
  CHECK_STATUS(queue.queue().add(request_and_response, 0u, head), Status::invalid_parameter);
  const VirtqSegment huge[] = {
      VirtqSegment(0x1000u, 0xFFFFFFFFu, SegmentDirection::device_writes),
      VirtqSegment(0x2000u, 1u, SegmentDirection::device_writes),
  };
  CHECK_STATUS(queue.queue().add(huge, 2u, head), Status::invalid_parameter);
  CHECK_EQ(queue.queue().free_descriptors(), 8u); }

TEST(virt_queue, survives_the_16_bit_index_wrapping)
{ Queue queue(2u);
  FakeVirtqueueDevice device = queue.device();
  std::uint16_t head = 0u, done = 0u; std::uint32_t written = 0u;
  for (std::uint32_t round = 0u; round < 70000u; round++) {
      if (queue.queue().add(request_and_response, 1u, head) != Status::ok) { CHECK(false); break; }
      device.complete_all(0u);
      if (queue.queue().take_used(done, written) != Status::ok || done != head) { CHECK(false); break; }
  }
  CHECK_EQ(queue.queue().free_descriptors(), 2u);
  CHECK(queue.queue().invariants_hold()); }

TEST(virt_queue_take_used, hostile_elements_are_rejected_and_the_queue_stays_usable)
{ struct Case { std::uint32_t id; std::uint32_t length; };
  const Case cases[] = {
      {8u, 0u},          /* id out of range */
      {1u, 0u},          /* id is not a head in flight (head is 0) */
      {0u, 65u},         /* longer than the chain's writable bytes */
  };
  for (const Case &c : cases) {
      Queue queue(8u);
      std::uint16_t head = 0u;
      CHECK_STATUS(queue.queue().add(request_and_response, 2u, head), Status::ok);
      CHECK_EQ(head, 0u);
      FakeVirtqueueDevice device = queue.device();
      device.write_used(0u, c.id, c.length);
      device.set_used_index(1u);
      std::uint16_t done = 0u; std::uint32_t written = 0u;
      CHECK_STATUS(queue.queue().take_used(done, written), Status::invalid_state);
      CHECK(queue.queue().invariants_hold());
      CHECK_STATUS(queue.queue().take_used(done, written), Status::not_found);
      CHECK_STATUS(queue.queue().add(request_and_response, 2u, head), Status::ok);
  } }

TEST(virt_queue_take_used, a_used_index_past_what_is_in_flight_is_rejected)
{ Queue queue(8u);
  std::uint16_t head = 0u;
  CHECK_STATUS(queue.queue().add(request_and_response, 1u, head), Status::ok);
  FakeVirtqueueDevice device = queue.device();
  device.set_used_index(2u);
  std::uint16_t done = 0u; std::uint32_t written = 0u;
  CHECK_STATUS(queue.queue().take_used(done, written), Status::invalid_state);
  device.set_used_index(9u);
  CHECK_STATUS(queue.queue().take_used(done, written), Status::invalid_state);
  CHECK(queue.queue().invariants_hold()); }

TEST(virt_queue, add_and_take_used_are_dispatch_safe)
{ Queue queue(8u);
  queue.platform().set_irql(Irql::dispatch);
  std::uint16_t head = 0u, done = 0u; std::uint32_t written = 0u;
  CHECK_STATUS(queue.queue().add(request_and_response, 2u, head), Status::ok);
  queue.device().complete_all(1u);
  CHECK_STATUS(queue.queue().take_used(done, written), Status::ok);
  CHECK_EQ(queue.platform().unclaimed_violations(), 0u);
  queue.platform().set_irql(Irql::passive); }

TEST(virt_queue, an_uninitialized_queue_refuses_work)
{ MockPlatform platform; VirtQueue queue(platform, 3u);
  CHECK_EQ(queue.index(), 3u);
  std::uint16_t head = 0u, done = 0u; std::uint32_t written = 0u;
  CHECK_STATUS(queue.add(request_and_response, 1u, head), Status::invalid_state);
  CHECK_STATUS(queue.take_used(done, written), Status::invalid_state); }

TEST(virtq_segment, reports_what_it_was_given)
{ const VirtqSegment segment(0x1234u, 99u, SegmentDirection::device_writes);
  CHECK(segment.physical_address() == 0x1234u);
  CHECK_EQ(segment.length(), 99u);
  CHECK(segment.direction() == SegmentDirection::device_writes); }

TEST(virt_queue_bind_notify_offset, records_where_to_notify)
{ Queue queue(4u);
  queue.queue().bind_notify_offset(0x40u);
  CHECK(queue.queue().notify_offset() == 0x40u); }

TEST(virt_queue_invariants, corruption_is_detected)
{ Queue queue(8u);
  VirtQueueProbe probe(queue.queue());
  probe.set_free_count(9u);
  CHECK(!queue.queue().invariants_hold());
  probe.set_free_count(8u);
  probe.set_avail_index(9u);
  CHECK(!queue.queue().invariants_hold());
  probe.set_avail_index(0u);
  probe.set_free_count(0u);
  probe.set_size(0u);
  CHECK(!queue.queue().invariants_hold());
  probe.set_size(8u);
  probe.set_free_count(8u); }

/* ---- activation through the transport -------------------------------------- */

namespace {

class Transport {
public:
    Transport() : notify_(0x1000u), device_(16u), transport_(platform_, common_, notify_, isr_, device_, 4u)
    {
        common_.offer_features(VirtioPciTransport::feature_version_1);
        common_.set_queues(2u, 16u);
        common_.set_queue_notify_off(1u, 1u);
    }
    MockPlatform       &platform() { return platform_; }
    FakeCommonConfig   &common() { return common_; }
    FakeRegisterBytes  &notify() { return notify_; }
    VirtioPciTransport &transport() { return transport_; }
    Status negotiate() { return transport_.negotiate(0u, VirtioPciTransport::feature_version_1); }

private:
    MockPlatform       platform_;
    FakeCommonConfig   common_;
    FakeRegisterBytes  notify_;
    FakeIsrRegister    isr_;
    FakeRegisterBytes  device_;
    VirtioPciTransport transport_;
};

}  // namespace

TEST(virtio_pci_transport_activate_queue, programs_the_device_and_enables_the_queue)
{ Transport t;
  CHECK_STATUS(t.negotiate(), Status::ok);
  VirtQueue queue(t.platform(), 1u);
  CHECK_STATUS(t.transport().activate_queue(queue, 3u), Status::ok);
  const FakeCommonConfig::Queue &registers = t.common().queue(1u);
  CHECK_EQ(registers.size, 16u);
  CHECK_EQ(registers.vector, 3u);
  CHECK_EQ(registers.enable, 1u);
  CHECK(registers.desc == queue.descriptor_area());
  CHECK(registers.driver == queue.driver_area());
  CHECK(registers.device == queue.device_area());
  CHECK(queue.notify_offset() == 4u);
  t.transport().notify(queue);
  CHECK(t.notify().last_write_offset() == 4u);
  CHECK(t.notify().last_write_value() == 1u);
  CHECK(t.transport().invariants_hold()); }

TEST(virtio_pci_transport_activate_queue, requires_negotiation)
{ Transport t;
  VirtQueue queue(t.platform(), 0u);
  CHECK_STATUS(t.transport().activate_queue(queue, 0u), Status::invalid_state); }

TEST(virtio_pci_transport_activate_queue, an_absent_queue_is_not_found)
{ Transport t;
  CHECK_STATUS(t.negotiate(), Status::ok);
  VirtQueue beyond(t.platform(), 2u);
  CHECK_STATUS(t.transport().activate_queue(beyond, 0u), Status::not_found);
  t.common().set_queue_max_size(0u, 0u);
  VirtQueue empty(t.platform(), 0u);
  CHECK_STATUS(t.transport().activate_queue(empty, 0u), Status::not_found); }

TEST(virtio_pci_transport_activate_queue, a_hostile_queue_size_is_malformed)
{ Transport t;
  CHECK_STATUS(t.negotiate(), Status::ok);
  t.common().set_queue_max_size(0u, 100u);
  VirtQueue queue(t.platform(), 0u);
  CHECK_STATUS(t.transport().activate_queue(queue, 0u), Status::malformed); }

TEST(virtio_pci_transport_activate_queue, a_refused_vector_is_no_space)
{ Transport t;
  CHECK_STATUS(t.negotiate(), Status::ok);
  t.common().refuse_vectors();
  VirtQueue queue(t.platform(), 0u);
  CHECK_STATUS(t.transport().activate_queue(queue, 1u), Status::no_space); }

TEST(virtio_pci_transport_activate_queue, a_notify_offset_outside_the_region_is_malformed)
{ Transport t;
  CHECK_STATUS(t.negotiate(), Status::ok);
  t.common().set_queue_notify_off(0u, 0x400u);
  VirtQueue queue(t.platform(), 0u);
  CHECK_STATUS(t.transport().activate_queue(queue, 0u), Status::malformed); }

TEST(virtio_pci_transport_activate_queue, a_queue_that_will_not_enable_is_invalid_state)
{ Transport t;
  CHECK_STATUS(t.negotiate(), Status::ok);
  t.common().refuse_queue_enable();
  VirtQueue queue(t.platform(), 0u);
  CHECK_STATUS(t.transport().activate_queue(queue, 0u), Status::invalid_state); }

TEST(virtio_pci_transport_activate_queue, propagates_queue_allocation_failure)
{ Transport t;
  CHECK_STATUS(t.negotiate(), Status::ok);
  t.platform().fail_allocation_at(1u);
  VirtQueue queue(t.platform(), 0u);
  CHECK_STATUS(t.transport().activate_queue(queue, 0u), Status::no_memory); }

TEST(virtio_pci_transport_activate_queue, a_notify_region_too_small_for_any_notification_is_malformed)
{ MockPlatform platform;
  FakeCommonConfig common;
  FakeRegisterBytes notify(1u);
  FakeIsrRegister isr;
  FakeRegisterBytes device(16u);
  VirtioPciTransport transport(platform, common, notify, isr, device, 0u);
  common.offer_features(VirtioPciTransport::feature_version_1);
  common.set_queues(1u, 8u);
  CHECK_STATUS(transport.negotiate(0u, VirtioPciTransport::feature_version_1), Status::ok);
  VirtQueue queue(platform, 0u);
  CHECK_STATUS(transport.activate_queue(queue, 0u), Status::malformed); }

TEST(virtio_pci_transport_activate_queue, a_queue_size_above_the_virtio_maximum_is_malformed)
{ Transport t;
  CHECK_STATUS(t.negotiate(), Status::ok);
  t.common().set_queue_max_size(0u, 0xFFFFu);
  VirtQueue queue(t.platform(), 0u);
  CHECK_STATUS(t.transport().activate_queue(queue, 0u), Status::malformed); }
