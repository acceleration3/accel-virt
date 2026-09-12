#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/client_descriptor.hpp"
#include "guest/suite/guest_session.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

using av::ClientDescriptor;
using av::GuestSession;
using av::MockKmt;
using av::PciIdentity;
using av::Status;

namespace av {

struct GuestSessionProbe {
    /* One field of a session set behind its back: whether it is still sound. */
    static bool sound_with_device(GuestSession &session, std::uint32_t device)
    {
        session.device_ = device;
        return session.invariants_hold();
    }
    static void orphan(GuestSession &session, std::uint32_t count, std::uint32_t queue, std::uint32_t context)
    {
        session.allocation_count_ = count;
        session.queue_ = queue;
        session.context_ = context;
    }
    static void stray(GuestSession &session, std::uint32_t fence_object, std::uint64_t unheld_address)
    {
        session.fence_object_ = fence_object;
        session.addresses_[0] = unheld_address;
    }
    static void clear(GuestSession &session)
    {
        orphan(session, 0u, 0u, 0u);
        stray(session, 0u, 0u);
        session.device_ = 0u;
    }
};

}  // namespace av

namespace {

class Fixture {
public:
    Fixture() : adapter_(0u)
    {
        kmt_.add_adapter(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
        std::uint32_t count = 0u;
        static_cast<void>(kmt_.enumerate_adapters(&adapter_, 1u, count));
    }
    ~Fixture() { static_cast<void>(kmt_.close_adapter(adapter_)); }

    MockKmt      &kmt() { return kmt_; }
    std::uint32_t adapter() const { return adapter_; }

private:
    MockKmt       kmt_;
    std::uint32_t adapter_;
};

const ClientDescriptor descriptor(8192u);

/* Every object a submission needs: the address of the mapping. */
std::uint64_t submittable(GuestSession &session)
{
    std::uint32_t index = 9u;
    std::uint64_t address = 0u;
    CHECK_STATUS(session.open_device(), Status::ok);
    CHECK_STATUS(session.allocate(descriptor.bytes(), descriptor.size(), index), Status::ok);
    CHECK_EQ(index, 0u);
    CHECK_STATUS(session.open_paging_queue(), Status::ok);
    CHECK_STATUS(session.map(index, descriptor.pages(), address), Status::ok);
    CHECK_STATUS(session.open_context(), Status::ok);
    CHECK(session.invariants_hold());
    return address;
}

}  // namespace

TEST(guest_session, creates_every_object_submits_waits_and_destroys_them_in_reverse)
{ Fixture fixture;
  GuestSession session(fixture.kmt(), fixture.adapter());
  CHECK(session.invariants_hold());
  const std::uint64_t address = submittable(session);
  CHECK(address != 0u);
  CHECK_STATUS(session.submit(address, 64u), Status::ok);
  CHECK_STATUS(session.wait(), Status::ok);
  CHECK_EQ(fixture.kmt().open_objects(), 6u);
  CHECK_STATUS(session.tear_down(), Status::ok);
  CHECK(session.invariants_hold());
  CHECK_EQ(fixture.kmt().open_objects(), 1u);   /* the adapter, the fixture's */
  CHECK_STATUS(session.tear_down(), Status::ok);
  CHECK_EQ(fixture.kmt().misuses(), 0u); }

TEST(guest_session, refuses_each_step_before_what_it_needs_exists)
{ Fixture fixture;
  GuestSession unbound(fixture.kmt(), 0u);
  CHECK_STATUS(unbound.open_device(), Status::invalid_state);
  GuestSession session(fixture.kmt(), fixture.adapter());
  std::uint32_t index = 0u;
  std::uint64_t address = 0u;
  CHECK_STATUS(session.allocate(descriptor.bytes(), descriptor.size(), index), Status::invalid_state);
  CHECK_STATUS(session.open_paging_queue(), Status::invalid_state);
  CHECK_STATUS(session.open_context(), Status::invalid_state);
  CHECK_STATUS(session.submit(0x1000u, 16u), Status::invalid_state);
  CHECK_STATUS(session.wait(), Status::invalid_state);
  CHECK_STATUS(session.open_device(), Status::ok);
  CHECK_STATUS(session.open_device(), Status::invalid_state);
  CHECK_STATUS(session.map(0u, 1u, address), Status::invalid_state);   /* no allocation */
  CHECK_STATUS(session.allocate(descriptor.bytes(), descriptor.size(), index), Status::ok);
  CHECK_STATUS(session.map(0u, 1u, address), Status::invalid_state);   /* no paging queue */
  CHECK_STATUS(session.open_paging_queue(), Status::ok);
  CHECK_STATUS(session.open_paging_queue(), Status::invalid_state);
  CHECK_STATUS(session.map(1u, 1u, address), Status::invalid_state);   /* no such slot */
  CHECK_STATUS(session.map(0u, 1u, address), Status::ok);
  CHECK_STATUS(session.map(0u, 1u, address), Status::invalid_state);   /* already mapped */
  CHECK_EQ(address, 0u);
  CHECK_STATUS(session.allocate(descriptor.bytes(), descriptor.size(), index), Status::ok);
  CHECK_EQ(index, 1u);
  CHECK_STATUS(session.allocate(descriptor.bytes(), descriptor.size(), index), Status::invalid_state);   /* full */
  CHECK_STATUS(session.open_context(), Status::ok);
  CHECK_STATUS(session.open_context(), Status::invalid_state);
  CHECK(session.invariants_hold());
  CHECK_STATUS(session.tear_down(), Status::ok);
  CHECK_EQ(fixture.kmt().misuses(), 0u); }

TEST(guest_session, a_failure_at_every_call_keeps_nothing_it_did_not_get_and_leaks_nothing)
{ const MockKmt::Call calls[] = {
      MockKmt::Call::create_device, MockKmt::Call::create_allocation, MockKmt::Call::create_paging_queue,
      MockKmt::Call::make_resident, MockKmt::Call::wait_for_fence, MockKmt::Call::map_gpu_virtual_address,
      MockKmt::Call::create_context, MockKmt::Call::submit, MockKmt::Call::wait_for_context};
  for (MockKmt::Call call : calls) {
      Fixture fixture;
      { GuestSession session(fixture.kmt(), fixture.adapter());
        fixture.kmt().fail_at(call, 1u, 0xC0000001u);
        std::uint32_t index = 0u;
        std::uint64_t address = 0u;
        Status status = session.open_device();
        if (status == Status::ok) status = session.allocate(descriptor.bytes(), descriptor.size(), index);
        if (status == Status::ok) status = session.open_paging_queue();
        if (status == Status::ok) status = session.map(index, descriptor.pages(), address);
        if (status == Status::ok) status = session.open_context();
        if (status == Status::ok) status = session.submit(address, 64u);
        if (status == Status::ok) status = session.wait();
        CHECK_STATUS(status, Status::invalid_state);
        CHECK(session.invariants_hold()); }
      CHECK_EQ(fixture.kmt().open_objects(), 1u);
      CHECK_EQ(fixture.kmt().misuses(), 0u);
  }
  Fixture second_wait;   /* the wait after the mapping itself: the mapping is kept to free */
  { GuestSession session(second_wait.kmt(), second_wait.adapter());
    std::uint32_t index = 0u;
    std::uint64_t address = 0u;
    CHECK_STATUS(session.open_device(), Status::ok);
    CHECK_STATUS(session.allocate(descriptor.bytes(), descriptor.size(), index), Status::ok);
    CHECK_STATUS(session.open_paging_queue(), Status::ok);
    CHECK_STATUS(session.map(index, 0u, address), Status::invalid_parameter);   /* no pages */
    second_wait.kmt().fail_at(MockKmt::Call::wait_for_fence, 2u, 0xC0000001u);
    CHECK_STATUS(session.map(index, descriptor.pages(), address), Status::invalid_state);
    CHECK_EQ(address, 0u);
    CHECK(session.invariants_hold());
    CHECK_STATUS(session.map(index, descriptor.pages(), address), Status::invalid_state);   /* mapped already */
    CHECK_STATUS(session.tear_down(), Status::ok);
    CHECK_EQ(second_wait.kmt().calls(MockKmt::Call::free_gpu_virtual_address), 1u); }
  CHECK_EQ(second_wait.kmt().open_objects(), 1u);
  CHECK_EQ(second_wait.kmt().misuses(), 0u); }

TEST(guest_session, zero_handles_answered_as_success_are_malformed_and_never_used)
{ const MockKmt::Call calls[] = {MockKmt::Call::create_device, MockKmt::Call::create_allocation,
                                 MockKmt::Call::create_paging_queue, MockKmt::Call::map_gpu_virtual_address,
                                 MockKmt::Call::create_context};
  for (MockKmt::Call call : calls) {
      Fixture fixture;
      { GuestSession session(fixture.kmt(), fixture.adapter());
        fixture.kmt().zero_next(call);
        std::uint32_t index = 0u;
        std::uint64_t address = 0u;
        Status status = session.open_device();
        if (status == Status::ok) status = session.allocate(descriptor.bytes(), descriptor.size(), index);
        if (status == Status::ok) status = session.open_paging_queue();
        if (status == Status::ok) status = session.map(index, descriptor.pages(), address);
        if (status == Status::ok) status = session.open_context();
        CHECK_STATUS(status, Status::malformed);
        CHECK(session.invariants_hold()); }
      CHECK_EQ(fixture.kmt().open_objects(), 1u);
      CHECK_EQ(fixture.kmt().misuses(), 0u);
  } }

TEST(guest_session, teardown_attempts_every_destruction_and_answers_the_first_failure)
{ const MockKmt::Call calls[] = {MockKmt::Call::destroy_context, MockKmt::Call::free_gpu_virtual_address,
                                 MockKmt::Call::destroy_paging_queue, MockKmt::Call::destroy_allocation,
                                 MockKmt::Call::destroy_device};
  for (MockKmt::Call call : calls) {
      Fixture fixture;
      GuestSession session(fixture.kmt(), fixture.adapter());
      static_cast<void>(submittable(session));
      fixture.kmt().fail_at(call, 1u, 0xC0000001u);
      CHECK_STATUS(session.tear_down(), Status::invalid_state);
      CHECK_EQ(session.teardown_code(), 0xC0000001u);
      CHECK(session.invariants_hold());
      CHECK_EQ(fixture.kmt().calls(MockKmt::Call::destroy_device), 1u);
  }
  Fixture both;
  GuestSession session(both.kmt(), both.adapter());
  static_cast<void>(submittable(session));
  both.kmt().fail_at(MockKmt::Call::destroy_context, 1u, 0xC0000017u);
  both.kmt().fail_at(MockKmt::Call::destroy_device, 1u, 0xC0000001u);
  CHECK_STATUS(session.tear_down(), Status::no_memory);
  CHECK_EQ(session.teardown_code(), 0xC0000017u);
  CHECK_STATUS(session.tear_down(), Status::ok);
  CHECK_EQ(session.teardown_code(), 0u); }

TEST(guest_session_invariants, an_orphaned_object_a_stray_fence_object_or_an_unheld_mapping_is_caught)
{ Fixture fixture;
  GuestSession session(fixture.kmt(), fixture.adapter());
  using av::GuestSessionProbe;
  GuestSessionProbe::orphan(session, 1u, 0u, 0u);
  CHECK(!GuestSessionProbe::sound_with_device(session, 0u));
  GuestSessionProbe::orphan(session, 0u, 5u, 0u);
  CHECK(!GuestSessionProbe::sound_with_device(session, 0u));
  GuestSessionProbe::orphan(session, 0u, 0u, 6u);
  CHECK(!GuestSessionProbe::sound_with_device(session, 0u));
  GuestSessionProbe::orphan(session, 0u, 0u, 0u);
  GuestSessionProbe::stray(session, 7u, 0u);
  CHECK(!GuestSessionProbe::sound_with_device(session, 9u));
  GuestSessionProbe::stray(session, 0u, 0x1000u);
  CHECK(!GuestSessionProbe::sound_with_device(session, 9u));
  GuestSessionProbe::clear(session);
  CHECK(session.invariants_hold()); }

/* The vertical blank is the adapter's, waited on through the session's device:
 * without one there is nothing to wait on. */
TEST(guest_session_await_vertical_blank, needs_the_device_and_names_it)
{ Fixture fixture;
  GuestSession session(fixture.kmt(), fixture.adapter());
  CHECK_STATUS(session.await_vertical_blank(0u), Status::invalid_state);
  CHECK_STATUS(session.open_device(), Status::ok);
  CHECK_STATUS(session.await_vertical_blank(0u), Status::ok);
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_vertical_blank), 1u);
  CHECK_EQ(fixture.kmt().misuses(), 0u);
  CHECK_STATUS(session.tear_down(), Status::ok); }
