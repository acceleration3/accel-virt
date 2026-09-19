/* The device's table of shared objects, which is where kb 11's risk 6 is
 * answered: identity is the device's, not the context's. The two tests that
 * matter most are named for the two claims the spec makes - two contexts
 * holding one object share one descriptor (US3 scenario 1), and an object
 * outlives the context that published it (US1 scenario 3) - and everything
 * else here exists so those two cannot pass by accident.
 *
 * A "context" in this file is a caller: the table knows nothing about
 * contexts, only about how many holders an object has, which is exactly the
 * separation that lets a context die without the object doing so. */
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/rvk_executor.h"
#include "device/memory/host_shared_object.hpp"
#include "device/memory/host_shared_objects.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>
#include <cstring>

using av::HostSharedObjects;
using av::MockHost;
using av::StateWriter;
using av::Status;

namespace av {

/* The lock a worker would be holding while the main loop dumps state, and the
 * slots themselves, so a table can be put in a state it never reaches for
 * itself. */
struct HostSharedObjectsProbe {
    static void *lock(HostSharedObjects &table) { return table.lock_; }
    static HostSharedObject &slot(HostSharedObjects &table, std::uint32_t index)
    { return table.objects_[index]; }
};

/* The one field a duplicate needs. */
struct HostSharedObjectProbe {
    static void set_id(HostSharedObject &object, std::uint64_t id) { object.id_ = id; }
};

}  // namespace av

namespace {

constexpr std::uint64_t size = 8u * 4096u;

class Fixture {
public:
    Fixture() : objects_(host_) { CHECK_STATUS(objects_.initialize(), Status::ok); }

    MockHost          &host() { return host_; }
    HostSharedObjects &objects() { return objects_; }
    int                descriptor() { return host_.open_descriptor(); }
    void               check()
    { if (!objects_.invariants_hold())
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "invariants broken"); }

private:
    MockHost          host_;
    HostSharedObjects objects_;
};

}  // namespace

TEST(host_shared_objects_initialize, is_done_once_and_a_table_with_no_lock_answers_nothing)
{ MockHost          host;
  HostSharedObjects objects(host);
  CHECK(objects.invariants_hold());
  /* Before initialize every path refuses rather than touching a null lock,
   * and a refusal there is the one that does not close the descriptor: there
   * is no lock to close it under. */
  const int early = host.open_descriptor();
  CHECK_STATUS(objects.publish(1u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, early),
               Status::invalid_state);
  CHECK(host.descriptor_open(early));
  host.close_dmabuf(early);
  std::uint32_t handle_kind = 9u;
  std::uint64_t answered = 9u;
  int           fd = 9;
  CHECK_STATUS(objects.open(1u, RVK_SHARED_MEMORY, size, handle_kind, answered, fd), Status::invalid_state);
  CHECK_EQ(handle_kind, 0u);
  CHECK_EQ(answered, 0u);
  CHECK_EQ(fd, -1);
  objects.close(1u);
  objects.release_all();
  CHECK_EQ(objects.live(), 0u);
  CHECK_STATUS(objects.initialize(), Status::ok);
  CHECK_STATUS(objects.initialize(), Status::invalid_state); }

TEST(host_shared_objects_initialize, propagates_a_host_with_no_mutex_to_give)
{ MockHost host;
  host.fail_mutex_at(1u);
  HostSharedObjects objects(host);
  CHECK_STATUS(objects.initialize(), Status::no_memory);
  CHECK(objects.invariants_hold()); }

