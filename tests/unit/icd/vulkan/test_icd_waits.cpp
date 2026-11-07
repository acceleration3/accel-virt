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
#include "proto/wire/wire_writer.hpp"

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
        physical_ = rig_.physical_device(instance_);
        VkDeviceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        rig_.host().vk_handle(0, ScriptedHost::vk_device);
        CHECK_EQ(entry<CreateDevice>("vkCreateDevice")(physical_, &info, nullptr, &device_), VK_SUCCESS);
        rig_.kmt().spend_on_wait(slice_ticks);
    }
    ~WaitRig() { rig_.destroy_instance(instance_); }

    IcdRig  &rig() { return rig_; }
    MockKmt &kmt() { return rig_.kmt(); }
    VkDevice device() const { return device_; }
    VkPhysicalDevice physical() const { return physical_; }

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

    std::uint32_t id_back(std::uint32_t back) { return kmt().command_id(kmt().commands() - 1u - back); }

    WireReader body_back(std::uint32_t back)
    {
        return WireReader(kmt().command_bytes(kmt().commands() - 1u - back) + CommandHeader::encoded_size,
                          MockKmt::journal_bytes - CommandHeader::encoded_size);
    }

    std::uint64_t timeout_back(std::uint32_t back)
    {
        const std::uint32_t index = kmt().commands() - 1u - back;
        return WireReader(kmt().command_bytes(index) + kmt().command_size(index) - 8u, 8u).read_u64();
    }

private:
    IcdRig     rig_;
    VkInstance       instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDevice         device_ = VK_NULL_HANDLE;
};

}

TEST(icd_waits, a_1200_ms_fence_wait_is_three_non_blocking_host_asks_one_slice_apart_and_times_out)
{ WaitRig rig;
  const std::uint32_t waits = rig.kmt().calls(MockKmt::Call::wait_for_context);
  rig.answer(VK_TIMEOUT, 3u);
  CHECK_EQ(rig.wait_for_fence(1200u * ms), VK_TIMEOUT);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), waits + 3u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  const std::uint64_t sent[] = {0u, 0u, 0u};
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
  CHECK_EQ(rig.timeout_back(0u), 0u); }

TEST(icd_waits, a_zero_timeout_is_one_host_wait_of_zero)
{ WaitRig rig;
  rig.answer(VK_TIMEOUT, 1u);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.wait_for_fence(0u), VK_TIMEOUT);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.timeout_back(0u), 0u); }

TEST(icd_waits, an_infinite_wait_asks_again_without_blocking_the_host_until_it_answers)
{ WaitRig rig;
  rig.answer(VK_TIMEOUT, 6u);
  rig.answer(VK_SUCCESS, 1u);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.wait_for_fence(SlicedWait::forever), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 7u);
  for (std::uint32_t back = 0u; back < 7u; back++) CHECK_EQ(rig.timeout_back(back), 0u); }

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
      CHECK_EQ(rig.timeout_back(back), 0u);
  }
  const std::uint32_t idle = rig.kmt().commands();
  CHECK_EQ(rig.entry<QueueWaitIdle>("vkQueueWaitIdle")(queue), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), idle); }

