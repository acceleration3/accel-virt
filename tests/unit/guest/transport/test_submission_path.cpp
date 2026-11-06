#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_doorbell.hpp"
#include "guest/transport/submission_path.hpp"
#include "guest/transport/transport_ring.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/transport/recording_release_route.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/escape/release_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/submission/ring_header.hpp"

#include <cstdint>

using av::DoorbellStatus;
using av::GuestDoorbell;
using av::ReleaseRecord;
using av::RecordingReleaseRoute;
using av::SubmissionDescriptor;
using av::MockKmt;
using av::RingHeader;
using av::Status;
using av::SubmissionPath;
using av::TransportRing;

namespace av {
struct SubmissionPathProbe {
    static void count_a_doorbell_submission(SubmissionPath &path) { path.doorbell_submissions_++; }
};

}

namespace {

constexpr std::uint8_t  order = 12u;
constexpr std::size_t   ring_bytes = 32u + (1u << order);
constexpr std::uint64_t ring_allocation_bytes = 4096u;
constexpr std::size_t   awake_byte = 5u;

class Submitting {
public:
    Submitting()
    {
        kmt_.allow_hardware_scheduling();
        kmt_.add_adapter(av::PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
        std::uint32_t adapters[1] = {};
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(adapters, 1u, count), Status::ok);
        CHECK_STATUS(kmt_.create_device(adapters[0], device_), Status::ok);
        CHECK_STATUS(kmt_.create_context(device_, context_), Status::ok);
        allocation_ = allocate(av::AllocationKind::context_ring, ring_allocation_bytes);
        const std::uint32_t dma = allocate(av::AllocationKind::client, 4096u);
        std::uint32_t       fence_object = 0u;
        CHECK_STATUS(kmt_.create_paging_queue(device_, paging_queue_, fence_object), Status::ok);
        std::uint64_t fence = 0u;
        CHECK_STATUS(kmt_.make_resident(paging_queue_, dma, fence), Status::ok);
        CHECK_STATUS(kmt_.map_gpu_virtual_address(paging_queue_, dma, 1u, dma_address_, fence), Status::ok);
        CHECK_STATUS(ring_.initialize(ring_memory_, order), Status::ok);
    }
    Submitting(const Submitting &) = delete;
    Submitting &operator=(const Submitting &) = delete;

    MockKmt        &kmt() { return kmt_; }
    GuestDoorbell  &doorbell() { return doorbell_; }
    TransportRing  &ring() { return ring_; }
    SubmissionPath &path() { return path_; }
    std::uint32_t   context() const { return context_; }
    std::uint32_t   allocation() const { return allocation_; }

    std::uint64_t dma_address() const { return dma_address_; }

    Status open_doorbell()
    {
        const Status made = doorbell_.initialize(device_, context_, allocation_);
        return made != Status::ok ? made : doorbell_.connect();
    }

    Status write_batch(std::uint64_t fence)
    {
        std::uint8_t  payload[64] = {};
        std::uint64_t begin = 0u;
        std::uint64_t end = 0u;
        return ring_.write_batch(payload, sizeof payload, nullptr, 0u, fence, 0u, begin, end);
    }

    void consumer_awake(bool awake) { ring_memory_[awake_byte] = awake ? 1u : 0u; }

    std::uint64_t published_tail()
    {
        RingHeader header;
        CHECK_STATUS(header.decode(ring_memory_, ring_bytes), Status::ok);
        return header.tail();
    }

private:
    std::uint32_t allocate(av::AllocationKind kind, std::uint64_t bytes)
    {
        const av::AllocationDescriptor descriptor(kind, true, av::SurfaceLayout(), bytes, av::ScanoutTarget());
        std::uint8_t                   encoded[av::AllocationDescriptor::encoded_size] = {};
        CHECK_STATUS(descriptor.encode(encoded, sizeof encoded), Status::ok);
        std::uint32_t made = 0u;
        CHECK_STATUS(kmt_.create_allocation(device_, encoded, sizeof encoded, made), Status::ok);
        return made;
    }

    MockKmt        kmt_;
    std::uint32_t  device_ = 0u;
    std::uint32_t  context_ = 0u;
    std::uint32_t  allocation_ = 0u;
    std::uint32_t  paging_queue_ = 0u;
    std::uint64_t  dma_address_ = 0u;
    std::uint8_t   ring_memory_[ring_bytes] = {};
    TransportRing  ring_;
    GuestDoorbell  doorbell_{kmt_};
    SubmissionPath path_{kmt_, doorbell_, ring_};
};

}

