#include "executor/vulkan/vulkan_timeline_submit.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace {

VkSemaphore semaphore_named(std::uintptr_t number)
{ return reinterpret_cast<VkSemaphore>(number); }

}

TEST(vulkan_timeline_submit, a_sparse_bind_gets_the_progress_semaphore_signalled_after_its_own)
{ av::MockPlatform platform;
  av::VulkanTimelineSubmit stamper(platform);
  const VkSemaphore own[1] = {semaphore_named(0x10u)};
  const std::uint64_t values[1] = {5u};
  VkTimelineSemaphoreSubmitInfo guest = {};
  guest.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  guest.signalSemaphoreValueCount = 1u;
  guest.pSignalSemaphoreValues = values;
  VkBindSparseInfo binds[2] = {};
  binds[0].sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
  binds[1].sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
  binds[1].pNext = &guest;
  binds[1].signalSemaphoreCount = 1u;
  binds[1].pSignalSemaphores = own;
  std::uint32_t appended = 0u;
  const VkBindSparseInfo *const sent = stamper.append(binds, 2u, semaphore_named(0x20u), 9u, appended);
  CHECK(sent != nullptr);
  if (sent == nullptr) return;
  CHECK_EQ(appended, 2u);
  CHECK_EQ(sent[0].signalSemaphoreCount, 0u);
  CHECK_EQ(sent[1].signalSemaphoreCount, 2u);
  CHECK(sent[1].pSignalSemaphores[0] == own[0]);
  CHECK(sent[1].pSignalSemaphores[1] == semaphore_named(0x20u));
  const auto *const timeline = static_cast<const VkTimelineSemaphoreSubmitInfo *>(sent[1].pNext);
  CHECK_EQ(timeline->signalSemaphoreValueCount, 2u);
  CHECK_EQ(timeline->pSignalSemaphoreValues[0], std::uint64_t{5u});
  CHECK_EQ(timeline->pSignalSemaphoreValues[1], std::uint64_t{9u}); }

TEST(vulkan_timeline_submit, a_sparse_bind_with_no_info_still_signals_the_progress_semaphore)
{ av::MockPlatform platform;
  av::VulkanTimelineSubmit stamper(platform);
  std::uint32_t appended = 0u;
  const VkBindSparseInfo *const sent = stamper.append(static_cast<const VkBindSparseInfo *>(nullptr), 0u,
                                                      semaphore_named(0x20u), 3u, appended);
  CHECK(sent != nullptr);
  if (sent == nullptr) return;
  CHECK_EQ(appended, 1u);
  CHECK_EQ(sent[0].sType, VK_STRUCTURE_TYPE_BIND_SPARSE_INFO);
  CHECK_EQ(sent[0].signalSemaphoreCount, 1u);
  const auto *const timeline = static_cast<const VkTimelineSemaphoreSubmitInfo *>(sent[0].pNext);
  CHECK_EQ(timeline->pSignalSemaphoreValues[0], std::uint64_t{3u}); }
