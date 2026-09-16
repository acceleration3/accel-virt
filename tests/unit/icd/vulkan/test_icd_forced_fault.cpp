/* The probe's forced fault (feature 045, T037): vkRvkForceContextFault is a
 * physical-device-level entry point, so the Windows loader reaches it through
 * vk_icdGetPhysicalDeviceProcAddr and calls it with the ICD's own physical
 * device. It sends a vkCreateInstance cut after its structure type on the
 * instance's own transport; the host faults the context (MockKmt withholding
 * the answer), the call answers VK_ERROR_DEVICE_LOST, and every later call on
 * that instance answers the loss while a new instance works. Invariants after
 * every call that changes the root. */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::IcdRig;
using av::MockKmt;
using av::ScriptedHost;
using Kind = av::icd::LoaderHandle::Kind;

namespace {

using ForceFault = VkResult (*)(VkPhysicalDevice);
using CreateDevice = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
                                  VkDevice *);
using DeviceWaitIdle = VkResult (*)(VkDevice);

constexpr const char *entry_name = "vkRvkForceContextFault";

std::uint32_t kmt_calls(const MockKmt &kmt)
{
    std::uint32_t total = 0u;
    for (unsigned call = 0u; call < static_cast<unsigned>(MockKmt::Call::count); call++)
        total += kmt.calls(static_cast<MockKmt::Call>(call));
    return total;
}

class FaultRig {
public:
    FaultRig()
    {
        instance_ = rig_.create_instance();
        physical_ = rig_.physical_device(instance_);
        CHECK(physical_ != VK_NULL_HANDLE);
    }
    ~FaultRig()
    {
        if (instance_ != VK_NULL_HANDLE) rig_.destroy_instance(instance_);
        CHECK(rig_.root().invariants_hold());
        CHECK_EQ(rig_.kmt().open_objects(), 0u);
        CHECK_EQ(rig_.kmt().misuses(), 0u);
    }

    IcdRig          &rig() { return rig_; }
    MockKmt         &kmt() { return rig_.kmt(); }
    VkInstance       instance() const { return instance_; }
    VkPhysicalDevice physical() const { return physical_; }
    av::icd::Connection *connection() { return rig_.root().connection_of(instance_, Kind::instance); }
    ForceFault       force() { return rig_.entry<ForceFault>(instance_, entry_name); }

    VkDevice device()
    {
        VkDeviceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        rig_.host().vk_handle(0, ScriptedHost::vk_device);
        VkDevice made = VK_NULL_HANDLE;
        CHECK_EQ(rig_.entry<CreateDevice>(instance_, "vkCreateDevice")(physical_, &info, nullptr, &made),
                 VK_SUCCESS);
        return made;
    }

private:
    IcdRig           rig_;
    VkInstance       instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
};

}  // namespace

/* The loader asks for it by name on a physical device, and the instance's
 * table answers the same function; before an instance exists nothing does. */
TEST(icd_forced_fault, it_is_a_physical_device_entry_point)
{ FaultRig rig;
  const PFN_vkVoidFunction by_device = rig.rig().root().physical_device_proc_addr(entry_name);
  CHECK(by_device != nullptr);
  CHECK(by_device == reinterpret_cast<PFN_vkVoidFunction>(rig.force()));
  CHECK(rig.rig().root().instance_proc_addr(VK_NULL_HANDLE, entry_name) == nullptr); }

/* The host faults the context the cut frame reached: the call answers the
 * loss, the frame on the ring is vkCreateInstance with four bytes of body,
 * and the instance's device answers the loss from then on without a Kmt call. */
TEST(icd_forced_fault, a_withheld_answer_loses_the_instance)
{ FaultRig rig;
  const VkDevice device = rig.device();
  const std::uint32_t sent = rig.kmt().commands();
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.force()(rig.physical()), VK_ERROR_DEVICE_LOST);
  CHECK(rig.rig().root().invariants_hold());
  CHECK(rig.connection()->faulted());
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent), av::vkproto::kCmd_vkCreateInstance);
  CHECK_EQ(rig.kmt().command_size(sent), CommandHeader::encoded_size + 4u);
  std::uint32_t structure = 0u;
  std::memcpy(&structure, rig.kmt().command_bytes(sent) + CommandHeader::encoded_size, sizeof structure);
  CHECK_EQ(structure, static_cast<std::uint32_t>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO));
  const std::uint32_t before = kmt_calls(rig.kmt());
  CHECK_EQ(rig.rig().entry<DeviceWaitIdle>(rig.instance(), "vkDeviceWaitIdle")(device), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.force()(rig.physical()), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(kmt_calls(rig.kmt()), before);
  CHECK(rig.rig().log().holding("vkRvkForceContextFault on a lost device") == 1u); }

/* A second instance opens a connection of its own, which the first one's
 * fault leaves working. */
TEST(icd_forced_fault, a_new_instance_is_not_lost)
{ FaultRig rig;
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.force()(rig.physical()), VK_ERROR_DEVICE_LOST);
  const VkInstance again = rig.rig().create_instance();
  CHECK(again != VK_NULL_HANDLE);
  CHECK(rig.rig().root().invariants_hold());
  CHECK(!rig.rig().root().connection_of(again, Kind::instance)->faulted());
  const VkPhysicalDevice physical = rig.rig().physical_device(again);
  CHECK(physical != VK_NULL_HANDLE);
  rig.rig().destroy_instance(again);
  CHECK(rig.rig().root().invariants_hold()); }

/* A host that answers the cut frame did not fault anything: the call says it
 * could not, logs it, and the instance goes on working. */
TEST(icd_forced_fault, an_answered_frame_is_refused_as_not_faulting)
{ FaultRig rig;
  rig.rig().host().vk_result(VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(rig.force()(rig.physical()), VK_ERROR_INITIALIZATION_FAILED);
  CHECK(rig.rig().root().invariants_hold());
  CHECK(!rig.connection()->faulted());
  CHECK_EQ(rig.rig().log().holding("rvkvk: the host answered a truncated vkCreateInstance"), 1u);
  CHECK(rig.device() != VK_NULL_HANDLE); }

/* A physical device this ICD did not make is not dispatched on: nothing is
 * sent, and it is logged. */
TEST(icd_forced_fault, a_foreign_physical_device_is_refused)
{ FaultRig rig;
  const std::uint32_t before = kmt_calls(rig.kmt());
  std::uint64_t stack_word = 0u;
  const VkPhysicalDevice foreign = reinterpret_cast<VkPhysicalDevice>(&stack_word);
  CHECK_EQ(rig.force()(foreign), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(rig.force()(reinterpret_cast<VkPhysicalDevice>(rig.instance())), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(kmt_calls(rig.kmt()), before);
  CHECK_EQ(rig.rig().log().holding("rvkvk: vkRvkForceContextFault on a physical device this driver did not make"),
           2u); }
