#include "harness/framework/test_registry.hpp"
#include "icd/vulkan/loader_handle.hpp"

#include <cstdint>

using av::icd::LoaderHandle;

namespace {

constexpr std::uint64_t queue_host = 0x6600u;

}

TEST(loader_handle, a_queue_has_no_idle_fence_until_one_is_kept)
{ LoaderHandle queue(LoaderHandle::Kind::queue, queue_host, nullptr, nullptr);
  CHECK_EQ(queue.idle_fence(), 0u);
  queue.keep_idle_fence(0x8800u);
  CHECK_EQ(queue.idle_fence(), 0x8800u);
  queue.keep_idle_fence(0u);
  CHECK_EQ(queue.idle_fence(), 0u); }

TEST(loader_handle, a_queue_was_never_idle_until_marked_and_then_at_that_count)
{ LoaderHandle queue(LoaderHandle::Kind::queue, queue_host, nullptr, nullptr);
  CHECK(queue.idle_at() == ~std::uint64_t{0});
  CHECK(queue.idle_at() != 0u);
  queue.mark_idle(0u);
  CHECK_EQ(queue.idle_at(), 0u);
  queue.mark_idle(41u);
  CHECK_EQ(queue.idle_at(), 41u);
  CHECK_EQ(queue.idle_fence(), 0u);
  queue.keep_idle_fence(0x8801u);
  CHECK_EQ(queue.idle_at(), 41u); }
