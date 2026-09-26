/* What a queue's handle keeps for vkQueueWaitIdle: the fence it waits on, made
 * once and kept, and the connection's submission count when it was last found
 * idle, which is what lets a wait with nothing sent since answer without the
 * host. */
#include "harness/framework/test_registry.hpp"
#include "icd/vulkan/loader_handle.hpp"

#include <cstdint>

using av::icd::LoaderHandle;

namespace {

constexpr std::uint64_t queue_host = 0x6600u;

}  // namespace

TEST(loader_handle, a_queue_has_no_idle_fence_until_one_is_kept)
{ LoaderHandle queue(LoaderHandle::Kind::queue, queue_host, nullptr, nullptr);
  CHECK_EQ(queue.idle_fence(), 0u);
  queue.keep_idle_fence(0x8800u);
  CHECK_EQ(queue.idle_fence(), 0x8800u);
  /* Given back with its device: kept as nothing again. */
  queue.keep_idle_fence(0u);
  CHECK_EQ(queue.idle_fence(), 0u); }

/* All ones until marked, so that no submission count - 0 included, which a
 * connection that has sent nothing has - reads as idle before a wait said so. */
TEST(loader_handle, a_queue_was_never_idle_until_marked_and_then_at_that_count)
{ LoaderHandle queue(LoaderHandle::Kind::queue, queue_host, nullptr, nullptr);
  CHECK(queue.idle_at() == ~std::uint64_t{0});
  CHECK(queue.idle_at() != 0u);
  queue.mark_idle(0u);
  CHECK_EQ(queue.idle_at(), 0u);
  queue.mark_idle(41u);
  CHECK_EQ(queue.idle_at(), 41u);
  /* Marking leaves the fence alone, and keeping a fence the mark. */
  CHECK_EQ(queue.idle_fence(), 0u);
  queue.keep_idle_fence(0x8801u);
  CHECK_EQ(queue.idle_at(), 41u); }
