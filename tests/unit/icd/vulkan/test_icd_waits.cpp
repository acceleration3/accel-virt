/* Waits as the loader reaches them through the root (research.md §6): every
 * host wait is at most 500 ms, repeated while the host times out until the
 * guest's own counter passes the deadline. MockKmt's counter moves 500 ms with
 * each host wait, as a host that used its whole slice would; the host is played
 * wait by wait. A queue or device waits idle by submitting nothing with a fence
 * on each queue and waiting for the fence in slices. Invariants after every call
 * that changes the root. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "icd/common/sliced_wait.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/client/icd_handle_registry.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>

using av::CommandHeader;
using av::IcdRig;
using av::MockKmt;
using av::ScriptedHost;
using av::WireReader;
using av::icd::LoaderHandle;
using av::icd::SlicedWait;
using Kind = av::icd::LoaderHandle::Kind;
using namespace av::vkproto;

namespace {

using CreateDevice = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
                                  VkDevice *);
using GetDeviceQueue = void (*)(VkDevice, std::uint32_t, std::uint32_t, VkQueue *);
using WaitForFences = VkResult (*)(VkDevice, std::uint32_t, const VkFence *, VkBool32, std::uint64_t);
using WaitSemaphores = VkResult (*)(VkDevice, const VkSemaphoreWaitInfo *, std::uint64_t);
using QueueWaitIdle = VkResult (*)(VkQueue);
using DeviceWaitIdle = VkResult (*)(VkDevice);

constexpr std::uint64_t ms = 1000000u;
constexpr std::uint64_t slice_ticks = MockKmt::ticks_per_second / 2u;
constexpr std::uint64_t fence_host = 0x8800u;
constexpr std::uint64_t semaphore_host = 0x8900u;
constexpr std::uint64_t queue_host = 0x6600u;

class WaitRig {
public:
    WaitRig()
    {
        instance_ = rig_.create_instance();
        const VkPhysicalDevice physical_device = rig_.physical_device(instance_);
        VkDeviceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        rig_.host().vk_handle(0, ScriptedHost::vk_device);
        CHECK_EQ(entry<CreateDevice>("vkCreateDevice")(physical_device, &info, nullptr, &device_), VK_SUCCESS);
        rig_.kmt().spend_on_wait(slice_ticks);
    }
    ~WaitRig() { rig_.destroy_instance(instance_); }

    IcdRig  &rig() { return rig_; }
    MockKmt &kmt() { return rig_.kmt(); }
    VkDevice device() const { return device_; }

    template <typename Function>
    Function entry(const char *name) { return rig_.entry<Function>(instance_, name); }

    void answer(std::int32_t result, std::uint32_t times)
    {
        for (std::uint32_t index = 0u; index < times; index++) rig_.host().vk_result(result);
    }

    VkResult wait_for_fence(std::uint64_t timeout)
    {
        const VkFence fence = reinterpret_cast<VkFence>(fence_host);
        const VkResult result = entry<WaitForFences>("vkWaitForFences")(device_, 1u, &fence, VK_TRUE, timeout);
        CHECK(rig_.root().invariants_hold());
        return result;
    }

    VkQueue queue(std::uint64_t host)
    {
        VkQueue made = VK_NULL_HANDLE;
        rig_.host().vk_handle(0, host);
        entry<GetDeviceQueue>("vkGetDeviceQueue")(device_, 0u, static_cast<std::uint32_t>(host - queue_host), &made);
        return made;
    }

    /* The id of the command journalled `back` from the last (0 the last). */
    std::uint32_t id_back(std::uint32_t back) { return kmt().command_id(kmt().commands() - 1u - back); }

    WireReader body_back(std::uint32_t back)
    {
        return WireReader(kmt().command_bytes(kmt().commands() - 1u - back) + CommandHeader::encoded_size,
                          MockKmt::journal_bytes - CommandHeader::encoded_size);
    }

    /* The timeout a journalled wait sent, its command's last eight bytes. */
    std::uint64_t timeout_back(std::uint32_t back)
    {
        const std::uint32_t index = kmt().commands() - 1u - back;
        return WireReader(kmt().command_bytes(index) + kmt().command_size(index) - 8u, 8u).read_u64();
    }

private:
    IcdRig     rig_;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDevice   device_ = VK_NULL_HANDLE;
};

}  // namespace

