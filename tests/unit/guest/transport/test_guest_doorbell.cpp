/* The doorbell a process submits through (feature 049, US1), over MockKmt.
 *
 * What it owns is three mappings the kernel handed out once: the page a store
 * lands in, the status a process reads before every store, and where it writes
 * the fence value it has queued. What these tests judge is that it reads the
 * status *every* time - a doorbell disconnected between two submissions must
 * not be stored to - and that a machine whose OS gives out no hardware queue
 * leaves the process with nothing but the old path.
 */
#include "core/support/status.hpp"
#include "guest/transport/guest_doorbell.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstdint>

namespace av {

/* The mappings a doorbell holds, taken away one at a time: nothing reachable
 * leaves it half-mapped or on no queue, so a test of those invariants has to
 * do it itself. */
struct GuestDoorbellProbe {
    explicit GuestDoorbellProbe(GuestDoorbell &doorbell)
        : doorbell_(doorbell), page_(doorbell.page_), status_(doorbell.status_), queued_(doorbell.queued_),
          queue_(doorbell.queue_)
    {
    }
    void forget_page() { doorbell_.page_ = nullptr; }
    void restore_page() { doorbell_.page_ = page_; }
    void forget_status() { doorbell_.status_ = nullptr; }
    void restore_status() { doorbell_.status_ = status_; }
    void forget_queued() { doorbell_.queued_ = nullptr; }
    void restore_queued() { doorbell_.queued_ = queued_; }
    void forget_queue() { doorbell_.queue_ = 0u; }
    void restore_queue() { doorbell_.queue_ = queue_; }

    GuestDoorbell           &doorbell_;
    volatile uint32_t       *page_;
    const volatile uint32_t *status_;
    volatile uint64_t       *queued_;
    uint32_t                 queue_;
};

}  // namespace av

using av::DoorbellStatus;
using av::GuestDoorbell;
using av::MockKmt;
using av::Status;

namespace {

/* A device, a context and a ring allocation, as a transport makes them. */
class Made {
public:
    Made()
    {
        kmt_.allow_hardware_scheduling();
        kmt_.add_adapter(av::PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
        std::uint32_t adapters[1] = {};
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(adapters, 1u, count), Status::ok);
        adapter_ = adapters[0];
        CHECK_STATUS(kmt_.create_device(adapter_, device_), Status::ok);
        CHECK_STATUS(kmt_.create_context(device_, context_), Status::ok);
        /* A ring allocation of one page, as a transport's is. */
        const av::AllocationDescriptor descriptor(av::AllocationKind::context_ring, true, av::SurfaceLayout(),
                                                  4096u, av::ScanoutTarget());
        std::uint8_t                   bytes[av::AllocationDescriptor::encoded_size] = {};
        CHECK_STATUS(descriptor.encode(bytes, sizeof bytes), Status::ok);
        CHECK_STATUS(kmt_.create_allocation(device_, bytes, sizeof bytes, ring_), Status::ok);
    }
    Made(const Made &) = delete;
    Made &operator=(const Made &) = delete;

    MockKmt      &kmt() { return kmt_; }
    std::uint32_t device() const { return device_; }
    std::uint32_t context() const { return context_; }
    std::uint32_t ring() const { return ring_; }

private:
    MockKmt       kmt_;
    std::uint32_t adapter_ = 0u;
    std::uint32_t device_ = 0u;
    std::uint32_t context_ = 0u;
    std::uint32_t ring_ = 0u;
};

}  // namespace

TEST(guest_doorbell, a_connected_doorbell_stores_and_says_what_it_queued)
{ Made          made;
  GuestDoorbell doorbell(made.kmt());
  CHECK(doorbell.invariants_hold());
  CHECK(!doorbell.established());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK(doorbell.established());
  CHECK(doorbell.invariants_hold());
  /* Created is not connected: the status says to submit the other way until
   * the process connects it. */
  CHECK(doorbell.status() == DoorbellStatus::disconnected_retry);
  CHECK(!doorbell.ring(7u));
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 0u);

  CHECK_STATUS(doorbell.connect(), Status::ok);
  CHECK(doorbell.status() == DoorbellStatus::connected);
  CHECK(doorbell.ring(7u));
  /* One four-byte store at offset 0 of the page, and the fence it queued
   * where the driver reads it. */
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 1u);
  CHECK_EQ(made.kmt().queued_fence(doorbell.handle()), 7u);
  /* Cleared, a second submission stores again: the value carries nothing, so
   * what a test can see is that the page was written. */
  made.kmt().clear_doorbell_page(doorbell.handle());
  CHECK(doorbell.ring(8u));
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 1u);
  CHECK_EQ(made.kmt().queued_fence(doorbell.handle()), 8u);
  /* Nothing of that went through the kernel: the whole point (FR-004). */
  CHECK_EQ(made.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK_EQ(made.kmt().calls(MockKmt::Call::notify_work_submission), 0u);
  doorbell.teardown();
  CHECK(!doorbell.established());
  CHECK(doorbell.invariants_hold()); }

