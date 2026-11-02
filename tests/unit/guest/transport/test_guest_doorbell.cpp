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
    void forget_queue() { doorbell_.queue_ = av::GuestHwQueue(); }
    void restore_queue() { doorbell_.queue_ = queue_; }

    GuestDoorbell           &doorbell_;
    volatile uint32_t       *page_;
    const volatile uint32_t *status_;
    volatile uint64_t       *queued_;
    GuestHwQueue             queue_;
};

}

using av::DoorbellStatus;
using av::GuestDoorbell;
using av::MockKmt;
using av::Status;

namespace {
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

}

TEST(guest_doorbell, a_connected_doorbell_stores_and_says_what_it_queued)
{ Made          made;
  GuestDoorbell doorbell(made.kmt());
  CHECK(doorbell.invariants_hold());
  CHECK(!doorbell.established());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK(doorbell.established());
  CHECK(doorbell.invariants_hold());
  CHECK(doorbell.status() == DoorbellStatus::disconnected_retry);
  CHECK(!doorbell.ring(7u));
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 0u);

  CHECK_STATUS(doorbell.connect(), Status::ok);
  CHECK(doorbell.status() == DoorbellStatus::connected);
  CHECK(doorbell.ring(7u));
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 1u);
  CHECK_EQ(made.kmt().queued_fence(doorbell.handle()), 7u);
  made.kmt().clear_doorbell_page(doorbell.handle());
  CHECK(doorbell.ring(8u));
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 1u);
  CHECK_EQ(made.kmt().queued_fence(doorbell.handle()), 8u);
  CHECK_EQ(made.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK_EQ(made.kmt().calls(MockKmt::Call::notify_work_submission), 0u);
  doorbell.teardown();
  CHECK(!doorbell.established());
  CHECK(doorbell.invariants_hold()); }

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
  CHECK_EQ(made.kmt().queued_fence(doorbell.handle()), 1u);

  made.kmt().set_doorbell_status(doorbell.handle(),
                                 static_cast<std::uint32_t>(DoorbellStatus::disconnected_abort));
  CHECK(!doorbell.ring(3u));
  CHECK(doorbell.status() == DoorbellStatus::disconnected_abort);

  CHECK_STATUS(doorbell.connect(), Status::ok);
  CHECK(doorbell.ring(4u));
  CHECK_EQ(made.kmt().doorbell_page(doorbell.handle()), 1u);
  CHECK_EQ(made.kmt().queued_fence(doorbell.handle()), 4u); }

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

TEST(guest_doorbell, a_machine_without_hardware_queues_has_no_doorbell_to_offer)
{ Made made;
  made.kmt().refuse_hardware_scheduling();
  GuestDoorbell doorbell(made.kmt());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::not_supported);
  CHECK(!doorbell.established());
  CHECK(!doorbell.ring(1u));
  CHECK(doorbell.status() == DoorbellStatus::disconnected_abort);
  CHECK_STATUS(doorbell.connect(), Status::invalid_state);
  doorbell.teardown();
  CHECK(doorbell.invariants_hold());
  CHECK_EQ(made.kmt().calls(MockKmt::Call::create_doorbell), 0u); }

TEST(guest_doorbell, a_doorbell_that_cannot_be_made_leaves_no_queue_behind)
{ Made made;
  GuestDoorbell doorbell(made.kmt());
  CHECK(doorbell.initialize(made.device(), made.context(), 0x900u) != Status::ok);
  CHECK(!doorbell.established());
  CHECK_EQ(made.kmt().calls(MockKmt::Call::destroy_hw_queue), 1u);
  CHECK(doorbell.initialize(made.device(), 0x900u, made.ring()) != Status::ok);
  CHECK(!doorbell.established());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::invalid_state);
  doorbell.teardown();
  doorbell.teardown();
  CHECK(doorbell.invariants_hold()); }