TEST(icd_waits, a_queue_idle_after_a_send_resets_the_fence_it_kept)
{ WaitRig rig;
  const VkQueue first = rig.queue(queue_host);
  const VkQueue second = rig.queue(queue_host + 1u);
  rig.rig().host().vk_handle(0, fence_host);
  rig.answer(VK_SUCCESS, 1u);
  CHECK_EQ(rig.rig().root().queue_wait_idle(first), VK_SUCCESS);
  rig.rig().host().vk_handle(0, fence_host + 1u);
  rig.answer(VK_SUCCESS, 1u);
  CHECK_EQ(rig.rig().root().queue_wait_idle(second), VK_SUCCESS);
  rig.answer(VK_SUCCESS, 1u);
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
  const std::uint32_t idle = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().queue_wait_idle(first), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), idle);
  rig.answer(VK_SUCCESS, 1u);
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
  CHECK_EQ(rig.kmt().commands(), sent + 3u);
  CHECK_EQ(rig.id_back(0u), static_cast<std::uint32_t>(kCmd_vkWaitForFences));
  rig.kmt().fail_at(MockKmt::Call::submit, 2u, MockKmt::invalid_parameter);
  CHECK_EQ(rig.rig().root().queue_wait_idle(queue), VK_ERROR_DEVICE_LOST);
  CHECK(av::icd::connection_of(queue)->lost());
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
  CHECK_EQ(rig.kmt().commands(), sent + 6u);
  WireReader later = rig.body_back(1u);
  WireReader earlier = rig.body_back(4u);
  CHECK_EQ(rig.id_back(1u), static_cast<std::uint32_t>(kCmd_vkQueueSubmit));
  CHECK_EQ(rig.id_back(4u), static_cast<std::uint32_t>(kCmd_vkQueueSubmit));
  const std::uint64_t submitted[] = {earlier.read_u64(), later.read_u64()};
  CHECK(submitted[0] != submitted[1]);
  CHECK(submitted[0] == queue_host || submitted[0] == queue_host + 1u);
  CHECK(submitted[1] == queue_host || submitted[1] == queue_host + 1u);
  rig.answer(VK_ERROR_OUT_OF_HOST_MEMORY, 1u);
  sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().device_wait_idle(rig.device()), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().commands(), sent + 3u);
  CHECK_EQ(rig.id_back(2u), static_cast<std::uint32_t>(kCmd_vkResetFences));
  CHECK_EQ(rig.id_back(1u), static_cast<std::uint32_t>(kCmd_vkQueueSubmit));
  CHECK_EQ(rig.id_back(0u), static_cast<std::uint32_t>(kCmd_vkWaitForFences));
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(icd_waits, signal_acquire_submits_nothing_but_the_signal_on_the_first_queue)
{ WaitRig rig;
  const VkSemaphore semaphore = reinterpret_cast<VkSemaphore>(semaphore_host);
  const VkFence     fence = reinterpret_cast<VkFence>(fence_host);
  std::uint32_t     sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().signal_acquire(rig.device(), VK_NULL_HANDLE, VK_NULL_HANDLE), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent);
  int stack = 0;
  CHECK_EQ(rig.rig().root().signal_acquire(reinterpret_cast<VkDevice>(&stack), semaphore, fence),
           VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.rig().root().signal_acquire(rig.device(), semaphore, fence), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.rig().log().holding("vkAcquireNextImageKHR has nothing to signal on"), 1u);
  CHECK_EQ(rig.kmt().commands(), sent);

  static_cast<void>(rig.queue(queue_host + 1u));
  static_cast<void>(rig.queue(queue_host));
  sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().signal_acquire(rig.device(), semaphore, fence), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.id_back(0u), static_cast<std::uint32_t>(kCmd_vkQueueSubmit));
  WireReader submitted = rig.body_back(0u);
  CHECK_EQ(submitted.read_u64(), queue_host + 1u);
  CHECK_EQ(submitted.read_u32(), 1u);
  CHECK_EQ(rig.rig().root().signal_acquire(rig.device(), VK_NULL_HANDLE, fence), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK(rig.rig().root().invariants_hold()); }

TEST(icd_waits, present_context_answers_a_device_s_physical_device_and_connection)
{ WaitRig rig;
  VkPhysicalDevice  physical = VK_NULL_HANDLE;
  av::icd::Connection *connection = nullptr;
  std::uint64_t        host_device = 0u;
  CHECK(rig.rig().root().present_context(rig.device(), physical, host_device, connection));
  CHECK_EQ(host_device, ScriptedHost::vk_device);
  CHECK(physical == rig.physical());
  CHECK(connection == av::icd::connection_of(rig.device()));
  CHECK(connection != nullptr);
  int stack = 0;
  physical = VK_NULL_HANDLE;
  connection = nullptr;
  CHECK(!rig.rig().root().present_context(reinterpret_cast<VkDevice>(&stack), physical, host_device, connection));
  CHECK(physical == VK_NULL_HANDLE);
  CHECK(connection == nullptr); }