TEST(submission_path, a_connected_doorbell_publishes_the_tail_and_makes_no_kernel_call)
{ Submitting submitting;
  CHECK(submitting.path().invariants_hold());
  CHECK_STATUS(submitting.open_doorbell(), Status::ok);
  const std::uint8_t descriptor[24] = {};
  CHECK_STATUS(submitting.write_batch(1u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 1u),
               Status::ok);
  CHECK_EQ(submitting.path().through_doorbell(), 1u);
  CHECK_EQ(submitting.path().through_kernel(), 0u);
  CHECK_EQ(submitting.published_tail(), submitting.ring().tail());
  CHECK_EQ(submitting.kmt().doorbell_page(submitting.doorbell().handle()), 1u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK(submitting.path().invariants_hold()); }

TEST(submission_path, a_disconnect_mid_stream_is_survived_by_submitting_the_other_way)
{ Submitting submitting;
  CHECK_STATUS(submitting.open_doorbell(), Status::ok);
  const std::uint8_t descriptor[24] = {};
  CHECK_STATUS(submitting.write_batch(1u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 1u),
               Status::ok);

  submitting.kmt().set_doorbell_status(submitting.doorbell().handle(),
                                       static_cast<std::uint32_t>(DoorbellStatus::disconnected_retry));
  submitting.kmt().clear_doorbell_page(submitting.doorbell().handle());
  CHECK_STATUS(submitting.write_batch(2u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 2u),
               Status::ok);
  CHECK_EQ(submitting.path().through_doorbell(), 1u);
  CHECK_EQ(submitting.path().through_kernel(), 1u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit_to_hw_queue), 1u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK_EQ(submitting.kmt().doorbell_page(submitting.doorbell().handle()), 0u);
  CHECK_EQ(submitting.published_tail(), submitting.ring().tail());

  submitting.kmt().set_doorbell_status(submitting.doorbell().handle(),
                                       static_cast<std::uint32_t>(DoorbellStatus::connected));
  CHECK_STATUS(submitting.write_batch(3u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 3u),
               Status::ok);
  CHECK_EQ(submitting.path().through_doorbell(), 2u);
  CHECK_EQ(submitting.path().through_kernel(), 1u);
  CHECK(submitting.path().invariants_hold()); }

TEST(submission_path, without_a_doorbell_every_submission_goes_through_the_kernel)
{ Submitting submitting;
  submitting.kmt().refuse_hardware_scheduling();
  CHECK_STATUS(submitting.open_doorbell(), Status::not_supported);
  const std::uint8_t descriptor[24] = {};
  for (std::uint64_t fence = 1u; fence <= 3u; fence++) {
      CHECK_STATUS(submitting.write_batch(fence), Status::ok);
      CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor,
                                            fence),
                   Status::ok);
  }
  CHECK_EQ(submitting.path().through_doorbell(), 0u);
  CHECK_EQ(submitting.path().through_kernel(), 3u);
  CHECK_EQ(submitting.published_tail(), 0u);
  CHECK(submitting.path().invariants_hold()); }

TEST(submission_path, a_refused_submission_is_counted_nowhere)
{ Submitting submitting;
  const std::uint8_t descriptor[24] = {};
  CHECK_STATUS(submitting.write_batch(1u), Status::ok);
  submitting.kmt().fail_at(MockKmt::Call::submit, 1u, MockKmt::invalid_parameter);
  CHECK(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 1u) !=
        Status::ok);
  CHECK_EQ(submitting.path().through_kernel(), 0u);
  CHECK_EQ(submitting.path().through_doorbell(), 0u);
  CHECK(submitting.path().invariants_hold()); }

TEST(submission_path, a_tail_that_cannot_be_published_stops_the_submission)
{ Submitting submitting;
  CHECK_STATUS(submitting.open_doorbell(), Status::ok);
  av::TransportRing  unbound;
  SubmissionPath     path(submitting.kmt(), submitting.doorbell(), unbound);
  const std::uint8_t descriptor[24] = {};
  CHECK_STATUS(path.submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 1u),
               Status::invalid_state);
  CHECK_EQ(path.through_doorbell(), 0u);
  CHECK_EQ(path.through_kernel(), 0u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK(path.invariants_hold()); }

TEST(submission_path, a_given_back_doorbell_leaves_a_hardware_queue_the_published_tail_is_not_for)
{ Submitting submitting;
  CHECK_STATUS(submitting.open_doorbell(), Status::ok);
  submitting.doorbell().give_back();
  submitting.path().publish_before_kernel();
  CHECK_STATUS(submitting.write_batch(1u), Status::ok);
  submitting.consumer_awake(true);
  const std::uint8_t descriptor[24] = {};
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 1u),
               Status::ok);
  CHECK_EQ(submitting.published_tail(), 0u);
  CHECK_EQ(submitting.path().through_kernel(), 1u);
  CHECK_EQ(submitting.path().through_doorbell(), 0u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit_to_hw_queue), 1u);
  CHECK(submitting.path().invariants_hold()); }

TEST(submission_path, a_published_tail_that_cannot_be_published_stops_the_submission_without_a_doorbell)
{ Submitting submitting;
  av::TransportRing  unbound;
  SubmissionPath     path(submitting.kmt(), submitting.doorbell(), unbound);
  path.publish_before_kernel();
  const std::uint8_t descriptor[24] = {};
  CHECK_STATUS(path.submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 1u),
               Status::invalid_state);
  CHECK_EQ(path.through_kernel(), 0u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 0u); }

TEST(submission_path_invariants, a_submission_counted_for_an_empty_ring_is_detected)
{ Submitting submitting;
  CHECK(submitting.path().invariants_hold());
  av::SubmissionPathProbe::count_a_doorbell_submission(submitting.path());
  CHECK(!submitting.path().invariants_hold());
  CHECK_STATUS(submitting.write_batch(1u), Status::ok);
  CHECK(submitting.path().invariants_hold()); }

TEST(submission_path, a_published_tail_skips_the_kernel_while_the_worker_is_awake)
{ Submitting submitting;
  submitting.kmt().refuse_hardware_scheduling();
  CHECK_STATUS(submitting.open_doorbell(), Status::not_supported);
  const std::uint8_t descriptor[24] = {};
  CHECK_STATUS(submitting.write_batch(1u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 1u),
               Status::ok);
  CHECK_EQ(submitting.published_tail(), 0u);
  CHECK_EQ(submitting.path().through_kernel(), 1u);

  submitting.path().publish_before_kernel();
  CHECK_STATUS(submitting.write_batch(2u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 2u),
               Status::ok);
  CHECK_EQ(submitting.published_tail(), submitting.ring().tail());
  CHECK_EQ(submitting.path().through_kernel(), 2u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 2u);

  submitting.consumer_awake(true);
  CHECK_STATUS(submitting.write_batch(3u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 3u),
               Status::ok);
  CHECK_EQ(submitting.published_tail(), submitting.ring().tail());
  CHECK_EQ(submitting.path().through_kernel(), 2u);
  CHECK_EQ(submitting.path().through_doorbell(), 0u);
  CHECK_EQ(submitting.path().submitted(), 3u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 2u);
  CHECK(submitting.path().invariants_hold()); }

namespace {

void describe(std::uint8_t (&bytes)[SubmissionDescriptor::encoded_size], std::uint64_t fence, std::uint64_t begin,
              std::uint64_t end)
{
    CHECK_STATUS(SubmissionDescriptor(fence, begin, end).encode(bytes, sizeof bytes), Status::ok);
}

}

TEST(submission_path, a_release_route_takes_the_submission_as_a_record_naming_the_ring_and_the_batch)
{ Submitting submitting;
  RecordingReleaseRoute route;
  CHECK(!submitting.path().releases());
  submitting.path().release_through(&route, 7u);
  CHECK(submitting.path().releases());
  std::uint8_t descriptor[SubmissionDescriptor::encoded_size] = {};
  describe(descriptor, 5u, 64u, 192u);
  CHECK_STATUS(submitting.write_batch(5u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 5u),
               Status::ok);
  CHECK_EQ(route.releases(), 1u);
  CHECK_EQ(route.last().ring(), 7u);
  CHECK_EQ(route.last().batch().fence_id(), 5u);
  CHECK_EQ(route.last().batch().begin(), 64u);
  CHECK_EQ(route.last().batch().end(), 192u);
  CHECK_EQ(submitting.path().through_release(), 1u);
  CHECK_EQ(submitting.path().submitted(), 1u);
  CHECK_EQ(submitting.path().through_kernel(), 0u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK(submitting.path().invariants_hold()); }

TEST(submission_path, a_route_that_does_not_support_the_submission_leaves_it_to_the_kernel)
{ Submitting submitting;
  RecordingReleaseRoute route;
  route.answer(Status::not_supported);
  submitting.path().release_through(&route, 1u);
  std::uint8_t descriptor[SubmissionDescriptor::encoded_size] = {};
  describe(descriptor, 1u, 0u, 32u);
  CHECK_STATUS(submitting.write_batch(1u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 1u),
               Status::ok);
  CHECK_EQ(route.releases(), 1u);
  CHECK_EQ(submitting.path().through_release(), 0u);
  CHECK_EQ(submitting.path().through_kernel(), 1u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 1u);
  CHECK(submitting.path().invariants_hold()); }

TEST(submission_path, a_route_that_fails_stops_the_submission_and_a_bad_descriptor_never_reaches_it)
{ Submitting submitting;
  RecordingReleaseRoute route;
  submitting.path().release_through(&route, 1u);
  std::uint8_t descriptor[SubmissionDescriptor::encoded_size] = {};
  describe(descriptor, 1u, 0u, 32u);
  route.answer(Status::conflict);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 1u),
               Status::conflict);
  CHECK_EQ(submitting.path().submitted(), 0u);
  const std::uint8_t garbage[24] = {};
  route.answer(Status::ok);
  const std::uint32_t asked = route.releases();
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), garbage, sizeof garbage, 2u),
               Status::invalid_parameter);
  CHECK_EQ(route.releases(), asked);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 0u); }

TEST(submission_path, releasing_through_nothing_returns_the_path_to_the_doorbell_and_the_kernel)
{ Submitting submitting;
  RecordingReleaseRoute route;
  submitting.path().release_through(&route, 3u);
  submitting.path().release_through(nullptr, 3u);
  CHECK(!submitting.path().releases());
  const std::uint8_t descriptor[24] = {};
  CHECK_STATUS(submitting.write_batch(1u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 1u),
               Status::ok);
  CHECK_EQ(route.releases(), 0u);
  CHECK_EQ(submitting.path().through_release(), 0u);
  CHECK_EQ(submitting.path().through_kernel(), 1u);
  CHECK(submitting.path().invariants_hold()); }