/* The status is read before every store, not once: dxgkrnl disconnects a
 * doorbell for suspend, reset and migration without telling the process. */
TEST(guest_doorbell, a_doorbell_disconnected_under_the_process_is_not_stored_to)
{ Made          made;
  GuestDoorbell doorbell(made.kmt());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK_STATUS(doorbell.connect(), Status::ok);
  CHECK(doorbell.ring(1u));

  made.kmt().set_doorbell_status(doorbell.handle(),
                                 static_cast<std::uint32_t>(DoorbellStatus::disconnected_retry));
  made.kmt().clear_doorbell_page(doorbell.handle());
  CHECK(!doorbell.ring(2u));
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 0u);
  /* And the fence it did not store is not claimed as queued. */
  CHECK_EQ(made.kmt().queued_fence(doorbell.handle()), 1u);

  /* Abort is the same refusal from the process's side - what differs is that
   * it must make a new doorbell, which is the transport's business. */
  made.kmt().set_doorbell_status(doorbell.handle(),
                                 static_cast<std::uint32_t>(DoorbellStatus::disconnected_abort));
  CHECK(!doorbell.ring(3u));
  CHECK(doorbell.status() == DoorbellStatus::disconnected_abort);

  /* Connected again, it stores again. */
  CHECK_STATUS(doorbell.connect(), Status::ok);
  CHECK(doorbell.ring(4u));
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 1u);
  CHECK_EQ(made.kmt().queued_fence(doorbell.handle()), 4u); }

/* CONNECTED_NOTIFY_KMD: the store, and then the kernel call the status asked
 * for. This device never asks (research.md §7), but a process that obeyed only
 * the value it expected would be a process that broke on a driver that did. */
TEST(guest_doorbell, a_status_that_asks_for_the_kernel_gets_the_store_and_the_call)
{ Made          made;
  GuestDoorbell doorbell(made.kmt());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK_STATUS(doorbell.connect(), Status::ok);
  made.kmt().set_doorbell_status(doorbell.handle(),
                                 static_cast<std::uint32_t>(DoorbellStatus::connected_notify_kmd));
  CHECK(doorbell.ring(5u));
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 1u);
  CHECK_EQ(made.kmt().calls(MockKmt::Call::notify_work_submission), 1u); }

/* A value the status page holds that is no status at all is not a licence to
 * store: the page is written by something outside this process. */
TEST(guest_doorbell, a_status_nobody_defines_is_not_a_connection)
{ Made          made;
  GuestDoorbell doorbell(made.kmt());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK_STATUS(doorbell.connect(), Status::ok);
  const std::uint32_t nonsense[] = {4u, 0xFFFFFFFFu, 0x80000000u};
  for (const std::uint32_t value : nonsense) {
      made.kmt().set_doorbell_status(doorbell.handle(), value);
      CHECK(!doorbell.ring(9u));
  }
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 0u); }

/* An OS that gives this adapter no hardware queue gives it no doorbell either,
 * and the process is left with the path it always had (research.md §11). */
TEST(guest_doorbell, a_machine_without_hardware_queues_has_no_doorbell_to_offer)
{ Made made;
  made.kmt().refuse_hardware_scheduling();
  GuestDoorbell doorbell(made.kmt());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::not_supported);
  CHECK(!doorbell.established());
  CHECK(!doorbell.ring(1u));
  CHECK(doorbell.status() == DoorbellStatus::disconnected_abort);
  CHECK_STATUS(doorbell.connect(), Status::invalid_state);
  /* Tearing down what was never set up is no-one's error. */
  doorbell.teardown();
  CHECK(doorbell.invariants_hold());
  CHECK_EQ(made.kmt().calls(MockKmt::Call::create_doorbell), 0u); }

/* Everything a create refuses, and that nothing is left half-made. */
TEST(guest_doorbell, a_doorbell_that_cannot_be_made_leaves_no_queue_behind)
{ Made made;
  GuestDoorbell doorbell(made.kmt());
  /* A ring allocation nobody made. */
  CHECK(doorbell.initialize(made.device(), made.context(), 0x900u) != Status::ok);
  CHECK(!doorbell.established());
  CHECK_EQ(made.kmt().calls(MockKmt::Call::destroy_hw_queue), 1u);
  /* A context nobody made. */
  CHECK(doorbell.initialize(made.device(), 0x900u, made.ring()) != Status::ok);
  CHECK(!doorbell.established());
  /* Set up twice is a mistake, not a second doorbell. */
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::invalid_state);
  doorbell.teardown();
  doorbell.teardown();
  CHECK(doorbell.invariants_hold()); }

/* What a process does about a status that refused it (US2): a retry is the
 * same doorbell connected again, an abort is another doorbell entirely, and a
 * connection that is fine is left alone. */