TEST(icd_waits, a_1200_ms_fence_wait_is_three_host_waits_of_at_most_500_ms_and_times_out)
{ WaitRig rig;
  const std::uint32_t waits = rig.kmt().calls(MockKmt::Call::wait_for_context);
  rig.answer(VK_TIMEOUT, 3u);
  CHECK_EQ(rig.wait_for_fence(1200u * ms), VK_TIMEOUT);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), waits + 3u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  /* The last slice is what is left: each of the first two also spent the
   * millisecond the transport watches its reply slot before the kernel's wait. */
  const std::uint64_t sent[] = {198u * ms, 500u * ms, 500u * ms};
  for (std::uint32_t back = 0u; back < 3u; back++) {
      CHECK_EQ(rig.id_back(back), static_cast<std::uint32_t>(kCmd_vkWaitForFences));
      CHECK_EQ(rig.timeout_back(back), sent[back]);
      WireReader body = rig.body_back(back);
      CHECK_EQ(body.read_u64(), ScriptedHost::vk_device);
      CHECK_EQ(body.read_u32(), 1u);
      CHECK_EQ(body.read_u64(), fence_host);
      CHECK_EQ(body.read_u32(), static_cast<std::uint32_t>(VK_TRUE));
  }
}

TEST(icd_waits, a_fence_signalled_in_the_second_slice_answers_success)
{ WaitRig rig;
  rig.answer(VK_TIMEOUT, 1u);
  rig.answer(VK_SUCCESS, 1u);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.wait_for_fence(1200u * ms), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.timeout_back(0u), 500u * ms); }

TEST(icd_waits, a_zero_timeout_is_one_host_wait_of_zero)
{ WaitRig rig;
  rig.answer(VK_TIMEOUT, 1u);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.wait_for_fence(0u), VK_TIMEOUT);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.timeout_back(0u), 0u); }

TEST(icd_waits, an_infinite_wait_asks_again_in_slices_until_the_host_answers)
{ WaitRig rig;
  rig.answer(VK_TIMEOUT, 6u);
  rig.answer(VK_SUCCESS, 1u);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.wait_for_fence(SlicedWait::forever), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 7u);
  for (std::uint32_t back = 0u; back < 7u; back++) CHECK_EQ(rig.timeout_back(back), 500u * ms); }

TEST(icd_waits, a_lost_device_answer_or_a_lost_transport_ends_the_wait)
{ WaitRig rig;
  rig.answer(VK_ERROR_DEVICE_LOST, 1u);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.wait_for_fence(1200u * ms), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.wait_for_fence(SlicedWait::forever), VK_ERROR_DEVICE_LOST); }

TEST(icd_waits, semaphores_wait_in_the_same_slices_under_either_spelling)
{ WaitRig rig;
  const VkSemaphore semaphore = reinterpret_cast<VkSemaphore>(semaphore_host);
  VkSemaphoreWaitInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  info.semaphoreCount = 1u;
  info.pSemaphores = &semaphore;
  const std::uint64_t value = 3u;
  info.pValues = &value;
  const char *const spellings[] = {"vkWaitSemaphores", "vkWaitSemaphoresKHR"};
  for (const char *name : spellings) {
      rig.answer(VK_TIMEOUT, 3u);
      const std::uint32_t sent = rig.kmt().commands();
      CHECK_EQ(rig.entry<WaitSemaphores>(name)(rig.device(), &info, 1200u * ms), VK_TIMEOUT);
      CHECK(rig.rig().root().invariants_hold());
      CHECK_EQ(rig.kmt().commands(), sent + 3u);
      /* Less the millisecond each earlier wait watched its reply slot. */
      const std::uint64_t slices[] = {198u * ms, 500u * ms, 500u * ms};
      for (std::uint32_t back = 0u; back < 3u; back++) {
          CHECK_EQ(rig.id_back(back), static_cast<std::uint32_t>(kCmd_vkWaitSemaphores));
          CHECK_EQ(rig.timeout_back(back), slices[back]);
          CHECK_EQ(rig.body_back(back).read_u64(), ScriptedHost::vk_device);
      }
  }
  rig.answer(VK_SUCCESS, 1u);
  CHECK_EQ(rig.rig().root().wait_semaphores(rig.device(), &info, 0u), VK_SUCCESS);
  CHECK_EQ(rig.timeout_back(0u), 0u); }

