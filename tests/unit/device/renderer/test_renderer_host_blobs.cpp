/* Which context each host blob was made for, as the renderer knows it: the
 * contexts that import a blob run in the renderer, so it records the owner as
 * it allocates and forgets it as it frees, and a context imports only what its
 * own process opened. */
#include "core/support/status.hpp"
#include "device/renderer/renderer_host_blobs.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/resources/host_blob_id.hpp"

#include <cstdint>

using av::HostBlobId;
using av::HostViolation;
using av::MockHost;
using av::RendererHostBlobs;
using av::Status;

namespace {

constexpr std::uint32_t first = HostBlobId::first;
constexpr std::uint32_t last = HostBlobId::first + HostBlobId::count - 1u;

}  // namespace

TEST(renderer_host_blobs_adopt, records_the_owner_of_an_id_in_range)
{ MockHost host;
  RendererHostBlobs blobs(host);
  CHECK_STATUS(blobs.initialize(), Status::ok);
  CHECK_STATUS(blobs.adopt(first, 3u), Status::ok);
  CHECK_STATUS(blobs.adopt(last, 4u), Status::ok);
  CHECK_EQ(blobs.live(), 2u);
  CHECK(blobs.owns(3u, first));
  CHECK(blobs.owns(4u, last));
  CHECK(!blobs.owns(4u, first));
  CHECK(!blobs.owns(3u, last));
  CHECK(!blobs.owns(3u, first + 1u));
  CHECK_STATUS(blobs.adopt(first, 5u), Status::ok);   /* QEMU made a new blob under a freed id */
  CHECK(blobs.owns(5u, first));
  CHECK(!blobs.owns(3u, first));
  CHECK_EQ(blobs.live(), 2u); }

TEST(renderer_host_blobs_adopt, refuses_an_id_outside_the_range_context_zero_or_no_lock)
{ MockHost host;
  RendererHostBlobs blobs(host);
  CHECK_STATUS(blobs.adopt(first, 3u), Status::invalid_parameter);   /* before initialize */
  CHECK_STATUS(blobs.initialize(), Status::ok);
  CHECK_STATUS(blobs.adopt(first - 1u, 3u), Status::invalid_parameter);
  CHECK_STATUS(blobs.adopt(last + 1u, 3u), Status::invalid_parameter);
  CHECK_STATUS(blobs.adopt(0u, 3u), Status::invalid_parameter);
  CHECK_STATUS(blobs.adopt(first, 0u), Status::invalid_parameter);
  CHECK_EQ(blobs.live(), 0u);
  CHECK(!blobs.owns(0u, first)); }

TEST(renderer_host_blobs_forget, drops_one_owner_or_every_one)
{ MockHost host;
  RendererHostBlobs blobs(host);
  blobs.forget(first);       /* no lock yet: nothing */
  blobs.forget_all();
  CHECK_STATUS(blobs.initialize(), Status::ok);
  CHECK_STATUS(blobs.adopt(first, 3u), Status::ok);
  CHECK_STATUS(blobs.adopt(first + 1u, 3u), Status::ok);
  CHECK_STATUS(blobs.adopt(first + 2u, 4u), Status::ok);
  blobs.forget(first + 1u);
  blobs.forget(first + 9u);   /* never adopted: nothing */
  blobs.forget(7u);           /* not a host blob id: nothing */
  CHECK_EQ(blobs.live(), 2u);
  CHECK(!blobs.owns(3u, first + 1u));
  CHECK(blobs.owns(3u, first));
  blobs.forget_all();
  CHECK_EQ(blobs.live(), 0u);
  CHECK(!blobs.owns(3u, first));
  CHECK(!blobs.owns(4u, first + 2u)); }

TEST(renderer_host_blobs_owns, answers_nothing_without_a_lock)
{ MockHost host;
  RendererHostBlobs blobs(host);
  CHECK(!blobs.owns(3u, first));
  CHECK(!blobs.owns(0u, 0u)); }

TEST(renderer_host_blobs_initialize, makes_its_lock_once_and_gives_it_back)
{ MockHost host;
  { RendererHostBlobs blobs(host);
    host.fail_mutex_at(1u);
    CHECK_STATUS(blobs.initialize(), Status::no_memory);
    CHECK_EQ(host.live_mutexes(), 0u);
    CHECK_STATUS(blobs.initialize(), Status::ok);
    CHECK_STATUS(blobs.initialize(), Status::invalid_state);
    CHECK_EQ(host.live_mutexes(), 1u); }
  CHECK_EQ(host.live_mutexes(), 0u); }

TEST(renderer_host_blobs, changes_owners_on_the_main_loop_only)
{ MockHost host;
  RendererHostBlobs blobs(host);
  CHECK_STATUS(blobs.initialize(), Status::ok);
  CHECK_STATUS(blobs.adopt(first, 3u), Status::ok);
  struct Call {
      RendererHostBlobs *blobs;
      bool               owned;
  } call = {&blobs, false};
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const c = static_cast<Call *>(context);
                       c->owned = c->blobs->owns(3u, first);   /* what a worker may ask */
                       static_cast<void>(c->blobs->adopt(first + 1u, 3u));
                       c->blobs->forget(first + 1u);
                       c->blobs->forget_all();
                   },
                   &call, thread),
               Status::ok);
  host.join_thread(thread);
  for (int count = 0; count < 3; count++) CHECK(host.claim(HostViolation::main_loop_required));
  CHECK(!host.claim(HostViolation::main_loop_required));
  CHECK(call.owned); }
