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
constexpr std::uint32_t last = 0xFFFFFFFFu;

}

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
  CHECK_STATUS(blobs.adopt(first, 5u), Status::ok);
  CHECK(blobs.owns(5u, first));
  CHECK(!blobs.owns(3u, first));
  CHECK_EQ(blobs.live(), 2u); }

TEST(renderer_host_blobs_adopt, refuses_an_id_outside_the_range_context_zero_or_no_lock)
{ MockHost host;
  RendererHostBlobs blobs(host);
  CHECK_STATUS(blobs.adopt(first, 3u), Status::invalid_parameter);
  CHECK_STATUS(blobs.initialize(), Status::ok);
  CHECK_STATUS(blobs.adopt(first - 1u, 3u), Status::invalid_parameter);
  CHECK_STATUS(blobs.adopt(0u, 3u), Status::invalid_parameter);
  CHECK_STATUS(blobs.adopt(first, 0u), Status::invalid_parameter);
  CHECK_EQ(blobs.live(), 0u);
  CHECK(!blobs.owns(0u, first)); }

TEST(renderer_host_blobs_adopt, keeps_owners_well_past_the_old_range)
{ MockHost host;
  RendererHostBlobs blobs(host);
  CHECK_STATUS(blobs.initialize(), Status::ok);
  constexpr std::uint32_t many = 40000u;
  for (std::uint32_t index = 0u; index < many; index++)
      CHECK_STATUS(blobs.adopt(first + index * 3u, 1u + index % 7u), Status::ok);
  CHECK_EQ(blobs.live(), many);
  for (std::uint32_t index = 0u; index < many; index++) CHECK(blobs.owns(1u + index % 7u, first + index * 3u));
  CHECK(!blobs.owns(1u, first + 1u));
  for (std::uint32_t index = 0u; index < many; index += 2u) blobs.forget(first + index * 3u);
  CHECK_EQ(blobs.live(), many / 2u);
  CHECK(!blobs.owns(1u, first));
  CHECK(blobs.owns(2u, first + 3u)); }

TEST(renderer_host_blobs_adopt, an_owner_with_nowhere_to_be_kept_is_out_of_memory)
{ MockHost host;
  RendererHostBlobs blobs(host);
  CHECK_STATUS(blobs.initialize(), Status::ok);
  host.fail_allocation_at(1u);
  CHECK_STATUS(blobs.adopt(first, 3u), Status::no_memory);
  CHECK(!blobs.owns(3u, first));
  CHECK_EQ(blobs.live(), 0u);
  CHECK_STATUS(blobs.adopt(first, 3u), Status::ok);
  CHECK(blobs.owns(3u, first)); }

TEST(renderer_host_blobs_forget, drops_one_owner_or_every_one)
{ MockHost host;
  RendererHostBlobs blobs(host);
  blobs.forget(first);
  blobs.forget_all();
  CHECK_STATUS(blobs.initialize(), Status::ok);
  CHECK_STATUS(blobs.adopt(first, 3u), Status::ok);
  CHECK_STATUS(blobs.adopt(first + 1u, 3u), Status::ok);
  CHECK_STATUS(blobs.adopt(first + 2u, 4u), Status::ok);
  blobs.forget(first + 1u);
  blobs.forget(first + 9u);
  blobs.forget(7u);
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
                       c->owned = c->blobs->owns(3u, first);
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