TEST(guest_doorbell, a_refusal_is_recovered_from_the_way_the_status_asked)
{ Made          made;
  GuestDoorbell doorbell(made.kmt());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK_STATUS(doorbell.connect(), Status::ok);
  const std::uint32_t connects = made.kmt().calls(MockKmt::Call::connect_doorbell);

  /* Connected: nothing to recover, and no call made looking for something to
   * do. */
  CHECK(doorbell.recover());
  made.kmt().set_doorbell_status(doorbell.handle(),
                                 static_cast<std::uint32_t>(DoorbellStatus::connected_notify_kmd));
  CHECK(doorbell.recover());
  CHECK_EQ(made.kmt().calls(MockKmt::Call::connect_doorbell), connects);

  /* Retry: the same doorbell, connected again, and storing again. */
  made.kmt().set_doorbell_status(doorbell.handle(),
                                 static_cast<std::uint32_t>(DoorbellStatus::disconnected_retry));
  const std::uint32_t kept = doorbell.handle();
  CHECK(doorbell.recover());
  CHECK_EQ(doorbell.handle(), kept);
  CHECK_EQ(made.kmt().calls(MockKmt::Call::connect_doorbell), connects + 1u);
  CHECK(doorbell.ring(1u));

  /* Abort: that doorbell and its queue are given back, and the process holds
   * another on the same context. */
  made.kmt().set_doorbell_status(doorbell.handle(),
                                 static_cast<std::uint32_t>(DoorbellStatus::disconnected_abort));
  CHECK(doorbell.recover());
  CHECK(doorbell.handle() != kept);
  CHECK(doorbell.established());
  CHECK_EQ(made.kmt().calls(MockKmt::Call::destroy_doorbell), 1u);
  CHECK_EQ(made.kmt().calls(MockKmt::Call::destroy_hw_queue), 1u);
  CHECK(doorbell.ring(2u));
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 1u);
  CHECK_EQ(made.kmt().queued_fence(doorbell.handle()), 2u);
  CHECK(doorbell.invariants_hold()); }

/* An abort there is no answer to leaves the process on the kernel path rather
 * than holding a doorbell it cannot store to. */
TEST(guest_doorbell, an_abort_that_cannot_be_answered_leaves_no_doorbell)
{ Made          made;
  GuestDoorbell never(made.kmt());
  /* One that was never set up has nothing to make another from. */
  CHECK(!never.recover());
  CHECK(!never.established());
  CHECK_EQ(made.kmt().calls(MockKmt::Call::create_hw_queue), 0u);

  GuestDoorbell doorbell(made.kmt());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK_STATUS(doorbell.connect(), Status::ok);
  made.kmt().set_doorbell_status(doorbell.handle(),
                                 static_cast<std::uint32_t>(DoorbellStatus::disconnected_abort));
  /* And an OS that has stopped giving out hardware queues - which is what a
   * reset that turned scheduling off looks like - answers no second time. */
  made.kmt().refuse_hardware_scheduling();
  CHECK(!doorbell.recover());
  CHECK(!doorbell.established());
  CHECK(!doorbell.ring(1u));
  CHECK(doorbell.invariants_hold()); }

/* A driver that answers a doorbell and maps none of the three pages it is
 * supposed to has given the process nothing to store to: believing it would be
 * a store into a null pointer on the submission path. */
TEST(guest_doorbell, a_doorbell_with_no_pages_behind_it_is_not_one)
{ const std::uint32_t missing[] = {1u, 2u, 3u};
  for (const std::uint32_t which : missing) {
      Made made;
      made.kmt().answer_doorbell_without_pages(which);
      GuestDoorbell doorbell(made.kmt());
      CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::invalid_state);
      CHECK(!doorbell.established());
      CHECK(doorbell.invariants_hold());
      /* And the queue it was going to hang off is given back. */
      CHECK_EQ(made.kmt().calls(MockKmt::Call::destroy_hw_queue), 1u);
  } }

/* The invariants themselves: everything mapped or nothing, and a doorbell only
 * on a queue. Nothing reachable breaks them, so they are broken here. */
TEST(guest_doorbell_invariants, a_doorbell_half_mapped_or_on_no_queue_is_detected)
{ Made          made;
  GuestDoorbell doorbell(made.kmt());
  CHECK(doorbell.invariants_hold());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK(doorbell.invariants_hold());
  av::GuestDoorbellProbe probe(doorbell);
  probe.forget_page();
  CHECK(!doorbell.invariants_hold());
  probe.restore_page();
  CHECK(doorbell.invariants_hold());
  probe.forget_status();
  CHECK(!doorbell.invariants_hold());
  probe.restore_status();
  probe.forget_queued();
  CHECK(!doorbell.invariants_hold());
  probe.restore_queued();
  CHECK(doorbell.invariants_hold());
  probe.forget_queue();
  CHECK(!doorbell.invariants_hold());
  probe.restore_queue();
  CHECK(doorbell.invariants_hold()); }
