#include "miniport/present/present_page_view.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/pages/present_progress.hpp"

#include <cstdint>
#include <cstring>

using av::Irql;
using av::MockPlatform;
using av::PresentPageView;
using av::PresentProgress;
using av::Status;

namespace {

std::uint8_t *page_of(MockPlatform &platform, const PresentPageView &view)
{
    return static_cast<std::uint8_t *>(platform.translate_physical(view.address(), PresentProgress::page_size));
}

}

TEST(present_page_view, a_page_is_a_zeroed_block_the_device_can_address_and_is_freed)
{ MockPlatform platform;
  {
      PresentPageView view(platform);
      CHECK_EQ(view.address(), 0u);
      PresentProgress progress(4u, 1u);
      CHECK_STATUS(view.read(progress), Status::invalid_state);
      view.clear();
      CHECK_STATUS(view.initialize(), Status::ok);
      CHECK(view.address() != 0u);
      CHECK_EQ(page_of(platform, view)[PresentProgress::page_size - 1u], 0u);
      CHECK_STATUS(view.initialize(), Status::invalid_state);
      CHECK_EQ(platform.live_allocations(), 1u);
  }
  CHECK_EQ(platform.live_allocations(), 0u);
  platform.fail_allocation_at(1u);
  PresentPageView starved(platform);
  CHECK_STATUS(starved.initialize(), Status::no_memory);
  CHECK_EQ(starved.address(), 0u); }

TEST(present_page_view, reads_what_the_device_wrote_at_dispatch)
{ MockPlatform    platform;
  PresentPageView view(platform);
  CHECK_STATUS(view.initialize(), Status::ok);
  PresentProgress progress(4u, 1u);
  CHECK_STATUS(view.read(progress), Status::malformed);
  CHECK_EQ(progress.completed(), 4u);
  CHECK_STATUS(PresentProgress(0x100000009u, 2u).encode(page_of(platform, view), PresentProgress::encoded_size),
               Status::ok);
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(view.read(progress), Status::ok);
  platform.set_irql(Irql::passive);
  CHECK_EQ(progress.completed(), 0x100000009u);
  CHECK_EQ(progress.refused(), 2u);
  view.clear();
  CHECK_STATUS(view.read(progress), Status::malformed);
  CHECK_EQ(progress.completed(), 0x100000009u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(present_page_view, a_page_that_says_more_refused_than_completed_says_nothing)
{ MockPlatform    platform;
  PresentPageView view(platform);
  CHECK_STATUS(view.initialize(), Status::ok);
  CHECK_STATUS(PresentProgress(3u, 3u).encode(page_of(platform, view), PresentProgress::encoded_size), Status::ok);
  const std::uint64_t refused = 4u;
  std::memcpy(page_of(platform, view) + PresentProgress::refused_at, &refused, sizeof refused);
  PresentProgress progress(1u, 0u);
  CHECK_STATUS(view.read(progress), Status::malformed);
  CHECK_EQ(progress.completed(), 1u); }
