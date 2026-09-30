#include "core/support/request_room.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

using av::Irql;
using av::MockPlatform;
using av::RequestRoom;
using av::Status;
using av::Violation;

namespace {

constexpr std::size_t fixed = 64u;
constexpr std::size_t entry = 16u;

}  // namespace

TEST(request_room, starts_with_no_room)
{ MockPlatform platform; RequestRoom room(platform, fixed, entry);
  CHECK(room.start() == nullptr);
  CHECK_EQ(room.size(), 0u);
  CHECK(room.invariants_hold()); }

/* The size a writer asks for at DISPATCH was made at PASSIVE: taking it moves
 * no memory, which the mock would record as an allocation at DISPATCH. */
TEST(request_room, takes_at_dispatch_what_was_prepared_at_passive)
{ MockPlatform platform; RequestRoom room(platform, fixed, entry);
  CHECK_STATUS(room.prepare(256u), Status::ok);
  CHECK_EQ(room.size(), 0u);   /* prepared, not yet in use */
  CHECK_EQ(platform.live_allocations(), 1u);
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(room.take(256u), Status::ok);
  platform.set_irql(Irql::passive);
  CHECK(room.start() != nullptr);
  CHECK_EQ(room.size(), fixed + 256u * entry);
  CHECK_EQ(platform.live_allocations(), 1u);
  CHECK(room.invariants_hold()); }

TEST(request_room, taking_more_than_was_prepared_is_refused)
{ MockPlatform platform; RequestRoom room(platform, fixed, entry);
  CHECK_STATUS(room.take(1u), Status::no_memory);
  CHECK_STATUS(room.prepare(256u), Status::ok);
  CHECK_STATUS(room.take(257u), Status::no_memory);
  CHECK_EQ(room.size(), 0u);
  CHECK_STATUS(room.take(256u), Status::ok);
  CHECK_EQ(room.size(), fixed + 256u * entry); }

TEST(request_room, a_smaller_request_keeps_the_room_it_has)
{ MockPlatform platform; RequestRoom room(platform, fixed, entry);
  CHECK_STATUS(room.prepare(512u), Status::ok);
  CHECK_STATUS(room.take(512u), Status::ok);
  std::uint8_t *const held = room.start();
  CHECK_STATUS(room.prepare(256u), Status::ok);
  CHECK_EQ(platform.live_allocations(), 1u);   /* nothing made for it */
  CHECK_STATUS(room.take(256u), Status::ok);
  CHECK(room.start() == held);
  CHECK_EQ(room.size(), fixed + 512u * entry); }

/* Growing twice: the outgrown room waits as the spare, and the next prepare
 * replaces it rather than keeping both. */
TEST(request_room, the_outgrown_room_is_the_spare_the_next_prepare_replaces)
{ MockPlatform platform; RequestRoom room(platform, fixed, entry);
  CHECK_STATUS(room.prepare(256u), Status::ok);
  CHECK_STATUS(room.take(256u), Status::ok);
  CHECK_STATUS(room.prepare(512u), Status::ok);
  CHECK_EQ(platform.live_allocations(), 2u);
  CHECK_STATUS(room.take(512u), Status::ok);
  CHECK_EQ(room.size(), fixed + 512u * entry);
  CHECK_EQ(platform.live_allocations(), 2u);
  CHECK_STATUS(room.prepare(256u), Status::ok);   /* the spare already fits */
  CHECK_EQ(platform.live_allocations(), 2u);
  CHECK_STATUS(room.prepare(1024u), Status::ok);
  CHECK_EQ(platform.live_allocations(), 2u);
  CHECK_STATUS(room.take(1024u), Status::ok);
  CHECK_EQ(room.size(), fixed + 1024u * entry);
  CHECK(room.invariants_hold()); }

TEST(request_room, a_prepare_that_gets_no_memory_leaves_the_room_in_use)
{ MockPlatform platform; RequestRoom room(platform, fixed, entry);
  CHECK_STATUS(room.prepare(256u), Status::ok);
  CHECK_STATUS(room.take(256u), Status::ok);
  std::uint8_t *const held = room.start();
  platform.fail_allocation_at(1u);
  CHECK_STATUS(room.prepare(512u), Status::no_memory);
  CHECK(room.start() == held);
  CHECK_EQ(room.size(), fixed + 256u * entry);
  CHECK_STATUS(room.take(512u), Status::no_memory);
  CHECK(room.invariants_hold()); }

TEST(request_room, releases_both_blocks)
{ MockPlatform platform;
  { RequestRoom room(platform, fixed, entry);
    CHECK_STATUS(room.prepare(256u), Status::ok);
    CHECK_STATUS(room.take(256u), Status::ok);
    CHECK_STATUS(room.prepare(512u), Status::ok);
    CHECK_EQ(platform.live_allocations(), 2u); }
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(request_room, prepare_is_passive_and_take_dispatch)
{ MockPlatform platform; RequestRoom room(platform, fixed, entry);
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(room.prepare(256u), Status::no_memory);   /* the mock has no memory at DISPATCH */
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::allocation_at_dispatch));
  platform.set_irql(Irql::passive);
  CHECK_STATUS(room.prepare(256u), Status::ok);
  platform.set_irql(Irql::isr);
  CHECK_STATUS(room.take(256u), Status::ok);
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }
