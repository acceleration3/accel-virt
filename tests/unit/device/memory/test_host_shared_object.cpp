/* One host resource several contexts name. What it owns is a descriptor no
 * guest ever sees, and the whole point of the refcount is that the descriptor
 * is closed exactly once - not when the publisher goes, not once per importer,
 * but when the last holder lets go. Every test here asks the kernel whether
 * the descriptor is still open rather than trusting a counter. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/memory/host_shared_object.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

using av::HostSharedObject;
using av::MockHost;
using av::Status;

namespace {

constexpr std::uint64_t size = 8u * 4096u;

class Fixture {
public:
    MockHost &host() { return host_; }
    int       descriptor() { return host_.open_descriptor(); }
    /* Every mutation is followed by the invariant, so no test repeats it. */
    void      check(const HostSharedObject &object)
    { if (!object.invariants_hold())
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "invariants broken"); }

private:
    MockHost host_;
};

}  // namespace

TEST(host_shared_object, a_new_one_holds_nothing_and_giving_it_back_closes_nothing)
{ Fixture          rig;
  HostSharedObject object;
  rig.check(object);
  CHECK(!object.live());
  CHECK_EQ(object.descriptor(), -1);
  CHECK_EQ(object.holders(), 0u);
  CHECK(!object.holds(0u));
  object.give_back(rig.host());
  rig.check(object);
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(host_shared_object_take, keeps_the_descriptor_and_counts_the_publisher_as_the_first_holder)
{ Fixture   rig;
  const int fd = rig.descriptor();
  CHECK(fd >= 0);
  HostSharedObject object;
  CHECK_STATUS(object.take(7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, fd), Status::ok);
  rig.check(object);
  CHECK(object.live());
  CHECK(object.holds(7u));
  CHECK(!object.holds(8u));
  CHECK_EQ(object.id(), 7u);
  CHECK_EQ(object.kind(), RVK_SHARED_MEMORY);
  CHECK_EQ(object.handle_kind(), RVK_SHARED_HANDLE_DMA_BUF);
  CHECK_EQ(object.size(), size);
  CHECK_EQ(object.descriptor(), fd);
  CHECK_EQ(object.holders(), 1u);
  CHECK(rig.host().descriptor_open(fd));
  object.give_back(rig.host());
  rig.check(object);
  CHECK(!rig.host().descriptor_open(fd));
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(host_shared_object_take, refuses_every_field_no_object_can_have_and_keeps_the_descriptor_the_callers)
{ struct Case { std::uint64_t id; std::uint32_t kind; std::uint32_t handle_kind; std::uint64_t size; bool fd; };
  const Case cases[] = {
      {0u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, true},          /* no id */
      {7u, 0u, RVK_SHARED_HANDLE_DMA_BUF, size, true},                         /* no kind */
      {7u, RVK_SHARED_NATIVE_FENCE + 1u, RVK_SHARED_HANDLE_DMA_BUF, size, true},
      {7u, 0xFFFFFFFFu, RVK_SHARED_HANDLE_DMA_BUF, size, true},
      {7u, RVK_SHARED_MEMORY, 0u, size, true},                        /* no blob kind */
      {7u, RVK_SHARED_MEMORY, 3u, size, true},
      {7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 0u, true},            /* memory with no length */
      {7u, RVK_SHARED_NATIVE_FENCE, RVK_SHARED_HANDLE_OPAQUE_FD, size, true},  /* a fence with one */
      {7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, false},         /* no descriptor */
  };
  for (const Case &c : cases) {
      Fixture   rig;
      const int fd = c.fd ? rig.descriptor() : -1;
      HostSharedObject object;
      CHECK_STATUS(object.take(c.id, c.kind, c.handle_kind, c.size, fd), Status::invalid_parameter);
      rig.check(object);
      CHECK(!object.live());
      CHECK_EQ(object.descriptor(), -1);
      if (fd >= 0) {
          /* Refused means the caller still owns it: the object closed nothing. */
          CHECK(rig.host().descriptor_open(fd));
          rig.host().close_dmabuf(fd);
      }
  } }

TEST(host_shared_object_take, refuses_a_slot_already_live_without_disturbing_what_is_in_it)
{ Fixture   rig;
  const int first = rig.descriptor();
  const int second = rig.descriptor();
  HostSharedObject object;
  CHECK_STATUS(object.take(7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, first), Status::ok);
  rig.check(object);
  CHECK_STATUS(object.take(9u, RVK_SHARED_NATIVE_FENCE, RVK_SHARED_HANDLE_OPAQUE_FD, 0u, second),
               Status::invalid_state);
  rig.check(object);
  CHECK_EQ(object.id(), 7u);
  CHECK_EQ(object.descriptor(), first);
  CHECK(rig.host().descriptor_open(second));
  rig.host().close_dmabuf(second);
  object.give_back(rig.host()); }

TEST(host_shared_object_drop, closes_the_descriptor_only_when_the_last_holder_lets_go)
{ Fixture   rig;
  const int fd = rig.descriptor();
  HostSharedObject object;
  CHECK_STATUS(object.take(7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, fd), Status::ok);
  rig.check(object);
  object.hold();
  rig.check(object);
  object.hold();
  rig.check(object);
  CHECK_EQ(object.holders(), 3u);
  CHECK(!object.drop(rig.host()));
  rig.check(object);
  CHECK(rig.host().descriptor_open(fd));
  CHECK(!object.drop(rig.host()));
  rig.check(object);
  CHECK_EQ(object.holders(), 1u);
  CHECK(rig.host().descriptor_open(fd));
  CHECK(object.drop(rig.host()));
  rig.check(object);
  CHECK(!object.live());
  CHECK(!rig.host().descriptor_open(fd));
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(host_shared_object_drop, a_slot_holding_nothing_drops_nothing_and_closes_nothing)
{ Fixture          rig;
  HostSharedObject object;
  CHECK(!object.drop(rig.host()));
  rig.check(object);
  CHECK_EQ(object.holders(), 0u); }

TEST(host_shared_object_hold, an_empty_slot_never_becomes_held_so_a_stray_hold_cannot_resurrect_one)
{ Fixture          rig;
  HostSharedObject object;
  object.hold();
  rig.check(object);
  CHECK_EQ(object.holders(), 0u);
  CHECK(!object.live()); }

TEST(host_shared_object_give_back, drops_every_holder_at_once_because_a_reset_leaves_no_context)
{ Fixture   rig;
  const int fd = rig.descriptor();
  HostSharedObject object;
  CHECK_STATUS(object.take(4u, RVK_SHARED_MONITORED_FENCE, RVK_SHARED_HANDLE_OPAQUE_FD, 0u, fd), Status::ok);
  object.hold();
  object.hold();
  rig.check(object);
  object.give_back(rig.host());
  rig.check(object);
  CHECK(!object.live());
  CHECK_EQ(object.holders(), 0u);
  CHECK(!rig.host().descriptor_open(fd));
  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  /* And a slot given back takes another object without complaint. */
  const int again = rig.descriptor();
  CHECK_STATUS(object.take(5u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 4096u, again), Status::ok);
  rig.check(object);
  object.give_back(rig.host()); }

TEST(host_shared_object, every_kind_and_every_handle_kind_the_abi_has_is_taken)
{ const std::uint32_t kinds[] = {RVK_SHARED_MEMORY, RVK_SHARED_MONITORED_FENCE, RVK_SHARED_NATIVE_FENCE};
  const std::uint32_t blobs[] = {RVK_SHARED_HANDLE_OPAQUE_FD, RVK_SHARED_HANDLE_DMA_BUF};
  for (const std::uint32_t kind : kinds)
      for (const std::uint32_t blob : blobs) {
          Fixture          rig;
          HostSharedObject object;
          const std::uint64_t bytes = kind == RVK_SHARED_MEMORY ? 4096u : 0u;
          CHECK_STATUS(object.take(1u, kind, blob, bytes, rig.descriptor()), Status::ok);
          rig.check(object);
          CHECK_EQ(object.kind(), kind);
          CHECK_EQ(object.handle_kind(), blob);
          object.give_back(rig.host());
      } }