TEST(icd_waits, a_fence_kept_for_queue_idle_that_cannot_be_reset_ends_the_wait_with_the_error)
{ WaitRig rig;
  const VkQueue first = rig.queue(queue_host);
  const VkQueue second = rig.queue(queue_host + 1u);
  rig.rig().host().vk_handle(0, fence_host);
  rig.answer(VK_SUCCESS, 1u);
  CHECK_EQ(rig.rig().root().queue_wait_idle(first), VK_SUCCESS);
  rig.rig().host().vk_handle(0, fence_host + 1u);
  rig.answer(VK_SUCCESS, 1u);
  CHECK_EQ(rig.rig().root().queue_wait_idle(second), VK_SUCCESS);
  rig.kmt().fail_at(MockKmt::Call::submit, 1u, MockKmt::invalid_parameter);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().queue_wait_idle(first), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(av::icd::connection_of(first)->lost());
  CHECK(rig.rig().root().invariants_hold()); }

TEST(icd_waits, an_acquire_signal_on_a_lost_connection_is_device_lost)
{ WaitRig rig;
  static_cast<void>(rig.queue(queue_host));
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.wait_for_fence(SlicedWait::forever), VK_ERROR_DEVICE_LOST);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().signal_acquire(rig.device(), reinterpret_cast<VkSemaphore>(semaphore_host), VK_NULL_HANDLE),
           VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.rig().root().invariants_hold()); }

TEST(icd_waits, query_results_come_from_the_host_at_once_or_after_a_wait_and_are_refused_for_a_lost_or_foreign_device)
{ WaitRig rig;
  std::uint64_t results[1] = {};
  const VkQueryPool pool = reinterpret_cast<VkQueryPool>(0x6100u);
  const auto answer = [&rig](std::int32_t result, std::uint64_t value) {
      std::uint8_t bytes[MockKmt::max_answer] = {};
      av::WireWriter writer(bytes, sizeof bytes);
      writer.write_pod(result);
      writer.write_pod(value);
      rig.kmt().queue_answer(bytes, static_cast<std::uint32_t>(writer.offset()));
  };
  answer(VK_SUCCESS, 77u);
  CHECK_EQ(rig.rig().root().query_pool_results(rig.device(), pool, 0u, 1u, sizeof results, results, sizeof results[0],
                                               VK_QUERY_RESULT_64_BIT),
           VK_SUCCESS);
  CHECK_EQ(results[0], 77u);
  answer(VK_NOT_READY, 0u);
  answer(VK_SUCCESS, 88u);
  CHECK_EQ(rig.rig().root().query_pool_results(rig.device(), pool, 0u, 1u, sizeof results, results, sizeof results[0],
                                               VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
           VK_SUCCESS);
  CHECK_EQ(results[0], 88u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  int stack = 0;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().query_pool_results(reinterpret_cast<VkDevice>(&stack), pool, 0u, 1u, sizeof results, results,
                                               sizeof results[0], VK_QUERY_RESULT_64_BIT),
           VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.rig().log().holding("vkGetQueryPoolResults on a device this driver did not make"), 1u);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.wait_for_fence(SlicedWait::forever), VK_ERROR_DEVICE_LOST);
  const std::uint32_t lost_at = rig.kmt().commands();
  CHECK_EQ(rig.rig().root().query_pool_results(rig.device(), pool, 0u, 1u, sizeof results, results, sizeof results[0],
                                               VK_QUERY_RESULT_64_BIT),
           VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.kmt().commands(), lost_at);
  CHECK(sent <= lost_at);
  CHECK(rig.rig().root().invariants_hold()); }