TEST(icd_waits, a_wait_on_a_device_it_did_not_make_is_lost)
{ WaitRig rig;
  int stack = 0;
  const VkDevice foreign = reinterpret_cast<VkDevice>(&stack);
  const VkFence fence = VK_NULL_HANDLE;
  const std::uint32_t sent = rig.kmt().calls(MockKmt::Call::submit);
  CHECK_EQ(rig.entry<WaitForFences>("vkWaitForFences")(foreign, 1u, &fence, VK_TRUE, 0u), VK_ERROR_DEVICE_LOST);
  VkSemaphoreWaitInfo info = {};
  CHECK_EQ(rig.entry<WaitSemaphores>("vkWaitSemaphores")(foreign, &info, 0u), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<DeviceWaitIdle>("vkDeviceWaitIdle")(foreign), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<QueueWaitIdle>("vkQueueWaitIdle")(reinterpret_cast<VkQueue>(&stack)), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), sent);
  CHECK_EQ(rig.rig().log().holding("on a device this driver did not make"), 3u);
  CHECK_EQ(rig.rig().log().holding("vkQueueWaitIdle on a queue this driver did not make through a device"), 1u); }

/* An empty submission completes after everything already submitted on its
 * queue, so its fence signals when the queue is idle. The fence is the
 * queue's own from then on, kept for its next idle wait. */
TEST(icd_waits, a_queue_waits_idle_on_a_fence_submitted_with_nothing_in_slices)
{ WaitRig rig;
  const VkQueue queue = rig.queue(queue_host);
  CHECK(queue != VK_NULL_HANDLE);
  rig.rig().host().vk_handle(0, fence_host);
  rig.answer(VK_TIMEOUT, 2u);
  rig.answer(VK_SUCCESS, 1u);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.entry<QueueWaitIdle>("vkQueueWaitIdle")(queue), VK_SUCCESS);
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(rig.kmt().commands(), sent + 5u);
  CHECK_EQ(rig.id_back(4u), static_cast<std::uint32_t>(kCmd_vkCreateFence));
  CHECK_EQ(rig.body_back(4u).read_u64(), ScriptedHost::vk_device);
  CHECK_EQ(rig.id_back(3u), static_cast<std::uint32_t>(kCmd_vkQueueSubmit));
  WireReader submitted = rig.body_back(3u);
  CHECK_EQ(submitted.read_u64(), queue_host);
  CHECK_EQ(submitted.read_u32(), 0u);
  CHECK_EQ(submitted.read_u64(), fence_host);
  for (std::uint32_t back = 0u; back <= 2u; back++) {
      CHECK_EQ(rig.id_back(back), static_cast<std::uint32_t>(kCmd_vkWaitForFences));
      CHECK_EQ(rig.timeout_back(back), 500u * ms);
  }
  /* Nothing sent since the queue was found idle: it is still idle, and the
   * host is not asked. */
  const std::uint32_t idle = rig.kmt().commands();
  CHECK_EQ(rig.entry<QueueWaitIdle>("vkQueueWaitIdle")(queue), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), idle); }

/* Once something was sent, the kept fence is reset rather than made again. */
TEST(icd_waits, a_queue_idle_after_a_send_resets_the_fence_it_kept)
{ WaitRig rig;
  const VkQueue first = rig.queue(queue_host);
  const VkQueue second = rig.queue(queue_host + 1u);
  rig.rig().host().vk_handle(0, fence_host);
  rig.answer(VK_SUCCESS, 1u);
  CHECK_EQ(rig.rig().root().queue_wait_idle(first), VK_SUCCESS);
  /* The other queue's wait is a send on the same connection. */
  rig.rig().host().vk_handle(0, fence_host + 1u);
  rig.answer(VK_SUCCESS, 1u);
  CHECK_EQ(rig.rig().root().queue_wait_idle(second), VK_SUCCESS);
  rig.answer(VK_SUCCESS, 1u);   /* the wait; the reset is not answered */
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().queue_wait_idle(first), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 3u);
  CHECK_EQ(rig.id_back(2u), static_cast<std::uint32_t>(kCmd_vkResetFences));
  CHECK_EQ(rig.id_back(1u), static_cast<std::uint32_t>(kCmd_vkQueueSubmit));
  WireReader submitted = rig.body_back(1u);
  CHECK_EQ(submitted.read_u64(), queue_host);
  CHECK_EQ(submitted.read_u32(), 0u);
  CHECK_EQ(submitted.read_u64(), fence_host);
  CHECK_EQ(rig.id_back(0u), static_cast<std::uint32_t>(kCmd_vkWaitForFences));
  /* The first queue's own wait was the last send: it is idle again, and the
   * second, sent past, is not. */
  const std::uint32_t idle = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().queue_wait_idle(first), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), idle);
  rig.answer(VK_SUCCESS, 1u);   /* the wait; the reset is not answered */
  CHECK_EQ(rig.rig().root().queue_wait_idle(second), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), idle + 3u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.rig().root().invariants_hold()); }