TEST(guest_doorbell, a_refusal_is_recovered_from_the_way_the_status_asked)
{ Made          made;
  GuestDoorbell doorbell(made.kmt());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK_STATUS(doorbell.connect(), Status::ok);
  const std::uint32_t connects = made.kmt().calls(MockKmt::Call::connect_doorbell);

  CHECK(doorbell.recover());
  made.kmt().set_doorbell_status(doorbell.handle(),
                                 static_cast<std::uint32_t>(DoorbellStatus::connected_notify_kmd));
  CHECK(doorbell.recover());
  CHECK_EQ(made.kmt().calls(MockKmt::Call::connect_doorbell), connects);

  made.kmt().set_doorbell_status(doorbell.handle(),
                                 static_cast<std::uint32_t>(DoorbellStatus::disconnected_retry));
  const std::uint32_t kept = doorbell.handle();
  CHECK(doorbell.recover());
  CHECK_EQ(doorbell.handle(), kept);
  CHECK_EQ(made.kmt().calls(MockKmt::Call::connect_doorbell), connects + 1u);
  CHECK(doorbell.ring(1u));

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

TEST(guest_doorbell, an_abort_that_cannot_be_answered_leaves_no_doorbell)
{ Made          made;
  GuestDoorbell never(made.kmt());
  CHECK(!never.recover());
  CHECK(!never.established());
  CHECK_EQ(made.kmt().calls(MockKmt::Call::create_hw_queue), 0u);

  GuestDoorbell doorbell(made.kmt());
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK_STATUS(doorbell.connect(), Status::ok);
  made.kmt().set_doorbell_status(doorbell.handle(),
                                 static_cast<std::uint32_t>(DoorbellStatus::disconnected_abort));
  made.kmt().refuse_hardware_scheduling();
  CHECK(!doorbell.recover());
  CHECK(!doorbell.established());
  CHECK(!doorbell.ring(1u));
  CHECK(doorbell.invariants_hold()); }

TEST(guest_doorbell, a_doorbell_with_no_pages_behind_it_is_not_one)
{ const std::uint32_t missing[] = {1u, 2u, 3u};
  for (const std::uint32_t which : missing) {
      Made made;
      made.kmt().answer_doorbell_without_pages(which);
      GuestDoorbell doorbell(made.kmt());
      CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::invalid_state);
      CHECK(!doorbell.established());
      CHECK(doorbell.invariants_hold());
      CHECK_EQ(made.kmt().calls(MockKmt::Call::destroy_hw_queue), 1u);
  } }

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

TEST(guest_doorbell, holds_the_queue_its_submissions_and_waits_go_through)
{ Made          made;
  GuestDoorbell doorbell(made.kmt());
  CHECK_EQ(doorbell.queue(), 0u);
  CHECK_EQ(doorbell.progress_fence(), 0u);
  CHECK_EQ(doorbell.progress(), 0u);
  CHECK_STATUS(doorbell.initialize(made.device(), made.context(), made.ring()), Status::ok);
  CHECK(doorbell.queue() != 0u);
  CHECK_EQ(doorbell.progress_fence(), doorbell.queue());
  CHECK_EQ(doorbell.progress(), 0u);
  CHECK_STATUS(doorbell.connect(), Status::ok);
  CHECK(doorbell.ring(4u));

  CHECK_STATUS(made.kmt().wait_for_hw_queue(made.device(), doorbell.progress_fence(), 4u), Status::ok);
  CHECK_EQ(doorbell.progress(), 4u);

  const std::uint32_t queue = doorbell.queue();
  doorbell.give_back();
  CHECK(!doorbell.established());
  CHECK_EQ(doorbell.queue(), queue);
  CHECK_EQ(made.kmt().calls(MockKmt::Call::destroy_doorbell), 1u);
  CHECK_EQ(made.kmt().calls(MockKmt::Call::destroy_hw_queue), 0u);
  CHECK(!doorbell.ring(5u));
  CHECK(doorbell.invariants_hold());
  doorbell.teardown();
  CHECK_EQ(doorbell.queue(), 0u);
  CHECK_EQ(made.kmt().calls(MockKmt::Call::destroy_hw_queue), 1u); }