TEST(host_shared_objects_publish, records_the_descriptor_and_counts_the_publisher)
{ Fixture   rig;
  const int fd = rig.descriptor();
  CHECK_STATUS(rig.objects().publish(7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, fd), Status::ok);
  rig.check();
  CHECK_EQ(rig.objects().live(), 1u);
  std::uint32_t handle_kind = 0u;
  std::uint64_t answered = 0u;
  int           given = -1;
  CHECK_STATUS(rig.objects().open(7u, RVK_SHARED_MEMORY, size, handle_kind, answered, given), Status::ok);
  rig.check();
  CHECK_EQ(handle_kind, RVK_SHARED_HANDLE_DMA_BUF);
  CHECK_EQ(answered, size);
  CHECK_EQ(given, fd);
  rig.objects().close(7u);
  rig.check();
  rig.objects().close(7u);
  rig.check();
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(!rig.host().descriptor_open(fd));
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(host_shared_objects, two_contexts_holding_one_object_share_one_descriptor)
{ Fixture   rig;
  const int fd = rig.descriptor();
  CHECK_STATUS(rig.objects().publish(3u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, fd), Status::ok);
  rig.check();
  /* Two importers, of two guest devices, of two guest processes. */
  std::uint32_t first_kind = 0u, second_kind = 0u;
  std::uint64_t first_size = 0u, second_size = 0u;
  int           first = -1, second = -1;
  CHECK_STATUS(rig.objects().open(3u, RVK_SHARED_MEMORY, size, first_kind, first_size, first), Status::ok);
  rig.check();
  CHECK_STATUS(rig.objects().open(3u, RVK_SHARED_MEMORY, size, second_kind, second_size, second), Status::ok);
  rig.check();
  CHECK_EQ(first, second);
  CHECK_EQ(first, fd);
  CHECK_EQ(first_size, second_size);
  CHECK_EQ(first_kind, second_kind);
  /* One object, whatever the guest thinks it holds. */
  CHECK_EQ(rig.objects().live(), 1u);
  rig.objects().close(3u);          /* the publisher */
  rig.check();
  CHECK(rig.host().descriptor_open(fd));
  rig.objects().close(3u);          /* the first importer */
  rig.check();
  CHECK(rig.host().descriptor_open(fd));
  CHECK_EQ(rig.objects().live(), 1u);
  rig.objects().close(3u);          /* the last */
  rig.check();
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(!rig.host().descriptor_open(fd)); }

TEST(host_shared_objects, an_object_outlives_the_context_that_published_it)
{ Fixture   rig;
  const int fd = rig.descriptor();
  CHECK_STATUS(rig.objects().publish(3u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, fd), Status::ok);
  std::uint32_t handle_kind = 0u;
  std::uint64_t answered = 0u;
  int           given = -1;
  CHECK_STATUS(rig.objects().open(3u, RVK_SHARED_MEMORY, size, handle_kind, answered, given), Status::ok);
  rig.check();
  /* The publisher's process exits, so its context is torn down and releases
   * what it held. Nothing the other process holds may change. */
  rig.objects().close(3u);
  rig.check();
  CHECK_EQ(rig.objects().live(), 1u);
  CHECK(rig.host().descriptor_open(fd));
  std::uint32_t again_kind = 0u;
  std::uint64_t again_size = 0u;
  int           again = -1;
  CHECK_STATUS(rig.objects().open(3u, RVK_SHARED_MEMORY, size, again_kind, again_size, again), Status::ok);
  rig.check();
  CHECK_EQ(again, fd);
  CHECK_EQ(again_size, size);
  rig.objects().close(3u);
  rig.objects().close(3u);
  rig.check();
  CHECK(!rig.host().descriptor_open(fd)); }

TEST(host_shared_objects_publish, refuses_an_id_already_published_and_leaves_the_descriptor_the_callers)
{ Fixture   rig;
  const int first = rig.descriptor();
  const int second = rig.descriptor();
  CHECK_STATUS(rig.objects().publish(7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, first), Status::ok);
  rig.check();
  CHECK_STATUS(rig.objects().publish(7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, second), Status::conflict);
  rig.check();
  CHECK_EQ(rig.objects().live(), 1u);
  /* The one already published is untouched; the refused one is closed, since
   * whoever exported it has nowhere to close it. */
  std::uint32_t handle_kind = 0u;
  std::uint64_t answered = 0u;
  int           given = -1;
  CHECK_STATUS(rig.objects().open(7u, RVK_SHARED_MEMORY, size, handle_kind, answered, given), Status::ok);
  CHECK_EQ(given, first);
  CHECK(!rig.host().descriptor_open(second));
  rig.objects().release_all();
  rig.check();
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(host_shared_objects_publish, passes_a_field_no_object_can_have_through_to_a_refusal)
{ Fixture rig;
  CHECK_STATUS(rig.objects().publish(0u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, rig.descriptor()),
               Status::invalid_parameter);
  rig.check();
  CHECK_STATUS(rig.objects().publish(1u, 0u, RVK_SHARED_HANDLE_DMA_BUF, size, rig.descriptor()),
               Status::invalid_parameter);
  rig.check();
  CHECK_STATUS(rig.objects().publish(1u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, -1),
               Status::invalid_parameter);
  rig.check();
  /* Memory with no length, and a fence with one: both describe something the
   * host cannot make. */
  CHECK_STATUS(rig.objects().publish(1u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 0u, rig.descriptor()),
               Status::invalid_parameter);
  rig.check();
  CHECK_STATUS(rig.objects().publish(1u, RVK_SHARED_NATIVE_FENCE, RVK_SHARED_HANDLE_OPAQUE_FD, size,
                                     rig.descriptor()),
               Status::invalid_parameter);
  rig.check();
  CHECK_EQ(rig.objects().live(), 0u); }

TEST(host_shared_objects_open, refuses_a_size_that_disagrees_so_nothing_is_imported_past_its_end)
{ Fixture rig;
  CHECK_STATUS(rig.objects().publish(7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, rig.descriptor()),
               Status::ok);
  const std::uint64_t wrong[] = {0u, 4096u, size - 4096u, size + 4096u, 0xFFFFFFFFFFFFF000u};
  for (const std::uint64_t claimed : wrong) {
      std::uint32_t handle_kind = 9u;
      std::uint64_t answered = 9u;
      int           fd = 9;
      CHECK_STATUS(rig.objects().open(7u, RVK_SHARED_MEMORY, claimed, handle_kind, answered, fd),
                   Status::invalid_parameter);
      rig.check();
      CHECK_EQ(fd, -1);
  }
  std::uint32_t handle_kind = 0u;
  std::uint64_t answered = 0u;
  int           fd = -1;
  CHECK_STATUS(rig.objects().open(7u, RVK_SHARED_MEMORY, size, handle_kind, answered, fd), Status::ok);
  rig.check();
  CHECK_EQ(answered, size);
  rig.objects().release_all();
  rig.check(); }

TEST(host_shared_objects_publish, fills_up_and_then_has_no_space)
{ Fixture rig;
  for (std::uint64_t id = 1u; id <= HostSharedObjects::max_objects; id++)
      CHECK_STATUS(rig.objects().publish(id, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 4096u, rig.descriptor()),
                   Status::ok);
  rig.check();
  CHECK_EQ(rig.objects().live(), HostSharedObjects::max_objects);
  const int spare = rig.descriptor();
  CHECK_STATUS(rig.objects().publish(0xFFFFu, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 4096u, spare),
               Status::no_space);
  rig.check();
  CHECK(!rig.host().descriptor_open(spare));
  /* One let go makes room again, and the id that was refused is taken. */
  rig.objects().close(1u);
  rig.check();
  CHECK_STATUS(rig.objects().publish(0xFFFFu, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 4096u, rig.descriptor()),
               Status::ok);
  rig.check();
  rig.objects().release_all();
  rig.check();
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(host_shared_objects_open, refuses_an_id_this_device_never_published)
{ Fixture rig;
  CHECK_STATUS(rig.objects().publish(7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, rig.descriptor()),
               Status::ok);
  const std::uint64_t forged[] = {0u, 6u, 8u, 0xFFFFFFFFFFFFFFFFu};
  for (const std::uint64_t id : forged) {
      std::uint32_t handle_kind = 9u;
      std::uint64_t answered = 9u;
      int           fd = 9;
      CHECK_STATUS(rig.objects().open(id, RVK_SHARED_MEMORY, size, handle_kind, answered, fd), Status::not_found);
      rig.check();
      CHECK_EQ(fd, -1);
      CHECK_EQ(answered, 0u);
  }
  rig.objects().release_all(); }

TEST(host_shared_objects_open, refuses_a_kind_that_disagrees_so_a_fence_is_never_opened_as_memory)
{ Fixture rig;
  CHECK_STATUS(rig.objects().publish(7u, RVK_SHARED_MONITORED_FENCE, RVK_SHARED_HANDLE_OPAQUE_FD, 0u,
                                     rig.descriptor()),
               Status::ok);
  const std::uint32_t wrong[] = {RVK_SHARED_MEMORY, RVK_SHARED_NATIVE_FENCE, 0u, 99u};
  for (const std::uint32_t kind : wrong) {
      std::uint32_t handle_kind = 9u;
      std::uint64_t answered = 9u;
      int           fd = 9;
      CHECK_STATUS(rig.objects().open(7u, kind, 0u, handle_kind, answered, fd), Status::invalid_parameter);
      rig.check();
      CHECK_EQ(fd, -1);
  }
  /* And the right kind still opens it, so the refusal was the kind and not the object. */
  std::uint32_t handle_kind = 0u;
  std::uint64_t answered = 0u;
  int           fd = -1;
  CHECK_STATUS(rig.objects().open(7u, RVK_SHARED_MONITORED_FENCE, 0u, handle_kind, answered, fd), Status::ok);
  rig.check();
  CHECK(fd >= 0);
  rig.objects().release_all(); }

TEST(host_shared_objects_close, an_id_this_device_does_not_hold_is_ignored)
{ Fixture rig;
  rig.objects().close(1u);
  rig.check();
  CHECK_STATUS(rig.objects().publish(7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, rig.descriptor()),
               Status::ok);
  rig.objects().close(8u);
  rig.check();
  CHECK_EQ(rig.objects().live(), 1u);
  rig.objects().close(7u);
  rig.objects().close(7u);       /* the second is the idempotent one */
  rig.check();
  CHECK_EQ(rig.objects().live(), 0u); }

TEST(host_shared_objects_release_all, closes_every_descriptor_whatever_its_count)
{ Fixture rig;
  int      fds[3] = {-1, -1, -1};
  for (std::uint64_t id = 1u; id <= 3u; id++) {
      fds[id - 1u] = rig.descriptor();
      CHECK_STATUS(rig.objects().publish(id, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 4096u, fds[id - 1u]),
                   Status::ok);
      std::uint32_t handle_kind = 0u;
      std::uint64_t answered = 0u;
      int           given = -1;
      CHECK_STATUS(rig.objects().open(id, RVK_SHARED_MEMORY, 4096u, handle_kind, answered, given), Status::ok);
  }
  rig.check();
  CHECK_EQ(rig.objects().live(), 3u);
  rig.objects().release_all();
  rig.check();
  CHECK_EQ(rig.objects().live(), 0u);
  for (const int fd : fds) CHECK(!rig.host().descriptor_open(fd));
  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  /* A second reset changes nothing. */
  rig.objects().release_all();
  rig.check(); }

TEST(host_shared_objects, a_table_destroyed_with_objects_left_closes_them_rather_than_leaking)
{ MockHost host;
  int      fd = -1;
  {
      HostSharedObjects objects(host);
      CHECK_STATUS(objects.initialize(), Status::ok);
      fd = host.open_descriptor();
      CHECK_STATUS(objects.publish(1u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 4096u, fd), Status::ok);
  }
  CHECK(!host.descriptor_open(fd));
  CHECK_EQ(host.live_dmabufs(), 0u); }

TEST(host_shared_objects_report, names_every_object_and_says_busy_when_a_worker_holds_it)
{ MockHost host;
  {
      HostSharedObjects unstarted(host);
      char              text[512] = {};
      StateWriter       writer(text, sizeof text);
      unstarted.report(writer);
      CHECK(std::strstr(writer.dump(), "shared") != nullptr);
      CHECK(std::strstr(writer.dump(), "busy=false") != nullptr);
      CHECK(std::strstr(writer.dump(), "live=") == nullptr);
  }
  Fixture rig;
  CHECK_STATUS(rig.objects().publish(0x2Au, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_OPAQUE_FD, 0x3000u,
                                     rig.descriptor()),
               Status::ok);
  char        text[1024] = {};
  StateWriter writer(text, sizeof text);
  rig.objects().report(writer);
  CHECK(std::strstr(writer.dump(), "live=1") != nullptr);
  CHECK(std::strstr(writer.dump(), "object") != nullptr);
  CHECK(std::strstr(writer.dump(), "holders=1") != nullptr);
  CHECK(std::strstr(writer.dump(), "size=12288") != nullptr);
  CHECK(std::strstr(writer.dump(), "busy") == nullptr);

  rig.host().hold_mutex_elsewhere(av::HostSharedObjectsProbe::lock(rig.objects()), true);
  char        busy[512] = {};
  StateWriter second(busy, sizeof busy);
  rig.objects().report(second);
  CHECK(std::strstr(second.dump(), "busy=true") != nullptr);
  CHECK(std::strstr(second.dump(), "live=") == nullptr);
  rig.host().hold_mutex_elsewhere(av::HostSharedObjectsProbe::lock(rig.objects()), false);
  rig.objects().release_all();
  rig.check(); }

TEST(host_shared_objects_invariants, refuse_a_broken_object_and_two_slots_naming_one_id)
{ Fixture rig;
  CHECK_STATUS(rig.objects().publish(7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, rig.descriptor()),
               Status::ok);
  CHECK_STATUS(rig.objects().publish(8u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, size, rig.descriptor()),
               Status::ok);
  CHECK(rig.objects().invariants_hold());

  /* One object whose own invariant is broken breaks the table's. */
  av::HostSharedObject &first = av::HostSharedObjectsProbe::slot(rig.objects(), 0u);
  av::HostSharedObject &second = av::HostSharedObjectsProbe::slot(rig.objects(), 1u);
  const std::uint64_t   kept = second.id();
  av::HostSharedObjectProbe::set_id(second, 0u);
  CHECK(!rig.objects().invariants_hold());
  av::HostSharedObjectProbe::set_id(second, kept);
  CHECK(rig.objects().invariants_hold());

  /* Two live slots naming one id is the other way a table goes wrong: an open
   * would reach whichever it found first and a close the other. */
  av::HostSharedObjectProbe::set_id(second, first.id());
  CHECK(!rig.objects().invariants_hold());
  av::HostSharedObjectProbe::set_id(second, kept);
  CHECK(rig.objects().invariants_hold());
  rig.objects().release_all(); }