TEST(icd_waits, a_queue_idle_passes_on_a_refused_fence_a_refused_submission_and_a_lost_wait)
{ WaitRig rig;
  const VkQueue queue = rig.queue(queue_host);
  rig.rig().host().vk_handle(VK_ERROR_OUT_OF_DEVICE_MEMORY, 0u);
  std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().queue_wait_idle(queue), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  rig.rig().host().vk_handle(0, fence_host);
  rig.answer(VK_ERROR_DEVICE_LOST, 1u);
  sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().queue_wait_idle(queue), VK_ERROR_DEVICE_LOST);
  /* The fence made, the submission and the wait; the fence is kept. */
  CHECK_EQ(rig.kmt().commands(), sent + 3u);
  CHECK_EQ(rig.id_back(0u), static_cast<std::uint32_t>(kCmd_vkWaitForFences));
  /* The kept fence is reset, unanswered, and the fence's submission after it -
   * the second submission from here - is refused, which loses the context. */
  rig.kmt().fail_at(MockKmt::Call::submit, 2u, MockKmt::invalid_parameter);
  CHECK_EQ(rig.rig().root().queue_wait_idle(queue), VK_ERROR_DEVICE_LOST);
  CHECK(av::icd::connection_of(queue)->faulted());
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.rig().root().invariants_hold()); }

TEST(icd_waits, a_queue_not_made_through_a_device_cannot_wait_idle)
{ WaitRig rig;
  void *const queue = rig.rig().root().wrap(Kind::queue, queue_host, av::icd::connection_of(rig.device()));
  CHECK(queue != nullptr);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().queue_wait_idle(static_cast<VkQueue>(queue)), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.kmt().commands(), sent); }

TEST(icd_waits, a_device_waits_idle_on_every_queue_it_handed_out_in_turn)
{ WaitRig rig;
  /* No queue handed out: nothing can have been submitted. */
  std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.entry<DeviceWaitIdle>("vkDeviceWaitIdle")(rig.device()), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent);
  const VkQueue first = rig.queue(queue_host);
  const VkQueue second = rig.queue(queue_host + 1u);
  CHECK(first != second);
  for (std::uint32_t queue = 0u; queue < 2u; queue++) {
      rig.rig().host().vk_handle(0, fence_host + queue);
      rig.answer(VK_SUCCESS, 1u);
  }
  sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().device_wait_idle(rig.device()), VK_SUCCESS);
  CHECK(rig.rig().root().invariants_hold());
  /* Each queue: its fence made, the empty submission, the wait. */
  CHECK_EQ(rig.kmt().commands(), sent + 6u);
  WireReader later = rig.body_back(1u);
  WireReader earlier = rig.body_back(4u);
  CHECK_EQ(rig.id_back(1u), static_cast<std::uint32_t>(kCmd_vkQueueSubmit));
  CHECK_EQ(rig.id_back(4u), static_cast<std::uint32_t>(kCmd_vkQueueSubmit));
  const std::uint64_t submitted[] = {earlier.read_u64(), later.read_u64()};
  CHECK(submitted[0] != submitted[1]);
  CHECK(submitted[0] == queue_host || submitted[0] == queue_host + 1u);
  CHECK(submitted[1] == queue_host || submitted[1] == queue_host + 1u);
  /* The first queue waited before the second's sends, so only the second is
   * idle now; a device wait asks the first again, on the fence it kept. Its
   * failed wait is the answer, and the second is not asked. */
  rig.answer(VK_ERROR_OUT_OF_HOST_MEMORY, 1u);
  sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().device_wait_idle(rig.device()), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().commands(), sent + 3u);
  CHECK_EQ(rig.id_back(2u), static_cast<std::uint32_t>(kCmd_vkResetFences));
  CHECK_EQ(rig.id_back(1u), static_cast<std::uint32_t>(kCmd_vkQueueSubmit));
  CHECK_EQ(rig.id_back(0u), static_cast<std::uint32_t>(kCmd_vkWaitForFences));
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }
