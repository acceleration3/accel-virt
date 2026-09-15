/* The physical device as the guest's adapter: the host's properties with the API
 * version held to 1.3 and the adapter's LUID in VkPhysicalDeviceIDProperties.
 * The reply is the host's to write, so it is also fed cut at every length,
 * with every byte flipped, and crafted to relabel the application's own
 * structures: the chain the application passed comes back exactly as it was,
 * nothing outside a structure it labelled is written, and a chain that cannot be
 * remembered is not sent. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/vulkan/physical_device_view.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::IcdRig;
using av::MockTransport;
using av::ScriptedHost;
using av::WireReader;
using av::WireWriter;
using av::icd::PhysicalDeviceView;
using namespace av::vkproto;

namespace av::icd {

struct PhysicalDeviceViewProbe {
    static void overrun(PhysicalDeviceView &view) { view.remembered_ = PhysicalDeviceView::max_chain + 1u; }
};

}  // namespace av::icd

using av::icd::PhysicalDeviceViewProbe;

namespace {

constexpr std::uint32_t vulkan_1_1_250 = 0x004010FAu;
constexpr std::uint32_t vulkan_1_3 = 0x00403000u;
constexpr std::uint32_t vulkan_1_4_300 = 0x0040412Cu;
constexpr std::uint32_t host_vendor = 0x10DEu;
constexpr std::size_t   reply_bytes = 4096u;
constexpr std::uint8_t  canary = 0xA5u;

/* A physical device the root made, so its handle crosses as the host's. */
class ViewRig {
public:
    ViewRig()
    {
        instance_ = rig_.create_instance();
        physical_device_ = rig_.physical_device(instance_);
        CHECK(physical_device_ != VK_NULL_HANDLE);
    }
    ~ViewRig() { rig_.destroy_instance(instance_); }

    IcdRig          &rig() { return rig_; }
    VkPhysicalDevice physical_device() { return physical_device_; }
    MockTransport   &transport() { return transport_; }

    void answer(const WireWriter &reply) { transport_.set_reply(reply_, reply.offset()); }
    std::uint8_t *reply_block() { return reply_; }

    /* What the view sent: the command and the physical device it named. */
    void check_sent(std::uint32_t command)
    {
        CHECK_EQ(transport_.last_command(), command);
        WireReader sent(transport_.last_payload(), transport_.last_length());
        CHECK_EQ(sent.read_u64(), ScriptedHost::vk_physical_device);
    }

private:
    IcdRig           rig_;
    VkInstance       instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    MockTransport    transport_;
    std::uint8_t     reply_[reply_bytes] = {};
};

VkPhysicalDeviceProperties host_properties(std::uint32_t api_version)
{
    VkPhysicalDeviceProperties properties = {};
    properties.apiVersion = api_version;
    properties.driverVersion = 0x12345u;
    properties.vendorID = host_vendor;
    properties.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    std::strcpy(properties.deviceName, "host gpu");
    return properties;
}

/* The host's answer to vkGetPhysicalDeviceProperties2 with an ID node whose
 * LUID is its own and not valid, as a Linux host's is. */
void write_properties2_with_identity(WireWriter &reply, std::uint32_t api_version)
{
    VkPhysicalDeviceIDProperties identity = {};
    identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    std::memset(identity.deviceUUID, 0x11, sizeof identity.deviceUUID);
    std::memset(identity.deviceLUID, 0xEE, sizeof identity.deviceLUID);
    identity.deviceLUIDValid = VK_FALSE;
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &identity;
    properties.properties = host_properties(api_version);
    reply.write_pod<std::int32_t>(0);
    serialize_VkPhysicalDeviceProperties2(reply, properties);
}

bool luid_is_the_adapters(const VkPhysicalDeviceIDProperties &identity)
{
    const std::uint64_t luid = av::IcdRig::adapter_luid;
    return std::memcmp(identity.deviceLUID, &luid, sizeof identity.deviceLUID) == 0 &&
           identity.deviceLUIDValid == VK_TRUE;
}

}  // namespace

TEST(physical_device_view, holds_a_newer_host_to_the_version_this_icd_implements)
{ ViewRig view_rig;
  WireWriter reply(view_rig.reply_block(), reply_bytes);
  reply.write_pod<std::int32_t>(0);
  serialize_VkPhysicalDeviceProperties(reply, host_properties(vulkan_1_4_300));
  view_rig.answer(reply);
  PhysicalDeviceView view(view_rig.transport(), av::IcdRig::adapter_luid, vulkan_1_3, view_rig.rig().log());
  VkPhysicalDeviceProperties answer = {};
  view.properties(view_rig.physical_device(), &answer);
  CHECK(view.invariants_hold());
  view_rig.check_sent(kCmd_vkGetPhysicalDeviceProperties);
  CHECK_EQ(answer.apiVersion, vulkan_1_3);
  CHECK_EQ(answer.vendorID, host_vendor);
  CHECK_EQ(answer.driverVersion, 0x12345u);
  CHECK(std::strcmp(answer.deviceName, "host gpu") == 0); }

TEST(physical_device_view, leaves_an_older_host_at_its_own_version)
{ ViewRig view_rig;
  WireWriter reply(view_rig.reply_block(), reply_bytes);
  reply.write_pod<std::int32_t>(0);
  serialize_VkPhysicalDeviceProperties(reply, host_properties(vulkan_1_1_250));
  view_rig.answer(reply);
  PhysicalDeviceView view(view_rig.transport(), av::IcdRig::adapter_luid, vulkan_1_3, view_rig.rig().log());
  VkPhysicalDeviceProperties answer = {};
  view.properties(view_rig.physical_device(), &answer);
  CHECK_EQ(answer.apiVersion, vulkan_1_1_250);
  /* Exactly 1.3 stays 1.3. */
  WireWriter exact(view_rig.reply_block(), reply_bytes);
  exact.write_pod<std::int32_t>(0);
  serialize_VkPhysicalDeviceProperties(exact, host_properties(vulkan_1_3));
  view_rig.answer(exact);
  view.properties(view_rig.physical_device(), &answer);
  CHECK_EQ(answer.apiVersion, vulkan_1_3); }

TEST(physical_device_view, a_reply_that_never_came_answers_zeroed_properties)
{ ViewRig view_rig;
  view_rig.transport().fail_next();
  PhysicalDeviceView view(view_rig.transport(), av::IcdRig::adapter_luid, vulkan_1_3, view_rig.rig().log());
  VkPhysicalDeviceProperties answer = host_properties(vulkan_1_4_300);
  view.properties(view_rig.physical_device(), &answer);
  CHECK_EQ(answer.apiVersion, 0u);
  CHECK_EQ(answer.vendorID, 0u);
  CHECK_EQ(answer.deviceName[0], '\0'); }

TEST(physical_device_view, properties2_writes_the_adapters_luid_over_the_hosts)
{ const std::uint32_t host_versions[] = {vulkan_1_4_300, vulkan_1_1_250};
  for (const std::uint32_t host_version : host_versions) {
      ViewRig view_rig;
      WireWriter reply(view_rig.reply_block(), reply_bytes);
      write_properties2_with_identity(reply, host_version);
      view_rig.answer(reply);
      PhysicalDeviceView view(view_rig.transport(), av::IcdRig::adapter_luid, vulkan_1_3,
                              view_rig.rig().log());
      VkPhysicalDeviceIDProperties identity = {};
      identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
      VkPhysicalDeviceProperties2 answer = {};
      answer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
      answer.pNext = &identity;
      view.properties2(view_rig.physical_device(), &answer);
      CHECK(view.invariants_hold());
      view_rig.check_sent(kCmd_vkGetPhysicalDeviceProperties2);
      CHECK_EQ(answer.properties.apiVersion, host_version == vulkan_1_4_300 ? vulkan_1_3 : vulkan_1_1_250);
      CHECK_EQ(answer.properties.vendorID, host_vendor);
      CHECK(answer.pNext == &identity);
      CHECK(identity.pNext == nullptr);
      CHECK_EQ(identity.deviceUUID[0], 0x11u);
      CHECK(luid_is_the_adapters(identity));
      /* LowPart first: the byte order a LUID structure has. */
      CHECK_EQ(identity.deviceLUID[0], 0x34u);
      CHECK_EQ(identity.deviceLUID[4], 0x02u);
  }
}

TEST(physical_device_view, the_luid_goes_only_into_identity_nodes_anywhere_in_the_chain)
{ ViewRig view_rig;
  WireWriter reply(view_rig.reply_block(), reply_bytes);
  write_properties2_with_identity(reply, vulkan_1_3);
  view_rig.answer(reply);
  PhysicalDeviceView view(view_rig.transport(), 0x0000000900000001ull, vulkan_1_3, view_rig.rig().log());
  VkPhysicalDeviceIDProperties identity = {};
  identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
  VkPhysicalDeviceDriverProperties driver = {};
  driver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
  driver.pNext = &identity;
  VkPhysicalDeviceProperties2 answer = {};
  answer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  answer.pNext = &driver;
  view.properties2(view_rig.physical_device(), &answer);
  const std::uint64_t luid = 0x0000000900000001ull;
  CHECK(std::memcmp(identity.deviceLUID, &luid, sizeof luid) == 0);
  CHECK_EQ(identity.deviceLUIDValid, VK_TRUE);
  CHECK_EQ(driver.driverID, 0u);
  CHECK(driver.pNext == &identity);
  CHECK(driver.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES); }

TEST(physical_device_view, properties2_with_no_chain_is_only_held_to_the_version)
{ ViewRig view_rig;
  WireWriter reply(view_rig.reply_block(), reply_bytes);
  VkPhysicalDeviceProperties2 host = {};
  host.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  host.properties = host_properties(vulkan_1_4_300);
  reply.write_pod<std::int32_t>(0);
  serialize_VkPhysicalDeviceProperties2(reply, host);
  view_rig.answer(reply);
  PhysicalDeviceView view(view_rig.transport(), av::IcdRig::adapter_luid, vulkan_1_3, view_rig.rig().log());
  VkPhysicalDeviceProperties2 answer = {};
  answer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  view.properties2(view_rig.physical_device(), &answer);
  CHECK_EQ(answer.properties.apiVersion, vulkan_1_3);
  CHECK(answer.pNext == nullptr);
  CHECK(answer.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2); }

/* The host names the application's small point-clipping node by its own type
 * but writes the identity type into the node's sType. Believed, the LUID would
 * land 48 bytes into a 24-byte structure. */
TEST(physical_device_view, a_host_relabelling_the_applications_structures_is_undone)
{ ViewRig view_rig;
  WireWriter reply(view_rig.reply_block(), reply_bytes);
  VkPhysicalDevicePointClippingProperties forged = {};
  forged.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
  reply.write_pod<std::int32_t>(0);
  reply.write_pod(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
  reply.write_varint(std::uint64_t{1});
  reply.write_pod<std::uint32_t>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES);
  serialize_VkPhysicalDevicePointClippingProperties(reply, forged);
  serialize_VkPhysicalDeviceProperties(reply, host_properties(vulkan_1_3));
  view_rig.answer(reply);
  PhysicalDeviceView view(view_rig.transport(), av::IcdRig::adapter_luid, vulkan_1_3, view_rig.rig().log());
  std::uint8_t block[128] = {};
  std::memset(block, canary, sizeof block);
  VkPhysicalDevicePointClippingProperties clipping = {};
  clipping.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES;
  std::memcpy(block, &clipping, sizeof clipping);
  VkPhysicalDeviceProperties2 answer = {};
  answer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  answer.pNext = block;
  view.properties2(view_rig.physical_device(), &answer);
  std::memcpy(&clipping, block, sizeof clipping);
  CHECK(clipping.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES);
  CHECK(clipping.pNext == nullptr);
  CHECK(answer.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
  CHECK(answer.pNext == block);
  bool untouched = true;
  for (std::size_t offset = sizeof clipping; offset < sizeof block; offset++)
      untouched = untouched && block[offset] == canary;
  CHECK(untouched); }

TEST(physical_device_view, a_chain_too_long_to_remember_or_cyclic_is_not_sent)
{ ViewRig view_rig;
  PhysicalDeviceView view(view_rig.transport(), av::IcdRig::adapter_luid, vulkan_1_3, view_rig.rig().log());
  VkPhysicalDeviceDriverProperties nodes[PhysicalDeviceView::max_chain] = {};
  for (std::uint32_t index = 0u; index < PhysicalDeviceView::max_chain; index++) {
      nodes[index].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
      nodes[index].pNext = index + 1u < PhysicalDeviceView::max_chain ? &nodes[index + 1u] : nullptr;
  }
  VkPhysicalDeviceProperties2 answer = {};
  answer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  answer.pNext = &nodes[0];
  answer.properties.apiVersion = 77u;
  /* The application's structure and 32 nodes: one more than can be remembered. */
  view.properties2(view_rig.physical_device(), &answer);
  CHECK(view.invariants_hold());
  CHECK_EQ(view_rig.transport().sends(), 0u);
  CHECK_EQ(answer.properties.apiVersion, 77u);
  CHECK_EQ(view_rig.rig().log().holding("chain longer than 32"), 1u);
  /* 31 nodes fit, and cross. */
  nodes[PhysicalDeviceView::max_chain - 2u].pNext = nullptr;
  view_rig.transport().fail_next();
  view.properties2(view_rig.physical_device(), &answer);
  CHECK_EQ(answer.properties.apiVersion, 0u);
  CHECK(nodes[PhysicalDeviceView::max_chain - 2u].pNext == nullptr);
  /* A node pointing at itself. */
  nodes[0].pNext = &nodes[0];
  answer.properties.apiVersion = 77u;
  view.properties2(view_rig.physical_device(), &answer);
  CHECK_EQ(answer.properties.apiVersion, 77u);
  CHECK_EQ(view_rig.rig().log().holding("chain longer than 32"), 2u); }

TEST(physical_device_view, a_reply_cut_at_every_length_leaves_the_chain_and_the_luid_right)
{ ViewRig view_rig;
  std::uint8_t valid[reply_bytes] = {};
  WireWriter full(valid, sizeof valid);
  write_properties2_with_identity(full, vulkan_1_4_300);
  const std::size_t length = full.offset();
  PhysicalDeviceView view(view_rig.transport(), av::IcdRig::adapter_luid, vulkan_1_3, view_rig.rig().log());
  for (std::size_t cut = 0u; cut <= length; cut++) {
      view_rig.transport().set_reply(valid, cut);
      VkPhysicalDeviceIDProperties identity = {};
      identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
      VkPhysicalDeviceProperties2 answer = {};
      answer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
      answer.pNext = &identity;
      answer.properties.vendorID = 0xFFFFu;
      view.properties2(view_rig.physical_device(), &answer);
      CHECK(view.invariants_hold());
      CHECK(answer.pNext == &identity && identity.pNext == nullptr);
      CHECK(identity.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
      CHECK(answer.properties.apiVersion <= vulkan_1_3);
      CHECK(luid_is_the_adapters(identity));
      /* Only a reply that reached the properties fills them. */
      CHECK(cut == length ? answer.properties.vendorID == host_vendor : answer.properties.vendorID != 0xFFFFu);
  }
}

TEST(physical_device_view, every_byte_of_a_reply_flipped_leaves_the_chain_and_the_luid_right)
{ ViewRig view_rig;
  std::uint8_t valid[reply_bytes] = {};
  WireWriter full(valid, sizeof valid);
  write_properties2_with_identity(full, vulkan_1_4_300);
  const std::size_t length = full.offset();
  PhysicalDeviceView view(view_rig.transport(), av::IcdRig::adapter_luid, vulkan_1_3, view_rig.rig().log());
  std::uint8_t flipped[reply_bytes] = {};
  for (std::size_t offset = 0u; offset < length; offset++) {
      std::memcpy(flipped, valid, length);
      flipped[offset] = static_cast<std::uint8_t>(~flipped[offset]);
      view_rig.transport().set_reply(flipped, length);
      VkPhysicalDeviceIDProperties identity = {};
      identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
      VkPhysicalDeviceProperties2 answer = {};
      answer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
      answer.pNext = &identity;
      view.properties2(view_rig.physical_device(), &answer);
      CHECK(answer.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
      CHECK(answer.pNext == &identity && identity.pNext == nullptr);
      CHECK(identity.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
      CHECK(answer.properties.apiVersion <= vulkan_1_3);
      CHECK(luid_is_the_adapters(identity));
  }
}

TEST(physical_device_view, invariants_refuse_more_remembered_than_it_has_room_for)
{ ViewRig view_rig;
  PhysicalDeviceView view(view_rig.transport(), av::IcdRig::adapter_luid, vulkan_1_3, view_rig.rig().log());
  CHECK(view.invariants_hold());
  PhysicalDeviceViewProbe::overrun(view);
  CHECK(!view.invariants_hold()); }
