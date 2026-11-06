#include "harness/framework/test_registry.hpp"
#include "icd/common/frame_wait.hpp"
#include "icd/vulkan/entry_lookup.hpp"
#include "icd/vulkan/fence_ready.hpp"

#include <cstdint>
#include <cstring>

using av::icd::EntryLookup;
using av::icd::FenceReady;
using av::icd::FrameWait;

namespace {

VkResult wait_answer = VK_SUCCESS;
VkResult reset_answer = VK_SUCCESS;
std::uint32_t waits = 0u;
std::uint32_t resets = 0u;
std::uint64_t waited_for = 0u;
VkDevice      waited_on = VK_NULL_HANDLE;
VkFence       waited_fence = VK_NULL_HANDLE;
VkBool32      waited_all = VK_FALSE;

VKAPI_ATTR VkResult VKAPI_CALL wait_for_fences(VkDevice device, uint32_t count, const VkFence *fences, VkBool32 all,
                                                uint64_t timeout)
{
    waits++;
    waited_on = device;
    waited_fence = count == 1u ? fences[0] : VK_NULL_HANDLE;
    waited_all = all;
    waited_for = timeout;
    return wait_answer;
}

VKAPI_ATTR VkResult VKAPI_CALL reset_fences(VkDevice, uint32_t, const VkFence *)
{
    resets++;
    return reset_answer;
}

class Lookup final : public EntryLookup {
public:
    PFN_vkVoidFunction entry(const char *name) override
    {
        if (hide_ != nullptr && std::strcmp(hide_, name) == 0) return nullptr;
        if (std::strcmp(name, "vkWaitForFences") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&wait_for_fences);
        if (std::strcmp(name, "vkResetFences") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&reset_fences);
        return nullptr;
    }

    void hide(const char *name) { hide_ = name; }

private:
    const char *hide_ = nullptr;
};

void reset_script()
{
    wait_answer = VK_SUCCESS;
    reset_answer = VK_SUCCESS;
    waits = 0u;
    resets = 0u;
}

const FenceReady::Mark mark{reinterpret_cast<VkDevice>(0x9u), reinterpret_cast<VkFence>(0x41u)};

}

TEST(fence_ready, a_signaled_fence_is_waited_for_in_one_slice_reset_and_is_drawn)
{ reset_script();
  Lookup     entries;
  FenceReady ready(entries);
  CHECK(ready.wait(mark) == FrameWait::drawn);
  CHECK_EQ(waits, 1u);
  CHECK_EQ(resets, 1u);
  CHECK(waited_on == mark.device_);
  CHECK(waited_fence == mark.fence_);
  CHECK_EQ(waited_all, VK_TRUE);
  CHECK_EQ(waited_for, FenceReady::slice_ns); }

TEST(fence_ready, a_fence_that_times_out_is_later_and_is_not_reset)
{ reset_script();
  wait_answer = VK_TIMEOUT;
  Lookup     entries;
  FenceReady ready(entries);
  CHECK(ready.wait(mark) == FrameWait::later);
  CHECK_EQ(resets, 0u); }

TEST(fence_ready, a_lost_device_a_failed_reset_a_missing_entry_or_no_fence_means_the_frame_is_gone)
{ reset_script();
  Lookup     entries;
  FenceReady ready(entries);
  wait_answer = VK_ERROR_DEVICE_LOST;
  CHECK(ready.wait(mark) == FrameWait::gone);
  CHECK_EQ(resets, 0u);
  wait_answer = VK_SUCCESS;
  reset_answer = VK_ERROR_OUT_OF_HOST_MEMORY;
  CHECK(ready.wait(mark) == FrameWait::gone);
  reset_answer = VK_SUCCESS;
  const std::uint32_t asked = waits;
  CHECK(ready.wait(FenceReady::Mark{mark.device_, VK_NULL_HANDLE}) == FrameWait::gone);
  CHECK_EQ(waits, asked);
  entries.hide("vkResetFences");
  CHECK(ready.wait(mark) == FrameWait::gone);
  entries.hide("vkWaitForFences");
  CHECK(ready.wait(mark) == FrameWait::gone);
  CHECK_EQ(waits, asked); }
