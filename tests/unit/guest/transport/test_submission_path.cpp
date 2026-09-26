/* Which way a submission goes (feature 049, US1 and US2), over MockKmt.
 *
 * Both ways are the same submission - the same batch, in the same ring, at the
 * same tail - and what decides is the doorbell's status page, read per
 * submission. What these tests judge is that a disconnect mid-stream is
 * survived without the kernel being told anything it does not already know,
 * and that the connected path makes no kernel call at all.
 */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_doorbell.hpp"
#include "guest/transport/submission_path.hpp"
#include "guest/transport/transport_ring.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/ring_header.hpp"

#include <cstdint>

using av::DoorbellStatus;
using av::GuestDoorbell;
using av::MockKmt;
using av::RingHeader;
using av::Status;
using av::SubmissionPath;
using av::TransportRing;

namespace av {

/* A submission counted for a ring nothing was written into: nothing reachable
 * does that, so the invariant is broken here to see it caught. */
struct SubmissionPathProbe {
    static void count_a_doorbell_submission(SubmissionPath &path) { path.doorbell_submissions_++; }
};

}  // namespace av

namespace {

constexpr std::uint8_t  order = 12u;
constexpr std::size_t   ring_bytes = 32u + (1u << order);
/* What the ring allocation is asked for: whole pages, as a transport's is. */
constexpr std::uint64_t ring_allocation_bytes = 8192u;
/* The header's byte where the device's worker says it is watching the ring. */
constexpr std::size_t   awake_byte = 5u;

/* A context with a ring, a doorbell on it, and the path that chooses. */
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
        /* A DMA buffer mapped into the device's address space, as the kernel
         * path's submission names. */
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

    /* One batch written into the ring, and the descriptor the kernel path
     * would carry. */
    Status write_batch(std::uint64_t fence)
    {
        std::uint8_t  payload[64] = {};
        std::uint64_t begin = 0u;
        std::uint64_t end = 0u;
        return ring_.write_batch(payload, sizeof payload, fence, 0u, begin, end);
    }

    /* Play the device's worker saying whether it is watching the ring. */
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

}  // namespace

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
  /* The process published its own tail - on the kernel path the miniport would
   * have - and the store is what tells the device to read it. */
  CHECK_EQ(submitting.published_tail(), submitting.ring().tail());
  CHECK_EQ(submitting.kmt().doorbell_page(submitting.doorbell().handle()), 1u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK(submitting.path().invariants_hold()); }

/* US2: dxgkrnl disconnects the doorbell between two submissions. The next one
 * goes through the kernel, carrying the same descriptor at the same tail, and
 * the one after it goes back through the doorbell once it reconnects. */
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
  /* Through the queue, not through the context: a hardware-scheduled context
   * refuses D3DKMTSubmitCommand outright (research.md §13). */
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit_to_hw_queue), 1u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 0u);
  /* Nothing was stored to the page that was disconnected. */
  CHECK_EQ(submitting.kmt().doorbell_page(submitting.doorbell().handle()), 0u);
  /* And the tail the process published before it learnt of the disconnect is
   * the tail the kernel path submits: the same batch, once (US2 scenario 2). */
  CHECK_EQ(submitting.published_tail(), submitting.ring().tail());

  submitting.kmt().set_doorbell_status(submitting.doorbell().handle(),
                                       static_cast<std::uint32_t>(DoorbellStatus::connected));
  CHECK_STATUS(submitting.write_batch(3u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 3u),
               Status::ok);
  CHECK_EQ(submitting.path().through_doorbell(), 2u);
  CHECK_EQ(submitting.path().through_kernel(), 1u);
  CHECK(submitting.path().invariants_hold()); }

/* A machine whose OS gives out no hardware queue has no doorbell to choose,
 * and every submission goes the way it always did (research.md §11). */
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
  /* The miniport publishes the tail on that path, so the process left the
   * header alone. */
  CHECK_EQ(submitting.published_tail(), 0u);
  CHECK(submitting.path().invariants_hold()); }

/* A kernel submission the driver refuses is the caller's failure, and nothing
 * is counted for it. */
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

/* A ring that was never bound cannot publish a tail, so the submission stops
 * there: nothing is stored and nothing goes through the kernel either, because
 * what the kernel would carry is a tail nobody wrote. */
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

/* The invariant itself: every submission counted is a batch this process wrote
 * into its ring. */
TEST(submission_path_invariants, a_submission_counted_for_an_empty_ring_is_detected)
{ Submitting submitting;
  CHECK(submitting.path().invariants_hold());
  av::SubmissionPathProbe::count_a_doorbell_submission(submitting.path());
  CHECK(!submitting.path().invariants_hold());
  /* A batch written puts the ring where that count says it should be. */
  CHECK_STATUS(submitting.write_batch(1u), Status::ok);
  CHECK(submitting.path().invariants_hold()); }

/* A process that publishes its own tail leaves the kernel out of a submission
 * the device's worker is already watching for; a worker asleep is woken the
 * way it always was, through the kernel, with the tail already published. */
TEST(submission_path, a_published_tail_skips_the_kernel_while_the_worker_is_awake)
{ Submitting submitting;
  submitting.kmt().refuse_hardware_scheduling();
  CHECK_STATUS(submitting.open_doorbell(), Status::not_supported);
  const std::uint8_t descriptor[24] = {};
  /* Before it is asked to, the path publishes nothing itself: the miniport does. */
  CHECK_STATUS(submitting.write_batch(1u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 1u),
               Status::ok);
  CHECK_EQ(submitting.published_tail(), 0u);
  CHECK_EQ(submitting.path().through_kernel(), 1u);

  submitting.path().publish_before_kernel();
  /* Asleep: the tail is published and the kernel still asked, to wake it. */
  CHECK_STATUS(submitting.write_batch(2u), Status::ok);
  CHECK_STATUS(submitting.path().submit(submitting.context(), submitting.dma_address(), descriptor, sizeof descriptor, 2u),
               Status::ok);
  CHECK_EQ(submitting.published_tail(), submitting.ring().tail());
  CHECK_EQ(submitting.path().through_kernel(), 2u);
  CHECK_EQ(submitting.kmt().calls(MockKmt::Call::submit), 2u);

  /* Awake: published, counted, and no kernel call. */
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
